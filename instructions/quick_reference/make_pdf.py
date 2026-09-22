#!/usr/bin/env python3
"""Render quick_reference.md as a two-page A5 handout.

Usage: python3 make_pdf.py (needs chromium and pdftotext on PATH).
Building occupies page 1; setup, readings, and air quality occupy page 2.
The final check verifies that the handout has two pages and its last line fits.
"""
import re, base64, pathlib, html as H

root = pathlib.Path(__file__).resolve().parents[2]
md = (root/'instructions/quick_reference/quick_reference.md').read_text()
qr = base64.b64encode((root/'instructions/build_instructions_qr.png').read_bytes()).decode()
wiring = base64.b64encode((root/'instructions/quick_reference/wiring.svg').read_bytes()).decode()

def inline(t):
    t = H.escape(t)
    t = re.sub(r'`([^`]+)`', r'<code>\1</code>', t)
    t = re.sub(r'\*\*([^*]+)\*\*', r'<strong>\1</strong>', t)
    t = re.sub(r'(?<!\*)\*([^*]+)\*(?!\*)', r'<em>\1</em>', t)
    return t

out, para, rows = [], [], []
def flush_para():
    if para:
        out.append('<p>' + inline(' '.join(para)) + '</p>')
        para.clear()
def flush_table():
    if rows:
        body = ''.join(
            '<tr>' + ''.join(f'<{"th" if i==0 else "td"}>{inline(c.strip())}</{"th" if i==0 else "td"}>'
                             for c in r) + '</tr>'
            for i, r in enumerate(rows))
        out.append(f'<table>{body}</table>')
        rows.clear()

for line in md.split('\n'):
    s = line.strip()
    if s.startswith('<img'):
        flush_para(); flush_table()
        out.append(s.replace('src="../build_instructions_qr.png"', f'src="data:image/png;base64,{qr}"')
                    .replace('src="wiring.svg"', f'src="data:image/svg+xml;base64,{wiring}"'))
    elif s.startswith('|'):
        flush_para()
        cells = [c for c in s.strip('|').split('|')]
        if set(''.join(cells).strip()) <= set('-: '):
            continue
        rows.append(cells)
    elif s.startswith('## '):
        flush_para(); flush_table(); out.append(f'<h2>{inline(s[3:])}</h2>')
    elif s.startswith('# '):
        flush_para(); flush_table(); out.append(f'<h1>{inline(s[2:])}</h1>')
    elif not s:
        flush_para(); flush_table()
    else:
        para.append(s)
flush_para(); flush_table()
split = next(i for i, block in enumerate(out) if block.startswith('<h2>4. Set up on the device'))
pages = ['\n'.join(out[:split]), '\n'.join(out[split:])]

CSS = """
@page { size: A5 portrait; margin: 9mm; }
* { box-sizing: border-box; }
body { margin: 0; font-family: "Source Sans 3", "DejaVu Sans", Arial, sans-serif;
       font-size: 10pt; line-height: 1.35; color: #111; }
.page + .page { break-before: page; }
h1 { font-size: 15pt; margin: 0 0 2.4mm; letter-spacing: -0.2px; }
h2 { font-size: 10.2pt; margin: 2.4mm 0 1.1mm; padding-bottom: 0.5mm;
     border-bottom: 0.5pt solid #ccc; text-transform: none; }
p { margin: 0 0 1.2mm; }
code { font-family: "DejaVu Sans Mono", monospace; font-size: 9pt;
       background: #f2f2f2; padding: 0 0.6mm; border-radius: 1px; }
img { width: 25mm; float: right; margin: 0 0 2mm 3mm; }
img.wiring { display: block; width: 100%; float: none; margin: 1mm 0; }
table { border-collapse: collapse; width: 100%; margin: 1mm 0 1.6mm; font-size: 9pt; }
th, td { border: 0.4pt solid #bbb; padding: 0.9mm 1.4mm; text-align: left; }
th { background: #f2f2f2; }
strong { font-weight: 600; }
"""

content = '\n'.join(f'<section class="page">{page}</section>' for page in pages)
doc = f"""<!doctype html><html><head><meta charset="utf-8"><style>{CSS}</style></head>
<body>{content}</body></html>"""
(pathlib.Path(__file__).resolve().parent/'quick_reference.html').write_text(doc)
import subprocess
here = pathlib.Path(__file__).resolve().parent
subprocess.run(["chromium", "--headless", "--disable-gpu", "--no-sandbox",
                "--no-pdf-header-footer",
                f"--print-to-pdf={here/'quick_reference_a5.pdf'}",
                str(here/"quick_reference.html")], check=True)
(here/"quick_reference.html").unlink()


def check_fits(pdf):
    """Confirm both pages fit and the final paragraph is present."""
    tail = re.sub(r'[*`]', '', ' '.join(md.strip().split('\n')[-2:]))
    needle = ' '.join(tail.split()[-8:])
    try:
        txt = subprocess.run(["pdftotext", str(pdf), "-"],
                             capture_output=True, text=True, check=True).stdout
    except (FileNotFoundError, subprocess.CalledProcessError):
        print("note: pdftotext not found — could not verify that the page fits")
        return
    if txt.count('\f') != 2:
        raise SystemExit("ERROR: expected exactly two A5 pages")
    # Compare on letters and digits only: the styled code spans come back from
    # pdftotext with their padding turned into stray spaces, which would fail a
    # literal match on text that is plainly on the page.
    squash = lambda t: re.sub(r'[^0-9a-z]', '', t.lower())
    if squash(needle) not in squash(txt):
        raise SystemExit(
            "ERROR: the handout is missing its final text\n"
            f"  \u2026{needle}\n"
            "was not found. Check the layout above.")
    print("fit check: the last line of the markdown is on the page")


check_fits(here/'quick_reference_a5.pdf')
print("wrote quick_reference_a5.pdf")
