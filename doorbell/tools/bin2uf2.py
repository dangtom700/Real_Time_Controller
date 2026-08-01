#!/usr/bin/env python3
"""Wrap a raw .bin in the UF2 container the Metro M4 bootloader expects.

    python tools/bin2uf2.py .pio/build/metro_gpio/firmware.bin

Writes firmware.uf2 beside the input.  Copy that onto the METROM4BOOT drive
(double-tap RESET to make it appear) and the board reboots into the program.

Why this exists.  With `framework =` empty there is no Arduino core, so there
is also no `pio run -t upload` recipe -- PlatformIO gets its upload rules from
the framework.  UF2 sidesteps the whole problem: the bootloader is a USB mass
storage device, so "flashing" is a file copy and needs no driver, no COM port
and no bossac.  Which matters on this machine specifically, where a missing
CP2102N driver already cost a serial port (see HANDOFF).

The format is deliberately trivial -- 512-byte blocks, each carrying at most
256 bytes of payload plus the address it belongs at, so the bootloader can
write blocks as they arrive and never has to buffer the image or trust that
the host copies it in order.  Spec: https://github.com/microsoft/uf2
"""

import struct
import sys
from pathlib import Path

UF2_MAGIC_START0 = 0x0A324655  # "UF2\n"
UF2_MAGIC_START1 = 0x9E5D5157
UF2_MAGIC_END = 0x0AB16F30

UF2_FLAG_FAMILY_ID = 0x00002000

# Family ID for SAMD51.  The bootloader refuses blocks whose family does not
# match, which is what stops a Metro M4 image being copied onto, say, a SAMD21
# board that presents an identically-shaped drive.
FAMILY_SAMD51 = 0x55114460

# Must agree with ORIGIN(FLASH) in samd51j19a.ld: the 16 KB bootloader sits
# below this, and a .bin carries no address of its own.
APP_START = 0x4000

PAYLOAD = 256


def convert(bin_path: Path, base: int = APP_START) -> Path:
    data = bin_path.read_bytes()
    if not data:
        sys.exit(f"{bin_path}: empty, nothing to convert")

    blocks = (len(data) + PAYLOAD - 1) // PAYLOAD
    out = bytearray()

    for i in range(blocks):
        chunk = data[i * PAYLOAD:(i + 1) * PAYLOAD]
        out += struct.pack(
            "<IIIIIIII",
            UF2_MAGIC_START0,
            UF2_MAGIC_START1,
            UF2_FLAG_FAMILY_ID,
            base + i * PAYLOAD,   # where this block belongs in flash
            len(chunk),
            i,
            blocks,
            FAMILY_SAMD51,
        )
        # Payload is padded to a fixed 476 bytes so every block is exactly 512
        # and the bootloader can seek by block index.  payloadSize above says
        # how much of it is real.
        out += chunk.ljust(476, b"\x00")
        out += struct.pack("<I", UF2_MAGIC_END)

    uf2_path = bin_path.with_suffix(".uf2")
    uf2_path.write_bytes(out)
    print(f"{bin_path.name}: {len(data)} bytes -> {uf2_path.name}: "
          f"{blocks} blocks, flashed to 0x{base:08X}")
    return uf2_path


def main() -> None:
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    path = Path(sys.argv[1])
    if not path.is_file():
        sys.exit(f"{path}: not found -- run `pio run -e metro_gpio` first")
    convert(path)


if __name__ == "__main__":
    main()
