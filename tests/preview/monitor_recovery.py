"""Exercise the real supervisor with children that need no camera or display."""
import os
from pathlib import Path
import shutil
import signal
import subprocess
import sys
import tempfile
import time


CHILD = '''#!/usr/bin/python3
import os
from pathlib import Path
import signal
import time

name = Path(__file__).name
directory = Path(__file__).parent
def log(event):
    with (directory / "events").open("a") as stream:
        stream.write(f"{name} {event} {os.getpid()}\\n")
def stop(signum, frame):
    log("stop")
    raise SystemExit(0)
signal.signal(signal.SIGTERM, stop)
log("start")
if name == "camera-ui" and not (directory / "crashed").exists():
    (directory / "crashed").touch()
    os.kill(os.getpid(), signal.SIGKILL)
while True:
    time.sleep(0.05)
'''


with tempfile.TemporaryDirectory(prefix="tspi-monitor-test-") as temporary:
    directory = Path(temporary)
    monitor = directory / "camera-monitor"
    shutil.copy2(sys.argv[1], monitor)
    for name in ("media-service", "camera-ui"):
        child = directory / name
        child.write_text(CHILD)
        child.chmod(0o755)
    process = subprocess.Popen([str(monitor)], stderr=subprocess.PIPE, text=True)
    try:
        deadline = time.monotonic() + 8
        events = []
        while time.monotonic() < deadline:
            if (directory / "events").exists():
                events = (directory / "events").read_text().splitlines()
                if sum(line.startswith("camera-ui start ") for line in events) == 2:
                    break
            if process.poll() is not None:
                raise AssertionError("monitor exited before recovering the UI")
            time.sleep(0.02)
        assert sum(line.startswith("camera-ui start ") for line in events) == 2, events
        assert sum(line.startswith("media-service start ") for line in events) == 1, events
        process.send_signal(signal.SIGTERM)
        _, stderr = process.communicate(timeout=6)
        assert process.returncode == 0, stderr
        events = (directory / "events").read_text().splitlines()
        assert sum(line.startswith("camera-ui stop ") for line in events) == 1, events
        assert sum(line.startswith("media-service stop ") for line in events) == 1, events
        print("UI restarted independently; media pid preserved; shutdown reaped both children: passed")
    finally:
        if process.poll() is None:
            process.terminate()
            process.communicate(timeout=6)
