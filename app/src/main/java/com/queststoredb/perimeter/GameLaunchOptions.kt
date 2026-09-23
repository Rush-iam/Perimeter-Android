package com.queststoredb.perimeter

import android.content.Context

internal data class RendererSelection(val graph: String, val dxvkVersion: String?)

class GameLaunchOptions(private val context: Context) {
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

    /** DXVK generation loaded by the next Android game process. */
    fun dxvkVersion(): String? = parse(load())
        .lastOrNull { it.removePrefix("tmp_").substringBefore('=') == "android_dxvk_version" }
        ?.substringAfter('=')
        ?.takeIf { it == "1" || it == "2" }

    /** Selects and persists the best renderer when no complete renderer choice exists. */
    internal fun ensureRendererSelected() {
        val arguments = parse(load())
        val graph = arguments.lastOrNull { keyOf(it) == GRAPH_KEY }?.substringAfter('=')
        val version = arguments.lastOrNull { keyOf(it) == DXVK_VERSION_KEY }?.substringAfter('=')
        val hasCompleteChoice = when {
            graph == SOKOL_GRAPH -> true
            (graph == null || graph == DEFAULT_GRAPH) && version != null && version in DXVK_VERSIONS -> true
            else -> false
        }
        if (hasCompleteChoice) return

        val preserved = arguments.filterNot { keyOf(it) == GRAPH_KEY || keyOf(it) == DXVK_VERSION_KEY }
        val best = bestSupportedRenderer()
        save((preserved + listOfNotNull(
            "$GRAPH_KEY=${best.graph}",
            best.dxvkVersion?.let { "$DXVK_VERSION_KEY=$it" }
        )).joinToString("\n"))
    }

    internal fun bestSupportedRenderer(): RendererSelection = when {
        AndroidVulkanCapabilities.supportsVulkan13() -> RendererSelection(DEFAULT_GRAPH, "2")
        AndroidVulkanCapabilities.supportsVulkan11() -> RendererSelection(DEFAULT_GRAPH, "1")
        else -> RendererSelection(SOKOL_GRAPH, null)
    }

    /** Requests Android Sustained Performance Mode to disable boost clocks. */
    fun sustainedPerformance(): Boolean = parse(load())
        .lastOrNull { it.removePrefix("tmp_").substringBefore('=') == "sustained_performance" }
        ?.substringAfter('=') == "1"

    /** Whether the Android process memory sampler should run during gameplay. */
    fun memoryMonitorEnabled(): Boolean = parse(load())
        .lastOrNull { it.removePrefix("tmp_").substringBefore('=') == MEMORY_MONITOR_KEY }
        ?.substringAfter('=') == "1"

    /** Replaces the renderer argument without discarding unrelated launch options. */
    fun selectRenderer(renderer: String) {
        val arguments = parse(load())
            .filterNot {
                val key = it.removePrefix("tmp_").substringBefore('=')
                key == GRAPH_KEY || renderer == SOKOL_GRAPH && key == DXVK_VERSION_KEY
            }
        save((arguments + "graph=$renderer").joinToString("\n"))
    }

    /** Replaces the Android DXVK generation without discarding other options. */
    fun selectDxvkVersion(version: String) {
        require(version == "1" || version == "2") { "DXVK version must be 1 or 2." }
        val arguments = parse(load())
            .filterNot { it.removePrefix("tmp_").substringBefore('=') == "android_dxvk_version" }
        save((arguments + "android_dxvk_version=$version").joinToString("\n"))
    }

    fun arguments(contentPath: String): Array<String> {
        val custom = parse(load())
        val keys = custom.map(::keyOf).toSet()
        // SDL passes each array entry as one argument, so spaces need no shell quoting
        // RunBackground is an engine focus policy: with 0, a focus-loss event
        // stops engine update/render quantization and pauses the network client.
        val defaults = mutableListOf(
            "content=$contentPath",
            "FullScreen=1",
            "VSync=1",
            "RunBackground=0",
            "show_lifebars=1"
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
        private const val GRAPH_KEY = "graph"
        private const val DXVK_VERSION_KEY = "android_dxvk_version"
        internal const val MEMORY_MONITOR_KEY = "android_memory_monitor"
        private const val SOKOL_GRAPH = "sokol"
        private const val DEFAULT_GRAPH = "d3d9"
        private val DXVK_VERSIONS = setOf("1", "2")
        private fun keyOf(argument: String): String =
            argument.removePrefix("tmp_").substringBefore('=')

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
