#!/usr/bin/env python3
import argparse
import subprocess
import sys
import re
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor, as_completed

ESPOTA = Path.home() / ".platformio/packages/framework-arduinoespressif32/tools/espota.py"
FIRMWARE_BIN = Path(".pio/build/debug/firmware.bin")


def upload_with_log(ip: str, firmware: Path):
        print(f"Starting {ip}")
        return upload(ip, firmware)

def upload(ip: str, firmware: Path):
    cmd = [
        sys.executable,
        str(ESPOTA),
        "--ip", ip,
        "--file", str(firmware)
    ]
    p = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    return ip, p.returncode, p.stdout

def sanitizeOutput(out: str) -> str:
    return re.sub(r"\.{30,}", "..............................", out)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--parallel", action="store_true", help="Run uploads in parallel")
    ap.add_argument("ips", nargs="+", help="destination IP addresses")
    args = ap.parse_args()

    firmware = FIRMWARE_BIN
    if not firmware.is_file():
        sys.exit(f"firmware not found: {firmware}")

    if not ESPOTA.is_file():
        sys.exit(f"espota.py not found: {ESPOTA}")

    ok = fail = 0
    if args.parallel:
        with ThreadPoolExecutor(max_workers=len(args.ips)) as pool:
            futures = [
                pool.submit(upload_with_log, ip, firmware)
                for ip in args.ips
            ]
            for f in as_completed(futures):
                ip, rc, out = f.result()
                if rc == 0:
                    ok += 1
                    print(f"[{ip}] OK")
                else:
                    fail += 1
                    print(f"[{ip}] FAIL\n{sanitizeOutput(out)}")
    else:
        for ip in args.ips:
            ip, rc, out = upload_with_log(ip, firmware)
            if rc == 0:
                ok += 1
                print(f"[{ip}] OK")
            else:
                fail += 1
                print(f"[{ip}] FAIL\n{sanitizeOutput(out)}")

    print(f"\nDONE: {ok} OK, {fail} FAIL")
    sys.exit(0 if fail == 0 else 1)

if __name__ == "__main__":
    main()