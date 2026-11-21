# {{project_name}}

Native Android and iOS starter backed by a YAZA compiled engine. The platform UI remains Kotlin/Android and SwiftUI/iOS; business state transitions run in the YAZA engine through the versioned `YazaMobile` C ABI.

## Project layout

- `src/mobile_engine.yz` contains the YAZA state reducer exported to native code.
- `src/main.yz` runs a small CLI preview of the reducer.
- `mobile/android/` contains the Android app shell and JNI adapter.
- `mobile/ios/` contains the SwiftUI app shell and framework integration files.
- `yaza.toml` stores project metadata.

## Native engine bridge

The bridge accepts bounded UTF-8 event payloads, runs one reducer call in a request-scoped arena, and returns an owned immutable buffer. Call `yaza_mobile_v1_buffer_release` for every returned buffer. Calls on one engine serialize state transitions. The state is held by the engine, so a reducer error does not change it. The starter submits reducer work to a background executor and updates UI state on the platform main thread; it allows one UI event at a time. This is platform-level dispatch, not YAZA `async`/`await` or a general task scheduler.

The example supports snapshot, increment, and reset events. Android refreshes its visible state when the activity starts again; SwiftUI refreshes when the view task restarts. This snapshot is in-memory state for the lifetime of the YAZA engine, not durable storage. The starter discards stale UI results after screen/activity exit, but synchronous reducer work itself cannot be cancelled. It is a functional bridge demo, not yet a qualified store app: physical-device, storage, network, signing, and performance gates must pass before shipping.

The mobile engine build disables the runtime's child-process launch primitive. Mobile OS sandboxes do not support desktop-style arbitrary process spawning; APIs that attempt it return exit status `127`.

## Run the CLI preview

```sh
yaza run src/main.yz
```

## Build and run the native app

Check the platform tools from the generated project root:

```sh
yaza mobile doctor
```

Build the engine library for Android ARM64, an iOS ARM64 device, or an Apple Silicon iOS simulator:

```sh
yaza mobile build --target android-arm64 --release
yaza mobile build --target ios-arm64 --release
yaza mobile build --target ios-simulator-arm64 --release
```

Run the starter on an Android emulator or a booted iOS simulator:

```sh
yaza mobile run --target android-emulator
yaza mobile run --target ios-simulator
```

Android builds require Android SDK, NDK, Java, Gradle, and `adb`; iOS builds require macOS and Xcode. The current mobile build helper supports macOS/Linux hosts. The generated app is a bridge starter, not a production-qualified mobile framework; physical-device lifecycle, storage, network, signing, and performance gates remain open.

When building from a YAZA source checkout, point the platform build at that checkout's compiler so an older installed `yaza` is not selected accidentally:

```sh
export YAZA_BIN="/path/to/yaza-language-public/build/yaza-llvm"
```

Android Gradle builds use the optimized YAZA engine for release tasks and a debug engine for debug-only tasks. If one invocation requests both build types, it uses the optimized engine artifact for both; the C ABI remains the same.

From a YAZA source checkout, `sh scripts/validate-mobile-engine.sh` validates bridge round-trips, invalid input, output ownership, error rollback, and serialized concurrent dispatch.
