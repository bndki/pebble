package com.pebblenote

import android.app.NotificationManager
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.os.PowerManager
import android.provider.Settings
import android.widget.Button
import android.widget.EditText
import android.widget.TextView
import android.widget.Toast
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import androidx.documentfile.provider.DocumentFile

class MainActivity : AppCompatActivity() {

    private val prefsName = "PebbleNotePrefs"
    private val keyTreeUri = "saved_tree_uri"
    private val keyServerPort = "saved_server_port"

    private var currentTreeUri: Uri? = null

    private lateinit var tvFolderStatus: TextView
    private lateinit var tvServerStatus: TextView
    private lateinit var tvBatteryStatus: TextView
    private lateinit var tvNotificationStatus: TextView
    private lateinit var etServerPort: EditText
    private lateinit var btnSelectFolder: Button
    private lateinit var btnToggleServer: Button
    private lateinit var btnBatteryOptimization: Button
    private lateinit var btnNotificationPermission: Button

    private val openDocumentTreeLauncher = registerForActivityResult(
        ActivityResultContracts.OpenDocumentTree()
    ) { uri: Uri? ->
        if (uri != null) {
            val takeFlags = Intent.FLAG_GRANT_READ_URI_PERMISSION
            contentResolver.takePersistableUriPermission(uri, takeFlags)

            currentTreeUri = uri
            getSharedPreferences(prefsName, Context.MODE_PRIVATE)
                .edit()
                .putString(keyTreeUri, uri.toString())
                .apply()

            updateFolderUI()
            startServer()
        }
    }

    private val requestNotificationPermissionLauncher = registerForActivityResult(
        ActivityResultContracts.RequestPermission()
    ) { _ ->
        updateNotificationUI()
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        tvFolderStatus = findViewById(R.id.tvFolderStatus)
        tvServerStatus = findViewById(R.id.tvServerStatus)
        tvBatteryStatus = findViewById(R.id.tvBatteryStatus)
        tvNotificationStatus = findViewById(R.id.tvNotificationStatus)
        etServerPort = findViewById(R.id.etServerPort)
        btnSelectFolder = findViewById(R.id.btnSelectFolder)
        btnToggleServer = findViewById(R.id.btnToggleServer)
        btnBatteryOptimization = findViewById(R.id.btnBatteryOptimization)
        btnNotificationPermission = findViewById(R.id.btnNotificationPermission)

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            if (checkSelfPermission(android.Manifest.permission.POST_NOTIFICATIONS) != PackageManager.PERMISSION_GRANTED) {
                requestNotificationPermissionLauncher.launch(android.Manifest.permission.POST_NOTIFICATIONS)
            }
        }

        val savedPort = getSharedPreferences(prefsName, Context.MODE_PRIVATE)
            .getInt(keyServerPort, 8765)
        etServerPort.setText(savedPort.toString())

        val savedUriString = getSharedPreferences(prefsName, Context.MODE_PRIVATE)
            .getString(keyTreeUri, null)

        if (savedUriString != null) {
            try {
                currentTreeUri = Uri.parse(savedUriString)
            } catch (_: Exception) {
                currentTreeUri = null
            }
        }

        updateFolderUI()
        updateBatteryUI()
        updateNotificationUI()
        updateServerUI(NoteServerService.isRunning, NoteServerService.currentPort)

        btnSelectFolder.setOnClickListener {
            openDocumentTreeLauncher.launch(null)
        }

        btnToggleServer.setOnClickListener {
            if (NoteServerService.isRunning) {
                NoteServerService.stop(this)
            } else {
                startServer()
            }
        }

        btnBatteryOptimization.setOnClickListener {
            requestDisableBatteryOptimization()
        }

        btnNotificationPermission.setOnClickListener {
            requestNotificationPermission()
        }

