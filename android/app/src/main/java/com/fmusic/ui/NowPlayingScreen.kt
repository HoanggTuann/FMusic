package com.fmusic.ui

import android.graphics.Bitmap
import androidx.compose.animation.core.Animatable
import androidx.compose.animation.core.FastOutSlowInEasing
import androidx.compose.animation.core.LinearEasing
import androidx.compose.animation.core.RepeatMode as AnimRepeatMode
import androidx.compose.animation.core.Spring
import androidx.compose.animation.core.animateDpAsState
import androidx.compose.animation.core.animateFloat
import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.core.infiniteRepeatable
import androidx.compose.animation.core.rememberInfiniteTransition
import androidx.compose.animation.core.spring
import androidx.compose.animation.core.tween
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.gestures.awaitEachGesture
import androidx.compose.foundation.gestures.awaitFirstDown
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
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
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.shadow
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Rect
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.min
import androidx.compose.ui.unit.sp
import com.fmusic.PlayerViewModel
import com.fmusic.RepeatMode
import kotlinx.coroutines.delay
import kotlin.math.PI
import kotlin.math.abs
import kotlin.math.cos
import kotlin.math.pow
import kotlin.math.sin

@Composable
fun AmbientBackground(
    backdrop: Bitmap?,
    token: Int,
    enabled: Boolean,
    modifier: Modifier = Modifier
) {
    val infinite = rememberInfiniteTransition(label = "ambientRot")
    val angle by infinite.animateFloat(
        initialValue = 0f,
        targetValue = 360f,
        animationSpec = infiniteRepeatable(
            animation = tween(durationMillis = 60_000, easing = LinearEasing),
            repeatMode = AnimRepeatMode.Restart
        ),
        label = "angle"
    )

    var currentImg by remember { mutableStateOf<Bitmap?>(backdrop) }
    var prevImg by remember { mutableStateOf<Bitmap?>(null) }
    val fade = remember { Animatable(1f) }

    LaunchedEffect(token, backdrop) {
        if (currentImg != backdrop) {
            prevImg = currentImg
            currentImg = backdrop
            fade.snapTo(0f)
            fade.animateTo(1f, animationSpec = tween(durationMillis = 900, easing = FastOutSlowInEasing))
            prevImg = null
        }
    }

    val fadeVal = fade.value

    Box(modifier = modifier.background(Color.Black)) {
        if (!enabled || (currentImg == null && prevImg == null)) {
            Box(
                modifier = Modifier
                    .fillMaxSize()
                    .background(
                        Brush.verticalGradient(
                            listOf(
                                Color(0xFF1A1A26),
                                Color(0xFF0E0E14),
                                Color(0xFF07070A)
                            )
                        )
                    )
            )
        } else {
            prevImg?.let { oldBmp ->
                AmbientLayers(bmp = oldBmp, angle = angle, alphaFactor = 1f - fadeVal)
            }
            currentImg?.let { newBmp ->
                AmbientLayers(bmp = newBmp, angle = angle, alphaFactor = fadeVal)
            }
        }
        Box(modifier = Modifier.fillMaxSize().background(Color.Black.copy(alpha = 0.38f)))
    }
}

@Composable
private fun AmbientLayers(bmp: Bitmap, angle: Float, alphaFactor: Float) {
    if (alphaFactor <= 0.005f) return
    val imgBmp = remember(bmp) { bmp.asImageBitmap() }
    Box(modifier = Modifier.fillMaxSize()) {
        Image(
            bitmap = imgBmp,
            contentDescription = null,
            contentScale = ContentScale.Crop,
            modifier = Modifier
                .fillMaxSize()
                .graphicsLayer {
                    scaleX = 1.85f
                    scaleY = 1.85f
                    rotationZ = angle * 0.4f
                    alpha = alphaFactor
                }
        )
        Image(
            bitmap = imgBmp,
            contentDescription = null,
            contentScale = ContentScale.Crop,
            modifier = Modifier
                .fillMaxSize()
                .graphicsLayer {
                    scaleX = 2.15f
                    scaleY = 2.15f
                    rotationZ = -angle * 0.25f
                    alpha = 0.55f * alphaFactor
                }
        )
    }
}

