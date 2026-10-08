package com.impact.meter.ble

import android.annotation.SuppressLint
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCallback
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothManager
import android.bluetooth.BluetoothProfile
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanFilter
import android.bluetooth.le.ScanResult
import android.bluetooth.le.ScanSettings
import android.content.Context
import android.os.Build
import android.os.Handler
import android.os.Looper
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.asStateFlow
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.util.UUID

@SuppressLint("MissingPermission")
class BleManager(private val context: Context) {
    companion object {
        val SERVICE_UUID: UUID = UUID.fromString("00001820-0000-1000-8000-00805f9b34fb")
        val CHAR_PING_UUID: UUID = UUID.fromString("00002A91-0000-1000-8000-00805f9b34fb")
        val CHAR_TELEMETRY_UUID: UUID = UUID.fromString("00002A90-0000-1000-8000-00805f9b34fb")
    }

    private val bluetoothManager = context.getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
    private val adapter = bluetoothManager.adapter
    private var gatt: BluetoothGatt? = null
    private var pingChar: BluetoothGattCharacteristic? = null

    private val _connectionState = MutableStateFlow("Disconnected")
    val connectionState = _connectionState.asStateFlow()

    private val _isScanning = MutableStateFlow(false)
    val isScanning = _isScanning.asStateFlow()

    private val _lastPingRttMs = MutableStateFlow<Long?>(null)
    val lastPingRttMs = _lastPingRttMs.asStateFlow()

    private val _pingCount = MutableStateFlow(0)
    val pingCount = _pingCount.asStateFlow()

    private val _logMessages = MutableStateFlow<List<String>>(emptyList())
    val logMessages = _logMessages.asStateFlow()

    private var pingStartMs: Long = 0

    private val scanCallback = object : ScanCallback() {
        override fun onScanResult(callbackType: Int, result: ScanResult) {
            stopScan()
            addLog("Found device: ${result.device.name} (${result.rssi} dBm)")
            connect(result.device)
        }
    }

    fun addLog(msg: String) {
        _logMessages.value = listOf(msg) + _logMessages.value.take(49)
    }

    fun startScan() {
        val scanner = adapter?.bluetoothLeScanner ?: return
        _isScanning.value = true
        _connectionState.value = "Scanning for ImpactMaster..."
        addLog("Scanning for ImpactMaster...")

        val filter = ScanFilter.Builder().setDeviceName("ImpactMaster").build()
        val settings = ScanSettings.Builder().setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY).build()

        scanner.startScan(listOf(filter), settings, scanCallback)

        Handler(Looper.getMainLooper()).postDelayed({
            if (_isScanning.value) stopScan()
        }, 8000)
    }

    fun stopScan() {
        if (_isScanning.value) {
            adapter?.bluetoothLeScanner?.stopScan(scanCallback)
            _isScanning.value = false
        }
    }

    private fun connect(device: BluetoothDevice) {
        _connectionState.value = "Connecting..."
        gatt = device.connectGatt(context, false, object : BluetoothGattCallback() {
            override fun onConnectionStateChange(g: BluetoothGatt, status: Int, newState: Int) {
                if (newState == BluetoothProfile.STATE_CONNECTED) {
                    _connectionState.value = "Connected. Discovering..."
                    g.discoverServices()
                } else {
                    _connectionState.value = "Disconnected"
                }
            }

            override fun onServicesDiscovered(g: BluetoothGatt, status: Int) {
                val service = g.getService(SERVICE_UUID)
                pingChar = service?.getCharacteristic(CHAR_PING_UUID)
                _connectionState.value = "Ready (ImpactMaster Connected)"
                addLog("GATT Services discovered!")
            }

            override fun onCharacteristicWrite(g: BluetoothGatt, characteristic: BluetoothGattCharacteristic, status: Int) {
                if (characteristic.uuid == CHAR_PING_UUID) {
                    g.readCharacteristic(characteristic)
                }
            }

            @Deprecated("Deprecated in Java")
            override fun onCharacteristicRead(g: BluetoothGatt, characteristic: BluetoothGattCharacteristic, status: Int) {
                if (Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU && characteristic.uuid == CHAR_PING_UUID) {
                    handlePingAck()
                }
            }

            override fun onCharacteristicRead(
                g: BluetoothGatt,
                characteristic: BluetoothGattCharacteristic,
                value: ByteArray,
                status: Int
            ) {
                if (characteristic.uuid == CHAR_PING_UUID) {
                    handlePingAck()
                }
            }
        })
    }

    private fun handlePingAck() {
        val rtt = System.currentTimeMillis() - pingStartMs
        _lastPingRttMs.value = rtt
        _pingCount.value += 1
        addLog("🏓 Ping ACK received! RTT = $rtt ms")
    }

    fun disconnect() {
        gatt?.disconnect()
        gatt?.close()
        gatt = null
        _connectionState.value = "Disconnected"
    }

    fun sendPing() {
        val g = gatt ?: return
        val p = pingChar ?: return
        pingStartMs = System.currentTimeMillis()
        val data = ByteBuffer.allocate(5).order(ByteOrder.LITTLE_ENDIAN)
            .put(0x01.toByte())
            .putInt((pingStartMs and 0xFFFFFFFFL).toInt())
            .array()

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            g.writeCharacteristic(p, data, BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT)
        } else {
            @Suppress("DEPRECATION")
            p.value = data
            @Suppress("DEPRECATION")
            g.writeCharacteristic(p)
        }
        addLog("Sent Ping to Master...")
    }
}
