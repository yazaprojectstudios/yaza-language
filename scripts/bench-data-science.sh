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
use "std/data/stats"
use "std/data/matrix"
use "std/data/ml"

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

        # 4. Ill-Conditioned Summation Accuracy Torture Test ([1e16, 1.0 x 10000, -1e16] -> true sum = 10000.0)
        let ill: List[Float64] = List[Float64]()
        list_push(ill, 10000000000000000.0)
        var k: Number = 0
        while k < 10000:
            list_push(ill, 1.0)
            set k = k + 1
        list_push(ill, -10000000000000000.0)
        let yaza_comp_sum: Float64 = stats_sum_compensated(ill)

        # 5. Catastrophic Cancellation Variance at 10^9 Offset ([1e9+1, 1e9+2, 1e9+3] -> true var = 1.0)
        let sh: List[Float64] = List[Float64]()
        list_push(sh, 1000000001.0)
        list_push(sh, 1000000002.0)
        list_push(sh, 1000000003.0)
        let sh_desc: StatsSummary = result_unwrap(stats_describe(sh))

        say "YAZA_DS_BENCH:"
        say "  Welford 4-Moment Stats (1M pts) : " + float64_text(describe_ms) + " ms"
        say "  OLS Regression + R2 (1M pts)    : " + float64_text(ols_ms) + " ms"
        say "  Z-Score + EWMA + Dot (100K pts) : " + float64_text(vec_ms) + " ms"
        say "  Ill-Conditioned Sum (true=10000): " + float64_text(yaza_comp_sum)
        say "  10^9 Shifted Variance (true=1.0): " + float64_text(sh_desc.variance_sample)
        assert(mean_acc > 0.0 and slope_acc > 0.0 and dot_v > -999999999.0)
EOF

"$YAZA_BIN" emit-llvm "$WORK_DIR/bench_yaza_ds.yz" -o "$WORK_DIR/bench_yaza_ds.ll"
clang -Wno-override-module -std=c11 -O2 "$WORK_DIR/bench_yaza_ds.ll" "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" -I"$PROJECT_DIR/runtime/include" -pthread -lm -o "$WORK_DIR/bench_yaza_ds"
/usr/bin/time -l "$WORK_DIR/bench_yaza_ds" 2>"$WORK_DIR/yaza_time.txt"
grep "maximum resident set size" "$WORK_DIR/yaza_time.txt" || true

python3 - << 'PYEOF'
import time, math, statistics, resource

n = 100000
xs = [i * 0.01 for i in range(n)]
ys = [3.5 * x + 12.25 for x in xs]

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

# 4. Naive Python sum() on ill-conditioned series & textbook 2-term variance at 10^9 offset
ill = [1e16] + [1.0] * 10000 + [-1e16]
py_naive_sum = sum(ill)

sh = [1000000001.0, 1000000002.0, 1000000003.0]
sh_mean = sum(sh) / 3.0
py_naive_var = (sum(x * x for x in sh) - 3.0 * (sh_mean * sh_mean)) / 2.0

rss_mb = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss / (1024 * 1024)
print("PYTHON3_BENCH:")
print(f"  Stats 4-Moment (1M pts)         : {py_describe_ms:.2f} ms")
print(f"  OLS Regression + R2 (1M pts)    : {py_ols_ms:.2f} ms")
print(f"  Z-Score + EWMA + Dot (100K pts) : {py_vec_ms:.2f} ms")
print(f"  Naive sum() (true=10000)        : {py_naive_sum}")
print(f"  Naive 2-Term Var (true=1.0)     : {py_naive_var}")
print(f"  Python Peak RSS                 : {rss_mb:.2f} MB")
PYEOF
