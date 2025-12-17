import subprocess
from pathlib import Path
import sys

downloads = Path.home() / "Downloads"
matches = list(downloads.glob("coredump*.elf"))

if not matches:
    print("no file matching coredump*.bin found in downloads")
    sys.exit(1)

newest_coredump = max(matches, key=lambda p: p.stat().st_mtime)

elf = Path(".pio/build/debug/firmware.elf")
if not elf.exists():
    print("missing .pio/build/debug/firmware.elf")
    sys.exit(1)

gdb = Path.home() / ".platformio/tools/tool-xtensa-esp-elf-gdb/bin/xtensa-esp32s3-elf-gdb.exe"

subprocess.run([
    str(gdb),
    str(elf),
    str(newest_coredump),
], check=False)
