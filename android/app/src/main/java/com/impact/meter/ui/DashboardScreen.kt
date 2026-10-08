package com.impact.meter.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBar
import androidx.compose.material3.TopAppBarDefaults
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.impact.meter.ble.BleManager

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun DashboardScreen(bleManager: BleManager) {
    val connectionState by bleManager.connectionState.collectAsState()
    val isScanning by bleManager.isScanning.collectAsState()
    val lastRtt by bleManager.lastPingRttMs.collectAsState()
    val pingCount by bleManager.pingCount.collectAsState()
    val logs by bleManager.logMessages.collectAsState()

    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text("Impact Time Meter", fontWeight = FontWeight.Bold) },
                colors = TopAppBarDefaults.topAppBarColors(containerColor = Color(0xFF0F172A), titleContentColor = Color.White)
            )
        },
        containerColor = Color(0xFF020617)
    ) { padding ->
        Column(modifier = Modifier.fillMaxSize().padding(padding).padding(16.dp), verticalArrangement = Arrangement.spacedBy(16.dp)) {
            Card(colors = CardDefaults.cardColors(containerColor = Color(0xFF1E293B)), modifier = Modifier.fillMaxWidth()) {
                Column(modifier = Modifier.padding(16.dp)) {
                    Text("Connection Status", color = Color.Gray, fontSize = 12.sp)
                    Text(connectionState, fontSize = 16.sp, fontWeight = FontWeight.Bold, color = if (connectionState.contains("Ready")) Color(0xFF4ADE80) else Color.White)
                    Spacer(modifier = Modifier.height(12.dp))
                    Button(
                        onClick = { bleManager.startScan() },
                        enabled = !isScanning && !connectionState.contains("Ready"),
                        modifier = Modifier.fillMaxWidth()
                    ) {
                        Text(if (isScanning) "Scanning..." else "Scan Master")
                    }
                }
            }

            Card(colors = CardDefaults.cardColors(containerColor = Color(0xFF1E293B)), modifier = Modifier.fillMaxWidth()) {
                Column(modifier = Modifier.padding(16.dp)) {
                    Text("BLE Ping-Pong Latency", color = Color.Gray, fontSize = 12.sp)
                    Text(text = if (lastRtt != null) "$lastRtt ms" else "-- ms", fontSize = 28.sp, fontWeight = FontWeight.Bold, color = Color(0xFF38BDF8))
                    Text("Total Pings: $pingCount", color = Color.White, fontSize = 12.sp)
                    Spacer(modifier = Modifier.height(12.dp))
                    Button(
                        onClick = { bleManager.sendPing() },
                        enabled = connectionState.contains("Ready"),
                        modifier = Modifier.fillMaxWidth()
                    ) {
                        Text("🏓 Send Ping to Master")
                    }
                }
            }

            Card(colors = CardDefaults.cardColors(containerColor = Color(0xFF0F172A)), modifier = Modifier.fillMaxWidth().weight(1f)) {
                LazyColumn(modifier = Modifier.padding(12.dp)) {
                    items(logs) { log ->
                        Text(log, color = Color(0xFFCBD5E1), fontSize = 11.sp, fontFamily = FontFamily.Monospace)
                    }
                }
            }
        }
    }
}
