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

## Live development (Auto-Detect Device & Hot Reload)

```sh
yaza mobile doctor
yaza mobile run --target android
yaza mobile run --target ios
```

### Features of `yaza mobile run`
- **Compact `--target` flag:** Use `--target android` or `--target ios` (aliases `android-emulator` and `ios-simulator` remain supported; omit `--target` in a terminal to choose interactively).
- **Smart device / simulator selection:**
  - Automatically picks a single active emulator/simulator.
  - Prompts for selection if multiple devices/simulators are running.
  - Auto-boots (or prompts to boot) an Android AVD or iOS iPhone Simulator if none are running and waits for readiness.
- **Live watch & reload loop:** Watches `src/**/*.yz`, `yaza.toml`, and `mobile/<platform>/**` (~250ms debounce), incrementally rebuilds native core + re-deploys on change, keeps running cleanly if a compile error occurs while editing, and stops cleanly on `Ctrl+C`. Use `--once` for a single build/launch cycle.

## Direct native library builds

```sh
yaza mobile build --target android-arm64 --release
yaza mobile build --target ios-arm64 --release
yaza mobile build --target ios-simulator-arm64 --release
```
