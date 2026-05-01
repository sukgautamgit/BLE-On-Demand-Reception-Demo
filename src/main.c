/* main.c - BLE on-demand reception demo using scannable advertising */

/*
 * Copyright (c) 2015-2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/types.h>
#include <stddef.h>
#include <stdbool.h>
#include <errno.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/addr.h>

/*
 * Number of extended advertising events transmitted in each burst.
 *
 * For SCANNABLE mode, this defines how many availability beacons are sent
 * before the controller terminates the burst automatically.
 *
 * For DATA mode, this defines how many data-carrying copies of the same
 * extended advertising event are sent before termination of the burst.
 */
#define ADV_EVENT_COUNT        3U

/*
 * If no scan request is received during an availability window, the node
 * opens the next availability window after this interval.
 */
#define AVAILABILITY_RETRY_MS  5000U

/*
 * After the data-transfer burst completes, the node waits before returning
 * to scannable advertising. This gives the user time to disable smartphone
 * scanning if no further data request is needed.
 */
#define POST_DATA_RESTART_MS   10000U

enum adv_mode {
	MODE_SCANNABLE,  /* Availability / trigger-detection phase */
	MODE_DATA,       /* Logged-data transfer phase */
};

static enum adv_mode current_mode = MODE_SCANNABLE;

static uint32_t scannable_cnt = 0U;
static uint32_t trigger_cnt = 0U;

/*
 * This flag is set when a scan request is received during the current
 * scannable advertising burst.
 *
 * Why it is needed:
 * A scan request can be received on the last scannable advertising event.
 * In that case, both the .scanned and .sent callbacks may be invoked for the
 * same burst. This flag lets the .sent callback know that the burst already
 * produced a valid trigger, so the normal scannable-restart path should not
 * be taken from there.
 */
static bool flag_trigger = false;

/*
 * Dummy application data used to demonstrate the data-transfer phase.
 * In this demo, the complete application-level data buffer is 1650 bytes.
 * It is split across multiple AD structures for transmission in one
 * extended advertising event, and the individual fragments include the
 * app-specific header bytes required by the application format.
 */
static const uint8_t adv_payload[] = {
	0x59, 0x00, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11,
	0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B,
	0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25,
	0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
	0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x40, 0x41, 0x42, 0x43,
	0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D,
	0x4E, 0x4F, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,
	0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F, 0x60, 0x61,
	0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x6B,
	0x6C, 0x6D, 0x6E, 0x6F, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75,
	0x76, 0x77, 0x78, 0x79, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F,
	0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
	0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F, 0x90, 0x91, 0x92, 0x93,
	0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D,
	0x9E, 0x9F, 0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7,
	0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xB0, 0xB1,
	0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xBB,
	0xBC, 0xBD, 0xBE, 0xBF, 0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5,
	0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF,
	0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9,
	0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF, 0xE0, 0xE1, 0xE2, 0xE3,
	0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED,
	0xEE, 0xEF, 0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7,
	0xF8, 0xF9, 0xFA, 0xFB
};

static const uint8_t scan_rsp_payload[] = { 0x59, 0x00, 0x01 };

static const struct bt_data sd_scannable[] = {
	BT_DATA(BT_DATA_MANUFACTURER_DATA,
		scan_rsp_payload, sizeof(scan_rsp_payload)),
};

/*
 * Data payload for the large single-event transfer case.
 * The node transmits a 1650-byte application buffer using extended advertising.
 * The payload is split across multiple AD structures for transmission through
 * one extended advertising event.
 */
static const struct bt_data ad_data[] = {
	BT_DATA(BT_DATA_MANUFACTURER_DATA, adv_payload, sizeof(adv_payload)),
	BT_DATA(BT_DATA_MANUFACTURER_DATA, adv_payload, sizeof(adv_payload)),
	BT_DATA(BT_DATA_MANUFACTURER_DATA, adv_payload, sizeof(adv_payload)),
	BT_DATA(BT_DATA_MANUFACTURER_DATA, adv_payload, sizeof(adv_payload)),
	BT_DATA(BT_DATA_MANUFACTURER_DATA, adv_payload, sizeof(adv_payload)),
	BT_DATA(BT_DATA_MANUFACTURER_DATA, adv_payload, sizeof(adv_payload)),
	BT_DATA(BT_DATA_MANUFACTURER_DATA, adv_payload, 112),
};

