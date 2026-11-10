package com.fmusic.ui

import androidx.compose.animation.core.*
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.gestures.detectHorizontalDragGestures
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.*
import androidx.compose.ui.graphics.drawscope.DrawScope
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.*
import com.fmusic.PlayerViewModel
import com.fmusic.RepeatMode
import kotlin.math.*

@Composable
fun NowPlayingScreen(vm: PlayerViewModel, onShowQueue: () -> Unit, onShowLyrics: () -> Unit) {
    val isPlaying by vm.isPlaying.collectAsState()
    val position by vm.position.collectAsState()
    val duration by vm.duration.collectAsState()
    val title by vm.title.collectAsState()
    val artist by vm.artist.collectAsState()
    val bands by vm.bands.collectAsState()
    val bass by vm.bass.collectAsState()

    Box(modifier = Modifier.fillMaxSize().background(Color.Black)) {
        // Ambient sweep gradient background
        AmbientBackground(modifier = Modifier.fillMaxSize())

        Column(
            modifier = Modifier.fillMaxSize().padding(24.dp),
            horizontalAlignment = Alignment.CenterHorizontally
        ) {
            Spacer(modifier = Modifier.height(40.dp))

            // Artwork + 96-bar spectrum ring with bass-pulse scale animation
            val artScale by animateFloatAsState(
                targetValue = if (isPlaying) 1.0f + bass * 0.02f else 0.86f,
                animationSpec = spring(dampingRatio = Spring.DampingRatioMediumBouncy),
                label = "artScale"
            )
            Box(
                modifier = Modifier
                    .size(300.dp)
                    .graphicsLayer(scaleX = artScale, scaleY = artScale)
            ) {
                // 96 radial spectrum bars drawn in a ring around artwork
                SpectrumRingCanvas(bands = bands, modifier = Modifier.fillMaxSize())
                // Artwork placeholder (replace with actual Coil AsyncImage when cover art is available)
                Box(
                    modifier = Modifier
                        .size(220.dp)
                        .align(Alignment.Center)
                        .clip(RoundedCornerShape(10.dp))
                        .background(Color(0xFF333355))
                )
            }

            Spacer(modifier = Modifier.height(32.dp))

            // Track title
            Text(
                title,
                style = MaterialTheme.typography.headlineSmall.copy(fontWeight = FontWeight.Bold),
                color = Color.White,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis
            )
            // Artist name
            Text(
                artist,
                style = MaterialTheme.typography.bodyMedium,
                color = Color.White.copy(alpha = 0.7f),
                maxLines = 1,
                overflow = TextOverflow.Ellipsis
            )

            Spacer(modifier = Modifier.height(24.dp))

            // Seekable progress bar with time labels
            PlayerProgressBar(position = position, duration = duration, onSeek = { vm.seek(it) })

            Spacer(modifier = Modifier.height(24.dp))

            // Playback controls row
            PlayerControls(vm = vm, isPlaying = isPlaying)

            Spacer(modifier = Modifier.height(16.dp))

            // Navigation shortcuts
            Row(horizontalArrangement = Arrangement.spacedBy(16.dp)) {
                TextButton(onClick = onShowLyrics) {
                    Text("Lời", color = Color.White.copy(alpha = 0.8f))
                }
                TextButton(onClick = onShowQueue) {
                    Text("Danh sách", color = Color.White.copy(alpha = 0.8f))
                }
            }
        }
    }
}

/**
 * Draws 96 radial spectrum bars arranged in a ring around the artwork.
 * Bars mirror symmetrically (left half = bass, right half = treble).
 */
@Composable
fun SpectrumRingCanvas(bands: FloatArray, modifier: Modifier = Modifier) {
    Canvas(modifier = modifier) {
        val cx = size.width / 2f
        val cy = size.height / 2f
        val artSide = size.width * 0.72f
        val gap = artSide * 0.035f
        val maxLen = artSide * 0.15f
        val n = 96 // total bar count
        val perimeter = 2f * Math.PI.toFloat() * (artSide / 2f + gap)
        val bw = maxOf(2f, perimeter / n * 0.52f)

        for (k in 0 until n) {
            // Map bar index to symmetric frequency (0 = bass, 1 = treble, mirrored)
            val f = (k + 0.5f) / n
            val g = abs(f - 0.5f) * 2f
            val x = g * (bands.size - 1)
            val i0 = x.toInt()
            val i1 = minOf(i0 + 1, bands.size - 1)
            val v = bands[i0] * (1f - (x - i0)) + bands[i1] * (x - i0)
            val len = 3f + v.pow(1.2f) * maxLen

            // Distribute bars evenly around the ring
            val angle = (f * 2f * Math.PI - Math.PI / 2f).toFloat()
            val r = artSide / 2f + gap
            val rx = r * cos(angle)
            val ry = r * sin(angle)
            val nx = cos(angle)
            val ny = sin(angle)

            val alpha = (0.28f + 0.62f * v).coerceIn(0f, 1f)
            drawLine(
                color = Color.White.copy(alpha = alpha),
                start = Offset(cx + rx, cy + ry),
                end = Offset(cx + rx + nx * len, cy + ry + ny * len),
                strokeWidth = bw,
                cap = StrokeCap.Round
            )
        }
    }
}

