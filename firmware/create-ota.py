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

import zigpy.ota


def create(filename, manufacturer_id, image_type, file_version, header_string):
	with open(filename, "rb") as f:
		data = f.read()

	# zobj = zlib.compressobj(level=zlib.Z_BEST_COMPRESSION)
	# zobj = zlib.compressobj(level=zlib.Z_NO_COMPRESSION)
	# zdata = zobj.compress(data)
	# zdata += zobj.flush()
	zdata = data
	

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
				tag_id=zigpy.ota.image.ElementTagId.UPGRADE_IMAGE, data=zdata,
			)
		],
	)

	image.header.header_length = len(image.header.serialize())
	image.header.image_size = image.header.header_length + len(image.subelements.serialize())
	print(image.header.header_length)
	print(len(image.subelements.serialize()))
	print(image.header.image_size)
	return image.serialize()

if __name__ == "__main__":
	any_int = functools.wraps(int)(functools.partial(int, base=0))
	parser = argparse.ArgumentParser(description="Create zlib-compressed Zigbee OTA file",
		epilog="Reads a firmware image file and outputs an OTA file on standard output")
	parser.add_argument("filename", metavar="INPUT", type=str, help="Firmware image filename")
	parser.add_argument("output", metavar="OUTPUT", type=str, help="OTA filename")
	parser.add_argument("-m", "--manufacturer_id", metavar="MANUFACTURER_ID", type=any_int, required=True, help="Manufacturer ID")
	parser.add_argument("-i", "--image_type", metavar="IMAGE_ID", type=any_int, required=True, help="Image ID")
	parser.add_argument("-v", "--file_version", metavar="VERSION", type=any_int, required=True, help="File version")
	parser.add_argument("-s", "--header_string", metavar="HEADER_STRING", type=str, default="", help="Header String")

	args = parser.parse_args()
	output = args.output
	del args.output

	data = create(**vars(args))
	with open(output, "wb") as f:
		f.write(data)