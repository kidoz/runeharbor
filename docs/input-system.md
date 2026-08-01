---
title: "Input System"
summary: "Input handling combines synchronous and asynchronous DirectInput paths with cursor management."
doc_type: reference
status: partial
last_updated: 2026-08-01
---
# Input System

Input handling combines synchronous and asynchronous DirectInput paths with cursor management.

Coverage is limited to MM7 v1.21 and the artifacts cited in Scope.

## Scope

This page covers MM7 v1.21 behavior and the binary or data artifacts cited below.
Unmarked findings are **observed**; deductions and gaps are labeled **inferred**
or **unknown** per the [documentation standard](contributing/documentation-standard.md).
Artifacts and versions not cited on this page are out of scope.
RuneHarbor-specific decisions, when present, belong in Integration notes.

---

## Overview

The input system uses Microsoft DirectInput (via `DINPUT.DLL`) for keyboard and mouse
handling. There are two separate implementations: a synchronous (event-driven)
interface used in the main game loop, and an asynchronous (polling) interface used for
real-time input during gameplay. Cursor management uses Win32 API calls.

**Source file references:**

- `D:\mm7Src_eng\MM7\Code\DirectInputKeyboard.cpp` (at `004e4ad8`)
- `D:\mm7Src_eng\MM7\Code\DirectInputMouse.cpp` (at `004e4bb4`)
- `D:\mm7Src_eng\MM7\Code\KeyboardAsync.cpp` (at `004e924c`)
- `D:\mm7Src_eng\MM7\Code\MouseAsync.cpp` (at `004eadf4`)
- `D:\mm7Src_eng\MM7\Code\Mouse.cpp` (at `004eada0`)

---

## 1. DirectInput Initialization

The engine creates DirectInput objects via `DirectInputCreateA` (imported from
`DINPUT.DLL` at `004ddab4`). Two device types are initialized:

### 1.1 Keyboard Device

**Setup function group:** `FUN_0043b854`, `FUN_0043b89e`, `FUN_0043b90f`,
`FUN_0043b991`, `FUN_0043b790`

Initialization sequence:

1. Call `DirectInputCreateA` to get the `IDirectInput` interface
2. Call `CreateDevice` with `GUID_SysKeyboard` to get a keyboard device
3. Set data format to standard keyboard format (256-byte state array)
4. Set cooperative level (foreground, non-exclusive)
5. Acquire the device

Error: `"Error: No keyboard found"` (at `004e4b28`) if device creation fails.

### 1.2 Mouse Device

**Setup function group:** `FUN_0043ba22`, `FUN_0043baf1`, `FUN_0043bb3b`,
`FUN_0043bbac`

Initialization sequence:

1. Call `DirectInputCreateA` to get the `IDirectInput` interface
2. Call `CreateDevice` with `GUID_SysMouse` to get a mouse device
3. Set data format to standard mouse format (DIMOUSESTATE)
4. Set cooperative level (foreground, non-exclusive)
5. Acquire the device

Error: `"Error: No mouse found"` (at `004e4be0`) if device creation fails.

---

## 2. Keyboard Async (Polling Interface)

**Source:** `KeyboardAsync.cpp` at `004e924c`
**Functions:** `FUN_0045b143`, `FUN_0045b2e0`, `FUN_0045b362`, `FUN_0045b3ef`,
`FUN_0045b5b6`

The async keyboard interface polls the device state each frame for real-time input
(movement, combat, etc.).

### 2.1 Operations

| Function       | Purpose                                    |
|----------------|--------------------------------------------|
| `FUN_0045b143` | Initialize async keyboard object           |
| `FUN_0045b2e0` | Resume keyboard (re-acquire after focus)   |
| `FUN_0045b362` | Suspend keyboard (release on focus loss)   |
| `FUN_0045b3ef` | Update keyboard data (poll current state)  |
| `FUN_0045b5b6` | Process keyboard state (key-down checks)   |

### 2.2 Error Messages

| Error String                                                  | Condition           |
|---------------------------------------------------------------|---------------------|
| `"Could not initialize asynchronos keyboard object"`          | Init failure        |
| `"Invalid DI_Keyboard, bailing out of resume()"`             | NULL device pointer |
| `"Invalid DI_Keyboard, bailing out of suspend()"`            | NULL device pointer |
| `"Invalid DI_Keyboard, bailing out of update_keyboard_data()"` | NULL device pointer |

### 2.3 Key State

The keyboard state is a 256-byte array where each byte represents one key. A byte
with the high bit set (0x80) indicates the key is currently pressed. The engine polls
this array via `GetDeviceState` each frame.

### 2.4 Key Binding Strings

Key bindings are loaded from text data. Known binding identifiers:

| String           | Purpose                    |
|------------------|----------------------------|
| `KEY_CAST`       | Cast spell key             |
| `KEY_CASTREADY`  | Cast-ready toggle key      |

---

## 3. Mouse Async (Polling Interface)

**Source:** `MouseAsync.cpp` at `004eadf4`
**Functions:** `FUN_0046bbd4`, `FUN_0046ae9b`, `FUN_0046af54`, `FUN_0046b153`,
`FUN_0046b380`

The async mouse interface polls for real-time cursor position and button state.

### 3.1 Operations

