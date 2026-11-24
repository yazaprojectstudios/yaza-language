# {{project_name}}

Native Android and iOS app built on **Shared YAZA Core (MVI + YBIF + Outbox Effect) + Pure Native Dumb UI**. All business logic, input validation, YBIF state encoding, and embedded YazaBase KV persistence live inside `src/`, while Android Kotlin and iOS SwiftUI render the screen ("Welcome to YAZA Mobile") and execute outbox effects through `yaza_mobile_v1.h`.

## Project layout

- `src/main.yz`: Desktop CLI smoke-test & verifier for state transitions and YBIF encoding.
- `src/mobile_engine.yz`: C ABI entry point (`yaza emit-llvm-module --export-request mobile_reduce`).
- `src/state/app_state.yz` & `src/state/codec.yz`: `AppState` model and binary **YBIF v1** (`std/ybif`) encoder/decoder.
- `src/actions/home_actions.yz`: MVI action handlers (`INIT`, `GREET`, `INCREMENT`, `PERSIST`, `RESET`).
- `src/domain/greeting.yz`: Pure business validation & counter domain rules.
- `src/effects/outbox.yz`: Declarative outbox effect builders (`KV_SAVE`, `TOAST`, `HTTP`).
- `mobile/android/`: Kotlin + JNI UI shell (Dumb View).
- `mobile/ios/`: SwiftUI + Bridging Header UI shell (Dumb View).

## Run desktop smoke-test

```sh
yaza run src/main.yz
```

## Live development (Auto-Detect Device & Debug Dynamic Core Hot-Swap)

```sh
yaza mobile doctor
yaza mobile run android
yaza mobile run ios
# or with explicit --target / --once:
yaza mobile run --target android --once
```

### Features of `yaza mobile run` (Debug mode by default)
- **Debug mode (`YAZA_MOBILE_BUILD_MODE=debug`, `-O0`):** `yaza mobile run` always launches in debug mode so hot-swap is enabled out of the box.
- **Positional or `--target` syntax:** Use `yaza mobile run android` / `ios` (or `--target android` / `--target ios`; aliases `android-emulator` and `ios-simulator` remain supported; omit the target in a terminal to choose interactively).
- **Smart device / simulator selection:**
  - Automatically picks a single active emulator/simulator.
  - Prompts for selection if multiple devices/simulators are running.
  - Auto-boots (or prompts to boot) an Android AVD or iOS iPhone Simulator if none are running and waits for readiness.
- **Debug Dynamic Core Hot-Swap (`dlopen`):** Edits inside `src/**/*.yz` or `yaza.toml` (~250ms debounce) recompile only the native core (`libyaza_mobile_hotswap.so` / `libyaza_mobile_hotswap.dylib`) and swap `yaza_mobile_v1_*` function pointers in-process without closing or relaunching the app (`⚡ <file> changed -> hot-swapped in ...ms, no restart`), then re-dispatch `INIT`. Edits to native UI files (`mobile/<platform>/**`) trigger a full Gradle/Xcode rebuild and relaunch. Compile errors keep the watcher running, and `Ctrl+C` stops cleanly (pass `--once` for a single build/launch cycle).

## Direct native library builds (`--debug` vs `--release`)

```sh
# Debug (-O0 + emits libyaza_mobile_hotswap.so / .dylib for live hot-swap)
yaza mobile build --target android-arm64 --debug
yaza mobile build --target ios-simulator-arm64 --debug

# Release (-O2 production binaries; hot-swap excluded)
yaza mobile build --target android-arm64 --release
yaza mobile build --target ios-arm64 --release
yaza mobile build --target ios-simulator-arm64 --release
```