@Composable
fun NowPlayingScreen(vm: PlayerViewModel) {
    val isPlaying by vm.isPlaying.collectAsState()
    val position by vm.position.collectAsState()
    val duration by vm.duration.collectAsState()
    val title by vm.title.collectAsState()
    val artist by vm.artist.collectAsState()
    val cover by vm.coverBitmap.collectAsState()
    val showSpectrum by vm.spectrumRing.collectAsState()

    Column(
        modifier = Modifier
            .fillMaxSize()
            .padding(top = 10.dp),
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        Spacer(modifier = Modifier.height(12.dp))

        ArtworkStage(
            cover = cover,
            isPlaying = isPlaying,
            showSpectrum = showSpectrum,
            vm = vm,
            modifier = Modifier
                .fillMaxWidth()
                .weight(1f)
                .padding(horizontal = 12.dp)
        )

        Column(
            modifier = Modifier
                .fillMaxWidth()
                .padding(horizontal = 28.dp)
                .padding(top = 6.dp, bottom = 14.dp),
            horizontalAlignment = Alignment.CenterHorizontally,
            verticalArrangement = Arrangement.spacedBy(18.dp)
        ) {
            Column(
                horizontalAlignment = Alignment.CenterHorizontally,
                verticalArrangement = Arrangement.spacedBy(4.dp)
            ) {
                Text(
                    text = title,
                    color = Color.White,
                    fontSize = 22.sp,
                    fontWeight = FontWeight.SemiBold,
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis,
                    textAlign = TextAlign.Center
                )
                Text(
                    text = artist,
                    color = Color.White.copy(alpha = 0.6f),
                    fontSize = 17.sp,
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis,
                    textAlign = TextAlign.Center
                )
            }

            PlayerProgressBar(
                position = position,
                duration = duration,
                onSeek = { vm.seek(it) }
            )

            PlayerControls(vm = vm, isPlaying = isPlaying)
        }
    }
}

@Composable
private fun ArtworkStage(
    cover: Bitmap?,
    isPlaying: Boolean,
    showSpectrum: Boolean,
    vm: PlayerViewModel,
    modifier: Modifier = Modifier
) {
    val playScale by animateFloatAsState(
        targetValue = if (isPlaying) 1.0f else 0.86f,
        animationSpec = spring(dampingRatio = 0.70f, stiffness = Spring.StiffnessLow),
        label = "playScale"
    )
    val bassState = vm.bass.collectAsState()

    BoxWithConstraints(
        modifier = modifier,
        contentAlignment = Alignment.Center
    ) {
        val full = min(maxWidth, maxHeight)
        val art = full * 0.72f
        val radius = art * 0.045f

        Box(
            modifier = Modifier
                .size(full)
                .graphicsLayer {
                    val s = playScale + bassState.value * 0.02f
                    scaleX = s
                    scaleY = s
                },
            contentAlignment = Alignment.Center
        ) {
            if (showSpectrum) {
                SpectrumRingCanvas(vm = vm, modifier = Modifier.size(full))
            }

            val shape = RoundedCornerShape(radius)
            Box(
                modifier = Modifier
                    .size(art)
                    .shadow(
                        elevation = art * 0.08f,
                        shape = shape,
                        ambientColor = Color.Black.copy(alpha = 0.75f),
                        spotColor = Color.Black.copy(alpha = 0.75f)
                    )
                    .clip(shape)
                    .border(0.75.dp, Color.White.copy(alpha = 0.12f), shape),
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
                    Box(
                        modifier = Modifier
                            .fillMaxSize()
                            .background(
                                Brush.linearGradient(
                                    listOf(Color(0xFF474750), Color(0xFF1A1A20))
                                )
                            ),
                        contentAlignment = Alignment.Center
                    ) {
                        MusicNoteIcon(
                            tint = Color.White.copy(alpha = 0.5f),
                            size = art * 0.30f
                        )
                    }
                }
            }
        }
    }
}

/**
 * Ve 96 tia song nhac quanh chu vi hinh vuong bo goc cua anh bia (khop 100% voi iOS SpectrumRing).
 */
