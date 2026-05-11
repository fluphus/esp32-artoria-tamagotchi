#!/usr/bin/env python3
"""
Asset Build Script - 将 PNG 图片转换为 PROGMEM C++ 源文件
生成可直接编译链接的 .h/.cpp 文件，包含像素数据和查找表

使用方法:
    python tools/convert_assets.py

输出:
    src/display/generated_assets/generated_asset_data.cpp   (PROGMEM 像素数组)
    src/display/generated_assets/generated_asset_index.h    (查找表 + 描述符)

文件命名规则:
    assets/idle/{form_name}_{condition}_f{N}.png
    assets/eating/{form_name}_generic_f{N}.png
    assets/eating/{form_name}_{food_name}_f{N}.png  (专属覆盖)
    assets/reaction/{form_name}_{reaction}_f{N}.png
    assets/food_icons/{food_name}_f{N}.png
    assets/combo/{special_food_name}_f{N}.png
    assets/poke/{form_name}_f{N}.png
    assets/special/mapo_tofu_{form_name}_f{N}.png
"""

import os
import sys
import re
from pathlib import Path
from collections import defaultdict

try:
    from PIL import Image
    HAS_PIL = True
except ImportError:
    HAS_PIL = False

# ============================================================================
#  Configuration
# ============================================================================

PROJECT_ROOT = Path(__file__).parent.parent
ASSETS_DIR = PROJECT_ROOT / "assets"
OUTPUT_DIR = PROJECT_ROOT / "src" / "display" / "generated_assets"

# Frame delay default (ms)
DEFAULT_FRAME_DELAY = 400

# Form name → C enum mapping
FORM_ENUM = {
    "lily": "FORM_LILY",
    "white_saber": "FORM_WHITE_SABER",
    "black_saber": "FORM_BLACK_SABER",
    "white_lancer": "FORM_WHITE_LANCER",
    "black_lancer": "FORM_BLACK_LANCER",
    "white_archer": "FORM_WHITE_ARCHER",
    "black_rider": "FORM_BLACK_RIDER",
    "white_ruler": "FORM_WHITE_RULER",
    "rhongomyniad": "FORM_WHITE_LANCER_RHONGOMYNIAD",
    "black_rhongomyniad": "FORM_BLACK_LANCER_RHONGOMYNIAD",
    "nobu": "FORM_NOBU",
    "oda_nobunaga": "FORM_ODA_NOBUNAGA",
}

IDLE_CONDITION_MAP = {
    "normal": "NORMAL",
    "sr_high": "SR_HIGH",
    "hp_low": "HP_LOW",
    "hp_critical": "HP_CRITICAL",
    "lion_king_sr_max": "LION_KING_SR_MAX",
    "lion_king_sr_critical": "LION_KING_SR_CRITICAL",
}

REACTION_MAP = {
    "like": "REACTION_LIKE",
    "dislike": "REACTION_DISLIKE",
    "umu": "REACTION_UMU",
    "eww": "REACTION_EWW",
    "satisfy": "REACTION_SATISFY",
    "abhor": "REACTION_ABHOR",
    "perfect": "REACTION_PERFECT",
}

FOOD_MAP = {
    "rice_ball": 0, "salad": 1, "grilled_fish": 2, "bread": 3,
    "cake": 4, "fried_chicken": 5, "soda": 6, "candy": 7,
}

SFOOD_MAP = {
    "golden_apple": 0, "holy_grail_mug": 1, "emiya_cooking": 2, "jaguar_snack": 3,
}

# ============================================================================
#  Image conversion
# ============================================================================

def rgb888_to_rgb565(r, g, b):
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def image_to_rgb565_array(image):
    """Convert PIL Image to RGB565 uint16 array."""
    if image.mode == "RGBA":
        # Composite onto black background
        bg = Image.new("RGB", image.size, (0, 0, 0))
        bg.paste(image, mask=image.split()[3])
        image = bg
    elif image.mode != "RGB":
        image = image.convert("RGB")

    pixels = list(image.getdata())
    result = []
    for r, g, b in pixels:
        result.append(rgb888_to_rgb565(r, g, b))
    return result, image.size[0], image.size[1]


# ============================================================================
#  File scanning and grouping
# ============================================================================

def parse_frame_number(filename):
    """Extract frame number from filename like 'xxx_f0.png' → 0"""
    m = re.search(r'_f(\d+)\.png$', filename)
    if m:
        return int(m.group(1))
    return 0


def get_sequence_base(filename):
    """Get base name without frame number: 'xxx_f0.png' → 'xxx'"""
    return re.sub(r'_f\d+\.png$', '', filename)


