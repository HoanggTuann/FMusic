package com.fmusic.ui

import androidx.compose.animation.animateColorAsState
import androidx.compose.animation.core.Spring
import androidx.compose.animation.core.animateDpAsState
import androidx.compose.animation.core.spring
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.offset
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.shadow
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Rect
import androidx.compose.ui.geometry.RoundRect
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.Shape
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.StrokeJoin
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import kotlin.math.PI
import kotlin.math.cos
import kotlin.math.sin

val IosGreen = Color(0xFF34C759)
val IosRed = Color(0xFFFF453A)

fun Modifier.iosGlass(
    shape: Shape,
    fillAlpha: Float = 0.56f,
    borderAlpha: Float = 0.16f
): Modifier = this
    .clip(shape)
    .background(Color(0xFF1A1A20).copy(alpha = fillAlpha))
    .background(
        Brush.verticalGradient(
            listOf(
                Color.White.copy(alpha = 0.11f),
                Color.White.copy(alpha = 0.03f)
            )
        )
    )
    .border(0.75.dp, Color.White.copy(alpha = borderAlpha), shape)

@Composable
fun IosSwitch(
    checked: Boolean,
    onCheckedChange: (Boolean) -> Unit,
    modifier: Modifier = Modifier
) {
    val trackColor by animateColorAsState(
        targetValue = if (checked) IosGreen else Color.White.copy(alpha = 0.24f),
        animationSpec = spring(stiffness = Spring.StiffnessMediumLow),
        label = "switchTrack"
    )
    val thumbOffset by animateDpAsState(
        targetValue = if (checked) 20.dp else 0.dp,
        animationSpec = spring(dampingRatio = 0.72f, stiffness = Spring.StiffnessMedium),
        label = "switchThumb"
    )

    Box(
        modifier = modifier
            .size(width = 51.dp, height = 31.dp)
            .clip(CircleShape)
            .background(trackColor)
            .clickable(
                interactionSource = remember { MutableInteractionSource() },
                indication = null,
                role = Role.Switch,
                onClick = { onCheckedChange(!checked) }
            )
            .padding(2.dp),
        contentAlignment = Alignment.CenterStart
    ) {
        Box(
            modifier = Modifier
                .offset(x = thumbOffset)
                .size(27.dp)
                .shadow(3.dp, CircleShape)
                .clip(CircleShape)
                .background(Color.White)
        )
    }
}

@Composable
fun MusicNoteIcon(
    tint: Color = Color.White,
    size: Dp = 20.dp,
    modifier: Modifier = Modifier
) {
    Canvas(modifier = modifier.size(size)) {
        val w = this.size.width
        val h = this.size.height
        // Single / beamed note matching SF music.note
        val path = Path().apply {
            moveTo(w * 0.42f, h * 0.72f)
            lineTo(w * 0.42f, h * 0.22f)
            lineTo(w * 0.80f, h * 0.12f)
            lineTo(w * 0.80f, h * 0.62f)
        }
        drawPath(
            path = path,
            color = tint,
            style = Stroke(width = w * 0.11f, cap = StrokeCap.Round, join = StrokeJoin.Round)
        )
        drawOval(
            color = tint,
            topLeft = Offset(w * 0.18f, h * 0.62f),
            size = Size(w * 0.28f, h * 0.23f)
        )
        drawOval(
            color = tint,
            topLeft = Offset(w * 0.56f, h * 0.52f),
            size = Size(w * 0.28f, h * 0.23f)
        )
    }
}

