package org.example.balancingrobot.connection

import org.example.balancingrobot.protocol.ControlCommand

interface RobotConnection {
    val isConnected: Boolean
    fun connect(onStateChanged: (String) -> Unit)
    fun send(command: ControlCommand): Result<Unit>
    fun disconnect()
}

/**
 * Deliberately nonfunctional BLE adapter scaffold. It never reports a successful
 * connection and never transmits commands until GATT discovery, pairing, and
 * the firmware service are implemented and tested.
 */
class BleRobotConnection : RobotConnection {
    override val isConnected: Boolean = false

    override fun connect(onStateChanged: (String) -> Unit) {
        onStateChanged("BLE transport not implemented — robot control unavailable")
    }

    override fun send(command: ControlCommand): Result<Unit> =
        Result.failure(IllegalStateException("BLE transport is not implemented"))

    override fun disconnect() = Unit
}
