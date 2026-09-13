package com.queststoredb.perimeter

import android.content.Context

class GameLaunchOptions(context: Context) {
    private val preferences = context.getSharedPreferences("game_launch", Context.MODE_PRIVATE)
    fun load(): String = preferences.getString("arguments", "") ?: ""

    fun save(text: String) {
        parse(text)
        preferences.edit().putString("arguments", text).apply()
    }

    /** The renderer selected by launch arguments; D3D9/DXVK is the engine default. */
    fun renderer(): String = parse(load())
        .lastOrNull { it.removePrefix("tmp_").substringBefore('=') == "graph" }
        ?.substringAfter('=')
        ?: "d3d9"

    /** Requests Android Sustained Performance Mode to disable boost clocks. */
    fun sustainedPerformance(): Boolean = parse(load())
        .lastOrNull { it.removePrefix("tmp_").substringBefore('=') == "sustained_performance" }
        ?.substringAfter('=') == "1"

    /** Replaces the renderer argument without discarding unrelated launch options. */
    fun selectRenderer(renderer: String) {
        val arguments = parse(load())
            .filterNot { it.removePrefix("tmp_").substringBefore('=') == "graph" }
        save((arguments + "graph=$renderer").joinToString("\n"))
    }

    fun arguments(contentPath: String): Array<String> {
        val custom = parse(load())
        val keys = custom.map { it.removePrefix("tmp_").substringBefore('=') }.toSet()
        // SDL passes each array entry as one argument, so spaces need no shell quoting
        // RunBackground is an engine focus policy: with 0, a focus-loss event
        // stops engine update/render quantization and pauses the network client.
        val defaults = mutableListOf(
            "content=$contentPath",
            "FullScreen=1",
            "VSync=1",
            "RunBackground=0"
        )
        // The alternate-LOD cache uses shared tilemap resources and is safe
        // for both Android renderers. Keep an explicit 0 authoritative for
        // A/B comparisons.
        if ("zoom_lod_cache" !in keys) {
            defaults += "zoom_lod_cache=1"
        }
        return (custom + defaults.filter { it.substringBefore('=') !in keys }).toTypedArray()
    }

    companion object {
        internal fun parse(text: String): List<String> {
            val entries = text.lineSequence().map { it.trim() }.filter { it.isNotEmpty() }.toList()
            val keys = mutableSetOf<String>()
            for (entry in entries) {
                require('\u0000' !in entry) { "Arguments cannot contain NUL characters." }
                val key = entry.removePrefix("tmp_").substringBefore('=')
                require('=' in entry && key.matches(Regex("[A-Za-z_][A-Za-z_0-9:]*"))) {
                    "Use key=value, one argument per line."
                }
                require(keys.add(key)) { "Duplicate argument: $key" }
            }
            return entries
        }

    }
}
