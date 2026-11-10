package com.fmusic.ui

import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.itemsIndexed
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import com.fmusic.PlayerViewModel

@Composable
fun QueueScreen(vm: PlayerViewModel) {
    val tracks by vm.tracks.collectAsState()
    val currentIndex by vm.currentIndex.collectAsState()
    val context = LocalContext.current

    // Audio file picker — multiple selection
    val picker = rememberLauncherForActivityResult(
        ActivityResultContracts.GetMultipleContents()
    ) { uris ->
        val paths = uris.mapNotNull { uri ->
            // Convert content URI to real path via MediaStore query
            context.contentResolver.query(uri, arrayOf("_data"), null, null, null)?.use { cursor ->
                if (cursor.moveToFirst()) cursor.getString(0) else null
            } ?: uri.path
        }
        vm.addFiles(paths)
    }

    Column(modifier = Modifier.fillMaxSize().padding(16.dp)) {
        // Header row with actions
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween
        ) {
            Text("Danh sách phát", style = MaterialTheme.typography.titleLarge)
            Row {
                TextButton(onClick = { vm.openFiles(emptyList()) }) { Text("Xoá hết") }
                TextButton(onClick = { picker.launch("audio/*") }) { Text("Thêm") }
            }
        }

        // Track list
        LazyColumn {
            itemsIndexed(tracks) { i, track ->
                ListItem(
                    headlineContent = { Text(track.title) },
                    supportingContent = { Text(track.artist) },
                    leadingContent = {
                        // Show music note for currently playing track, index otherwise
                        Text(
                            if (i == currentIndex) "♪" else "${i + 1}",
                            style = MaterialTheme.typography.bodyMedium
                        )
                    },
                    modifier = Modifier.clickable { vm.loadTrack(i, autoplay = true) }
                )
                HorizontalDivider()
            }
        }
    }
}