| Function       | Purpose                                      |
|----------------|----------------------------------------------|
| `FUN_0046acad` | Initialize async mouse object                |
| `FUN_0046ae9b` | Resume mouse device                          |
| `FUN_0046af54` | Load async mouse cursor image                |
| `FUN_0046b153` | Suspend mouse (release device)               |
| `FUN_0046b380` | Update mouse data (poll delta + buttons)     |
| `FUN_0046bbd4` | Clip cursor to screen boundaries             |

### 3.2 Error Messages

| Error String                                                    | Condition           |
|-----------------------------------------------------------------|---------------------|
| `"Could not initialize CMouseAsync object"`                     | Init failure        |
| `"Could not load async mouse cursor image"`                     | Cursor load failed  |
| `"DI_Mouse pointer invalid; bailing out from suspend()"`        | NULL device pointer |
| `"DI_Mouse pointer invalid bailing out from update_mouse_data()"` | NULL device pointer |
| `"Could not clip cursor to screen!"`                            | ClipCursor failed   |

### 3.3 Mouse State

DirectInput mouse returns delta values (relative motion) rather than absolute
position. The engine tracks:

- X/Y delta per frame (from `DIMOUSESTATE`)
- Button states (left, right, middle)
- Accumulated absolute position (maintained by the engine)

---

## 4. Cursor Management

The engine uses Win32 API calls for cursor display and positioning:

| Win32 API        | Address    | Purpose                        |
|------------------|------------|--------------------------------|
| `GetCursorPos`   | `004dcf22` | Read current cursor position   |
| `SetCursorPos`   | `004dd1b0` | Set cursor to specific coords  |
| `ClipCursor`     | `004dcfb4` | Constrain cursor to rectangle  |
| `LoadCursorA`    | `004dd1d0` | Load cursor resource           |
| `ShowCursor`     | `004dd1fe` | Show or hide the cursor        |
| `ClientToScreen` | `004dd1ec` | Convert client to screen coords|
| `ScreenToClient` | `004dcf10` | Convert screen to client coords|

### 4.1 Cursor Visibility

The cursor is hidden during video playback (see [video-system.md](video-system.md)) and restored
when playback completes. During gameplay, the cursor uses a custom image loaded from
ICONS.LOD.

### 4.2 Cursor Clipping

The `ClipCursor` API constrains the mouse to the game window area. The async mouse
system calls this during initialization and when the window gains focus. Failure
produces: `"Could not clip cursor to screen!"`.

---

## 5. Mouse.cpp (High-Level Mouse System)

**Source:** `Mouse.cpp` at `004eada0`
**Function:** `FUN_0046a338`

The high-level mouse system (`Mouse.cpp`) sits above the DirectInput layer and
provides:

- Mouse-over detection for UI elements
- Click event dispatch to the UI button system
- Drag operations (inventory item drag)
- Right-click context menus (item inspection, NPC info)

Mouse events are translated into the engine's event queue system
(`DAT_0050ca50` count, `DAT_0050ca54` array).

---

## 6. Input Event Dispatch

The main event dispatcher at `FUN_00435737` (2,699 bytes) processes a command queue:

```text
Queue structure:
  DAT_0050ca50 = event count
  DAT_0050ca54 = event array (3 ints per entry: type, param1, param2)

```

Input events are enqueued from both keyboard and mouse handlers. The dispatcher
processes them sequentially, translating raw input into game actions:

- Movement keys -> party movement
- Action keys -> attack, cast, interact
- UI keys -> open inventory, spellbook, map
- Mouse clicks -> button activation, object interaction

### Key Message IDs

| ID    | Action                    |
|-------|---------------------------|
| 0x36  | New Game (title screen)   |
| 0x37  | Load Game (title screen)  |
| 0x38  | Credits (title screen)    |
| 0x39  | Exit Game (title screen)  |
| 0x71  | NPC dialogue option       |
| 0x88  | Spell/ability selection   |
| 0xA2  | Scroll up (save/load)     |
| 0xA3  | Scroll down (save/load)   |
| 0xA4  | Confirm (load/save)       |
| 0xA5  | Select slot (param=index) |
| 0xA6  | Cancel                    |
| 0xA7  | Delete save               |

---

## 7. Focus Management

When the game window loses focus (`WM_ACTIVATEAPP`), the input system:

1. Suspends the DirectInput keyboard device (unacquire)
2. Suspends the DirectInput mouse device (unacquire)
3. Sets focus flags in `DAT_006be1e5`

When focus is regained:

1. Resumes keyboard (re-acquire)
2. Resumes mouse (re-acquire)
3. Re-clips cursor to window
4. Clears accumulated input state

The WndProc at `FUN_00463828` handles `WM_ACTIVATEAPP` to trigger these transitions.

---

## Integration notes

- DirectInput is Windows-specific; SDL3 provides cross-platform equivalents
- The async polling model maps well to SDL's `SDL_GetKeyboardState()` and
  `SDL_GetMouseState()`
- Key bindings should be configurable (the original uses text-based binding names)
- Mouse delta vs. absolute position tracking must be consistent
- Cursor clipping to the window is handled by `SDL_SetWindowMouseGrab()` in SDL3
- The event queue (3 ints per entry) should be replaced with a typed event struct
- Focus gain/loss must properly suspend and resume input devices
- The original uses cooperative-level DirectInput (non-exclusive, foreground);
  SDL handles this automatically
