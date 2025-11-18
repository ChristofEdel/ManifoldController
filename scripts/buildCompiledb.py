import os
import subprocess

Import("env")
print("===================================================================================================")

# Avoid recursion: when we are already in the compiledb subprocess, skip
if os.environ.get("PIO_COMPILEDB_PHASE") != "1":
    env_name = env["PIOENV"]
    project_dir = env["PROJECT_DIR"]

    new_env = os.environ.copy()
    new_env["PIO_COMPILEDB_PHASE"] = "1"
    print("===================================================================================================")
    print (f"pio run -e '{env_name}' -t compiledb' in {project_dir}'")
    print("===================================================================================================")
    # run compiledb for this environment before the normal build
    subprocess.run(
        ["pio", "run", "-e", env_name, "-t", "compiledb"],
        cwd=project_dir,
        check=False,
        env=new_env,
    )
    print("===================================================================================================")
