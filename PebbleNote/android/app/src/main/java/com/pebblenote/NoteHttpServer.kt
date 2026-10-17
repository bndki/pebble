package com.pebblenote

import android.content.Context
import android.net.Uri
import androidx.documentfile.provider.DocumentFile
import fi.iki.elonen.NanoHTTPD
import org.json.JSONArray
import org.json.JSONObject
import java.io.InputStream
import java.net.URLDecoder

class NoteHttpServer(
    private val context: Context,
    private val treeUriProvider: () -> Uri?,
    port: Int = 8765
) : NanoHTTPD("127.0.0.1", port) {

    override fun serve(session: IHTTPSession): Response {
        val uri = session.uri
        val params = session.parms

        if (session.method == Method.OPTIONS) {
            val response = newFixedLengthResponse(Response.Status.OK, MIME_PLAINTEXT, "")
            addCorsHeaders(response)
            return response
        }

        val treeUri = treeUriProvider() ?: return createErrorResponse(
            Response.Status.BAD_REQUEST,
            "No root folder selected. Open PebbleNote Android app and choose a directory."
        )

        val rootDoc = DocumentFile.fromTreeUri(context, treeUri) ?: return createErrorResponse(
            Response.Status.INTERNAL_ERROR,
            "Failed to access document tree storage."
        )

        val rawPath = params["path"].orEmpty()
        val cleanPath = try {
            URLDecoder.decode(rawPath, "UTF-8").trim('/')
        } catch (_: Exception) {
            rawPath.trim('/')
        }

        return when (uri) {
            "/tree" -> handleTreeRequest(rootDoc, cleanPath)
            "/file" -> handleFileRequest(rootDoc, cleanPath)
            else -> createErrorResponse(Response.Status.NOT_FOUND, "Unknown endpoint: $uri")
        }
    }

    private fun handleTreeRequest(rootDoc: DocumentFile, relativePath: String): Response {
        val targetDir = resolveDirectory(rootDoc, relativePath) ?: return createErrorResponse(
            Response.Status.NOT_FOUND,
            "Directory not found: $relativePath"
        )

        val files = targetDir.listFiles()
        val jsonArray = JSONArray()
        val seenNames = mutableSetOf<String>()

        files.forEach { file ->
            val name = file.name ?: return@forEach
            if (!name.startsWith(".") && seenNames.add(name.lowercase())) {
                if (file.isDirectory) {
                    val obj = JSONObject().apply {
                        put("name", name)
                        put("isDir", true)
                        put("lastModified", file.lastModified())
                    }
                    jsonArray.put(obj)
                } else if (name.endsWith(".txt", ignoreCase = true) || name.endsWith(".md", ignoreCase = true)) {
                    val obj = JSONObject().apply {
                        put("name", name)
                        put("isDir", false)
                        put("lastModified", file.lastModified())
                    }
                    jsonArray.put(obj)
                }
            }
        }

        val response = newFixedLengthResponse(
            Response.Status.OK,
            "application/json; charset=utf-8",
            jsonArray.toString()
        )
        addCorsHeaders(response)
        return response
    }

    private fun handleFileRequest(rootDoc: DocumentFile, relativePath: String): Response {
        if (relativePath.isBlank()) {
            return createErrorResponse(Response.Status.BAD_REQUEST, "Missing path parameter")
        }

        val targetFile = resolveFile(rootDoc, relativePath) ?: return createErrorResponse(
            Response.Status.NOT_FOUND,
            "File not found: $relativePath"
        )

        return try {
            val inputStream: InputStream = context.contentResolver.openInputStream(targetFile.uri)
                ?: return createErrorResponse(Response.Status.INTERNAL_ERROR, "Cannot open file stream")

            val content = inputStream.bufferedReader(Charsets.UTF_8).use { it.readText() }
            val response = newFixedLengthResponse(
                Response.Status.OK,
                "text/plain; charset=utf-8",
                content
            )
            addCorsHeaders(response)
            response
        } catch (e: Exception) {
            createErrorResponse(Response.Status.INTERNAL_ERROR, "Error reading file: ${e.message}")
        }
    }

    private fun findChild(dir: DocumentFile, name: String): DocumentFile? {
        val direct = dir.findFile(name)
        if (direct != null && direct.exists()) return direct
        return dir.listFiles().firstOrNull { it.name.equals(name, ignoreCase = true) }
    }

    private fun resolveDirectory(root: DocumentFile, relativePath: String): DocumentFile? {
        if (relativePath.isEmpty()) return root

        var current: DocumentFile = root
        val segments = relativePath.split('/')
        for (segment in segments) {
            if (segment.isEmpty()) continue
            current = findChild(current, segment) ?: return null
            if (!current.isDirectory) return null
        }
        return current
    }

    private fun resolveFile(root: DocumentFile, relativePath: String): DocumentFile? {
        val lastSlash = relativePath.lastIndexOf('/')
        return if (lastSlash == -1) {
            findChild(root, relativePath)
        } else {
            val dirPath = relativePath.substring(0, lastSlash)
            val fileName = relativePath.substring(lastSlash + 1)
            val dir = resolveDirectory(root, dirPath) ?: return null
            findChild(dir, fileName)
        }
    }

    private fun createErrorResponse(status: Response.Status, message: String): Response {
        val json = JSONObject().apply { put("error", message) }
        val response = newFixedLengthResponse(status, "application/json; charset=utf-8", json.toString())
        addCorsHeaders(response)
        return response
    }

    private fun addCorsHeaders(response: Response) {
        response.addHeader("Access-Control-Allow-Origin", "*")
        response.addHeader("Access-Control-Allow-Methods", "GET, OPTIONS")
        response.addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization")
    }
}
