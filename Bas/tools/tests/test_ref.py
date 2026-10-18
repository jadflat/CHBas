"""The port against the cart's Lua (tools/ref/compare.py): 3 tapping runs and
2 bot runs. Skipped without the cart or lupa."""
import importlib.util
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]

if not (ROOT / "assets" / "bas.p8").exists() or importlib.util.find_spec("lupa") is None:
    print("test_ref: skipped (needs assets/bas.p8 and pip install lupa)")
    sys.exit(0)
sys.exit(subprocess.call([sys.executable, str(ROOT / "tools" / "ref" / "compare.py"), "--seeds", "3",
                          "--bot", "2", "--frames", "2000", "--quiet"]))
