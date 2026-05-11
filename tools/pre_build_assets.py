"""
PlatformIO Pre-Build Script
Runs asset conversion before compilation (if assets exist)
"""
Import("env")

import subprocess
import sys
from pathlib import Path

project_dir = Path(env["PROJECT_DIR"])
assets_dir = project_dir / "assets"
convert_script = project_dir / "tools" / "convert_assets.py"

def run_asset_conversion(source, target, env):
    """Run asset conversion if PNG files exist"""
    # Check if there are any PNG files to convert
    has_assets = False
    for subdir in assets_dir.iterdir():
        if subdir.is_dir():
            if list(subdir.glob("*.png")):
                has_assets = True
                break
    
    if not has_assets:
        print("[Assets] No PNG files found in assets/, skipping conversion")
        return
    
    print("[Assets] Converting PNG assets...")
    try:
        result = subprocess.run(
            [sys.executable, str(convert_script)],
            cwd=str(project_dir),
            capture_output=True,
            text=True,
            timeout=60
        )
        if result.returncode == 0:
            print("[Assets] Conversion complete")
        else:
            print(f"[Assets] WARNING: Conversion failed: {result.stderr}")
    except FileNotFoundError:
        print("[Assets] WARNING: Python not found, skipping asset conversion")
    except subprocess.TimeoutExpired:
        print("[Assets] WARNING: Asset conversion timed out")
    except Exception as e:
        print(f"[Assets] WARNING: {e}")

# Only run if assets directory exists
if assets_dir.exists() and convert_script.exists():
    env.AddPreAction("buildprog", run_asset_conversion)
