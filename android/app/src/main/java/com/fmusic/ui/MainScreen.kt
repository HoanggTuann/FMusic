package com.fmusic.ui

import android.graphics.Bitmap
import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.Crossfade
import androidx.compose.animation.animateColorAsState
import androidx.compose.animation.core.tween
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.slideInVertically
import androidx.compose.animation.slideOutVertically
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.navigationBarsPadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.shadow
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.fmusic.PlayerViewModel

enum class Screen(val title: String) {
    NOW_PLAYING("Đang phát"),
    LYRICS("Lời"),
    QUEUE("Danh sách"),
    EQUALIZER("EQ"),
    SETTINGS("Cài đặt")
}

@Composable
fun MainScreen(vm: PlayerViewModel) {
    var screen by remember { mutableStateOf(Screen.NOW_PLAYING) }
    val backdrop by vm.backdropBitmap.collectAsState()
    val coverToken by vm.coverToken.collectAsState()
    val ambientEnabled by vm.ambientBackground.collectAsState()
    val tracks by vm.tracks.collectAsState()
    val cover by vm.coverBitmap.collectAsState()
    val title by vm.title.collectAsState()
    val artist by vm.artist.collectAsState()
    val isPlaying by vm.isPlaying.collectAsState()

    Box(modifier = Modifier.fillMaxSize().background(Color.Black)) {
        // Nen ambient xoay + crossfade phu toan man hinh
        AmbientBackground(
            backdrop = backdrop,
            token = coverToken,
            enabled = ambientEnabled,
            modifier = Modifier.fillMaxSize()
        )

        // Noi dung chinh voi fade transition giong iOS
        Crossfade(
            targetState = screen,
            animationSpec = tween(durationMillis = 240),
            modifier = Modifier
                .fillMaxSize()
                .statusBarsPadding()
                .padding(top = 6.dp),
            label = "tabCrossfade"
        ) { current ->
            when (current) {
                Screen.NOW_PLAYING -> Box(
                    modifier = Modifier
                        .fillMaxSize()
                        .navigationBarsPadding()
                        .padding(bottom = 86.dp)
                ) {
                    NowPlayingScreen(vm = vm)
                }
                Screen.LYRICS -> LyricsScreen(vm = vm)
                Screen.QUEUE -> QueueScreen(vm = vm)
                Screen.EQUALIZER -> EqualizerScreen(vm = vm)
                Screen.SETTINGS -> SettingsScreen(vm = vm)
            }
        }

        // Lop phu mo nhe phia duoi de noi bat MiniPlayer & GlassTabBar khi cuon noi dung
        Box(
            modifier = Modifier
                .align(Alignment.BottomCenter)
                .fillMaxWidth()
                .background(
                    Brush.verticalGradient(
                        listOf(
                            Color.Transparent,
                            Color.Black.copy(alpha = 0.45f)
                        )
                    )
                )
                .navigationBarsPadding()
                .padding(top = 24.dp, bottom = 10.dp)
        ) {
            Column(
                modifier = Modifier.fillMaxWidth(),
                verticalArrangement = Arrangement.spacedBy(8.dp)
            ) {
                AnimatedVisibility(
                    visible = screen != Screen.NOW_PLAYING && tracks.isNotEmpty(),
                    enter = fadeIn(tween(200)) + slideInVertically(tween(220)) { it / 2 },
                    exit = fadeOut(tween(180)) + slideOutVertically(tween(180)) { it / 2 }
                ) {
                    MiniPlayerBar(
                        cover = cover,
                        title = title,
                        artist = artist,
                        isPlaying = isPlaying,
                        onTogglePlay = { vm.togglePlay() },
                        onNext = { vm.next() },
                        onTap = { screen = Screen.NOW_PLAYING }
                    )
                }

                GlassTabBar(
                    selected = screen,
                    onSelect = { screen = it }
                )
            }
        }
    }
}

