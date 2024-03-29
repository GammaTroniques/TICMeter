import logging
import sys

def zigbee_get_hex_version(version):
    if not version:
        logging.error("Invalid version: NULL")
        return 4294967295

    if version[0] == 'v' or version[0] == 'V':
        version = version[1:]

    version_buf = version[:10]  # Ensure version_buf has a maximum length of 10 characters
    version_buf = version_buf.split('-')[0]  # Remove the -xxx part of the version

    # Check if the version is in the format "x.y.z"
    version_parts = version_buf.split('.')
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


version = sys.argv[1]
hex_version = zigbee_get_hex_version(version)
print(hex_version)
