import os
import re
import sys
import subprocess

ADDR2LINE = os.path.expanduser(
    "~/.platformio/packages/toolchain-xtensa-esp-elf/bin/xtensa-esp32-elf-addr2line"
)
PROJECT_DIR = os.path.abspath(".").replace("\\","/")
PACKAGE_DIR = os.environ.get("PACKAGE_DIR", os.path.join(os.path.expanduser("~"), ".platformio", "packages")).replace("\\","/")
XTENSA_DIR = PACKAGE_DIR + "/toolchain-xtensa-esp-elf/xtensa-esp32s3-elf"
ESPIDF_DIR = PACKAGE_DIR + "/framework-espidf/components"

ELF_FILE = os.path.join(PROJECT_DIR, ".pio/build/debug/firmware.elf")

# helper: replace PROJECT_DIR and PACKAGE_DIR in paths so we have more streamlined output
def normalisePath(p: str) -> str:
    p = p.replace("\\", "/")
    p = p.replace(PROJECT_DIR+"/",     "PROJECT ")
    p = re.sub(re.escape(XTENSA_DIR),  "XTENSA  ", p, flags=re.IGNORECASE)
    p = re.sub(re.escape(ESPIDF_DIR),  "ESPIDF  ", p, flags=re.IGNORECASE)
    p = re.sub(re.escape(PACKAGE_DIR), "PACKAGE ", p, flags=re.IGNORECASE)
    return p

def main():
    # take all arguments as addresses, complain if none ar given
    addrs = sys.argv[1:]
    if not addrs:
        print(f"Usage: {sys.argv[0]} <addr1> [addr2 ...]")
        sys.exit(1)

    # build and run the addr2line command
    # -f: show function names
    # -i: unwind inlines
    # -C: demangle function names
    cmd = [ADDR2LINE, "-e", ELF_FILE, "-fiC"] + addrs

    try:
        result = subprocess.run(
            cmd, check=True, capture_output=True, text=True
        )
    except FileNotFoundError:
        print(f"Error: addr2line not found at {ADDR2LINE}", file=sys.stderr)
        sys.exit(1)
    except subprocess.CalledProcessError as e:
        print("addr2line failed:", e.stderr, file=sys.stderr)
        sys.exit(1)

    # split the result into individual lines
    lines = [l.rstrip("\n") for l in result.stdout.splitlines()]

    # we now have line pairs for each address, 1st line function, 2nd line file
    it = iter(lines)
    print ("")
    print ("XTENSA  = " + XTENSA_DIR)
    print ("ESPIDF  = " + ESPIDF_DIR)
    print ("PACKAGE = " + PACKAGE_DIR)
    print ("")

    for function in it:
        # extract function and location
        try:
            location = next(it)
        except StopIteration:
            location = "??:0"

        function = function.strip() or "??"
        location = normalisePath(location.strip() or "??:0")

        # print the functions and where they are in a more palatable format
        if len(location) <= 78:
            # pad to 40 chars and print on same line
            print(f"{location:<80}{function}")
        else:
            # long function name: separate lines
            print(location)
            print(" " * 80 + function)
    print ("")
    
if __name__ == "__main__":
    main()
