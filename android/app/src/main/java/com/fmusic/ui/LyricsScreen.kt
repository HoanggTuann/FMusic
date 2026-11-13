package com.fmusic.ui

import android.os.Build
import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.core.tween
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.navigationBarsPadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.itemsIndexed
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.alpha
import androidx.compose.ui.draw.blur
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.fmusic.PlayerViewModel
import kotlin.math.abs

@Composable
fun LyricsScreen(vm: PlayerViewModel) {
    val lyrics by vm.lyrics.collectAsState()
    val synced by vm.lyricsSynced.collectAsState()
    val currentLyric by vm.currentLyric.collectAsState()
    val lyricOffsetMs by vm.lyricOffsetMs.collectAsState()
    val listState = rememberLazyListState()

    if (lyrics.isEmpty()) {
        Box(
            modifier = Modifier
                .fillMaxSize()
                .navigationBarsPadding()
                .padding(horizontal = 32.dp)
                .padding(bottom = 140.dp),
            contentAlignment = Alignment.Center
        ) {
            Column(
                horizontalAlignment = Alignment.CenterHorizontally,
                verticalArrangement = Arrangement.spacedBy(8.dp)
            ) {
                Text(
                    text = "Không có lời bài hát",
                    color = Color.White,
                    fontSize = 20.sp,
                    fontWeight = FontWeight.SemiBold,
                    textAlign = TextAlign.Center
                )
                Text(
                    text = "Chọn kèm tệp .lrc cùng tên với tệp nhạc (vd: bai-hat.mp3 + bai-hat.lrc), hoặc dùng tệp có lời nhúng trong tag.",
                    color = Color.White.copy(alpha = 0.6f),
                    fontSize = 13.sp,
                    lineHeight = 18.sp,
                    textAlign = TextAlign.Center
                )
            }
        }
        return
    }

    BoxWithConstraints(modifier = Modifier.fillMaxSize()) {
        val heightPx = with(LocalDensity.current) { maxHeight.toPx() }
        val anchorOffset = -(heightPx * 0.28f).toInt()
        var firstScroll by remember { mutableStateOf(true) }

        LaunchedEffect(currentLyric, synced) {
            if (synced && currentLyric in lyrics.indices) {
                if (firstScroll) {
                    firstScroll = false
                    listState.scrollToItem(index = currentLyric, scrollOffset = anchorOffset)
                } else {
                    listState.animateScrollToItem(index = currentLyric, scrollOffset = anchorOffset)
                }
            }
        }

        LazyColumn(
            state = listState,
            modifier = Modifier.fillMaxSize(),
            contentPadding = PaddingValues(
                start = 24.dp,
                end = 24.dp,
                top = 72.dp,
                bottom = 200.dp
            ),
            verticalArrangement = Arrangement.spacedBy(22.dp)
        ) {
            itemsIndexed(lyrics, key = { _, line -> line.id }) { idx, line ->
                val isCurrent = idx == currentLyric
                val targetAlpha = when {
                    !synced -> 0.9f
                    isCurrent -> 1.0f
                    else -> 0.35f
                }
                val animAlpha by animateFloatAsState(
                    targetValue = targetAlpha,
                    animationSpec = tween(durationMillis = 300),
                    label = "lyricAlpha"
                )
                val blurDp = if (synced && currentLyric >= 0) minOf(3, abs(idx - currentLyric)).dp else 0.dp
                val blurMod = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S && blurDp > 0.dp) {
                    Modifier.blur(blurDp)
                } else {
                    Modifier
                }

                Text(
                    text = line.text.ifBlank { "♪" },
                    color = Color.White,
                    fontSize = 27.sp,
                    lineHeight = 33.sp,
                    fontWeight = FontWeight.Bold,
                    modifier = Modifier
                        .fillMaxWidth()
                        .alpha(animAlpha)
                        .then(blurMod)
                        .clickable(
                            interactionSource = remember { MutableInteractionSource() },
                            indication = null,
                            enabled = synced && line.time >= 0.0
                        ) {
                            vm.seek(maxOf(0.0, line.time - lyricOffsetMs / 1000.0))
                        }
                )
            }
        }
    }
}
