package org.example.balancingrobot.control
import org.junit.Assert.*
import org.junit.Test
import kotlin.math.sqrt

/** Verify the actual touch mapping independently of Android drawing or motor hardware. */
class JoystickInputTest {
    private fun input(x: Float, y: Float) = JoystickInput.fromTouch(x, y, 0f, 0f, 100f)
    @Test fun centerAndDeadZoneAreNeutral() {
        assertEquals(JoystickInput(), input(0f, 0f))
        assertEquals(JoystickInput(), input(3f, -3f))
    }
    @Test fun cardinalDirectionsReachFullIntent() {
        assertEquals(JoystickInput(0f, 1f), input(0f, -100f))
        assertEquals(JoystickInput(0f, -1f), input(0f, 100f))
        assertEquals(JoystickInput(1f, 0f), input(100f, 0f))
        assertEquals(JoystickInput(-1f, 0f), input(-100f, 0f))
    }
    @Test fun intensityIncreasesWithRadius() {
        val near = input(0f, -10f).magnitude
        val middle = input(0f, -50f).magnitude
        assertTrue(near > 0 && middle > near && middle < 1)
        assertEquals((0.5f - 0.05f) / 0.95f, middle, 0.00001f)
    }
    @Test fun diagonalsAreCircularAndOutsideDragClamps() {
        val diagonal = input(100f, -100f)
        assertEquals(1f, diagonal.magnitude, 0.00001f)
        assertEquals(1f / sqrt(2f), diagonal.turn, 0.00001f)
        assertEquals(diagonal.turn, diagonal.forward, 0f)
        assertEquals(diagonal, input(200f, -200f))
    }
    @Test fun invalidGeometryProducesNoIntent() {
        assertEquals(JoystickInput(), input(Float.NaN, 0f))
        assertEquals(JoystickInput(), JoystickInput.fromTouch(1f, 1f, 0f, 0f, 0f))
        assertEquals(JoystickInput(), input(Float.POSITIVE_INFINITY, 0f))
    }
}