@Composable
private fun MiniPlayerBar(
    cover: Bitmap?,
    title: String,
    artist: String,
    isPlaying: Boolean,
    onTogglePlay: () -> Unit,
    onNext: () -> Unit,
    onTap: () -> Unit
) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .padding(horizontal = 16.dp)
            .shadow(12.dp, CircleShape, ambientColor = Color.Black.copy(alpha = 0.5f))
            .iosGlass(CircleShape, fillAlpha = 0.68f, borderAlpha = 0.16f)
            .clickable(
                interactionSource = remember { MutableInteractionSource() },
                indication = null,
                onClick = onTap
            )
            .padding(horizontal = 12.dp, vertical = 8.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Box(
            modifier = Modifier
                .size(44.dp)
                .clip(RoundedCornerShape(9.dp))
                .background(Color.White.copy(alpha = 0.12f)),
            contentAlignment = Alignment.Center
        ) {
            if (cover != null) {
                val bmp = remember(cover) { cover.asImageBitmap() }
                Image(
                    bitmap = bmp,
                    contentDescription = null,
                    contentScale = ContentScale.Crop,
                    modifier = Modifier.fillMaxSize()
                )
            } else {
                MusicNoteIcon(tint = Color.White.copy(alpha = 0.7f), size = 20.dp)
            }
        }

        Spacer(modifier = Modifier.width(12.dp))

        Column(
            modifier = Modifier.weight(1f),
            verticalArrangement = Arrangement.spacedBy(1.dp)
        ) {
            Text(
                text = title,
                color = Color.White,
                fontSize = 15.sp,
                fontWeight = FontWeight.SemiBold,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis
            )
            Text(
                text = artist,
                color = Color.White.copy(alpha = 0.6f),
                fontSize = 12.sp,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis
            )
        }

        Spacer(modifier = Modifier.width(8.dp))

        Box(
            modifier = Modifier
                .size(36.dp)
                .clickable(
                    interactionSource = remember { MutableInteractionSource() },
                    indication = null,
                    onClick = onTogglePlay
                ),
            contentAlignment = Alignment.Center
        ) {
            if (isPlaying) {
                PauseIcon(tint = Color.White, size = 20.dp)
            } else {
                PlayIcon(tint = Color.White, size = 20.dp)
            }
        }

        Box(
            modifier = Modifier
                .size(36.dp)
                .clickable(
                    interactionSource = remember { MutableInteractionSource() },
                    indication = null,
                    onClick = onNext
                ),
            contentAlignment = Alignment.Center
        ) {
            ForwardIcon(tint = Color.White, size = 19.dp)
        }
    }
}

@Composable
private fun GlassTabBar(
    selected: Screen,
    onSelect: (Screen) -> Unit
) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .padding(horizontal = 16.dp)
            .shadow(14.dp, CircleShape, ambientColor = Color.Black.copy(alpha = 0.6f))
            .iosGlass(CircleShape, fillAlpha = 0.70f, borderAlpha = 0.16f)
            .padding(6.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Screen.values().forEach { tab ->
            val isSelected = tab == selected
            val contentColor by animateColorAsState(
                targetValue = if (isSelected) Color.White else Color.White.copy(alpha = 0.55f),
                animationSpec = tween(200),
                label = "tabTint"
            )
            val pillBg by animateColorAsState(
                targetValue = if (isSelected) Color.White.copy(alpha = 0.18f) else Color.Transparent,
                animationSpec = tween(220),
                label = "tabPill"
            )

            Column(
                modifier = Modifier
                    .weight(1f)
                    .clip(CircleShape)
                    .background(pillBg)
                    .clickable(
                        interactionSource = remember { MutableInteractionSource() },
                        indication = null
                    ) { onSelect(tab) }
                    .padding(vertical = 8.dp),
                horizontalAlignment = Alignment.CenterHorizontally,
                verticalArrangement = Arrangement.spacedBy(3.dp)
            ) {
                when (tab) {
                    Screen.NOW_PLAYING -> MusicNoteIcon(tint = contentColor, size = 20.dp)
                    Screen.LYRICS -> QuoteBubbleIcon(tint = contentColor, size = 20.dp)
                    Screen.QUEUE -> ListBulletIcon(tint = contentColor, size = 20.dp)
                    Screen.EQUALIZER -> SlidersIcon(tint = contentColor, size = 20.dp)
                    Screen.SETTINGS -> GearIcon(tint = contentColor, size = 20.dp)
                }
                Text(
                    text = tab.title,
                    color = contentColor,
                    fontSize = 10.sp,
                    fontWeight = FontWeight.SemiBold,
                    maxLines = 1
                )
            }
        }
    }
}
