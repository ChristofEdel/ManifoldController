from SCons.Script import Import
from SCons.Util import CLVar
from os.path import join
Import("env")

# This script changes the board variant include path from esp32s3 to arduino_nano_nora
# because the platformio framwork does not support variants automatically
# when usging framework=esp-idf with an Arduino variant board.

framework_dir = env.PioPlatform().get_package_dir("framework-arduinoespressif32")
wrong_variant_path = join(framework_dir, "variants", "esp32s3").replace("\\", "/")
correct_variant_path = join(framework_dir, "variants", "arduino_nano_nora").replace("\\", "/")

def _rewrite_cppincflags(env_, source_, target_):
    newResult = CLVar()
    for x in env_.get("CPPPATH", []):
        if wrong_variant_path in str(x):
            newResult.append("-I"+str(x).replace(wrong_variant_path, correct_variant_path))
        else:
            newResult.append("-I"+x)
    return newResult
 
env["_rewrite_cppincflags"] = _rewrite_cppincflags
env.Replace(_CPPINCFLAGS="${_rewrite_cppincflags(__env__, TARGET, SOURCE)}")

