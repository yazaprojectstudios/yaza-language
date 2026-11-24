#!/usr/bin/env python3
"""YAZA Mobile smart target selector, emulator/simulator runner, and live reload loop."""

from __future__ import annotations

import base64
import json
import os
import pathlib
import re
import shlex
import shutil
import signal
import subprocess
import sys
import time
from typing import Dict, List, Optional, Tuple


STOP_REQUESTED = False
ACTIVE_CHILD: Optional[subprocess.Popen] = None


def handle_signal(signum: int, _frame: object) -> None:
    global STOP_REQUESTED
    STOP_REQUESTED = True
    if ACTIVE_CHILD is not None and ACTIVE_CHILD.poll() is None:
        try:
            ACTIVE_CHILD.terminate()
        except OSError:
            pass


def now_hms() -> str:
    return time.strftime("%H:%M:%S")


def is_interactive() -> bool:
    return sys.stdin.isatty() or os.environ.get("YAZA_MOBILE_FORCE_TTY") == "1"


def prompt_choice(title: str, options: List[str]) -> int:
    print(title, flush=True)
    for idx, label in enumerate(options, 1):
        print(f"  [{idx}] {label}", flush=True)
    while not STOP_REQUESTED:
        try:
            raw = input(f"Select number [1-{len(options)}]: ").strip()
        except KeyboardInterrupt:
            raise SystemExit(0)
        except EOFError:
            raise SystemExit(1)
        if raw.isdigit():
            val = int(raw)
            if 1 <= val <= len(options):
                return val - 1
        print(f"Invalid choice; enter a number from 1 to {len(options)}.", file=sys.stderr, flush=True)
    raise SystemExit(0)


def normalize_target(raw_target: str) -> str:
    if raw_target == "android-emulator" or raw_target == "android":
        return "android"
    if raw_target == "ios-simulator" or raw_target == "ios":
        return "ios"
    if raw_target in ("", "auto"):
        if not is_interactive():
            print("Usage: yaza mobile run [android|ios] [--target <android|ios>] [--once]", file=sys.stderr)
            print("  --target android   Build, run, and watch on an Android emulator/device", file=sys.stderr)
            print("  --target ios       Build, run, and watch on an iOS Simulator (macOS)", file=sys.stderr)
            print("  --once             Run build & launch once without live file watching", file=sys.stderr)
            raise SystemExit(2)
        choice = prompt_choice("Select mobile target platform:", ["android (Android Emulator / Device)", "ios (iOS Simulator)"])
        return "android" if choice == 0 else "ios"
    print(f"error: unsupported mobile run target: {raw_target}", file=sys.stderr)
    raise SystemExit(2)


