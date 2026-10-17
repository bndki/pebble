package com.pebblenote

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.Context
import android.content.Intent
import android.content.pm.ServiceInfo
import android.net.Uri
import android.os.Build
import android.os.IBinder
import androidx.core.app.NotificationCompat

class NoteServerService : Service() {

    private var httpServer: NoteHttpServer? = null
    private var treeUri: Uri? = null

    companion object {
        const val CHANNEL_ID = "pebblenote_server_channel"
        const val NOTIFICATION_ID = 1001

        const val ACTION_START = "com.pebblenote.action.START"
        const val ACTION_STOP = "com.pebblenote.action.STOP"

        const val EXTRA_PORT = "extra_port"
        const val EXTRA_TREE_URI = "extra_tree_uri"

        var isRunning: Boolean = false
            private set

        var currentPort: Int = 8765
            private set

        var onStateChangeListener: ((Boolean, Int) -> Unit)? = null

        fun start(context: Context, port: Int, treeUri: Uri) {
            val intent = Intent(context, NoteServerService::class.java).apply {
                action = ACTION_START
                putExtra(EXTRA_PORT, port)
                putExtra(EXTRA_TREE_URI, treeUri.toString())
            }
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                context.startForegroundService(intent)
            } else {
                context.startService(intent)
            }
        }

        fun stop(context: Context) {
            val intent = Intent(context, NoteServerService::class.java).apply {
                action = ACTION_STOP
            }
            context.startService(intent)
        }
    }

    override fun onBind(intent: Intent?): IBinder? = null

    override fun onCreate() {
        super.onCreate()
        createNotificationChannel()
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        val action = intent?.action

        if (action == ACTION_STOP) {
            stopServer()
            stopSelf()
            return START_NOT_STICKY
        }

        if (action == ACTION_START) {
            val port = intent.getIntExtra(EXTRA_PORT, 8765)
            val uriStr = intent.getStringExtra(EXTRA_TREE_URI)
            if (uriStr != null) {
                treeUri = Uri.parse(uriStr)
            }

            startServer(port)
        }

        return START_STICKY
    }

    private fun startServer(port: Int) {
        if (treeUri == null) {
            stopSelf()
            return
        }

        if (httpServer != null && httpServer!!.isAlive && currentPort == port) {
            return
        }

        httpServer?.let {
            if (it.isAlive) it.stop()
        }

        try {
            httpServer = NoteHttpServer(this, { treeUri }, port)
            httpServer!!.start()
            currentPort = port
            isRunning = true

            val notification = buildNotification(port)
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
                startForeground(NOTIFICATION_ID, notification, ServiceInfo.FOREGROUND_SERVICE_TYPE_DATA_SYNC)
            } else {
                startForeground(NOTIFICATION_ID, notification)
            }

            onStateChangeListener?.invoke(true, port)
        } catch (e: Exception) {
            isRunning = false
            onStateChangeListener?.invoke(false, port)
            stopSelf()
        }
    }

    private fun stopServer() {
        httpServer?.let {
            if (it.isAlive) it.stop()
        }
        httpServer = null
        isRunning = false
        onStateChangeListener?.invoke(false, currentPort)

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            stopForeground(STOP_FOREGROUND_REMOVE)
        } else {
            @Suppress("DEPRECATION")
            stopForeground(true)
        }
    }

    private fun buildNotification(port: Int): Notification {
        val contentIntent = Intent(this, MainActivity::class.java).let { notificationIntent ->
            PendingIntent.getActivity(
                this, 0, notificationIntent,
                PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
            )
        }

        val stopIntent = Intent(this, NoteServerService::class.java).apply {
            action = ACTION_STOP
        }
        val stopPendingIntent = PendingIntent.getService(
            this, 1, stopIntent,
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
        )

        return NotificationCompat.Builder(this, CHANNEL_ID)
            .setContentTitle("PebbleNote Server")
            .setContentText("Running on http://127.0.0.1:$port")
            .setSmallIcon(R.mipmap.ic_launcher)
            .setContentIntent(contentIntent)
            .setOngoing(true)
            .setPriority(NotificationCompat.PRIORITY_LOW)
            .addAction(android.R.drawable.ic_menu_close_clear_cancel, "Stop", stopPendingIntent)
            .build()
    }

    private fun createNotificationChannel() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val channel = NotificationChannel(
                CHANNEL_ID,
                "PebbleNote Server Service",
                NotificationManager.IMPORTANCE_LOW
            ).apply {
                description = "Maintains PebbleNote background HTTP server"
                setShowBadge(false)
            }
            val manager = getSystemService(NotificationManager::class.java)
            manager?.createNotificationChannel(channel)
        }
    }

    override fun onDestroy() {
        stopServer()
        super.onDestroy()
    }
}
