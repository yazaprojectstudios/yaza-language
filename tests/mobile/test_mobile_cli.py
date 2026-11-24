#!/usr/bin/env python3
"""Integration tests for `yaza new/init --mobile` and `yaza mobile run` smart detection & live reload."""

from __future__ import annotations

import json
import os
import pathlib
import shutil
import signal
import subprocess
import sys
import tempfile
import time


REPO_ROOT = pathlib.Path(__file__).resolve().parents[2]
YAZA_BIN = pathlib.Path(os.environ.get("YAZA_BIN") or (REPO_ROOT / "build" / "yaza-llvm"))


def base_env(extra: dict[str, str] | None = None) -> dict[str, str]:
    env = os.environ.copy()
    env["YAZA_STDLIB_DIR"] = str(REPO_ROOT / "std")
    env["YAZA_RUNTIME_DIR"] = str(REPO_ROOT / "runtime")
    env["YAZA_BIN"] = str(YAZA_BIN)
    if extra:
        env.update(extra)
    return env


def run_cli(args: list[str], cwd: pathlib.Path, extra_env: dict[str, str] | None = None, stdin_text: str | None = None, timeout: float = 30.0) -> subprocess.CompletedProcess:
    kwargs: dict = {
        "cwd": str(cwd),
        "env": base_env(extra_env),
        "stdout": subprocess.PIPE,
        "stderr": subprocess.STDOUT,
        "text": True,
        "check": False,
        "timeout": timeout,
    }
    if stdin_text is not None:
        kwargs["input"] = stdin_text
    else:
        kwargs["stdin"] = subprocess.DEVNULL
    return subprocess.run([str(YAZA_BIN), *args], **kwargs)


