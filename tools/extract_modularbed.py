"""Extract the modular bed firmware (fw-modularbed.bin) from an official XL .bbf (optional).

usage: python3 extract_modularbed.py XL_firmware_6.2.6.bbf fw-modularbed.bin
needs: pip install littlefs-python

The XL .bbf carries the toolhead and modular bed firmware in a littlefs image (TLV 1,
block size in TLV 2, block count in TLV 3). Giving the official bed firmware to the
build (-D MODULARBED_BINARY_PATH:FILEPATH=...) keeps the modular bed on the official
binary, so only the toolheads are re-flashed. Without it the bed firmware is built
from the same v6.2.6 source.
"""
import hashlib
import io
import sys

from littlefs import LittleFS

FW_LEN_OFFSET = 96  # little-endian uint32 in the .bbf header
HEADER_SIZE = 576


def read_tlv(data):
    fw_len = int.from_bytes(data[FW_LEN_OFFSET:FW_LEN_OFFSET + 4], 'little')
    t = io.BytesIO(data[HEADER_SIZE + fw_len:])
    tlv = {}
    while True:
        ty = t.read(1)
        if not ty or ty[0] == 0:
            break
        ln = int.from_bytes(t.read(4), 'little')
        tlv[ty[0]] = t.read(ln)
    return tlv


def main(bbf, out):
    tlv = read_tlv(open(bbf, 'rb').read())
    bs = int.from_bytes(tlv[2], 'little')
    bc = int.from_bytes(tlv[3], 'little')
    fs = LittleFS(block_size=bs, block_count=bc, mount=False)
    fs.context.buffer = bytearray(tlv[1])
    fs.mount()
    data = fs.open('/puppies/fw-modularbed.bin', 'rb').read()
    open(out, 'wb').write(data)
    print(out, len(data), 'bytes, sha256', hashlib.sha256(data).hexdigest())


if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
