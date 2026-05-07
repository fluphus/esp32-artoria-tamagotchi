[English](README.md) | [简体中文](README_zh.md) | [日本語](README_ja.md)
# 👑 Artoria Tamagotchi (Fate/Grand Order Virtual Pet)

A Tamagotchi-like virtual pet project powered by the **ESP32-S3 (N16R8)**.

Raise your own Artoria! Start your journey with Saber Lily and guide her growth. Depending on your care and interactions, she can evolve into various classes including Archer, Lancer, Rider, and more.

## ✨ Features
* **Evolution System:** Multiple growth routes based on pet stats (Health, Seriousness, etc.).
* **Time & Day Management:** Built-in internal clock to handle daily resets and penalties for missing meals.
* **Persistent Storage:** Auto save/load system with checksum validation (prevents data corruption on power loss).
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
| M      | Status panel |
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
t 60           # Advance 60 minutes
d              # Advance 1 day (triggers day-end)
save           # Manual save
load           # Load save
erase          # Erase save
reset          # Destroy and reset pet
ctx            # Show current UI context
```

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
