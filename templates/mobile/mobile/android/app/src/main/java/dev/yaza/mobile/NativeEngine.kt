package dev.yaza.mobile

import java.nio.ByteBuffer
import java.nio.ByteOrder

data class MobileUiState(
    val title: String,
    val greeting: String,
    val counter: Long,
    val status: String,
    val effects: List<String>
)

object NativeEngine {
    const val EVENT_INCREMENT = 1
    const val EVENT_RESET = 2
    const val EVENT_SNAPSHOT = 3
    const val EVENT_ACTION = 4

    init {
        System.loadLibrary("yaza_mobile")
    }

    private external fun nativeCreate(): Long
    private external fun nativeDispatch(engine: Long, eventId: Int, payloadUtf8: ByteArray): ByteArray
    private external fun nativeDestroy(engine: Long)

    fun create(): Long {
        val engine = nativeCreate()
        if (engine == 0L) throw IllegalStateException("Could not create YAZA mobile engine")
        return engine
    }

    fun dispatchBytes(engine: Long, eventId: Int, payload: String): ByteArray {
        val encoded = nativeDispatch(engine, eventId, payload.toByteArray(Charsets.UTF_8))
        if (encoded.size < Int.SIZE_BYTES) {
            throw IllegalStateException("YAZA mobile bridge returned a truncated status")
        }
        val status = ByteBuffer.wrap(encoded, 0, Int.SIZE_BYTES)
            .order(ByteOrder.BIG_ENDIAN)
            .int
        val body = encoded.copyOfRange(Int.SIZE_BYTES, encoded.size)
        if (status != 0) {
            val errMsg = body.toString(Charsets.UTF_8)
            throw IllegalStateException(if (errMsg.isEmpty()) "YAZA mobile engine failed ($status)" else errMsg)
        }
        return body
    }

    fun dispatch(engine: Long, eventId: Int, payload: String): String {
        return dispatchBytes(engine, eventId, payload).toString(Charsets.UTF_8)
    }

    fun decodeYbifState(bytes: ByteArray): MobileUiState {
        if (bytes.size >= 5 &&
            bytes[0] == 0x59.toByte() &&
            bytes[1] == 0x42.toByte() &&
            bytes[2] == 0x49.toByte() &&
            bytes[3] == 0x46.toByte() &&
            bytes[4] == 0x01.toByte()
        ) {
            var pos = 5
            fun readText(): String {
                if (pos + 5 > bytes.size || bytes[pos] != 0x04.toByte()) {
                    throw IllegalStateException("Invalid YBIF text tag at offset $pos")
                }
                val len = ByteBuffer.wrap(bytes, pos + 1, 4).order(ByteOrder.LITTLE_ENDIAN).int
                if (len < 0 || pos + 5 + len > bytes.size) {
                    throw IllegalStateException("Invalid YBIF text length at offset $pos")
                }
                val str = bytes.copyOfRange(pos + 5, pos + 5 + len).toString(Charsets.UTF_8)
                pos += 5 + len
                return str
            }
            fun readI64(): Long {
                if (pos + 9 > bytes.size || bytes[pos] != 0x03.toByte()) {
                    throw IllegalStateException("Invalid YBIF number tag at offset $pos")
                }
                val v = ByteBuffer.wrap(bytes, pos + 1, 8).order(ByteOrder.LITTLE_ENDIAN).long
                pos += 9
                return v
            }
            val title = readText()
            val greeting = readText()
            val counter = readI64()
            val status = readText()
            val rawEffects = if (pos < bytes.size) readText() else ""
            val fxList = rawEffects.split('\n').map { it.trim() }.filter { it.isNotEmpty() }
            return MobileUiState(title, greeting, counter, status, fxList)
        }
        val legacyCounter = bytes.toString(Charsets.UTF_8).trim().toLongOrNull() ?: 0L
        return MobileUiState(
            title = "Welcome to YAZA Mobile",
            greeting = "Shared YAZA Core aktif",
            counter = legacyCounter,
            status = "ready",
            effects = emptyList()
        )
    }

    fun destroy(engine: Long) {
        if (engine != 0L) nativeDestroy(engine)
    }
}
