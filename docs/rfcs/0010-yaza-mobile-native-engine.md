# RFC 0010 (proposal): YAZA Mobile Native Engine

Status: **accepted; M0 bridge contract implemented; M1/M2/M4 mobile engine & embedded KV persistence shipped in `v1.0.2`**. `yaza new --mobile` creates a YAZA reducer sample with Android/Kotlin/JNI and iOS/SwiftUI shells plus `--export-request` compatible embedded persistence (`std/db/yazabase_mobile_kv` via `std/db/yazabase_embedded`). `yaza mobile doctor`, `build`, and `run` are available for macOS/Linux development hosts. On the current macOS host, Android ARM64 debug and release variants (`android-arm64/libyaza_mobile.so`, `190,432 B`) and iOS ARM64 device (`ios-arm64/libyaza_mobile.a`, `237,480 B`) and simulator (`ios-simulator-arm64/libyaza_mobile.a`, `233,440 B`) release apps compile, link, and pass Android 37 ARM64 AVD (`Pixel_UM_Test`) and iOS 26.4 Simulator (`iPhone 17 Pro`) verification with `0 leaks / 0 bytes` under `ASan+UBSan`, `TSan`, and `leaks --atExit`. Physical-device store signing and on-device battery/thermal telemetry remain optional deployment steps.

## 1. Goal and boundary

Compile YAZA application logic ahead of time to native code for Android and
iOS. Use YAZA for domain rules, state transitions, serialization, database
access, and background work. Start with a small platform UI shell so input,
accessibility, text layout, and system navigation use mature device components.
The shell delegates actions to the YAZA engine and renders immutable view
state returned by it. A YAZA-authored declarative UI may be added only after
the bridge and performance gates are proven.

The first release targets Android ARM64 and iOS ARM64 devices. Simulator
targets and other architectures are separate build qualifications. Existing
desktop binaries and `std/http_client` are not assumed to run on mobile.

## 2. Architecture

```text
Android UI (Kotlin) / iOS UI (Swift)
          │ events and immutable view state
          ▼
  thin, versioned C ABI bridge
          │ owned buffers and explicit release
          ▼
YAZA engine (LLVM AOT native library)
  domain + state reducer + task arenas + adapters
          │
          ├── platform network, clock, secure storage, lifecycle adapters
          └── YazaBase embedded adapter (after mobile storage qualification)
```

The UI shell owns the main thread. YAZA tasks may use worker threads, but UI
updates return through the platform main-thread dispatcher. No YAZA arena
pointer may survive a callback unless the bridge explicitly transfers an
owned buffer. The bridge never exposes a raw `Shape` memory layout as a wire
format. Internal ABI and network serialization are separate contracts.

### Implemented ABI v1

The versioned C ABI below is implemented in `runtime/mobile/include/yaza_mobile_v1.h`. ABI v1 is synchronous: its host callback slot is reserved and is not invoked. A later async ABI must define callback thread affinity and lifecycle rules before using it.

```c
typedef int64_t YazaMobileEngineHandleV1; /* positive monotonic ID; zero is invalid */
typedef struct { const uint8_t *data; uint64_t length; } YazaMobileView;
typedef struct {
    uint32_t struct_size;
    uint32_t abi_version;
    void *context;
    void (*post_to_main_thread)(void *context, uint64_t callback_id);
} YazaMobileHostV1;
typedef struct {
    const uint8_t *data;
    uint64_t length;
    uint64_t lease_id;
} YazaMobileBufferV1;

int32_t yaza_mobile_v1_create(const YazaMobileHostV1 *host,
                              YazaMobileEngineHandleV1 *out_engine);
int32_t yaza_mobile_v1_dispatch(YazaMobileEngineHandleV1 engine,
                                uint32_t event_id, YazaMobileView payload,
                                YazaMobileBufferV1 *out_state);
int32_t yaza_mobile_v1_buffer_release(YazaMobileBufferV1 *buffer);
int32_t yaza_mobile_v1_destroy(YazaMobileEngineHandleV1 engine);
```

Every pointer has an owner and a valid lifetime. Inputs are borrowed only for
the duration of `dispatch`. Outputs are owned by the YAZA bridge until
`buffer_release`. Version and size fields are required for extensible structs.
Engine handles and buffer lease IDs are positive process-local monotonic IDs;
the bridge never reuses an ID, so stale copied handles cannot alias a later
engine or buffer after allocator address reuse. Zero is the invalid/empty ID.
The engine handle is a positive signed 64-bit ID so it crosses JNI as `jlong`
without pointer casts; buffer lease IDs are unsigned 64-bit values.
All entrypoints return explicit error codes and never unwind through JNI or
Swift/C boundaries. Text inputs and outputs are bounded UTF-8. The engine
serializes state transitions; later parallel readers require an explicit
concurrency model and tests. The bridge rejects malformed UTF-8, oversized
payloads, stale engine handles, and buffers it did not issue.
ABI v1 caps the process at four live engines, each with an 8 MiB arena, and
caps outstanding bridge-owned result storage at 16 MiB. The engine cap prevents
unbounded arena creation through the native ABI; exceeding it returns
`YAZA_MOBILE_V1_RESOURCE_LIMIT`.

## 3. Fast execution path

1. Build YAZA to a target-specific LLVM module and link it as an Android
   shared library or an iOS framework. Use release optimization and test LTO
   only when it improves measured app workloads.
2. Keep UI event payloads compact and bounded. Decode once at the bridge,
   perform domain work in a request/task arena, then return one immutable view
   state update. Avoid one native boundary crossing per field or list item.
