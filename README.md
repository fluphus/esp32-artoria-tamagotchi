[English](README.md) | [简体中文](README_zh.md) | [日本語](README_ja.md)
# 👑 Artoria Tamagotchi (Fate/Grand Order Virtual Pet)

A Tamagotchi-like virtual pet project powered by the **ESP32-S3 (N16R8)**.

Raise your own Artoria! Start your journey with Saber Lily and guide her growth. Depending on your care and interactions, she can evolve into various classes including Archer, Lancer, Rider, and more.

## ✨ Features
* **Evolution System:** Multiple growth routes based on pet stats (Health, Seriousness, etc.).
* **Time & Day Management:** Built-in internal clock to handle daily resets and penalties for missing meals.
* **Persistent Storage:** 3-slot save system with checksum validation, serial slot import/export, and import-time clock alignment safeguards.
* **Interactive Actions:** Feed, Poke, and monitor status.
* **Display:** SSD1351 128x128 65K Color OLED via TFT_eSPI, with Serial placeholder backend for headless testing.

## 🚧 Current Project Status

* Display: TFT_eSPI backend (SSD1351 128x128) and Serial placeholder backend both compile and run.
* Input: 3-button physical input (L/M/R) with serial simulation (`btn l|m|r`, `btnl l|m|r`, `btnr l|m|r`).
* Assets: Placeholder rectangles and text when real sprites are not present.

## 🛠️ Hardware Requirement

* ESP32-S3 Development Board (N16R8 recommended)
* SSD1351 1.5" 128x128 65K Color OLED (SPI)
* 3 tactile buttons (GPIO 4, 5, 6)

### Pin Mapping

| Function | GPIO |
|----------|------|
| BTN_L    | 4    |
| BTN_M    | 5    |
| BTN_R    | 6    |
| TFT_DC   | 8    |
| TFT_RST  | 9    |
| TFT_CS   | 10   |
| TFT_MOSI | 11   |
| TFT_SCLK | 12   |

## Build

```bash
pio run                    # Build (TFT_eSPI backend, default)
pio run -t upload          # Upload to device
pio device monitor         # Serial monitor (115200 baud)
```

To switch to Serial placeholder backend (no screen), edit `src/display/display_config.h`:
```c
#define DISPLAY_BACKEND_SERIAL_PLACEHOLDER  1
#define DISPLAY_BACKEND_TFT_ESPI            0
```

## Button Mapping

### Idle Screen
| Button | Action |
|--------|--------|
| L      | Feed (start draw) |
| M (short) | Status panel |
| M (long)  | Open Gallery |
| R      | Poke |

### Status Panel
| Button | Action |
|--------|--------|
| M      | Close |

### Feed Pick (select 3 of 4)
| Button | Action |
|--------|--------|
| L      | Move cursor left |
| R      | Move cursor right |
| M      | Toggle selection (auto-submit at 3) |

### Special Food Selection
| Button | Action |
|--------|--------|
| L      | Move cursor up |
| R      | Move cursor down |
| M      | Select |

### Gallery
| Button | Action |
|--------|--------|
| L      | Navigate left (with page wrap) |
| R      | Navigate right (with page wrap) |
| M      | Close gallery (back to idle) |

### Destroy Confirm
| Button | Action |
|--------|--------|
| L      | Move to YES |
| R      | Move to NO |
| M      | Confirm selection |

### Destroy Combo
Hold all 3 buttons (L+M+R) for 5 seconds to trigger destroy confirmation (default cursor on NO).

## Smoke Test Procedure (Serial / Button)

Use serial commands to simulate button presses. Connect at 115200 baud.

### 1. Boot -> Idle
Power on. Boot screen displays for ~1.5s, then transitions to Idle automatically.

### 2. Status Panel
```
btn m          # Open status panel
btn m          # Close status panel (back to idle)
```

### 3. Feed Flow
```
btn l          # Start feed draw (shows 4 food cards with animation)
               # Wait ~800ms for draw animation to complete
               # Automatically enters Feed Pick screen
btn l          # Move cursor left
btn r          # Move cursor right
btn m          # Select food at cursor (repeat to select 3)
btn m          # Second selection
btn m          # Third selection -> auto-submit, shows Feed Result
               # Feed Result stays for at least 1.5 seconds
               # Then returns to idle automatically
```

### 4. Non-combo Feed Result Hold
After selecting 3 foods (no combo triggered):
- PAGE_FEED_RESULT displays for at least 1500ms
- Input is blocked during this hold period
- Page returns to idle automatically after hold expires

### 5. Combo Feed Result -> Special Food
If a combo is triggered:
- PAGE_FEED_RESULT shows with "COMBO" text
- ANIM_COMBO plays (~1000ms)
- After combo animation, enters Special Food selection
```
btn l          # Move cursor in special food list
btn r          # Move cursor in special food list
btn m          # Select special food
               # Confirmation displays for at least 1 second
               # Then returns to idle
```

