"""Extract the patched block from ModbusControl.cpp for the host test.

usage: python3 extract_block.py <Prusa-Firmware-Buddy>/src/puppy/dwarf/modbus/ModbusControl.cpp > patched_block.inc

The block starts at the line "namespace tkr17536 {" that opens the namespace
(not the one-line forward declaration) and ends just before
"static bool should_check_fault_status". It is used unchanged by
tkr17536_hosttest.cpp.
"""
import sys


def main(path):
    src = open(path, encoding='utf-8').read()
    start = src.index('namespace tkr17536 {\n')
    end = src.index('static bool should_check_fault_status')
    sys.stdout.write(src[start:end])


if __name__ == '__main__':
    main(sys.argv[1])
