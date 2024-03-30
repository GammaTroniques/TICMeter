#!/usr/bin/env python
# create-ota - Create zlib-compressed Zigbee OTA file
# Copyright 2023  Simon Arlott
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

import argparse
import functools
import zlib
import logging

import zigpy.ota


def zigbee_get_hex_version(version):
    if not version:
        logging.error("Invalid version: NULL")
        return 4294967295

    if version[0] == "v" or version[0] == "V":
        version = version[1:]

    version_buf = version[
        :10
    ]  # Ensure version_buf has a maximum length of 10 characters
    version_buf = version_buf.split("-")[0]  # Remove the -xxx part of the version

    # Check if the version is in the format "x.y.z"
    version_parts = version_buf.split(".")
    if len(version_parts) == 3:
        try:
            major, minor, revision = map(int, version_parts)
            hex_version = (major << 16) | (minor << 8) | revision
        except ValueError:
            logging.error("Invalid version format: %s", version)
            return 4294967295
    elif len(version_parts) == 2:
        try:
            major, minor = map(int, version_parts)
            hex_version = (major << 8) | minor
        except ValueError:
            logging.error("Invalid version format: %s", version)
            return 4294967295
    else:
        logging.error("Invalid version format: %s", version)
        return 4294967295

    return hex_version


def create(filename, storage_file, manufacturer_id, image_type, version, header_string):
    with open(filename, "rb") as f:
        data = f.read()

    zobj = zlib.compressobj(level=zlib.Z_BEST_COMPRESSION)
    zdata = zobj.compress(data)
    zdata += zobj.flush()

    with open(storage_file, "rb") as f:
        storage_data = f.read()

    zobj = zlib.compressobj(level=zlib.Z_BEST_COMPRESSION)
    zstorage_data = zobj.compress(storage_data)
    zstorage_data += zobj.flush()

    file_version = zigbee_get_hex_version(version)
    print("OTA file version: {} 0x{:08x}".format(version, file_version))
    image = zigpy.ota.image.OTAImage(
        header=zigpy.ota.image.OTAImageHeader(
            upgrade_file_id=zigpy.ota.image.OTAImageHeader.MAGIC_VALUE,
            header_version=0x0100,
            header_length=0,
            field_control=zigpy.ota.image.FieldControl(0),
            manufacturer_id=manufacturer_id,
            image_type=image_type,
            file_version=file_version,
            stack_version=2,
            header_string=header_string[0:32],
            # header_string=header_string[0:32],
            image_size=0,
        ),
        subelements=[
            zigpy.ota.image.SubElement(
                tag_id=0x100,
                data=zstorage_data,
            ),
            zigpy.ota.image.SubElement(
                tag_id=zigpy.ota.image.ElementTagId.UPGRADE_IMAGE,
                data=zdata,
            ),
        ],
    )

    image.header.header_length = len(image.header.serialize())
    image.header.image_size = image.header.header_length + len(
        image.subelements.serialize()
    )

    return image.serialize()


if __name__ == "__main__":
    any_int = functools.wraps(int)(functools.partial(int, base=0))
    parser = argparse.ArgumentParser(
        description="Create zlib-compressed Zigbee OTA file",
        epilog="Reads a firmware image file and outputs an OTA file on standard output",
    )
    parser.add_argument(
        "filename", metavar="INPUT", type=str, help="Firmware image filename"
    )
    parser.add_argument(
        "storage_file", metavar="INPUT", type=str, help="Storage image filename"
    )
    parser.add_argument("output", metavar="OUTPUT", type=str, help="OTA filename")
    parser.add_argument(
        "-m",
        "--manufacturer_id",
        metavar="MANUFACTURER_ID",
        type=any_int,
        required=True,
        help="Manufacturer ID",
    )
    parser.add_argument(
        "-i",
        "--image_type",
        metavar="IMAGE_ID",
        type=any_int,
        required=True,
        help="Image ID",
    )
    parser.add_argument(
        "-v",
        "--version",
        metavar="VERSION",
        type=str,
        required=True,
        help="File version",
    )
    parser.add_argument(
        "-s",
        "--header_string",
        metavar="HEADER_STRING",
        type=str,
        default="",
        help="Header String",
    )

    args = parser.parse_args()
    output = args.output
    del args.output

    data = create(**vars(args))
    with open(output, "wb") as f:
        f.write(data)