def make_mock_toolchain(mock_bin: pathlib.Path, state_file: pathlib.Path, scenario: str) -> pathlib.Path:
    mock_bin.mkdir(parents=True, exist_ok=True)
    log_file = mock_bin / "invocations.log"

    def write_exec(name: str, body: str) -> None:
        p = mock_bin / name
        p.write_text(body, encoding="utf-8")
        p.chmod(0o755)

    # Mock gradle creates a dummy APK in output path
    write_exec(
        "gradle",
        f"""#!/usr/bin/env python3
import os, pathlib, sys
with open({str(log_file)!r}, 'a') as f: f.write('gradle ' + ' '.join(sys.argv[1:]) + '\\n')
apk_dir = pathlib.Path.cwd() / 'app' / 'build' / 'outputs' / 'apk' / 'debug'
apk_dir.mkdir(parents=True, exist_ok=True)
(apk_dir / 'app-debug.apk').write_bytes(b'PK\\x03\\x04mockapk')
print('BUILD SUCCESSFUL (mock gradle)')
""",
    )

    # Mock adb handles devices -l, emu avd name, shell getprop, install, am start
    write_exec(
        "adb",
        f"""#!/usr/bin/env python3
import json, pathlib, sys
args = sys.argv[1:]
with open({str(log_file)!r}, 'a') as f: f.write('adb ' + ' '.join(args) + '\\n')
st = json.loads(pathlib.Path({str(state_file)!r}).read_text())
if args[:2] == ['devices', '-l']:
    print('List of devices attached')
    for d in st.get('adb_devices', []):
        print(d)
    sys.exit(0)
if '-s' in args:
    idx = args.index('-s')
    rest = args[idx+2:]
    if rest == ['emu', 'avd', 'name']:
        serial = args[idx+1]
        print(st.get('avd_names', {{}}).get(serial, 'Pixel_Mock'))
        print('OK')
        sys.exit(0)
    if rest == ['shell', 'getprop', 'sys.boot_completed']:
        print('1' if st.get('boot_completed', True) else '0')
        sys.exit(0)
    if rest[:1] == ['install'] or rest[:2] == ['shell', 'am'] or rest[:2] == ['shell', 'monkey']:
        print('Success')
        sys.exit(0)
sys.exit(0)
""",
    )

    # Mock emulator handles -list-avds and -avd <name>
    write_exec(
        "emulator",
        f"""#!/usr/bin/env python3
import json, pathlib, sys, time
args = sys.argv[1:]
with open({str(log_file)!r}, 'a') as f: f.write('emulator ' + ' '.join(args) + '\\n')
st = json.loads(pathlib.Path({str(state_file)!r}).read_text())
if args == ['-list-avds']:
    for a in st.get('avds', []):
        print(a)
    sys.exit(0)
if '-avd' in args:
    avd = args[args.index('-avd') + 1]
    st['adb_devices'] = ['emulator-5556 device product:sdk model:Pixel_Booted_AVD']
    st['avd_names'] = {{'emulator-5556': avd}}
    st['boot_completed'] = True
    pathlib.Path({str(state_file)!r}).write_text(json.dumps(st))
    time.sleep(0.05)
    sys.exit(0)
sys.exit(0)
""",
    )

    # Mock xcrun handles simctl list / boot / bootstatus / install / launch
    write_exec(
        "xcrun",
        f"""#!/usr/bin/env python3
import json, pathlib, sys
args = sys.argv[1:]
with open({str(log_file)!r}, 'a') as f: f.write('xcrun ' + ' '.join(args) + '\\n')
st = json.loads(pathlib.Path({str(state_file)!r}).read_text())
if args[:4] == ['simctl', 'list', 'devices', 'available'] and '-j' in args:
    print(json.dumps(st.get('simctl_json', {{}})))
    sys.exit(0)
if args[:2] == ['simctl', 'boot']:
    print('Booted (mock)')
    sys.exit(0)
if args[:2] == ['simctl', 'bootstatus']:
    print('Device ready (mock)')
    sys.exit(0)
if args[:2] in (['simctl', 'install'], ['simctl', 'launch']):
    print('OK (mock)')
    sys.exit(0)
# Delegate real xcrun for clang/ar inside build-engine.sh if needed
import os
os.execv('/usr/bin/xcrun', ['xcrun'] + args)
""",
    )

    write_exec(
        "open",
        f"""#!/usr/bin/env python3
import sys
with open({str(log_file)!r}, 'a') as f: f.write('open ' + ' '.join(sys.argv[1:]) + '\\n')
print('Opened Simulator (mock)')
""",
    )

    write_exec(
        "xcodebuild",
        f"""#!/usr/bin/env python3
import pathlib, sys
args = sys.argv[1:]
with open({str(log_file)!r}, 'a') as f: f.write('xcodebuild ' + ' '.join(args) + '\\n')
derived = pathlib.Path(args[args.index('-derivedDataPath') + 1])
scheme = args[args.index('-scheme') + 1]
app_dir = derived / 'Build' / 'Products' / 'Debug-iphonesimulator' / (scheme + '.app')
app_dir.mkdir(parents=True, exist_ok=True)
(app_dir / 'Info.plist').write_text('<plist/>', encoding='utf-8')
print('** BUILD SUCCEEDED ** (mock)')
""",
    )
    return log_file


def test_help_and_templates(tmp: pathlib.Path) -> None:
    print("1. Testing --help & new/init --mobile modular structure...")
    res_help = run_cli(["mobile", "--help"], REPO_ROOT)
    assert res_help.returncode == 0, res_help.stdout
    assert "run [android|ios] [--target <android|ios>] [--once]" in res_help.stdout, res_help.stdout
    assert "android-emulator -> android" in res_help.stdout, res_help.stdout

    # yaza new --mobile
    proj_new = tmp / "my_new_app"
    res_new = run_cli(["new", "--mobile", "my_new_app"], tmp)
    assert res_new.returncode == 0, res_new.stdout
    for rel in [
        "yaza.toml",
        "README.md",
        "src/main.yz",
        "src/mobile_engine.yz",
        "src/state/app_state.yz",
        "src/state/codec.yz",
        "src/actions/home_actions.yz",
        "src/domain/greeting.yz",
        "src/effects/outbox.yz",
        "mobile/android/app/src/main/java/dev/yaza/mobile/MainActivity.kt",
        "mobile/android/app/src/main/java/dev/yaza/mobile/NativeEngine.kt",
        "mobile/ios/YazaMobileApp.swift",
    ]:
        assert (proj_new / rel).is_file(), f"Missing {rel}"

    # Verify MVI "Welcome to YAZA Mobile" in native shells and YBIF usage
    assert "Welcome to YAZA Mobile" in (proj_new / "mobile/android/app/src/main/java/dev/yaza/mobile/MainActivity.kt").read_text()
    assert "decodeYbifState" in (proj_new / "mobile/android/app/src/main/java/dev/yaza/mobile/NativeEngine.kt").read_text()
    assert "decodeYbifState" in (proj_new / "mobile/ios/YazaMobileApp.swift").read_text()

    # CLI smoke test via yaza run src/main.yz
    res_smoke = run_cli(["run", "src/main.yz"], proj_new)
    assert res_smoke.returncode == 0, res_smoke.stdout
    assert "Welcome to YAZA Mobile" in res_smoke.stdout, res_smoke.stdout

    # yaza init --mobile in existing dir
    proj_init = tmp / "init-existing-dir"
    proj_init.mkdir()
    res_init = run_cli(["init", "--mobile"], proj_init)
    assert res_init.returncode == 0, res_init.stdout
    assert (proj_init / "yaza.toml").is_file()
    assert (proj_init / "src/state/codec.yz").is_file()
    # Second init in same dir must fail safely
    res_init_dup = run_cli(["init", "--mobile"], proj_init)
    assert res_init_dup.returncode != 0
    print("   PASS")


