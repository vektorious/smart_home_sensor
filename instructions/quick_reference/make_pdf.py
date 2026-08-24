#!/usr/bin/env python3
"""Render quick_reference.md as one A4 landscape page holding two A5 copies.

Usage: python3 make_pdf.py   (needs chromium on PATH)
Cut the printed sheet down the dashed centre line.
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
       font-size: 9.4pt; line-height: 1.42; color: #111; }
.page { display: flex; width: 297mm; height: 210mm; }
.half { width: 148.5mm; height: 210mm; padding: 11mm 10mm; overflow: hidden; }
.half.left { border-right: 0.3pt dashed #999; }
h1 { font-size: 16pt; margin: 0 0 3mm; letter-spacing: -0.2px; }
h2 { font-size: 10.6pt; margin: 4.6mm 0 1.6mm; padding-bottom: 0.6mm;
     border-bottom: 0.5pt solid #ccc; text-transform: none; }
p { margin: 0 0 2.2mm; }
code { font-family: "DejaVu Sans Mono", monospace; font-size: 8.6pt;
       background: #f2f2f2; padding: 0 0.6mm; border-radius: 1px; }
img { width: 27mm; float: right; margin: 0 0 2mm 3mm; }
table { border-collapse: collapse; width: 100%; margin: 1.2mm 0 2mm; font-size: 8.8pt; }
th, td { border: 0.4pt solid #bbb; padding: 1.1mm 1.6mm; text-align: left; }
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
print("wrote quick_reference_2up_a4.pdf")