@Composable
fun QuoteBubbleIcon(
    tint: Color = Color.White,
    size: Dp = 20.dp,
    modifier: Modifier = Modifier
) {
    Canvas(modifier = modifier.size(size)) {
        val w = this.size.width
        val h = this.size.height
        val stroke = Stroke(width = w * 0.085f, cap = StrokeCap.Round, join = StrokeJoin.Round)
        val bubble = Path().apply {
            addRoundRect(
                RoundRect(
                    rect = Rect(w * 0.12f, h * 0.16f, w * 0.88f, h * 0.74f),
                    cornerRadius = CornerRadius(w * 0.18f, w * 0.18f)
                )
            )
        }
        drawPath(bubble, tint, style = stroke)
        // Small tail
        val tail = Path().apply {
            moveTo(w * 0.34f, h * 0.74f)
            lineTo(w * 0.26f, h * 0.89f)
            lineTo(w * 0.48f, h * 0.74f)
        }
        drawPath(tail, tint, style = stroke)
        // Quote marks
        drawLine(tint, Offset(w * 0.36f, h * 0.38f), Offset(w * 0.36f, h * 0.50f), strokeWidth = w * 0.09f, cap = StrokeCap.Round)
        drawLine(tint, Offset(w * 0.62f, h * 0.38f), Offset(w * 0.62f, h * 0.50f), strokeWidth = w * 0.09f, cap = StrokeCap.Round)
    }
}

@Composable
fun ListBulletIcon(
    tint: Color = Color.White,
    size: Dp = 20.dp,
    modifier: Modifier = Modifier
) {
    Canvas(modifier = modifier.size(size)) {
        val w = this.size.width
        val h = this.size.height
        val sw = w * 0.095f
        listOf(0.26f, 0.50f, 0.74f).forEach { y ->
            drawCircle(tint, radius = w * 0.055f, center = Offset(w * 0.18f, h * y))
            drawLine(tint, Offset(w * 0.34f, h * y), Offset(w * 0.86f, h * y), strokeWidth = sw, cap = StrokeCap.Round)
        }
    }
}

@Composable
fun SlidersIcon(
    tint: Color = Color.White,
    size: Dp = 20.dp,
    modifier: Modifier = Modifier
) {
    Canvas(modifier = modifier.size(size)) {
        val w = this.size.width
        val h = this.size.height
        val sw = w * 0.085f
        val rows = listOf(0.24f to 0.36f, 0.50f to 0.68f, 0.76f to 0.42f)
        rows.forEach { (y, knobX) ->
            drawLine(tint, Offset(w * 0.12f, h * y), Offset(w * 0.88f, h * y), strokeWidth = sw, cap = StrokeCap.Round)
            drawCircle(tint, radius = w * 0.095f, center = Offset(w * knobX, h * y))
        }
    }
}

@Composable
fun GearIcon(
    tint: Color = Color.White,
    size: Dp = 20.dp,
    modifier: Modifier = Modifier
) {
    Canvas(modifier = modifier.size(size)) {
        val w = this.size.width
        val h = this.size.height
        val cx = w / 2f
        val cy = h / 2f
        val sw = w * 0.09f
        drawCircle(tint, radius = w * 0.27f, center = Offset(cx, cy), style = Stroke(width = sw))
        drawCircle(tint, radius = w * 0.11f, center = Offset(cx, cy), style = Stroke(width = sw * 0.8f))
        for (i in 0 until 8) {
            val a = i * (PI / 4.0).toFloat()
            val r1 = w * 0.29f
            val r2 = w * 0.41f
            drawLine(
                color = tint,
                start = Offset(cx + r1 * cos(a), cy + r1 * sin(a)),
                end = Offset(cx + r2 * cos(a), cy + r2 * sin(a)),
                strokeWidth = w * 0.11f,
                cap = StrokeCap.Round
            )
        }
    }
}

@Composable
fun PlayIcon(
    tint: Color = Color.White,
    size: Dp = 24.dp,
    modifier: Modifier = Modifier
) {
    Canvas(modifier = modifier.size(size)) {
        val w = this.size.width
        val h = this.size.height
        val path = Path().apply {
            moveTo(w * 0.24f, h * 0.16f)
            lineTo(w * 0.84f, h * 0.50f)
            lineTo(w * 0.24f, h * 0.84f)
            close()
        }
        drawPath(path, tint)
        drawPath(path, tint, style = Stroke(width = w * 0.06f, join = StrokeJoin.Round))
    }
}

