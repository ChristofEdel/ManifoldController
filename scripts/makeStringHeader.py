import sys
from pathlib import Path
from textwrap import dedent

# Helper: Convert the filename into the identifier for the DEFINE
# Convention: the file name is converted to uppercase and . gets changed to _
# e.g. scripts.js -> SCRIPTS_JS, styles.css -> STYLES_CSS
def make_identifier(path: Path) -> str:
    base = path.name
    return base.upper().replace("-", "_").replace(".", "_") + "_STRING"

# USage: makeStringConstantHeader <inputfile.xxx> <outputfile.h>
def main():
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} input.js output.h", file=sys.stderr)
        return 1

    src = Path(sys.argv[1])
    dst = Path(sys.argv[2])
    ident = make_identifier(src)
    text = src.read_text(encoding="utf-8")

    # Generate the raw string tag and ensure it is not contained in the text
    raw_tag = "RAW_STRING"
    while f"){raw_tag}\"" in text:
        raw_tag += "_R"

    # generate the code
    code = dedent(f"""\
        #pragma once
        #include <pgmspace.h>

        const char {ident}[] PROGMEM = R"{raw_tag}(""")
    code += "\n" + text;
    if not code.endswith("\n"):
        code += "\n"
    code += f"){raw_tag}\";"


    # ensure destination folder exists
    dst.parent.mkdir(parents=True, exist_ok=True)

    # do not rewrite unchanged file to avoid unneccessary builds
    if dst.exists():
        old = dst.read_text(encoding="utf-8")
        if old == code:
            return 0

    # write thie code to fhe file
    print(f"[makeStringConstantHeader] {src} -> {dst}")
    dst.write_text(code, encoding="utf-8")
    
    return 0

if __name__ == "__main__":
    raise SystemExit(main())