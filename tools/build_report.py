#!/usr/bin/env python3
"""Build the updated report; preserve the original four evidence pages.

Requires PyMuPDF and the user's original 2026-10-21 report (not shipped here).
Example: python tools/build_report.py --original /path/to/original.pdf --output report.pdf
"""
import argparse
import html
from pathlib import Path
import re
import tempfile
import fitz

ROOT = Path(__file__).resolve().parents[1]
CSS = """
@font-face {font-family: NotoTC; src: url(NotoTC.otf);}
body {font-family: NotoTC; font-size: 10pt; line-height: 1.5; color: #253133; margin: 0;}
h1 {font-size: 25pt; line-height: 1.2; font-weight: normal; color: #164a41; margin: 0 0 9pt;}
h2 {font-size: 19pt; line-height: 1.35; font-weight: normal; color: #164a41; margin: 0 0 14pt;}
h2.subtitle {font-size: 17pt; margin-bottom: 12pt;}
h3 {font-size: 12.5pt; font-weight: normal; color: #164a41; margin: 15pt 0 7pt;}
p {margin: 6pt 0 9pt;}
p.lead {font-size: 11pt; color: #4d605b; margin-bottom: 15pt;}
.eyebrow {font-size: 8pt; color: #617d74; margin: 0 0 10pt;}
.small {font-size: 8.8pt; line-height: 1.5; color: #50635e;}
.callout {background: #edf5f1; padding: 11pt 12pt; margin: 9pt 0 12pt; font-size: 10pt;}
table {border-collapse: collapse; width: 100%; margin: 4pt 0 10pt; font-size: 9pt; line-height: 1.45;}
th {background: #e4eee8; color: #164a41; font-weight: normal;}
td, th {border-bottom: 0.5pt solid #b8cbc0; padding: 6pt 7pt; text-align: left; vertical-align: top;}
.numeric td, .numeric th {text-align: center;}
a {color: #246d64; text-decoration: none;}
b {color: #164a41;}
"""


def add_link_rectangles(page, source_text):
    """Retain useful links even on PyMuPDF builds without Story link export."""
    for url, label in re.findall(r'<a href="([^"]+)">([^<]+)</a>', source_text):
        text = html.unescape(label)
        for rect in page.search_for(text):
            page.insert_link({'kind': fitz.LINK_URI, 'from': rect, 'uri': html.unescape(url)})


def frame(page, number, total, appendix=False):
    green = (0.086, 0.29, 0.255)
    label = 'SMARTTREE | ORIGINAL V1 TEST EVIDENCE' if appendix else 'SMARTTREE | WEEK 3 + V2.0 UPDATE'
    page.insert_text((46, 30), label, fontsize=8, color=green)
    page.draw_line((46, 40), (549, 40), color=(0.67, 0.76, 0.71), width=0.5)
    page.draw_line((46, 803), (549, 803), color=(0.67, 0.76, 0.71), width=0.5)
    page.insert_textbox(fitz.Rect(46, 808, 549, 828), f'{number} / {total}', fontsize=9, align=1, color=green)


