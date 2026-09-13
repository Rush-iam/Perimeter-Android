package com.queststoredb.perimeter

import android.app.Activity
import android.app.AlertDialog
import android.graphics.Typeface
import android.os.PowerManager
import android.text.InputType
import android.view.View
import android.view.WindowManager
import android.widget.ArrayAdapter
import android.widget.Button
import android.widget.CheckBox
import android.widget.EditText
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.Spinner
import android.widget.TextView
import java.io.File

/** Edits a draft; only Save changes the persisted engine arguments. */
internal class GameLaunchOptionsEditor(
    private val activity: Activity,
    original: String,
    private val save: (String) -> Unit
) {
    private val saved = GameLaunchOptions.parse(original)
        .associateBy { it.removePrefix("tmp_").substringBefore('=') }
    private val sustainedPerformanceSupported =
        activity.getSystemService(PowerManager::class.java)?.isSustainedPerformanceModeSupported == true
    private val readers = linkedMapOf<String, () -> String?>()
    private val resetters = mutableListOf<() -> Unit>()

    @Suppress("DEPRECATION") // Platform dialogs still use adjustResize for the software keyboard.
    fun show() {
        val layout = column().apply { setPadding(dp(20), dp(8), dp(20), dp(24)) }
        for ((section, options) in specs.groupBy { it.section }) {
            if (section.isEmpty()) {
                options.forEach { addOption(layout, it) }
                continue
            }
            val body = column().apply { visibility = View.GONE }
            val heading = Button(activity).apply {
                text = "$section  +"
                isAllCaps = false
                gravity = android.view.Gravity.START or android.view.Gravity.CENTER_VERTICAL
                setOnClickListener {
                    val expanded = body.visibility != View.VISIBLE
                    body.visibility = if (expanded) View.VISIBLE else View.GONE
                    text = "$section  ${if (expanded) "−" else "+"}"
                }
            }
            layout.addView(heading, fullWidth())
            layout.addView(body, fullWidth())
            options.forEach { addOption(body, it) }
        }

        val error = label("", 14f).apply {
            visibility = View.GONE
            accessibilityLiveRegion = View.ACCESSIBILITY_LIVE_REGION_POLITE
        }
        layout.addView(error)
        val dialog = AlertDialog.Builder(activity)
            .setTitle(R.string.launch_arguments)
            .setView(ScrollView(activity).apply { addView(layout) })
            .setPositiveButton(R.string.save_launch_arguments, null)
            .setNegativeButton("Cancel", null)
            .setNeutralButton("Reset", null)
            .create()
        dialog.setOnShowListener {
            dialog.window?.setLayout(WindowManager.LayoutParams.MATCH_PARENT, WindowManager.LayoutParams.MATCH_PARENT)
            dialog.window?.setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_ADJUST_RESIZE)
            dialog.getButton(AlertDialog.BUTTON_NEUTRAL).setOnClickListener {
                resetters.forEach { it() }
                error.visibility = View.GONE
            }
            dialog.getButton(AlertDialog.BUTTON_POSITIVE).setOnClickListener {
                try {
                    val entries = readers.mapNotNull { (key, read) ->
                        read()?.let { value ->
                            "$key=$value"
                        }
                    }
                    val text = entries.joinToString("\n")
                    GameLaunchOptions.parse(text)
                    save(text)
                    dialog.dismiss()
                } catch (invalid: IllegalArgumentException) {
                    error.text = invalid.message
                    error.visibility = View.VISIBLE
                    (layout.parent as ScrollView).post { error.requestRectangleOnScreen(android.graphics.Rect(0, 0, error.width, error.height)) }
                }
            }
        }
        dialog.show()
    }

    private fun addOption(parent: LinearLayout, spec: Option) {
        val row = column().apply { setPadding(dp(8), dp(12), dp(8), dp(16)) }
        parent.addView(row, fullWidth())
        if (spec.kind == Kind.ACTION) {
            row.addView(Button(activity).apply {
                text = spec.title
                isAllCaps = false
                setOnClickListener { showLastLog() }
            }, fullWidth())
            if (spec.description.isNotEmpty()) row.addView(label(spec.description, 13f))
            parent.addView(View(activity).apply { setBackgroundColor(0x33888888) }, LinearLayout.LayoutParams(-1, dp(1)))
            return
        }
        val old = saved[spec.key]?.substringAfter('=')
        if (spec.kind == Kind.FLAG || spec.kind == Kind.TOGGLE || spec.kind == Kind.INVERTED_BOOLEAN) {
            val checkbox = CheckBox(activity).apply {
                val supported = spec.key != SUSTAINED_PERFORMANCE_KEY || sustainedPerformanceSupported
                text = if (supported) spec.title else "(unsupported) ${spec.title}"
                isEnabled = supported
                // Presence flags are enabled even when their original value was "0".
                isChecked = when (spec.kind) {
                    Kind.FLAG -> old != null
                    Kind.TOGGLE -> old?.toIntOrNull()?.let { it != 0 } == true
                    Kind.INVERTED_BOOLEAN -> old == "0"
                    else -> false
                }
            }
            row.addView(checkbox, fullWidth())
            readers[spec.key] = {
                if (spec.kind == Kind.TOGGLE) {
                    if (checkbox.isChecked) "1" else "0"
                } else if (spec.kind == Kind.INVERTED_BOOLEAN) {
                    if (checkbox.isChecked) "0" else null
                } else if (checkbox.isChecked) old ?: "1" else null
            }
            resetters += { checkbox.isChecked = false }
        } else {
            row.addView(label(spec.title, 16f).apply { setTypeface(typeface, Typeface.BOLD) })
            when (spec.kind) {
                Kind.CHOICE -> {
                    val choices = listOf((spec.defaultChoiceValue ?: "") to spec.defaultChoiceLabel) + spec.choices
                    val selector = Spinner(activity).apply {
                        adapter = ArrayAdapter(activity, android.R.layout.simple_spinner_item, choices.map { it.second }).apply {
                            setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item)
                        }
                        contentDescription = spec.title
                        setSelection(if (old == null) 0 else choices.indexOfFirst { it.first == old }.coerceAtLeast(0))
                    }
                    row.addView(selector, fullWidth())
                    readers[spec.key] = { choices[selector.selectedItemPosition].first.ifEmpty { null } }
                    resetters += { selector.setSelection(0) }
                }
                else -> {
                    val field = EditText(activity).apply {
                        setSingleLine(true)
                        inputType = when (spec.kind) {
                            Kind.PASSWORD -> InputType.TYPE_CLASS_TEXT or InputType.TYPE_TEXT_VARIATION_PASSWORD
                            else -> InputType.TYPE_CLASS_TEXT or InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS
                        }
                        hint = spec.valueHint
                        contentDescription = spec.title
                        setText(old ?: "")
                    }
                    row.addView(field, fullWidth())
                    readers[spec.key] = {
                        val value = field.text.toString().trim()
                        if (value.isEmpty()) null else {
                            require('\n' !in value && '\r' !in value) { "${spec.title}: enter a single value." }
                            value
                        }
                    }
                    resetters += { field.setText("") }
                }
            }
        }
        if (spec.description.isNotEmpty()) row.addView(label(spec.description, 13f))
        parent.addView(View(activity).apply { setBackgroundColor(0x33888888) }, LinearLayout.LayoutParams(-1, dp(1)))
    }

    private fun showLastLog() {
        try {
            // SDL_GetPrefPath() maps to the app's internal files directory on Android.
            val log = File(activity.filesDir, "logfile.txt")
            check(log.isFile) { "No logfile.txt was found at ${log.path}." }
            val text = log.inputStream().bufferedReader().use { reader ->
                val contents = reader.readText()
                if (contents.length > MAX_LOG_CHARACTERS) {
                    contents.take(MAX_LOG_CHARACTERS) + "\n\n[Log truncated]"
                } else {
                    contents
                }
            }
            val logView = TextView(activity).apply {
                setTextIsSelectable(true)
                typeface = Typeface.MONOSPACE
                this.text = if (text.isEmpty()) "[Log is empty]" else text
                setPadding(dp(16), dp(8), dp(16), dp(8))
            }
            AlertDialog.Builder(activity)
                .setTitle("Last log: ${log.name}")
                .setView(ScrollView(activity).apply { addView(logView) })
                .setPositiveButton(android.R.string.ok, null)
                .show()
        } catch (error: Exception) {
            AlertDialog.Builder(activity)
                .setTitle("Last log")
                .setMessage(error.localizedMessage ?: "Unable to read the last log.")
                .setPositiveButton(android.R.string.ok, null)
                .show()
        }
    }

    private fun column() = LinearLayout(activity).apply { orientation = LinearLayout.VERTICAL }
    private fun label(value: String, size: Float) = TextView(activity).apply { text = value; textSize = size }
    private fun dp(value: Int) = (value * activity.resources.displayMetrics.density).toInt()
    private fun fullWidth() = LinearLayout.LayoutParams(-1, -2)

    private enum class Kind { FLAG, TOGGLE, INVERTED_BOOLEAN, CHOICE, TEXT, PASSWORD, ACTION }
    private data class Option(val section: String, val key: String, val title: String, val kind: Kind,
                              val description: String = "", val choices: List<Pair<String, String>> = emptyList(),
                              val defaultChoiceLabel: String = "Game default", val valueHint: String? = "Game default",
                              val defaultChoiceValue: String? = null)

    private companion object {
        val specs = listOf(
            Option("", "locale", "Game language", Kind.CHOICE, choices = listOf(
                "English" to "English", "Russian" to "Russian"), defaultChoiceLabel = "Last used"),
            Option("Gameplay & startup", "start_splash", "Disable intro movies", Kind.INVERTED_BOOLEAN),
            Option("Gameplay & startup", "disable_sound", "Disable sound", Kind.FLAG),
            Option("Gameplay & startup", "disableBriefing", "Skip briefings", Kind.FLAG),
            Option("Gameplay & startup", "pause", "Start paused", Kind.FLAG),
            Option("Gameplay & startup", "autoSwitchAI", "Autoswitch controls to AI when idle", Kind.FLAG,
                "After 60 seconds without player input, the active human player becomes AI-controlled. Player input switches control back and resets the timer."),
            Option("Display & performance", "graph", "Graphics backend", Kind.CHOICE, choices = listOf(
                "sokol" to "Sokol / GLES3 legacy"),
                defaultChoiceLabel = "DXVK ${BuildConfig.DXVK_VERSION}.x / Vulkan ${if (BuildConfig.DXVK_VERSION == "1") "1.1" else "1.3"}",
                defaultChoiceValue = "d3d9"),
            Option("Display & performance", "show_fps", "Show FPS counter", Kind.TOGGLE),
            Option("Display & performance", "HT", "Disable multithreading", Kind.INVERTED_BOOLEAN),
            Option("Display & performance", "sustained_performance", "Android Sustained Performance Mode", Kind.TOGGLE,
                "Disables boost clocks"),
            Option("Replays", "saveplay", "Record replay to file", Kind.TEXT, valueHint = null),
            Option("Replays", "replay", "Replay file", Kind.TEXT, valueHint = null),
            Option("Replays", "AI", "Replay AI mode", Kind.CHOICE, choices = listOf(
                "0" to "No AI", "1" to "Normal", "2" to "All AI")),
            Option("Multiplayer", "name", "Player name", Kind.TEXT, valueHint = null),
            Option("Multiplayer", "server", "Host address", Kind.TEXT, "IP:port to listen on.", valueHint = null),
            Option("Multiplayer", "connect", "Join server", Kind.TEXT, "Server IP:port.", valueHint = null),
            Option("Multiplayer", "connect_room", "Join room ID", Kind.TEXT, valueHint = null),
            Option("Multiplayer", "room", "Hosted room name", Kind.TEXT, valueHint = null),
            Option("Multiplayer", "password", "Room password", Kind.PASSWORD, "Saved on this device.", valueHint = null),
            Option("Multiplayer", "public", "List hosted game publicly", Kind.TOGGLE,
                "By default, hosted games are private and listen only on their port."),
            Option("Multiplayer", "netrelay", "Relay server", Kind.TEXT),
            Option("Multiplayer", "ServerArchMask", "Architecture compatibility mask", Kind.TEXT, "Hexadecimal mask, such as FFFE."),
            Option("Diagnostics", "read_log_file", "Open last log file", Kind.ACTION),
            Option("Diagnostics", "console", "Redirect logs to Logcat instead of file", Kind.FLAG,
                "The log file is not created when enabled."),
            Option("Diagnostics", "frame_timing", "Record frame timing", Kind.TOGGLE,
                "Writes renderer-independent frame and presentation timestamps for benchmark captures."),
            Option("Diagnostics", "dxvk_max_frame_latency", "DXVK maximum frame latency", Kind.CHOICE,
                "Diagnostic override for the DXVK 1 D3D9 submission queue.",
                choices = listOf("1" to "1 frame"), defaultChoiceLabel = "DXVK default"),
            Option("Diagnostics", "zoom_lod_cache", "Zoom LOD cache", Kind.TOGGLE,
                "Cache prepared terrain LOD variants for camera zooms on either renderer."),
            Option("Diagnostics", "content_debug", "Log content loading", Kind.FLAG),
            Option("Diagnostics", "content_dump_debug", "Export content file mapping", Kind.FLAG),
            Option("Diagnostics", "debug_key_handler", "Enable debug keyboard commands", Kind.FLAG),
            Option("Diagnostics", "dump_mt_tls", "Log thread-local storage", Kind.FLAG),
            Option("Diagnostics", "render_debug", "Sokol Renderer debug panel", Kind.CHOICE, choices = listOf(
                "b" to "Buffers", "i" to "Images", "s" to "Samplers", "r" to "Shaders", "p" to "Pipelines",
                "a" to "Attachments", "u" to "Capture", "c" to "Capabilities", "f" to "Frame statistics"),
                defaultChoiceLabel = "Disabled"),
            Option("Diagnostics", "stack_frames", "Decode crash addresses", Kind.TEXT, "Comma-separated hexadecimal addresses. Decodes the trace, then exits.", valueHint = null),
            Option("Diagnostics", "stack_reference", "Crash reference address", Kind.TEXT, "Hexadecimal reference address from the same crash.", valueHint = null)
        )

        const val MAX_LOG_CHARACTERS = 512 * 1024
        const val SUSTAINED_PERFORMANCE_KEY = "sustained_performance"
    }
}
