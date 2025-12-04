import os
import stat
import shutil

if "__file__" in globals():
    # Running directly (normal Python execution)
    SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
    PROJECT_DIR = os.path.dirname(SCRIPT_DIR)
else:
    # Running under PlatformIO (executed via exec, __file__ missing)
    Import("env")
    PROJECT_DIR = env["PROJECT_DIR"]

def ensure_git_hook():

    git_dir = os.path.join(PROJECT_DIR, ".git")
    hooks_dir = os.path.join(git_dir, "hooks")

    # Not a git repo – just skip
    if not os.path.isdir(git_dir):
        return

    os.makedirs(hooks_dir, exist_ok=True)

    source = os.path.join(PROJECT_DIR, "scripts", "bumpVersion.sh")
    destination = os.path.join(hooks_dir, "pre-commit")

    if not os.path.isfile(source):
        return

    # Copy if missing or different
    copy = False
    if not os.path.exists(destination):
        copy = True
    else:
        with open(source, "rb") as f1, open(destination, "rb") as f2:
            if f1.read() != f2.read():
                copy = True

    if copy:
        print(f"Copying {source} --> {destination}")

        shutil.copy2(source, destination)
        st = os.stat(destination)
        os.chmod(destination, st.st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)

ensure_git_hook()