def group_into_sequences(png_files):
    """Group PNG files into animation sequences by base name."""
    groups = defaultdict(list)
    for f in png_files:
        base = get_sequence_base(f.name)
        frame_num = parse_frame_number(f.name)
        groups[base].append((frame_num, f))

    # Sort each group by frame number
    for base in groups:
        groups[base].sort(key=lambda x: x[0])

    return groups


# ============================================================================
#  C++ code generation
# ============================================================================

def sanitize_name(name):
    """Convert to valid C identifier."""
    return re.sub(r'[^a-zA-Z0-9_]', '_', name)


def generate_progmem_array(var_name, pixels):
    """Generate a PROGMEM uint16_t array."""
    lines = []
    lines.append(f"static const uint16_t {var_name}[] PROGMEM = {{")
    # Write 8 values per line
    for i in range(0, len(pixels), 8):
        chunk = pixels[i:i+8]
        hex_vals = ", ".join(f"0x{v:04X}" for v in chunk)
        lines.append(f"    {hex_vals},")
    lines.append("};")
    return "\n".join(lines)


def generate_cpp_and_header(sequences_by_category):
    """Generate the .cpp data file and .h index file."""

    cpp_lines = [
        "// AUTO-GENERATED by tools/convert_assets.py - DO NOT EDIT",
        "// Contains PROGMEM pixel data for all animation frames",
        "",
        '#include "generated_asset_index.h"',
        '#include <Arduino.h>',
        "",
    ]

    h_lines = [
        "// AUTO-GENERATED by tools/convert_assets.py - DO NOT EDIT",
        "#ifndef GENERATED_ASSET_INDEX_H",
        "#define GENERATED_ASSET_INDEX_H",
        "",
        "#include <stdint.h>",
        "#include <Arduino.h>",
        '#include "../asset_loader.h"',
        "",
    ]

    # Track all frame descriptors and sequence descriptors
    frame_decls = []  # (var_name, width, height)
    seq_decls = []    # (seq_var_name, category, key, frame_count, frame_delay)

    # Lookup tables to build - store as list of (key1, key2, seq_var_name)
    idle_lookup = []       # (form_idx, condition_idx, seq_var_name)
    eating_lookup = []     # (form_idx, food_idx_or_255, seq_var_name)
    reaction_lookup = []   # (form_idx, reaction_idx, seq_var_name)
    poke_lookup = []       # (form_idx, 0, seq_var_name)
    combo_lookup = []      # (sfood_idx, 0, seq_var_name)
    mapo_lookup = []       # (form_idx, 0, seq_var_name)
    food_icon_lookup = []  # (food_idx, 0, seq_var_name)
    random_idle_lookup = [] # (form_idx, anim_idx, seq_var_name)

    total_bytes = 0

    for category, sequences in sequences_by_category.items():
        cpp_lines.append(f"// === {category.upper()} ===")
        cpp_lines.append("")

        for base_name, frames_data in sequences.items():
            safe_name = sanitize_name(f"{category}_{base_name}")

            # Generate pixel arrays for each frame
            frame_var_names = []
            width = 0
            height = 0
            for frame_idx, (pixels, w, h) in enumerate(frames_data):
                var_name = f"pixels_{safe_name}_f{frame_idx}"
                cpp_lines.append(generate_progmem_array(var_name, pixels))
                cpp_lines.append("")
                frame_var_names.append(var_name)
                width = w
                height = h
                total_bytes += len(pixels) * 2

            # Generate frame descriptor array
            frames_arr_name = f"frames_{safe_name}"
            cpp_lines.append(f"static const AnimFrameDescriptor {frames_arr_name}[] PROGMEM = {{")
            for fvar in frame_var_names:
                cpp_lines.append(f"    {{ (const uint8_t*){fvar}, {width}, {height}, ASSET_FMT_RGB565, 0, nullptr }},")
            cpp_lines.append("};")
            cpp_lines.append("")

            # Generate sequence descriptor
            seq_var_name = f"seq_{safe_name}"
            frame_count = len(frame_var_names)
            cpp_lines.append(f"const AnimSequenceDescriptor {seq_var_name} PROGMEM = {{")
            cpp_lines.append(f"    {frames_arr_name}, {frame_count}, {DEFAULT_FRAME_DELAY}, true")
            cpp_lines.append("};")
            cpp_lines.append("")

            # Declare in header
            h_lines.append(f"extern const AnimSequenceDescriptor {seq_var_name};")

            # Register in lookup tables
            _register_lookup(category, base_name, seq_var_name,
                           idle_lookup, eating_lookup, reaction_lookup,
                           poke_lookup, combo_lookup, mapo_lookup, food_icon_lookup,
                           random_idle_lookup)

    # Generate lookup table functions in header
    h_lines.append("")
    h_lines.append("// === Lookup table sizes ===")
    h_lines.append(f"#define GENERATED_IDLE_COUNT {len(idle_lookup)}")
    h_lines.append(f"#define GENERATED_EATING_COUNT {len(eating_lookup)}")
    h_lines.append(f"#define GENERATED_REACTION_COUNT {len(reaction_lookup)}")
    h_lines.append(f"#define GENERATED_POKE_COUNT {len(poke_lookup)}")
    h_lines.append(f"#define GENERATED_COMBO_COUNT {len(combo_lookup)}")
    h_lines.append(f"#define GENERATED_MAPO_COUNT {len(mapo_lookup)}")
    h_lines.append(f"#define GENERATED_FOOD_ICON_COUNT {len(food_icon_lookup)}")
    h_lines.append(f"#define GENERATED_RANDOM_IDLE_COUNT {len(random_idle_lookup)}")
    h_lines.append(f"#define GENERATED_TOTAL_BYTES {total_bytes}")
    h_lines.append("")

    # Generate lookup structs
    _generate_lookup_structs(h_lines, cpp_lines,
                            idle_lookup, eating_lookup, reaction_lookup,
                            poke_lookup, combo_lookup, mapo_lookup, food_icon_lookup,
                            random_idle_lookup)

    h_lines.append("")
    h_lines.append("#endif // GENERATED_ASSET_INDEX_H")

    return "\n".join(cpp_lines), "\n".join(h_lines)