def run_cmd(cmd: List[str], cwd: Optional[str] = None, env: Optional[Dict[str, str]] = None, capture: bool = False) -> Tuple[int, str]:
    global ACTIVE_CHILD
    if capture:
        proc = subprocess.run(cmd, cwd=cwd, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        return proc.returncode, proc.stdout or ""
    proc = subprocess.Popen(cmd, cwd=cwd, env=env)
    ACTIVE_CHILD = proc
    try:
        rc = proc.wait()
    finally:
        ACTIVE_CHILD = None
    return rc, ""


def find_adb() -> str:
    adb = os.environ.get("YAZA_ADB_BIN") or shutil.which("adb") or ""
    if not adb:
        sdk = os.environ.get("ANDROID_HOME") or os.environ.get("ANDROID_SDK_ROOT") or ""
        if sdk and os.path.isfile(os.path.join(sdk, "platform-tools", "adb")):
            adb = os.path.join(sdk, "platform-tools", "adb")
    if not adb:
        print("error: Android platform-tools (adb) are required", file=sys.stderr)
        raise SystemExit(1)
    return adb


def find_emulator() -> str:
    emu = os.environ.get("YAZA_EMULATOR_BIN") or shutil.which("emulator") or ""
    if not emu:
        sdk = os.environ.get("ANDROID_HOME") or os.environ.get("ANDROID_SDK_ROOT") or ""
        if sdk and os.path.isfile(os.path.join(sdk, "emulator", "emulator")):
            emu = os.path.join(sdk, "emulator", "emulator")
    return emu


def find_gradle() -> str:
    gradle = os.environ.get("YAZA_GRADLE_BIN") or shutil.which("gradle") or ""
    if not gradle:
        print("error: Gradle is required; install it or set YAZA_GRADLE_BIN", file=sys.stderr)
        raise SystemExit(1)
    return gradle


def parse_adb_devices(output: str, adb_bin: str) -> List[Tuple[str, str]]:
    devices: List[Tuple[str, str]] = []
    for line in output.splitlines():
        line = line.strip()
        if not line or line.startswith("List of devices"):
            continue
        parts = line.split()
        if len(parts) < 2 or parts[1] != "device":
            continue
        serial = parts[0]
        model = ""
        for token in parts[2:]:
            if token.startswith("model:"):
                model = token.split(":", 1)[1].replace("_", " ")
                break
        if serial.startswith("emulator-"):
            rc, avd_out = run_cmd([adb_bin, "-s", serial, "emu", "avd", "name"], capture=True)
            avd_lines = [ln.strip() for ln in avd_out.splitlines() if ln.strip() and ln.strip() != "OK"]
            if rc == 0 and avd_lines:
                model = avd_lines[0]
        label = model or "Android device"
        devices.append((serial, label))
    return devices


def detect_android_device(adb_bin: str) -> Tuple[str, str]:
    rc, out = run_cmd([adb_bin, "devices", "-l"], capture=True)
    if rc != 0:
        print(f"error: failed to list Android devices: {out.strip()}", file=sys.stderr)
        raise SystemExit(1)
    active = parse_adb_devices(out, adb_bin)
    if len(active) == 1:
        serial, label = active[0]
        print(f"✓ Auto-selected target: {serial} ({label})", flush=True)
        return serial, label
    if len(active) > 1:
        idx = prompt_choice(
            "Multiple active Android devices/emulators found:",
            [f"{s} ({lbl})" for s, lbl in active],
        )
        return active[idx]

    # 0 active devices -> inspect available AVDs via emulator -list-avds
    emu_bin = find_emulator()
    if not emu_bin:
        print("error: no active Android device/emulator found and the 'emulator' binary was not found.", file=sys.stderr)
        raise SystemExit(1)
    rc_avd, avd_out = run_cmd([emu_bin, "-list-avds"], capture=True)
    avds = [ln.strip() for ln in avd_out.splitlines() if ln.strip()]
    if rc_avd != 0 or not avds:
        print("error: no Android AVDs available. Create an AVD in Android Studio and try again.", file=sys.stderr)
        raise SystemExit(1)
    if len(avds) == 1:
        chosen_avd = avds[0]
        print(f"↺ No active emulator found. Booting single AVD: {chosen_avd} ...", flush=True)
    else:
        idx = prompt_choice("No active emulator found. Select an AVD to boot:", avds)
        chosen_avd = avds[idx]
        print(f"↺ Booting AVD: {chosen_avd} ...", flush=True)

    subprocess.Popen(
        [emu_bin, "-avd", chosen_avd],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        start_new_session=True,
    )

    timeout_s = float(os.environ.get("YAZA_MOBILE_BOOT_TIMEOUT", "180"))
    poll_s = float(os.environ.get("YAZA_MOBILE_POLL_INTERVAL", "1.0"))
    deadline = time.monotonic() + timeout_s
    while not STOP_REQUESTED and time.monotonic() < deadline:
        rc_d, out_d = run_cmd([adb_bin, "devices", "-l"], capture=True)
        if rc_d == 0:
            online = parse_adb_devices(out_d, adb_bin)
            if online:
                serial, label = online[0]
                rc_b, prop_out = run_cmd([adb_bin, "-s", serial, "shell", "getprop", "sys.boot_completed"], capture=True)
                if rc_b == 0 and prop_out.strip() == "1":
                    print(f"✓ Emulator ready: {serial} ({chosen_avd})", flush=True)
                    return serial, chosen_avd
        time.sleep(poll_s)

    print("error: timed out waiting for Android emulator to finish booting (sys.boot_completed != 1).", file=sys.stderr)
    raise SystemExit(1)


def parse_simctl_runtime_label(runtime_key: str) -> str:
    # Example: com.apple.CoreSimulator.SimRuntime.iOS-18-2 -> iOS 18.2
    tail = runtime_key.rsplit(".", 1)[-1]
    if "-" in tail:
        parts = tail.split("-")
        return f"{parts[0]} {'.'.join(parts[1:])}"
    return tail


def detect_ios_simulator() -> Tuple[str, str, str]:
    rc, out = run_cmd(["xcrun", "simctl", "list", "devices", "available", "-j"], capture=True)
    if rc != 0:
        print(f"error: failed to query iOS simulators: {out.strip()}", file=sys.stderr)
        raise SystemExit(1)
    try:
        data = json.loads(out)
    except json.JSONDecodeError as exc:
        print(f"error: invalid JSON from simctl: {exc}", file=sys.stderr)
        raise SystemExit(1)

    booted: List[Tuple[str, str, str]] = []
    available_iphones: List[Tuple[str, str, str]] = []
    for runtime_key, dev_list in data.get("devices", {}).items():
        rt_label = parse_simctl_runtime_label(runtime_key)
        for dev in dev_list:
            if dev.get("isAvailable") is False:
                continue
            udid = dev.get("udid", "")
            name = dev.get("name", "iPhone Simulator")
            state = dev.get("state", "")
            if state == "Booted":
                booted.append((udid, name, rt_label))
            if "iPhone" in name or "iPhone" in dev.get("deviceTypeIdentifier", ""):
                available_iphones.append((udid, name, rt_label))

    if len(booted) == 1:
        udid, name, rt = booted[0]
        print(f"✓ Auto-selected simulator: {name} ({rt} · {udid})", flush=True)
        return udid, name, rt
    if len(booted) > 1:
        idx = prompt_choice(
            "Multiple booted iOS simulators found:",
            [f"{name} · {rt} ({udid})" for udid, name, rt in booted],
        )
        return booted[idx]

    # 0 Booted
    if not available_iphones:
        print("error: no available iPhone simulators found in Xcode.", file=sys.stderr)
        raise SystemExit(1)
    if len(available_iphones) == 1:
        udid, name, rt = available_iphones[0]
        print(f"↺ No booted simulator found. Booting single simulator: {name} ({rt}) ...", flush=True)
    else:
        idx = prompt_choice(
            "No booted simulator found. Select an iPhone simulator to boot:",
            [f"{name} · {rt} ({udid})" for udid, name, rt in available_iphones],
        )
        udid, name, rt = available_iphones[idx]
        print(f"↺ Booting simulator: {name} ({rt}) ...", flush=True)

    run_cmd(["xcrun", "simctl", "boot", udid], capture=True)
    run_cmd(["open", "-a", "Simulator"], capture=True)
    rc_wait, wait_out = run_cmd(["xcrun", "simctl", "bootstatus", udid, "-b"], capture=True)
    if rc_wait != 0:
        print(f"error: simulator bootstatus failed: {wait_out.strip()}", file=sys.stderr)
        raise SystemExit(1)
    print(f"✓ Simulator ready: {name} ({rt} · {udid})", flush=True)
    return udid, name, rt


def discover_android_metadata(project_root: pathlib.Path) -> Tuple[str, str]:
    app_id = "dev.yaza.mobile"
    activity = ".MainActivity"
    gradle_kts = project_root / "mobile" / "android" / "app" / "build.gradle.kts"
    if gradle_kts.is_file():
        m = re.search(r'applicationId\s*=\s*"([^"]+)"', gradle_kts.read_text(encoding="utf-8", errors="ignore"))
        if m:
            app_id = m.group(1)
    manifest = project_root / "mobile" / "android" / "app" / "src" / "main" / "AndroidManifest.xml"
    if manifest.is_file():
        m = re.search(r'<activity[^>]*android:name="([^"]+)"', manifest.read_text(encoding="utf-8", errors="ignore"))
        if m:
            activity = m.group(1)
    return app_id, activity


def discover_ios_metadata(project_root: pathlib.Path) -> Tuple[pathlib.Path, str, str]:
    ios_dir = project_root / "mobile" / "ios"
    projs = sorted(ios_dir.glob("*.xcodeproj"))
    proj_path = projs[0] if projs else (ios_dir / "YazaMobile.xcodeproj")
    scheme = proj_path.stem
    schemes_dir = proj_path / "xcshareddata" / "xcschemes"
    if schemes_dir.is_dir():
        schemes = sorted(schemes_dir.glob("*.xcscheme"))
        if schemes:
            scheme = schemes[0].stem
    bundle_id = "dev.yaza.mobile"
    pbxproj = proj_path / "project.pbxproj"
    if pbxproj.is_file():
        m = re.search(r'PRODUCT_BUNDLE_IDENTIFIER\s*=\s*([^;\s"]+)', pbxproj.read_text(encoding="utf-8", errors="ignore"))
        if m:
            bundle_id = m.group(1)
    return proj_path, scheme, bundle_id


def find_apk(project_root: pathlib.Path) -> pathlib.Path:
    apk_dir = project_root / "mobile" / "android" / "app" / "build" / "outputs" / "apk" / "debug"
    apks = sorted(apk_dir.glob("*.apk"))
    if apks:
        return apks[0]
    return apk_dir / "app-debug.apk"


def build_core_engine(yaza_bin: str, core_target: str, project_root: str) -> Tuple[bool, int, str]:
    t0 = time.monotonic()
    rc, out = run_cmd([yaza_bin, "mobile", "build", "--target", core_target, "--debug"], cwd=project_root, capture=True)
    elapsed_ms = int((time.monotonic() - t0) * 1000)
    return rc == 0, elapsed_ms, out


def deploy_android(
    yaza_bin: str,
    project_root: pathlib.Path,
    adb_bin: str,
    gradle_bin: str,
    serial: str,
    rebuild_apk: bool = True,
) -> Tuple[bool, str]:
    app_id, activity = discover_android_metadata(project_root)
    env = os.environ.copy()
    env["YAZA_SKIP_ENGINE_BUILD"] = "1"
    if rebuild_apk:
        rc, g_out = run_cmd([gradle_bin, "assembleDebug"], cwd=str(project_root / "mobile" / "android"), env=env, capture=True)
        if rc != 0:
            return False, f"Gradle build failed:\n{g_out}"
    apk_path = find_apk(project_root)
    if not apk_path.is_file():
        return False, f"Debug APK not found: {apk_path}"
    rc_in, in_out = run_cmd([adb_bin, "-s", serial, "install", "-r", "-t", str(apk_path)], capture=True)
    if rc_in != 0:
        return False, f"adb install failed:\n{in_out}"
    run_cmd(
        [adb_bin, "-s", serial, "shell", "run-as", app_id, "sh", "-c", "rm -f files/yaza_hotswap_ready.txt files/yaza_hotswap_*.so"],
        capture=True,
    )
    run_cmd([adb_bin, "-s", serial, "shell", "am", "force-stop", app_id], capture=True)
    comp = f"{app_id}/{activity}"
    rc_st, _ = run_cmd([adb_bin, "-s", serial, "shell", "am", "start", "-n", comp], capture=True)
    if rc_st != 0:
        rc_mk, mk_out = run_cmd([adb_bin, "-s", serial, "shell", "monkey", "-p", app_id, "1"], capture=True)
        if rc_mk != 0:
            return False, f"Failed to launch Android app:\n{mk_out}"
    return True, app_id


def deploy_ios(
    project_root: pathlib.Path,
    udid: str,
) -> Tuple[bool, str]:
    proj_path, scheme, bundle_id = discover_ios_metadata(project_root)
    derived = project_root / "build" / "mobile" / "ios-derived"
    env = os.environ.copy()
    env["YAZA_SKIP_ENGINE_BUILD"] = "1"
    rc_xb, xb_out = run_cmd(
        [
            "xcodebuild",
            "-project",
            str(proj_path),
            "-scheme",
            scheme,
            "-configuration",
            "Debug",
            "-sdk",
            "iphonesimulator",
            "-destination",
            f"id={udid}",
            "-derivedDataPath",
            str(derived),
            "CODE_SIGNING_ALLOWED=NO",
            "build",
        ],
        env=env,
        capture=True,
    )
    if rc_xb != 0:
        return False, f"xcodebuild failed:\n{xb_out}"
    app_path = derived / "Build" / "Products" / "Debug-iphonesimulator" / f"{scheme}.app"
    if not app_path.is_dir():
        apps = sorted((derived / "Build" / "Products" / "Debug-iphonesimulator").glob("*.app"))
        if apps:
            app_path = apps[0]
    rc_in, in_out = run_cmd(["xcrun", "simctl", "install", udid, str(app_path)], capture=True)
    if rc_in != 0:
        return False, f"xcrun simctl install failed:\n{in_out}"
    rc_ctr, ctr_out = run_cmd(["xcrun", "simctl", "get_app_container", udid, bundle_id, "data"], capture=True)
    if rc_ctr == 0 and ctr_out.strip():
        tmp_dir = pathlib.Path(ctr_out.strip()) / "tmp"
        if tmp_dir.is_dir():
            for stale in list(tmp_dir.glob("yaza_hotswap_*")):
                try:
                    stale.unlink()
                except OSError:
                    pass
    rc_ln, ln_out = run_cmd(
        ["xcrun", "simctl", "launch", "--terminate-running-process", udid, bundle_id],
        capture=True,
    )
    if rc_ln != 0:
        return False, f"xcrun simctl launch failed:\n{ln_out}"
    return True, bundle_id


def hotswap_android(
    project_root: pathlib.Path,
    adb_bin: str,
    serial: str,
    gen: int,
) -> Tuple[bool, str]:
    so_src = project_root / "build" / "mobile" / "android-arm64" / "libyaza_mobile_hotswap.so"
    if not so_src.is_file():
        return False, "missing libyaza_mobile_hotswap.so"
    app_id, _activity = discover_android_metadata(project_root)
    so_name = f"yaza_hotswap_{gen}.so"
    tmp_remote = f"/data/local/tmp/{so_name}"

    rc_push, push_out = run_cmd([adb_bin, "-s", serial, "push", str(so_src), tmp_remote], capture=True)
    copied = False
    if rc_push == 0:
        run_cmd([adb_bin, "-s", serial, "shell", "chmod", "644", tmp_remote], capture=True)
        stage_cmd = (
            f"mkdir -p files && cp {shlex.quote(tmp_remote)} files/{shlex.quote(so_name)} "
            f"&& chmod 700 files/{shlex.quote(so_name)} "
            f"&& printf '%s' {shlex.quote(so_name)} > files/yaza_hotswap_ready.txt.tmp "
            f"&& mv files/yaza_hotswap_ready.txt.tmp files/yaza_hotswap_ready.txt"
        )
        rc_cp, cp_out = run_cmd([adb_bin, "-s", serial, "shell", "run-as", app_id, "sh", "-c", stage_cmd], capture=True)
        run_cmd([adb_bin, "-s", serial, "shell", "rm", "-f", tmp_remote], capture=True)
        copied = rc_cp == 0
        last_err = cp_out
    else:
        last_err = push_out

    if not copied:
        b64_payload = base64.b64encode(so_src.read_bytes()).decode("ascii")
        b64_cmd = (
            f"mkdir -p files && base64 -d > files/{shlex.quote(so_name)} "
            f"&& chmod 700 files/{shlex.quote(so_name)} "
            f"&& printf '%s' {shlex.quote(so_name)} > files/yaza_hotswap_ready.txt.tmp "
            f"&& mv files/yaza_hotswap_ready.txt.tmp files/yaza_hotswap_ready.txt"
        )
        proc = subprocess.run(
            [adb_bin, "-s", serial, "shell", "run-as", app_id, "sh", "-c", b64_cmd],
            input=b64_payload,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
        )
        if proc.returncode != 0:
            return False, (proc.stdout or last_err or "adb run-as hot-swap failed").strip()

    return True, app_id


def hotswap_ios(
    project_root: pathlib.Path,
    udid: str,
    gen: int,
) -> Tuple[bool, str]:
    dylib_src = project_root / "build" / "mobile" / "ios-simulator-arm64" / "libyaza_mobile_hotswap.dylib"
    if not dylib_src.is_file():
        return False, "missing libyaza_mobile_hotswap.dylib"
    _proj, _scheme, bundle_id = discover_ios_metadata(project_root)
    rc_ctr, ctr_out = run_cmd(["xcrun", "simctl", "get_app_container", udid, bundle_id, "data"], capture=True)
    container_path = ctr_out.strip().splitlines()[-1].strip() if rc_ctr == 0 and ctr_out.strip() else ""
    if rc_ctr != 0 or not container_path:
        return False, (ctr_out or "xcrun simctl get_app_container failed").strip()
    tmp_dir = pathlib.Path(container_path) / "tmp"
    tmp_dir.mkdir(parents=True, exist_ok=True)
    dylib_name = f"yaza_hotswap_{gen}.dylib"
    shutil.copy2(dylib_src, tmp_dir / dylib_name)
    tmp_marker = tmp_dir / "yaza_hotswap_ready.txt.tmp"
    final_marker = tmp_dir / "yaza_hotswap_ready.txt"
    tmp_marker.write_text(dylib_name, encoding="utf-8")
    tmp_marker.replace(final_marker)
    return True, bundle_id


def snapshot_watch_files(project_root: pathlib.Path, platform: str) -> Dict[str, int]:
    ignore_dirs = {"build", ".gradle", "DerivedData", ".git", ".idea", "node_modules"}
    roots = [project_root / "src", project_root / "mobile" / platform]
    mtimes: Dict[str, int] = {}
    manifest = project_root / "yaza.toml"
    if manifest.is_file():
        mtimes["yaza.toml"] = manifest.stat().st_mtime_ns
    for root in roots:
        if not root.is_dir():
            continue
        for dirpath, dirnames, filenames in os.walk(root):
            dirnames[:] = [d for d in dirnames if d not in ignore_dirs and not d.startswith(".")]
            for fn in filenames:
                if fn.startswith(".") or fn.endswith("~") or fn.endswith(".swp"):
                    continue
                full = pathlib.Path(dirpath) / fn
                try:
                    rel = str(full.relative_to(project_root))
                    mtimes[rel] = full.stat().st_mtime_ns
                except OSError:
                    pass
    return mtimes


def changed_paths(prev: Dict[str, int], curr: Dict[str, int]) -> List[str]:
    changed: List[str] = []
    for k, v in curr.items():
        if prev.get(k) != v:
            changed.append(k)
    for k in prev:
        if k not in curr:
            changed.append(k)
    return sorted(changed)


def watch_loop(
    yaza_bin: str,
    platform: str,
    project_root: pathlib.Path,
    device_id: str,
    adb_bin: str = "",
    gradle_bin: str = "",
) -> None:
    core_target = "android-arm64" if platform == "android" else "ios-simulator-arm64"
    print(
        f"\n👀 Live Watch active ({platform} · {device_id}) watching src/**/*.yz, yaza.toml, mobile/{platform}/**. Press Ctrl+C to stop.",
        flush=True,
    )
    last_snap = snapshot_watch_files(project_root, platform)
    pending_changes: List[str] = []
    last_change_ts = 0.0
    hotswap_gen = 0

    while not STOP_REQUESTED:
        time.sleep(0.10)
        if STOP_REQUESTED:
            break
        curr_snap = snapshot_watch_files(project_root, platform)
        diffs = changed_paths(last_snap, curr_snap)
        if diffs:
            last_snap = curr_snap
            for d in diffs:
                if d not in pending_changes:
                    pending_changes.append(d)
            last_change_ts = time.monotonic()
            continue

        if pending_changes and (time.monotonic() - last_change_ts) >= 0.25:
            files = pending_changes[:]
            pending_changes.clear()
            primary = files[0]
            t_start = time.monotonic()

            ok_core, core_ms, core_out = build_core_engine(yaza_bin, core_target, str(project_root))
            if not ok_core:
                print(f"\n[{now_hms()}] ✗ YAZA compile failed after {primary} changed:", file=sys.stderr, flush=True)
                print(core_out.rstrip(), file=sys.stderr, flush=True)
                print(f"[{now_hms()}]   Waiting for the next file fix...", file=sys.stderr, flush=True)
                continue

            core_only = all(f == "yaza.toml" or f.startswith("src/") or f.startswith("src\\") for f in files)
            if core_only:
                hotswap_gen += 1
                if platform == "android":
                    ok_swap, swap_msg = hotswap_android(project_root, adb_bin, device_id, hotswap_gen)
                else:
                    ok_swap, swap_msg = hotswap_ios(project_root, device_id, hotswap_gen)
                if ok_swap:
                    total_ms = int((time.monotonic() - t_start) * 1000)
                    print(
                        f"[{now_hms()}] ⚡ {primary} changed -> hot-swapped in {total_ms}ms (core {core_ms}ms, no restart)",
                        flush=True,
                    )
                    continue
                print(
                    f"[{now_hms()}] ↺ Hot-swap unavailable ({swap_msg}); falling back to full app reload...",
                    flush=True,
                )

            if platform == "android":
                ok_dep, dep_msg = deploy_android(yaza_bin, project_root, adb_bin, gradle_bin, device_id, rebuild_apk=True)
            else:
                ok_dep, dep_msg = deploy_ios(project_root, device_id)

            total_ms = int((time.monotonic() - t_start) * 1000)
            if not ok_dep:
                print(f"\n[{now_hms()}] ✗ Deploy failed for {platform} after {primary} changed:", file=sys.stderr, flush=True)
                print(dep_msg.rstrip(), file=sys.stderr, flush=True)
                print(f"[{now_hms()}]   Watcher remains active. Waiting for changes...", file=sys.stderr, flush=True)
            else:
                print(f"[{now_hms()}] ↻ {primary} changed -> rebuilt in {total_ms}ms (core {core_ms}ms) & reloaded", flush=True)


def main() -> int:
    signal.signal(signal.SIGINT, handle_signal)
    signal.signal(signal.SIGTERM, handle_signal)

    if len(sys.argv) < 3:
        print("Usage: dev_runner.py <yaza_bin> <android|ios|auto>", file=sys.stderr)
        return 2

    yaza_bin = sys.argv[1]
    raw_target = sys.argv[2]
    platform = normalize_target(raw_target)
    project_root = pathlib.Path.cwd().resolve()
    watch_enabled = os.environ.get("YAZA_MOBILE_WATCH", "1") != "0"

    if platform == "android":
        adb_bin = find_adb()
        gradle_bin = find_gradle()
        serial, _label = detect_android_device(adb_bin)
        print("↺ Building YAZA Native Core (android-arm64)...", flush=True)
        ok_core, core_ms, core_out = build_core_engine(yaza_bin, "android-arm64", str(project_root))
        if not ok_core:
            print(core_out, file=sys.stderr, flush=True)
            return 1
        print(f"✓ Core built ({core_ms}ms). Installing & launching on {serial}...", flush=True)
        ok_dep, dep_msg = deploy_android(yaza_bin, project_root, adb_bin, gradle_bin, serial, rebuild_apk=True)
        if not ok_dep:
            print(dep_msg, file=sys.stderr, flush=True)
            return 1
        print(f"✓ App {dep_msg} running on {serial}.", flush=True)
        if watch_enabled:
            watch_loop(yaza_bin, "android", project_root, serial, adb_bin=adb_bin, gradle_bin=gradle_bin)
    else:
        if not shutil.which("xcodebuild") or not shutil.which("xcrun"):
            print("error: Xcode command-line tools are required to run iOS", file=sys.stderr)
            return 1
        udid, name, _rt = detect_ios_simulator()
        print("↺ Building YAZA Native Core (ios-simulator-arm64)...", flush=True)
        ok_core, core_ms, core_out = build_core_engine(yaza_bin, "ios-simulator-arm64", str(project_root))
        if not ok_core:
            print(core_out, file=sys.stderr, flush=True)
            return 1
        print(f"✓ Core built ({core_ms}ms). Installing & launching on {name}...", flush=True)
        ok_dep, dep_msg = deploy_ios(project_root, udid)
        if not ok_dep:
            print(dep_msg, file=sys.stderr, flush=True)
            return 1
        print(f"✓ App {dep_msg} running on {name} ({udid}).", flush=True)
        if watch_enabled:
            watch_loop(yaza_bin, "ios", project_root, udid)

    if STOP_REQUESTED:
        print("\n✓ Live reload stopped cleanly.", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
