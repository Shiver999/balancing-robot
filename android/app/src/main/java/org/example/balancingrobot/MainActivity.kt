package org.example.balancingrobot

import android.app.Activity
import android.os.Bundle
import android.view.Gravity
import android.view.MotionEvent
import android.view.View
import android.widget.Button
import android.widget.LinearLayout
import android.widget.TextView
import org.example.balancingrobot.connection.BleRobotConnection
import org.example.balancingrobot.connection.RobotConnection
import org.example.balancingrobot.protocol.ControlCommand

/** Scaffold screen: transport is unavailable and all actuator controls stay disabled. */
class MainActivity : Activity() {
    // Depend on the transport interface so a future verified BLE implementation can replace it.
    private val connection: RobotConnection = BleRobotConnection()
    private var sequence = 0
    private lateinit var statusText: TextView
    private lateinit var armButton: Button
    private lateinit var driveButton: Button

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        buildUi()
    }

    /** Construct a small native UI; labels/status expose the current scaffold limitations. */
    private fun buildUi() {
        val root = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            gravity = Gravity.CENTER
            setPadding(32, 32, 32, 32)
        }
        statusText = TextView(this).apply {
            text = "DISCONNECTED — scaffold only; motors cannot be controlled"
            textSize = 18f
            gravity = Gravity.CENTER
        }
        val connectButton = Button(this).apply {
            text = "Connect over BLE"
            setOnClickListener { connection.connect { statusText.text = it } }
        }
        armButton = Button(this).apply {
            text = "Arm (unavailable)"
            isEnabled = false
            setOnClickListener { sendZeroSetpoint(arm = true, deadman = false) }
        }
        // Future hold semantics: both lift and cancellation release arm/dead-man intent.
        // This disabled button has no periodic lease refresh and cannot drive hardware.
        driveButton = Button(this).apply {
            text = "Hold to drive (unavailable)"
            isEnabled = false
            setOnTouchListener { _: View, event: MotionEvent ->
                if (event.action == MotionEvent.ACTION_DOWN) {
                    sendZeroSetpoint(arm = true, deadman = true)
                    true
                } else if (event.action == MotionEvent.ACTION_UP || event.action == MotionEvent.ACTION_CANCEL) {
                    sendZeroSetpoint(arm = false, deadman = false)
                    true
                } else {
                    true
                }
            }
        }
        val stopButton = Button(this).apply {
            text = "Disarm"
            isEnabled = false
            setOnClickListener { sendZeroSetpoint(arm = false, deadman = false) }
        }
        root.addView(statusText)
        root.addView(connectButton)
        root.addView(armButton)
        root.addView(driveButton)
        root.addView(stopButton)
        setContentView(root)
    }

    /** Exercise the command boundary with zero motion; this is not an implemented drive loop. */
    private fun sendZeroSetpoint(arm: Boolean, deadman: Boolean) {
        val command = ControlCommand(
            // Keep transmitted identifiers within uint16 while the local counter increments.
            sequence = sequence++ and 0xFFFF,
            forwardVelocityMps = 0f,
            yawRateRadS = 0f,
            armRequested = arm,
            deadmanActive = deadman,
            leaseMs = 200,
        )
        val result = connection.send(command)
        if (result.isFailure) {
            statusText.text = result.exceptionOrNull()?.message ?: "Not connected"
        }
    }

    // Release a future connection on teardown; firmware lease expiry must work independently.
    override fun onDestroy() {
        connection.disconnect()
        super.onDestroy()
    }
}