@Composable
private fun SpectrumRingCanvas(vm: PlayerViewModel, modifier: Modifier = Modifier) {
    val bandsState = vm.bands.collectAsState()

    Canvas(modifier = modifier) {
        val bands = bandsState.value
        val full = minOf(size.width, size.height)
        val artSide = full * 0.72f
        val gap = artSide * 0.035f
        val maxLen = artSide * 0.145f
        val cx = size.width / 2f
        val cy = size.height / 2f

        val ring = Rect(
            left = cx - artSide / 2f - gap,
            top = cy - artSide / 2f - gap,
            right = cx + artSide / 2f + gap,
            bottom = cy + artSide / 2f + gap
        )
        val r = artSide * 0.045f + gap
        val hw = ring.width - 2f * r
        val hh = ring.height - 2f * r
        val perimeter = 2f * hw + 2f * hh + 2f * PI.toFloat() * r
        val n = 96
        val bw = maxOf(2f, perimeter / n * 0.52f)
        val count = bands.size

        for (pass in 0..1) {
            for (k in 0 until n) {
                val f = (k + 0.5f) / n
                val g = abs(f - 0.5f) * 2f
                val x = g * (count - 1)
                val i0 = x.toInt().coerceIn(0, count - 1)
                val i1 = minOf(i0 + 1, count - 1)
                val frac = x - i0
                val v = bands[i0] * (1f - frac) + bands[i1] * frac

                val (px, py, nx, ny) = perimeterPoint(ring, r, perimeter * f)
                val len = 3f + v.coerceIn(0f, 1f).pow(1.2f) * maxLen
                val alpha = if (pass == 0) 0.15f * v else (0.28f + 0.62f * v).coerceIn(0f, 1f)

                drawLine(
                    color = Color.White.copy(alpha = alpha),
                    start = Offset(px, py),
                    end = Offset(px + nx * len, py + ny * len),
                    strokeWidth = if (pass == 0) bw * 2.4f else bw,
                    cap = StrokeCap.Round
                )
            }
        }
    }
}

private data class RingPt(val px: Float, val py: Float, val nx: Float, val ny: Float)

private fun perimeterPoint(rc: Rect, r: Float, sIn: Float): RingPt {
    val hw = rc.width - 2f * r
    val hh = rc.height - 2f * r
    val q = (PI.toFloat() * r) / 2f
    val lens = floatArrayOf(hw / 2f, q, hh, q, hw, q, hh, q, hw / 2f)
    var s = sIn
    var i = 0
    while (i < 8 && s > lens[i]) {
        s -= lens[i]
        i++
    }
    val u = if (lens[i] > 0f) (s / lens[i]).coerceIn(0f, 1f) else 0f

    fun arc(cx: Float, cy: Float, deg: Float): RingPt {
        val rad = deg * (PI.toFloat() / 180f)
        val nx = cos(rad)
        val ny = sin(rad)
        return RingPt(cx + r * nx, cy + r * ny, nx, ny)
    }

    return when (i) {
        0 -> RingPt(rc.center.x + u * hw / 2f, rc.bottom, 0f, 1f)
        1 -> arc(rc.right - r, rc.bottom - r, 90f - 90f * u)
        2 -> RingPt(rc.right, rc.bottom - r - u * hh, 1f, 0f)
        3 -> arc(rc.right - r, rc.top + r, -90f * u)
        4 -> RingPt(rc.right - r - u * hw, rc.top, 0f, -1f)
        5 -> arc(rc.left + r, rc.top + r, -90f - 90f * u)
        6 -> RingPt(rc.left, rc.top + r + u * hh, -1f, 0f)
        7 -> arc(rc.left + r, rc.bottom - r, 180f - 90f * u)
        else -> RingPt(rc.left + r + u * hw / 2f, rc.bottom, 0f, 1f)
    }
}

