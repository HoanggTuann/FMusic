package com.fmusic.ui

import android.content.Intent
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.ExperimentalFoundationApi
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.combinedClickable
import androidx.compose.foundation.gestures.awaitEachGesture
import androidx.compose.foundation.gestures.awaitFirstDown
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.navigationBarsPadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.OutlinedTextFieldDefaults
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableLongStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.alpha
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.core.content.FileProvider
import com.fmusic.PlayerViewModel
import kotlin.math.abs
import kotlin.math.round

private val EQ_FREQ_LABELS = listOf("31", "62", "125", "250", "500", "1k", "2k", "4k", "8k", "16k")

@OptIn(ExperimentalFoundationApi::class)
@Composable
fun EqualizerScreen(vm: PlayerViewModel) {
    val eqEnabled by vm.eqEnabled.collectAsState()
    val eqPreset by vm.eqPreset.collectAsState()
    val eqGains by vm.eqGains.collectAsState()
    val userPresets by vm.userPresets.collectAsState()
    val presets = vm.eqPresetNames
    val context = LocalContext.current

    var showSaveDialog by remember { mutableStateOf(false) }
    var presetName by remember { mutableStateOf("") }

    Column(
        modifier = Modifier
            .fillMaxSize()
            .verticalScroll(rememberScrollState())
            .navigationBarsPadding()
            .padding(horizontal = 20.dp)
            .padding(top = 6.dp, bottom = 200.dp),
        verticalArrangement = Arrangement.spacedBy(16.dp)
    ) {
        // Header: tieu de + switch iOS
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            Text(
                text = "Equalizer",
                color = Color.White,
                fontSize = 21.sp,
                fontWeight = FontWeight.Bold
            )
            IosSwitch(
                checked = eqEnabled,
                onCheckedChange = { vm.setEqEnabled(it) }
            )
        }

        // Preset mac dinh (lay tu C++ giong iOS)
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .horizontalScroll(rememberScrollState()),
            horizontalArrangement = Arrangement.spacedBy(8.dp)
        ) {
            presets.forEachIndexed { i, name ->
                val active = eqPreset == i
                Box(
                    modifier = Modifier
                        .clip(CircleShape)
                        .background(if (active) Color.White else Color.White.copy(alpha = 0.16f))
                        .clickable { vm.applyPreset(i) }
                        .padding(horizontal = 14.dp, vertical = 8.dp),
                    contentAlignment = Alignment.Center
                ) {
                    Text(
                        text = name,
                        color = if (active) Color.Black else Color.White,
                        fontSize = 14.sp,
                        fontWeight = FontWeight.SemiBold
                    )
                }
            }
        }

        // Preset nguoi dung (.feq)
        if (userPresets.isNotEmpty()) {
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .horizontalScroll(rememberScrollState()),
                horizontalArrangement = Arrangement.spacedBy(8.dp)
            ) {
                userPresets.forEach { preset ->
                    Row(
                        modifier = Modifier
                            .clip(CircleShape)
                            .background(Color.White.copy(alpha = 0.22f))
                            .combinedClickable(
                                onClick = { vm.applyUserPreset(preset) },
                                onLongClick = { vm.deleteUserPreset(preset.name) }
                            )
                            .padding(horizontal = 12.dp, vertical = 6.dp),
                        verticalAlignment = Alignment.CenterVertically,
                        horizontalArrangement = Arrangement.spacedBy(5.dp)
                    ) {
                        BookmarkIcon(tint = Color.White, size = 11.dp)
                        Text(
                            text = preset.name,
                            color = Color.White,
                            fontSize = 13.sp,
                            fontWeight = FontWeight.SemiBold
                        )
                    }
                }
            }
        }

        // 10 thanh truot doc EQ chuan iOS
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .height(270.dp)
                .alpha(if (eqEnabled) 1f else 0.45f),
            horizontalArrangement = Arrangement.SpaceBetween
        ) {
            for (band in 0 until 10) {
                val gain = if (band < eqGains.size) eqGains[band] else 0f
                EQBandSlider(
                    gain = gain,
                    label = EQ_FREQ_LABELS[band],
                    onChange = { vm.setEqGain(band, it) },
                    onReset = { vm.setEqGain(band, 0f) },
                    modifier = Modifier.weight(1f).fillMaxHeight()
                )
            }
        }

        // Nut Luu preset va Xuat .FEQ
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.spacedBy(12.dp, Alignment.CenterHorizontally)
        ) {
            Row(
                modifier = Modifier
                    .clip(CircleShape)
                    .background(Color.White.copy(alpha = 0.16f))
                    .clickable { showSaveDialog = true }
                    .padding(horizontal = 14.dp, vertical = 9.dp),
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.spacedBy(6.dp)
            ) {
                TrayArrowIcon(up = false, tint = Color.White, size = 15.dp)
                Text(
                    text = "Lưu preset",
                    color = Color.White,
                    fontSize = 13.sp,
                    fontWeight = FontWeight.SemiBold
                )
            }

            Row(
                modifier = Modifier
                    .clip(CircleShape)
                    .background(Color.White.copy(alpha = 0.16f))
                    .clickable {
                        val file = vm.exportPresetFile("FMusic_Preset")
                        val uri = FileProvider.getUriForFile(
                            context,
                            "${context.packageName}.fileprovider",
                            file
                        )
                        val share = Intent(Intent.ACTION_SEND).apply {
                            type = "application/octet-stream"
                            putExtra(Intent.EXTRA_STREAM, uri)
                            addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
                        }
                        context.startActivity(Intent.createChooser(share, "Xuất preset .FEQ"))
                    }
                    .padding(horizontal = 14.dp, vertical = 9.dp),
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.spacedBy(6.dp)
            ) {
                TrayArrowIcon(up = true, tint = Color.White, size = 15.dp)
                Text(
                    text = "Xuất .FEQ",
                    color = Color.White,
                    fontSize = 13.sp,
                    fontWeight = FontWeight.SemiBold
                )
            }
        }

        Text(
            text = "Kéo để chỉnh · chạm hai lần để về 0 dB",
            color = Color.White.copy(alpha = 0.5f),
            fontSize = 12.sp,
            textAlign = TextAlign.Center,
            modifier = Modifier.fillMaxWidth()
        )
    }

    if (showSaveDialog) {
        AlertDialog(
            onDismissRequest = { showSaveDialog = false },
            containerColor = Color(0xFF222228),
            titleContentColor = Color.White,
            textContentColor = Color.White,
            title = { Text("Lưu Preset EQ", fontWeight = FontWeight.SemiBold) },
            text = {
                OutlinedTextField(
                    value = presetName,
                    onValueChange = { presetName = it },
                    label = { Text("Tên preset") },
                    singleLine = true,
                    colors = OutlinedTextFieldDefaults.colors(
                        focusedTextColor = Color.White,
                        unfocusedTextColor = Color.White,
                        focusedBorderColor = Color.White,
                        unfocusedBorderColor = Color.White.copy(alpha = 0.4f),
                        focusedLabelColor = Color.White,
                        unfocusedLabelColor = Color.White.copy(alpha = 0.6f)
                    )
                )
            },
            confirmButton = {
                TextButton(onClick = {
                    val trimmed = presetName.trim()
                    if (trimmed.isNotEmpty()) {
                        vm.saveEqPreset(trimmed)
                        presetName = ""
                    }
                    showSaveDialog = false
                }) {
                    Text("Lưu", color = Color.White)
                }
            },
            dismissButton = {
                TextButton(onClick = {
                    presetName = ""
                    showSaveDialog = false
                }) {
                    Text("Huỷ", color = Color.White.copy(alpha = 0.7f))
                }
            }
        )
    }
}

