#!/usr/bin/env python3
"""
Generate placeholder stick-figure animations for all forms and animation types.
Each form gets a unique color. Each animation type has distinct poses.
Text label at bottom identifies the animation.
"""

from PIL import Image, ImageDraw, ImageFont
import os
from pathlib import Path

# ============================================================================
#  Configuration
# ============================================================================

SPRITE_SIZE = 64  # 64x64 pixels
OUTPUT_DIR = Path(__file__).parent.parent / "assets"

# Form definitions: name -> color (RGB)
FORMS = {
    "lily":                 (255, 200, 220),   # pink
    "white_saber":          (255, 255, 255),   # white
    "black_saber":          (80, 80, 80),      # dark gray
    "white_lancer":         (200, 200, 255),   # light blue
    "black_lancer":         (100, 50, 150),    # purple
    "white_archer":         (255, 100, 100),   # red
    "black_rider":          (150, 100, 50),    # brown
    "white_ruler":          (255, 215, 0),     # gold
    "rhongomyniad":         (0, 200, 255),     # cyan
    "black_rhongomyniad":   (50, 0, 80),       # very dark purple
    "nobu":                 (255, 150, 0),     # orange
    "oda_nobunaga":         (200, 0, 0),       # dark red
}

# Background color (transparent would be ideal but for visibility use dark bg)
BG_COLOR = (0, 0, 0, 0)  # transparent

# ============================================================================
#  Drawing helpers
# ============================================================================

