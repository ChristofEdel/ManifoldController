from pathlib import Path
Import("env")

project = Path(env["PROJECT_DIR"])

rules = [
    ("src/webserver/scripts.js",    "src/webserver/scripts_string.h",   "scripts/makeStringHeader.py"),
    ("src/webserver/styles.css",    "src/webserver/styles_string.h",    "scripts/makeStringHeader.py")
]

commands = []

for src, dst, tool in rules:
    srcPath = project / src
    dstPath = project / dst
    toolPath = project / tool
    # print(f"Adding action:  \"{toolPath}\" \"{srcPath}\" \"{dstPath}\"")   # for debugging

    node = env.Command(
        source=str(srcPath),
        target=str(dstPath),
        action=env.VerboseAction(
            f"python \"{toolPath}\" $SOURCE $TARGET",
            f"{toolPath} $SOURCE -> $TARGET",
        ),
    )

    commands += node if isinstance(node, list) else [node]

env.Depends("buildprog", commands)