/**
 * Animated ambient background using an infinite sweep gradient.
 * In production, replace with Coil image + BlurTransformation seeded from cover art.
 */
@Composable
fun AmbientBackground(modifier: Modifier = Modifier) {
    Box(
        modifier = modifier.background(
            Brush.sweepGradient(
                listOf(
                    Color(0xFF1a1a3e),
                    Color(0xFF2d1b4e),
                    Color(0xFF1a2a3e),
                    Color(0xFF1a1a3e)
                )
            )
        )
    )
}

/**
 * Horizontal progress bar supporting tap-and-drag seeking.
 * Grows taller while dragging for better UX feedback.
 */
@Composable
fun PlayerProgressBar(position: Double, duration: Double, onSeek: (Double) -> Unit) {
    var dragFraction by remember { mutableStateOf<Float?>(null) }
    val frac = dragFraction ?: if (duration > 0) (position / duration).toFloat() else 0f

    Column {
        BoxWithConstraints(
            modifier = Modifier
                .fillMaxWidth()
                .height(24.dp)
                .pointerInput(Unit) {
                    detectHorizontalDragGestures(
                        onDragStart = { offset ->
                            dragFraction = (offset.x / size.width).coerceIn(0f, 1f)
                        },
                        onDragEnd = {
                            dragFraction?.let { onSeek(it * duration); dragFraction = null }
                        },
                        onHorizontalDrag = { _, delta ->
                            dragFraction = ((dragFraction ?: 0f) + delta / size.width).coerceIn(0f, 1f)
                        }
                    )
                }
        ) {
            LinearProgressIndicator(
                progress = { frac },
                modifier = Modifier
                    .fillMaxWidth()
                    .align(Alignment.Center)
                    .height(if (dragFraction != null) 10.dp else 6.dp),
                color = Color.White,
                trackColor = Color.White.copy(alpha = 0.25f)
            )
        }
        // Elapsed and remaining time labels
        Row(modifier = Modifier.fillMaxWidth()) {
            Text(
                formatTime(frac * duration),
                color = Color.White.copy(alpha = 0.6f),
                style = MaterialTheme.typography.labelSmall
            )
            Spacer(modifier = Modifier.weight(1f))
            Text(
                "-${formatTime((1f - frac) * duration)}",
                color = Color.White.copy(alpha = 0.6f),
                style = MaterialTheme.typography.labelSmall
            )
        }
    }
}

/**
 * Playback control bar: shuffle, previous, play/pause, next, repeat.
 */
@Composable
fun PlayerControls(vm: PlayerViewModel, isPlaying: Boolean) {
    val shuffle by vm.shuffle.collectAsState()
    val repeatMode by vm.repeatMode.collectAsState()

    Row(
        modifier = Modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.SpaceEvenly,
        verticalAlignment = Alignment.CenterVertically
    ) {
        // Shuffle toggle
        IconButton(onClick = { vm.toggleShuffle() }) {
            Text(
                "⇄",
                color = if (shuffle) Color.White else Color.White.copy(alpha = 0.4f),
                fontSize = 20.sp
            )
        }
        // Previous track / seek to start
        IconButton(onClick = { vm.previous() }, modifier = Modifier.size(48.dp)) {
            Text("⏮", fontSize = 28.sp)
        }
        // Play / Pause
        FilledIconButton(onClick = { vm.togglePlay() }, modifier = Modifier.size(64.dp)) {
            Text(if (isPlaying) "⏸" else "▶", fontSize = 28.sp)
        }
        // Next track
        IconButton(onClick = { vm.next() }, modifier = Modifier.size(48.dp)) {
            Text("⏭", fontSize = 28.sp)
        }
        // Repeat cycle: OFF → ALL → ONE → OFF
        IconButton(onClick = { vm.cycleRepeat() }) {
            Text(
                when (repeatMode) {
                    RepeatMode.OFF -> "↺"
                    RepeatMode.ALL -> "↺"
                    RepeatMode.ONE -> "①"
                },
                color = if (repeatMode != RepeatMode.OFF) Color.White else Color.White.copy(alpha = 0.4f),
                fontSize = 20.sp
            )
        }
    }
}

private fun formatTime(s: Double): String {
    val t = maxOf(0, s.toInt()); return "%d:%02d".format(t / 60, t % 60)
}

private fun formatTime(s: Float): String = formatTime(s.toDouble())