def draw_stick_figure(draw, cx, cy, color, pose="neutral", scale=1.0):
    """Draw a stick figure at center (cx, cy) with given pose."""
    s = scale
    head_r = int(6 * s)
    
    # Head
    head_y = cy - int(20 * s)
    draw.ellipse([cx - head_r, head_y - head_r, cx + head_r, head_y + head_r],
                 outline=color, width=2)
    
    # Body
    body_top = head_y + head_r
    body_bot = cy + int(5 * s)
    draw.line([cx, body_top, cx, body_bot], fill=color, width=2)
    
    # Arms and legs based on pose
    if pose == "neutral":
        # Arms down
        draw.line([cx, body_top + int(8*s), cx - int(10*s), cy], fill=color, width=2)
        draw.line([cx, body_top + int(8*s), cx + int(10*s), cy], fill=color, width=2)
        # Legs
        draw.line([cx, body_bot, cx - int(8*s), cy + int(18*s)], fill=color, width=2)
        draw.line([cx, body_bot, cx + int(8*s), cy + int(18*s)], fill=color, width=2)
    
    elif pose == "neutral_alt":
        # Arms slightly raised
        draw.line([cx, body_top + int(8*s), cx - int(12*s), cy - int(3*s)], fill=color, width=2)
        draw.line([cx, body_top + int(8*s), cx + int(12*s), cy - int(3*s)], fill=color, width=2)
        # Legs slightly apart
        draw.line([cx, body_bot, cx - int(10*s), cy + int(18*s)], fill=color, width=2)
        draw.line([cx, body_bot, cx + int(10*s), cy + int(18*s)], fill=color, width=2)
    
    elif pose == "eating_open":
        # Mouth open (bigger head gap), arms reaching forward
        draw.line([cx, body_top + int(8*s), cx + int(14*s), cy - int(5*s)], fill=color, width=2)
        draw.line([cx, body_top + int(8*s), cx - int(5*s), cy + int(2*s)], fill=color, width=2)
        draw.line([cx, body_bot, cx - int(8*s), cy + int(18*s)], fill=color, width=2)
        draw.line([cx, body_bot, cx + int(8*s), cy + int(18*s)], fill=color, width=2)
        # Open mouth
        draw.arc([cx - int(3*s), head_y + int(2*s), cx + int(3*s), head_y + int(7*s)],
                 0, 180, fill=color, width=1)
    
    elif pose == "eating_chew":
        # Chewing, arms at sides
        draw.line([cx, body_top + int(8*s), cx + int(12*s), cy - int(2*s)], fill=color, width=2)
        draw.line([cx, body_top + int(8*s), cx - int(8*s), cy + int(3*s)], fill=color, width=2)
        draw.line([cx, body_bot, cx - int(7*s), cy + int(18*s)], fill=color, width=2)
        draw.line([cx, body_bot, cx + int(9*s), cy + int(18*s)], fill=color, width=2)
        # Closed mouth line
        draw.line([cx - int(3*s), head_y + int(4*s), cx + int(3*s), head_y + int(4*s)],
                  fill=color, width=1)
    
    elif pose == "happy":
        # Arms up in V
        draw.line([cx, body_top + int(8*s), cx - int(12*s), cy - int(12*s)], fill=color, width=2)
        draw.line([cx, body_top + int(8*s), cx + int(12*s), cy - int(12*s)], fill=color, width=2)
        draw.line([cx, body_bot, cx - int(8*s), cy + int(18*s)], fill=color, width=2)
        draw.line([cx, body_bot, cx + int(8*s), cy + int(18*s)], fill=color, width=2)
        # Smile
        draw.arc([cx - int(3*s), head_y + int(1*s), cx + int(3*s), head_y + int(6*s)],
                 0, 180, fill=color, width=1)
    
    elif pose == "happy_jump":
        # Arms up, one leg up (jumping)
        head_y -= int(3*s)
        body_top -= int(3*s)
        body_bot -= int(3*s)
        draw.ellipse([cx - head_r, head_y - head_r, cx + head_r, head_y + head_r],
                     outline=color, width=2)
        draw.line([cx, body_top, cx, body_bot], fill=color, width=2)
        draw.line([cx, body_top + int(8*s), cx - int(12*s), cy - int(15*s)], fill=color, width=2)
        draw.line([cx, body_top + int(8*s), cx + int(12*s), cy - int(15*s)], fill=color, width=2)
        draw.line([cx, body_bot, cx - int(10*s), cy + int(14*s)], fill=color, width=2)
        draw.line([cx, body_bot, cx + int(5*s), cy + int(12*s)], fill=color, width=2)
        draw.arc([cx - int(3*s), head_y + int(1*s), cx + int(3*s), head_y + int(6*s)],
                 0, 180, fill=color, width=1)
        return  # already drew head
    
    elif pose == "sad":
        # Arms drooping down
        draw.line([cx, body_top + int(8*s), cx - int(8*s), cy + int(8*s)], fill=color, width=2)
        draw.line([cx, body_top + int(8*s), cx + int(8*s), cy + int(8*s)], fill=color, width=2)
        draw.line([cx, body_bot, cx - int(6*s), cy + int(18*s)], fill=color, width=2)
        draw.line([cx, body_bot, cx + int(6*s), cy + int(18*s)], fill=color, width=2)
        # Frown
        draw.arc([cx - int(3*s), head_y + int(3*s), cx + int(3*s), head_y + int(8*s)],
                 180, 360, fill=color, width=1)
    
    elif pose == "sad_slump":
        # Slumped, head tilted
        draw.line([cx, body_top + int(8*s), cx - int(10*s), cy + int(6*s)], fill=color, width=2)
        draw.line([cx, body_top + int(8*s), cx + int(6*s), cy + int(8*s)], fill=color, width=2)
        draw.line([cx, body_bot, cx - int(7*s), cy + int(19*s)], fill=color, width=2)
        draw.line([cx, body_bot, cx + int(7*s), cy + int(17*s)], fill=color, width=2)
        draw.arc([cx - int(3*s), head_y + int(3*s), cx + int(3*s), head_y + int(8*s)],
                 180, 360, fill=color, width=1)
    
    elif pose == "angry":
        # Arms crossed / fists
        draw.line([cx, body_top + int(8*s), cx - int(14*s), cy - int(5*s)], fill=color, width=2)
        draw.line([cx, body_top + int(8*s), cx + int(14*s), cy - int(5*s)], fill=color, width=2)
        draw.line([cx, body_bot, cx - int(9*s), cy + int(18*s)], fill=color, width=2)
        draw.line([cx, body_bot, cx + int(9*s), cy + int(18*s)], fill=color, width=2)
        # Angry eyebrows (V shape above head)
        draw.line([cx - int(4*s), head_y - head_r - int(2*s),
                   cx - int(1*s), head_y - head_r + int(1*s)], fill=color, width=1)
        draw.line([cx + int(4*s), head_y - head_r - int(2*s),
                   cx + int(1*s), head_y - head_r + int(1*s)], fill=color, width=1)
    
    elif pose == "angry_stomp":
        # Stomping
        draw.line([cx, body_top + int(8*s), cx - int(14*s), cy - int(3*s)], fill=color, width=2)
        draw.line([cx, body_top + int(8*s), cx + int(14*s), cy - int(3*s)], fill=color, width=2)
        draw.line([cx, body_bot, cx - int(12*s), cy + int(16*s)], fill=color, width=2)
        draw.line([cx, body_bot, cx + int(5*s), cy + int(19*s)], fill=color, width=2)
        draw.line([cx - int(4*s), head_y - head_r - int(2*s),
                   cx - int(1*s), head_y - head_r + int(1*s)], fill=color, width=1)
        draw.line([cx + int(4*s), head_y - head_r - int(2*s),
                   cx + int(1*s), head_y - head_r + int(1*s)], fill=color, width=1)
    
    elif pose == "poke_react":
        # Surprised jump back
        draw.line([cx, body_top + int(8*s), cx - int(14*s), cy - int(8*s)], fill=color, width=2)
        draw.line([cx, body_top + int(8*s), cx + int(14*s), cy - int(8*s)], fill=color, width=2)
        draw.line([cx, body_bot, cx - int(10*s), cy + int(16*s)], fill=color, width=2)
        draw.line([cx, body_bot, cx + int(10*s), cy + int(16*s)], fill=color, width=2)
        # Surprise marks
        draw.text((cx + int(10*s), head_y - head_r - int(5*s)), "!", fill=color)
    
    elif pose == "poke_recover":
        # Recovering
        draw.line([cx, body_top + int(8*s), cx - int(10*s), cy - int(2*s)], fill=color, width=2)
        draw.line([cx, body_top + int(8*s), cx + int(10*s), cy - int(2*s)], fill=color, width=2)
        draw.line([cx, body_bot, cx - int(8*s), cy + int(18*s)], fill=color, width=2)
        draw.line([cx, body_bot, cx + int(8*s), cy + int(18*s)], fill=color, width=2)
    
    elif pose == "perfect_star":
        # Star pose - arms and legs spread wide
        draw.line([cx, body_top + int(8*s), cx - int(15*s), cy - int(10*s)], fill=color, width=2)
        draw.line([cx, body_top + int(8*s), cx + int(15*s), cy - int(10*s)], fill=color, width=2)
        draw.line([cx, body_bot, cx - int(12*s), cy + int(16*s)], fill=color, width=2)
        draw.line([cx, body_bot, cx + int(12*s), cy + int(16*s)], fill=color, width=2)
        # Star above head
        draw.text((cx - int(3*s), head_y - head_r - int(8*s)), "*", fill=(255, 255, 0))
    
    elif pose == "perfect_spin":
        # Spinning (tilted)
        draw.line([cx, body_top + int(8*s), cx - int(15*s), cy - int(5*s)], fill=color, width=2)
        draw.line([cx, body_top + int(8*s), cx + int(10*s), cy - int(12*s)], fill=color, width=2)
        draw.line([cx, body_bot, cx - int(5*s), cy + int(18*s)], fill=color, width=2)
        draw.line([cx, body_bot, cx + int(12*s), cy + int(14*s)], fill=color, width=2)
        draw.text((cx - int(3*s), head_y - head_r - int(8*s)), "*", fill=(255, 255, 0))


