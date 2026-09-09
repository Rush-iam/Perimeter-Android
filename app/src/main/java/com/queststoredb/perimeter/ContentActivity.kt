package com.queststoredb.perimeter

import android.Manifest
import android.app.Activity
import android.app.AlertDialog
import android.content.Intent
import android.content.pm.PackageManager
import android.graphics.Color
import android.net.Uri
import android.os.Bundle
import android.os.Build
import android.provider.DocumentsContract
import android.provider.Settings
import android.widget.Button
import android.widget.CheckBox
import android.widget.LinearLayout
import android.widget.SeekBar
import android.widget.TextView
import android.widget.ScrollView
import android.view.View

/** Selects content, then obtains the raw path required by the native engine. */
class ContentActivity : Activity() {
    private lateinit var storage: GameContentStorage
    private lateinit var status: TextView
    private lateinit var choose: Button
    private lateinit var play: Button
    private lateinit var buildNote: TextView

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
            setOnClickListener { showLaunchOptionsEditor() }
        })
        addResolutionScaleControl(layout)
        addFrameRateLimitControl(layout)
        layout.addView(play)
        buildNote = TextView(this).apply {
            textSize = 12f
            setTextColor(Color.GRAY)
        }
        layout.addView(buildNote)
        refreshBuildNote()
        setContentView(ScrollView(this).apply { addView(layout) })
        refresh()
        if (savedInstanceState == null && Build.VERSION.SDK_INT == Build.VERSION_CODES.Q &&
            !storage.hasStorageAccess()) {
            requestLegacyStorageAccess()
        }
    }

    override fun onResume() {
        super.onResume()
        if (::storage.isInitialized) {
            refresh()
            refreshBuildNote()
        }
    }

    private fun showLaunchOptionsEditor() {
        val options = GameLaunchOptions(this)
        GameLaunchOptionsEditor(this, options.load()) {
            options.save(it)
            refreshBuildNote()
        }.show()
    }

    private fun addResolutionScaleControl(layout: LinearLayout) {
        val label = TextView(this)
        val slider = SeekBar(this).apply {
            max = ResolutionScale.percentages.lastIndex
            progress = ResolutionScale.percentages.indexOf(ResolutionScale.load(this@ContentActivity))
            contentDescription = getString(R.string.render_resolution)
        }

        fun updateLabel(progress: Int) {
            val percent = ResolutionScale.percentages[progress]
            val size = ResolutionScale.renderSize(this@ContentActivity, percent)
            label.text = getString(R.string.render_resolution_value, size.first, size.second)
        }

        updateLabel(slider.progress)
        slider.setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
            override fun onProgressChanged(seekBar: SeekBar, progress: Int, fromUser: Boolean) {
                updateLabel(progress)
                if (fromUser) ResolutionScale.save(this@ContentActivity,
                    ResolutionScale.percentages[progress])
            }

            override fun onStartTrackingTouch(seekBar: SeekBar) = Unit
            override fun onStopTrackingTouch(seekBar: SeekBar) = Unit
        })
        layout.addView(label)
        layout.addView(slider)
    }

    private fun addFrameRateLimitControl(layout: LinearLayout) {
        layout.addView(CheckBox(this).apply {
            text = getString(
                R.string.limit_frame_rate_to,
                FrameRateLimit.displayedFramesPerSecond(this@ContentActivity)
            )
            isChecked = FrameRateLimit.load(this@ContentActivity)
            setOnCheckedChangeListener { _, checked ->
                FrameRateLimit.save(this@ContentActivity, checked)
            }
        })
    }

    private fun refreshBuildNote() {
        buildNote.text = getString(R.string.build_note, BuildConfig.VERSION_NAME, BuildConfig.FLAVOR) +
            if (GameLaunchOptions(this).renderer() == "sokol") {
                "\nwarning: legacy Sokol GLES3 renderer is selected, it is recommended to switch to DXVK"
            } else {
                ""
            }
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
            if (requiresVulkan13ForDxvk() && !hasVulkan13Support()) {
                showDxvk2UnsupportedDialog()
                return
            }
            startGame()
        } catch (error: Exception) {
            status.text = getString(R.string.content_error, error.localizedMessage)
        }
    }

    private fun requiresVulkan13ForDxvk(): Boolean =
        BuildConfig.DXVK_VERSION == "2" && GameLaunchOptions(this).renderer() == "d3d9"

    private fun hasVulkan13Support(): Boolean = packageManager.hasSystemFeature(
        PackageManager.FEATURE_VULKAN_HARDWARE_VERSION,
        VULKAN_1_3_VERSION
    )

    private fun showDxvk2UnsupportedDialog() {
        AlertDialog.Builder(this)
            .setTitle(R.string.dxvk2_vulkan_unsupported_title)
            .setMessage(R.string.dxvk2_vulkan_unsupported_message)
            .setPositiveButton(R.string.use_sokol_renderer) { _, _ ->
                GameLaunchOptions(this).selectRenderer("sokol")
                startGame()
            }
            .setNegativeButton(android.R.string.cancel, null)
            .show()
    }

    private fun startGame() {
        try {
            // Replace the setup task with the game. This makes MainActivity the task root,
            // removes this Activity from the back stack, and still lets Meta Quest assign
            // fresh panel bounds for the landscape game Activity.
            startActivity(Intent(this, MainActivity::class.java).apply {
                addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK)
            })
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
        // VK_MAKE_API_VERSION(0, 1, 3, 0). PackageManager accepts Vulkan's encoded version.
        const val VULKAN_1_3_VERSION = 0x00403000
    }
}