@Composable
fun PlayerProgressBar(position: Double, duration: Double, onSeek: (Double) -> Unit) {
    var dragFraction by remember { mutableStateOf<Float?>(null) }
    val rawFrac = if (duration > 0.0) (position / duration).toFloat().coerceIn(0f, 1f) else 0f
    val frac = dragFraction ?: rawFrac

    val barHeight by animateDpAsState(
        targetValue = if (dragFraction != null) 10.dp else 6.dp,
        animationSpec = tween(durationMillis = 150),
        label = "barHeight"
    )

    Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
        Box(
            modifier = Modifier
                .fillMaxWidth()
                .height(26.dp)
                .pointerInput(duration) {
                    awaitEachGesture {
                        val down = awaitFirstDown(requireUnconsumed = false)
                        down.consume()
                        val w = size.width.toFloat().coerceAtLeast(1f)
                        dragFraction = (down.position.x / w).coerceIn(0f, 1f)
                        do {
                            val event = awaitPointerEvent()
                            val change = event.changes.firstOrNull { it.id == down.id } ?: break
                            if (change.pressed) {
                                change.consume()
                                dragFraction = (change.position.x / w).coerceIn(0f, 1f)
                            }
                        } while (event.changes.any { it.pressed })
                        val finalFrac = dragFraction
                        dragFraction = null
                        if (finalFrac != null && duration > 0.0) {
                            onSeek(finalFrac * duration)
                        }
                    }
                },
            contentAlignment = Alignment.CenterStart
        ) {
            Box(
                modifier = Modifier
                    .fillMaxWidth()
                    .height(barHeight)
                    .clip(CircleShape)
                    .background(Color.White.copy(alpha = 0.25f))
            ) {
                Box(
                    modifier = Modifier
                        .fillMaxWidth(frac)
                        .fillMaxHeight()
                        .clip(CircleShape)
                        .background(Color.White.copy(alpha = 0.92f))
                )
            }
        }

        val shown = frac * duration
        Row(modifier = Modifier.fillMaxWidth()) {
            Text(
                text = formatTime(shown),
                color = Color.White.copy(alpha = 0.6f),
                style = TextStyle(fontSize = 11.sp, fontFeatureSettings = "tnum")
            )
            Spacer(modifier = Modifier.weight(1f))
            Text(
                text = "-${formatTime(maxOf(0.0, duration - shown))}",
                color = Color.White.copy(alpha = 0.6f),
                style = TextStyle(fontSize = 11.sp, fontFeatureSettings = "tnum")
            )
        }
    }
}

@Composable
private fun PlayerControls(vm: PlayerViewModel, isPlaying: Boolean) {
    val shuffle by vm.shuffle.collectAsState()
    val repeatMode by vm.repeatMode.collectAsState()

    Row(
        modifier = Modifier.fillMaxWidth(),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Box(
            modifier = Modifier
                .size(40.dp)
                .clickable(
                    interactionSource = remember { MutableInteractionSource() },
                    indication = null
                ) { vm.toggleShuffle() },
            contentAlignment = Alignment.Center
        ) {
            ShuffleIcon(
                tint = Color.White.copy(alpha = if (shuffle) 1f else 0.4f),
                size = 21.dp
            )
        }

        Spacer(modifier = Modifier.weight(1f))

        Row(
            horizontalArrangement = Arrangement.spacedBy(38.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            Box(
                modifier = Modifier
                    .size(44.dp)
                    .clickable(
                        interactionSource = remember { MutableInteractionSource() },
                        indication = null
                    ) { vm.previous() },
                contentAlignment = Alignment.Center
            ) {
                BackwardIcon(tint = Color.White, size = 30.dp)
            }

            Box(
                modifier = Modifier
                    .size(56.dp)
                    .clickable(
                        interactionSource = remember { MutableInteractionSource() },
                        indication = null
                    ) { vm.togglePlay() },
                contentAlignment = Alignment.Center
            ) {
                if (isPlaying) {
                    PauseIcon(tint = Color.White, size = 40.dp)
                } else {
                    PlayIcon(tint = Color.White, size = 42.dp)
                }
            }

            Box(
                modifier = Modifier
                    .size(44.dp)
                    .clickable(
                        interactionSource = remember { MutableInteractionSource() },
                        indication = null
                    ) { vm.next() },
                contentAlignment = Alignment.Center
            ) {
                ForwardIcon(tint = Color.White, size = 30.dp)
            }
        }

        Spacer(modifier = Modifier.weight(1f))

        Box(
            modifier = Modifier
                .size(40.dp)
                .clickable(
                    interactionSource = remember { MutableInteractionSource() },
                    indication = null
                ) { vm.cycleRepeat() },
            contentAlignment = Alignment.Center
        ) {
            RepeatIcon(
                one = repeatMode == RepeatMode.ONE,
                tint = Color.White.copy(alpha = if (repeatMode == RepeatMode.OFF) 0.4f else 1f),
                size = 21.dp
            )
        }
    }
}

fun formatTime(s: Double): String {
    val t = maxOf(0, kotlin.math.round(s).toInt())
    return "%d:%02d".format(t / 60, t % 60)
}