def create_frame(form_name, color, label, pose, frame_idx):
    """Create a single animation frame."""
    img = Image.new("RGBA", (SPRITE_SIZE, SPRITE_SIZE), BG_COLOR)
    draw = ImageDraw.Draw(img)
    
    # Draw stick figure
    cx = SPRITE_SIZE // 2
    cy = SPRITE_SIZE // 2 - 4  # leave room for label at bottom
    draw_stick_figure(draw, cx, cy, color, pose, scale=0.9)
    
    # Draw label at bottom
    short_label = label[:16]  # truncate if too long
    # Calculate text position (centered at bottom)
    try:
        bbox = draw.textbbox((0, 0), short_label)
        tw = bbox[2] - bbox[0]
    except:
        tw = len(short_label) * 6
    tx = (SPRITE_SIZE - tw) // 2
    ty = SPRITE_SIZE - 10
    draw.text((tx, ty), short_label, fill=(200, 200, 200))
    
    # Draw form name at top
    short_name = form_name[:12]
    try:
        bbox = draw.textbbox((0, 0), short_name)
        nw = bbox[2] - bbox[0]
    except:
        nw = len(short_name) * 6
    nx = (SPRITE_SIZE - nw) // 2
    draw.text((nx, 1), short_name, fill=color)
    
    return img


