package org.example.balancingrobot.control

import kotlin.math.hypot

/** Normalized preview intent, not physical speed or permission to enable motors. */
data class JoystickInput(val turn: Float = 0f, val forward: Float = 0f) {
    val magnitude: Float get() = hypot(turn, forward)
    companion object {
        /** Screen X points right, screen Y points down. Radial clamping preserves direction. */
        fun fromTouch(x: Float, y: Float, centerX: Float, centerY: Float, radius: Float): JoystickInput {
            if (!x.isFinite() || !y.isFinite() || !centerX.isFinite() || !centerY.isFinite() ||
                !radius.isFinite() || radius <= 0f) return JoystickInput()
            val dx = (x - centerX) / radius
            val dy = (centerY - y) / radius
            val distance = hypot(dx, dy)
            // A small center dead zone avoids unintended intent from finger jitter.
            if (!distance.isFinite() || distance <= 0.05f) return JoystickInput()
            val magnitude = ((distance.coerceAtMost(1f) - 0.05f) / 0.95f).coerceIn(0f, 1f)
            return JoystickInput(dx / distance * magnitude, dy / distance * magnitude)
        }
    }
}