@Composable
private fun EQBandSlider(
    gain: Float,
    label: String,
    onChange: (Float) -> Unit,
    onReset: () -> Unit,
    modifier: Modifier = Modifier
) {
    val gainText = remember(gain) {
        if (gain == round(gain)) {
            val v = gain.toInt()
            if (v > 0) "+$v" else "$v"
        } else {
            val formatted = "%.1f".format(gain)
            if (gain > 0f) "+$formatted" else formatted
        }
    }

    Column(
        modifier = modifier,
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.spacedBy(6.dp)
    ) {
        Text(
            text = gainText,
            color = Color.White.copy(alpha = 0.7f),
            style = TextStyle(fontSize = 10.sp, fontFeatureSettings = "tnum"),
            maxLines = 1
        )

        var lastTapMs by remember { mutableLongStateOf(0L) }

        Box(
            modifier = Modifier
                .weight(1f)
                .fillMaxWidth()
                .pointerInput(Unit) {
                    awaitEachGesture {
                        val down = awaitFirstDown(requireUnconsumed = false)
                        down.consume()
                        val padY = 10.dp.toPx()
                        val usableH = (size.height - 2f * padY).coerceAtLeast(1f)
                        val startY = down.position.y
                        val startMs = System.currentTimeMillis()
                        var moved = false

                        fun updateFromY(y: Float) {
                            val f = 1f - ((y - padY) / usableH).coerceIn(0f, 1f)
                            var db = f * 24f - 12f
                            db = round(db * 2f) / 2f
                            if (abs(db) < 0.4f) db = 0f
                            onChange(db)
                        }

                        updateFromY(down.position.y)
                        do {
                            val event = awaitPointerEvent()
                            val change = event.changes.firstOrNull { it.id == down.id } ?: break
                            if (change.pressed) {
                                change.consume()
                                if (abs(change.position.y - startY) > viewConfiguration.touchSlop) {
                                    moved = true
                                }
                                updateFromY(change.position.y)
                            }
                        } while (event.changes.any { it.pressed })

                        val now = System.currentTimeMillis()
                        if (!moved && (now - startMs) < 260L) {
                            if (now - lastTapMs < 320L) {
                                onReset()
                                lastTapMs = 0L
                            } else {
                                lastTapMs = now
                            }
                        }
                    }
                }
        ) {
            Canvas(modifier = Modifier.fillMaxSize()) {
                val padY = 10.dp.toPx()
                val trackH = (size.height - 2f * padY).coerceAtLeast(1f)
                val cx = size.width / 2f
                val midY = padY + trackH / 2f
                val norm = ((gain.coerceIn(-12f, 12f) + 12f) / 24f)
                val knobY = padY + trackH * (1f - norm)
                val trackW = 4.dp.toPx()
                val cr = CornerRadius(trackW / 2f, trackW / 2f)

                // Track nen
                drawRoundRect(
                    color = Color.White.copy(alpha = 0.25f),
                    topLeft = Offset(cx - trackW / 2f, padY),
                    size = Size(trackW, trackH),
                    cornerRadius = cr
                )

                // Doan fill tu 0 dB toi nut keo
                val fillTop = minOf(knobY, midY)
                val fillH = maxOf(2.dp.toPx(), abs(knobY - midY))
                drawRoundRect(
                    color = Color.White.copy(alpha = 0.90f),
                    topLeft = Offset(cx - trackW / 2f, fillTop),
                    size = Size(trackW, fillH),
                    cornerRadius = cr
                )

                // Vach 0 dB nhe
                drawCircle(
                    color = Color.White.copy(alpha = 0.45f),
                    radius = 2.dp.toPx(),
                    center = Offset(cx, midY)
                )

                // Nut keo tron trang giong iOS
                drawCircle(
                    color = Color.White,
                    radius = 9.5.dp.toPx(),
                    center = Offset(cx, knobY)
                )
            }
        }

        Text(
            text = label,
            color = Color.White.copy(alpha = 0.6f),
            fontSize = 10.sp,
            maxLines = 1
        )
    }
}
