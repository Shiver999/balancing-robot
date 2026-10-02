package org.example.balancingrobot

import android.Manifest
import android.app.Activity
import android.bluetooth.BluetoothAdapter
import android.content.Intent
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import android.widget.*
import android.view.View
import android.view.Gravity
import android.view.WindowInsets
import org.example.balancingrobot.ui.JoystickView
import org.example.balancingrobot.control.JoystickInput
import org.example.balancingrobot.connection.*
import org.example.balancingrobot.protocol.SensorTelemetry
import org.example.balancingrobot.protocol.Vector3
import java.util.Locale

/** Foreground sensor viewer. Leaving the screen closes BLE and invalidates displayed data. */
class MainActivity : Activity(), RobotConnection.Listener {
    private lateinit var connection: RobotConnection
    private lateinit var state: TextView
    private lateinit var sensorText: TextView
    private lateinit var deviceList: LinearLayout
    private lateinit var discovery: View
    private lateinit var connected: FrameLayout
    private lateinit var connectedState: TextView
    private lateinit var joystick: JoystickView
    private lateinit var intentText: TextView
    private var active = false
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        connection = BleRobotConnection(this, this)
        val root = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL; setPadding(32, 32, 32, 32) }
        fun label(text: String, size: Float = 16f) = TextView(this).apply { this.text = text; textSize = size; setPadding(0, 12, 0, 12) }
        root.addView(label("Balancing Robot", 26f))
        root.addView(label("Sensor bench • Motor output disabled"))
        state = label("Disconnected"); root.addView(state)
        root.addView(Button(this).apply { text = "Find robot"; setOnClickListener { requestScan() } })
        root.addView(Button(this).apply { text = "Enable Bluetooth"; setOnClickListener {
            if (!permissionsGranted()) { requestScan(); return@setOnClickListener }
            try {
                startActivityForResult(Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE), 2)
            } catch (_: SecurityException) {
                state.text = "Bluetooth permission was revoked — tap Find robot to request it"
            } catch (_: Exception) {
                state.text = "Enable Bluetooth in phone Settings"
            }
        } })
        root.addView(Button(this).apply { text = "Disconnect"; setOnClickListener { connection.disconnect() } })
        deviceList = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL }
        root.addView(deviceList)
        discovery = ScrollView(this).apply { addView(root) }
        connected = FrameLayout(this)
        val header = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL; setPadding(24, 8, 24, 0) }
        val bar = LinearLayout(this).apply { gravity = Gravity.CENTER_VERTICAL }
        bar.addView(label("Balancing Robot", 22f), LinearLayout.LayoutParams(0, -2, 1f))
        bar.addView(Button(this).apply { text = "Disconnect"; setOnClickListener { joystick.reset(); connection.disconnect() } })
        header.addView(bar)
        connectedState = label("Connected", 14f); header.addView(connectedState)
        connected.addView(header, FrameLayout.LayoutParams(-1, -2, Gravity.TOP))
        joystick = JoystickView(this).apply {
            onInput = { updateIntent(it) }
            isEnabled = false
        }
        connected.addView(joystick, FrameLayout.LayoutParams(1, 1, Gravity.CENTER))
        val footer = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL; setPadding(24, 0, 24, 8) }
        intentText = label("Motion preview • Motor output disabled", 14f); footer.addView(intentText)
        sensorText = label("Sensor readings unavailable", 14f)
        footer.addView(ScrollView(this).apply { addView(sensorText) }, LinearLayout.LayoutParams(-1, 0, 1f))
        connected.addView(footer, FrameLayout.LayoutParams(-1, 1, Gravity.BOTTOM))
        // Keep the circle at the screen center, fitting between header and telemetry on rotation.
        val resizeJoystick = {
            val density = resources.displayMetrics.density
            val footerHeight = minOf((190 * density).toInt(), (connected.height * 0.28f).toInt())
            if (footer.layoutParams.height != footerHeight) {
                footer.layoutParams = (footer.layoutParams as FrameLayout.LayoutParams).apply { height = footerHeight }
            }
            val gap = 12 * density
            val halfSpace = minOf(connected.height / 2f - header.height - gap,
                connected.height / 2f - footerHeight - gap).coerceAtLeast(0f)
            val size = minOf(300 * density, connected.width - 32 * density, halfSpace * 2).toInt().coerceAtLeast(1)
            if (joystick.layoutParams.width != size) {
                joystick.reset()
                joystick.layoutParams = FrameLayout.LayoutParams(size, size, Gravity.CENTER)
            }
        }
        connected.addOnLayoutChangeListener { _, _, _, _, _, _, _, _, _ -> resizeJoystick() }
        header.addOnLayoutChangeListener { _, _, _, _, _, _, _, _, _ -> resizeJoystick() }
        val screens = FrameLayout(this).apply {
            addView(discovery, FrameLayout.LayoutParams(-1, -1))
            addView(connected, FrameLayout.LayoutParams(-1, -1))
        }
        // Respect status/navigation bars, including Android 15's enforced edge-to-edge layout.
        screens.setOnApplyWindowInsetsListener { view, insets ->
            if (Build.VERSION.SDK_INT >= 30) {
                val bars = insets.getInsets(WindowInsets.Type.systemBars() or WindowInsets.Type.displayCutout())
                view.setPadding(bars.left, bars.top, bars.right, bars.bottom)
            } else {
                @Suppress("DEPRECATION")
                view.setPadding(insets.systemWindowInsetLeft, insets.systemWindowInsetTop,
                    insets.systemWindowInsetRight, insets.systemWindowInsetBottom)
            }
            insets
        }
        connected.visibility = View.GONE
        setContentView(screens)
    }
    private fun updateIntent(input: JoystickInput) {
        // Preview only: physical velocity/yaw limits and firmware command admission are not ready.
        intentText.text = String.format(Locale.US,
            "Preview: forward %+.0f%% • turn %+.0f%% • intensity %.0f%%\nMotor output disabled",
            input.forward * 100, input.turn * 100, input.magnitude * 100)
    }

    private fun permissions(): Array<String> = if (Build.VERSION.SDK_INT >= 31) {
        arrayOf(Manifest.permission.BLUETOOTH_SCAN, Manifest.permission.BLUETOOTH_CONNECT)
    } else arrayOf(Manifest.permission.ACCESS_FINE_LOCATION, Manifest.permission.ACCESS_COARSE_LOCATION)
    private fun permissionsGranted() = permissions().all { checkSelfPermission(it) == PackageManager.PERMISSION_GRANTED }
    private fun requestScan() {
        if (permissionsGranted()) connection.scan() else requestPermissions(permissions(), 1)
    }
    override fun onRequestPermissionsResult(code: Int, permissions: Array<out String>, results: IntArray) {
        super.onRequestPermissionsResult(code, permissions, results)
        if (code == 1 && active) {
            if (permissionsGranted()) connection.scan()
            else state.text = "Permission denied — allow Nearby devices (or Location on Android 8–11) in Settings"
        }
    }
    override fun onState(message: String, dataFresh: Boolean) {
        state.text = message
        connectedState.text = message
        val showConnected = connection.isConnected
        discovery.visibility = if (showConnected) View.GONE else View.VISIBLE
        connected.visibility = if (showConnected) View.VISIBLE else View.GONE
        // Stale telemetry or disconnect also cancels any held preview gesture.
        if (!showConnected || !dataFresh) joystick.reset()
        joystick.isEnabled = showConnected && dataFresh
        if (!dataFresh) sensorText.text = "Sensor readings unavailable"
    }
    override fun onDevices(devices: List<RobotDevice>) {
        deviceList.removeAllViews()
        devices.forEach { device -> deviceList.addView(Button(this).apply {
            text = "${device.name}\n${device.address}"
            setOnClickListener {
                if (permissionsGranted()) { deviceList.removeAllViews(); connection.connect(device.address) }
                else requestScan()
            }
        }) }
    }
    private fun number(value: Float?): String = value?.let { String.format(Locale.US, "%.3f", it) } ?: "unavailable"
    private fun vector(value: Vector3?): String = value?.let { "${number(it.x)}, ${number(it.y)}, ${number(it.z)}" } ?: "unavailable"
    override fun onTelemetry(snapshot: SensorTelemetry) {
        sensorText.text = "IMU ${if (snapshot.acceleration != null) "0x%02X".format(snapshot.imuId) else "unavailable"}\n" +
            "Acceleration XYZ (m/s²): ${vector(snapshot.acceleration)}\nGyro XYZ (rad/s): ${vector(snapshot.gyro)}\n\n" +
            "Left wheel\nAngle (rad): ${number(snapshot.leftAngle)}\nSpeed (rad/s): ${number(snapshot.leftSpeed)}\nRaw: ${snapshot.leftRaw ?: "unavailable"}\n\n" +
            "Right wheel\nAngle (rad): ${number(snapshot.rightAngle)}\nSpeed (rad/s): ${number(snapshot.rightSpeed)}\nRaw: ${snapshot.rightRaw ?: "unavailable"}\n\n" +
            "Packet ${snapshot.sequence} • Robot time ${snapshot.timestampUs / 1000} ms"
    }
    override fun onStart() { super.onStart(); active = true }
    override fun onStop() {
        active = false; joystick.reset(); connection.disconnect(); deviceList.removeAllViews(); super.onStop()
    }
}
