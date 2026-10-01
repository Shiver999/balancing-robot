package org.example.balancingrobot.protocol

import org.junit.Assert.assertArrayEquals
import org.junit.Test

class RobotProtocolTest {
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
}
