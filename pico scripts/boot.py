import storage

# Read the mode from a file
try:
    with open("mode.txt", "r") as mode_file:
        mode = mode_file.read().strip()
except FileNotFoundError:
    mode = "read-only"


if mode == "Read-Write":
    storage.remount("/", readonly=False)
else:
    storage.remount("/", readonly=True)
