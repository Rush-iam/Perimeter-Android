package com.queststoredb.perimeter

import android.content.Context

class GameLaunchOptions(context: Context) {
    private val preferences = context.getSharedPreferences("game_launch", Context.MODE_PRIVATE)
    fun load(): String = preferences.getString("arguments", "") ?: ""

    fun save(text: String) {
        parse(text)
        preferences.edit().putString("arguments", text).apply()
    }

    fun arguments(contentPath: String): Array<String> {
        val custom = parse(load())
        val keys = custom.map { it.removePrefix("tmp_").substringBefore('=') }.toSet()
        // SDL passes each array entry as one argument, so spaces need no shell quoting
        // RunBackground is an engine focus policy: with 0, a focus-loss event
        // stops engine update/render quantization and pauses the network client.
        val defaults = listOf(
            "content=$contentPath",
            "FullScreen=1",
            "VSync=1",
            "RunBackground=0"
        )
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
