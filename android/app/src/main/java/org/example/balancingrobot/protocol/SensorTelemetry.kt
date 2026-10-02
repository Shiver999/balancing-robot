package org.example.balancingrobot.protocol

import java.nio.ByteBuffer
import java.nio.ByteOrder

/** Read-only bench snapshot. Null sensor values represent unavailable measurements. */
data class Vector3(val x: Float, val y: Float, val z: Float)
data class SensorTelemetry(
    val sequence: Int, val timestampUs: Long, val imuId: Int,
    val acceleration: Vector3?, val gyro: Vector3?,
    val leftAngle: Float?, val rightAngle: Float?,
    val leftSpeed: Float?, val rightSpeed: Float?,
    val leftRaw: Int?, val rightRaw: Int?,
)

object TelemetryProtocol {
    const val PACKET_SIZE = 60
    const val MIN_MTU = PACKET_SIZE + 3

    /** Match firmware telemetry.cpp; reject incompatible/truncated packets before display. */
    fun decode(bytes: ByteArray): SensorTelemetry {
        require(bytes.size == PACKET_SIZE) { "Expected 60-byte sensor telemetry" }
        val b = ByteBuffer.wrap(bytes).order(ByteOrder.LITTLE_ENDIAN)
        require(b.get().toInt() == 1 && b.get().toInt() == 2) { "Unsupported telemetry version/type" }
        val sequence = b.short.toInt() and 0xffff
        val flags = b.get().toInt() and 0xff
        val id = b.get().toInt() and 0xff
        require(flags and 0xf0 == 0 && flags and 8 != 0) { "Unsupported bench status" }
        require(flags and 4 == 0 || flags and 2 != 0) { "Velocity requires valid encoders" }
        require(b.short.toInt() == 0) { "Reserved fields must be zero" }
        val time = b.long
        require(time >= 0) { "Invalid robot timestamp" }
        val values = FloatArray(10) { b.float }
        require(values.all { it.isFinite() }) { "Nonfinite telemetry" }
        val leftRaw = b.short.toInt() and 0xffff
        val rightRaw = b.short.toInt() and 0xffff
        val imuValid = flags and 1 != 0
        val wheelsValid = flags and 2 != 0
        val speedValid = flags and 4 != 0
        require(!imuValid || id == 0x70 || id == 0x71) { "Unsupported IMU identity" }
        require(!wheelsValid || (leftRaw < 16384 && rightRaw < 16384 &&
            values[6] >= 0f && values[6] < 6.284f && values[7] >= 0f && values[7] < 6.284f)) { "Invalid encoder angles" }
        return SensorTelemetry(sequence, time, id,
            if (imuValid) Vector3(values[0], values[1], values[2]) else null,
            if (imuValid) Vector3(values[3], values[4], values[5]) else null,
            if (wheelsValid) values[6] else null, if (wheelsValid) values[7] else null,
            if (speedValid) values[8] else null, if (speedValid) values[9] else null,
            if (wheelsValid) leftRaw else null, if (wheelsValid) rightRaw else null)
    }
}

/** Single-session stream validation with phone-local freshness, independent of robot clock. */
class TelemetryStream {
    var latest: SensorTelemetry? = null
        private set
    private var receivedAtMs: Long? = null
    fun reset() { latest = null; receivedAtMs = null }
    fun accept(packet: ByteArray, nowMs: Long): SensorTelemetry {
        require(nowMs >= 0)
        val next = TelemetryProtocol.decode(packet)
        latest?.let { prior ->
            val distance = (next.sequence - prior.sequence) and 0xffff
            require(distance in 1..32767 && next.timestampUs > prior.timestampUs) { "Repeated or out-of-order telemetry" }
        }
        receivedAtMs?.let { require(nowMs >= it) }
        latest = next; receivedAtMs = nowMs
        return next
    }
    fun isFresh(nowMs: Long): Boolean = receivedAtMs?.let { nowMs >= it && nowMs - it < 1500 } ?: false
}
