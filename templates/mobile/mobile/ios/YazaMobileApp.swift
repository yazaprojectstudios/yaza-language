import Darwin
import SwiftUI

private let yazaEngineQueue = DispatchQueue(label: "dev.yaza.mobile.engine", qos: .userInitiated)
private let yazaEventAction: UInt32 = 4

struct MobileUiState {
    let title: String
    let greeting: String
    let counter: Int64
    let status: String
    let effects: [String]
}

@main
struct YazaMobileApp: App {
    var body: some Scene {
        WindowGroup {
            HomeView()
        }
    }
}

@MainActor
struct HomeView: View {
    @Environment(\.scenePhase) private var scenePhase
    @State private var engine: MobileEngine?
    @State private var uiState = MobileUiState(
        title: "Welcome to YAZA Mobile",
        greeting: "Loading Shared YAZA Core...",
        counter: 0,
        status: "ready",
        effects: []
    )
    @State private var nameInput = ""
    @State private var errorMessage = ""
    @State private var toastBanner = ""
    @State private var dispatchPending = false
    @State private var lifecycleGeneration: UInt64 = 0
    @State private var lastHotSwapMarker = ""
    @State private var hotSwapTimer: Timer?

    var body: some View {
        VStack(spacing: 16) {
            Text(uiState.title)
                .font(.title2.bold())
                .multilineTextAlignment(.center)
                .accessibilityIdentifier("title")

            Text(uiState.greeting)
                .font(.body)
                .multilineTextAlignment(.center)
                .foregroundStyle(.secondary)
                .accessibilityIdentifier("greeting")

            Text("Counter: \(uiState.counter)")
                .font(.largeTitle.monospacedDigit())
                .accessibilityIdentifier("counter")

            Text("Status: \(uiState.status)")
                .font(.caption.monospaced())
                .padding(.horizontal, 10)
                .padding(.vertical, 4)
                .background(.quaternary, in: Capsule())

            if !toastBanner.isEmpty {
                Text(toastBanner)
                    .font(.caption)
                    .foregroundStyle(.green)
            }

            if !errorMessage.isEmpty {
                Text(errorMessage)
                    .font(.footnote)
                    .foregroundStyle(.red)
                    .accessibilityIdentifier("error")
            }

            HStack(spacing: 8) {
                TextField("Enter your name", text: $nameInput)
                    .textFieldStyle(.roundedBorder)
                Button("Greet") {
                    dispatchAction("GREET|\(nameInput)", showToast: true)
                }
                .buttonStyle(.bordered)
                .disabled(engine == nil || dispatchPending)
            }

            HStack(spacing: 12) {
                Button("Increment (+1)") {
                    dispatchAction("INCREMENT|1", showToast: true)
                }
                .buttonStyle(.borderedProminent)
                .disabled(engine == nil || dispatchPending)

                Button("Save") {
                    dispatchAction("PERSIST", showToast: true)
                }
                .buttonStyle(.bordered)
                .disabled(engine == nil || dispatchPending)

                Button("Reset") {
                    dispatchAction("RESET", showToast: true)
                }
                .buttonStyle(.bordered)
                .disabled(engine == nil || dispatchPending)
            }
        }
        .padding(24)
        .task {
            if engine == nil {
                do {
                    engine = try MobileEngine()
                } catch {
                    errorMessage = error.localizedDescription
                }
            }
            guard !Task.isCancelled else { return }
            dispatchAction("INIT", showToast: false)
        }
        .onChange(of: scenePhase) { phase in
            if phase == .active {
                dispatchAction("INIT", showToast: false)
            } else if phase == .background {
                lifecycleGeneration &+= 1
                dispatchPending = false
            }
        }
        .onAppear {
            startHotSwapWatcherIfNeeded()
        }
        .onDisappear {
            hotSwapTimer?.invalidate()
            hotSwapTimer = nil
            lifecycleGeneration &+= 1
            dispatchPending = false
        }
    }

    private func startHotSwapWatcherIfNeeded() {
        #if DEBUG && targetEnvironment(simulator)
        guard hotSwapTimer == nil else { return }
        hotSwapTimer = Timer.scheduledTimer(withTimeInterval: 0.3, repeats: true) { _ in
            DispatchQueue.main.async {
                self.pollHotSwapMarker()
            }
        }
        #endif
    }

