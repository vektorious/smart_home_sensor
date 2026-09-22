#!/usr/bin/env python3
"""Render quick_reference.md as a two-page A5 handout.

Usage: python3 make_pdf.py (needs chromium and pdftotext on PATH).
Building occupies page 1; setup, readings, air quality and troubleshooting page 2.
The final check verifies that the handout has two pages and its last line fits.

Markdown supported here: headings, paragraphs, `-`/`1.` lists, `>` callouts,
tables and inline <img>. Sections listed in TWO_COLUMN set their lists in two
columns; callouts always stay full width.
"""
import re, base64, pathlib, html as H

PAGE_BREAK_BEFORE = None                        # heading to force onto page 2, or None to flow
TWO_COLUMN = {'3', '6', '7'}                    # section numbers set in two columns
COLS_WITH_TIP = set()                                # …and pull their callout into the columns
WIRING_WIDTH = '68%'                            # shrink to win a line on page 1

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

# Blocks are (kind, html); kind drives the two-column grouping below.
out, para, rows, items, quote = [], [], [], [], []
list_tag = 'ul'

def flush_para():
    if para:
        out.append(('p', '<p>' + inline(' '.join(para)) + '</p>'))
        para.clear()
def flush_list():
    if items:
        body = ''.join(f'<li>{inline(i)}</li>' for i in items)
        out.append(('list', f'<{list_tag}>{body}</{list_tag}>'))
        items.clear()
def flush_quote():
    if quote:
        out.append(('tip', '<p class="tip">' + inline(' '.join(quote)) + '</p>'))
        quote.clear()
def flush_table():
    if rows:
        body = ''.join(
            '<tr>' + ''.join(f'<{"th" if i==0 else "td"}>{inline(c.strip())}</{"th" if i==0 else "td"}>'
                             for c in r) + '</tr>'
            for i, r in enumerate(rows))
        out.append(('table', f'<table>{body}</table>'))
        rows.clear()
def flush_all():
    flush_para(); flush_list(); flush_quote(); flush_table()

for line in md.split('\n'):
    s = line.strip()
    if s.startswith('<img'):
        flush_all()
        out.append(('img', s.replace('src="../build_instructions_qr.png"', f'src="data:image/png;base64,{qr}"')
                           .replace('src="wiring.svg"', f'src="data:image/svg+xml;base64,{wiring}"')))
    elif s.startswith('> '):
        flush_para(); flush_list(); flush_table()
        quote.append(s[2:])
    elif s.startswith('- '):
        flush_para(); flush_quote(); flush_table()
        if list_tag != 'ul': flush_list()
        list_tag = 'ul'; items.append(s[2:])
    elif re.match(r'\d+\. ', s):
        flush_para(); flush_quote(); flush_table()
        if list_tag != 'ol': flush_list()
        list_tag = 'ol'; items.append(s.split('. ', 1)[1])
    elif s.startswith('|'):
        flush_para(); flush_list(); flush_quote()
        cells = [c for c in s.strip('|').split('|')]
        if set(''.join(cells).strip()) <= set('-: '):
            continue
        rows.append(cells)
    elif s.startswith('## '):
        flush_all()
        num, _, rest = s[3:].partition('. ')
        out.append(('h2', f'<h2><span class="num">{num}</span>{inline(rest)}</h2>', num))
    elif s.startswith('# '):
        flush_all(); out.append(('h1', f'<h1>{inline(s[2:])}</h1>'))
    elif not s:
        flush_all()
    else:
        para.append(s)
flush_all()

# Wrap the list-ish blocks of a two-column section in a .cols container. The
# callout that closes such a section stays full width, where it reads as a note
# on the whole section rather than on one column.
grouped, i = [], 0
while i < len(out):
    block = out[i]
    grouped.append(block[1])
    if block[0] == 'h2' and block[2] in TWO_COLUMN:
        run, i = [], i + 1
        while i < len(out) and out[i][0] == 'p':   # lead-in stays full width
            grouped.append(out[i][1]); i += 1
        kinds = ('list', 'table') + (('tip',) if block[2] in COLS_WITH_TIP else ())
        while i < len(out) and out[i][0] in kinds:
            run.append(out[i][1]); i += 1
        if run:
            grouped.append('<div class="cols">' + ''.join(run) + '</div>')
        continue
    i += 1