def test_mobile_build_cross_targets(proj: pathlib.Path) -> None:
    print("2. Testing real native engine cross-builds (android-arm64 & ios-simulator-arm64)...")
    res_doc = run_cli(["mobile", "doctor"], proj)
    assert res_doc.returncode == 0, res_doc.stdout
    if "NDK: found" in res_doc.stdout:
        res_and = run_cli(["mobile", "build", "--target", "android-arm64", "--debug"], proj)
        assert res_and.returncode == 0, res_and.stdout
        assert (proj / "build/mobile/android-arm64/libyaza_mobile.so").is_file()
        assert (proj / "mobile/android/app/build/generated/yazaJniLibs/arm64-v8a/libyaza_mobile.so").is_file()
        print("   android-arm64 PASS")
    if "Xcode iOS SDK: found" in res_doc.stdout:
        res_ios = run_cli(["mobile", "build", "--target", "ios-simulator-arm64", "--debug"], proj)
        assert res_ios.returncode == 0, res_ios.stdout
        assert (proj / "build/mobile/ios-simulator-arm64/libyaza_mobile.a").is_file()
        print("   ios-simulator-arm64 PASS")


def test_smart_detection_scenarios(proj: pathlib.Path, tmp: pathlib.Path) -> None:
    print("3. Testing smart Android/iOS device detection, multi-device prompt & auto-boot...")
    mock_bin = tmp / "mock_bin"
    state_file = tmp / "mock_state.json"

    # A. Android 1 active device (auto-select)
    state_file.write_text(json.dumps({
        "adb_devices": ["emulator-5554 device product:sdk model:Pixel_8_API_35"],
        "avd_names": {"emulator-5554": "Pixel_8_API_35"},
    }))
    log_file = make_mock_toolchain(mock_bin, state_file, "android1")
    extra = {"PATH": f"{mock_bin}:{os.environ['PATH']}", "YAZA_GRADLE_BIN": str(mock_bin / "gradle"), "YAZA_ADB_BIN": str(mock_bin / "adb"), "YAZA_EMULATOR_BIN": str(mock_bin / "emulator"), "YAZA_MOBILE_BOOT_TIMEOUT": "10", "YAZA_MOBILE_POLL_INTERVAL": "0.05"}
    res = run_cli(["mobile", "run", "--target", "android", "--once"], proj, extra)
    assert res.returncode == 0, res.stdout
    assert "Auto-selected target: emulator-5554 (Pixel_8_API_35)" in res.stdout, res.stdout

    # A2. Positional syntax `yaza mobile run android --once` (without --target)
    res_pos = run_cli(["mobile", "run", "android", "--once"], proj, extra)
    assert res_pos.returncode == 0, res_pos.stdout
    assert "Auto-selected target: emulator-5554 (Pixel_8_API_35)" in res_pos.stdout, res_pos.stdout

    # B. Android 2 active devices (interactive pick #2 + legacy alias android-emulator)
    log_file.write_text("")
    state_file.write_text(json.dumps({
        "adb_devices": [
            "emulator-5554 device model:Pixel_7",
            "emulator-5558 device model:Pixel_9_Pro",
        ],
        "avd_names": {"emulator-5554": "Pixel_7_AVD", "emulator-5558": "Pixel_9_AVD"},
    }))
    res = run_cli(["mobile", "run", "--target", "android-emulator", "--once"], proj, {**extra, "YAZA_MOBILE_FORCE_TTY": "1"}, stdin_text="2\n")
    assert res.returncode == 0, res.stdout
    assert "[2] emulator-5558 (Pixel_9_AVD)" in res.stdout, res.stdout
    assert "-s emulator-5558" in log_file.read_text(), log_file.read_text()

    # C. Android 0 active devices, 1 AVD -> auto-boot & wait sys.boot_completed
    log_file.write_text("")
    state_file.write_text(json.dumps({"adb_devices": [], "avds": ["Auto_Pixel_AVD"]}))
    res = run_cli(["mobile", "run", "--target", "android", "--once"], proj, extra)
    assert res.returncode == 0, res.stdout
    assert "Booting single AVD: Auto_Pixel_AVD" in res.stdout, res.stdout
    assert "emulator -avd Auto_Pixel_AVD" in log_file.read_text(), log_file.read_text()

    # D. Android 0 active devices, 2 AVDs -> prompt #2
    log_file.write_text("")
    state_file.write_text(json.dumps({"adb_devices": [], "avds": ["Small_Phone", "Pixel_Fold_AVD"]}))
    res = run_cli(["mobile", "run", "--target", "android", "--once"], proj, {**extra, "YAZA_MOBILE_FORCE_TTY": "1"}, stdin_text="2\n")
    assert res.returncode == 0, res.stdout
    assert "emulator -avd Pixel_Fold_AVD" in log_file.read_text(), log_file.read_text()

    # E. iOS 1 Booted simulator (auto-select)
    log_file.write_text("")
    state_file.write_text(json.dumps({
        "simctl_json": {
            "devices": {
                "com.apple.CoreSimulator.SimRuntime.iOS-18-2": [
                    {"udid": "UDID-111", "name": "iPhone 16 Pro", "state": "Booted", "isAvailable": True},
                ]
            }
        }
    }))
    res = run_cli(["mobile", "run", "--target", "ios", "--once"], proj, extra)
    assert res.returncode == 0, res.stdout
    assert "Auto-selected simulator: iPhone 16 Pro (iOS 18.2 · UDID-111)" in res.stdout, res.stdout

    # F. iOS 2 Booted simulators -> prompt #2 + legacy alias ios-simulator
    log_file.write_text("")
    state_file.write_text(json.dumps({
        "simctl_json": {
            "devices": {
                "com.apple.CoreSimulator.SimRuntime.iOS-18-2": [
                    {"udid": "UDID-111", "name": "iPhone 16", "state": "Booted", "isAvailable": True},
                    {"udid": "UDID-222", "name": "iPhone 17 Pro", "state": "Booted", "isAvailable": True},
                ]
            }
        }
    }))
    res = run_cli(["mobile", "run", "--target", "ios-simulator", "--once"], proj, {**extra, "YAZA_MOBILE_FORCE_TTY": "1"}, stdin_text="2\n")
    assert res.returncode == 0, res.stdout
    assert "UDID-222" in log_file.read_text(), log_file.read_text()

    # G. iOS 0 Booted -> auto-boot single available iPhone + open -a Simulator + bootstatus
    log_file.write_text("")
    state_file.write_text(json.dumps({
        "simctl_json": {
            "devices": {
                "com.apple.CoreSimulator.SimRuntime.iOS-26-4": [
                    {"udid": "UDID-999", "name": "iPhone 17 Pro", "state": "Shutdown", "isAvailable": True},
                ]
            }
        }
    }))
    res = run_cli(["mobile", "run", "--target", "ios", "--once"], proj, extra)
    assert res.returncode == 0, res.stdout
    assert "simctl boot UDID-999" in log_file.read_text(), log_file.read_text()
    assert "open -a Simulator" in log_file.read_text(), log_file.read_text()
    assert "simctl bootstatus UDID-999 -b" in log_file.read_text(), log_file.read_text()

    # H. Omitted --target (non-TTY usage vs TTY prompt choosing iOS=2)
    res_no_tty = run_cli(["mobile", "run", "--once"], proj, extra)
    assert res_no_tty.returncode == 2
    assert "Usage: yaza mobile run" in res_no_tty.stdout
    res_tty = run_cli(["mobile", "run", "--once"], proj, {**extra, "YAZA_MOBILE_FORCE_TTY": "1"}, stdin_text="2\n")
    assert res_tty.returncode == 0, res_tty.stdout
    print("   PASS")


