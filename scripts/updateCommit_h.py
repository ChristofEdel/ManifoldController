#!/usr/bin/env python3
import os
import re
import subprocess
import sys

if "__file__" in globals():
    # Running directly (normal Python execution)
    SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
    PROJECT_DIR = os.path.dirname(SCRIPT_DIR)
else:
    # Running under PlatformIO (executed via exec, __file__ missing)
    Import("env")
    PROJECT_DIR = env["PROJECT_DIR"]


# Helper function to run a GIT command and return its output
def GitCommand(args: list[str]) -> str | None:
    try:
        out = subprocess.check_output(["git"] + args, cwd=PROJECT_DIR, stderr=subprocess.DEVNULL)
        return out.decode("utf-8").strip()
    except Exception:
        return None

# Helper function to get the current commit by exmining the "HEAD"
# returns the short hash, and appends "-UNCOMMITTED" if there are uncommitted changes
def GetCommitString() -> str:
    # short hash of HEAD
    sha = GitCommand(["rev-parse", "--short", "HEAD"])
    if not sha:
        sha = "NOHEAD"

    status = GitCommand(["status", "--porcelain"])
    dirty = bool(status)
    
    if dirty:
        return f"{sha} + UNCOMMITTED"
    return sha

# Main function - update
def UpdateCommitHeader(source=None, target=None, env=None):
    commitHeaderPath = os.path.join(PROJECT_DIR, "src/commit.h")
    commitString = GetCommitString()

    # if the commit.h file does not exist, we create it with standard contents
    if not os.path.exists(commitHeaderPath):
        with open(commitHeaderPath, "w", encoding="utf-8") as f:
            f.write("\n".join([
                "#ifndef __COMMIT_H",
                "#define __COMMIT_H",
                "",
                "const char * COMMIT = \"xxx\";",
                "",
                "#endif",
            ]) + "\n")

    # read the file contents
    with open(commitHeaderPath, "r", encoding="utf-8") as f:
        currentContent = f.read()

    # replace the string within the definition
    pattern = re.compile(
        r'^(const\s+char\s*\*\s*COMMIT\s*=\s*)"[^"]*"(;\s*)$',
        re.MULTILINE,
    )

    if pattern.search(currentContent):
        newContent = pattern.sub(rf'\1"{commitString}"\2', currentContent)

        # write the file if missing or different
        write = newContent != currentContent
        if not os.path.exists(commitHeaderPath):
            write = True

        if write:
            with open(commitHeaderPath, "w", encoding="utf-8") as f:
                print("***")
                print("************** UPDATING COMMIT HEADER ***************************************")
                print("***")
                f.write(newContent)

UpdateCommitHeader()
