package org.example.balancingrobot.connection

import android.annotation.SuppressLint
import android.bluetooth.*
import android.bluetooth.le.*
import android.content.Context
import android.os.Build
import android.os.Handler
import android.os.Looper
import android.os.ParcelUuid
import android.os.SystemClock
import org.example.balancingrobot.protocol.*
import java.util.UUID

/** Foreground, single-session BLE client. One asynchronous GATT operation at a time. */
@SuppressLint("MissingPermission") // Activity obtains version-appropriate runtime permissions before entry.
class BleRobotConnection(context: Context, private val listener: RobotConnection.Listener) : RobotConnection {
    private val context = context.applicationContext
    private val adapter = context.getSystemService(BluetoothManager::class.java)?.adapter
    private val handler = Handler(Looper.getMainLooper())
    private var gatt: BluetoothGatt? = null
    private var scanner: BluetoothLeScanner? = null
    private var scanCallback: ScanCallback? = null
    private var deadline: Runnable? = null
    private var staleCheck: Runnable? = null
    private val devices = linkedMapOf<String, RobotDevice>()
    private val stream = TelemetryStream()
    private var statusCharacteristic: BluetoothGattCharacteristic? = null
    private var stage = "idle"
    override var isConnected = false
        private set
    private val cccd = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")

    private fun cancelDeadline() { deadline?.let(handler::removeCallbacks); deadline = null }
    private fun timeout(message: String, ms: Long = 15000) {
        cancelDeadline()
        deadline = Runnable { fail(message) }.also { handler.postDelayed(it, ms) }
    }
    private fun stopScan() {
        scanCallback?.let { callback -> runCatching { scanner?.stopScan(callback) } }
        scanCallback = null; scanner = null
    }
    private fun closeSession() {
        cancelDeadline(); stopScan()
        staleCheck?.let(handler::removeCallbacks); staleCheck = null
        val old = gatt; gatt = null
        runCatching { old?.disconnect() }; runCatching { old?.close() }
        statusCharacteristic = null; isConnected = false; stream.reset(); stage = "idle"
    }
    private fun fail(message: String) { closeSession(); listener.onState(message) }

