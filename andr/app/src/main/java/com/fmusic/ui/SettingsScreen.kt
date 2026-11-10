package com.fmusic.ui

import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.fmusic.PlayerViewModel

@Composable
fun SettingsScreen(vm: PlayerViewModel) {
    val sampleRate by vm.sampleRate.collectAsState()

    Column(
        modifier = Modifier.fillMaxSize().padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(16.dp)
    ) {
        Text("Cài đặt", style = MaterialTheme.typography.titleLarge)

        // Audio settings card
        Card(modifier = Modifier.fillMaxWidth()) {
            Column(
                modifier = Modifier.fillMaxWidth().padding(16.dp),
                verticalArrangement = Arrangement.spacedBy(8.dp)
            ) {
                Text("Âm thanh", style = MaterialTheme.typography.titleMedium)
                Text("Tần số lấy mẫu (Sample Rate)", style = MaterialTheme.typography.bodyMedium)
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    listOf(44100, 48000, 96000).forEach { sr ->
                        FilterChip(
                            selected = sampleRate == sr,
                            onClick = { vm.setSampleRate(sr) },
                            label = { Text("${sr / 1000}kHz") }
                        )
                    }
                }
                Spacer(modifier = Modifier.height(4.dp))
                Text(
                    "Thay đổi sample rate sẽ khởi động lại AudioTrack.",
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.6f)
                )
            }
        }

        // About card
        Card(modifier = Modifier.fillMaxWidth()) {
            Column(
                modifier = Modifier.fillMaxWidth().padding(16.dp),
                verticalArrangement = Arrangement.spacedBy(4.dp)
            ) {
                Text("Phiên bản", style = MaterialTheme.typography.titleMedium)
                Text("FMusic v1.1 — Android", style = MaterialTheme.typography.bodySmall)
                Text(
                    "Nhân xử lý âm thanh: C++ (FFmpeg + custom EQ/Spectrum)",
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.6f)
                )
            }
        }
    }
}
