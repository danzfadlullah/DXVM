package dev.zenithblue.panvklauncher

import android.content.Context
import android.content.Intent
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.text.selection.SelectionContainer
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.launch
import java.io.File

/**
 * The one button.
 *
 * Press Start: the app proves DXVM is the Direct3D implementation in play, identifies the driver
 * from the driver itself, runs both halves against the phone's own Vulkan driver, and leaves one
 * plain-text report to share. Nothing here selects a driver — that is the entire point of the
 * app, so the configuration is the phone's, and PanVK exists only as an explicit comparison
 * (see [mergeComparison]).
 */
@Composable
fun TestScreen(onOpenComponents: () -> Unit) {
    val ctx = LocalContext.current
    val scope = rememberCoroutineScope()
    var running by remember { mutableStateOf(false) }
    var progress by remember { mutableStateOf(listOf<String>()) }
    var report by remember { mutableStateOf<String?>(null) }

    fun addProgress(line: String) {
        progress = progress + line
    }

    Column(
        modifier = Modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp)
    ) {
        Text("DXVM TestPack", style = MaterialTheme.typography.headlineSmall)
        Text(
            "Runs DXVM's Direct3D suite inside the Wine container and a Vulkan capability suite " +
                "against this phone's own driver, then writes one report you can share.",
            style = MaterialTheme.typography.bodyMedium
        )

        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            Button(
                onClick = {
                    running = true
                    report = null
                    progress = emptyList()
                    scope.launch {
                        try {
                            val text = runFullPass(ctx, ::addProgress)
                            report = text
                            runCatching {
                                File(ctx.filesDir, "dxvm-testpack-report.txt").writeText(text)
                            }
                        } catch (t: Throwable) {
                            addProgress("Test pass failed: ${t.message}")
                        } finally {
                            running = false
                        }
                    }
                },
                enabled = !running,
                modifier = Modifier.weight(1f).heightIn(min = 56.dp)
            ) { Text(if (running) "Running…" else "Start") }

            OutlinedButton(
                onClick = onOpenComponents,
                enabled = !running,
                modifier = Modifier.heightIn(min = 56.dp)
            ) { Text("Components") }
        }

        if (running) {
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                CircularProgressIndicator()
                Text("Working — the container takes a minute to start the first time.")
            }
        }

        if (progress.isNotEmpty()) {
            Card(Modifier.fillMaxWidth()) {
                Column(Modifier.padding(12.dp), verticalArrangement = Arrangement.spacedBy(2.dp)) {
                    Text("Progress", style = MaterialTheme.typography.titleSmall)
                    for (line in progress) {
                        Text(line, style = MaterialTheme.typography.bodySmall)
                    }
                }
            }
        }

        report?.let { text ->
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                Button(
                    onClick = { shareReport(ctx, text) },
                    modifier = Modifier.weight(1f).heightIn(min = 56.dp)
                ) { Text("Share report") }
            }
            Card(Modifier.fillMaxWidth()) {
                Column(Modifier.padding(12.dp)) {
                    Text("Report", style = MaterialTheme.typography.titleSmall)
                    SelectionContainer {
                        Text(
                            text,
                            style = MaterialTheme.typography.bodySmall,
                            fontFamily = FontFamily.Monospace
                        )
                    }
                }
            }
        }
    }
}

/** Plain-text share: the report is paste-ready, so the text itself is the payload. */
private fun shareReport(ctx: Context, text: String) {
    val send = Intent(Intent.ACTION_SEND).apply {
        type = "text/plain"
        putExtra(Intent.EXTRA_SUBJECT, "DXVM TestPack report")
        putExtra(Intent.EXTRA_TEXT, text)
    }
    ctx.startActivity(Intent.createChooser(send, "Share report"))
}