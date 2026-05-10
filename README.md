[English](README.md) | [简体中文](README_zh.md) | [日本語](README_ja.md)
# 👑 Artoria Tamagotchi (Fate/Grand Order Virtual Pet)

A Tamagotchi-like virtual pet project powered by the **ESP32-S3 (N16R8)**.

Raise your own Artoria! Start your journey with Saber Lily and guide her growth. Depending on your care and interactions, she can evolve into various classes including Archer, Lancer, Rider, and more.

## ✨ Features
* **Evolution System:** Multiple growth routes based on pet stats (Health, Seriousness, etc.).
* **Time & Day Management:** Built-in internal clock to handle daily resets and penalties for missing meals.
* **Persistent Storage:** 3-slot save system with checksum validation, serial full or pet-only export, slot import with full-restore or visit (trade-in) mode, device-bound rounds/clock state, and import-time safeguards.
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
reset          # Destroy and reset pet (ends visit if visiting)
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
SAVE_SLOT_STATUS   # Print slot0/1/2 status (seq/time/ver/size/crc)
SAVE_EXPORT <slot> # Export full backup (pet + gallery) over serial (hex stream)
SAVE_EXPORT_PET <slot> # Export pet-only snapshot (visit/trade; hex stream)
SAVE_IMPORT_BEGIN <slot> # Start slot import session
SAVE_IMPORT_DATA <hex>   # Append one hex payload chunk
SAVE_IMPORT_COMMIT       # Commit import payload
SAVE_IMPORT_ABORT        # Abort import session
s0 / s1 / s2   # Print per-slot snapshot status
devinfo        # Show device state (rounds, visit mode, clock epoch)
bright <0-15>  # Set screen brightness
dim <0-15>     # Set dim brightness
dim_t <sec>    # Set dim timeout
off_t <sec>    # Set off timeout
pwrsave        # Save power config to NVS (optional; values also persist when set)
pwrinfo        # Print power config
btn l|m|r      # Simulate short press
btnl l|m|r     # Simulate long press
btnr l|m|r     # Simulate repeat
ctx            # Show current UI context
```

## Serial Save Import/Export

The project supports per-slot save import/export over serial for backup, migration, and pet-only exchange (“visit”) workflows.

### Device state, exports, and visit mode

Some fields are stored **per device** (not inside a slot): **`rounds`** (incremented on successful full import and on visit import), **`device_clock_epoch`** (baseline used with **`SET_TIME`** for offline catch-up), and **visit mode** flags (**`is_visiting`**, frozen owner slot, visit start time). Inspect them with the **`devinfo`** serial command.

- **`SAVE_EXPORT <slot>`** — Full backup: pet plus bundled gallery data for that workflow (larger payload).
- **`SAVE_EXPORT_PET <slot>`** — Pet-only snapshot for trading or **visit mode** (smaller payload; no gallery block).
- **`SAVE_IMPORT_BEGIN`** prints **`[SaveImport] READY ... target_bytes=... (legacy=...)`**, announcing the expected full and pet-only payload sizes; the host should send a file whose length matches one of these before **`SAVE_IMPORT_COMMIT`**.

**Full import** (payload length = full backup): restores gallery from the bundle, rebuilds the active slot pair around the target slot, aligns pet timestamps to the local clock, clears visit mode, bumps **`rounds`**, updates **`device_clock_epoch`**, and persists.

**Pet-only import** (payload length = pet-only): **Visit mode** — among the two active slots, the newer pet is treated as the **owner** and that slot stays frozen on disk; the imported slot holds the **guest**, which is loaded into RAM. Local gallery data stays in place, but the guest’s current form is unlocked if needed. Import into the owner’s latest slot is **rejected** (pick the other active slot). **`rounds`** and **`device_clock_epoch`** are updated similarly to a full import.

**While visiting:** new **`SAVE_IMPORT_*`** is rejected; **`SET_TIME`**, **`stime`**, **`t <min>`**, and **`d`** are rejected so time does not drift with a guest loaded. **`reset`** destroys the guest and restores the owner (visit ends). Invalid payloads (e.g. **`save_time == 0`**) print **`[SaveImport] REJECTED:`** and abort.

### Firmware commands

- `SAVE_SLOT_STATUS`: Query slot status for all slots (`slot0`, `slot1`, `slot2`).
- `SAVE_EXPORT <slot>`: Export **full** backup (pet + gallery) as a hex stream block.
- `SAVE_EXPORT_PET <slot>`: Export **pet-only** snapshot as a hex stream block.
- `SAVE_IMPORT_BEGIN <slot>` -> `SAVE_IMPORT_DATA <hex>` -> `SAVE_IMPORT_COMMIT`: Import one slot payload (length selects full restore vs visit).
- `SAVE_IMPORT_ABORT`: Abort the current import session.
- `s0` / `s1` / `s2`: Print one slot snapshot in status format.
- `devinfo`: Print device-bound state (rounds, visit mode, clock epoch).

### Host-side helper (Python)

From the `SaveManager` directory:

```bash
py save_manager.py
```

Or direct CLI usage:

```bash
py save_manager.py --port COM5 status
py save_manager.py --port COM5 export --slot 0 --out slot0_full.bin
py save_manager.py --port COM5 export --slot 0 --out slot0_pet.bin --pet-only
py save_manager.py --port COM5 import --slot 1 --in slot0_full.bin
```

### Import behavior (important)

- **Full backup:** The imported slot joins the active pair; from the other two slots, the **older** one becomes the second active slot (by sequence, then save time; empty slots count as older). The remaining slot is **frozen** and skipped by auto/manual rotation. Success line: **`[SaveImport] Full backup restored ...`**.
- **Pet-only (visit):** Owner slot is frozen; guest lives in the imported slot; **`[SaveImport] Visit started ...`** on success. **`reset`** ends the visit and restores the owner.
- If the device was waiting for **first-time** clock setup when import completes, it leaves that wait using the stored device clock anchor when possible; normal boots are unchanged. Offline “catch-up from old save time” is **not** applied in the import paths described above.
- After alignment, data is persisted; if verification fails elsewhere in the stack, background retry may still apply as implemented.

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
