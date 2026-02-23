"""
_make_junction.py
Creates a directory junction (Windows symlink) from argv[1] -> argv[2].
Using Python avoids the mklink quoting/availability issues when the
script is launched from PowerShell.
Usage: python _make_junction.py <link_path> <target_path>
"""
import sys
import os
import subprocess

if len(sys.argv) != 3:
    print("Usage: _make_junction.py <link> <target>", file=sys.stderr)
    sys.exit(1)

link   = sys.argv[1].rstrip('\\')
target = sys.argv[2].rstrip('\\')

if os.path.exists(link):
    # Already exists — remove it first so we start clean
    subprocess.run(['cmd', '/c', 'rmdir', link], check=False)

result = subprocess.run(
    ['cmd', '/c', 'mklink', '/J', link, target],
    capture_output=True, text=True
)
if result.returncode != 0 or not os.path.exists(link):
    print(f"mklink failed: {result.stderr.strip() or result.stdout.strip()}", file=sys.stderr)
    sys.exit(1)

print(f"  Junction created: {link} -> {target}")
