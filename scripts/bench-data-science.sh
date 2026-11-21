#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
YAZA_BIN=${YAZA_TEST_CLI:-"$PROJECT_DIR/build/yaza"}
TEMP_ROOT=${TMPDIR:-/tmp}
TEMP_ROOT=${TEMP_ROOT%/}
WORK_DIR=$(mktemp -d "$TEMP_ROOT/yaza-ds-bench.XXXXXX")
trap 'rm -rf "$WORK_DIR"' EXIT HUP INT TERM

export YAZA_STDLIB_DIR="$PROJECT_DIR/std"
export YAZA_RUNTIME_DIR="$PROJECT_DIR/runtime"

cat > "$WORK_DIR/bench_yaza_ds.yz" << 'EOF'
use "std/time"
use "std/math"
use "std/data/random"
use "std/data/stats"
use "std/data/matrix"
use "std/data/frame"
use "std/data/ml"
use "std/data/viz"

start:
    arena_scope 134217728:
        let n: Number = 100000
        let xs: List[Float64] = List[Float64]()
        let ys: List[Float64] = List[Float64]()
        var i: Number = 0
        while i < n:
            let xf: Float64 = float64_from_number(i) * 0.01
            list_push(xs, xf)
            list_push(ys, 3.5 * xf + 12.25)
            set i = i + 1

        # 1. Welford 4-Moment Describe (100,000 rows x 10 iterations = 1M items)
        let t0: Number = time_now_monotonic_ns()
        var it1: Number = 0
        var mean_acc: Float64 = 0.0
        while it1 < 10:
            let d: StatsSummary = result_unwrap(stats_describe(xs))
            set mean_acc = mean_acc + d.mean + d.std_dev_sample + d.skewness
            set it1 = it1 + 1
        let t1: Number = time_now_monotonic_ns()
        let describe_ms: Float64 = float64_from_number(t1 - t0) / 1000000.0

        # 2. OLS Linear Regression + R^2 + RMSE (100,000 rows x 10 iterations = 1M items)
        let t2: Number = time_now_monotonic_ns()
        var it2: Number = 0
        var slope_acc: Float64 = 0.0
        while it2 < 10:
            let m: OlsModel = result_unwrap(ml_ols_fit(xs, ys))
            set slope_acc = slope_acc + m.slope + m.r_squared
            set it2 = it2 + 1
        let t3: Number = time_now_monotonic_ns()
        let ols_ms: Float64 = float64_from_number(t3 - t2) / 1000000.0

        # 3. Vector Z-Score + EWMA Time-Series + Compensated Dot Product (100,000 rows)
        let t4: Number = time_now_monotonic_ns()
        let z_xs: List[Float64] = result_unwrap(vec_zscore_normalize(xs))
        let ew_ys: List[Float64] = result_unwrap(vec_ewma(ys, 0.2))
        let dot_v: Float64 = result_unwrap(vec_dot_compensated(z_xs, ew_ys))
        let t5: Number = time_now_monotonic_ns()
        let vec_ms: Float64 = float64_from_number(t5 - t4) / 1000000.0

        # 4. Silverman Gaussian KDE (5,000 observations x 64 grid points = 320,000 Gaussian kernel evals)
        let kde_sample: List[Float64] = rng_normal_vec(2026, 5000, 100.0, 15.0)
        let t6: Number = time_now_monotonic_ns()
        let kde_res: KdeCurve = result_unwrap(stats_kde_gaussian(kde_sample, 64))
        let t7: Number = time_now_monotonic_ns()
        let kde_ms: Float64 = float64_from_number(t7 - t6) / 1000000.0

        # 5. Columnar DataFrame ETL Pipeline (5,000-row CSV -> df_from_csv -> df_fillna_mean -> df_filter_gt -> df_groupby_agg -> df_inner_join)
        let csv_lines: TextList = text_list()
        list_push(csv_lines, "region,latency_ms,req_count")
        let regions: TextList = text_list()
        list_push(regions, "ap-southeast-1")
        list_push(regions, "us-east-1")
        list_push(regions, "eu-central-1")
        list_push(regions, "ap-northeast-1")
        var r_idx: Number = 0
        while r_idx < 5000:
            let reg: Text = list_get(regions, r_idx - (r_idx / 4) * 4)
            var lat_txt: Text = float64_text(10.0 + float64_from_number(r_idx - (r_idx / 50) * 50) * 1.5)
            if r_idx - (r_idx / 25) * 25 == 0:
                set lat_txt = "NA"
            let req_txt: Text = number_text(100 + (r_idx - (r_idx / 20) * 20) * 5)
            list_push(csv_lines, reg + "," + lat_txt + "," + req_txt)
            set r_idx = r_idx + 1
        let raw_csv: Text = text_join(csv_lines, "\n")
        let dim_csv: Text = "region,tier\nap-southeast-1,tier-1\nus-east-1,tier-1\neu-central-1,tier-2\nap-northeast-1,tier-2"

        let t_df0: Number = time_now_monotonic_ns()
        let df_raw: DataFrame = result_unwrap(df_from_csv(raw_csv))
        let df_dim: DataFrame = result_unwrap(df_from_csv(dim_csv))
        let t_df_parse: Number = time_now_monotonic_ns()
        let df_clean: DataFrame = result_unwrap(df_fillna_mean(df_raw, "latency_ms"))
        let df_filt: DataFrame = result_unwrap(df_filter_gt(df_clean, "latency_ms", 25.0))
        let df_grp: List[GroupBySummaryRow] = result_unwrap(df_groupby_agg(df_filt, "region", "latency_ms"))
        let df_joined: DataFrame = result_unwrap(df_inner_join(df_filt, df_dim, "region"))
        let t_df1: Number = time_now_monotonic_ns()
        let df_parse_ms: Float64 = float64_from_number(t_df_parse - t_df0) / 1000000.0
        let df_ops_ms: Float64 = float64_from_number(t_df1 - t_df_parse) / 1000000.0
        let df_total_ms: Float64 = float64_from_number(t_df1 - t_df0) / 1000000.0

        # 6. Ill-Conditioned Summation Accuracy Torture Test ([1e16, 1.0 x 10000, -1e16] -> true sum = 10000.0)
        let ill: List[Float64] = List[Float64]()
        list_push(ill, 10000000000000000.0)
        var k: Number = 0
        while k < 10000:
            list_push(ill, 1.0)
            set k = k + 1
        list_push(ill, -10000000000000000.0)
        let yaza_comp_sum: Float64 = stats_sum_compensated(ill)

        # 7. Catastrophic Cancellation Variance & OLS at 10^9 Offset ([1e9+1, 1e9+2, 1e9+3] -> true var = 1.0, true slope = 2.0)
        let sh_x: List[Float64] = List[Float64]()
        list_push(sh_x, 1000000001.0)
        list_push(sh_x, 1000000002.0)
        list_push(sh_x, 1000000003.0)
        let sh_y: List[Float64] = List[Float64]()
        list_push(sh_y, 2.0)
        list_push(sh_y, 4.0)
        list_push(sh_y, 6.0)
        let sh_desc: StatsSummary = result_unwrap(stats_describe(sh_x))
        let sh_ols: OlsModel = result_unwrap(ml_ols_fit(sh_x, sh_y))

        say "YAZA_DS_BENCH:"
        say "  Welford 4-Moment Stats (1M pts) : " + viz_fmt_num(describe_ms) + " ms"
        say "  OLS Regression + R2 (1M pts)    : " + viz_fmt_num(ols_ms) + " ms"
        say "  Z-Score + EWMA + Dot (100K pts) : " + viz_fmt_num(vec_ms) + " ms"
        say "  Silverman Gaussian KDE (320K)   : " + viz_fmt_num(kde_ms) + " ms"
        say "  DataFrame CSV Parse (5K rows)   : " + viz_fmt_num(df_parse_ms) + " ms"
        say "  DataFrame FillNA+Filter+Grp+Join: " + viz_fmt_num(df_ops_ms) + " ms"
        say "  DataFrame Total ETL (5K rows)   : " + viz_fmt_num(df_total_ms) + " ms (joined_rows=" + number_text(df_joined.row_count) + ", groups=" + number_text(list_count(df_grp)) + ")"
        say "  Ill-Conditioned Sum (true=10000): " + viz_fmt_num(yaza_comp_sum)
        say "  10^9 Shifted Variance (true=1.0): " + viz_fmt_num(sh_desc.variance_sample)
        say "  10^9 Shifted OLS Slope (true=2) : " + viz_fmt_num(sh_ols.slope)
        assert(mean_acc > 0.0 and slope_acc > 0.0 and dot_v > -999999999.0 and kde_res.bandwidth > 0.0 and df_joined.row_count > 0)
