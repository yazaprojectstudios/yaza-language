#!/usr/bin/env python3
"""
Automated SciPy / R-compatible Golden Reference Generator for YAZA std/data/* & std/math.
Generates:
  1. tests/stdlib/data_science_golden.json (golden inputs, SciPy reference outputs, and explicit tolerances)
  2. tests/stdlib/data_science_golden_check.yz (YAZA CI verification program that checks every golden case and prints max errors)
"""

import json
import math
import os
import numpy as np
import scipy.special as sp
import scipy.stats as st

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GOLDEN_JSON_PATH = os.path.join(ROOT, "tests", "stdlib", "data_science_golden.json")
GOLDEN_YZ_PATH = os.path.join(ROOT, "tests", "stdlib", "data_science_golden_check.yz")


def build_golden_suite():
    suite = {
        "generator": {
            "numpy_version": np.__version__,
            "scipy_version": st.__name__ + " / " + sp.__name__ + " (" + __import__("scipy").__version__ + ")",
        },
        "tolerances": {
            "normal_ppf_rel": 1e-13,
            "normal_cdf_abs": 1e-14,
            " normal_sf_rel": 1e-12,
            "betainc_abs": 1e-12,
            "gammainc_upper_abs": 1e-12,
            "student_t_p_abs": 1e-11,
            "f_dist_p_abs": 1e-11,
            "chi2_p_abs": 1e-11,
            "hypothesis_tests_rel": 1e-10,
        },
        "cases": {},
    }

    # 1. Inverse Normal Quantile (qnorm / norm.ppf) including deep tails
    ppf_probs = [
        1e-12, 1e-9, 1e-6, 1e-4, 1e-3, 0.005, 0.01, 0.025, 0.05, 0.1,
        0.25, 0.5, 0.75, 0.9, 0.95, 0.975, 0.99, 0.995,
        1.0 - 1e-3, 1.0 - 1e-4, 1.0 - 1e-6, 1.0 - 1e-9, 1.0 - 1e-12,
    ]
    suite["cases"]["normal_ppf"] = [
        {"p": p, "expected": float(st.norm.ppf(p))} for p in ppf_probs
    ]

    # 2. Normal CDF & Upper-Tail Survival Function including z = +/- 7
    cdf_zs = [-7.0, -5.0, -3.5, -2.5, -1.959963984540054, -1.0, -0.25, 0.0, 0.25, 1.0, 1.959963984540054, 2.5, 3.5, 5.0, 7.0]
    suite["cases"]["normal_cdf"] = [
        {"z": z, "cdf": float(st.norm.cdf(z)), "sf": float(st.norm.sf(z))}
        for z in cdf_zs
    ]

    # 3. Regularized Incomplete Beta I_x(a, b) across small, symmetric, skewed, and extreme parameters
    beta_params = [
        (0.5, 0.5, 0.05),
        (0.5, 0.5, 0.50),
        (0.5, 0.5, 0.95),
        (0.5, 1.0, 0.25),
        (1.0, 2.5, 0.25),
        (2.5, 10.0, 0.40),
        (10.0, 2.5, 0.60),
        (25.0, 25.0, 0.48),
        (50.0, 50.0, 0.52),
        (0.5, 80.0, 0.02),
        (80.0, 0.5, 0.98),
    ]
    suite["cases"]["betainc"] = [
        {"a": a, "b": b, "x": x, "expected": float(sp.betainc(a, b, x))}
        for (a, b, x) in beta_params
    ]

    # 4. Regularized Upper Incomplete Gamma Q(a, x) across small and extreme parameters
    gamma_params = [
        (0.5, 0.05),
        (0.5, 1.920729410347062),
        (0.5, 5.0),
        (1.0, 3.0),
        (1.5, 0.75),
        (2.5, 1.2),
        (5.0, 9.23635689978112),
        (10.0, 15.0),
        (50.0, 45.0),
        (80.0, 95.0),
    ]
    suite["cases"]["gammainc_upper"] = [
        {"a": a, "x": x, "expected": float(sp.gammaincc(a, x))}
        for (a, x) in gamma_params
    ]

    # 5. Student-t two-tailed p-values (small df=1, 2, 3.5, 5 and extreme t)
    t_params = [
        (0.5, 1.0),
        (2.0, 1.0),
        (12.706204736432095, 1.0),
        (1.5, 2.0),
        (4.302652729911275, 2.0),
        (2.1, 3.5),
        (2.5705818366147395, 5.0),
        (6.0, 5.0),
        (2.2281388519649385, 10.0),
        (4.5, 10.0),
        (2.0422724563012373, 30.0),
        (5.5, 30.0),
        (1.9799304050527238, 120.0),
    ]
    suite["cases"]["student_t_p"] = [
        {"t": t, "df": df, "expected": float(2.0 * st.t.sf(abs(t), df))}
        for (t, df) in t_params
    ]

    # 6. F-distribution upper-tail p-values
    f_params = [
        (4.5, 1.0, 2.0),
        (18.512820512820515, 1.0, 2.0),
        (5.786135043349964, 2.0, 5.0),
        (5.95254468500487, 3.0, 12.0),
        (3.1, 5.0, 50.0),
        (2.5, 10.0, 100.0),
    ]
    suite["cases"]["f_dist_p"] = [
        {"f": f, "d1": d1, "d2": d2, "expected": float(st.f.sf(f, d1, d2))}
        for (f, d1, d2) in f_params
    ]

    # 7. Chi-Square upper-tail p-values
    chi2_params = [
        (3.841458820694124, 1.0),
        (10.827566170662733, 1.0),
        (5.991464547107979, 2.0),
        (11.344866730144373, 3.0),
        (15.086272469388987, 5.0),
        (23.209251158954356, 10.0),
        (50.89218131151707, 30.0),
    ]
    suite["cases"]["chi2_p"] = [
        {"chi2": c, "df": df, "expected": float(st.chi2.sf(c, df))}
        for (c, df) in chi2_params
    ]

    # 8. End-to-end hypothesis tests & R Type-7 quantiles against SciPy / NumPy
    sample_a = [12.4, 14.1, 13.8, 15.2, 11.9, 13.5, 14.7]
    sample_b = [9.8, 10.5, 11.2, 8.9, 10.1, 9.4]
    welch_res = st.ttest_ind(sample_a, sample_b, equal_var=False)
    # Welch-Satterthwaite df
    va = np.var(sample_a, ddof=1) / len(sample_a)
    vb = np.var(sample_b, ddof=1) / len(sample_b)
    welch_df = ((va + vb) ** 2) / ((va ** 2) / (len(sample_a) - 1) + (vb ** 2) / (len(sample_b) - 1))

    g1 = [10.2, 11.5, 10.8, 12.1, 11.0]
    g2 = [14.5, 15.2, 13.9, 14.8, 15.6]
    g3 = [9.1, 8.7, 9.5, 8.9, 9.3]
    anova_res = st.f_oneway(g1, g2, g3)

    obs = [58.0, 42.0, 35.0, 65.0]
    exp = [50.0, 50.0, 50.0, 50.0]
    chi_res = st.chisquare(obs, exp)

    suite["cases"]["hypothesis"] = {
        "welch_t": float(welch_res.statistic),
        "welch_df": float(welch_df),
        "welch_p": float(welch_res.pvalue),
        "anova_f": float(anova_res.statistic),
        "anova_p": float(anova_res.pvalue),
        "chi2_stat": float(chi_res.statistic),
        "chi2_p": float(chi_res.pvalue),
        "q25": float(np.quantile(sample_a, 0.25, method="linear")),
        "q50": float(np.quantile(sample_a, 0.50, method="linear")),
        "q75": float(np.quantile(sample_a, 0.75, method="linear")),
    }

    return suite


