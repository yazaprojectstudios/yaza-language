package dev.yaza.mobile

import android.app.Activity
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.view.Gravity
import android.widget.Button
import android.widget.LinearLayout
import android.widget.TextView
import java.util.concurrent.Executors

class MainActivity : Activity() {
    private val engineExecutor = Executors.newSingleThreadExecutor()
    private val mainHandler = Handler(Looper.getMainLooper())
    private var engine: Long = 0L
    private var dispatchPending = false
    private var disposed = false
    private var lifecycleGeneration = 0L
    private lateinit var stateView: TextView
    private lateinit var errorView: TextView

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        stateView = TextView(this).apply {
            textSize = 30f
            gravity = Gravity.CENTER
            text = "Counter: 0"
        }
        errorView = TextView(this).apply {
            textSize = 14f
            gravity = Gravity.CENTER
        }
        val increment = Button(this).apply {
            text = "Increment"
            setOnClickListener { dispatch(1, "1") }
        }
        val reset = Button(this).apply {
            text = "Reset"
            setOnClickListener { dispatch(2, "") }
        }
        setContentView(LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            gravity = Gravity.CENTER
            setPadding(24, 24, 24, 24)
            addView(stateView)
            addView(errorView)
            addView(increment)
            addView(reset)
        })
        try {
            engine = NativeEngine.create()
        } catch (failure: Exception) {
            errorView.text = failure.message ?: "Could not create YAZA engine"
        }
    }

    private fun dispatch(eventId: Int, payload: String) {
        if (disposed) return
        if (engine == 0L) {
            errorView.text = "YAZA engine is not initialized"
            return
        }
        if (dispatchPending) {
            errorView.text = "YAZA engine is processing the previous event"
            return
        }
        dispatchPending = true
        val requestGeneration = lifecycleGeneration
        val activeEngine = engine
        errorView.text = "Working…"
        engineExecutor.execute {
            val result = runCatching { NativeEngine.dispatch(activeEngine, eventId, payload) }
            mainHandler.post {
                if (disposed || lifecycleGeneration != requestGeneration) return@post
                dispatchPending = false
                result.onSuccess { stateView.text = "Counter: $it" }
                    .onFailure { failure -> errorView.text = failure.message ?: "YAZA engine error" }
                if (result.isSuccess) errorView.text = ""
            }
        }
    }

    override fun onStart() {
        super.onStart()
        if (!disposed && engine != 0L) dispatch(3, "")
    }

    override fun onStop() {
        lifecycleGeneration++
        dispatchPending = false
        super.onStop()
    }

    override fun onDestroy() {
        disposed = true
        lifecycleGeneration++
        mainHandler.removeCallbacksAndMessages(null)
        val closingEngine = engine
        engine = 0L
        if (closingEngine != 0L) {
            engineExecutor.execute { NativeEngine.destroy(closingEngine) }
        }
        engineExecutor.shutdown()
        super.onDestroy()
    }
}