EOF

"$YAZA_BIN" emit-llvm "$WORK_DIR/bench_yaza_ds.yz" -o "$WORK_DIR/bench_yaza_ds.ll"
clang -Wno-override-module -std=c11 -O2 "$WORK_DIR/bench_yaza_ds.ll" "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" -I"$PROJECT_DIR/runtime/include" -pthread -lm -o "$WORK_DIR/bench_yaza_ds"
/usr/bin/time -l "$WORK_DIR/bench_yaza_ds" 2>"$WORK_DIR/yaza_time.txt"
grep "maximum resident set size" "$WORK_DIR/yaza_time.txt" || true

PY_BIN="python3"
if [ -x "/tmp/yaza_ds_venv/bin/python3" ]; then
  PY_BIN="/tmp/yaza_ds_venv/bin/python3"
fi

"$PY_BIN" - << 'PYEOF'
import time, math, statistics, resource, random, csv, io

n = 100000
xs = [i * 0.01 for i in range(n)]
ys = [3.5 * x + 12.25 for x in xs]

try:
    import numpy as np
    import scipy.stats as st
    HAS_NUMPY = True
except ImportError:
    HAS_NUMPY = False

if HAS_NUMPY:
    np_xs = np.array(xs, dtype=np.float64)
    np_ys = np.array(ys, dtype=np.float64)
    # Warmup
    _ = st.describe(np_xs)
    _ = st.linregress(np_xs, np_ys)

    # 1. NumPy / SciPy 4-Moment Describe (100,000 rows x 10 iterations = 1M items)
    t0 = time.perf_counter_ns()
    np_acc = 0.0
    for _ in range(10):
        d = st.describe(np_xs)
        np_acc += float(d.mean) + math.sqrt(float(d.variance)) + float(d.skewness) + float(d.kurtosis)
    t1 = time.perf_counter_ns()
    np_describe_ms = (t1 - t0) / 1e6

    # 1b. NumPy direct array ops (mean, std, central 3rd & 4th moments) x 10 iterations
    t0b = time.perf_counter_ns()
    for _ in range(10):
        m = np_xs.mean()
        d_arr = np_xs - m
        d2 = d_arr * d_arr
        v = d2.sum() / (n - 1)
        sk = (d2 * d_arr).sum() / n
        ku = (d2 * d2).sum() / n
        np_acc += float(m + v + sk + ku)
    t1b = time.perf_counter_ns()
    np_direct_moments_ms = (t1b - t0b) / 1e6

    # 2. SciPy OLS Linear Regression + R2 (100,000 rows x 10 iterations = 1M items)
    t2 = time.perf_counter_ns()
    for _ in range(10):
        lr = st.linregress(np_xs, np_ys)
        np_acc += float(lr.slope + lr.rvalue ** 2)
    t3 = time.perf_counter_ns()
    np_ols_ms = (t3 - t2) / 1e6

    # 3. NumPy Z-Score + EWMA + Dot Product (100,000 items)
    t4 = time.perf_counter_ns()
    z_np = (np_xs - np_xs.mean()) / np_xs.std(ddof=1)
    ew_np = np.empty_like(np_ys)
    ew_np[0] = np_ys[0]
    s_val = float(np_ys[0])
    for idx in range(1, n):
        s_val = 0.2 * float(np_ys[idx]) + 0.8 * s_val
        ew_np[idx] = s_val
    dot_np = float(np.dot(z_np, ew_np))
    t5 = time.perf_counter_ns()
    np_vec_ms = (t5 - t4) / 1e6

    # 4. SciPy gaussian_kde & NumPy broadcast KDE (5,000 obs x 64 grid = 320K evals)
    rng_np = np.random.default_rng(2026)
    kde_arr = rng_np.normal(100.0, 15.0, size=5000)
    grid_arr = np.linspace(kde_arr.min() - 10.0, kde_arr.max() + 10.0, 64)
    t6 = time.perf_counter_ns()
    kde_obj = st.gaussian_kde(kde_arr, bw_method="silverman")
    kde_out = kde_obj(grid_arr)
    t7 = time.perf_counter_ns()
    scipy_kde_ms = (t7 - t6) / 1e6

    t6b = time.perf_counter_ns()
    sd_k = float(kde_arr.std(ddof=1))
    iqr_k = float(np.quantile(kde_arr, 0.75) - np.quantile(kde_arr, 0.25)) / 1.34
    bw_k = 0.9 * min(sd_k, iqr_k) * (5000.0 ** -0.2)
    u_mat = (grid_arr[:, None] - kde_arr[None, :]) / bw_k
    dens_np = np.exp(-0.5 * (u_mat * u_mat)).sum(axis=1) * (0.3989422804014327 / (5000.0 * bw_k))
    t7b = time.perf_counter_ns()
    np_kde_ms = (t7b - t6b) / 1e6

    print(f"NUMPY_SCIPY_BENCH (NumPy {np.__version__} / SciPy {__import__('scipy').__version__}):")
    print(f"  scipy.stats.describe (1M pts)   : {np_describe_ms:.2f} ms")
    print(f"  NumPy vectorized moments (1M)   : {np_direct_moments_ms:.2f} ms")
    print(f"  scipy.stats.linregress (1M pts) : {np_ols_ms:.2f} ms")
    print(f"  NumPy Z-Score + EWMA + Dot(100K): {np_vec_ms:.2f} ms")
    print(f"  scipy.stats.gaussian_kde (320K) : {scipy_kde_ms:.2f} ms")
    print(f"  NumPy 2D Broadcast KDE (320K)   : {np_kde_ms:.2f} ms")