@Composable
fun PauseIcon(
    tint: Color = Color.White,
    size: Dp = 24.dp,
    modifier: Modifier = Modifier
) {
    Canvas(modifier = modifier.size(size)) {
        val w = this.size.width
        val h = this.size.height
        val barW = w * 0.22f
        val barH = h * 0.66f
        val r = CornerRadius(barW * 0.35f, barW * 0.35f)
        drawRoundRect(tint, Offset(w * 0.22f, h * 0.17f), Size(barW, barH), r)
        drawRoundRect(tint, Offset(w * 0.56f, h * 0.17f), Size(barW, barH), r)
    }
}

@Composable
fun ForwardIcon(
    tint: Color = Color.White,
    size: Dp = 24.dp,
    modifier: Modifier = Modifier
) {
    Canvas(modifier = modifier.size(size)) {
        val w = this.size.width
        val h = this.size.height
        fun tri(x0: Float, x1: Float) {
            val p = Path().apply {
                moveTo(x0, h * 0.22f)
                lineTo(x1, h * 0.50f)
                lineTo(x0, h * 0.78f)
                close()
            }
            drawPath(p, tint)
            drawPath(p, tint, style = Stroke(width = w * 0.05f, join = StrokeJoin.Round))
        }
        tri(w * 0.12f, w * 0.52f)
        tri(w * 0.50f, w * 0.90f)
    }
}

@Composable
fun BackwardIcon(
    tint: Color = Color.White,
    size: Dp = 24.dp,
    modifier: Modifier = Modifier
) {
    Canvas(modifier = modifier.size(size)) {
        val w = this.size.width
        val h = this.size.height
        fun tri(xTip: Float, xBase: Float) {
            val p = Path().apply {
                moveTo(xTip, h * 0.50f)
                lineTo(xBase, h * 0.22f)
                lineTo(xBase, h * 0.78f)
                close()
            }
            drawPath(p, tint)
            drawPath(p, tint, style = Stroke(width = w * 0.05f, join = StrokeJoin.Round))
        }
        tri(w * 0.10f, w * 0.50f)
        tri(w * 0.48f, w * 0.88f)
    }
}

@Composable
fun ShuffleIcon(
    tint: Color = Color.White,
    size: Dp = 20.dp,
    modifier: Modifier = Modifier
) {
    Canvas(modifier = modifier.size(size)) {
        val w = this.size.width
        val h = this.size.height
        val sw = w * 0.095f
        val p1 = Path().apply {
            moveTo(w * 0.14f, h * 0.30f)
            lineTo(w * 0.34f, h * 0.30f)
            lineTo(w * 0.64f, h * 0.70f)
            lineTo(w * 0.84f, h * 0.70f)
        }
        val p2 = Path().apply {
            moveTo(w * 0.14f, h * 0.70f)
            lineTo(w * 0.34f, h * 0.70f)
            lineTo(w * 0.64f, h * 0.30f)
            lineTo(w * 0.84f, h * 0.30f)
        }
        drawPath(p1, tint, style = Stroke(sw, cap = StrokeCap.Round, join = StrokeJoin.Round))
        drawPath(p2, tint, style = Stroke(sw, cap = StrokeCap.Round, join = StrokeJoin.Round))
        // Arrowheads
        listOf(h * 0.30f, h * 0.70f).forEach { ay ->
            val ah = Path().apply {
                moveTo(w * 0.76f, ay - h * 0.10f)
                lineTo(w * 0.88f, ay)
                lineTo(w * 0.76f, ay + h * 0.10f)
            }
            drawPath(ah, tint, style = Stroke(sw, cap = StrokeCap.Round, join = StrokeJoin.Round))
        }
    }
}

