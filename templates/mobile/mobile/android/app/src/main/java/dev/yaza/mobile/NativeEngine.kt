package dev.yaza.mobile

import java.nio.ByteBuffer
import java.nio.ByteOrder

object NativeEngine {
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

    fun dispatch(engine: Long, eventId: Int, payload: String): String {
        val encoded = nativeDispatch(engine, eventId, payload.toByteArray(Charsets.UTF_8))
        if (encoded.size < Int.SIZE_BYTES) {
            throw IllegalStateException("YAZA mobile bridge returned a truncated status")
        }
        val status = ByteBuffer.wrap(encoded, 0, Int.SIZE_BYTES)
            .order(ByteOrder.BIG_ENDIAN)
            .int
        val body = encoded.copyOfRange(Int.SIZE_BYTES, encoded.size).toString(Charsets.UTF_8)
        if (status != 0) {
            throw IllegalStateException(if (body.isEmpty()) "YAZA mobile engine failed ($status)" else body)
        }
        return body
    }

    fun destroy(engine: Long) {
        if (engine != 0L) nativeDestroy(engine)
    }
}