    private func pollHotSwapMarker() {
        #if DEBUG && targetEnvironment(simulator)
        guard let activeEngine = engine, !dispatchPending else { return }
        let tmpDir = NSTemporaryDirectory()
        let readyPath = (tmpDir as NSString).appendingPathComponent("yaza_hotswap_ready.txt")
        guard let rawMarker = try? String(contentsOfFile: readyPath, encoding: .utf8) else { return }
        let marker = rawMarker.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !marker.isEmpty, marker != lastHotSwapMarker else { return }
        let dylibPath = (tmpDir as NSString).appendingPathComponent(marker)
        guard FileManager.default.fileExists(atPath: dylibPath) else { return }

        lastHotSwapMarker = marker
        dispatchPending = true
        let requestGeneration = lifecycleGeneration
        errorMessage = ""
        yazaEngineQueue.async {
            let swapResult = Result<Void, Error> {
                try activeEngine.hotSwap(dylibPath: dylibPath)
                if let entries = try? FileManager.default.contentsOfDirectory(atPath: tmpDir) {
                    for entry in entries where entry.hasPrefix("yaza_hotswap_") && entry.hasSuffix(".dylib") && entry != marker {
                        let stalePath = (tmpDir as NSString).appendingPathComponent(entry)
                        try? FileManager.default.removeItem(atPath: stalePath)
                    }
                }
            }
            DispatchQueue.main.async {
                guard self.lifecycleGeneration == requestGeneration else { return }
                self.dispatchPending = false
                switch swapResult {
                case .success:
                    self.dispatchAction("INIT", showToast: false)
                case .failure(let failure):
                    self.errorMessage = failure.localizedDescription
                }
            }
        }
        #endif
    }

    private func dispatchAction(_ actionPayload: String, showToast: Bool) {
        guard let activeEngine = engine else {
            errorMessage = "YAZA engine is not initialized"
            return
        }
        guard !dispatchPending else { return }
        dispatchPending = true
        let requestGeneration = lifecycleGeneration
        errorMessage = ""
        yazaEngineQueue.async {
            let result = Result {
                let raw = try activeEngine.dispatchBytes(event: yazaEventAction, payload: actionPayload)
                return try decodeYbifState(raw)
            }
            DispatchQueue.main.async {
                guard self.lifecycleGeneration == requestGeneration else { return }
                self.dispatchPending = false
                switch result {
                case .success(let nextState):
                    self.uiState = nextState
                    self.errorMessage = ""
                    if showToast, let fx = nextState.effects.first(where: { $0.hasPrefix("TOAST|") }) {
                        self.toastBanner = String(fx.dropFirst(6))
                    } else if let kv = nextState.effects.first(where: { $0.hasPrefix("KV_SAVE|") }) {
                        self.toastBanner = kv
                    }
                case .failure(let failure):
                    self.errorMessage = failure.localizedDescription
                }
            }
        }
    }
}

private func decodeYbifState(_ data: Data) throws -> MobileUiState {
    let bytes = [UInt8](data)
    if bytes.count >= 5 &&
        bytes[0] == 0x59 && bytes[1] == 0x42 && bytes[2] == 0x49 && bytes[3] == 0x46 && bytes[4] == 0x01 {
        var pos = 5
        func readText() throws -> String {
            guard pos + 5 <= bytes.count, bytes[pos] == 0x04 else {
                throw MobileBridgeError(status: 7, message: "Invalid YBIF text tag at offset \(pos)")
            }
            let len = Int(UInt32(bytes[pos + 1])
                | (UInt32(bytes[pos + 2]) << 8)
                | (UInt32(bytes[pos + 3]) << 16)
                | (UInt32(bytes[pos + 4]) << 24))
            guard len >= 0, pos + 5 + len <= bytes.count else {
                throw MobileBridgeError(status: 7, message: "Invalid YBIF text length at offset \(pos)")
            }
            let slice = bytes[(pos + 5)..<(pos + 5 + len)]
            pos += 5 + len
            return String(decoding: slice, as: UTF8.self)
        }
        func readI64() throws -> Int64 {
            guard pos + 9 <= bytes.count, bytes[pos] == 0x03 else {
                throw MobileBridgeError(status: 7, message: "Invalid YBIF number tag at offset \(pos)")
            }
            var raw: UInt64 = 0
            for i in 0..<8 {
                raw |= UInt64(bytes[pos + 1 + i]) << (8 * i)
            }
            pos += 9
            return Int64(bitPattern: raw)
        }
        let title = try readText()
        let greeting = try readText()
        let counter = try readI64()
        let status = try readText()
        let rawEffects = pos < bytes.count ? try readText() : ""
        let fxList = rawEffects.split(separator: "\n").map { String($0).trimmingCharacters(in: .whitespaces) }.filter { !$0.isEmpty }
        return MobileUiState(title: title, greeting: greeting, counter: counter, status: status, effects: fxList)
    }
    let fallback = Int64(String(decoding: bytes, as: UTF8.self).trimmingCharacters(in: .whitespacesAndNewlines)) ?? 0
    return MobileUiState(
        title: "Welcome to YAZA Mobile",
        greeting: "Shared YAZA Core active",
        counter: fallback,
        status: "ready",
        effects: []
    )
}

// The view sends every call through `yazaEngineQueue`; the handle is never
// accessed concurrently, and the bridge also serializes each engine handle.
private final class MobileEngine: @unchecked Sendable {
    private typealias CreateFn = @convention(c) (UnsafePointer<YazaMobileHostV1>?, UnsafeMutablePointer<YazaMobileEngineHandleV1>?) -> Int32
    private typealias DispatchFn = @convention(c) (YazaMobileEngineHandleV1, UInt32, YazaMobileView, UnsafeMutablePointer<YazaMobileBufferV1>?) -> Int32
    private typealias ReleaseFn = @convention(c) (UnsafeMutablePointer<YazaMobileBufferV1>?) -> Int32
    private typealias DestroyFn = @convention(c) (YazaMobileEngineHandleV1) -> Int32