def _register_lookup(category, base_name, seq_var_name,
                     idle_lookup, eating_lookup, reaction_lookup,
                     poke_lookup, combo_lookup, mapo_lookup, food_icon_lookup,
                     random_idle_lookup):
    """Register a sequence in the appropriate lookup table with numeric indices."""

    form_names_list = list(FORM_ENUM.keys())  # ordered list for index lookup
    condition_names_list = list(IDLE_CONDITION_MAP.keys())
    reaction_names_list = list(REACTION_MAP.keys())

    if category == "idle":
        # base_name like "black_saber_normal" or "white_lancer_lion_king_sr_max"
        for form_name in form_names_list:
            if base_name.startswith(form_name + "_"):
                condition = base_name[len(form_name) + 1:]
                if condition in IDLE_CONDITION_MAP:
                    form_idx = form_names_list.index(form_name)
                    cond_idx = condition_names_list.index(condition)
                    idle_lookup.append((form_idx, cond_idx, seq_var_name))
                break

    elif category == "eating":
        for form_name in form_names_list:
            if base_name.startswith(form_name + "_"):
                rest = base_name[len(form_name) + 1:]
                form_idx = form_names_list.index(form_name)
                if rest == "generic":
                    eating_lookup.append((form_idx, 255, seq_var_name))
                else:
                    for food_name, food_id in FOOD_MAP.items():
                        if rest == food_name:
                            eating_lookup.append((form_idx, food_id, seq_var_name))
                            return
                    # Unknown food name, treat as generic
                    eating_lookup.append((form_idx, 255, seq_var_name))
                break

    elif category == "reaction":
        for form_name in form_names_list:
            if base_name.startswith(form_name + "_"):
                reaction = base_name[len(form_name) + 1:]
                if reaction in REACTION_MAP:
                    form_idx = form_names_list.index(form_name)
                    # +1 because C++ ReactionType enum starts at 1 (REACTION_LIKE=1)
                    react_idx = reaction_names_list.index(reaction) + 1
                    reaction_lookup.append((form_idx, react_idx, seq_var_name))
                break

    elif category == "poke":
        for form_name in form_names_list:
            if base_name == form_name:
                form_idx = form_names_list.index(form_name)
                poke_lookup.append((form_idx, 0, seq_var_name))
                break

    elif category == "combo":
        for sfood_name, sfood_id in SFOOD_MAP.items():
            if base_name == sfood_name:
                combo_lookup.append((sfood_id, 0, seq_var_name))
                break

    elif category == "special":
        for form_name in form_names_list:
            if base_name == f"mapo_tofu_{form_name}":
                form_idx = form_names_list.index(form_name)
                mapo_lookup.append((form_idx, 0, seq_var_name))
                break

    elif category == "food_icons":
        for food_name, food_id in FOOD_MAP.items():
            if base_name == food_name:
                food_icon_lookup.append((food_id, 0, seq_var_name))
                break

    elif category == "special_food_icons":
        for sfood_name, sfood_id in SFOOD_MAP.items():
            if base_name == sfood_name:
                # Reuse combo lookup slot with key2=1 to distinguish from combo giver anims
                combo_lookup.append((sfood_id, 1, seq_var_name))
                break

    elif category == "random_idle":
        # base_name like "black_saber_smirk" or "lily_yawn"
        for form_name in form_names_list:
            if base_name.startswith(form_name + "_"):
                action = base_name[len(form_name) + 1:]
                form_idx = form_names_list.index(form_name)
                # Use action index: 0=yawn, 1=stretch, etc (just sequential)
                anim_idx = len([x for x in random_idle_lookup if x[0] == form_idx])
                random_idle_lookup.append((form_idx, anim_idx, seq_var_name))
                break


