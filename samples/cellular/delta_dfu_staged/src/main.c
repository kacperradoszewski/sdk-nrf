/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/*
 * Temporary test of the modem delta update on nRF92 Series devices, without a download.
 * The package is programmed with a debugger, either directly to the DFU partition, or to
 * the staging partition, from where the shell commands copy it to the DFU partition
 * through the modem delta DFU target:
 *
 *   ddfu info           show the partitions and the package headers in them
 *   ddfu update         schedule the update of the package in the DFU partition
 *   ddfu write <size>   copy <size> bytes of the staged package to the DFU partition
 *   ddfu schedule       schedule the update through the DFU target, after ddfu write
 *   ddfu rollback       schedule a rollback of a completed update
 *   ddfu cancel         reset the DFU target
 *   ddfu uuid           print the modem firmware UUID
 */

#include <stdlib.h>
#include <zephyr/kernel.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/shell/shell.h>
#include <zephyr/logging/log.h>
#include <modem/nrf_modem_lib.h>
#include <nrf_modem_delta_dfu.h>
#include <dfu/dfu_target.h>

LOG_MODULE_REGISTER(delta_dfu_staged, LOG_LEVEL_INF);

#define STAGING_PARTITION DT_NODELABEL(cpuapp_slot1_partition)
#define DFU_PARTITION DT_CHOSEN(nordic_dfu_partition)

#define CHUNK_SIZE 1024
#define PROGRESS_INTERVAL (64 * 1024)

/* Modem delta update packages start with a 2 byte magic word, "DU" */
#define MODEM_MAGIC 0x5544

static const struct device *const staging_dev = PARTITION_NODE_DEVICE(STAGING_PARTITION);
static uint8_t chunk[CHUNK_SIZE];

NRF_MODEM_LIB_ON_DFU_RES(delta_dfu_staged_res, on_dfu_res, NULL);

static void on_dfu_res(int dfu_res, void *ctx)
{
	ARG_UNUSED(ctx);

	LOG_INF("Modem DFU result 0x%08x", dfu_res);
}

static void dfu_target_cb(enum dfu_target_evt_id evt)
{
	LOG_INF("DFU target event %d", evt);
}

static int header_print(const struct shell *sh, const char *name, const struct device *dev,
			off_t offset)
{
	int err;
	uint8_t hdr[sizeof(uint16_t)];

	err = flash_read(dev, offset, hdr, sizeof(hdr));
	if (err) {
		shell_error(sh, "%s: read failed, err %d", name, err);
		return err;
	}

	shell_print(sh, "%s: id 0x%04x%s", name, sys_get_le16(hdr),
		    sys_get_le16(hdr) == MODEM_MAGIC ? " (DU)" : "");

	return 0;
}

static int cmd_info(const struct shell *sh, size_t argc, char **argv)
{
	shell_print(sh, "Staging partition 0x%08x, %u bytes",
		    (uint32_t)DT_MAPPED_PARTITION_ADDR(STAGING_PARTITION),
		    (uint32_t)DT_REG_SIZE(STAGING_PARTITION));
	shell_print(sh, "DFU partition     0x%08x, %u bytes",
		    (uint32_t)DT_MAPPED_PARTITION_ADDR(DFU_PARTITION),
		    (uint32_t)DT_REG_SIZE(DFU_PARTITION));

	(void)header_print(sh, "Staged package", staging_dev, PARTITION_NODE_OFFSET(STAGING_PARTITION));
	(void)header_print(sh, "DFU partition ", PARTITION_NODE_DEVICE(DFU_PARTITION),
			   PARTITION_NODE_OFFSET(DFU_PARTITION));

	return 0;
}

static int cmd_update(const struct shell *sh, size_t argc, char **argv)
{
	int err;
	uint8_t hdr[sizeof(uint16_t)];

	err = flash_read(PARTITION_NODE_DEVICE(DFU_PARTITION), PARTITION_NODE_OFFSET(DFU_PARTITION),
			 hdr, sizeof(hdr));
	if (err) {
		shell_error(sh, "DFU partition read failed, err %d", err);
		return err;
	}

	if (sys_get_le16(hdr) != MODEM_MAGIC) {
		shell_error(sh, "No modem delta update package in the DFU partition");
		return -ENOENT;
	}

	err = nrf_modem_delta_dfu_update();
	if (err) {
		shell_error(sh, "nrf_modem_delta_dfu_update failed, err %d", err);
		return err;
	}

	shell_print(sh, "Update scheduled, reset the device (pin reset) to run it");

	return 0;
}