if PAGE_BREAK_BEFORE:
    split = next(i for i, b in enumerate(grouped)
                 if b.startswith('<h2') and PAGE_BREAK_BEFORE in b)
    pages = ['\n'.join(grouped[:split]), '\n'.join(grouped[split:])]
else:
    pages = ['\n'.join(grouped)]

CSS = """
@page { size: A5 portrait; margin: 9mm; }
* { box-sizing: border-box; }
:root { --ink: #14181c; --muted: #4d5a66; --accent: #0e6473; --tint: #e8f1f3;
        --rule: #d6dde1; }
body { margin: 0; font-family: "Source Sans 3", "DejaVu Sans", Arial, sans-serif;
       font-size: 8.9pt; line-height: 1.29; color: var(--ink);
       -webkit-print-color-adjust: exact; print-color-adjust: exact; }
.page + .page { break-before: page; }

h1 { font-size: 16pt; margin: 0 0 1.6mm; letter-spacing: -0.3px; color: var(--accent); }
.lede { font-size: 10pt; color: var(--muted); margin: 0 0 1.4mm; }
.kicker { margin: 0 0 3mm; padding-bottom: 2.4mm; border-bottom: 1pt solid var(--accent); }

h2 { font-size: 10.2pt; break-after: avoid; font-weight: 700; color: var(--accent);
     margin: 2.2mm 0 1mm; display: flex; align-items: center; gap: 1.8mm; }
h2 .num { flex: none; width: 4.8mm; height: 4.8mm; border-radius: 1mm;
          background: var(--accent); color: #fff; font-size: 8.4pt;
          display: inline-flex; align-items: center; justify-content: center; }

p { margin: 0 0 1.4mm; }
ul, ol { margin: 0 0 1.6mm; padding-left: 5.2mm; }
li { margin: 0 0 0.5mm; break-inside: avoid; }
li::marker { color: var(--accent); font-weight: 700; }

.tip { background: var(--tint); border-left: 1.2pt solid var(--accent);
       padding: 1.2mm 2mm; margin: 0 0 1.5mm; border-radius: 0 1mm 1mm 0; }
.cols { column-count: 2; column-gap: 5mm; column-rule: 0.4pt solid var(--rule); }
.cols ul, .cols ol { margin-bottom: 0; }

code { overflow-wrap: anywhere; font-family: "DejaVu Sans Mono", monospace; font-size: 8.8pt;
       background: #eef1f3; padding: 0 0.6mm; border-radius: 1px; }
img { width: 25mm; float: right; margin: 0 0 2mm 3mm; }
img.wiring { display: block; width: WIRING; float: none; margin: 1.5mm auto 2mm; }
table { border-collapse: collapse; width: 100%; margin: 1mm 0 1.6mm; font-size: 9pt; }
th, td { border: 0.4pt solid var(--rule); padding: 0.9mm 1.4mm; text-align: left; }
th { background: var(--tint); }
strong { font-weight: 600; }
""".replace('WIRING', WIRING_WIDTH)

# The title, intro and QR code form one masthead above the accent rule.
pages[0] = re.sub(r'(<h1>.*?)(<h2)', r'<div class="kicker">\1</div>\2', pages[0],
                  flags=re.S)
pages[0] = pages[0].replace('<p>You build', '<p class="lede">You build')

content = '\n'.join(f'<section class="page">{page}</section>' for page in pages)
doc = f"""<!doctype html><html><head><meta charset="utf-8"><style>{CSS}</style></head>
<body>{content}</body></html>"""
here = pathlib.Path(__file__).resolve().parent
(here/'quick_reference.html').write_text(doc)
import subprocess
subprocess.run(["chromium", "--headless", "--disable-gpu", "--no-sandbox",
                "--no-pdf-header-footer",
                f"--print-to-pdf={here/'quick_reference_a5.pdf'}",
                str(here/"quick_reference.html")], check=True)
(here/"quick_reference.html").unlink()


def check_fits(pdf):
    """Confirm both pages fit and the final paragraph is present."""
    tail = re.sub(r'[*`>-]', '', ' '.join(md.strip().split('\n')[-2:]))
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
            f"  …{needle}\n"
            "was not found. Check the layout above.")
    print("fit check: the last line of the markdown is on the page")


check_fits(here/'quick_reference_a5.pdf')
print("wrote quick_reference_a5.pdf")
