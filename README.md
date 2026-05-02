# BLE On-Demand Reception Demo

A minimal single-SoC demo for ultra-low-power, BLE-based connectionless on-demand data retrieval from a sensor node.

## 📌 Overview

This repository provides a minimal Zephyr implementation demonstrating:

  * Scannable extended advertising as a sensor-node availability mechanism
  * Smartphone-triggered data retrieval using BLE scan request reception
  * Connectionless data transfer using non-scannable, non-connectable extended advertising
  * Transfer of a fixed 1650-byte demo payload using BLE extended advertisements

The demo is intentionally kept minimal so that the implementation highlights the core scan-request-triggered state transition. Additional deployment-specific logic, such as multi-node selection, smartphone-side filtering, external memory handling, or data-index management, can be added on top of this baseline.

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

For smartphone-side visualization, use any BLE smartphone application that supports scan control and BLE advertising extensions (BLE version 5.x), such as:

  * nRF Connect for Mobile

For observing serial logs, use any standard serial terminal, such as:

  * PuTTY
  * nRF Connect Serial Terminal

* * *

## 🔧 Supported Hardware

Tested/intended for Nordic DKs or devices based on:

  * nRF52832
  * nRF52840
  * nRF5340

For nRF5340, ensure that both cores are flashed/configured according to the board and NCS workflow.

* * *

## 🔄 Demo Operation

The sensor node operates in two phases.

### 1. Availability Phase

The node periodically transmits bounded scannable extended advertising bursts.

In this demo, each availability window contains **3 scannable advertising events**. These events make the node reachable for receiving a smartphone-triggered data-retrieval request without keeping the node in a continuous scanning/listening state.

If no scan request is received in one availability window, the node waits for **5 seconds** before opening the next availability window. Therefore, if the smartphone misses one trigger opportunity, the next opportunity appears after approximately **5 seconds**.

When a nearby smartphone is operated in active scanning mode, it may issue a BLE scan request after receiving the node's scannable advertisement.

### 2. Data-Transfer Phase

The node treats the received scan request as a trigger.

After receiving the trigger, the node exits the scannable advertising phase and switches to non-scannable, non-connectable extended advertising to transmit the demo payload.

In this demo, the data-transfer burst is configured with a retransmission count of **3**, meaning that the same 1650-byte demo payload is advertised through **3 extended advertising events** during the data-transfer phase.

After the data-transfer burst completes, the node waits for **10 seconds** before resuming scannable advertising. This gives the user time to disable smartphone scanning if no further data request is needed.

_No BLE connection is established._

The 5-second availability retry interval and the 10-second post-transfer return interval can be modified according to application requirements.


* * *

## 📦 Demo Payload

This demo considers a single sensor node and transmits a fixed dummy payload.

  * Payload size: 1650 bytes
  * Transfer mode: non-scannable, non-connectable extended advertising
  * Data retransmission count: 3 extended advertising events

*If the logged data in a real application is larger than 1650 bytes, it can be divided across multiple extended advertising events, each of which can be retransmitted independently. In this demo setup, one such 1650-byte extended advertising event takes approximately 17 ms to transmit.*

In a real deployment, this fixed payload can be replaced with logged sensor data stored in internal or external memory.

* * *

## 📡 Static Address Used

The sensor node uses the following random static address:

```text
CE:AD:BE:AF:BA:11
```

This address remains fixed during operation and helps identify the node in the smartphone app.

In this demo, the node uses scannable extended advertising during the availability phase. In this mode, application data or a device name cannot be included in the scannable advertising packet itself. Therefore, the fixed random static address is used as the primary identifier for easy visualization in the smartphone app.


* * *

## 🧠 Callback Roles

The implementation mainly relies on two extended advertising callbacks:

  * `scanned_cb()`

    This callback is invoked when a scan request is received during the scannable advertising phase. In this demo, scan request reception is treated as the smartphone trigger for data retrieval. The callback schedules the transition to the data-transfer phase.

  * `sent_cb()`

    This callback is invoked after the configured advertising burst is completed. If the node was in the availability phase and no trigger was received, it schedules the next availability window after 5 seconds. If the node was in the data-transfer phase, it schedules the return to the availability phase after 10 seconds.

The actual advertising stop/reconfigure/restart operations are not performed directly inside these callbacks. They are handled through Zephyr work scheduling, as explained below.

* * *

## ▶️ How to Run the Demo

### Step 1: Flash the Sensor Node

Flash a board with this code. The board acts as the sensor node.

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

Open the smartphone BLE app and start scanning.

Look for the device with address:

```text
CE:AD:BE:AF:BA:11
```

Since the node uses a fixed random static address, it can be easily identified in the app during testing.

* * *

### Step 4: Observe Trigger and Data Transfer

Once the smartphone sends a scan request, the node switches to the data-transfer phase.

The received 1650-byte demo payload should become visible on the smartphone app. This has been verified using the nRF Connect Mobile app.

Example node logs:

```text
Scannable advertising burst started: 1 
Trigger received: 1, From: XX:XX:XX:XX:XX:XX
Data-transfer burst started: 1650-byte payload, 3 retransmissions
Data-transfer burst completed. Scannable advertising will resume in 10000 ms.
Scannable advertising burst started: 2
```

The exact address printed after `From:` depends on the address used by the smartphone.

If no scan request is received during an availability window, the node opens the next availability window after approximately 5 seconds.

After data transfer completes, the node waits for approximately 10 seconds before resuming scannable advertising. This delay gives the user time to disable smartphone scanning if no further data request is required.

Both intervals can be changed according to application requirements.

* * *

## 🧾 Interpreting the Logs

  * `Scannable advertising burst started`

    The node is in the availability phase and is waiting for a smartphone trigger.

  * `Trigger received`

    A BLE scan request was received. In this demo, this scan request acts as the data-retrieval trigger.

  * `Data-transfer burst started`

    The node has switched to the non-scannable extended advertising phase and is transmitting the 1650-byte demo payload.

  * `Data-transfer burst completed`

    The configured data-transfer burst has completed. The node will wait for 10 seconds before returning to the availability phase.

  * Next `Scannable advertising burst started`

    The node has returned to the availability phase. This occurs after the 10-second post-transfer return interval if data was transmitted, or after the 5-second retry interval if no scan request was received.

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

## 🧩 Possible Extensions

This repository focuses only on the basic trigger-and-transfer mechanism.

Depending on the deployment, users may extend it with:

  * Custom smartphone application logic
  * Filtering to target a specific sensor node
  * Multi-node selection
  * External flash or memory integration
  * Data indexing to avoid retransmitting already retrieved chunks
  * Real sensor-data formatting
  * Application-layer reliability or duplicate handling

For example, if multiple nearby sensor nodes advertise their availability, a custom smartphone application may apply suitable filtering or selection logic so that the intended node is triggered. Similarly, in a real data-logging system, memory-control logic can be added to update the read index after a data chunk has been retrieved.

* * *

## 📝 Note

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