static struct bt_le_ext_adv *adv;

/*
 * Advertising parameters for the scannable availability phase.
 * The node emits scannable, non-connectable extended advertisements and waits
 * for a scan request from a nearby active scanner.
 *
 * BT_LE_ADV_OPT_NOTIFY_SCAN_REQ is required so that scanned_cb() is invoked
 * when a scan request is received.
 */
static struct bt_le_adv_param adv_param =
	BT_LE_ADV_PARAM_INIT(BT_LE_ADV_OPT_EXT_ADV |
			     BT_LE_ADV_OPT_SCANNABLE |
			     BT_LE_ADV_OPT_NOTIFY_SCAN_REQ |
			     BT_LE_ADV_OPT_USE_IDENTITY |
			     BT_LE_ADV_OPT_NO_2M,
			     0xA0, /* 100 ms */
			     0xA0,
			     NULL);

/*
 * Advertising parameters for the non-scannable data-transfer phase.
 * Once triggered, the node transmits data using non-scannable, non-connectable
 * extended advertisements.
 */
static struct bt_le_adv_param adv_data_param =
	BT_LE_ADV_PARAM_INIT(BT_LE_ADV_OPT_EXT_ADV |
			     BT_LE_ADV_OPT_USE_IDENTITY |
			     BT_LE_ADV_OPT_NO_2M,
			     0xA0, /* 100 ms */
			     0xA0,
			     NULL);

/*
 * Start advertising with no time limit but with a bounded number of events.
 * The controller terminates the burst automatically after ADV_EVENT_COUNT
 * advertising events. The sent_cb() callback is invoked when this limit is
 * reached.
 */
static struct bt_le_ext_adv_start_param ext_adv_param =
	BT_LE_EXT_ADV_START_PARAM_INIT(0, ADV_EVENT_COUNT);

static void data_mode_work_handler(struct k_work *work);
static void restart_scannable_work_handler(struct k_work *work);

static K_WORK_DEFINE(data_mode_work, data_mode_work_handler);
static K_WORK_DELAYABLE_DEFINE(restart_scannable_work,
			       restart_scannable_work_handler);

/*
 * Advertising state transitions are performed from Zephyr work handlers rather
 * than directly inside Bluetooth callbacks. This keeps callbacks short and
 * avoids races while stopping, reconfiguring, and restarting the same
 * advertising set.
 */

static int stop_adv_if_needed(void)
{
	int err = bt_le_ext_adv_stop(adv);

	if (err && err != -EALREADY) {
		printk("Advertising stop failed (err %d)\n", err);
	}

	return err;
}

static void schedule_scannable_restart(uint32_t delay_ms)
{
	k_work_reschedule(&restart_scannable_work, K_MSEC(delay_ms));
}

static int start_scannable_adv(void)
{
	int err;

	current_mode = MODE_SCANNABLE;
	flag_trigger = false;

	err = bt_le_ext_adv_update_param(adv, &adv_param);
	if (err) {
		printk("Failed to update scannable advertising parameters (err %d)\n",
		       err);
		return err;
	}

	err = bt_le_ext_adv_set_data(adv, NULL, 0,
				     sd_scannable, ARRAY_SIZE(sd_scannable));
	if (err) {
		printk("Failed to set scannable advertising data (err %d)\n", err);
		return err;
	}

	err = bt_le_ext_adv_start(adv, &ext_adv_param);
	if (err) {
		printk("Failed to start scannable advertising (err %d)\n", err);
		return err;
	}

	scannable_cnt++;
	printk("\nScannable advertising burst started: %u\n",
	       scannable_cnt);

	return 0;
}

static int start_data_adv(void)
{
	int err;

	current_mode = MODE_DATA;

	err = bt_le_ext_adv_update_param(adv, &adv_data_param);
	if (err) {
		printk("Failed to update data advertising parameters (err %d)\n",
		       err);
		return err;
	}

	err = bt_le_ext_adv_set_data(adv,
				     ad_data, ARRAY_SIZE(ad_data),
				     NULL, 0);
	if (err) {
		printk("Failed to set data advertising payload (err %d)\n", err);
		return err;
	}

	err = bt_le_ext_adv_start(adv, &ext_adv_param);
	if (err) {
		printk("Failed to start data advertising (err %d)\n", err);
		return err;
	}

	printk("Data-transfer burst started: 1650-byte payload, %u retransmissions\n",
	       ADV_EVENT_COUNT);

	return 0;
}

