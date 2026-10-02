package org.example.balancingrobot.protocol

import org.junit.Assert.*
import org.junit.Test
import java.nio.ByteBuffer
import java.nio.ByteOrder

/** Same fixed vector as firmware telemetry_tests.cpp; tests do not depend on a BLE radio. */
class SensorTelemetryTest {
    private val golden = "0102ffff0f7100004e61bc00000000000000803f000000c000004040000080c00000a0400000c0c00000803f00000040000040c0000080407b00c801"
    private fun packet(sequence: Int = 65535, time: Long = 12345678): ByteArray {
        val bytes = golden.chunked(2).map { it.toInt(16).toByte() }.toByteArray()
        ByteBuffer.wrap(bytes).order(ByteOrder.LITTLE_ENDIAN).putShort(2, sequence.toShort()).putLong(8, time)
        return bytes
    }
    @Test fun decodesFirmwareGoldenVector() {
        val sample = TelemetryProtocol.decode(packet())
        assertEquals(65535, sample.sequence); assertEquals(12345678L, sample.timestampUs)
        assertEquals(Vector3(1f, -2f, 3f), sample.acceleration)
        assertEquals(Vector3(-4f, 5f, -6f), sample.gyro)
        assertEquals(-3f, sample.leftSpeed!!, 0f); assertEquals(456, sample.rightRaw)
    }
    @Test fun distinguishesUnavailableAnglesAndVelocity() {
        val bytes = packet(); bytes[4] = 8
        val empty = TelemetryProtocol.decode(bytes)
        assertNull(empty.acceleration); assertNull(empty.leftAngle); assertNull(empty.leftSpeed)
        bytes[4] = 10
        val angles = TelemetryProtocol.decode(bytes)
        assertEquals(1f, angles.leftAngle!!, 0f); assertNull(angles.leftSpeed)
    }
    @Test fun rejectsMalformedPackets() {
        assertThrows(IllegalArgumentException::class.java) { TelemetryProtocol.decode(packet().copyOf(59)) }
        for ((offset, value) in listOf(0 to 2, 1 to 1, 4 to 0x8f, 4 to 7, 4 to 12, 5 to 0xFF, 6 to 1)) {
            val bytes = packet(); bytes[offset] = value.toByte()
            assertThrows(IllegalArgumentException::class.java) { TelemetryProtocol.decode(bytes) }
        }
        for (offset in listOf(16, 28, 40, 48)) {
            val bytes = packet(); ByteBuffer.wrap(bytes).order(ByteOrder.LITTLE_ENDIAN).putFloat(offset, Float.NaN)
            assertThrows(IllegalArgumentException::class.java) { TelemetryProtocol.decode(bytes) }
        }
        assertThrows(IllegalArgumentException::class.java) { TelemetryProtocol.decode(packet(time = -1)) }
        val bytes = packet(); ByteBuffer.wrap(bytes).order(ByteOrder.LITTLE_ENDIAN).putShort(56, 16384)
        assertThrows(IllegalArgumentException::class.java) { TelemetryProtocol.decode(bytes) }
    }
    @Test fun wraparoundAndFreshnessUseLocalTime() {
        val stream = TelemetryStream()
        assertFalse(stream.isFresh(100))
        stream.accept(packet(), 100)
        assertTrue(stream.isFresh(1599)); assertFalse(stream.isFresh(1600)); assertFalse(stream.isFresh(99))
        stream.accept(packet(0, 12345679), 1600)
        assertTrue(stream.isFresh(1600))
        assertThrows(IllegalArgumentException::class.java) { stream.accept(packet(0, 12345679), 2000) }
        assertFalse(stream.isFresh(3100)) // Rejected replay never renews freshness.
        stream.reset(); assertNull(stream.latest); assertFalse(stream.isFresh(3100))
        stream.accept(packet(1, 0), 3100) // A new connection may have a rebooted robot clock.
    }
    @Test fun rejectsOldAmbiguousOrBackwardsSamplesWithoutReplacingLatest() {
        val stream = TelemetryStream(); stream.accept(packet(10, 100), 1000)
        for ((seq, time) in listOf(9 to 101L, 32778 to 101L, 11 to 100L, 11 to 99L)) {
            assertThrows(IllegalArgumentException::class.java) { stream.accept(packet(seq, time), 1100) }
            assertEquals(10, stream.latest!!.sequence)
        }
        assertThrows(IllegalArgumentException::class.java) { stream.accept(packet(11, 101), 999) }
    }
}
