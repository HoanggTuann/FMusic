package com.fmusic.ui

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.horizontalScroll
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import com.fmusic.PlayerViewModel

val EQ_FREQ_LABELS = listOf("31", "62", "125", "250", "500", "1k", "2k", "4k", "8k", "16k")
val EQ_PRESETS = listOf(
    "Phẳng", "Nhạc pop", "Rock", "Jazz", "Classical",
    "Bass tăng", "Treble tăng", "Giọng nói", "Electronic"
)

@Composable
fun EqualizerScreen(vm: PlayerViewModel) {
    val eqEnabled by vm.eqEnabled.collectAsState()
    val eqGains by vm.eqGains.collectAsState()
    val context = LocalContext.current
    var presetName by remember { mutableStateOf("") }
    var showSaveDialog by remember { mutableStateOf(false) }

    Column(
        modifier = Modifier.fillMaxSize().padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp)
    ) {
        // Header: title + on/off toggle
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            Text("Equalizer", style = MaterialTheme.typography.titleLarge)
            Switch(checked = eqEnabled, onCheckedChange = { vm.setEqEnabled(it) })
        }

        // Preset chip row (horizontally scrollable for small screens)
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .horizontalScroll(rememberScrollState()),
            horizontalArrangement = Arrangement.spacedBy(8.dp)
        ) {
            EQ_PRESETS.forEachIndexed { i, name ->
                FilterChip(
                    selected = false,
                    onClick = { vm.applyPreset(i) },
                    label = { Text(name, style = MaterialTheme.typography.labelSmall) }
                )
            }
        }

        // 10-band vertical sliders arranged side by side
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .height(220.dp),
            horizontalArrangement = Arrangement.SpaceEvenly
        ) {
            eqGains.forEachIndexed { band, gain ->
                Column(
                    horizontalAlignment = Alignment.CenterHorizontally,
                    modifier = Modifier.weight(1f)
                ) {
                    Text("+12", style = MaterialTheme.typography.labelSmall)
                    // Vertical slider: map [-12, +12] dB to [0, 1]
                    Slider(
                        value = (gain + 12f) / 24f,
                        onValueChange = { vm.setEqGain(band, it * 24f - 12f) },
                        modifier = Modifier.weight(1f)
                    )
                    Text("-12", style = MaterialTheme.typography.labelSmall)
                    Spacer(modifier = Modifier.height(2.dp))
                    Text(EQ_FREQ_LABELS[band], style = MaterialTheme.typography.labelSmall)
                    // Current dB value
                    Text(
                        "${gain.toInt()}",
                        style = MaterialTheme.typography.labelSmall,
                        color = MaterialTheme.colorScheme.primary
                    )
                }
            }
        }

        // Save preset button
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            OutlinedButton(onClick = { showSaveDialog = true }) {
                Text("💾 Lưu preset")
            }
        }

        // Save EQ preset dialog
        if (showSaveDialog) {
            AlertDialog(
                onDismissRequest = { showSaveDialog = false },
                title = { Text("Lưu preset EQ") },
                text = {
                    OutlinedTextField(
                        value = presetName,
                        onValueChange = { presetName = it },
                        label = { Text("Tên preset") },
                        singleLine = true
                    )
                },
                confirmButton = {
                    TextButton(onClick = {
                        if (presetName.isNotBlank()) {
                            vm.saveEqPreset(presetName.trim(), context)
                        }
                        showSaveDialog = false
                    }) { Text("Lưu") }
                },
                dismissButton = {
                    TextButton(onClick = { showSaveDialog = false }) { Text("Huỷ") }
                }
            )
        }
    }
}