# ============================================================================
#  Animation definitions: (label, [pose_frame0, pose_frame1, ...])
# ============================================================================

IDLE_ANIMS = {
    "normal":       ("idle", ["neutral", "neutral_alt"]),
    "sr_high":      ("sr_high", ["neutral", "angry"]),
    "hp_low":       ("hp_low", ["sad", "sad_slump"]),
    "hp_critical":  ("hp_crit", ["sad_slump", "sad"]),
}

# Special idle for white_lancer
LANCER_SPECIAL_IDLES = {
    "lion_king_sr_max":      ("LK_max", ["angry", "angry_stomp"]),
    "lion_king_sr_critical": ("LK_crit", ["angry_stomp", "angry"]),
}

REACTION_ANIMS = {
    "like":     ("like", ["happy", "happy_jump"]),
    "dislike":  ("dislike", ["sad", "sad_slump"]),
    "umu":      ("umu", ["happy", "happy_jump"]),
    "eww":      ("eww", ["sad", "angry"]),
    "satisfy":  ("satisfy", ["happy_jump", "perfect_star"]),
    "abhor":    ("abhor", ["angry", "angry_stomp"]),
    "perfect":  ("perfect", ["perfect_star", "perfect_spin"]),
}

EATING_POSES = ["eating_open", "eating_chew"]
POKE_POSES = ["poke_react", "poke_recover"]


# ============================================================================
#  Generation
# ============================================================================

def generate_idle_anims():
    """Generate idle animations for all forms."""
    out_dir = OUTPUT_DIR / "idle"
    out_dir.mkdir(parents=True, exist_ok=True)
    count = 0
    
    for form_name, color in FORMS.items():
        for condition, (label, poses) in IDLE_ANIMS.items():
            for i, pose in enumerate(poses):
                full_label = f"{form_name[:6]}_{label}"
                img = create_frame(form_name, color, full_label, pose, i)
                filename = f"{form_name}_{condition}_f{i}.png"
                img.save(out_dir / filename)
                count += 1
        
        # Special lancer idles
        if form_name == "white_lancer":
            for condition, (label, poses) in LANCER_SPECIAL_IDLES.items():
                for i, pose in enumerate(poses):
                    full_label = f"wlancer_{label}"
                    img = create_frame(form_name, color, full_label, pose, i)
                    filename = f"{form_name}_{condition}_f{i}.png"
                    img.save(out_dir / filename)
                    count += 1
    
    print(f"  [idle] Generated {count} frames")


def generate_eating_anims():
    """Generate generic eating animations for all forms."""
    out_dir = OUTPUT_DIR / "eating"
    out_dir.mkdir(parents=True, exist_ok=True)
    count = 0
    
    for form_name, color in FORMS.items():
        for i, pose in enumerate(EATING_POSES):
            label = f"{form_name[:8]}_eat"
            img = create_frame(form_name, color, label, pose, i)
            filename = f"{form_name}_generic_f{i}.png"
            img.save(out_dir / filename)
            count += 1
    
    print(f"  [eating] Generated {count} frames")


