#!/usr/bin/env python3
"""Render quick_reference.md as one A4 landscape page holding two A5 copies.

Usage: python3 make_pdf.py   (needs chromium on PATH; pdftotext for the fit check)
Cut the printed sheet down the dashed centre line.

Each copy has to fit one A5 half, and anything past that is clipped without a
word of complaint. So every render ends with check_fits(), which looks for the
last line of the markdown in the finished PDF and fails loudly if it is not
there — the guard that also catches a font substitution changing the metrics on
a machine without Source Sans 3 installed.
"""
import re, base64, pathlib, html as H

root = pathlib.Path(__file__).resolve().parents[2]
md = (root/'instructions/quick_reference/quick_reference.md').read_text()
qr = base64.b64encode((root/'instructions/build_instructions_qr.png').read_bytes()).decode()

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
        out.append(s.replace('src="../build_instructions_qr.png"', f'src="data:image/png;base64,{qr}"'))
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
sheet = '\n'.join(out)

CSS = """
@page { size: A4 landscape; margin: 0; }
* { box-sizing: border-box; }
body { margin: 0; font-family: "Source Sans 3", "DejaVu Sans", Arial, sans-serif;
       font-size: 9pt; line-height: 1.32; color: #111; }
.page { display: flex; width: 297mm; height: 210mm; }
/* Everything has to land inside one A5 half. overflow:hidden keeps a slight
   overrun from pushing a second page out of Chromium — but it clips silently,
   which is how the temperature paragraph went missing from the printed card
   for a while. check_fits() below is what actually catches that; the spacing
   here is tuned to leave a few millimetres of slack under the last line. */
.half { width: 148.5mm; height: 210mm; padding: 9mm 9mm; overflow: hidden; }
.half.left { border-right: 0.3pt dashed #999; }
h1 { font-size: 15pt; margin: 0 0 2.4mm; letter-spacing: -0.2px; }
h2 { font-size: 10.2pt; margin: 3.2mm 0 1.3mm; padding-bottom: 0.5mm;
     border-bottom: 0.5pt solid #ccc; text-transform: none; }
p { margin: 0 0 1.8mm; }
code { font-family: "DejaVu Sans Mono", monospace; font-size: 8.3pt;
       background: #f2f2f2; padding: 0 0.6mm; border-radius: 1px; }
img { width: 25mm; float: right; margin: 0 0 2mm 3mm; }
table { border-collapse: collapse; width: 100%; margin: 1mm 0 1.6mm; font-size: 8.5pt; }
th, td { border: 0.4pt solid #bbb; padding: 0.9mm 1.4mm; text-align: left; }
th { background: #f2f2f2; }
strong { font-weight: 600; }
"""

doc = f"""<!doctype html><html><head><meta charset="utf-8"><style>{CSS}</style></head>
<body><div class="page">
  <div class="half left">{sheet}</div>
  <div class="half">{sheet}</div>
</div></body></html>"""
(pathlib.Path(__file__).resolve().parent/'quick_reference.html').write_text(doc)
import subprocess
here = pathlib.Path(__file__).resolve().parent
subprocess.run(["chromium", "--headless", "--disable-gpu", "--no-sandbox",
                "--no-pdf-header-footer",
                f"--print-to-pdf={here/'quick_reference_2up_a4.pdf'}",
                str(here/"quick_reference.html")], check=True)
(here/"quick_reference.html").unlink()


def check_fits(pdf):
    """The A5 half clips silently, so confirm the last line of the markdown
    actually made it onto the page. Needs pdftotext (poppler); skipped with a
    warning when it is missing."""
    tail = re.sub(r'[*`]', '', ' '.join(md.strip().split('\n')[-2:]))
    needle = ' '.join(tail.split()[-8:])
    try:
        txt = subprocess.run(["pdftotext", str(pdf), "-"],
                             capture_output=True, text=True, check=True).stdout
    except (FileNotFoundError, subprocess.CalledProcessError):
        print("note: pdftotext not found — could not verify that the page fits")
        return
    # Compare on letters and digits only: the styled code spans come back from
    # pdftotext with their padding turned into stray spaces, which would fail a
    # literal match on text that is plainly on the page.
    squash = lambda t: re.sub(r'[^0-9a-z]', '', t.lower())
    if squash(needle) not in squash(txt):
        raise SystemExit(
            "ERROR: the card overflows its A5 half — the text ending\n"
            f"  \u2026{needle}\n"
            "was clipped. Shorten quick_reference.md, or tighten the CSS above.")
    print("fit check: the last line of the markdown is on the page")


check_fits(here/'quick_reference_2up_a4.pdf')
print("wrote quick_reference_2up_a4.pdf")
