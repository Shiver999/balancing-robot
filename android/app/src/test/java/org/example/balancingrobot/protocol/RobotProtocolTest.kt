package org.example.balancingrobot.protocol

import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertThrows
import org.junit.Test

/** Encoder contract tests mirror firmware wire fixtures and stateless command policies. */
class RobotProtocolTest {
    // Synthetic limits allow moving test commands without setting production robot limits.
    private val limits = CommandLimits(maxForwardVelocityMps = 2f, maxYawRateRadS = 1f)
    private val moving = ControlCommand(1, 1.5f, -0.5f, true, true, 200)

    // Golden vector is also decoded by the firmware host suite.
    @Test
    fun controlPacketMatchesFirmwareLittleEndianLayout() {
        val packet = RobotProtocol.encode(
            ControlCommand(
                sequence = 0x1234,
                forwardVelocityMps = 1.5f,
                yawRateRadS = -0.5f,
                armRequested = true,
                deadmanActive = true,
                leaseMs = 200,
            ),
            CommandLimits(maxForwardVelocityMps = 2f, maxYawRateRadS = 1f),
        )

        assertArrayEquals(
            byteArrayOf(
                0x01, 0x01, 0x34, 0x12,
                0x00, 0x00, 0xC0.toByte(), 0x3F,
                0x00, 0x00, 0x00, 0xBF.toByte(),
                0x03, 0xC8.toByte(), 0x00,
            ),
            packet,
        )
    }

    // Default configuration admits stop/release intent while rejecting motion.
    @Test
    fun defaultsPermitOnlyZeroMotion() {
        assertThrows(IllegalArgumentException::class.java) { RobotProtocol.encode(moving) }
        val stopped = moving.copy(forwardVelocityMps = 0f, yawRateRadS = 0f,
            armRequested = false, deadmanActive = false)
        assertEquals(15, RobotProtocol.encode(stopped).size)
    }

    // NaN/infinity must never reach the wire; test each signed setpoint boundary.
    @Test
    fun rejectsNonfiniteAndOutOfRangeMotion() {
        for (value in listOf(Float.NaN, Float.POSITIVE_INFINITY, Float.NEGATIVE_INFINITY, 2.01f, -2.01f)) {
            assertThrows(IllegalArgumentException::class.java) {
                RobotProtocol.encode(moving.copy(forwardVelocityMps = value), limits)
            }
        }
        for (value in listOf(Float.NaN, Float.POSITIVE_INFINITY, Float.NEGATIVE_INFINITY, 1.01f, -1.01f)) {
            assertThrows(IllegalArgumentException::class.java) {
                RobotProtocol.encode(moving.copy(yawRateRadS = value), limits)
            }
        }
    }

    // Each authorization intent flag is required independently for nonzero motion.
    @Test
    fun motionRequiresArmAndDeadman() {
        for (command in listOf(moving.copy(armRequested = false), moving.copy(deadmanActive = false))) {
            assertThrows(IllegalArgumentException::class.java) { RobotProtocol.encode(command, limits) }
        }
    }

    // Serialization cannot truncate invalid identifiers or exceed the lease policy.
    @Test
    fun rejectsBadLeasesAndSequences() {
        for (lease in listOf(0, -1, 201, 65535)) {
            assertThrows(IllegalArgumentException::class.java) {
                RobotProtocol.encode(moving.copy(leaseMs = lease), limits)
            }
        }
        for (sequence in listOf(-1, 65536)) {
            assertThrows(IllegalArgumentException::class.java) {
                RobotProtocol.encode(moving.copy(sequence = sequence), limits)
            }
        }
        assertThrows(IllegalArgumentException::class.java) {
            RobotProtocol.encode(moving, limits.copy(maxLeaseMs = 100))
        }
    }

    // Invalid configuration itself must fail, even when a command appears well formed.
    @Test
    fun rejectsInvalidLimitConfiguration() {
        for (invalid in listOf(limits.copy(maxForwardVelocityMps = -1f),
            limits.copy(maxForwardVelocityMps = Float.NaN),
            limits.copy(maxYawRateRadS = -1f),
            limits.copy(maxYawRateRadS = Float.POSITIVE_INFINITY),
            limits.copy(maxLeaseMs = 0), limits.copy(maxLeaseMs = 201))) {
            assertThrows(IllegalArgumentException::class.java) { RobotProtocol.encode(moving, invalid) }
        }
    }

    // Valid extremes are inclusive; uint16 maximum survives signed JVM short conversion.
    @Test
    fun acceptsInclusiveMotionAndWireBoundaries() {
        for (sequence in listOf(0, 65535)) {
            for (lease in listOf(1, 200)) {
                assertEquals(15, RobotProtocol.encode(moving.copy(sequence = sequence,
                    forwardVelocityMps = -2f, yawRateRadS = 1f, leaseMs = lease), limits).size)
            }
        }
    }
}
