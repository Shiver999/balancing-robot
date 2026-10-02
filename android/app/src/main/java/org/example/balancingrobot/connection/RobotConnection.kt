package org.example.balancingrobot.connection

import org.example.balancingrobot.protocol.ControlCommand

/** Transport boundary; acceptance by send must not be treated as hardware arming.
 * A future asynchronous adapter must dispatch UI callbacks on the main thread. */
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

    // Fail explicitly rather than allowing the UI to infer a successful robot command.
    override fun send(command: ControlCommand): Result<Unit> =
        Result.failure(IllegalStateException("BLE transport is not implemented"))

    // No resources are allocated by this placeholder, so teardown is intentionally inert.
    override fun disconnect() = Unit
}