        if (currentTreeUri != null && !NoteServerService.isRunning) {
            startServer()
        }
    }

    override fun onResume() {
        super.onResume()
        updateBatteryUI()
        updateNotificationUI()
        NoteServerService.onStateChangeListener = { running, port ->
            runOnUiThread {
                updateServerUI(running, port)
            }
        }
        updateServerUI(NoteServerService.isRunning, NoteServerService.currentPort)
    }

    override fun onPause() {
        super.onPause()
        NoteServerService.onStateChangeListener = null
        val raw = etServerPort.text.toString().trim().toIntOrNull()
        if (raw != null && raw in 1024..65535) {
            getSharedPreferences(prefsName, Context.MODE_PRIVATE)
                .edit()
                .putInt(keyServerPort, raw)
                .apply()
        }
    }

    private fun updateServerUI(running: Boolean, port: Int) {
        if (running) {
            tvServerStatus.text = "Server: Running on http://127.0.0.1:$port"
            btnToggleServer.text = "Stop Server"
            etServerPort.setText(port.toString())
            etServerPort.isEnabled = false
        } else {
            tvServerStatus.text = "Server: Stopped"
            btnToggleServer.text = "Start Server"
            etServerPort.isEnabled = true
        }
    }

    private fun getPort(): Int {
        val raw = etServerPort.text.toString().trim()
        val port = raw.toIntOrNull()
        return if (port != null && port in 1024..65535) {
            port
        } else {
            Toast.makeText(this, "Invalid port ($raw). Resetting to 8765.", Toast.LENGTH_SHORT).show()
            etServerPort.setText("8765")
            8765
        }
    }

    private fun isBatteryOptimizationIgnored(): Boolean {
        return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            val pm = getSystemService(Context.POWER_SERVICE) as PowerManager
            pm.isIgnoringBatteryOptimizations(packageName)
        } else {
            true
        }
    }

    private fun updateBatteryUI() {
        if (isBatteryOptimizationIgnored()) {
            tvBatteryStatus.text = "Battery: Unrestricted (Optimization Disabled)"
            tvBatteryStatus.setTextColor(0xFF2E7D32.toInt())
            btnBatteryOptimization.text = "Battery Settings (Unrestricted ✓)"
        } else {
            tvBatteryStatus.text = "Battery: Optimized (May be killed in background)"
            tvBatteryStatus.setTextColor(0xFFD32F2F.toInt())
            btnBatteryOptimization.text = "Disable Battery Optimization"
        }
    }

    private fun areNotificationsEnabled(): Boolean {
        return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            checkSelfPermission(android.Manifest.permission.POST_NOTIFICATIONS) == PackageManager.PERMISSION_GRANTED
        } else {
            val nm = getSystemService(Context.NOTIFICATION_SERVICE) as? NotificationManager
            nm?.areNotificationsEnabled() ?: true
        }
    }

    private fun updateNotificationUI() {
        if (areNotificationsEnabled()) {
            tvNotificationStatus.text = "Notification: Allowed (Persistent Notice Active)"
            tvNotificationStatus.setTextColor(0xFF2E7D32.toInt())
            btnNotificationPermission.text = "Notification Settings (Allowed ✓)"
        } else {
            tvNotificationStatus.text = "Notification: Blocked (Notice Won't Display)"
            tvNotificationStatus.setTextColor(0xFFD32F2F.toInt())
            btnNotificationPermission.text = "Enable Notification Permission"
        }
    }

    private fun requestNotificationPermission() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            if (checkSelfPermission(android.Manifest.permission.POST_NOTIFICATIONS) != PackageManager.PERMISSION_GRANTED) {
                requestNotificationPermissionLauncher.launch(android.Manifest.permission.POST_NOTIFICATIONS)
                return
            }
        }
        try {
            val intent = Intent(Settings.ACTION_APP_NOTIFICATION_SETTINGS).apply {
                putExtra(Settings.EXTRA_APP_PACKAGE, packageName)
            }
            startActivity(intent)
        } catch (_: Exception) {
            try {
                val appSettingsIntent = Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS).apply {
                    data = Uri.parse("package:$packageName")
                }
                startActivity(appSettingsIntent)
            } catch (_: Exception) {
                Toast.makeText(this, "Unable to open notification settings", Toast.LENGTH_SHORT).show()
            }
        }
    }

    private fun requestDisableBatteryOptimization() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            try {
                val intent = Intent(Settings.ACTION_REQUEST_IGNORE_BATTERY_OPTIMIZATIONS).apply {
                    data = Uri.parse("package:$packageName")
                }
                startActivity(intent)
            } catch (_: Exception) {
                try {
                    val fallbackIntent = Intent(Settings.ACTION_IGNORE_BATTERY_OPTIMIZATION_SETTINGS)
                    startActivity(fallbackIntent)
                } catch (_: Exception) {
                    val appSettingsIntent = Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS).apply {
                        data = Uri.parse("package:$packageName")
                    }
                    startActivity(appSettingsIntent)
                }
            }
        } else {
            Toast.makeText(this, "Battery optimization not applicable on this Android version", Toast.LENGTH_SHORT).show()
        }
    }

    private fun updateFolderUI() {
        if (currentTreeUri != null) {
            val doc = DocumentFile.fromTreeUri(this, currentTreeUri!!)
            val folderName = doc?.name ?: currentTreeUri!!.lastPathSegment ?: "Selected Folder"
            tvFolderStatus.text = "Root Folder: $folderName"
        } else {
            tvFolderStatus.text = "Root Folder: (None selected - please choose a folder)"
        }
    }

    private fun startServer() {
        if (currentTreeUri == null) {
            Toast.makeText(this, "Please select a notes folder first", Toast.LENGTH_SHORT).show()
            return
        }

        val port = getPort()
        getSharedPreferences(prefsName, Context.MODE_PRIVATE)
            .edit()
            .putInt(keyServerPort, port)
            .apply()

        NoteServerService.start(this, port, currentTreeUri!!)
        updateServerUI(true, port)
    }

    override fun onDestroy() {
        super.onDestroy()
    }
}
