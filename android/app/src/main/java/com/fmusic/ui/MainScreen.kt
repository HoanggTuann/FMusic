package com.fmusic.ui

import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import com.fmusic.PlayerViewModel

enum class Screen { NOW_PLAYING, LYRICS, QUEUE, EQUALIZER, SETTINGS }

@Composable
fun MainScreen(vm: PlayerViewModel) {
    var screen by remember { mutableStateOf(Screen.NOW_PLAYING) }

    Scaffold(
        bottomBar = {
            NavigationBar {
                NavigationBarItem(
                    selected = screen == Screen.NOW_PLAYING,
                    onClick = { screen = Screen.NOW_PLAYING },
                    icon = { Text("▶") },
                    label = { Text("Đang phát") }
                )
                NavigationBarItem(
                    selected = screen == Screen.LYRICS,
                    onClick = { screen = Screen.LYRICS },
                    icon = { Text("♩") },
                    label = { Text("Lời") }
                )
                NavigationBarItem(
                    selected = screen == Screen.QUEUE,
                    onClick = { screen = Screen.QUEUE },
                    icon = { Text("☰") },
                    label = { Text("Danh sách") }
                )
                NavigationBarItem(
                    selected = screen == Screen.EQUALIZER,
                    onClick = { screen = Screen.EQUALIZER },
                    icon = { Text("⊻") },
                    label = { Text("EQ") }
                )
                NavigationBarItem(
                    selected = screen == Screen.SETTINGS,
                    onClick = { screen = Screen.SETTINGS },
                    icon = { Text("⚙") },
                    label = { Text("Cài đặt") }
                )
            }
        }
    ) { padding ->
        Box(modifier = Modifier.fillMaxSize().padding(padding)) {
            when (screen) {
                Screen.NOW_PLAYING -> NowPlayingScreen(
                    vm,
                    onShowQueue = { screen = Screen.QUEUE },
                    onShowLyrics = { screen = Screen.LYRICS }
                )
                Screen.LYRICS    -> LyricsScreen(vm)
                Screen.QUEUE     -> QueueScreen(vm)
                Screen.EQUALIZER -> EqualizerScreen(vm)
                Screen.SETTINGS  -> SettingsScreen(vm)
            }
        }
    }
}
