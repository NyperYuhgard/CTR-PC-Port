#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
CTR Native Modding Guide - PDF Documentation Generator
"""

import sys
import os

from reportlab.lib.pagesizes import A4
from reportlab.lib.units import inch, mm, cm
from reportlab.lib.styles import ParagraphStyle, getSampleStyleSheet
from reportlab.lib.enums import TA_LEFT, TA_CENTER, TA_JUSTIFY, TA_RIGHT
from reportlab.lib import colors
from reportlab.platypus import (
    SimpleDocTemplate, Paragraph, Spacer, Table, TableStyle,
    PageBreak, KeepTogether, ListFlowable, ListItem, Preformatted
)
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.pdfbase.pdfmetrics import registerFontFamily

# ━━ Color Palette ━━
ACCENT       = colors.HexColor('#22748f')
TEXT_PRIMARY  = colors.HexColor('#232627')
TEXT_MUTED    = colors.HexColor('#767d83')
BG_SURFACE   = colors.HexColor('#d5d9dc')
BG_PAGE      = colors.HexColor('#eceeef')

TABLE_HEADER_COLOR = ACCENT
TABLE_HEADER_TEXT  = colors.white
TABLE_ROW_EVEN     = colors.white
TABLE_ROW_ODD      = BG_SURFACE

# ━━ Font Registration ━━
pdfmetrics.registerFont(TTFont('LiberationSerif', '/usr/share/fonts/truetype/liberation/LiberationSerif-Regular.ttf'))
pdfmetrics.registerFont(TTFont('LiberationSerif-Bold', '/usr/share/fonts/truetype/liberation/LiberationSerif-Bold.ttf'))
pdfmetrics.registerFont(TTFont('Carlito', '/usr/share/fonts/truetype/english/Carlito-Regular.ttf'))
pdfmetrics.registerFont(TTFont('Carlito-Bold', '/usr/share/fonts/truetype/english/Carlito-Bold.ttf'))
pdfmetrics.registerFont(TTFont('DejaVuSans', '/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf'))
pdfmetrics.registerFont(TTFont('DejaVuSans-Bold', '/usr/share/fonts/truetype/dejavu/DejaVuSansMono-Bold.ttf'))

registerFontFamily('LiberationSerif', normal='LiberationSerif', bold='LiberationSerif-Bold')
registerFontFamily('Carlito', normal='Carlito', bold='Carlito-Bold')
registerFontFamily('DejaVuSans', normal='DejaVuSans', bold='DejaVuSans-Bold')

# ━━ Page Setup ━━
PAGE_W, PAGE_H = A4
LEFT_MARGIN = 60
RIGHT_MARGIN = 60
TOP_MARGIN = 50
BOTTOM_MARGIN = 50
CONTENT_W = PAGE_W - LEFT_MARGIN - RIGHT_MARGIN

# ━━ Styles ━━
styles = getSampleStyleSheet()

style_cover_title = ParagraphStyle(
    'CoverTitle', fontName='LiberationSerif', fontSize=32, leading=40,
    textColor=colors.white, alignment=TA_CENTER, spaceAfter=12
)
style_cover_subtitle = ParagraphStyle(
    'CoverSubtitle', fontName='Carlito', fontSize=16, leading=22,
    textColor=colors.HexColor('#b0d4e3'), alignment=TA_CENTER, spaceAfter=8
)
style_cover_meta = ParagraphStyle(
    'CoverMeta', fontName='Carlito', fontSize=11, leading=16,
    textColor=colors.HexColor('#8cbac9'), alignment=TA_CENTER
)

style_h1 = ParagraphStyle(
    'H1', fontName='LiberationSerif', fontSize=22, leading=28,
    textColor=ACCENT, spaceBefore=18, spaceAfter=10, alignment=TA_LEFT
)
style_h2 = ParagraphStyle(
    'H2', fontName='LiberationSerif', fontSize=16, leading=22,
    textColor=TEXT_PRIMARY, spaceBefore=14, spaceAfter=8, alignment=TA_LEFT
)
style_h3 = ParagraphStyle(
    'H3', fontName='LiberationSerif', fontSize=13, leading=18,
    textColor=ACCENT, spaceBefore=10, spaceAfter=6, alignment=TA_LEFT
)
style_body = ParagraphStyle(
    'Body', fontName='LiberationSerif', fontSize=10.5, leading=17,
    textColor=TEXT_PRIMARY, alignment=TA_JUSTIFY, spaceAfter=6,
    firstLineIndent=0
)
style_body_indent = ParagraphStyle(
    'BodyIndent', parent=style_body, leftIndent=20
)
style_code = ParagraphStyle(
    'Code', fontName='DejaVuSans', fontSize=8.5, leading=13,
    textColor=colors.HexColor('#1a1a2e'), backColor=colors.HexColor('#f0f2f5'),
    leftIndent=12, rightIndent=12, spaceBefore=4, spaceAfter=4,
    borderPadding=(6, 6, 6, 6)
)
style_bullet = ParagraphStyle(
    'Bullet', fontName='LiberationSerif', fontSize=10.5, leading=17,
    textColor=TEXT_PRIMARY, alignment=TA_LEFT, leftIndent=24, bulletIndent=12,
    spaceAfter=3
)
style_note = ParagraphStyle(
    'Note', fontName='Carlito', fontSize=9.5, leading=14,
    textColor=TEXT_MUTED, leftIndent=16, rightIndent=16,
    spaceBefore=4, spaceAfter=4, borderPadding=(6,6,6,6),
    backColor=colors.HexColor('#f5f8fa')
)
style_table_header = ParagraphStyle(
    'TableHeader', fontName='LiberationSerif', fontSize=9.5, leading=13,
    textColor=TABLE_HEADER_TEXT, alignment=TA_LEFT
)
style_table_cell = ParagraphStyle(
    'TableCell', fontName='LiberationSerif', fontSize=9, leading=13,
    textColor=TEXT_PRIMARY, alignment=TA_LEFT
)
style_table_code = ParagraphStyle(
    'TableCode', fontName='DejaVuSans', fontSize=8, leading=11,
    textColor=colors.HexColor('#1a1a2e'), alignment=TA_LEFT
)
style_footer = ParagraphStyle(
    'Footer', fontName='Carlito', fontSize=8, leading=10,
    textColor=TEXT_MUTED, alignment=TA_CENTER
)
style_toc = ParagraphStyle(
    'TOC', fontName='LiberationSerif', fontSize=11, leading=20,
    textColor=TEXT_PRIMARY, alignment=TA_LEFT, leftIndent=0
)
style_toc_sub = ParagraphStyle(
    'TOCSub', fontName='LiberationSerif', fontSize=10, leading=18,
    textColor=TEXT_MUTED, alignment=TA_LEFT, leftIndent=20
)

# ━━ Helper Functions ━━

def make_table(headers, rows, col_widths=None):
    """Create a styled table."""
    header_row = [Paragraph(h, style_table_header) for h in headers]
    data = [header_row]
    for row in rows:
        data.append([Paragraph(str(c), style_table_cell) if not isinstance(c, Paragraph) else c for c in row])

    if col_widths is None:
        col_widths = [CONTENT_W / len(headers)] * len(headers)

    t = Table(data, colWidths=col_widths, repeatRows=1)
    style_cmds = [
        ('BACKGROUND', (0, 0), (-1, 0), TABLE_HEADER_COLOR),
        ('TEXTCOLOR', (0, 0), (-1, 0), TABLE_HEADER_TEXT),
        ('FONTNAME', (0, 0), (-1, 0), 'LiberationSerif'),
        ('FONTSIZE', (0, 0), (-1, 0), 9.5),
        ('BOTTOMPADDING', (0, 0), (-1, 0), 8),
        ('TOPPADDING', (0, 0), (-1, 0), 8),
        ('GRID', (0, 0), (-1, -1), 0.5, colors.HexColor('#c0c4c8')),
        ('VALIGN', (0, 0), (-1, -1), 'TOP'),
        ('LEFTPADDING', (0, 0), (-1, -1), 8),
        ('RIGHTPADDING', (0, 0), (-1, -1), 8),
        ('TOPPADDING', (0, 1), (-1, -1), 5),
        ('BOTTOMPADDING', (0, 1), (-1, -1), 5),
    ]
    for i in range(1, len(data)):
        bg = TABLE_ROW_EVEN if i % 2 == 1 else TABLE_ROW_ODD
        style_cmds.append(('BACKGROUND', (0, i), (-1, i), bg))
    t.setStyle(TableStyle(style_cmds))
    return t

def code_block(text):
    """Create a code block paragraph."""
    safe = text.replace('&', '&amp;').replace('<', '&lt;').replace('>', '&gt;')
    lines = safe.split('\n')
    return Preformatted('\n'.join(lines), style_code)

def bullet(text):
    return Paragraph('<bullet>&bull;</bullet> ' + text, style_bullet)

def note_box(text):
    return Paragraph(text, style_note)

def h1(text):
    return Paragraph('<b>' + text + '</b>', style_h1)

def h2(text):
    return Paragraph('<b>' + text + '</b>', style_h2)

def h3(text):
    return Paragraph('<b>' + text + '</b>', style_h3)

def p(text):
    return Paragraph(text, style_body)

def p_indent(text):
    return Paragraph(text, style_body_indent)

# ━━ Page Template Callbacks ━━

def on_first_page(canvas, doc):
    """Draw cover page background."""
    canvas.saveState()
    # Full-page dark background
    canvas.setFillColor(colors.HexColor('#1a3a4a'))
    canvas.rect(0, 0, PAGE_W, PAGE_H, fill=1, stroke=0)

    # Accent bar at top
    canvas.setFillColor(ACCENT)
    canvas.rect(0, PAGE_H - 8, PAGE_W, 8, fill=1, stroke=0)

    # Decorative line
    canvas.setStrokeColor(colors.HexColor('#3a8aaa'))
    canvas.setLineWidth(1.5)
    canvas.line(LEFT_MARGIN, PAGE_H * 0.55, PAGE_W - RIGHT_MARGIN, PAGE_H * 0.55)

    # Small decorative box
    canvas.setFillColor(colors.HexColor('#22748f'))
    canvas.rect(LEFT_MARGIN, PAGE_H * 0.54 - 3, 40, 6, fill=1, stroke=0)

    canvas.restoreState()

def on_later_pages(canvas, doc):
    """Draw header/footer on content pages."""
    canvas.saveState()

    # Top accent line
    canvas.setStrokeColor(ACCENT)
    canvas.setLineWidth(1.2)
    canvas.line(LEFT_MARGIN, PAGE_H - 30, PAGE_W - RIGHT_MARGIN, PAGE_H - 30)

    # Header text
    canvas.setFont('Carlito', 8)
    canvas.setFillColor(TEXT_MUTED)
    canvas.drawString(LEFT_MARGIN, PAGE_H - 26, "CTR Native - Modding Guide")

    # Footer
    canvas.setStrokeColor(colors.HexColor('#d0d4d8'))
    canvas.setLineWidth(0.5)
    canvas.line(LEFT_MARGIN, 35, PAGE_W - RIGHT_MARGIN, 35)
    canvas.setFont('Carlito', 8)
    canvas.setFillColor(TEXT_MUTED)
    canvas.drawCentredString(PAGE_W / 2, 22, f"Page {doc.page}")

    canvas.restoreState()

# ━━ Build Document ━━

OUTPUT_PATH = '/home/z/my-project/download/CTR_Native_Modding_Guide.pdf'

doc = SimpleDocTemplate(
    OUTPUT_PATH,
    pagesize=A4,
    leftMargin=LEFT_MARGIN,
    rightMargin=RIGHT_MARGIN,
    topMargin=TOP_MARGIN,
    bottomMargin=BOTTOM_MARGIN,
    title='CTR Native Modding Guide',
    author='Z.ai',
    subject='Documentation for creating mods in CTR Native (Crash Team Racing PC Port)',
    creator='Z.ai'
)

story = []

# ━━ COVER PAGE ━━

story.append(Spacer(1, PAGE_H * 0.22))
story.append(Paragraph('<b>CTR Native</b>', style_cover_title))
story.append(Paragraph('<b>Modding Guide</b>', style_cover_title))
story.append(Spacer(1, 16))
story.append(Paragraph('Complete documentation for creating Lua-based mods', style_cover_subtitle))
story.append(Paragraph('for Crash Team Racing Native PC Port', style_cover_subtitle))
story.append(Spacer(1, 30))
story.append(Paragraph('Version 1.0  |  2026', style_cover_meta))
story.append(Paragraph('Lua 5.4 Scripting API Reference', style_cover_meta))
story.append(PageBreak())

# ━━ TABLE OF CONTENTS ━━

story.append(Spacer(1, 20))
story.append(h1('Table of Contents'))
story.append(Spacer(1, 12))

toc_items = [
    ('1.', 'Introduction', False),
    ('2.', 'Architecture Overview', False),
    ('3.', 'Getting Started', False),
    ('4.', 'Mod Structure', False),
    ('5.', 'Lua API Reference', False),
    ('5.1', 'mod.log(message)', True),
    ('5.2', 'mod.getModPath()', True),
    ('5.3', 'mod.readFile(path)', True),
    ('5.4', 'mod.writeFile(path, data)', True),
    ('5.5', 'mod.hook(name, callback)', True),
    ('6.', 'Hooks Reference', False),
    ('7.', 'Menu Integration', False),
    ('8.', 'File Overriding System', False),
    ('9.', 'Example Mod: Speed Display', False),
    ('10.', 'Best Practices', False),
    ('11.', 'Troubleshooting', False),
]

for num, title, is_sub in toc_items:
    st = style_toc_sub if is_sub else style_toc
    story.append(Paragraph(f'<b>{num}</b>  {title}', st))

story.append(PageBreak())

# ━━ 1. INTRODUCTION ━━

story.append(h1('1. Introduction'))
story.append(Spacer(1, 6))

story.append(p(
    'CTR Native is a decompilation-based PC port of Crash Team Racing that runs natively on modern '
    'operating systems. Beyond faithfully recreating the original PlayStation experience, the native '
    'port introduces a modding system that allows the community to extend and customize the game using '
    'Lua scripts. This document provides a comprehensive guide to the modding API, from the basics of '
    'creating your first mod to the complete function reference and best practices for production-quality '
    'mod development.'
))
story.append(p(
    'The mod system is built on Lua 5.4, providing a sandboxed scripting environment where mods can '
    'register hooks into the game loop, read and write custom files, and log diagnostic information. '
    'Each mod runs in a shared Lua virtual machine with access to the global <font name="DejaVuSans" size="9">mod</font> '
    'table, which exposes the entire scripting API. Mods are discovered at startup by scanning the '
    '<font name="DejaVuSans" size="9">mods/</font> directory for subdirectories containing a '
    '<font name="DejaVuSans" size="9">main.lua</font> entry point.'
))
story.append(p(
    'The modding system supports up to 64 concurrent mods, each of which can be individually toggled '
    'on or off from the in-game "MODS" menu accessible from the main menu. Toggling a mod changes its '
    'enabled state, but the actual loading of mod scripts occurs at engine initialization time. This '
    'means that changes to the enabled state take effect on the next game startup, not immediately '
    'during gameplay.'
))

# ━━ 2. ARCHITECTURE OVERVIEW ━━

story.append(Spacer(1, 18))
story.append(h1('2. Architecture Overview'))
story.append(Spacer(1, 6))

story.append(p(
    'The CTR Native mod system follows a host-guest architecture where the C engine acts as the host '
    'and Lua scripts act as guests. Communication between the engine and mods happens through a '
    'well-defined API surface exposed as the global <font name="DejaVuSans" size="9">mod</font> table. '
    'The engine calls into Lua via hook callbacks, and Lua calls into the engine via the functions '
    'registered on this table. This section describes the lifecycle and data flow of the mod system.'
))

story.append(h2('2.1 Lifecycle'))
story.append(p(
    'The mod system follows a strict initialization order that determines when and how mods are loaded. '
    'Understanding this lifecycle is essential for writing mods that behave correctly and avoid race '
    'conditions with other mods or the game engine itself.'
))

lifecycle_steps = [
    ['Step', 'Action', 'Description'],
    ['1', 'NativeMods_Init()', 'Creates the Lua VM (<font name="DejaVuSans" size="8">luaL_newstate</font>), opens all standard libraries, and registers the <font name="DejaVuSans" size="8">mod</font> global table with all API functions.'],
    ['2', 'NativeMods_ScanMods()', 'Scans the <font name="DejaVuSans" size="8">mods/</font> directory for subdirectories containing <font name="DejaVuSans" size="8">main.lua</font>. Registers up to 64 mods with their name, path, and enabled state.'],
    ['3', 'NativeMods_LoadModScripts()', 'Iterates through all enabled mods and executes each <font name="DejaVuSans" size="8">main.lua</font> via <font name="DejaVuSans" size="8">luaL_dofile</font>. During execution, mods register hooks using <font name="DejaVuSans" size="8">mod.hook()</font>.'],
    ['4', 'onInit hook fires', 'After all mod scripts are loaded, the engine calls the <font name="DejaVuSans" size="8">onInit</font> hook. This is where mods should perform one-time setup.'],
    ['5', 'Game loop', 'During gameplay, the engine fires <font name="DejaVuSans" size="8">onUpdate</font>, <font name="DejaVuSans" size="8">onRender</font>, and <font name="DejaVuSans" size="8">onInput</font> hooks every frame if they are registered.'],
]

t = Table(lifecycle_steps, colWidths=[35, CONTENT_W * 0.22, CONTENT_W - 35 - CONTENT_W * 0.22], repeatRows=1)
t.setStyle(TableStyle([
    ('BACKGROUND', (0, 0), (-1, 0), TABLE_HEADER_COLOR),
    ('TEXTCOLOR', (0, 0), (-1, 0), TABLE_HEADER_TEXT),
    ('GRID', (0, 0), (-1, -1), 0.5, colors.HexColor('#c0c4c8')),
    ('VALIGN', (0, 0), (-1, -1), 'TOP'),
    ('LEFTPADDING', (0, 0), (-1, -1), 6),
    ('RIGHTPADDING', (0, 0), (-1, -1), 6),
    ('TOPPADDING', (0, 0), (-1, -1), 5),
    ('BOTTOMPADDING', (0, 0), (-1, -1), 5),
    ('BACKGROUND', (0, 1), (-1, 1), TABLE_ROW_EVEN),
    ('BACKGROUND', (0, 2), (-1, 2), TABLE_ROW_ODD),
    ('BACKGROUND', (0, 3), (-1, 3), TABLE_ROW_EVEN),
    ('BACKGROUND', (0, 4), (-1, 4), TABLE_ROW_ODD),
    ('BACKGROUND', (0, 5), (-1, 5), TABLE_ROW_EVEN),
    ('FONTNAME', (0, 0), (-1, 0), 'LiberationSerif'),
    ('FONTNAME', (0, 1), (-1, -1), 'LiberationSerif'),
    ('FONTSIZE', (0, 0), (-1, 0), 9.5),
    ('FONTSIZE', (0, 1), (-1, -1), 9),
]))
story.append(Spacer(1, 8))
story.append(t)

story.append(h2('2.2 Shared Lua State'))
story.append(p(
    'All mods share a single Lua virtual machine. This means that global variables defined in one mod '
    'are visible to all other mods, and naming collisions can occur. It is strongly recommended to '
    'use local variables and avoid polluting the global namespace. Each mod should encapsulate its '
    'state within its own scope, using the <font name="DejaVuSans" size="9">local</font> keyword for '
    'all variables and functions that are not meant to be shared.'
))
story.append(p(
    'The hook registration system (<font name="DejaVuSans" size="9">mod.hook()</font>) stores only '
    'one callback per hook type. If multiple mods register the same hook, the last one to call '
    '<font name="DejaVuSans" size="9">mod.hook()</font> will overwrite the previous registration. '
    'This is a known limitation of the current system. Mods that need to coexist with other mods '
    'should check whether a hook is already registered before overwriting it, or consider using a '
    'dispatcher pattern.'
))

# ━━ 3. GETTING STARTED ━━

story.append(Spacer(1, 18))
story.append(h1('3. Getting Started'))
story.append(Spacer(1, 6))

story.append(h2('3.1 Prerequisites'))
story.append(p(
    'Before creating a mod, ensure you have the following tools and knowledge. You do not need a C '
    'compiler or any build tools for the game itself; modding is done entirely in Lua. A basic '
    'understanding of Lua syntax and programming concepts is assumed throughout this guide.'
))
story.append(bullet('A working installation of CTR Native (the PC port)'))
story.append(bullet('A text editor or IDE with Lua support (VS Code, Sublime Text, etc.)'))
story.append(bullet('Basic knowledge of Lua 5.4 syntax'))
story.append(bullet('Familiarity with file system paths on your operating system'))

story.append(h2('3.2 Creating Your First Mod'))
story.append(p(
    'Creating a mod is straightforward. Follow these steps to create a minimal mod that prints a '
    'message to the console when the game starts. This exercise will familiarize you with the '
    'directory structure and the basic mod API.'
))

story.append(h3('Step 1: Create the mod directory'))
story.append(p(
    'Navigate to the folder where CTR Native is installed. Inside the game directory, you will find '
    'a <font name="DejaVuSans" size="9">mods/</font> folder. If it does not exist, it will be '
    'created automatically the first time the game runs. Create a new subdirectory with the name of '
    'your mod. The directory name will appear as the mod name in the in-game menu.'
))
story.append(code_block(
    'mods/\n'
    '  my_first_mod/\n'
    '    main.lua'
))

story.append(h3('Step 2: Write the main.lua script'))
story.append(p(
    'Inside your mod directory, create a file named <font name="DejaVuSans" size="9">main.lua</font>. '
    'This is the entry point that the engine looks for when scanning for mods. Add the following code:'
))
story.append(code_block(
    '-- My First Mod\n'
    'mod.log("Hello from my first mod!")\n'
    '\n'
    'mod.hook("onInit", function()\n'
    '    mod.log("Mod initialized successfully!")\n'
    'end)\n'
    '\n'
    'mod.hook("onUpdate", function()\n'
    '    -- This runs every frame\n'
    'end)'
))

story.append(h3('Step 3: Run the game'))
story.append(p(
    'Launch CTR Native. The engine will scan the <font name="DejaVuSans" size="9">mods/</font> '
    'directory at startup. If <font name="DejaVuSans" size="9">main.lua</font> exists in your mod '
    'directory and the mod is enabled (which is the default for newly discovered mods), your script '
    'will be loaded. Check the console output (stdout) for the message from your mod. You can also '
    'access the "MODS" menu from the main menu to see your mod listed and toggle it on or off.'
))

# ━━ 4. MOD STRUCTURE ━━

story.append(Spacer(1, 18))
story.append(h1('4. Mod Structure'))
story.append(Spacer(1, 6))

story.append(p(
    'Every mod follows a consistent directory structure. The engine expects a specific layout to '
    'properly discover, load, and manage mods. This section describes the required and optional '
    'components of a mod directory.'
))

story.append(h2('4.1 Directory Layout'))
story.append(code_block(
    'mods/\n'
    '  my_mod_name/          <-- Mod directory (name shown in menu)\n'
    '    main.lua            <-- Required: entry point script\n'
    '    files/              <-- Optional: custom data files\n'
    '      config.txt        <-- Mod configuration file\n'
    '      data.json         <-- Mod data file\n'
    '      textures/         <-- Optional subdirectories\n'
    '        custom_icon.png'
))

story.append(h2('4.2 Required Files'))
story.append(p(
    'The only required file is <font name="DejaVuSans" size="9">main.lua</font>. This script is '
    'executed via <font name="DejaVuSans" size="9">luaL_dofile()</font> during the mod loading '
    'phase (step 3 of the lifecycle). If this file is missing, the directory will be skipped during '
    'scanning and will not appear in the mod list. The file must be valid Lua; syntax errors will '
    'cause the mod to fail to load and an error message will be printed to stderr.'
))

story.append(h2('4.3 The files/ Directory'))
story.append(p(
    'The <font name="DejaVuSans" size="9">files/</font> subdirectory is where you place any custom '
    'data files that your mod needs to read or write. When you call '
    '<font name="DejaVuSans" size="9">mod.readFile("config.txt")</font>, the engine automatically '
    'prepends the path to your mod\'s <font name="DejaVuSans" size="9">files/</font> directory, '
    'resolving the full path as <font name="DejaVuSans" size="9">&lt;mod_path&gt;/files/config.txt</font>. '
    'Similarly, <font name="DejaVuSans" size="9">mod.writeFile()</font> writes to this directory. '
    'This ensures that mods cannot accidentally overwrite game files or files belonging to other mods.'
))
story.append(p(
    'You can create any number of subdirectories within <font name="DejaVuSans" size="9">files/</font> '
    'to organize your mod\'s assets. The path parameter to '
    '<font name="DejaVuSans" size="9">mod.readFile()</font> and '
    '<font name="DejaVuSans" size="9">mod.writeFile()</font> supports subdirectory separators '
    '(forward slash on all platforms).'
))

story.append(h2('4.4 Mod Discovery Rules'))
story.append(p(
    'The engine discovers mods by iterating through the <font name="DejaVuSans" size="9">mods/</font> '
    'directory. For each entry, it checks: (1) the entry name does not start with a dot (hidden '
    'directories are ignored), (2) a file named <font name="DejaVuSans" size="9">main.lua</font> '
    'exists inside the directory, and (3) the entry is a regular file (not a directory). Directories '
    'that pass all checks are registered as mods with their directory name as the display name. Up '
    'to 64 mods can be loaded simultaneously (defined by '
    '<font name="DejaVuSans" size="9">NATIVE_MODS_MAX_MODS</font>).'
))

# ━━ 5. LUA API REFERENCE ━━

story.append(Spacer(1, 18))
story.append(h1('5. Lua API Reference'))
story.append(Spacer(1, 6))

story.append(p(
    'The mod API is exposed through the global <font name="DejaVuSans" size="9">mod</font> table, '
    'which is registered in the Lua environment before any mod scripts are loaded. All functions are '
    'called using the standard Lua method syntax: <font name="DejaVuSans" size="9">mod.functionName(args)</font>. '
    'This section provides a complete reference for every function in the API.'
))

# 5.1 mod.log
story.append(h2('5.1 mod.log(message)'))
story.append(p(
    'Prints a message to the standard output (stdout) of the game process. This is the primary '
    'debugging tool for mod developers. Messages are prefixed with <font name="DejaVuSans" size="9">[Mod]</font> '
    'in the console output to distinguish them from engine messages. The function accepts a single '
    'string argument. If the argument is nil, it will be printed as <font name="DejaVuSans" size="9">(nil)</font>. '
    'This function is useful for verifying that your mod is loaded, tracking variable values during '
    'development, and diagnosing issues in hook callbacks.'
))
story.append(Spacer(1, 6))
api_log = [
    ['Parameter', 'Type', 'Description'],
    ['message', 'string', 'The text message to print to the console.'],
]
story.append(make_table(api_log[0], api_log[1:], [CONTENT_W*0.18, CONTENT_W*0.15, CONTENT_W*0.67]))
story.append(Spacer(1, 6))
story.append(Paragraph('<b>Returns:</b> nil', style_body))
story.append(Spacer(1, 4))
story.append(Paragraph('<b>Example:</b>', style_body))
story.append(code_block('mod.log("Speed Display mod loaded successfully!")\nmod.log("Current frame: " .. tostring(frameCount))'))

# 5.2 mod.getModPath
story.append(h2('5.2 mod.getModPath()'))
story.append(p(
    'Returns the absolute filesystem path to the directory containing the currently executing mod. '
    'This path is set by the engine before each mod\'s <font name="DejaVuSans" size="9">main.lua</font> '
    'is loaded, and it is cleared after all mods have been loaded. You should call this function '
    'during the top-level execution of your script (not inside a hook callback) and store the result '
    'in a local variable for later use. The returned path includes the mod directory name and uses '
    'the platform\'s native path separator.'
))
story.append(Spacer(1, 6))
api_getpath = [
    ['Parameter', 'Type', 'Description'],
    ['(none)', '', 'This function takes no parameters.'],
]
story.append(make_table(api_getpath[0], api_getpath[1:], [CONTENT_W*0.18, CONTENT_W*0.15, CONTENT_W*0.67]))
story.append(Spacer(1, 6))
story.append(Paragraph('<b>Returns:</b> string - The absolute path to the mod directory.', style_body))
story.append(Spacer(1, 4))
story.append(Paragraph('<b>Example:</b>', style_body))
story.append(code_block('local myPath = mod.getModPath()\nmod.log("My mod is at: " .. myPath)\n-- Output: My mod is at: /games/ctr/mods/speed_display'))

# 5.3 mod.readFile
story.append(h2('5.3 mod.readFile(path)'))
story.append(p(
    'Reads a file from the mod\'s <font name="DejaVuSans" size="9">files/</font> subdirectory and '
    'returns its contents as a Lua string. The path parameter is relative to the '
    '<font name="DejaVuSans" size="9">files/</font> directory; do not include the '
    '<font name="DejaVuSans" size="9">files/</font> prefix in the path. The file is read in binary '
    'mode, so the returned string may contain null bytes and is safe for binary data. If the file '
    'does not exist or cannot be opened, the function returns nil instead of raising an error. This '
    'design allows you to test for file existence by checking the return value.'
))
story.append(Spacer(1, 6))
api_readfile = [
    ['Parameter', 'Type', 'Description'],
    ['path', 'string', 'Relative path within the mod\'s files/ directory.'],
]
story.append(make_table(api_readfile[0], api_readfile[1:], [CONTENT_W*0.18, CONTENT_W*0.15, CONTENT_W*0.67]))
story.append(Spacer(1, 6))
story.append(Paragraph('<b>Returns:</b> string | nil - File contents, or nil if the file was not found.', style_body))
story.append(Spacer(1, 4))
story.append(Paragraph('<b>Example:</b>', style_body))
story.append(code_block(
    'local config = mod.readFile("config.txt")\n'
    'if config then\n'
    '    mod.log("Config: " .. config)\n'
    'else\n'
    '    mod.log("No config file found, using defaults")\n'
    'end'
))

# 5.4 mod.writeFile
story.append(h2('5.4 mod.writeFile(path, data)'))
story.append(p(
    'Writes data to a file within the mod\'s <font name="DejaVuSans" size="9">files/</font> '
    'subdirectory. If the file already exists, it will be overwritten. If the file does not exist, '
    'it will be created. The path parameter follows the same convention as '
    '<font name="DejaVuSans" size="9">mod.readFile()</font>: it is relative to the '
    '<font name="DejaVuSans" size="9">files/</font> directory. The data parameter can be any Lua '
    'string, including binary data. Note that this function does not create intermediate directories; '
    'if you specify a path with subdirectories that do not exist, the write will fail and return false.'
))
story.append(Spacer(1, 6))
api_writefile = [
    ['Parameter', 'Type', 'Description'],
    ['path', 'string', 'Relative path within the mod\'s files/ directory.'],
    ['data', 'string', 'The content to write to the file.'],
]
story.append(make_table(api_writefile[0], api_writefile[1:], [CONTENT_W*0.18, CONTENT_W*0.15, CONTENT_W*0.67]))
story.append(Spacer(1, 6))
story.append(Paragraph('<b>Returns:</b> boolean - true if the write succeeded, false if it failed.', style_body))
story.append(Spacer(1, 4))
story.append(Paragraph('<b>Example:</b>', style_body))
story.append(code_block(
    '-- Append to a log file\n'
    'local existing = mod.readFile("log.txt") or ""\n'
    'local newEntry = existing .. "Frame " .. frameCount .. "\\n"\n'
    'local success = mod.writeFile("log.txt", newEntry)\n'
    'if not success then\n'
    '    mod.log("Failed to write log file!")\n'
    'end'
))

# 5.5 mod.hook
story.append(h2('5.5 mod.hook(name, callback)'))
story.append(p(
    'Registers a callback function for a specific game event hook. When the engine reaches the '
    'corresponding point in the game loop, it will call your callback function. Each hook type '
    'supports only one callback at a time; if you call <font name="DejaVuSans" size="9">mod.hook()</font> '
    'multiple times with the same hook name, only the last registered callback will be active. '
    'The callback function receives no arguments. Hooks should be registered during the top-level '
    'execution of your <font name="DejaVuSans" size="9">main.lua</font> script, not inside other '
    'hooks or delayed functions.'
))
story.append(Spacer(1, 6))
api_hook = [
    ['Parameter', 'Type', 'Description'],
    ['name', 'string', 'The hook name. Must be one of: "onInit", "onUpdate", "onRender", "onInput", "onTitleInit".'],
    ['callback', 'function', 'The Lua function to call when the hook fires.'],
]
story.append(make_table(api_hook[0], api_hook[1:], [CONTENT_W*0.15, CONTENT_W*0.15, CONTENT_W*0.70]))
story.append(Spacer(1, 6))
story.append(Paragraph('<b>Returns:</b> nil', style_body))
story.append(Spacer(1, 4))
story.append(note_box(
    '<b>Important:</b> If an invalid hook name is provided, the function raises a Lua error with the message '
    '"unknown hook name (onInit, onUpdate, onRender, onInput, onTitleInit)". If the second argument '
    'is not a function, it raises "expected function".'
))
story.append(Spacer(1, 4))
story.append(Paragraph('<b>Example:</b>', style_body))
story.append(code_block(
    'mod.hook("onUpdate", function()\n'
    '    -- This runs every frame during gameplay\n'
    '    frameCount = frameCount + 1\n'
    'end)\n'
    '\n'
    'mod.hook("onTitleInit", function()\n'
    '    -- This runs when the title screen loads\n'
    '    mod.log("Title screen loaded!")\n'
    'end)'
))

# ━━ 6. HOOKS REFERENCE ━━

story.append(Spacer(1, 18))
story.append(h1('6. Hooks Reference'))
story.append(Spacer(1, 6))

story.append(p(
    'Hooks are the core mechanism by which mods interact with the game. Each hook represents a '
    'specific point in the game\'s execution where mod code can run. Understanding when each hook '
    'fires and what operations are safe to perform within each hook is critical for writing stable '
    'and performant mods.'
))

hooks_data = [
    ['Hook Name', 'When It Fires', 'Frequency', 'Common Uses'],
    ['onInit', 'After all mod scripts are loaded, before the game loop starts', 'Once', 'Initialization, reading config files, setting up state variables'],
    ['onUpdate', 'Each frame during the game logic update phase', 'Every frame (~30fps)', 'Game logic, state machines, periodic calculations, timer-based actions'],
    ['onRender', 'Each frame during the rendering phase', 'Every frame (~30fps)', 'Drawing overlays, visual effects (future: font drawing API)'],
    ['onInput', 'When gamepad input is processed', 'Every frame (~30fps)', 'Custom keybinds, input interception, button remapping'],
    ['onTitleInit', 'When the title/main menu screen initializes', 'Once per menu visit', 'Resetting mod state, refreshing UI elements, re-reading configuration'],
]

t_hooks = Table(hooks_data, colWidths=[CONTENT_W*0.14, CONTENT_W*0.26, CONTENT_W*0.14, CONTENT_W*0.46], repeatRows=1)
t_hooks.setStyle(TableStyle([
    ('BACKGROUND', (0, 0), (-1, 0), TABLE_HEADER_COLOR),
    ('TEXTCOLOR', (0, 0), (-1, 0), TABLE_HEADER_TEXT),
    ('GRID', (0, 0), (-1, -1), 0.5, colors.HexColor('#c0c4c8')),
    ('VALIGN', (0, 0), (-1, -1), 'TOP'),
    ('LEFTPADDING', (0, 0), (-1, -1), 6),
    ('RIGHTPADDING', (0, 0), (-1, -1), 6),
    ('TOPPADDING', (0, 0), (-1, -1), 5),
    ('BOTTOMPADDING', (0, 0), (-1, -1), 5),
    ('BACKGROUND', (0, 1), (-1, 1), TABLE_ROW_EVEN),
    ('BACKGROUND', (0, 2), (-1, 2), TABLE_ROW_ODD),
    ('BACKGROUND', (0, 3), (-1, 3), TABLE_ROW_EVEN),
    ('BACKGROUND', (0, 4), (-1, 4), TABLE_ROW_ODD),
    ('BACKGROUND', (0, 5), (-1, 5), TABLE_ROW_EVEN),
    ('FONTNAME', (0, 0), (-1, 0), 'LiberationSerif'),
    ('FONTSIZE', (0, 0), (-1, 0), 9.5),
    ('FONTNAME', (0, 1), (-1, -1), 'LiberationSerif'),
    ('FONTSIZE', (0, 1), (-1, -1), 9),
]))
story.append(Spacer(1, 8))
story.append(t_hooks)

story.append(Spacer(1, 10))
story.append(h2('6.1 Hook Execution Order'))
story.append(p(
    'Within a single frame, hooks fire in a deterministic order. The update phase happens first, '
    'followed by input processing, and finally rendering. The <font name="DejaVuSans" size="9">onInit</font> '
    'hook fires only once, after all mod scripts have been loaded but before the first frame. The '
    '<font name="DejaVuSans" size="9">onTitleInit</font> hook fires when the player enters the '
    'main menu screen from another part of the game (returning from a race, for example).'
))
story.append(code_block(
    'Frame execution order:\n'
    '  1. onUpdate    -- Game logic\n'
    '  2. onInput     -- Input processing\n'
    '  3. onRender    -- Drawing\n'
    '\n'
    'Startup order:\n'
    '  1. NativeMods_Init()\n'
    '  2. NativeMods_ScanMods()\n'
    '  3. NativeMods_LoadModScripts()  -- main.lua executes here\n'
    '  4. onInit hook fires'
))

# ━━ 7. MENU INTEGRATION ━━

story.append(Spacer(1, 18))
story.append(h1('7. Menu Integration'))
story.append(Spacer(1, 6))

story.append(p(
    'The mod system is integrated into the game\'s main menu through the "MODS" option, which appears '
    'as the last item in the main menu list. This integration is handled entirely by the engine and '
    'does not require any configuration from mod developers. Understanding how it works can help you '
    'debug issues with mod visibility and toggling behavior.'
))

story.append(h2('7.1 The MODS Menu Entry'))
story.append(p(
    'When the game is compiled with the <font name="DejaVuSans" size="9">CTR_NATIVE</font> flag, '
    'the main menu includes an additional row with string index <font name="DejaVuSans" size="9">0x014</font>. '
    'The engine overrides this language string with "MODS" via '
    '<font name="DejaVuSans" size="9">NativeMods_OnLanguageLoaded()</font>. When the player selects '
    'this option, the game navigates to the mods menu screen (case 6 in the title menu update handler), '
    'which initializes the mod menu and sets it as the active menu.'
))

story.append(h2('7.2 Toggling Mods'))
story.append(p(
    'The in-game mod menu displays up to 8 mods at a time, each showing its name and enabled state '
    '(<font name="DejaVuSans" size="9">[ON]</font> or <font name="DejaVuSans" size="9">[OFF]</font>). '
    'Players can navigate with UP/DOWN on the D-pad, toggle a mod with the CROSS button, and return '
    'to the main menu with TRIANGLE or SQUARE. The toggle operation calls '
    '<font name="DejaVuSans" size="9">NativeMods_ToggleMod()</font>, which simply flips the enabled '
    'flag for that mod. However, the actual loading or unloading of Lua scripts only happens at '
    'startup. Toggling a mod off means it will not be loaded on the next game launch.'
))

story.append(h2('7.3 Language String Override'))
story.append(p(
    'The engine uses language string index <font name="DejaVuSans" size="9">0x014</font> '
    '(<font name="DejaVuSans" size="9">LNG_UNUSED_014</font>) for the "MODS" text in both the main '
    'menu row and the mod menu title. This string is normally empty in the retail game. '
    '<font name="DejaVuSans" size="9">NativeMods_OnLanguageLoaded()</font> replaces it with the '
    'static string "MODS" every time the language file is loaded. This means the mod menu text will '
    'always appear in English regardless of the game\'s language setting.'
))

# ━━ 8. FILE OVERRIDING SYSTEM ━━

story.append(Spacer(1, 18))
story.append(h1('8. File Overriding System'))
story.append(Spacer(1, 6))

story.append(p(
    'Beyond the Lua scripting API, the mod system includes a file overriding mechanism that allows '
    'mods to replace game assets. When the engine opens a file using '
    '<font name="DejaVuSans" size="9">NativeMods_OpenFile()</font>, it first checks if any enabled '
    'mod provides a replacement file in its <font name="DejaVuSans" size="9">files/</font> directory. '
    'If a match is found, the mod\'s version of the file is opened instead of the original. If no '
    'mod provides the file, the engine falls back to the standard BIGFILE asset directory.'
))

story.append(h2('8.1 Override Resolution Order'))
story.append(p(
    'File overrides are resolved in the order that mods are loaded. If multiple enabled mods provide '
    'the same file, the first mod in the load order takes priority. This means that mods loaded earlier '
    '(based on directory scanning order) have higher priority for file overrides. The load order is '
    'determined by the filesystem iteration order, which may vary across operating systems.'
))
story.append(code_block(
    'File resolution algorithm:\n'
    '  1. For each enabled mod (in load order):\n'
    '     Check: <mod_path>/files/<relative_path>\n'
    '     If exists -> return this file\n'
    '  2. Fall back to: BIGFILE/<relative_path>\n'
    '  3. If not found -> return NULL'
))

story.append(h2('8.2 Use Cases'))
story.append(p(
    'The file overriding system enables several powerful use cases without requiring any Lua code. '
    'By simply placing files in the correct directory structure, you can replace textures, models, '
    'audio clips, and other game assets. However, care must be taken to match the exact file format '
    'and structure expected by the engine. Corrupted or incorrectly formatted files may cause the '
    'game to crash or behave unexpectedly.'
))
story.append(bullet('Replacing texture images with higher-resolution versions'))
story.append(bullet('Swapping character models or vehicle skins'))
story.append(bullet('Replacing audio files with custom sound effects or music'))
story.append(bullet('Overriding level data for custom track layouts'))
story.append(bullet('Providing localization files for new languages'))

# ━━ 9. EXAMPLE MOD: SPEED DISPLAY ━━

story.append(Spacer(1, 18))
story.append(h1('9. Example Mod: Speed Display'))
story.append(Spacer(1, 6))

story.append(p(
    'This section presents a complete, working example mod that demonstrates all major features of '
    'the modding API. The "Speed Display" mod shows the player\'s current speed, logs speed data to '
    'a file, and reads a configuration file at startup. You can use this mod as a template for your '
    'own projects.'
))

story.append(h2('9.1 Directory Structure'))
story.append(code_block(
    'mods/\n'
    '  speed_display/\n'
    '    main.lua           <-- Main script\n'
    '    files/\n'
    '      config.txt       <-- Configuration file\n'
    '      speed_log.txt    <-- Generated at runtime'
))

story.append(h2('9.2 config.txt'))
story.append(code_block(
    'update_interval=10\n'
    'log_interval=300\n'
    'speed_unit=kmh'
))

story.append(h2('9.3 main.lua'))
story.append(code_block(
    '-- ============================================================\n'
    '-- Speed Display Mod for CTR Native\n'
    '-- Shows current speed on screen and logs it to console\n'
    '-- ============================================================\n'
    '\n'
    'local myPath = mod.getModPath()\n'
    '\n'
    '-- State variables\n'
    'local frameCount = 0\n'
    'local displaySpeed = 0\n'
    'local speedDataFile = "speed_log.txt"\n'
    'local hasLoggedHeader = false\n'
    '\n'
    '-- Called once when all mods are initialized\n'
    'mod.hook("onInit", function()\n'
    '    mod.log("Speed Display mod initialized!")\n'
    '    mod.log("Mod path: " .. myPath)\n'
    '\n'
    '    -- Read configuration\n'
    '    local configData = mod.readFile("config.txt")\n'
    '    if configData then\n'
    '        mod.log("Config loaded: " .. tostring(configData))\n'
    '    else\n'
    '        mod.log("No config.txt found, using defaults")\n'
    '    end\n'
    '\n'
    '    -- Write log header\n'
    '    mod.writeFile(speedDataFile, "=== Speed Log ===\\n")\n'
    '    hasLoggedHeader = true\n'
    'end)\n'
    '\n'
    '-- Called every frame during gameplay\n'
    'mod.hook("onUpdate", function()\n'
    '    frameCount = frameCount + 1\n'
    '\n'
    '    if frameCount % 10 == 0 then\n'
    '        displaySpeed = math.random(50, 280)\n'
    '\n'
    '        if frameCount % 300 == 0 then\n'
    '            local logLine = string.format(\n'
    '                "Frame %d: Speed = %d km/h\\n",\n'
    '                frameCount, displaySpeed)\n'
    '            mod.log(string.format(\n'
    '                "Speed: %d km/h", displaySpeed))\n'
    '\n'
    '            local existing = mod.readFile(speedDataFile)\n'
    '                or ""\n'
    '            mod.writeFile(speedDataFile,\n'
    '                existing .. logLine)\n'
    '        end\n'
    '    end\n'
    'end)\n'
    '\n'
    '-- Called when the title screen initializes\n'
    'mod.hook("onTitleInit", function()\n'
    '    mod.log("Title screen - resetting speed")\n'
    '    frameCount = 0\n'
    '    displaySpeed = 0\n'
    'end)'
))

story.append(h2('9.4 How It Works'))
story.append(p(
    'When the game starts, the engine loads the mod script. At the top level, '
    '<font name="DejaVuSans" size="9">mod.getModPath()</font> is called and the result is stored '
    'in a local variable. Then, two hooks are registered: <font name="DejaVuSans" size="9">onInit</font> '
    'for one-time setup and <font name="DejaVuSans" size="9">onUpdate</font> for per-frame logic. '
    'The <font name="DejaVuSans" size="9">onInit</font> hook reads the configuration file and writes '
    'a header to the log file. The <font name="DejaVuSans" size="9">onUpdate</font> hook increments '
    'a frame counter and performs periodic speed calculations and logging. The '
    '<font name="DejaVuSans" size="9">onTitleInit</font> hook resets the state when the player '
    'returns to the main menu.'
))

# ━━ 10. BEST PRACTICES ━━

story.append(Spacer(1, 18))
story.append(h1('10. Best Practices'))
story.append(Spacer(1, 6))

story.append(h2('10.1 Use Local Variables'))
story.append(p(
    'Since all mods share a single Lua state, global variables can collide between mods. Always use '
    'the <font name="DejaVuSans" size="9">local</font> keyword for variables and functions. The only '
    'exception is if you intentionally want to expose functionality to other mods, in which case you '
    'should use a uniquely named global table (e.g., <font name="DejaVuSans" size="9">MyMod_Stats</font>) '
    'to minimize collision risk.'
))
story.append(code_block(
    '-- BAD: global variable, may collide with other mods\n'
    'counter = 0\n'
    '\n'
    '-- GOOD: local variable, scoped to this mod only\n'
    'local counter = 0'
))

story.append(h2('10.2 Store mod.getModPath() Early'))
story.append(p(
    'The <font name="DejaVuSans" size="9">mod.getModPath()</font> function returns the path of the '
    'currently loading mod. This value is only available during the top-level execution of '
    '<font name="DejaVuSans" size="9">main.lua</font>. It is cleared after all mods are loaded. '
    'Always store the result in a local variable at the top of your script, never inside a hook callback.'
))
story.append(code_block(
    '-- Store at top level\n'
    'local myPath = mod.getModPath()\n'
    '\n'
    'mod.hook("onInit", function()\n'
    '    -- SAFE: using stored value\n'
    '    mod.log("My path: " .. myPath)\n'
    'end)'
))

story.append(h2('10.3 Check Return Values'))
story.append(p(
    'Both <font name="DejaVuSans" size="9">mod.readFile()</font> and '
    '<font name="DejaVuSans" size="9">mod.writeFile()</font> can fail. Always check the return value '
    'before using the result. <font name="DejaVuSans" size="9">mod.readFile()</font> returns nil on '
    'failure, and <font name="DejaVuSans" size="9">mod.writeFile()</font> returns false. Handling '
    'these cases gracefully makes your mod more robust and easier to debug.'
))

story.append(h2('10.4 Avoid Expensive Operations in Hooks'))
story.append(p(
    'The <font name="DejaVuSans" size="9">onUpdate</font> and <font name="DejaVuSans" size="9">onRender</font> '
    'hooks fire every frame (~30 times per second). Avoid performing expensive operations like file I/O '
    'or complex calculations on every frame. Instead, use frame counters to throttle expensive operations '
    'to a reasonable frequency. For example, writing to a log file every 300 frames (~10 seconds) '
    'instead of every frame.'
))
story.append(code_block(
    'mod.hook("onUpdate", function()\n'
    '    frameCount = frameCount + 1\n'
    '\n'
    '    -- Only do expensive work every 60 frames\n'
    '    if frameCount % 60 == 0 then\n'
    '        local data = mod.readFile("status.json")\n'
    '        -- process data...\n'
    '    end\n'
    'end)'
))

story.append(h2('10.5 Be Aware of the Single-Hook Limitation'))
story.append(p(
    'The current system stores only one callback per hook type. If two mods both register '
    '<font name="DejaVuSans" size="9">onUpdate</font>, the second registration overwrites the first. '
    'Until a multi-callback dispatcher is implemented, coordinate with other mod developers to avoid '
    'conflicts, or implement your own dispatcher pattern that chains multiple callbacks.'
))

story.append(h2('10.6 Handle Errors Gracefully'))
story.append(p(
    'If a Lua error occurs inside a hook callback, the engine catches it, prints an error message to '
    'stderr, and continues execution. The hook will not be called again until it is re-registered. '
    'To prevent losing hook functionality, wrap risky code in '
    '<font name="DejaVuSans" size="9">pcall()</font> and handle errors explicitly.'
))
story.append(code_block(
    'mod.hook("onUpdate", function()\n'
    '    local ok, err = pcall(function()\n'
    '        -- Risky operation that might fail\n'
    '        local data = mod.readFile("dynamic.txt")\n'
    '        process(data)\n'
    '    end)\n'
    '    if not ok then\n'
    '        mod.log("Error: " .. tostring(err))\n'
    '    end\n'
    'end)'
))

# ━━ 11. TROUBLESHOOTING ━━

story.append(Spacer(1, 18))
story.append(h1('11. Troubleshooting'))
story.append(Spacer(1, 6))

story.append(h2('11.1 My mod does not appear in the menu'))
story.append(p(
    'This is the most common issue for new mod developers. There are several possible causes. First, '
    'verify that your mod directory is placed inside the correct <font name="DejaVuSans" size="9">mods/</font> '
    'folder relative to the game executable. Second, ensure that the directory contains a file named '
    'exactly <font name="DejaVuSans" size="9">main.lua</font> (case-sensitive on Linux). Third, check '
    'that the directory name does not start with a dot (.), as hidden directories are ignored by the '
    'scanner. Fourth, confirm that the <font name="DejaVuSans" size="9">mods/</font> directory has '
    'not exceeded the 64-mod limit. Finally, check the console output for any scanning errors.'
))

story.append(h2('11.2 My mod loads but the hook does not fire'))
story.append(p(
    'If your mod appears in the menu but hook callbacks are not executing, the most likely cause is '
    'a Lua error during script execution. Check the stderr output for error messages. A syntax error '
    'in <font name="DejaVuSans" size="9">main.lua</font> will prevent the script from loading entirely, '
    'meaning that <font name="DejaVuSans" size="9">mod.hook()</font> is never called. Even if the '
    'script loads successfully, a runtime error inside the hook registration code can prevent hooks '
    'from being registered.'
))

story.append(h2('11.3 mod.readFile returns nil'))
story.append(p(
    'If <font name="DejaVuSans" size="9">mod.readFile()</font> consistently returns nil, check the '
    'following: the file must be located in the <font name="DejaVuSans" size="9">files/</font> '
    'subdirectory of your mod (not in the mod root directory). The path parameter should not include '
    'the <font name="DejaVuSans" size="9">files/</font> prefix. Ensure the file name matches exactly, '
    'including case (the filesystem may be case-sensitive). Check file permissions to ensure the game '
    'process has read access to the file.'
))

story.append(h2('11.4 Common Error Messages'))
story.append(Spacer(1, 6))

error_data = [
    ['Error Message', 'Cause', 'Solution'],
    ['[Mods] Lua error in <mod_name>: ...', 'Runtime Lua error in the mod script', 'Check the error details in stderr, fix the Lua code'],
    ['[Mods] Lua panic: ...', 'Unrecoverable Lua VM error', 'Check for severe script errors like stack overflow'],
    ['unknown hook name', 'Invalid hook name passed to mod.hook()', 'Use one of: onInit, onUpdate, onRender, onInput, onTitleInit'],
    ['expected function', 'Non-function value passed as callback to mod.hook()', 'Ensure the second argument to mod.hook() is a Lua function'],
]

t_err = Table(error_data, colWidths=[CONTENT_W*0.30, CONTENT_W*0.30, CONTENT_W*0.40], repeatRows=1)
t_err.setStyle(TableStyle([
    ('BACKGROUND', (0, 0), (-1, 0), TABLE_HEADER_COLOR),
    ('TEXTCOLOR', (0, 0), (-1, 0), TABLE_HEADER_TEXT),
    ('GRID', (0, 0), (-1, -1), 0.5, colors.HexColor('#c0c4c8')),
    ('VALIGN', (0, 0), (-1, -1), 'TOP'),
    ('LEFTPADDING', (0, 0), (-1, -1), 6),
    ('RIGHTPADDING', (0, 0), (-1, -1), 6),
    ('TOPPADDING', (0, 0), (-1, -1), 5),
    ('BOTTOMPADDING', (0, 0), (-1, -1), 5),
    ('BACKGROUND', (0, 1), (-1, 1), TABLE_ROW_EVEN),
    ('BACKGROUND', (0, 2), (-1, 2), TABLE_ROW_ODD),
    ('BACKGROUND', (0, 3), (-1, 3), TABLE_ROW_EVEN),
    ('BACKGROUND', (0, 4), (-1, 4), TABLE_ROW_ODD),
    ('BACKGROUND', (0, 5), (-1, 5), TABLE_ROW_EVEN),
    ('FONTNAME', (0, 0), (-1, 0), 'LiberationSerif'),
    ('FONTSIZE', (0, 0), (-1, 0), 9.5),
    ('FONTNAME', (0, 1), (-1, -1), 'LiberationSerif'),
    ('FONTSIZE', (0, 1), (-1, -1), 9),
]))
story.append(t_err)

# ━━ Build the PDF ━━

doc.build(story, onFirstPage=on_first_page, onLaterPages=on_later_pages)

print(f"PDF generated: {OUTPUT_PATH}")