    private var handle: Int64 = 0
    private var dlHandle: UnsafeMutableRawPointer?
    private var createFn: CreateFn = { yaza_mobile_v1_create($0, $1) }
    private var dispatchFn: DispatchFn = { yaza_mobile_v1_dispatch($0, $1, $2, $3) }
    private var releaseFn: ReleaseFn = { yaza_mobile_v1_buffer_release($0) }
    private var destroyFn: DestroyFn = { yaza_mobile_v1_destroy($0) }

    init() throws {
        var host = YazaMobileHostV1(
            struct_size: UInt32(MemoryLayout<YazaMobileHostV1>.size),
            abi_version: UInt32(YAZA_MOBILE_V1_ABI_VERSION),
            context: nil,
            post_to_main_thread: nil
        )
        let status = createFn(&host, &handle)
        guard status == Int32(YAZA_MOBILE_V1_OK.rawValue), handle > 0 else {
            throw MobileBridgeError(status: status, message: "Could not create YAZA engine")
        }
    }

    func hotSwap(dylibPath: String) throws {
        guard let nextDl = dlopen(dylibPath, RTLD_NOW | RTLD_LOCAL) else {
            let errMsg = dlerror().map { String(cString: $0) } ?? "dlopen failed for \(dylibPath)"
            throw MobileBridgeError(status: Int32(YAZA_MOBILE_V1_INTERNAL_ERROR.rawValue), message: errMsg)
        }
        guard
            let symCreate = dlsym(nextDl, "yaza_mobile_v1_create"),
            let symDispatch = dlsym(nextDl, "yaza_mobile_v1_dispatch"),
            let symRelease = dlsym(nextDl, "yaza_mobile_v1_buffer_release"),
            let symDestroy = dlsym(nextDl, "yaza_mobile_v1_destroy")
        else {
            dlclose(nextDl)
            throw MobileBridgeError(status: Int32(YAZA_MOBILE_V1_INTERNAL_ERROR.rawValue), message: "Hot-swap dylib is missing yaza_mobile_v1 symbols")
        }

        let nextCreate = unsafeBitCast(symCreate, to: CreateFn.self)
        let nextDispatch = unsafeBitCast(symDispatch, to: DispatchFn.self)
        let nextRelease = unsafeBitCast(symRelease, to: ReleaseFn.self)
        let nextDestroy = unsafeBitCast(symDestroy, to: DestroyFn.self)

        var host = YazaMobileHostV1(
            struct_size: UInt32(MemoryLayout<YazaMobileHostV1>.size),
            abi_version: UInt32(YAZA_MOBILE_V1_ABI_VERSION),
            context: nil,
            post_to_main_thread: nil
        )
        var nextHandle: YazaMobileEngineHandleV1 = 0
        let status = nextCreate(&host, &nextHandle)
        guard status == Int32(YAZA_MOBILE_V1_OK.rawValue), nextHandle > 0 else {
            dlclose(nextDl)
            throw MobileBridgeError(status: status, message: "Could not initialize hot-swapped YAZA engine")
        }

        if handle > 0 { _ = destroyFn(handle) }
        if let prevDl = dlHandle { _ = dlclose(prevDl) }
        dlHandle = nextDl
        createFn = nextCreate
        dispatchFn = nextDispatch
        releaseFn = nextRelease
        destroyFn = nextDestroy
        handle = nextHandle
    }

    func dispatchBytes(event: UInt32, payload: String) throws -> Data {
        guard handle > 0 else {
            throw MobileBridgeError(status: Int32(YAZA_MOBILE_V1_INVALID_ARGUMENT.rawValue), message: "YAZA engine was destroyed")
        }
        let bytes = Array(payload.utf8)
        var output = YazaMobileBufferV1(data: nil, length: 0, lease_id: 0)
        let status = bytes.withUnsafeBufferPointer { buffer -> Int32 in
            let view = YazaMobileView(data: buffer.baseAddress, length: UInt64(buffer.count))
            return dispatchFn(handle, event, view, &output)
        }
        defer { _ = releaseFn(&output) }
        let rawData: Data
        if let data = output.data, output.length > 0 {
            rawData = Data(bytes: data, count: Int(output.length))
        } else {
            rawData = Data()
        }
        guard status == Int32(YAZA_MOBILE_V1_OK.rawValue) else {
            let msg = String(decoding: rawData, as: UTF8.self)
            throw MobileBridgeError(status: status, message: msg)
        }
        return rawData
    }

    func dispatch(event: UInt32, payload: String) throws -> String {
        let data = try dispatchBytes(event: event, payload: payload)
        return String(decoding: data, as: UTF8.self)
    }

    deinit {
        if handle > 0 { _ = destroyFn(handle) }
        if let dl = dlHandle { _ = dlclose(dl) }
    }
}

private struct MobileBridgeError: LocalizedError {
    let status: Int32
    let message: String

    var errorDescription: String? {
        if message.isEmpty {
            return "YAZA mobile engine failed (status " + String(status) + ")"
        }
        return message
    }
}
