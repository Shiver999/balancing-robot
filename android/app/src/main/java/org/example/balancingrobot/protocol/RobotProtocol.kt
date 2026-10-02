package org.example.balancingrobot.protocol

import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.util.UUID

/** Wire-format constants shared with firmware/main/comms/protocol.cpp. */
object RobotProtocol {
    const val VERSION: Byte = 1
    const val CONTROL_MESSAGE_TYPE: Byte = 1
    const val CONTROL_PACKET_SIZE = 15
    const val FLAG_ARM_REQUEST = 1
    const val FLAG_DEADMAN_ACTIVE = 1 shl 1
    const val MAX_LEASE_MS = 200 // Scaffold policy; requires later BLE bench validation.

    // Provisional UUIDs; keep synchronized with the ESP32 GATT service when it is implemented.
    val SERVICE_UUID: UUID = UUID.fromString("7f510001-1b15-4f6c-9a2a-5b2e6b0a0100")
    val CONTROL_UUID: UUID = UUID.fromString("7f510002-1b15-4f6c-9a2a-5b2e6b0a0100")
    val STATUS_UUID: UUID = UUID.fromString("7f510003-1b15-4f6c-9a2a-5b2e6b0a0100")

    /** Validate stateless admission policy, then serialize exactly 15 little-endian bytes.
     * Replay ordering and receive-time lease enforcement remain firmware responsibilities. */
    fun encode(command: ControlCommand, limits: CommandLimits = CommandLimits()): ByteArray {
        require(command.sequence in 0..0xFFFF) { "sequence must fit uint16" }
        require(command.forwardVelocityMps.isFinite())
        require(command.yawRateRadS.isFinite())
        require(limits.maxForwardVelocityMps.isFinite() && limits.maxForwardVelocityMps >= 0f)
        require(limits.maxYawRateRadS.isFinite() && limits.maxYawRateRadS >= 0f)
        require(limits.maxLeaseMs in 1..MAX_LEASE_MS)
        require(kotlin.math.abs(command.forwardVelocityMps) <= limits.maxForwardVelocityMps)
        require(kotlin.math.abs(command.yawRateRadS) <= limits.maxYawRateRadS)
        require(command.leaseMs in 1..limits.maxLeaseMs)
        require((command.forwardVelocityMps == 0f && command.yawRateRadS == 0f) ||
            (command.armRequested && command.deadmanActive)) {
            "motion requires arm and dead-man flags"
        }

        // Pack only the two defined flags; firmware rejects unknown bits.
        val flags = (if (command.armRequested) FLAG_ARM_REQUEST else 0) or
            (if (command.deadmanActive) FLAG_DEADMAN_ACTIVE else 0)
        // Header(2), sequence(2), SI floats(8), flags(1), lease milliseconds(2).
        // Signed JVM shorts preserve the same 16-bit wire pattern as firmware uint16.
        return ByteBuffer.allocate(CONTROL_PACKET_SIZE)
            .order(ByteOrder.LITTLE_ENDIAN)
            .put(VERSION)
            .put(CONTROL_MESSAGE_TYPE)
            .putShort(command.sequence.toShort())
            .putFloat(command.forwardVelocityMps)
            .putFloat(command.yawRateRadS)
            .put(flags.toByte())
            .putShort(command.leaseMs.toShort())
            .array()
    }
}

// Zero motion until verified limits are explicitly provided. Firmware remains
// authoritative and independently rejects requests outside its own limits.
data class CommandLimits(
    val maxForwardVelocityMps: Float = 0f,
    val maxYawRateRadS: Float = 0f,
    val maxLeaseMs: Int = RobotProtocol.MAX_LEASE_MS,
)

/** Immutable requested motion in SI units. Flags are intent, not proof that motors are armed. */
data class ControlCommand(
    val sequence: Int,
    val forwardVelocityMps: Float,
    val yawRateRadS: Float,
    val armRequested: Boolean,
    val deadmanActive: Boolean,
    val leaseMs: Int,
)
