"""Require a valid default Mapping Jams URL in published editor builds."""
import os
from pathlib import Path
import re
import sys

label = r"[A-Za-z0-9](?:[A-Za-z0-9-]{0,61}[A-Za-z0-9])?"
url = os.environ.get("REYONDBROOM_JAM_DEFAULT_URL", "").strip().rstrip("/")
host = url.removeprefix("https://")
if len(host) > 253 or not re.fullmatch(rf"https://{label}(?:\.{label})+", url):
    raise SystemExit("Set REYONDBROOM_JAM_DEFAULT_URL to the public Mapping Jams HTTPS endpoint.")

header = Path(sys.argv[1])
header.parent.mkdir(parents=True, exist_ok=True)
header.write_text(f'#define REYONDBROOM_JAM_DEFAULT_URL "{url}"\n', encoding="ascii")