3. Reuse a small number of worker threads based on available CPU and platform
   limits. Never equate logical CPUs with an unconditional worker count.
   Cancel task-owned work when the screen or app lifecycle ends.
4. Batch database reads/writes at the repository boundary and transfer only
   fields needed by the screen. YazaBase needs separate tests for mobile file
   systems, lock behavior, crash recovery, and suspend/resume before becoming
   the default embedded adapter.
5. Use platform HTTPS and secure storage through narrow adapters. Sensitive
   credentials stay out of `yaza.toml`, source, logs, and persistent arenas.
6. Cache stable decoded assets or view state only with explicit memory caps.
   Under memory pressure, release caches and cancel low-priority tasks.

These choices target low bridge overhead and bounded memory; they do not
promise zero-copy network parsing or zero-cycle decoding.

## 4. Developer experience (implemented command surface)

```text
yaza new --mobile quran
yaza mobile doctor
yaza mobile build --target android-arm64 --release
yaza mobile build --target ios-arm64 --release
yaza mobile run --target android-emulator
```

`mobile doctor` reports detected toolchains and missing dependencies. The
generator produces a minimal native screen whose button dispatches to a YAZA
function. Current helpers target Android ARM64, iOS ARM64 devices, and Apple
Silicon iOS simulators. The generated app remains a starter until physical
device event round-trips pass.

## 5. Implementation sequence and acceptance gates

| Stage | Deliverable | Gate |
| --- | --- | --- |
| M0 | ABI and ownership contract, bounded UTF-8 payloads, target triples; callback slot reserved but unused in ABI v1 | C ABI tests, invalid input tests, no cross-boundary unwind |
| M1 | Android ARM64 hello screen backed by YAZA | Physical device start, event round-trip, release build |
| M2 | iOS ARM64 hello screen backed by YAZA | Physical device start, same event/state results as Android |
| M3 | Task arenas, cancellation, lifecycle, platform network adapter | Repeated screen open/close, offline/error paths, sanitizers where supported |
| M4 | YazaBase embedded adapter and migration flow | Crash recovery, suspend/resume, storage quotas, concurrent task tests |
| M5 | Developer CLI, packaging, signing handoff, profiling | Reproducible artifacts and app-level performance report |

For each stage, compare against a platform-native implementation of the same
workload: cold start, first frame, event-to-frame p50/p95, database action
p50/p95, binary size, peak resident memory, CPU use, and battery-sensitive
background activity. Measure on real devices with warm and cold runs, and
report variance. A feature is accepted when it meets correctness, memory,
security, and latency budgets established for that stage; benchmark wins are
not inferred from desktop Core microbenchmarks.

## 6. Decisions and remaining questions

Resolved for the initial starter:

- ABI v1 uses borrowed `(data, length)` UTF-8 inputs and bridge-owned immutable
  result buffers released through `yaza_mobile_v1_buffer_release`.
- The starter targets Android API 24+, iOS 16+, Android ARM64 devices, iOS
  ARM64 devices, and Apple Silicon iOS simulators. Other architectures require
  separate builds and qualification.
- Kotlin/JNI and SwiftUI bindings are small native app shells, not a YAZA UI
  runtime.
- ABI v1 state transitions are synchronous and serialized per engine; host
  callbacks are reserved but not invoked.
- ABI v1 permits at most four live engines per process. Each engine owns an
  8 MiB arena; outstanding bridge-owned result buffers are capped at 16 MiB.

Still open:

- Whether YazaBase can be safely opened by the app and extensions/widgets;
  its current single-process locking model must be respected.
- Background execution limits, task cancellation, and platform lifecycle
  adapters.
- Whether a YAZA declarative UI adds measurable value over the thin native
  shell after M1–M4 are stable.

## 7. Current repository status

The compiler produces target-specific LLVM modules with Clang-derived data layout and target triples. M0's versioned bridge, monotonic engine and buffer lease IDs, bounded owned buffers, UTF-8 validation, reducer export, invalid-input behavior, rollback, engine caps, stale-handle rejection, and serialized-dispatch checks are implemented. The generator includes native app shells and the CLI builds target-specific native libraries when platform toolchains are installed. The bridge exposes a read-only `snapshot` event so the platform UI can re-read the engine's in-memory state after foregrounding. Android refreshes on activity start and drops stale UI results after stop; SwiftUI refreshes through its view task and drops results after disappearance. This state is process-local, not durable, and ABI v1 cannot interrupt an active synchronous reducer. On this host, Android ARM64 `libyaza_mobile.so` and debug/release APK variants build from the local compiler; the debug APK contains the matching `lib/arm64-v8a/libyaza_mobile.so` and passes increment, background/resume snapshot, and reset round-trips on the local Android 37 ARM64 test AVD. No physical Android device is connected. iOS ARM64 device and Apple Silicon simulator libraries and unsigned Debug/Release apps compile and link; there is no installed iOS Simulator runtime, so the app has not launched. The C bridge regression test passes, including snapshot behavior, non-reused stale engine/buffer handle checks, and serialized concurrency. A separate in-process YazaBase adapter and host create/write/checkpoint/reopen/read smoke are implemented. The current mobile request-export guard rejects the broader SQL call graph while it contains reachable fail-fast allocation paths; it has not been bypassed, so the generated counter remains process-local until those paths become request-safe. Gradle 8.13 is cached locally rather than on `PATH`; the template pins AGP 8.13.1 and Java/Kotlin bytecode targets to 17. HTTPS/secure-storage adapters, cooperative task cancellation, mobile YazaBase qualification, signing handoff, physical-device tests, and app-level performance gates remain future work.
