import SwiftUI

private let yazaEngineQueue = DispatchQueue(label: "dev.yaza.mobile.engine", qos: .userInitiated)

@main
struct YazaMobileApp: App {
    var body: some Scene {
        WindowGroup {
            CounterView()
        }
    }
}

@MainActor
struct CounterView: View {
    @Environment(\.scenePhase) private var scenePhase
    @State private var engine: MobileEngine?
    @State private var counter = "0"
    @State private var errorMessage = ""
    @State private var dispatchPending = false
    @State private var lifecycleGeneration: UInt64 = 0

    var body: some View {
        VStack(spacing: 20) {
            Text("YAZA Mobile")
                .font(.title)
            Text("Counter: " + counter)
                .font(.largeTitle)
                .accessibilityIdentifier("counter")
            if !errorMessage.isEmpty {
                Text(errorMessage)
                    .foregroundStyle(.red)
                    .accessibilityIdentifier("error")
            }
            Button("Increment") {
                dispatch(event: 1, payload: "1")
            }
            .buttonStyle(.borderedProminent)
            .disabled(engine == nil || dispatchPending)
            Button("Reset") {
                dispatch(event: 2, payload: "")
            }
            .buttonStyle(.bordered)
            .disabled(engine == nil || dispatchPending)
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
            refreshState()
        }
        .onChange(of: scenePhase) { phase in
            if phase == .active {
                refreshState()
            } else if phase == .background {
                lifecycleGeneration &+= 1
                dispatchPending = false
            }
        }
        .onDisappear {
            // ABI v1 dispatch is synchronous and cannot be interrupted safely.
            // Invalidate its UI result if this screen leaves while work runs.
            lifecycleGeneration &+= 1
            dispatchPending = false
        }
    }

    private func dispatch(event: UInt32, payload: String) {
        guard let activeEngine = engine else {
            errorMessage = "YAZA engine is not initialized"
            return
        }
        guard !dispatchPending else { return }
        dispatchPending = true
        let requestGeneration = lifecycleGeneration
        errorMessage = "Working…"
        yazaEngineQueue.async {
            let result = Result { try activeEngine.dispatch(event: event, payload: payload) }
            DispatchQueue.main.async {
                guard self.lifecycleGeneration == requestGeneration else { return }
                self.dispatchPending = false
                switch result {
                case .success(let nextState):
                    self.counter = nextState
                    self.errorMessage = ""
                case .failure(let failure):
                    self.errorMessage = failure.localizedDescription
                }
            }
        }
    }

    private func refreshState() {
        guard let activeEngine = engine, !dispatchPending else { return }
        dispatchPending = true
        let requestGeneration = lifecycleGeneration
        errorMessage = "Loading…"
        yazaEngineQueue.async {
            let result = Result { try activeEngine.dispatch(event: 3, payload: "") }
            DispatchQueue.main.async {
                guard self.lifecycleGeneration == requestGeneration else { return }
                self.dispatchPending = false
                switch result {
                case .success(let currentState):
                    self.counter = currentState
                    self.errorMessage = ""
                case .failure(let failure):
                    self.errorMessage = failure.localizedDescription
                }
            }
        }
    }
}

// The view sends every call through `yazaEngineQueue`; the handle is never
// accessed concurrently, and the bridge also serializes each engine handle.
private final class MobileEngine: @unchecked Sendable {
    private var handle: Int64 = 0

    init() throws {
        var host = YazaMobileHostV1(
            struct_size: UInt32(MemoryLayout<YazaMobileHostV1>.size),
            abi_version: UInt32(YAZA_MOBILE_V1_ABI_VERSION),
            context: nil,
            post_to_main_thread: nil
        )
        let status = yaza_mobile_v1_create(&host, &handle)
        guard status == Int32(YAZA_MOBILE_V1_OK.rawValue), handle > 0 else {
            throw MobileBridgeError(status: status, message: "Could not create YAZA engine")
        }
    }

    func dispatch(event: UInt32, payload: String) throws -> String {
        guard handle > 0 else {
            throw MobileBridgeError(status: Int32(YAZA_MOBILE_V1_INVALID_ARGUMENT.rawValue), message: "YAZA engine was destroyed")
        }
        let bytes = Array(payload.utf8)
        var output = YazaMobileBufferV1(data: nil, length: 0, lease_id: 0)
        let status = bytes.withUnsafeBufferPointer { buffer -> Int32 in
            let view = YazaMobileView(data: buffer.baseAddress, length: UInt64(buffer.count))
            return yaza_mobile_v1_dispatch(handle, event, view, &output)
        }
        defer { _ = yaza_mobile_v1_buffer_release(&output) }
        let message: String
        if let data = output.data, output.length > 0 {
            message = String(decoding: UnsafeBufferPointer(start: data, count: Int(output.length)), as: UTF8.self)
        } else {
            message = ""
        }
        guard status == Int32(YAZA_MOBILE_V1_OK.rawValue) else {
            throw MobileBridgeError(status: status, message: message)
        }
        return message
    }

    deinit {
        if handle > 0 { _ = yaza_mobile_v1_destroy(handle) }
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