def generate_reaction_anims():
    """Generate reaction animations for all forms."""
    out_dir = OUTPUT_DIR / "reaction"
    out_dir.mkdir(parents=True, exist_ok=True)
    count = 0
    
    for form_name, color in FORMS.items():
        for reaction, (label, poses) in REACTION_ANIMS.items():
            for i, pose in enumerate(poses):
                full_label = f"{form_name[:6]}_{label}"
                img = create_frame(form_name, color, full_label, pose, i)
                filename = f"{form_name}_{reaction}_f{i}.png"
                img.save(out_dir / filename)
                count += 1
    
    print(f"  [reaction] Generated {count} frames")


def generate_poke_anims():
    """Generate poke animations for all forms."""
    out_dir = OUTPUT_DIR / "poke"
    out_dir.mkdir(parents=True, exist_ok=True)
    count = 0
    
    for form_name, color in FORMS.items():
        for i, pose in enumerate(POKE_POSES):
            label = f"{form_name[:8]}_poke"
            img = create_frame(form_name, color, label, pose, i)
            filename = f"{form_name}_f{i}.png"
            img.save(out_dir / filename)
            count += 1
    
    print(f"  [poke] Generated {count} frames")


def generate_food_icons():
    """Generate food icon animations."""
    out_dir = OUTPUT_DIR / "food_icons"
    out_dir.mkdir(parents=True, exist_ok=True)
    
    foods = ["rice_ball", "salad", "grilled_fish", "bread",
             "cake", "fried_chicken", "soda", "candy"]
    food_colors = [
        (255, 255, 255),  # rice - white
        (100, 200, 100),  # salad - green
        (200, 150, 100),  # fish - tan
        (220, 180, 100),  # bread - wheat
        (255, 150, 200),  # cake - pink
        (200, 100, 50),   # chicken - orange-brown
        (100, 150, 255),  # soda - blue
        (255, 100, 100),  # candy - red
    ]
    count = 0
    
    for food, color in zip(foods, food_colors):
        for i in range(2):
            img = Image.new("RGBA", (32, 32), (0, 0, 0, 0))
            draw = ImageDraw.Draw(img)
            # Frame 0: full food, Frame 1: half eaten (smaller)
            size = 12 - i * 4
            cx, cy = 16, 12
            draw.ellipse([cx - size, cy - size, cx + size, cy + size], fill=color)
            if i == 1:
                # Bite mark
                draw.ellipse([cx + 2, cy - 6, cx + 12, cy + 4], fill=(0, 0, 0, 0))
            # Label
            try:
                bbox = draw.textbbox((0, 0), food[:8])
                tw = bbox[2] - bbox[0]
            except:
                tw = len(food[:8]) * 6
            draw.text(((32 - tw) // 2, 24), food[:8], fill=(180, 180, 180))
            
            filename = f"{food}_f{i}.png"
            img.save(out_dir / filename)
            count += 1
    
    print(f"  [food_icons] Generated {count} frames")


def generate_combo_anims():
    """Generate combo giver animations."""
    out_dir = OUTPUT_DIR / "combo"
    out_dir.mkdir(parents=True, exist_ok=True)
    
    specials = {
        "golden_apple":   (180, 0, 255),    # Merlin - purple
        "holy_grail_mug": (255, 215, 0),    # Grail - gold
        "emiya_cooking":  (255, 50, 50),    # Archer - red
        "jaguar_snack":   (255, 200, 0),    # Taiga - yellow
    }
    count = 0
    
    for name, color in specials.items():
        for i in range(2):
            img = Image.new("RGBA", (SPRITE_SIZE, SPRITE_SIZE), (0, 0, 0, 0))
            draw = ImageDraw.Draw(img)
            # Draw a different stick figure (the giver)
            cx = SPRITE_SIZE // 2
            cy = SPRITE_SIZE // 2 - 4
            pose = "happy" if i == 0 else "happy_jump"
            draw_stick_figure(draw, cx, cy, color, pose, scale=0.9)
            # Label
            short = name[:12]
            try:
                bbox = draw.textbbox((0, 0), short)
                tw = bbox[2] - bbox[0]
            except:
                tw = len(short) * 6
            draw.text(((SPRITE_SIZE - tw) // 2, SPRITE_SIZE - 10), short, fill=(200, 200, 200))
            draw.text(((SPRITE_SIZE - tw) // 2, 1), "COMBO", fill=color)
            
            filename = f"{name}_f{i}.png"
            img.save(out_dir / filename)
            count += 1
    
    print(f"  [combo] Generated {count} frames")


def generate_special_anims():
    """Generate mapo tofu animations."""
    out_dir = OUTPUT_DIR / "special"
    out_dir.mkdir(parents=True, exist_ok=True)
    count = 0
    
    mapo_color = (200, 50, 0)  # Kirei - dark red
    
    for form_name, color in FORMS.items():
        for i in range(2):
            img = Image.new("RGBA", (SPRITE_SIZE, SPRITE_SIZE), (0, 0, 0, 0))
            draw = ImageDraw.Draw(img)
            cx = SPRITE_SIZE // 2
            cy = SPRITE_SIZE // 2 - 4
            pose = "sad" if i == 0 else "angry"
            draw_stick_figure(draw, cx, cy, color, pose, scale=0.9)
            # Mapo tofu icon (small red square)
            draw.rectangle([cx + 15, cy - 10, cx + 25, cy], fill=mapo_color)
            label = f"{form_name[:6]}_mapo"
            try:
                bbox = draw.textbbox((0, 0), label)
                tw = bbox[2] - bbox[0]
            except:
                tw = len(label) * 6
            draw.text(((SPRITE_SIZE - tw) // 2, SPRITE_SIZE - 10), label, fill=(200, 200, 200))
            
            filename = f"mapo_tofu_{form_name}_f{i}.png"
            img.save(out_dir / filename)
            count += 1
    
    print(f"  [special] Generated {count} frames")


def generate_random_idle_anims():
    """Generate random idle animations for a few forms."""
    out_dir = OUTPUT_DIR / "random_idle"
    out_dir.mkdir(parents=True, exist_ok=True)
    count = 0
    
    # Only generate for a few forms as examples
    random_anims = {
        "lily": [("yawn", ["sad", "neutral"]), ("stretch", ["perfect_star", "neutral"])],
        "white_saber": [("look_around", ["neutral", "neutral_alt"]), ("sigh", ["sad", "neutral"])],
        "black_saber": [("smirk", ["angry", "neutral"]), ("cross_arms", ["angry_stomp", "angry"])],
    }
    
    for form_name, anims in random_anims.items():
        color = FORMS[form_name]
        for anim_name, poses in anims:
            for i, pose in enumerate(poses):
                label = f"{form_name[:6]}_{anim_name[:6]}"
                img = create_frame(form_name, color, label, pose, i)
                filename = f"{form_name}_{anim_name}_f{i}.png"
                img.save(out_dir / filename)
                count += 1
    
    print(f"  [random_idle] Generated {count} frames")


# ============================================================================
#  Main
# ============================================================================

def main():
    print("=" * 50)
    print("  Generating Placeholder Animations")
    print("=" * 50)
    print()
    
    generate_idle_anims()
    generate_eating_anims()
    generate_reaction_anims()
    generate_poke_anims()
    generate_food_icons()
    generate_combo_anims()
    generate_special_anims()
    generate_random_idle_anims()
    
    # Count total files
    total = 0
    for subdir in OUTPUT_DIR.iterdir():
        if subdir.is_dir() and subdir.name != "__pycache__":
            pngs = list(subdir.glob("*.png"))
            total += len(pngs)
    
    print(f"\n  Total: {total} PNG files generated")
    print("  Done!")


if __name__ == "__main__":
    main()
