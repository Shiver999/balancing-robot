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

    // Provisional UUIDs; keep synchronized with the ESP32 GATT service when it is implemented.
    val SERVICE_UUID: UUID = UUID.fromString("7f510001-1b15-4f6c-9a2a-5b2e6b0a0100")
    val CONTROL_UUID: UUID = UUID.fromString("7f510002-1b15-4f6c-9a2a-5b2e6b0a0100")
    val STATUS_UUID: UUID = UUID.fromString("7f510003-1b15-4f6c-9a2a-5b2e6b0a0100")

    fun encode(command: ControlCommand): ByteArray {
        require(command.sequence in 0..0xFFFF) { "sequence must fit uint16" }
        require(command.forwardVelocityMps.isFinite())
        require(command.yawRateRadS.isFinite())
        require(command.leaseMs in 1..0xFFFF) { "lease must fit uint16 and be non-zero" }

        val flags = (if (command.armRequested) FLAG_ARM_REQUEST else 0) or
            (if (command.deadmanActive) FLAG_DEADMAN_ACTIVE else 0)
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

data class ControlCommand(
    val sequence: Int,
    val forwardVelocityMps: Float,
    val yawRateRadS: Float,
    val armRequested: Boolean,
    val deadmanActive: Boolean,
    val leaseMs: Int,
)