    override fun scan() {
        closeSession(); devices.clear(); listener.onDevices(emptyList())
        try {
            if (adapter?.isEnabled != true) { fail("Turn Bluetooth on, then scan again"); return }
            scanner = adapter.bluetoothLeScanner
            if (scanner == null) { fail("BLE scanning unavailable"); return }
            stage = "scanning"
            val callback = object : ScanCallback() {
                override fun onScanResult(type: Int, result: ScanResult) {
                    handler.post {
                        if (scanCallback !== this) return@post
                        try {
                            val address = result.device.address
                            devices[address] = RobotDevice(address, result.scanRecord?.deviceName ?: "Balancing Robot")
                            listener.onDevices(devices.values.toList())
                        } catch (_: SecurityException) { fail("Bluetooth permission was revoked") }
                    }
                }
                override fun onScanFailed(code: Int) { handler.post { if (scanCallback === this) fail("Scan failed ($code); try again") } }
            }
            scanCallback = callback
            scanner!!.startScan(listOf(ScanFilter.Builder().setServiceUuid(ParcelUuid(RobotProtocol.SERVICE_UUID)).build()),
                ScanSettings.Builder().setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY).build(), callback)
            listener.onState("Scanning for sensor benches…")
            // Finish scanning without discarding the discovered device list.
            deadline = Runnable {
                stopScan(); stage = "idle"
                listener.onState(if (devices.isEmpty()) "No robot found — check BLE bench firmware and power" else "Select a robot below")
            }.also { handler.postDelayed(it, 10000) }
        } catch (_: SecurityException) { fail("Bluetooth permission is required") }
        catch (e: Exception) { fail("Scan unavailable: ${e.message}") }
    }

    override fun connect(address: String) {
        closeSession()
        try {
            if (adapter?.isEnabled != true) { fail("Turn Bluetooth on first"); return }
            stage = "connecting"; listener.onState("Connecting…"); timeout("Connection timed out — try again")
            gatt = adapter.getRemoteDevice(address).connectGatt(context, false, callbacks, BluetoothDevice.TRANSPORT_LE)
            if (gatt == null) fail("Could not start BLE connection")
        } catch (_: SecurityException) { fail("Bluetooth permission is required") }
        catch (e: Exception) { fail("Connection failed: ${e.message}") }
    }

    // Framework callbacks can run on binder threads. Ignore callbacks from abandoned sessions.
    private fun dispatch(target: BluetoothGatt, action: () -> Unit) {
        handler.post {
            if (gatt !== target) return@post
            try { action() } catch (_: SecurityException) { fail("Bluetooth permission was revoked") }
            catch (e: Exception) { fail("BLE error: ${e.message}") }
        }
    }
    private val callbacks = object : BluetoothGattCallback() {
        override fun onConnectionStateChange(g: BluetoothGatt, status: Int, state: Int) = dispatch(g) {
            if (status != BluetoothGatt.GATT_SUCCESS || state == BluetoothProfile.STATE_DISCONNECTED) {
                fail("Disconnected ($status) — scan to reconnect")
            } else if (state == BluetoothProfile.STATE_CONNECTED && stage == "connecting") {
                stage = "mtu"; listener.onState("Negotiating sensor packet size…"); timeout("Packet-size negotiation timed out")
                if (!g.requestMtu(96)) fail("Cannot request telemetry packet size")
            }
        }
        override fun onMtuChanged(g: BluetoothGatt, mtu: Int, status: Int) = dispatch(g) {
            if (stage != "mtu") return@dispatch
            if (status != BluetoothGatt.GATT_SUCCESS || mtu < TelemetryProtocol.MIN_MTU) { fail("BLE MTU too small for sensor telemetry"); return@dispatch }
            stage = "services"; listener.onState("Checking robot service…"); timeout("Service discovery timed out")
            if (!g.discoverServices()) fail("Cannot discover robot service")
        }
        override fun onServicesDiscovered(g: BluetoothGatt, status: Int) = dispatch(g) {
            if (stage != "services") return@dispatch
            val characteristic = g.getService(RobotProtocol.SERVICE_UUID)?.getCharacteristic(RobotProtocol.STATUS_UUID)
            if (status != BluetoothGatt.GATT_SUCCESS || characteristic == null || characteristic.properties and BluetoothGattCharacteristic.PROPERTY_NOTIFY == 0) {
                fail("Compatible sensor service not found — flash BLE bench firmware"); return@dispatch
            }
            val descriptor = characteristic.getDescriptor(cccd)
            if (descriptor == null || !g.setCharacteristicNotification(characteristic, true)) { fail("Cannot enable telemetry notifications"); return@dispatch }
            statusCharacteristic = characteristic; stage = "subscribe"; timeout("Telemetry subscription timed out")
            val accepted = if (Build.VERSION.SDK_INT >= 33) {
                g.writeDescriptor(descriptor, BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE) == BluetoothStatusCodes.SUCCESS
            } else {
                @Suppress("DEPRECATION")
                descriptor.value = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
                @Suppress("DEPRECATION")
                g.writeDescriptor(descriptor)
            }
            if (!accepted) fail("Cannot start telemetry subscription")
        }
        override fun onDescriptorWrite(g: BluetoothGatt, descriptor: BluetoothGattDescriptor, status: Int) = dispatch(g) {
            if (stage != "subscribe" || descriptor.uuid != cccd) return@dispatch
            if (status != BluetoothGatt.GATT_SUCCESS) { fail("Telemetry subscription rejected ($status)"); return@dispatch }
            cancelDeadline(); stage = "stream"; isConnected = true
            listener.onState("Connected — waiting for sensor data")
            startFreshnessCheck()
        }
        override fun onCharacteristicChanged(g: BluetoothGatt, characteristic: BluetoothGattCharacteristic, value: ByteArray) {
            if (characteristic.uuid == RobotProtocol.STATUS_UUID) receive(g, value.copyOf())
        }
        @Deprecated("Legacy callback for Android 8–12")
        override fun onCharacteristicChanged(g: BluetoothGatt, characteristic: BluetoothGattCharacteristic) {
            if (Build.VERSION.SDK_INT < 33 && characteristic.uuid == RobotProtocol.STATUS_UUID) {
                @Suppress("DEPRECATION")
                val bytes = characteristic.value?.copyOf() ?: return
                receive(g, bytes)
            }
        }
    }
    private fun receive(g: BluetoothGatt, bytes: ByteArray) = dispatch(g) {
        if (stage != "stream") return@dispatch
        val snapshot = stream.accept(bytes, SystemClock.elapsedRealtime())
        listener.onTelemetry(snapshot)
        listener.onState("Connected — live sensor data", dataFresh = true)
    }
    private fun startFreshnessCheck() {
        val check = object : Runnable {
            override fun run() {
                if (!isConnected) return
                if (!stream.isFresh(SystemClock.elapsedRealtime())) listener.onState("Connected — sensor data stale/unavailable")
                handler.postDelayed(this, 500)
            }
        }
        staleCheck = check; handler.postDelayed(check, 1500)
    }
    // The bench deliberately exposes no writable command characteristic.
    override fun send(command: ControlCommand): Result<Unit> = Result.failure(IllegalStateException("Motor commands unavailable in sensor bench"))
    override fun disconnect() { closeSession(); listener.onState("Disconnected") }
}