# 1. Python stats describe (mean, stdev, skewness, kurtosis) x 10 iterations (1M items)
t0 = time.perf_counter_ns()
acc = 0.0
for _ in range(10):
    m = statistics.fmean(xs)
    sd = statistics.stdev(xs, m)
    inv_sd = 1.0 / sd
    m3 = sum(((x - m) * inv_sd) ** 3 for x in xs) / n
    m4 = sum(((x - m) * inv_sd) ** 4 for x in xs) / n - 3.0
    acc += m + sd + m3 + m4
t1 = time.perf_counter_ns()
py_describe_ms = (t1 - t0) / 1e6

# 2. Python OLS Linear Regression + R2 + RMSE x 10 iterations (1M items)
t2 = time.perf_counter_ns()
s_acc = 0.0
for _ in range(10):
    mx = sum(xs) / n
    my = sum(ys) / n
    sxx = sum((x - mx) ** 2 for x in xs)
    syy = sum((y - my) ** 2 for y in ys)
    sxy = sum((x - mx) * (y - my) for x, y in zip(xs, ys))
    slope = sxy / sxx
    intercept = my - slope * mx
    ss_res = sum((y - (intercept + slope * x)) ** 2 for x, y in zip(xs, ys))
    r2 = 1.0 - ss_res / syy
    s_acc += slope + r2
