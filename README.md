# BLE On-Demand Reception Demo

A minimal single-SoC demo for ultra-low-power, BLE-based connectionless on-demand data retrieval from a sensor node.

## 📌 Overview

This repository provides a minimal Zephyr implementation demonstrating:

  * Scannable extended advertising as a sensor-node availability mechanism
  * Smartphone-triggered data retrieval using BLE scan request reception
  * Connectionless data transfer using non-scannable, non-connectable extended advertising
  * Transfer of a fixed 1650-byte demo payload using BLE extended advertisements

The demo is intentionally kept minimal so that the implementation highlights the core scan-request-triggered state transition. Deployment-specific logic such as multi-node selection, smartphone-side filtering, external memory handling, and data-index management can be added on top of this baseline.

* * *

## 📁 Repository Structure

```text
src/
  main.c

CMakeLists.txt
prj.conf
README.md
```

* * *

## 💻 Software Requirements

This project must be built within the Nordic nRF Connect SDK (NCS) environment.

For smartphone-side visualization:

  * nRF Connect for Mobile

For observing serial logs:

  * PuTTY
  * nRF Connect Serial Terminal
  * Any standard serial terminal

* * *

## 🔧 Supported Hardware

Tested/intended for Nordic DKs or devices based on:

  * nRF52832
  * nRF52840
  * nRF5340

For nRF5340, ensure that both cores are flashed/configured according to the board and NCS workflow.

* * *

## 📡 Static Address Used

The sensor node uses the following random static address:

```text
CE:AD:BE:AF:BA:11
```

This address remains fixed during operation and helps identify the node in the nRF Connect mobile application.

In this demo, the node uses scannable extended advertising during the availability phase. In this advertising mode, application data or a device name is not included in the scannable advertising packet itself. Therefore, the fixed random static address is used as the primary identifier for easy visualization in the smartphone app.

* * *

## 🔄 Demo Operation

The sensor node operates in two phases.

### 1. Availability Phase

The node periodically transmits bounded scannable extended advertising bursts.

These bursts keep the node available for a smartphone-initiated trigger without keeping the node in a continuous scanning/listening state.

When a nearby smartphone is operated in active scanning mode, it may issue a BLE scan request after receiving the node's scannable advertisement.

### 2. Data-Transfer Phase

The node treats the received scan request as a trigger.

After receiving the trigger, the node exits the scannable advertising phase and switches to non-scannable, non-connectable extended advertising to transmit the demo payload.

No BLE connection is established.

* * *

## 📦 Demo Payload

This demo considers a single sensor node and transmits a fixed dummy payload.

  * Payload size: 1650 bytes
  * Transfer mode: non-scannable, non-connectable extended advertising
  * Purpose: demonstration of the on-demand trigger-and-transfer mechanism

In a real deployment, this fixed payload can be replaced with logged sensor data stored in internal or external memory.

Additional application-specific logic may be added for:

  * Reading data chunks from memory
  * Tracking transmitted chunks
  * Updating memory indices after retrieval
  * Avoiding retransmission of already retrieved data
  * Adding packet IDs, timestamps, node IDs, or integrity checks

* * *

## ▶️ How to Run the Demo

### Step 1: Build and Flash

Build the project using the correct board target.

Example for nRF52832 DK:

```bash
west build -b nrf52dk/nrf52832 .
west flash
```

For other boards, replace the board name accordingly.

* * *

### Step 2: Open Serial Logs

Open the serial terminal for the sensor node.

Expected startup logs:

```text
Starting broadcaster
Created new address
Bluetooth initialized
```

* * *

### Step 3: Start Smartphone Scanning

Open the nRF Connect mobile application and start scanning.

Look for the device with address:

```text
CE:AD:BE:AF:BA:11
```

Since the node uses a fixed random static address, it can be easily identified in the app during testing.

* * *

### Step 4: Observe Trigger and Data Transfer

Once the smartphone sends a scan request, the node switches to the data-transfer phase.

Example logs:

```text
Scannable advertising burst started: 1
Trigger received: 1, From: XX:XX:XX:XX:XX:XX
Data-transfer burst started
Scannable advertising burst started: 2
```

The exact address printed after `From:` depends on the scanner address used by the smartphone.

* * *

## 🧾 Interpreting the Logs

  * `Scannable advertising burst started`

    The node is in the availability phase and is waiting for a smartphone trigger.

  * `Trigger received`

    A BLE scan request was received. In this demo, this scan request acts as the data-retrieval trigger.

  * `Data-transfer burst started`

    The node has switched to the non-scannable extended advertising phase and is transmitting the 1650-byte demo payload.

  * Next `Scannable advertising burst started`

    The node has returned to the availability phase after the data-transfer burst.

* * *

## ⚠️ Important Configuration Notes

### Scan Request Notification

The following option must be enabled in the scannable advertising parameters:

```c
BT_LE_ADV_OPT_NOTIFY_SCAN_REQ
```

Without this option, the application is not notified when a scan request is received. Therefore, the node cannot use scan request reception to transition from the availability phase to the data-transfer phase.

### Work Scheduling

Advertising state transitions are performed using Zephyr work scheduling.

The scan request callback does not directly stop, reconfigure, and restart the advertising set. Instead, it schedules work that performs this transition outside the Bluetooth callback context.

This avoids race-prone behavior while reusing the same extended advertising set for both phases.

* * *

## 🧩 Deployment-Specific Extensions

This repository focuses only on the basic trigger-and-transfer mechanism.

Depending on the deployment, users may extend it with:

  * Custom smartphone application logic
  * Filtering to target a specific sensor node
  * Multi-node selection
  * External flash or memory integration
  * Data indexing to avoid retransmitting already retrieved chunks
  * Real sensor-data formatting
  * Application-layer reliability or duplicate handling

For example, if multiple nearby sensor nodes advertise their availability, a custom smartphone application may apply suitable filtering or selection logic so that the intended node is triggered.

Similarly, in a real data-logging system, memory-control logic can be added to update the read index after a data chunk has been retrieved.

* * *

## 📝 Notes

Scannable advertising is a broadcast mechanism. Any nearby device performing active scanning can, in principle, send a scan request after receiving the scannable advertisement.

This demo focuses on showing how scan request reception can be used as a lightweight trigger for connectionless on-demand data retrieval. Deployment-specific filtering, association, authorization, or user-selection mechanisms can be added on top when required.

* * *

## ✅ Summary

This demo shows:

  * Low-power node availability using bounded scannable extended advertising
  * Smartphone-triggered on-demand data retrieval using scan request reception
  * Connectionless data transfer using non-scannable extended advertising
  * A simple baseline that can be extended for real sensing deployments

* * *

## 📜 License

Apache-2.0
