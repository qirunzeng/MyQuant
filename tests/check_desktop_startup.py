"""Start a deployed desktop app with isolated data and reject loading failures."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument("executable", type=Path)
parser.add_argument("--clean-qt-path", action="store_true")
args = parser.parse_args()
exe = args.executable.resolve(strict=True)
env = dict(os.environ, QT_FORCE_STDERR_LOGGING="1")
if args.clean_qt_path:
    for key in ("QT_PLUGIN_PATH", "QML_IMPORT_PATH", "QML2_IMPORT_PATH",
                "DYLD_LIBRARY_PATH", "DYLD_FRAMEWORK_PATH"):
        env.pop(key, None)
    env["PATH"] = (str(Path(os.environ["SystemRoot"]) / "System32")
                   if os.name == "nt" else "/usr/bin:/bin")
startup = None
if os.name == "nt":
    startup = subprocess.STARTUPINFO()
    startup.dwFlags = subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = 0

with tempfile.TemporaryDirectory(prefix="myquant-desktop-") as home:
    env["MYQUANT_HOME"] = home
    process = subprocess.Popen([str(exe)], cwd=exe.parent, env=env,
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                               startupinfo=startup)
    try:
        stdout, stderr = process.communicate(timeout=8)
        raise RuntimeError(f"App exited early ({process.returncode}): {stderr.decode(errors='replace')}")
    except subprocess.TimeoutExpired:
        process.terminate()
        try:
            stdout, stderr = process.communicate(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()
            stdout, stderr = process.communicate()
    log = stderr.decode(errors="replace")
    print(log)
    for failure in ("failed to load", "plugin could not be loaded", "plugin not found",
                    "Could not find", "ReferenceError:", "TypeError:"):
        if failure.lower() in log.lower():
            raise RuntimeError(f"Desktop startup failure: {log}")
    if not (Path(home) / "data" / "myquant.db").is_file():
        raise RuntimeError("SQLite database was not initialized")
print("PASS: deployed desktop starts with isolated data")