static int cmd_write(const struct shell *sh, size_t argc, char **argv)
{
	int err;
	size_t size;
	size_t off;
	int64_t start;

	size = strtoul(argv[1], NULL, 0);
	if (size == 0 || size > DT_REG_SIZE(STAGING_PARTITION)) {
		shell_error(sh, "Invalid size %zu", size);
		return -EINVAL;
	}

	err = flash_read(staging_dev, PARTITION_NODE_OFFSET(STAGING_PARTITION), chunk, CHUNK_SIZE);
	if (err) {
		shell_error(sh, "Staging partition read failed, err %d", err);
		return err;
	}

	if (dfu_target_img_type(chunk, CHUNK_SIZE) != DFU_TARGET_IMAGE_TYPE_MODEM_DELTA) {
		shell_error(sh, "No modem delta update package in the staging partition");
		return -ENOENT;
	}

	err = dfu_target_init(DFU_TARGET_IMAGE_TYPE_MODEM_DELTA, 0, size, dfu_target_cb);
	if (err) {
		shell_error(sh, "dfu_target_init failed, err %d", err);
		return err;
	}

	start = k_uptime_get();

	for (off = 0; off < size; off += CHUNK_SIZE) {
		const size_t len = MIN(CHUNK_SIZE, size - off);

		err = flash_read(staging_dev, PARTITION_NODE_OFFSET(STAGING_PARTITION) + off,
				 chunk, len);
		if (err) {
			shell_error(sh, "Staging partition read failed at %zu, err %d", off, err);
			goto fail;
		}

		err = dfu_target_write(chunk, len);
		if (err) {
			shell_error(sh, "dfu_target_write failed at %zu, err %d", off, err);
			goto fail;
		}

		if ((off + len) % PROGRESS_INTERVAL == 0) {
			shell_print(sh, "%zu / %zu bytes", off + len, size);
		}
	}

	err = dfu_target_done(true);
	if (err) {
		shell_error(sh, "dfu_target_done failed, err %d", err);
		return err;
	}

	shell_print(sh, "Wrote %zu bytes in %lld ms, now run 'ddfu schedule'", size,
		    k_uptime_get() - start);

	return 0;

fail:
	(void)dfu_target_done(false);
	return err;
}

static int cmd_schedule(const struct shell *sh, size_t argc, char **argv)
{
	int err;

	err = dfu_target_schedule_update(0);
	if (err) {
		shell_error(sh, "dfu_target_schedule_update failed, err %d", err);
		return err;
	}

	shell_print(sh, "Update scheduled, reset the device (pin reset) to run it");

	return 0;
}

static int cmd_rollback(const struct shell *sh, size_t argc, char **argv)
{
	int err;

	err = nrf_modem_delta_dfu_rollback();
	if (err) {
		shell_error(sh, "nrf_modem_delta_dfu_rollback failed, err %d", err);
		return err;
	}

	shell_print(sh, "Rollback scheduled, reset the device (pin reset) to run it");

	return 0;
}

static int cmd_cancel(const struct shell *sh, size_t argc, char **argv)
{
	int err;

	err = dfu_target_reset();
	if (err) {
		shell_error(sh, "dfu_target_reset failed, err %d", err);
		return err;
	}

	shell_print(sh, "DFU target reset");

	return 0;
}

static int cmd_uuid(const struct shell *sh, size_t argc, char **argv)
{
	int err;
	struct nrf_modem_delta_dfu_uuid uuid;

	err = nrf_modem_delta_dfu_uuid(&uuid);
	if (err) {
		shell_error(sh, "nrf_modem_delta_dfu_uuid failed, err %d", err);
		return err;
	}

	shell_print(sh, "%.*s", (int)sizeof(uuid.data), uuid.data);

	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(ddfu_cmds,
	SHELL_CMD(info, NULL, "Show the partitions and package headers", cmd_info),
	SHELL_CMD(update, NULL, "Schedule the update of the package in the DFU partition",
		  cmd_update),
	SHELL_CMD_ARG(write, NULL, "<size> Copy the staged package to the DFU partition",
		      cmd_write, 2, 0),
	SHELL_CMD(schedule, NULL, "Schedule the update through the DFU target", cmd_schedule),
	SHELL_CMD(rollback, NULL, "Schedule a rollback", cmd_rollback),
	SHELL_CMD(cancel, NULL, "Reset the DFU target", cmd_cancel),
	SHELL_CMD(uuid, NULL, "Print the modem firmware UUID", cmd_uuid),
	SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(ddfu, &ddfu_cmds, "Staged modem delta update", NULL);

int main(void)
{
	int err;

	LOG_INF("Staged modem delta update test");

	err = nrf_modem_lib_init();
	if (err) {
		LOG_ERR("nrf_modem_lib_init failed, err %d", err);
		return 0;
	}

	LOG_INF("Modem initialized");

	return 0;
}
