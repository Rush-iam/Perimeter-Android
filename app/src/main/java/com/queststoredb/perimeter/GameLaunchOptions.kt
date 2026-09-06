package com.queststoredb.perimeter

import android.app.Activity
import android.content.Context

class GameLaunchOptions(context: Context) {
    private val preferences = context.getSharedPreferences("game_launch", Context.MODE_PRIVATE)
    private var text: String
        get() = preferences.getString("arguments", "") ?: ""
        set(value) { preferences.edit().putString("arguments", value).apply() }

    fun arguments(contentPath: String): Array<String> {
        val custom = parse(text)
        val keys = custom.map { it.removePrefix("tmp_").substringBefore('=') }.toSet()
        // SDL passes each array entry as one argument, so spaces need no shell quoting
        val defaults = listOf("content=$contentPath")
        return (custom + defaults.filter { it.substringBefore('=') !in keys }).toTypedArray()
    }

    fun showEditor(activity: Activity) {
        GameLaunchOptionsEditor(activity, text) { text = it }.show()
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