@Composable
fun RepeatIcon(
    one: Boolean = false,
    tint: Color = Color.White,
    size: Dp = 20.dp,
    modifier: Modifier = Modifier
) {
    Box(modifier = modifier.size(size), contentAlignment = Alignment.Center) {
        Canvas(modifier = Modifier.size(size)) {
            val w = this.size.width
            val h = this.size.height
            val sw = w * 0.095f
            val top = Path().apply {
                moveTo(w * 0.18f, h * 0.52f)
                lineTo(w * 0.18f, h * 0.36f)
                quadraticTo(w * 0.18f, h * 0.24f, w * 0.32f, h * 0.24f)
                lineTo(w * 0.80f, h * 0.24f)
            }
            val bot = Path().apply {
                moveTo(w * 0.82f, h * 0.48f)
                lineTo(w * 0.82f, h * 0.64f)
                quadraticTo(w * 0.82f, h * 0.76f, w * 0.68f, h * 0.76f)
                lineTo(w * 0.20f, h * 0.76f)
            }
            drawPath(top, tint, style = Stroke(sw, cap = StrokeCap.Round, join = StrokeJoin.Round))
            drawPath(bot, tint, style = Stroke(sw, cap = StrokeCap.Round, join = StrokeJoin.Round))

            val a1 = Path().apply {
                moveTo(w * 0.70f, h * 0.14f)
                lineTo(w * 0.82f, h * 0.24f)
                lineTo(w * 0.70f, h * 0.34f)
            }
            val a2 = Path().apply {
                moveTo(w * 0.30f, h * 0.66f)
                lineTo(w * 0.18f, h * 0.76f)
                lineTo(w * 0.30f, h * 0.86f)
            }
            drawPath(a1, tint, style = Stroke(sw, cap = StrokeCap.Round, join = StrokeJoin.Round))
            drawPath(a2, tint, style = Stroke(sw, cap = StrokeCap.Round, join = StrokeJoin.Round))

            if (one) {
                drawLine(tint, Offset(w * 0.50f, h * 0.39f), Offset(w * 0.50f, h * 0.61f), strokeWidth = sw * 1.05f, cap = StrokeCap.Round)
                drawLine(tint, Offset(w * 0.44f, h * 0.44f), Offset(w * 0.50f, h * 0.39f), strokeWidth = sw * 0.9f, cap = StrokeCap.Round)
            }
        }
    }
}

@Composable
fun BookmarkIcon(tint: Color = Color.White, size: Dp = 14.dp, modifier: Modifier = Modifier) {
    Canvas(modifier = modifier.size(size)) {
        val w = this.size.width; val h = this.size.height
        val p = Path().apply {
            moveTo(w * 0.24f, h * 0.12f)
            lineTo(w * 0.76f, h * 0.12f)
            lineTo(w * 0.76f, h * 0.88f)
            lineTo(w * 0.50f, h * 0.66f)
            lineTo(w * 0.24f, h * 0.88f)
            close()
        }
        drawPath(p, tint)
    }
}

@Composable
fun TrayArrowIcon(up: Boolean, tint: Color = Color.White, size: Dp = 16.dp, modifier: Modifier = Modifier) {
    Canvas(modifier = modifier.size(size)) {
        val w = this.size.width; val h = this.size.height; val sw = w * 0.095f
        val tray = Path().apply {
            moveTo(w * 0.18f, h * 0.58f)
            lineTo(w * 0.18f, h * 0.84f)
            lineTo(w * 0.82f, h * 0.84f)
            lineTo(w * 0.82f, h * 0.58f)
        }
        drawPath(tray, tint, style = Stroke(sw, cap = StrokeCap.Round, join = StrokeJoin.Round))
        drawLine(tint, Offset(w * 0.50f, h * 0.16f), Offset(w * 0.50f, h * 0.64f), strokeWidth = sw, cap = StrokeCap.Round)
        val tipY = if (up) h * 0.16f else h * 0.64f
        val wingY = if (up) h * 0.32f else h * 0.48f
        val head = Path().apply {
            moveTo(w * 0.34f, wingY)
            lineTo(w * 0.50f, tipY)
            lineTo(w * 0.66f, wingY)
        }
        drawPath(head, tint, style = Stroke(sw, cap = StrokeCap.Round, join = StrokeJoin.Round))
    }
}
