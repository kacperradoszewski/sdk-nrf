/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <errno.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/logging/log.h>
#include <zephyr/storage/flash_map.h>
#include <dfu/dfu_target_stream.h>
#include <dfu_partition.h>

LOG_MODULE_REGISTER(dfu_partition, CONFIG_DFU_TARGET_LOG_LEVEL);

#define DFU_PARTITION DT_CHOSEN(nordic_dfu_partition)

static const struct device *const flash_dev = PARTITION_NODE_DEVICE(DFU_PARTITION);
static uint8_t stream_buf[CONFIG_DFU_TARGET_DFU_PARTITION_BUF_SIZE] __aligned(4);

int dfu_partition_stream_init(const char *id, size_t file_size)
{
	int err;

	if (file_size > DT_REG_SIZE(DFU_PARTITION)) {
		LOG_ERR("Requested file too big to fit in the DFU partition %zu > %u",
			file_size, DT_REG_SIZE(DFU_PARTITION));
		return -EFBIG;
	}

	if (!device_is_ready(flash_dev)) {
		LOG_ERR("Flash device of the DFU partition is not ready");
		return -ENODEV;
	}

	err = dfu_target_stream_init(&(struct dfu_target_stream_init){
		.id = id,
		.fdev = flash_dev,
		.buf = stream_buf,
		.len = sizeof(stream_buf),
		.offset = PARTITION_NODE_OFFSET(DFU_PARTITION),
		.size = DT_REG_SIZE(DFU_PARTITION),
		.cb = NULL });
	if (err < 0) {
		LOG_ERR("dfu_target_stream_init failed %d", err);
	}

	return err;
}

int dfu_partition_read(off_t off, void *dst, size_t len)
{
	if (off < 0 || len > DT_REG_SIZE(DFU_PARTITION) || off > DT_REG_SIZE(DFU_PARTITION) - len) {
		return -EINVAL;
	}

	return flash_read(flash_dev, PARTITION_NODE_OFFSET(DFU_PARTITION) + off, dst, len);
}
