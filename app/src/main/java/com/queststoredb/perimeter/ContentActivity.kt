package com.queststoredb.perimeter

import android.Manifest
import android.app.Activity
import android.app.AlertDialog
import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.os.Build
import android.provider.DocumentsContract
import android.provider.Settings
import android.widget.Button
import android.widget.LinearLayout
import android.widget.TextView
import android.widget.ScrollView
import android.view.View

/** Selects content, then obtains the raw path required by the native engine. */
class ContentActivity : Activity() {
    private lateinit var storage: GameContentStorage
    private lateinit var status: TextView
    private lateinit var choose: Button
    private lateinit var play: Button

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        storage = GameContentStorage(this)
        val layout = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            val padding = (24 * resources.displayMetrics.density).toInt()
            setPadding(padding, padding, padding, padding)
        }
        status = TextView(this)
        choose = Button(this)
        choose.setOnClickListener { chooseOrGrantAccess() }
        play = Button(this).apply {
            setText(R.string.play)
            setOnClickListener { startGameIfReady() }
            measure(View.MeasureSpec.UNSPECIFIED, View.MeasureSpec.UNSPECIFIED)
            layoutParams = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, measuredHeight * 3)
        }
        layout.addView(status)
        layout.addView(choose)
        layout.addView(Button(this).apply {
            setText(R.string.launch_arguments)
            setOnClickListener { GameLaunchOptions(this@ContentActivity).showEditor(this@ContentActivity) }
        })
        layout.addView(play)
        setContentView(ScrollView(this).apply { addView(layout) })
        refresh()
        if (savedInstanceState == null && Build.VERSION.SDK_INT == Build.VERSION_CODES.Q &&
            !storage.hasStorageAccess()) {
            requestLegacyStorageAccess()
        }
    }

    override fun onResume() {
        super.onResume()
        if (::storage.isInitialized) refresh()
    }

    private fun chooseOrGrantAccess() {
        if (!storage.hasStorageAccess()) {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                startActivity(Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION,
                    Uri.parse("package:$packageName")))
            } else {
                requestLegacyStorageAccess()
            }
            return
        }
        @Suppress("DEPRECATION")
        startActivityForResult(Intent(Intent.ACTION_OPEN_DOCUMENT_TREE).apply {
            addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION or Intent.FLAG_GRANT_WRITE_URI_PERMISSION or
                Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION or Intent.FLAG_GRANT_PREFIX_URI_PERMISSION)
            storage.selectedTree()?.let { putExtra(DocumentsContract.EXTRA_INITIAL_URI, it) }
        }, SELECT_CONTENT)
    }

    override fun onRequestPermissionsResult(requestCode: Int, permissions: Array<out String>, grantResults: IntArray) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        if (requestCode != REQUEST_STORAGE) return
        refresh()
        if (grantResults.isNotEmpty() && !storage.hasStorageAccess() &&
            permissions.indices.any { grantResults[it] != android.content.pm.PackageManager.PERMISSION_GRANTED &&
                !shouldShowRequestPermissionRationale(permissions[it]) }) {
            AlertDialog.Builder(this)
                .setTitle(R.string.grant_legacy_access)
                .setMessage(R.string.storage_permission_settings)
                .setPositiveButton(R.string.open_app_settings) { _, _ ->
                    startActivity(Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS,
                        Uri.parse("package:$packageName")))
                }
                .setNegativeButton(android.R.string.cancel, null)
                .show()
        }
    }

    private fun requestLegacyStorageAccess() {
        val permissions = arrayOf(Manifest.permission.READ_EXTERNAL_STORAGE,
            Manifest.permission.WRITE_EXTERNAL_STORAGE)
        if (permissions.any { shouldShowRequestPermissionRationale(it) }) {
            AlertDialog.Builder(this)
                .setTitle(R.string.grant_legacy_access)
                .setMessage(R.string.storage_permission_rationale)
                .setPositiveButton(R.string.grant_legacy_access) { _, _ ->
                    requestPermissions(permissions, REQUEST_STORAGE)
                }
                .setNegativeButton(android.R.string.cancel, null)
                .show()
        } else {
            requestPermissions(permissions, REQUEST_STORAGE)
        }
    }

    @Deprecated("Uses the platform activity result API for the existing wrapper")
    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        super.onActivityResult(requestCode, resultCode, data)
        if (requestCode != SELECT_CONTENT || resultCode != RESULT_OK) return
        val tree = data?.data ?: return
        try {
            val flags = Intent.FLAG_GRANT_READ_URI_PERMISSION or Intent.FLAG_GRANT_WRITE_URI_PERMISSION
            contentResolver.takePersistableUriPermission(tree, flags)
            storage.select(tree)
            refresh()
        } catch (error: Exception) {
            status.text = getString(R.string.content_error, error.localizedMessage)
        }
    }

    private fun refresh() {
        if (!storage.hasStorageAccess()) {
            status.setText(storageAccessMessage())
            choose.setText(if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R)
                R.string.grant_access else R.string.grant_legacy_access)
            play.isEnabled = false
            return
        }
        choose.setText(R.string.choose_content)
        try {
            val path = storage.localFilesystemPath()
            if (path == null) {
                status.setText(R.string.content_instructions)
                play.isEnabled = false
            } else {
                status.text = getString(R.string.content_ready_path, path)
                play.isEnabled = true
            }
        } catch (error: Exception) {
            status.text = getString(R.string.content_error, error.localizedMessage)
            play.isEnabled = false
        }
    }

    private fun startGameIfReady() {
        try {
            storage.localFilesystemPath() ?: error(getString(storageAccessMessage()))
            startActivity(Intent(this, MainActivity::class.java))
            finish()
        } catch (error: Exception) {
            status.text = getString(R.string.content_error, error.localizedMessage)
        }
    }

    private fun storageAccessMessage(): Int =
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) R.string.grant_storage_access
        else R.string.grant_legacy_storage_access

    private companion object {
        const val SELECT_CONTENT = 1
        const val REQUEST_STORAGE = 2
    }
}