def _generate_lookup_structs(h_lines, cpp_lines,
                            idle_lookup, eating_lookup, reaction_lookup,
                            poke_lookup, combo_lookup, mapo_lookup, food_icon_lookup,
                            random_idle_lookup):
    """Generate C++ lookup table arrays from pre-computed numeric tuples."""

    cpp_lines.append("// === LOOKUP TABLES ===")
    cpp_lines.append("")

    h_lines.append("")
    h_lines.append("struct AssetLookupEntry {")
    h_lines.append("    uint8_t key1;  // form or food id")
    h_lines.append("    uint8_t key2;  // condition, reaction, etc")
    h_lines.append("    const AnimSequenceDescriptor* seq;")
    h_lines.append("};")
    h_lines.append("")

    def emit_table(name, entries, h_lines, cpp_lines):
        count = len(entries)
        h_lines.append(f"extern const AssetLookupEntry {name}[{count}];")
        cpp_lines.append(f"const AssetLookupEntry {name}[{max(count,1)}] = {{")
        for k1, k2, seq_var in entries:
            cpp_lines.append(f"    {{ {k1}, {k2}, &{seq_var} }},")
        cpp_lines.append("};")
        cpp_lines.append("")

    emit_table("IDLE_ASSET_TABLE", idle_lookup, h_lines, cpp_lines)
    emit_table("EATING_ASSET_TABLE", eating_lookup, h_lines, cpp_lines)
    emit_table("REACTION_ASSET_TABLE", reaction_lookup, h_lines, cpp_lines)
    emit_table("POKE_ASSET_TABLE", poke_lookup, h_lines, cpp_lines)
    emit_table("COMBO_ASSET_TABLE", combo_lookup, h_lines, cpp_lines)
    emit_table("MAPO_ASSET_TABLE", mapo_lookup, h_lines, cpp_lines)
    emit_table("FOOD_ICON_ASSET_TABLE", food_icon_lookup, h_lines, cpp_lines)
    emit_table("RANDOM_IDLE_ASSET_TABLE", random_idle_lookup, h_lines, cpp_lines)


# ============================================================================
#  Main
# ============================================================================

def main():
    print("=" * 60)
    print("  Asset Build Script - PROGMEM Generation")
    print("=" * 60)

    if not ASSETS_DIR.exists():
        print(f"\n  ERROR: Assets directory not found: {ASSETS_DIR}")
        sys.exit(1)

    if not HAS_PIL:
        print("\n  WARNING: Pillow not installed. Generating empty stubs.")
        OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
        with open(OUTPUT_DIR / "generated_asset_index.h", "w") as f:
            f.write("// No assets - Pillow not installed\n#ifndef GENERATED_ASSET_INDEX_H\n#define GENERATED_ASSET_INDEX_H\n#endif\n")
        with open(OUTPUT_DIR / "generated_asset_data.cpp", "w") as f:
            f.write("// No assets\n")
        return

    # Scan all categories
    categories = ["idle", "eating", "reaction", "food_icons", "combo", "poke", "special", "random_idle", "special_food_icons"]
    sequences_by_category = {}

    for category in categories:
        cat_dir = ASSETS_DIR / category
        if not cat_dir.exists():
            continue

        png_files = sorted(cat_dir.glob("*.png"))
        if not png_files:
            continue

        print(f"  [{category}] {len(png_files)} PNG files")

        # Group into sequences
        groups = group_into_sequences(png_files)
        sequences_by_category[category] = {}

        for base_name, frame_files in groups.items():
            frames_data = []
            for frame_num, filepath in frame_files:
                img = Image.open(filepath)
                pixels, w, h = image_to_rgb565_array(img)
                frames_data.append((pixels, w, h))
            sequences_by_category[category][base_name] = frames_data

        print(f"           → {len(groups)} sequences")

    # Generate C++ files
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    cpp_content, h_content = generate_cpp_and_header(sequences_by_category)

    cpp_path = OUTPUT_DIR / "generated_asset_data.cpp"
    h_path = OUTPUT_DIR / "generated_asset_index.h"

    with open(cpp_path, "w", encoding="utf-8") as f:
        f.write(cpp_content)
    with open(h_path, "w", encoding="utf-8") as f:
        f.write(h_content)

    # Stats
    total_seqs = sum(len(v) for v in sequences_by_category.values())
    cpp_size = cpp_path.stat().st_size
    print(f"\n  Generated: {h_path.name} + {cpp_path.name}")
    print(f"  Sequences: {total_seqs}")
    print(f"  CPP size:  {cpp_size:,} bytes")
    print("  Done!")


if __name__ == "__main__":
    main()
