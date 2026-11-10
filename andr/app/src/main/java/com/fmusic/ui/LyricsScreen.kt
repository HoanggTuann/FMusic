package com.fmusic.ui

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.fmusic.PlayerViewModel

@Composable
fun LyricsScreen(vm: PlayerViewModel) {
    val lyrics by vm.lyrics.collectAsState()
    val currentLyric by vm.currentLyric.collectAsState()
    val listState = rememberLazyListState()

    // Auto-scroll to the currently active lyric line
    LaunchedEffect(currentLyric) {
        if (currentLyric >= 0) {
            listState.animateScrollToItem(currentLyric, scrollOffset = -200)
        }
    }

    if (lyrics.isEmpty()) {
        Box(
            modifier = Modifier.fillMaxSize(),
            contentAlignment = Alignment.Center
        ) {
            Text("Không có lời bài hát", style = MaterialTheme.typography.titleMedium)
        }
    } else {
        LazyColumn(
            state = listState,
            modifier = Modifier
                .fillMaxSize()
                .padding(horizontal = 24.dp, vertical = 16.dp),
            verticalArrangement = Arrangement.spacedBy(20.dp)
        ) {
            items(lyrics.size) { i ->
                val line = lyrics[i]
                Text(
                    // Show music note for empty lines (instrumental breaks)
                    text = line.text.ifEmpty { "♪" },
                    style = MaterialTheme.typography.headlineSmall,
                    color = if (i == currentLyric)
                        MaterialTheme.colorScheme.primary
                    else
                        MaterialTheme.colorScheme.onSurface.copy(alpha = 0.4f)
                )
            }
        }
    }
}