### 6. Mapo Tofu Trigger
If mapo tofu is triggered during special food selection:
- ANIM_MAPO_TOFU plays (~1500ms)
- Input blocked during animation
- Returns to idle after animation

### 7. Poke
```
btn r          # Poke animation plays (~500ms)
               # Returns to idle after animation completes
```

### 8. Destroy (3-key combo)
```
btnl l         # Simulate long press L (in practice, hold all 3 for 5s)
               # For serial testing, use the 'reset' command instead
               # Or test destroy confirm UI:
ctx            # Check current context
```
Destroy confirm: default cursor on NO (safe). L/R to move, M to confirm.

### 9. Auto-save
Auto-save triggers periodically. It shows a brief toast "Autosaved" at the bottom of the screen without interrupting the current page or animation.

### 10. Debug Commands
```
s              # Print full status
h              # Help
fl             # Food list
t <min>        # Advance N minutes
d              # Advance 1 day (triggers day-end)
save           # Manual save
load           # Load save
erase          # Erase save
reset          # Destroy and reset pet
stime Y M D H m  # Set simulated time
SET_TIME <epoch> # Set system time (unix timestamp, triggers offline compensation)
hp <val>       # Debug: set health (0-100)
sr <val>       # Debug: set seriousness (0-100)
age <val>      # Debug: set age days
grad           # Debug: force child graduation
mapo           # Debug: +1 mapo tofu count
FORCE_NOBU     # Debug: force nobu route
UNLOCK_ALL     # Debug: unlock all gallery forms
RESET_GALLERY  # Debug: reset gallery (lock all)
IMPORT_TIME_SETUP  # Force import-time setup UI (no offline compensation)
SAVE_SLOT_STATUS   # Print slot0/1/2 status (seq/time/ver/size/crc)
SAVE_EXPORT <slot> # Export one slot snapshot over serial (hex stream)
SAVE_IMPORT_BEGIN <slot> # Start slot import session
SAVE_IMPORT_DATA <hex>   # Append one hex payload chunk
SAVE_IMPORT_COMMIT       # Commit import payload
SAVE_IMPORT_ABORT        # Abort import session
s0 / s1 / s2   # Print per-slot snapshot status
bright <0-15>  # Set screen brightness
dim <0-15>     # Set dim brightness
dim_t <sec>    # Set dim timeout
off_t <sec>    # Set off timeout
pwrsave        # Save power config to NVS
pwrinfo        # Print power config
btn l|m|r      # Simulate short press
btnl l|m|r     # Simulate long press
btnr l|m|r     # Simulate repeat
ctx            # Show current UI context
```

## Serial Save Import/Export

The project now supports per-slot save import/export over serial for backup and migration workflows.

### Firmware commands

- `SAVE_SLOT_STATUS`: Query slot status for all slots (`slot0`, `slot1`, `slot2`).
- `SAVE_EXPORT <slot>`: Export one slot as a hex stream block.
- `SAVE_IMPORT_BEGIN <slot>` -> `SAVE_IMPORT_DATA <hex>` -> `SAVE_IMPORT_COMMIT`: Import one slot payload.
- `SAVE_IMPORT_ABORT`: Abort the current import session.
- `s0` / `s1` / `s2`: Print one slot snapshot in status format.

### Host-side helper (Python)

From the `SaveManager` directory:

```bash
py save_manager.py
```

Or direct CLI usage:

```bash
py save_manager.py --port COM5 status
py save_manager.py --port COM5 export --slot 0 --out slot0.bin
py save_manager.py --port COM5 import --slot 1 --in slot0.bin
```

### Import behavior (important)

- The imported slot is always added to the active save pair.
- From the other two slots, the older one is selected as the second active slot.
- The remaining slot is frozen and excluded from future auto/manual overwrite rotation.
- After import, device enters forced time setup mode and skips offline compensation.
- After time confirmation, imported save time is aligned and persisted; if verification fails, background retry continues until success.

## Page Hold Behavior

The display system uses a "page hold" mechanism to ensure result/confirmation pages are visible:

- **Feed Result (non-combo):** Held for 1500ms, input blocked, then auto-returns to idle.
- **Special Food Confirm (non-mapo):** Held for 1000ms, input blocked, then auto-returns to idle.
- **Mapo Tofu:** Animation plays for 1500ms (managed by animation system, not hold).
- **Boot:** Auto-transitions to idle after 1500ms.
- **Day End:** Animation plays for 1500ms, input blocked.
- **Evolution:** Animation plays for 2000ms, input blocked.
- **Destroy animation:** Animation plays for 2000ms, input blocked.

During any hold or animation, `isPageBlockingInput()` returns true and the MenuController skips input processing.
