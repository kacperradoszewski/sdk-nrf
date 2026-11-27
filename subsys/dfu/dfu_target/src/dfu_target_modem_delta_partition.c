/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>
#include <nrf_modem_delta_dfu.h>
#include <dfu/dfu_target.h>
#include <dfu/dfu_target_stream.h>
#include <dfu/dfu_target_modem_delta.h>
#include <dfu_partition.h>

LOG_MODULE_REGISTER(dfu_target_modem_delta, CONFIG_DFU_TARGET_LOG_LEVEL);

/* Modem delta update packages start with a 2 byte magic word, "DU" */
#define MODEM_MAGIC 0x5544

bool dfu_target_modem_delta_identify(const void *const buf)
{
	return sys_get_le16(buf) == MODEM_MAGIC;
}

int dfu_target_modem_delta_init(size_t file_size, int img_num, dfu_target_callback_t cb)
{
	int err;
	struct nrf_modem_delta_dfu_uuid version;
	char version_string[NRF_MODEM_DELTA_DFU_UUID_LEN+1];

	ARG_UNUSED(img_num);
	ARG_UNUSED(cb);

	/* Retrieve and print modem firmware UUID */
	err = nrf_modem_delta_dfu_uuid(&version);
	if (err != 0) {
		LOG_ERR("Firmware version request failed, error %d", err);
		return -EFAULT;
	}

	snprintf(version_string, sizeof(version_string), "%.*s",
		 sizeof(version.data), version.data);

	LOG_INF("Modem firmware version: %s", version_string);

	return dfu_partition_stream_init("MODEM_DELTA", file_size);
}

int dfu_target_modem_delta_offset_get(size_t *out)
{
	size_t bytes_written, bytes_buffered;
	int err;

	err = dfu_target_stream_offset_get(&bytes_written);
	if (err) {
		return err;
	}

	err = dfu_target_stream_bytes_buffered_get(&bytes_buffered);
	if (err) {
		return err;
	}

	*out = bytes_written + bytes_buffered;

	return 0;
}

int dfu_target_modem_delta_write(const void *const buf, size_t len)
{
	return dfu_target_stream_write(buf, len);
}

int dfu_target_modem_delta_done(bool successful)
{
	return dfu_target_stream_done(successful);
}

int dfu_target_modem_delta_schedule_update(int img_num)
{
	int err;
	uint8_t header[sizeof(uint16_t)];

	ARG_UNUSED(img_num);

	err = dfu_partition_read(0, header, sizeof(header));
	if (err != 0) {
		LOG_ERR("Failed to read the DFU partition, error %d", err);
		return err;
	}

	if (!dfu_target_modem_delta_identify(header)) {
		LOG_ERR("No modem delta update in the DFU partition");
		return -EINVAL;
	}

	err = nrf_modem_delta_dfu_update();
	if (err != 0) {
		LOG_ERR("Modem firmware upgrade scheduling failed, error %d", err);
		return -EFAULT;
	}
	LOG_INF("Scheduling modem firmware upgrade at next boot");

	return 0;
}

int dfu_target_modem_delta_reset(void)
{
	/* Overwriting the start of the package keeps the modem from starting the update. */
	return dfu_target_stream_reset();
}