static void sent_cb(struct bt_le_ext_adv *adv,
		    struct bt_le_ext_adv_sent_info *info)
{
	ARG_UNUSED(adv);
	ARG_UNUSED(info);

	if (current_mode == MODE_SCANNABLE) {
		/*
		 * If no trigger was received during this scannable burst, open
		 * the next availability window after AVAILABILITY_RETRY_MS.
		 *
		 * If a trigger was already received, the data-mode transition has
		 * already been scheduled from scanned_cb(), so the normal
		 * scannable restart path should not be taken here.
		 */
		if (!flag_trigger) {
			printk("No trigger received. Next availability window in %u ms.\n",
			       AVAILABILITY_RETRY_MS);
			schedule_scannable_restart(AVAILABILITY_RETRY_MS);
		}
		return;
	}

	if (current_mode == MODE_DATA) {
		/*
		 * After the data-transfer burst ends, return to scannable mode
		 * after POST_DATA_RESTART_MS. This allows the user to disable
		 * smartphone scanning if no further data request is needed.
		 */
		printk("Data-transfer burst completed. Scannable advertising will resume in %u ms.\n",
		       POST_DATA_RESTART_MS);
		schedule_scannable_restart(POST_DATA_RESTART_MS);
	}
}

static void scanned_cb(struct bt_le_ext_adv *adv,
		       struct bt_le_ext_adv_scanned_info *info)
{
	char addr[BT_ADDR_LE_STR_LEN];

	ARG_UNUSED(adv);

	if (current_mode != MODE_SCANNABLE || flag_trigger) {
		return;
	}

	flag_trigger = true;
	trigger_cnt++;

	bt_addr_le_to_str(info->addr, addr, sizeof(addr));
	printk("Trigger received: %u, From: %s\n", trigger_cnt, addr);

	k_work_submit(&data_mode_work);
}

static struct bt_le_ext_adv_cb ext_callbacks = {
	/*
	 * Executes when the configured number of extended advertising events
	 * has been completed.
	 */
	.sent = sent_cb,

	/*
	 * Executes when a scan request is received and scan response data has
	 * been sent to the active scanner.
	 */
	.scanned = scanned_cb,
};

static void data_mode_work_handler(struct k_work *work)
{
	ARG_UNUSED(work);

	if (current_mode != MODE_SCANNABLE) {
		return;
	}

	k_work_cancel_delayable(&restart_scannable_work);

	stop_adv_if_needed();

	/* Give the controller time to complete the advertising stop. */
	k_msleep(30);

	start_data_adv();
}

static void restart_scannable_work_handler(struct k_work *work)
{
	ARG_UNUSED(work);

	stop_adv_if_needed();

	/* Give the controller time to complete the advertising stop. */
	k_msleep(30);

	start_scannable_adv();
}

static void set_random_static_address(void)
{
	bt_addr_le_t addr;
	int err;

	err = bt_addr_le_from_str("CE:AD:BE:AF:BA:11", "random", &addr);
	if (err) {
		printk("Invalid Bluetooth address (err %d)\n", err);
		return;
	}

	err = bt_id_create(&addr, NULL);
	if (err < 0) {
		printk("Failed to create identity (err %d)\n", err);
		return;
	}

	printk("Created new address\n");
}

int main(void)
{
	int err;

	printk("Starting broadcaster\n");

	/*
	 * A fixed random static address enables easy identification of the
	 * sensor node in the smartphone app.
	 */
	set_random_static_address();

	err = bt_enable(NULL);
	if (err) {
		printk("Bluetooth initialization failed (err %d)\n", err);
		return 0;
	}

	printk("Bluetooth initialized\n");

	err = bt_le_ext_adv_create(&adv_param, &ext_callbacks, &adv);
	if (err) {
		printk("Failed to create advertising set (err %d)\n", err);
		return 0;
	}

	start_scannable_adv();

	while (1) {
		k_sleep(K_SECONDS(1));
	}

	return 0;
}
