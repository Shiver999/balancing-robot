package org.example.balancingrobot.connection

import org.example.balancingrobot.protocol.ControlCommand
import org.example.balancingrobot.protocol.SensorTelemetry

data class RobotDevice(val address: String, val name: String)
/** All listener calls arrive on the main thread. Telemetry is available only after CCCD success. */
interface RobotConnection {
    val isConnected: Boolean
    interface Listener {
        fun onState(message: String, dataFresh: Boolean = false)
        fun onDevices(devices: List<RobotDevice>)
        fun onTelemetry(snapshot: SensorTelemetry)
    }
    fun scan()
    fun connect(address: String)
    fun send(command: ControlCommand): Result<Unit>
    fun disconnect()
}
