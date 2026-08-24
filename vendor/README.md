# Vendored ESP Web Tools

[ESP Web Tools](https://esphome.github.io/esp-web-tools/) **10.4.0**, Apache-2.0
(see `LICENSE`). Copied here rather than loaded from unpkg: a page that pulls a
script from a CDN hands every visitor's IP address to a third party before they
have clicked anything, which is a data transfer to explain in a privacy notice
for no benefit.

## Updating

The entry point is tiny and lazily `import()`s hashed sibling chunks — one per
supported chip — so copying `install-button.js` alone produces a page that loads
fine and then fails when someone clicks **Connect**. Fetch the whole graph:

```bash
cd web-flasher/vendor
rm -f *.js
python3 - <<'PY'
import re, subprocess
from pathlib import Path
BASE = "https://unpkg.com/esp-web-tools@10/dist/web/"
SPEC = re.compile(r'(?:from|import)\s*\(?\s*"(\./[^"]+?\.js)(?:\?module)?"')
seen, queue = set(), ["install-button.js"]
while queue:
    name = queue.pop(0)
    if name in seen:
        continue
    seen.add(name)
    body = subprocess.run(["curl", "-sSfL", BASE + name + "?module"],
                          capture_output=True, text=True, check=True).stdout
    Path(name).write_text(body, encoding="utf-8")
    queue += [m.group(1)[2:] for m in SPEC.finditer(body)]
print(len(seen), "files")
PY
curl -sSfL -o LICENSE https://unpkg.com/esp-web-tools@10/LICENSE
```

Then check that nothing still points outside this directory:

```bash
python3 - <<'PY'
import re
from pathlib import Path
SPEC = re.compile(r'(?:from|import)\s*\(?\s*"(\./[^"]+?\.js)(?:\?module)?"')
missing = {(f.name, m.group(1)[2:]) for f in Path(".").glob("*.js")
           for m in SPEC.finditer(f.read_text(encoding="utf-8"))
           if not Path(m.group(1)[2:]).is_file()}
print("unresolved:", missing or "none")
PY
```

The `?module` suffix in those specifiers is a query string; static hosting
ignores it and serves the plain `.js` file, so the names above need no rewriting.

## What still reaches the network

Nothing automatically. The bundle contains links a user may *click* — CH34x and
CP210x driver downloads, the ESP Web Tools homepage, a Home Assistant redirect —
which are ordinary outbound links, not resource loads.
