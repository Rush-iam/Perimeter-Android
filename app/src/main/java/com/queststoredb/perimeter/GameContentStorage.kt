package com.queststoredb.perimeter

import android.Manifest
import android.content.Context
import android.content.pm.PackageManager
import android.net.Uri
import android.os.Build
import android.os.Environment
import android.provider.DocumentsContract
import java.io.File

/** Stores the SAF selection and converts local primary-storage trees to a real path. */
class GameContentStorage(context: Context) {
    private val appContext = context.applicationContext
    private val preferences = context.applicationContext.getSharedPreferences("game-content", Context.MODE_PRIVATE)

    fun selectedTree(): Uri? = preferences.getString("tree", null)?.let(Uri::parse)

    fun hasStorageAccess(): Boolean =
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            Environment.isExternalStorageManager()
        } else {
            appContext.checkSelfPermission(Manifest.permission.READ_EXTERNAL_STORAGE) == PackageManager.PERMISSION_GRANTED &&
                appContext.checkSelfPermission(Manifest.permission.WRITE_EXTERNAL_STORAGE) == PackageManager.PERMISSION_GRANTED
        }

    fun select(uri: Uri) {
        check(uri.authority == "com.android.externalstorage.documents") {
            "Select a folder from local device storage or an SD card."
        }
        check(preferences.edit().putString("tree", uri.toString()).commit()) { "Cannot save selected folder" }
    }

    fun localFilesystemPath(): String? {
        if (!hasStorageAccess()) return null
        check(Build.VERSION.SDK_INT != Build.VERSION_CODES.Q || Environment.isExternalStorageLegacy()) {
            "Direct storage access is unavailable. Install the updated app with Android 10 legacy storage enabled."
        }
        val uri = selectedTree() ?: return null
        if (uri.authority != "com.android.externalstorage.documents") return null
        val id = DocumentsContract.getTreeDocumentId(uri)
        val separator = id.indexOf(':')
        if (separator <= 0 || !id.substring(0, separator).equals("primary", true)) return null
        val relative = id.substring(separator + 1).trim('/')
        val root = Environment.getExternalStorageDirectory().canonicalFile
        val path = if (relative.isEmpty()) root else File(root, relative).canonicalFile
        check(path == root || path.path.startsWith(root.path + File.separator)) { "Invalid selected path" }
        check(path.isDirectory && File(path, "Perimeter.ini").isFile) {
            "Select the game folder containing Perimeter.ini."
        }
        check(path.list() != null) { "Cannot list the game folder. Check Storage permission in app settings." }
        File(path, "Perimeter.ini").inputStream().use { it.read() }
        return path.path
    }
}
