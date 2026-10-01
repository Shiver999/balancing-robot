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

class MainActivity : Activity() {
    private val connection: RobotConnection = BleRobotConnection()
    private var sequence = 0
    private lateinit var statusText: TextView
    private lateinit var armButton: Button
    private lateinit var driveButton: Button

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        buildUi()
    }

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

    private fun sendZeroSetpoint(arm: Boolean, deadman: Boolean) {
        val command = ControlCommand(
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

    override fun onDestroy() {
        connection.disconnect()
        super.onDestroy()
    }
}
