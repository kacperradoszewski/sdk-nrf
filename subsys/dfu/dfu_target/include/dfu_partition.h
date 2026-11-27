/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef DFU_PARTITION_H__
#define DFU_PARTITION_H__

#include <stddef.h>
#include <sys/types.h>

/**
 * @brief Open a DFU target stream to the start of the DFU partition.
 *
 * The DFU partition is the partition set as the ``nordic,dfu-partition`` chosen property.
 * Only one stream can be open at a time.
 *
 * @param id Identifier of the stream, used for storing the write progress.
 * @param file_size Size of the image to be written.
 *
 * @retval 0 on success.
 * @retval -EFBIG if the image does not fit in the DFU partition.
 * @retval -ENODEV if the flash device of the DFU partition is not ready.
 * @return Another negative errno from @ref dfu_target_stream_init on failure.
 */
int dfu_partition_stream_init(const char *id, size_t file_size);

/**
 * @brief Read from the DFU partition.
 *
 * @param off Offset within the DFU partition.
 * @param dst Buffer to read into.
 * @param len Number of bytes to read.
 *
 * @retval 0 on success.
 * @retval -EINVAL if the range is outside of the DFU partition.
 * @return Another negative errno from the flash driver on failure.
 */
int dfu_partition_read(off_t off, void *dst, size_t len);

#endif /* DFU_PARTITION_H__ */