def repair_text_maps(doc):
    # Noto's alternate digit CIDs are used by shaping but absent from the source
    # ToUnicode CMap. Add the missing CID mappings; visual glyphs remain intact.
    pairs = '\n'.join(f'<{0xF6B1+i:04x}> <{0x30+i:04x}>' for i in range(10))
    for xref in range(1, doc.xref_length()):
        if not doc.xref_is_stream(xref):
            continue
        data = doc.xref_stream(xref)
        if b'begincmap' not in data:
            continue
        text = data.decode('latin1')
        text = re.sub(r'(<000e>\s+)<2011>', r'\g<1><002d>', text, flags=re.I)
        text = text.replace('endcmap', f'10 beginbfchar\n{pairs}\nendbfchar\nendcmap')
        doc.update_stream(xref, text.encode('latin1'))


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--original', required=True, type=Path)
    ap.add_argument('--output', required=True, type=Path)
    args = ap.parse_args()
    original = fitz.open(args.original)
    assert len(original) == 8, 'Expected the eight-page 2026-10-21 report'
    font_entry = next(f for f in original[0].get_fonts() if 'Noto' in f[3])
    font_data = original.extract_font(font_entry[0])[3]

    sections = re.findall(r'<section>(.*?)</section>', (ROOT/'docs/report_v2.0.html').read_text(), re.S)
    assert len(sections) == 4, 'Four main pages plus four evidence pages meet the eight-page limit'
    doc = fitz.open()
    total = len(sections) + 4
    with tempfile.TemporaryDirectory(prefix='smarttree-report-') as tmp:
        (Path(tmp)/'NotoTC.otf').write_bytes(font_data)
        archive = fitz.Archive(tmp)
        for n, content in enumerate(sections, 1):
            page = doc.new_page(width=595.276, height=841.89)
            page_css = CSS
            if n >= 2:
                page_css += ('body {font-size: 9.6pt; line-height: 1.45;}'
                             'h3 {margin-top: 11pt; margin-bottom: 6pt;}'
                             'td, th {padding: 5pt 6pt;} table {font-size: 8.8pt;}')
            story = fitz.Story(html=content, user_css=page_css, archive=archive)
            more, filled = story.place(fitz.Rect(46, 58, 549, 784))
            if more:
                raise ValueError(f'Content does not fit on page {n}; filled={filled}')
            device = fitz.DocumentWriter(str(Path(tmp)/f'page-{n}.pdf'))
            dev = device.begin_page(page.rect)
            story.draw(dev)
            device.end_page()
            device.close()
            rendered = fitz.open(Path(tmp)/f'page-{n}.pdf')
            page.show_pdf_page(page.rect, rendered, 0)
            frame(page, n, total)
            print(f'Page {n}: content bottom={fitz.Rect(filled).y1:.1f} pt')

        # Keep all original table and photograph regions intact.
        doc.insert_pdf(original, from_page=4, to_page=7)
        for i in range(len(sections), total):
            page = doc[i]
            page.add_redact_annot(fitz.Rect(42, 16, 553, 36), fill=(1, 1, 1))
            page.add_redact_annot(fitz.Rect(42, 806, 553, 833), fill=(1, 1, 1))
            if i < len(sections) + 2:
                page.add_redact_annot(fitz.Rect(44, 758, 551, 788), fill=(1, 1, 1))
            page.apply_redactions(images=0, graphics=0)
            frame(page, i+1, total, appendix=True)
            if i < len(sections) + 2:
                note = ('完整檔名為 WIN_20261021_時_分_秒_Pro.jpg，可由時間回查。欄位為原截圖可讀數值，'
                        '不是即時延遲或成功率；照片對照見第 7～8 頁。')
                result = page.insert_htmlbox(fitz.Rect(47, 758, 548, 790), note,
                                             css=CSS+'body {font-size: 8.3pt;}', archive=archive)
                assert result[0] >= 0 and result[1] >= 0.99, result
    doc.set_metadata({'title': '智慧植栽守護站：第三週成果報告與 v2.0 程式更新',
                      'author': '莊啓文、林品睿', 'subject': '既有實測證據、v2.0 架構與待實機驗證清單',
                      'keywords': 'SmartTree, AMB82-mini, AmebaPro2 4.0.9, YOLOv7-tiny'})
    args.output.parent.mkdir(parents=True, exist_ok=True)
    doc.subset_fonts(fallback=True)
    repair_text_maps(doc)
    # Reopen once so text searches see the corrected maps rather than cached ones.
    data = doc.tobytes(garbage=4, deflate=True)
    doc.close()
    doc = fitz.open(stream=data, filetype='pdf')
    for i, content in enumerate(sections):
        add_link_rectangles(doc[i], content)
    assert len(doc[len(sections)-1].get_links()) == 5, 'Expected repository, three videos, official source links'
    doc.save(args.output, garbage=4, deflate=True)
    doc.close()
    print(args.output)


if __name__ == '__main__':
    main()
