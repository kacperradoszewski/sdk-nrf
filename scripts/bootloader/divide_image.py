#!/usr/bin/env python3
#
# Copyright (c) 2024 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause

import argparse
import os
import struct

# Header magic (4 bytes) + block number (1 byte) + Is last flag (1 byte) + Block size (4 bytes) + Version string (32 bytes)
HEADER_LENGTH = 4 + 1 + 1 + 4 + 32

HEADER_MAGIC = 0x424ad2dc

def image_split(image, max_block_size):
    block_size = max_block_size - HEADER_LENGTH
    blocks = []

    with open(image, 'rb') as file:
        while True:
            block = file.read(block_size)
            if not block:
                break
            blocks.append(block)

    return blocks

class block_header:
    def __init__(self, number, is_last, offset, app_version):
        self.__number = number
        self.__is_last = is_last
        self.__offset = offset
        self.__app_version = app_version.encode('utf-8')
        self.__pack_format = '<IB?I32s'

    def encode(self):
        return struct.pack(self.__pack_format, HEADER_MAGIC, self.__number, self.__is_last, self.__offset, self.__app_version)

def parse_args():
    parser = argparse.ArgumentParser(
        description='Divide the application image into multiple chunks',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        allow_abbrev=False)

    parser.add_argument('--application', required=True,
                        help='The application image file.')
    parser.add_argument('--version-string', required=True,
                        help='The application version string to be included in the header.')
    parser.add_argument('--max-block-size', required=True,
                        help='The maximum size of a block in KB.')
    parser.add_argument('--out-directory', required=True,
                        help='The directory to store the divided image into.')

    return parser.parse_args()

def main():
    # Parse arguments.
    args = parse_args()

    app = args.application
    app_version_string = args.version_string
    max_block_size = int(args.max_block_size) * 1000
    out_directory = args.out_directory

    # Block offset within the whole image.
    offset = 0

    # Split the image.
    blocks = image_split(app, max_block_size)
    total_blocks = len(blocks)

    # Append headers to each block and store in the output directory.
    for i, block in enumerate(blocks):
        # Create the block header (1-indexed numbering).
        header = block_header(i + 1, (i == total_blocks - 1), offset, app_version_string)

		# Increase the offset (image block only, no header included).
        offset += len(block)

        # Append the header to the block.
        block = header.encode() + block

        # Generate file name of each block.
        block_version_string = app_version_string + '_{}of{}'.format(i + 1, total_blocks)

        # Write the block into the output directory.
        with open(os.path.join(out_directory, block_version_string), 'wb') as block_file:
            block_file.write(block)
            block_file.close()

if __name__ == '__main__':
    try:
        main()
    except Exception as e:
        print(e)
        exit(1)