t3 = time.perf_counter_ns()
py_ols_ms = (t3 - t2) / 1e6

# 3. Python Z-Score + EWMA + Dot Product (100K items)
t4 = time.perf_counter_ns()
mx = sum(xs) / n
sdx = math.sqrt(sum((x - mx) ** 2 for x in xs) / (n - 1))
z_xs = [(x - mx) / sdx for x in xs]
ew_ys = [ys[0]]
s = ys[0]
for y in ys[1:]:
    s = 0.2 * y + 0.8 * s
    ew_ys.append(s)
dot_v = sum(a * b for a, b in zip(z_xs, ew_ys))
t5 = time.perf_counter_ns()
py_vec_ms = (t5 - t4) / 1e6

# 4. Python Silverman Gaussian KDE (5,000 observations x 64 grid points = 320K kernel evals)
rng = random.Random(2026)
kde_sample = [rng.gauss(100.0, 15.0) for _ in range(5000)]
t6 = time.perf_counter_ns()
km = sum(kde_sample) / 5000.0
ksd = math.sqrt(sum((x - km) ** 2 for x in kde_sample) / 4999.0)
bw = 0.9 * ksd * (5000.0 ** -0.2)
x_start = min(kde_sample) - 2.5 * bw
x_end = max(kde_sample) + 2.5 * bw
step = (x_end - x_start) / 63.0
scale = 0.3989422804014327 / (5000.0 * bw)
dens = []
for g in range(64):
    xq = x_start + g * step
    ks = sum(math.exp(-0.5 * ((xq - x) / bw) ** 2) for x in kde_sample)
    dens.append(ks * scale)
t7 = time.perf_counter_ns()
py_kde_ms = (t7 - t6) / 1e6

