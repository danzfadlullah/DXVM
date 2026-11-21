package dev.zenithblue.panvklauncher

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp

/**
 * The test shell's single entry point: one button that runs a full pass against the phone's own
 * Vulkan driver and produces one shareable report.
 *
 * Wired in the plan's Task 9; this file exists from Task 3 so the app opens on it rather than on
 * the deleted game library.
 */
@Composable
fun TestScreen(
    onOpenComponents: () -> Unit
) {
    Column(
        modifier = Modifier.fillMaxSize().verticalScroll(rememberScrollState())
            .padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp)
    ) {
        Text("DXVM TestPack", style = MaterialTheme.typography.headlineSmall)
        Text(
            "Runs the Direct3D suite inside the Wine container and a Vulkan capability suite " +
                "against the phone's own driver, then writes one report you can share.",
            style = MaterialTheme.typography.bodyMedium
        )
        Card(Modifier.fillMaxWidth()) {
            Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                Text("Status", style = MaterialTheme.typography.titleMedium)
                Text("Test flow is wired in Task 9.", style = MaterialTheme.typography.bodyMedium)
                Button(onClick = onOpenComponents) { Text("Open Components") }
            }
        }
    }
}
