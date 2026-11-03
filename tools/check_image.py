"""Check a release image the way each installer reads it (tools/release_image.sh).

  python3 tools/check_image.py Tithe-vX.bin firmware.bin X

- The image starts with a bootloader (0xE9) and holds the version string once.
- The game at 0x10000 is byte for byte the built firmware.bin.
- M5Launcher before 2.6 (sd_functions.cpp, updateFromSD): finds the app entry
  whose offset's third byte is 0x01 (0x010000) and installs file size - 0x10000
  bytes from there; any data area past the end of the file is skipped.
- M5Launcher 2.6 and later: takes the app entry's own offset, and creates each
  spiffs-type data area (subtype 0x82) named in the table, empty when the file
  holds nothing for it. The Tithe's must be named "tithe".
"""
import struct, sys

img_path, app_path, version = sys.argv[1], sys.argv[2], sys.argv[3]
img = open(img_path, 'rb').read()
app = open(app_path, 'rb').read()
fail = []
if img[0] != 0xE9: fail.append('no bootloader at 0x0')
if img.count(version.encode()) < 1: fail.append('version string missing')
if img[0x10000:0x10000 + len(app)] != app or len(img) != 0x10000 + len(app):
    fail.append('the game at 0x10000 is not firmware.bin')

entries = []
for i in range(0, 0xC00, 32):
    e = img[0x8000 + i:0x8000 + i + 32]
    if e[:2] != b'\xaa\x50': break
    typ, sub = e[2], e[3]
    off, size = struct.unpack('<II', e[4:12])
    label = e[12:28].split(b'\0')[0].decode()
    entries.append((typ, sub, off, size, label))
if not entries: fail.append('no partition table at 0x8000')

# Older launchers: app entry by the offset byte, size from the file.
old = [e for e in entries if e[1] in (0x00, 0x10, 0x20) and ((e[2] >> 16) & 0xFF) == 0x01]
if not old or len(img) - 0x10000 != len(app): fail.append('older launchers would not find the game')
# Newer launchers: app entry by type, data areas by subtype.
new_app = [e for e in entries if e[0] == 0x00 and e[1] in (0x00, 0x10, 0x20)]
if not new_app or new_app[0][2] != 0x10000: fail.append('newer launchers would not find the game at 0x10000')
data = [e for e in entries if e[0] == 0x01 and e[1] in (0x82, 0x83)]
if [e[4] for e in data] != ['tithe']: fail.append('the data area is not one named "tithe": %r' % data)
if any(e[2] < len(img) for e in data): fail.append('a data area starts inside the file (it would be copied)')

for e in entries:
    print('  %-9s type %02x sub %02x at 0x%06x, 0x%06x bytes' % (e[4], e[0], e[1], e[2], e[3]))
if fail:
    print('check_image: ' + '; '.join(fail), file=sys.stderr)
    sys.exit(1)
print('check_image: ok, %d bytes, game %d bytes' % (len(img), len(app)))