def fmt_yz_float(val: float) -> str:
    s = f"{val:.28f}".rstrip("0")
    if s.endswith("."):
        s += "0"
    return s


def generate_yaza_checker(suite):
    lines = [
        "# AUTO-GENERATED BY scripts/generate-data-science-golden.py — DO NOT EDIT BY HAND.",
        "# Cross-validates YAZA std/data/stats & std/math against SciPy / NumPy golden references.",
        'use "std/math"',
        'use "std/data/stats"',
        "",
        "fn rel_err(actual: Float64, expected: Float64) -> Float64:",
        "    let diff: Float64 = math_abs_float64(actual - expected)",
        "    let denom: Float64 = math_abs_float64(expected)",
        "    if denom < 0.000000000000001:",
        "        return diff",
        "    return diff / denom",
        "",
        "fn check_tol(label: Text, err_val: Float64, tol: Float64) -> Number:",
        "    if math_is_nan_float64(err_val) or err_val > tol:",
        '        say "GOLDEN_FAIL " + label + " err=" + float64_text(err_val) + " tol=" + float64_text(tol)',
        "        return 1",
        "    return 0",
        "",
        "start:",
        "    var failures: Number = 0",
        "    var max_ppf_rel: Float64 = 0.0",
        "    var max_cdf_abs: Float64 = 0.0",
        "    var max_sf_rel: Float64 = 0.0",
        "    var max_beta_abs: Float64 = 0.0",
        "    var max_gamma_abs: Float64 = 0.0",
        "    var max_t_abs: Float64 = 0.0",
        "    var max_f_abs: Float64 = 0.0",
        "    var max_chi2_abs: Float64 = 0.0",
        "",
    ]

    # 1. normal_ppf
    for idx, c in enumerate(suite["cases"]["normal_ppf"]):
        p_lit = fmt_yz_float(c["p"])
        exp_lit = fmt_yz_float(c["expected"])
        lines.append(f"    let ppf_{idx}: Float64 = result_unwrap(stats_normal_ppf({p_lit}))")
        lines.append(f"    let ppf_err_{idx}: Float64 = rel_err(ppf_{idx}, {exp_lit})")
        lines.append(f"    if ppf_err_{idx} > max_ppf_rel: set max_ppf_rel = ppf_err_{idx}")
        lines.append(f'    set failures = failures + check_tol("normal_ppf_{idx}", ppf_err_{idx}, 0.0000000000001)')

    # 2. normal_cdf & normal_sf
    for idx, c in enumerate(suite["cases"]["normal_cdf"]):
        z_lit = fmt_yz_float(c["z"])
        cdf_lit = fmt_yz_float(c["cdf"])
        sf_lit = fmt_yz_float(c["sf"])
        lines.append(f"    let cdf_{idx}: Float64 = stats_normal_cdf({z_lit})")
        lines.append(f"    let cdf_err_{idx}: Float64 = math_abs_float64(cdf_{idx} - {cdf_lit})")
        lines.append(f"    if cdf_err_{idx} > max_cdf_abs: set max_cdf_abs = cdf_err_{idx}")
        lines.append(f'    set failures = failures + check_tol("normal_cdf_{idx}", cdf_err_{idx}, 0.00000000000001)')
        lines.append(f"    let sf_{idx}: Float64 = stats_normal_sf({z_lit})")
        lines.append(f"    let sf_err_{idx}: Float64 = rel_err(sf_{idx}, {sf_lit})")
        lines.append(f"    if sf_err_{idx} > max_sf_rel: set max_sf_rel = sf_err_{idx}")
        lines.append(f'    set failures = failures + check_tol("normal_sf_{idx}", sf_err_{idx}, 0.000000000001)')

    # 3. betainc
    for idx, c in enumerate(suite["cases"]["betainc"]):
        a_lit = fmt_yz_float(c["a"])
        b_lit = fmt_yz_float(c["b"])
        x_lit = fmt_yz_float(c["x"])
        exp_lit = fmt_yz_float(c["expected"])
        lines.append(f"    let beta_{idx}: Float64 = math_betainc_reg_float64({a_lit}, {b_lit}, {x_lit})")
        lines.append(f"    let beta_err_{idx}: Float64 = math_abs_float64(beta_{idx} - {exp_lit})")
        lines.append(f"    if beta_err_{idx} > max_beta_abs: set max_beta_abs = beta_err_{idx}")
        lines.append(f'    set failures = failures + check_tol("betainc_{idx}", beta_err_{idx}, 0.000000000001)')

    # 4. gammainc_upper
    for idx, c in enumerate(suite["cases"]["gammainc_upper"]):
        a_lit = fmt_yz_float(c["a"])
        x_lit = fmt_yz_float(c["x"])
        exp_lit = fmt_yz_float(c["expected"])
        lines.append(f"    let gam_{idx}: Float64 = math_gammainc_upper_reg_float64({a_lit}, {x_lit})")
        lines.append(f"    let gam_err_{idx}: Float64 = math_abs_float64(gam_{idx} - {exp_lit})")
        lines.append(f"    if gam_err_{idx} > max_gamma_abs: set max_gamma_abs = gam_err_{idx}")
        lines.append(f'    set failures = failures + check_tol("gammainc_{idx}", gam_err_{idx}, 0.000000000001)')

    # 5. student_t_p
    for idx, c in enumerate(suite["cases"]["student_t_p"]):
        t_lit = fmt_yz_float(c["t"])
        df_lit = fmt_yz_float(c["df"])
        exp_lit = fmt_yz_float(c["expected"])
        lines.append(f"    let tp_{idx}: Float64 = stats_student_t_p_two_tailed({t_lit}, {df_lit})")
        lines.append(f"    let tp_err_{idx}: Float64 = math_abs_float64(tp_{idx} - {exp_lit})")
        lines.append(f"    if tp_err_{idx} > max_t_abs: set max_t_abs = tp_err_{idx}")
        lines.append(f'    set failures = failures + check_tol("student_t_{idx}", tp_err_{idx}, 0.00000000001)')

    # 6. f_dist_p
    for idx, c in enumerate(suite["cases"]["f_dist_p"]):
        f_lit = fmt_yz_float(c["f"])
        d1_lit = fmt_yz_float(c["d1"])
        d2_lit = fmt_yz_float(c["d2"])
        exp_lit = fmt_yz_float(c["expected"])
        lines.append(f"    let fp_{idx}: Float64 = stats_f_dist_p_upper({f_lit}, {d1_lit}, {d2_lit})")
        lines.append(f"    let fp_err_{idx}: Float64 = math_abs_float64(fp_{idx} - {exp_lit})")
        lines.append(f"    if fp_err_{idx} > max_f_abs: set max_f_abs = fp_err_{idx}")
        lines.append(f'    set failures = failures + check_tol("f_dist_{idx}", fp_err_{idx}, 0.00000000001)')

    # 7. chi2_p
    for idx, c in enumerate(suite["cases"]["chi2_p"]):
        c_lit = fmt_yz_float(c["chi2"])
        df_lit = fmt_yz_float(c["df"])
        exp_lit = fmt_yz_float(c["expected"])
        lines.append(f"    let cp_{idx}: Float64 = stats_chi2_p_upper({c_lit}, {df_lit})")
        lines.append(f"    let cp_err_{idx}: Float64 = math_abs_float64(cp_{idx} - {exp_lit})")
        lines.append(f"    if cp_err_{idx} > max_chi2_abs: set max_chi2_abs = cp_err_{idx}")
        lines.append(f'    set failures = failures + check_tol("chi2_{idx}", cp_err_{idx}, 0.00000000001)')

    # 8. End-to-end hypothesis tests
    h = suite["cases"]["hypothesis"]
    lines.extend([
        "    let sa: List[Float64] = List[Float64]()",
        "    list_push(sa, 12.4)",
        "    list_push(sa, 14.1)",
        "    list_push(sa, 13.8)",
        "    list_push(sa, 15.2)",
        "    list_push(sa, 11.9)",
        "    list_push(sa, 13.5)",
        "    list_push(sa, 14.7)",
        "    let sb: List[Float64] = List[Float64]()",
        "    list_push(sb, 9.8)",
        "    list_push(sb, 10.5)",
        "    list_push(sb, 11.2)",
        "    list_push(sb, 8.9)",
        "    list_push(sb, 10.1)",
        "    list_push(sb, 9.4)",
        "    let w_res: WelchTTestResult = result_unwrap(stats_welch_ttest(sa, sb))",
        f'    set failures = failures + check_tol("welch_t", rel_err(w_res.t_stat, {fmt_yz_float(h["welch_t"])}), 0.0000000001)',
        f'    set failures = failures + check_tol("welch_df", rel_err(w_res.degrees_of_freedom, {fmt_yz_float(h["welch_df"])}), 0.0000000001)',
        f'    set failures = failures + check_tol("welch_p", rel_err(w_res.p_value_two_tailed, {fmt_yz_float(h["welch_p"])}), 0.0000000001)',
        "    let fns: FiveNumberSummary = result_unwrap(stats_five_number_summary(sa))",
        f'    set failures = failures + check_tol("q25", rel_err(fns.q25, {fmt_yz_float(h["q25"])}), 0.000000000001)',
        f'    set failures = failures + check_tol("q50", rel_err(fns.median, {fmt_yz_float(h["q50"])}), 0.000000000001)',
        f'    set failures = failures + check_tol("q75", rel_err(fns.q75, {fmt_yz_float(h["q75"])}), 0.000000000001)',
        "    let g1: List[Float64] = List[Float64]()",
        "    list_push(g1, 10.2)",
        "    list_push(g1, 11.5)",
        "    list_push(g1, 10.8)",
        "    list_push(g1, 12.1)",
        "    list_push(g1, 11.0)",
        "    let g2: List[Float64] = List[Float64]()",
        "    list_push(g2, 14.5)",
        "    list_push(g2, 15.2)",
        "    list_push(g2, 13.9)",
        "    list_push(g2, 14.8)",
        "    list_push(g2, 15.6)",
        "    let g3: List[Float64] = List[Float64]()",
        "    list_push(g3, 9.1)",
        "    list_push(g3, 8.7)",
        "    list_push(g3, 9.5)",
        "    list_push(g3, 8.9)",
        "    list_push(g3, 9.3)",
        "    let grps: List[List[Float64]] = List[List[Float64]]()",
        "    list_push(grps, g1)",
        "    list_push(grps, g2)",
        "    list_push(grps, g3)",
        "    let aov: AnovaOneWayResult = result_unwrap(stats_anova_oneway(grps))",
        f'    set failures = failures + check_tol("anova_f", rel_err(aov.f_stat, {fmt_yz_float(h["anova_f"])}), 0.0000000001)',
        f'    let aov_p: Float64 = stats_f_dist_p_upper(aov.f_stat, float64_from_number(aov.df_between), float64_from_number(aov.df_within))',
        f'    set failures = failures + check_tol("anova_p", rel_err(aov_p, {fmt_yz_float(h["anova_p"])}), 0.0000000001)',
        "    let obs: List[Float64] = List[Float64]()",
        "    list_push(obs, 58.0)",
        "    list_push(obs, 42.0)",
        "    list_push(obs, 35.0)",
        "    list_push(obs, 65.0)",
        "    let exp_l: List[Float64] = List[Float64]()",
        "    list_push(exp_l, 50.0)",
        "    list_push(exp_l, 50.0)",
        "    list_push(exp_l, 50.0)",
        "    list_push(exp_l, 50.0)",
        "    let chi_res: ChiSquareResult = result_unwrap(stats_chi_square_gof(obs, exp_l))",
        f'    set failures = failures + check_tol("chi2_stat", rel_err(chi_res.chi_square_stat, {fmt_yz_float(h["chi2_stat"])}), 0.0000000001)',
        f'    set failures = failures + check_tol("chi2_p", rel_err(chi_res.p_value, {fmt_yz_float(h["chi2_p"])}), 0.0000000001)',
        '    say "  max_rel_err(normal_ppf / qnorm)      = " + float64_text(max_ppf_rel)',
        '    say "  max_abs_err(normal_cdf / pnorm)      = " + float64_text(max_cdf_abs)',
        '    say "  max_rel_err(normal_sf / tail)        = " + float64_text(max_sf_rel)',
        '    say "  max_abs_err(betainc / pbeta)         = " + float64_text(max_beta_abs)',
        '    say "  max_abs_err(gammainc_upper / pgamma) = " + float64_text(max_gamma_abs)',
        '    say "  max_abs_err(student_t_p / pt)        = " + float64_text(max_t_abs)',
        '    say "  max_abs_err(f_dist_p / pf)           = " + float64_text(max_f_abs)',
        '    say "  max_abs_err(chi2_p / pchisq)         = " + float64_text(max_chi2_abs)',
        "    if failures > 0:",
        "        exit(1)",
        '    say "GOLDEN_SCIPY_CROSS_VALIDATION_PASS cases=86 failures=0"',
    ])
    return "\n".join(lines) + "\n"


if __name__ == "__main__":
    suite = build_golden_suite()
    with open(GOLDEN_JSON_PATH, "w", encoding="utf-8") as f:
        json.dump(suite, f, indent=2)
    with open(GOLDEN_YZ_PATH, "w", encoding="utf-8") as f:
        f.write(generate_yaza_checker(suite))
    print(f"Generated {GOLDEN_JSON_PATH} and {GOLDEN_YZ_PATH}")
