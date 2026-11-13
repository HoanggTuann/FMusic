package com.fmusic.ui

import androidx.compose.animation.animateColorAsState
import androidx.compose.animation.core.tween
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.navigationBarsPadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Slider
import androidx.compose.material3.SliderDefaults
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.fmusic.PlayerViewModel
import kotlin.math.roundToInt

@Composable
fun SettingsScreen(vm: PlayerViewModel) {
    val ambient by vm.ambientBackground.collectAsState()
    val spectrum by vm.spectrumRing.collectAsState()
    val sampleRate by vm.sampleRate.collectAsState()
    val lyricOffsetMs by vm.lyricOffsetMs.collectAsState()

    Column(
        modifier = Modifier
            .fillMaxSize()
            .verticalScroll(rememberScrollState())
            .navigationBarsPadding()
            .padding(horizontal = 20.dp)
            .padding(top = 6.dp, bottom = 200.dp),
        verticalArrangement = Arrangement.spacedBy(14.dp)
    ) {
        Text(
            text = "Cài đặt",
            color = Color.White,
            fontSize = 24.sp,
            fontWeight = FontWeight.Bold
        )

        // Card 1: Hieu ung giao dien
        GlassCard {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                Text("Nền ambient động", color = Color.White, fontSize = 15.sp)
                IosSwitch(checked = ambient, onCheckedChange = { vm.setAmbientBackground(it) })
            }

            HorizontalDivider(color = Color.White.copy(alpha = 0.14f), thickness = 0.5.dp)

            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                Text("Sóng nhạc quanh ảnh bìa", color = Color.White, fontSize = 15.sp)
                IosSwitch(checked = spectrum, onCheckedChange = { vm.setSpectrumRing(it) })
            }
        }

        // Card 2: Sample rate segmented picker giong iOS
        GlassCard {
            Text(
                text = "Tần số lấy mẫu (DAC output)",
                color = Color.White,
                fontSize = 14.sp,
                fontWeight = FontWeight.SemiBold
            )
            IosSegmentedSampleRate(
                selected = sampleRate,
                onSelect = { vm.setSampleRate(it) }
            )
        }

        // Card 3: Lech thoi gian loi
        GlassCard {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                Text("Lệch thời gian lời", color = Color.White, fontSize = 15.sp)
                val ms = lyricOffsetMs.roundToInt()
                Text(
                    text = "${if (ms > 0) "+" else ""}$ms ms",
                    color = Color.White.copy(alpha = 0.6f),
                    style = TextStyle(fontSize = 14.sp, fontFeatureSettings = "tnum")
                )
            }
            Slider(
                value = lyricOffsetMs,
                onValueChange = { raw ->
                    val snapped = ((raw / 50f).roundToInt() * 50f).coerceIn(-2000f, 2000f)
                    vm.setLyricOffsetMs(snapped)
                },
                valueRange = -2000f..2000f,
                colors = SliderDefaults.colors(
                    thumbColor = Color.White,
                    activeTrackColor = Color.White,
                    inactiveTrackColor = Color.White.copy(alpha = 0.25f)
                )
            )
            Text(
                text = "Dương: lời hiện sớm hơn · Âm: lời hiện muộn hơn",
                color = Color.White.copy(alpha = 0.5f),
                fontSize = 12.sp
            )
        }

        // Card 4: Xoa danh sach phat
        GlassCard {
            Text(
                text = "Xoá danh sách phát",
                color = IosRed,
                fontSize = 15.sp,
                fontWeight = FontWeight.Medium,
                modifier = Modifier
                    .fillMaxWidth()
                    .clickable(
                        interactionSource = remember { MutableInteractionSource() },
                        indication = null
                    ) { vm.clearPlaylist() }
                    .padding(vertical = 2.dp)
            )
        }

        // Card 5: Gioi thieu
        GlassCard {
            Text(
                text = "FMusic 1.1",
                color = Color.White,
                fontSize = 16.sp,
                fontWeight = FontWeight.SemiBold
            )
            Text(
                text = "Giải mã bằng FFmpeg. Tối ưu âm thanh và giao diện đồng bộ chuẩn iOS. Hỗ trợ tệp .lrc, danh sách phát .fpl và preset .feq.",
                color = Color.White.copy(alpha = 0.6f),
                fontSize = 13.sp,
                lineHeight = 18.sp
            )
        }
    }
}

@Composable
private fun GlassCard(content: @Composable ColumnScope.() -> Unit) {
    val shape = RoundedCornerShape(22.dp)
    Column(
        modifier = Modifier
            .fillMaxWidth()
            .iosGlass(shape, fillAlpha = 0.46f, borderAlpha = 0.14f)
            .padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp),
        content = content
    )
}

@Composable
private fun IosSegmentedSampleRate(
    selected: Int,
    onSelect: (Int) -> Unit
) {
    val options = listOf(44100 to "44,100 Hz", 48000 to "48,000 Hz", 96000 to "96,000 Hz")
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(10.dp))
            .background(Color.Black.copy(alpha = 0.28f))
            .padding(3.dp),
        horizontalArrangement = Arrangement.spacedBy(2.dp)
    ) {
        options.forEach { (rate, label) ->
            val isSelected = selected == rate
            val bg by animateColorAsState(
                targetValue = if (isSelected) Color.White.copy(alpha = 0.24f) else Color.Transparent,
                animationSpec = tween(180),
                label = "segBg"
            )
            Box(
                modifier = Modifier
                    .weight(1f)
                    .height(32.dp)
                    .clip(RoundedCornerShape(8.dp))
                    .background(bg)
                    .clickable(
                        interactionSource = remember { MutableInteractionSource() },
                        indication = null
                    ) { onSelect(rate) },
                contentAlignment = Alignment.Center
            ) {
                Text(
                    text = label,
                    color = if (isSelected) Color.White else Color.White.copy(alpha = 0.72f),
                    fontSize = 13.sp,
                    fontWeight = if (isSelected) FontWeight.SemiBold else FontWeight.Normal,
                    textAlign = TextAlign.Center
                )
            }
        }
    }
}