# 5. Python 5,000-row CSV Parse + FillNA + Filter + GroupBy + Inner Join ETL
regions = ["ap-southeast-1", "us-east-1", "eu-central-1", "ap-northeast-1"]
csv_rows = ["region,latency_ms,req_count"]
for r_idx in range(5000):
    reg = regions[r_idx % 4]
    lat_txt = "NA" if (r_idx % 25 == 0) else f"{10.0 + (r_idx % 50) * 1.5}"
    req_txt = str(100 + (r_idx % 20) * 5)
    csv_rows.append(f"{reg},{lat_txt},{req_txt}")
raw_csv = "\n".join(csv_rows)
dim_csv = "region,tier\nap-southeast-1,tier-1\nus-east-1,tier-1\neu-central-1,tier-2\nap-northeast-1,tier-2"

t_df0 = time.perf_counter_ns()
reader = list(csv.DictReader(io.StringIO(raw_csv)))
dim_rows = list(csv.DictReader(io.StringIO(dim_csv)))
parsed_reg = [r["region"] for r in reader]
parsed_lat = [float("nan") if r["latency_ms"] == "NA" else float(r["latency_ms"]) for r in reader]
parsed_req = [float(r["req_count"]) for r in reader]
t_df_parse = time.perf_counter_ns()
valid_lats = [v for v in parsed_lat if not math.isnan(v)]
mean_lat = math.fsum(valid_lats) / len(valid_lats)
clean_lat = [mean_lat if math.isnan(v) else v for v in parsed_lat]
filt_idx = [i for i, v in enumerate(clean_lat) if v > 25.0]
filt_reg = [parsed_reg[i] for i in filt_idx]
filt_lat = [clean_lat[i] for i in filt_idx]
buckets = {}
for k_reg, v_lat in zip(filt_reg, filt_lat):
    buckets.setdefault(k_reg, []).append(v_lat)
grp_summary = {k_reg: (len(vs), math.fsum(vs) / len(vs), statistics.stdev(vs)) for k_reg, vs in buckets.items()}
dim_map = {r["region"]: r["tier"] for r in dim_rows}
joined_tiers = [dim_map[k_reg] for k_reg in filt_reg if k_reg in dim_map]
t_df1 = time.perf_counter_ns()
py_df_parse_ms = (t_df_parse - t_df0) / 1e6
py_df_ops_ms = (t_df1 - t_df_parse) / 1e6
py_df_total_ms = (t_df1 - t_df0) / 1e6

# 6. Naive IEEE-754 Torture Tests vs Compensated Python Stdlib
ill = [1e16] + [1.0] * 10000 + [-1e16]
py_naive_sum = sum(ill)
py_fsum = math.fsum(ill)

sh_x = [1000000001.0, 1000000002.0, 1000000003.0]
sh_y = [2.0, 4.0, 6.0]
sh_mean = sum(sh_x) / 3.0
py_naive_var = (sum(x * x for x in sh_x) - 3.0 * (sh_mean * sh_mean)) / 2.0
py_std_var = statistics.variance(sh_x)
num_ols = 3.0 * sum(x * y for x, y in zip(sh_x, sh_y)) - sum(sh_x) * sum(sh_y)
den_ols = 3.0 * sum(x * x for x in sh_x) - (sum(sh_x) ** 2)
py_naive_slope = float("nan") if den_ols == 0.0 else num_ols / den_ols

rss_mb = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss / (1024 * 1024)
print("PYTHON3_BENCH:")
print(f"  Stats 4-Moment (1M pts)         : {py_describe_ms:.2f} ms")
print(f"  OLS Regression + R2 (1M pts)    : {py_ols_ms:.2f} ms")
print(f"  Z-Score + EWMA + Dot (100K pts) : {py_vec_ms:.2f} ms")
print(f"  Silverman Gaussian KDE (320K)   : {py_kde_ms:.2f} ms")
print(f"  DataFrame CSV Parse (5K rows)   : {py_df_parse_ms:.2f} ms")
print(f"  DataFrame FillNA+Filter+Grp+Join: {py_df_ops_ms:.2f} ms")
print(f"  DataFrame Total ETL (5K rows)   : {py_df_total_ms:.2f} ms (joined_rows={len(joined_tiers)}, groups={len(grp_summary)})")
print(f"  Naive sum() vs math.fsum()      : {py_naive_sum} vs {py_fsum}")
print(f"  Naive 2-Term vs statistics.var  : {py_naive_var} vs {py_std_var}")
print(f"  Naive Uncentered OLS (true=2.0) : {py_naive_slope}")
print(f"  Python Peak RSS                 : {rss_mb:.2f} MB")
PYEOF