def test_live_watch_and_reload(proj: pathlib.Path, tmp: pathlib.Path) -> None:
    print("4. Testing live watch & hot reload (rebuild -> compile error resilience -> recover -> SIGINT)...")
    mock_bin = tmp / "mock_bin"
    state_file = tmp / "mock_state.json"
    state_file.write_text(json.dumps({
        "adb_devices": ["emulator-5554 device model:Pixel_Watch_Test"],
        "avd_names": {"emulator-5554": "Pixel_Watch_Test"},
    }))
    make_mock_toolchain(mock_bin, state_file, "watch")
    extra = base_env({
        "PATH": f"{mock_bin}:{os.environ['PATH']}",
        "YAZA_GRADLE_BIN": str(mock_bin / "gradle"),
        "YAZA_ADB_BIN": str(mock_bin / "adb"),
        "YAZA_EMULATOR_BIN": str(mock_bin / "emulator"),
    })
    out_log = tmp / "watch_output.log"
    with out_log.open("w", encoding="utf-8") as fp:
        proc = subprocess.Popen(
            [str(YAZA_BIN), "mobile", "run", "--target", "android"],
            cwd=str(proj),
            env=extra,
            stdout=fp,
            stderr=subprocess.STDOUT,
            text=True,
            start_new_session=True,
        )
        try:
            def wait_for(substr: str, timeout: float = 20.0) -> str:
                deadline = time.monotonic() + timeout
                while time.monotonic() < deadline:
                    txt = out_log.read_text(encoding="utf-8", errors="ignore")
                    if substr in txt:
                        return txt
                    if proc.poll() is not None:
                        raise AssertionError(f"Watcher exited early ({proc.returncode}):\n{txt}")
                    time.sleep(0.1)
                raise AssertionError(f"Timeout waiting for {substr!r}.\nLog:\n{out_log.read_text()}")

            wait_for("Live Watch active")

            # 1) Valid edit in domain/greeting.yz -> triggers rebuild + reload
            domain_file = proj / "src" / "domain" / "greeting.yz"
            orig_domain = domain_file.read_text(encoding="utf-8")
            domain_file.write_text(orig_domain.replace("Hello, ", "Hi, "), encoding="utf-8")
            wait_for("changed -> rebuilt in")

            # 2) Syntax/compile error -> watcher prints error WITHOUT exiting
            engine_file = proj / "src" / "mobile_engine.yz"
            orig_engine = engine_file.read_text(encoding="utf-8")
            engine_file.write_text(orig_engine + "\nTHIS_IS_INVALID_SYNTAX(((\n", encoding="utf-8")
            wait_for("YAZA compile failed after")
            assert proc.poll() is None, "Watcher should survive compile errors!"

            # 3) Fix syntax error -> automatically rebuilds & reloads again
            engine_file.write_text(orig_engine + "\n", encoding="utf-8")
            wait_for("src/mobile_engine.yz changed -> rebuilt in")

            # 4) Clean SIGINT shutdown (deliver to foreground process group like terminal Ctrl+C)
            os.killpg(proc.pid, signal.SIGINT)
            rc = proc.wait(timeout=5)
            assert rc == 0, f"Expected clean SIGINT exit 0, got {rc}"
            final_log = out_log.read_text(encoding="utf-8")
            assert "Live reload stopped cleanly." in final_log, final_log
        finally:
            if proc.poll() is None:
                try:
                    os.killpg(proc.pid, signal.SIGKILL)
                except OSError:
                    proc.kill()
                proc.wait()
    print("   PASS")


def main() -> int:
    if not YAZA_BIN.is_file():
        print(f"Missing compiler binary: {YAZA_BIN}", file=sys.stderr)
        return 1
    with tempfile.TemporaryDirectory(prefix="yaza-mobile-cli-test-") as td:
        tmp = pathlib.Path(td).resolve()
        test_help_and_templates(tmp)
        proj = tmp / "my_new_app"
        test_mobile_build_cross_targets(proj)
        test_smart_detection_scenarios(proj, tmp)
        test_live_watch_and_reload(proj, tmp)
    print("ALL YAZA MOBILE CLI & LIVE RELOAD TESTS PASSED")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
