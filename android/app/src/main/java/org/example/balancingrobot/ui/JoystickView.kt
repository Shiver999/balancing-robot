package org.example.balancingrobot.ui

import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.view.MotionEvent
import android.view.View
import org.example.balancingrobot.control.JoystickInput
import kotlin.math.hypot
import kotlin.math.min

/** Round, single-finger joystick. Release/cancellation/focus loss always returns to center. */
class JoystickView(context: Context) : View(context) {
    var onInput: (JoystickInput) -> Unit = {}
    private val paint = Paint(Paint.ANTI_ALIAS_FLAG)
    private var pointerId = MotionEvent.INVALID_POINTER_ID
    private var input = JoystickInput()
    private val radius get() = (min(width, height) / 2f - 16 * resources.displayMetrics.density).coerceAtLeast(0f)
    init {
        isClickable = true
        isFocusable = true
        contentDescription = "Motion preview joystick. Up forward, down reverse, left and right turn. Motor output disabled."
    }
    fun reset() {
        pointerId = MotionEvent.INVALID_POINTER_ID
        input = JoystickInput(); onInput(input); invalidate()
        parent?.requestDisallowInterceptTouchEvent(false)
    }
    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        val cx = width / 2f; val cy = height / 2f; val r = radius
        paint.style = Paint.Style.FILL; paint.color = Color.rgb(24, 39, 57)
        canvas.drawCircle(cx, cy, r, paint)
        paint.style = Paint.Style.STROKE; paint.strokeWidth = 2 * resources.displayMetrics.density
        paint.color = if (isEnabled) Color.rgb(62, 184, 206) else Color.GRAY
        canvas.drawCircle(cx, cy, r, paint)
        paint.color = Color.rgb(61, 83, 105)
        canvas.drawCircle(cx, cy, r / 2, paint)
        canvas.drawLine(cx - r, cy, cx + r, cy, paint)
        canvas.drawLine(cx, cy - r, cx, cy + r, paint)
        paint.style = Paint.Style.FILL
        paint.color = Color.rgb(132, 231, 240)
        canvas.drawCircle(cx + input.turn * r, cy - input.forward * r, r * 0.15f, paint)
        paint.textAlign = Paint.Align.CENTER; paint.textSize = 12 * resources.displayMetrics.scaledDensity
        paint.color = Color.WHITE
        canvas.drawText("FORWARD", cx, cy - r * 0.72f, paint)
        canvas.drawText("REVERSE", cx, cy + r * 0.82f, paint)
    }
    override fun onTouchEvent(event: MotionEvent): Boolean {
        if (!isEnabled) return false
        val cx = width / 2f; val cy = height / 2f
        when (event.actionMasked) {
            MotionEvent.ACTION_DOWN -> {
                if (radius <= 0 || hypot(event.x - cx, event.y - cy) > radius) return false
                pointerId = event.getPointerId(0)
                parent?.requestDisallowInterceptTouchEvent(true)
            }
            MotionEvent.ACTION_MOVE -> if (pointerId == MotionEvent.INVALID_POINTER_ID) return false
            MotionEvent.ACTION_POINTER_DOWN -> return true // A second finger never takes ownership.
            MotionEvent.ACTION_POINTER_UP -> {
                if (event.getPointerId(event.actionIndex) == pointerId) reset()
                return true
            }
            MotionEvent.ACTION_UP -> { reset(); performClick(); return true }
            MotionEvent.ACTION_CANCEL -> { reset(); return true }
            else -> return pointerId != MotionEvent.INVALID_POINTER_ID
        }
        val index = event.findPointerIndex(pointerId)
        if (index < 0) { reset(); return true }
        // Movement outside the circle stays captured and clamps to maximum intent.
        input = JoystickInput.fromTouch(event.getX(index), event.getY(index), cx, cy, radius)
        onInput(input); invalidate()
        return true
    }
    override fun performClick(): Boolean { super.performClick(); return true }
    override fun onWindowFocusChanged(hasWindowFocus: Boolean) {
        super.onWindowFocusChanged(hasWindowFocus)
        if (!hasWindowFocus) reset()
    }
    override fun onDetachedFromWindow() { reset(); super.onDetachedFromWindow() }
}
