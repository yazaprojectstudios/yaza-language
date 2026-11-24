package dev.yaza.mobile

import android.app.Activity
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.view.Gravity
import android.widget.Button
import android.widget.EditText
import android.widget.LinearLayout
import android.widget.TextView
import android.widget.Toast
import java.util.concurrent.Executors

class MainActivity : Activity() {
    private val engineExecutor = Executors.newSingleThreadExecutor()
    private val mainHandler = Handler(Looper.getMainLooper())
    private var engine: Long = 0L
    private var dispatchPending = false
    private var disposed = false
    private var lifecycleGeneration = 0L

    private lateinit var titleView: TextView
    private lateinit var greetingView: TextView
    private lateinit var counterView: TextView
    private lateinit var statusView: TextView
    private lateinit var errorView: TextView
    private lateinit var nameInput: EditText

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        titleView = TextView(this).apply {
            textSize = 24f
            gravity = Gravity.CENTER
            text = "Welcome to YAZA Mobile"
        }
        greetingView = TextView(this).apply {
            textSize = 16f
            gravity = Gravity.CENTER
            text = "Memuat Shared YAZA Core..."
        }
        counterView = TextView(this).apply {
            textSize = 28f
            gravity = Gravity.CENTER
            text = "Counter: 0"
        }
        statusView = TextView(this).apply {
            textSize = 13f
            gravity = Gravity.CENTER
            text = "Status: init"
        }
        errorView = TextView(this).apply {
            textSize = 14f
            gravity = Gravity.CENTER
        }
        nameInput = EditText(this).apply {
            hint = "Masukkan nama Anda"
            setSingleLine(true)
        }
        val greetBtn = Button(this).apply {
            text = "Sapa"
            setOnClickListener { dispatchAction("GREET|" + nameInput.text.toString(), showToast = true) }
        }
        val incrementBtn = Button(this).apply {
            text = "Increment (+1)"
            setOnClickListener { dispatchAction("INCREMENT|1", showToast = true) }
        }
        val persistBtn = Button(this).apply {
            text = "Simpan State"
            setOnClickListener { dispatchAction("PERSIST", showToast = true) }
        }
        val resetBtn = Button(this).apply {
            text = "Reset"
            setOnClickListener { dispatchAction("RESET", showToast = true) }
        }
        setContentView(LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            gravity = Gravity.CENTER
            setPadding(32, 32, 32, 32)
            addView(titleView)
            addView(greetingView)
            addView(counterView)
            addView(statusView)
            addView(errorView)
            addView(nameInput)
            addView(greetBtn)
            addView(incrementBtn)
            addView(persistBtn)
            addView(resetBtn)
        })
        try {
            engine = NativeEngine.create()
        } catch (failure: Exception) {
            errorView.text = failure.message ?: "Could not create YAZA engine"
        }
    }

    private fun dispatchAction(actionPayload: String, showToast: Boolean = false) {
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
        errorView.text = ""
        engineExecutor.execute {
            val result = runCatching {
                val raw = NativeEngine.dispatchBytes(activeEngine, NativeEngine.EVENT_ACTION, actionPayload)
                NativeEngine.decodeYbifState(raw)
            }
            mainHandler.post {
                if (disposed || lifecycleGeneration != requestGeneration) return@post
                dispatchPending = false
                result.onSuccess { uiState ->
                    titleView.text = uiState.title
                    greetingView.text = uiState.greeting
                    counterView.text = "Counter: ${uiState.counter}"
                    val kvEffect = uiState.effects.firstOrNull { it.startsWith("KV_SAVE|") }
                    statusView.text = if (kvEffect != null) {
                        "Status: ${uiState.status} ($kvEffect)"
                    } else {
                        "Status: ${uiState.status}"
                    }
                    if (showToast) {
                        val toastMsg = uiState.effects.firstOrNull { it.startsWith("TOAST|") }?.removePrefix("TOAST|")
                        if (!toastMsg.isNullOrEmpty()) {
                            Toast.makeText(this@MainActivity, toastMsg, Toast.LENGTH_SHORT).show()
                        }
                    }
                    errorView.text = ""
                }.onFailure { failure ->
                    errorView.text = failure.message ?: "YAZA engine error"
                }
            }
        }
    }

    override fun onStart() {
        super.onStart()
        if (!disposed && engine != 0L) dispatchAction("INIT", showToast = false)
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
