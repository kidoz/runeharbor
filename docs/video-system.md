---
title: "Video System"
summary: "The video system plays Smacker and Bink media stored in VID containers or standalone files."
doc_type: reference
status: partial
last_updated: 2026-08-01
---
# Video System

The video system plays Smacker and Bink media stored in VID containers or standalone files.

Coverage is limited to MM7 v1.21 and the artifacts cited in Scope.

## Scope

This page covers MM7 v1.21 behavior and the binary or data artifacts cited below.
Unmarked findings are **observed**; deductions and gaps are labeled **inferred**
or **unknown** per the [documentation standard](contributing/documentation-standard.md).
Artifacts and versions not cited on this page are out of scope.
All multi-byte file fields are little-endian unless stated otherwise. RuneHarbor-specific
decisions, when present, belong in Integration notes.

> Original source files:
>
> - `D:\mm7Src_eng\MM7\Code\Video.cpp` (string at 0x004f0f34)
> - `D:\mm7Src_eng\MM7\Code\Show.cpp` (string at 0x004efea4)

---

## 1. Overview

MM7 uses two RAD Game Tools video codecs, packaged inside VID container files:

| Codec | DLL | Magic | Usage |
|-------|-----|-------|-------|
| Smacker | `SMACKW32.DLL` | `SMK2` / `SMK4` | 3DO logo, NPC/location animations |
| Bink | `BINKW32.DLL` | `BIKf` | Cutscene videos |

Both codecs route their audio through the Miles Sound System via
`_SmackSoundUseMSS` and `_BinkSetSoundSystem(_BinkOpenMiles, ...)`.

---

## 2. VID Container Format

The VID format is a simple flat archive bundling multiple video files.

### Header

```cpp
Offset  Size    Field
------  ------  ----------------------------------
0x00    4       uint32_t entry_count (little-endian)
0x04    44*N    Entry table (N = entry_count)

```

### Entry Table (44 bytes per entry)

```cpp
Offset  Size    Field
------  ------  ----------------------------------
0x00    40      char filename[40] (null-terminated ASCII)
0x28    4       uint32_t data_offset (absolute byte offset in VID file)

```

**Key properties:**

- No per-entry size field; size is computed as `next_entry.offset - this_entry.offset`
  (or `file_size - last_entry.offset` for the final entry)
- Entries are stored in alphabetical order
- Data begins immediately after the entry table: `4 + entry_count * 44` bytes
- Filenames include the extension (`.smk` or `.bik`), indicating the embedded codec
- Filenames may contain spaces (e.g., `"Arbiter Evil.bik"`)

### Verification

| File | Size (bytes) | Entries | Header Size | First Data Offset |
|------|-------------|---------|-------------|-------------------|
| `Magic7.vid` | 211,669,708 | 14 | 620 bytes | 0x0000026C (620) |
| `Might7.vid` | 112,600,244 | 162 | 7,132 bytes | 0x00001BDC (7132) |

In both cases, `first_data_offset == 4 + entry_count * 44`, confirming no
padding or extra header data between the entry table and the first video.

### VID file paths (from strings)

- `anims\magic7.vid` (0x004f0ea4) -- Cutscene videos
- `anims\might7.vid` (0x004f0eec) -- NPC/location animations
- Also: `X:\anims\magic7.vid` (0x004e9a6c) -- CD-ROM path variant

---

## 3. File Inventory

### Magic7.vid (14 entries: 1 SMK, 13 BIK)

| # | Name | Format | Resolution | Frames | FPS | Duration | Size (MB) |
|---|------|--------|-----------|--------|-----|----------|-----------|
| 0 | 3DOLOGO.SMK | SMK2 | 640x480 | 81 | 15.0 | 5.4s | 0.88 |
| 1 | Arbiter Evil.bik | BIKf | 320x240 | 736 | 15.0 | 49.1s | 12.05 |
| 2 | Arbiter Good.bik | BIKf | 320x240 | 623 | 15.0 | 41.5s | 9.05 |
| 3 | Endgame 1 Good.bik | BIKf | 320x240 | 3766 | 30.0 | 125.5s | 29.97 |
| 4 | Endgame 2 Evil.bik | BIKf | 320x240 | 961 | 15.0 | 64.1s | 15.56 |
| 5 | Family Reunion.bik | BIKf | 320x240 | 918 | 15.0 | 61.2s | 15.32 |
| 6 | Intro Post.bik | BIKf | 320x240 | 1361 | 15.0 | 90.7s | 21.99 |
| 7 | Intro.bik | BIKf | 320x240 | 2640 | 15.0 | 176.0s | 43.13 |
| 8 | JVC.bik | BIKf | 320x240 | 90 | 10.0 | 9.0s | 2.68 |
| 9 | LoseGame.bik | BIKf | 320x240 | 174 | 12.0 | 14.5s | 3.74 |
| 10 | MM3 People Evil.bik | BIKf | 320x240 | 1008 | 15.0 | 67.2s | 16.53 |
| 11 | MM3 People Good.bik | BIKf | 320x240 | 1347 | 15.0 | 89.8s | 21.24 |
| 12 | PCOut01.bik | BIKf | 320x240 | 530 | 12.0 | 44.2s | 6.08 |
| 13 | New World Logo.bik | BIKf | 320x240 | 195 | 15.0 | 13.0s | 3.66 |

### Might7.vid (162 entries: all SMK2)

All 162 entries are SMK2 format at 460x344 pixels. These are NPC animation
loops for guilds, shops, houses, and outdoor locations. Frame rates vary:
12, 13, 15, or 25 fps. Of 162 entries, 141 have the ring/loop flag set.

Examples:

| Name | Frames | FPS | Audio | Loop |
|------|--------|-----|-------|------|
| Air Guild.smk | 37 | 12.0 | Yes | Yes |
| Dark Guild.smk | 170 | 15.0 | Yes | Yes |
| Earth Guild.smk | 150 | 12.0 | Yes | Yes |
| Boat01.smk | 40 | 13.0 | Yes | Yes |
| Elf Alchemist.smk | 8 | 15.0 | No | No |
| Wizard Weapon Smith.smk | 37 | 12.0 | Yes | Yes |

---

## 4. Smacker Header Format (SMK2/SMK4)

### Header Structure (104 bytes)

```cpp
Offset  Size    Field                   Notes
------  ------  ----------------------  --------------------------------
0x00    4       char magic[4]           "SMK2" or "SMK4"
0x04    4       uint32_t width          Pixels
0x08    4       uint32_t height         Pixels
0x0C    4       uint32_t frameCount
0x10    4       int32_t  frameRate      Signed, special encoding (see below)
0x14    4       uint32_t flags          Bit 0 = ring/loop frame present
0x18    28      uint32_t audioSize[7]   Max audio packet size per track (7 tracks)
0x34    4       uint32_t treesSize      Huffman trees data size (bytes)
0x38    4       uint32_t mmapSize       MMap tree allocation hint
0x3C    4       uint32_t mclrSize       MClr tree allocation hint
0x40    4       uint32_t fullSize       Full tree allocation hint
0x44    4       uint32_t typeSize       Type tree allocation hint
0x48    28      uint32_t audioRate[7]   Audio config per track (bitfield)
0x64    4       uint32_t dummy          Always 0

```

Total header size: **104 bytes** (0x68).

### Frame Rate Encoding

The `frameRate` field uses signed encoding with three ranges:

```text
if (frameRate > 0):           fps = 1000.0 / frameRate          (milliseconds)
if (-10000 <= frameRate < 0): fps = 100000.0 / (-frameRate)     (10-microseconds)
if (frameRate < -10000):      fps = 1000000.0 / (-frameRate)    (microseconds)

```

Observed values in MM7:

| Raw Value | Computed FPS | Used By |
|-----------|-------------|---------|
| -6666 | 15.00 fps | 3DOLOGO.SMK |
| -8333 | 12.00 fps | Most Might7.vid entries |
| -7692 | 13.00 fps | Boat01.smk |
| -4000 | 25.00 fps | Dwarven Medium House 2.smk |

### Audio Rate Field Encoding (per track)

Each `audioRate[i]` is a bitfield:

```text
Bit 31:    Compressed (DPCM/Bink Audio)
Bit 30:    Bink Audio codec (vs. DPCM)
Bit 29:    Stereo
Bit 28:    16-bit samples
Bits 0-23: Sample rate in Hz

```

### Data Layout After Header

```text
Offset              Size                Content
------------------  ------------------  ---------------------------
104                 frameCount * 4      Frame sizes (bit 0 = keyframe flag)
104 + N*4           frameCount * 1      Frame type bytes
104 + N*5           treesSize           Huffman trees data
104 + N*5 + trees   (to end)            Frame data (sequential)

```

### Audio Track Usage Pattern (IMPORTANT)

- **Magic7.vid** (3DOLOGO.SMK): Audio on **track 0**
- **Might7.vid** (all 162 entries): Audio on **track 1** exclusively
  - 148 of 162 files have audio; 14 are silent
  - All audio: 22050 Hz, stereo, Bink Audio codec

This split is critical: a player that always reads track 0 will produce
correct audio for 3DOLOGO.SMK but silence for all Might7.vid videos.

### Sample: 3DOLOGO.SMK Header

```text
Magic:       SMK2
Resolution:  640x480
Frames:      81
Frame Rate:  15.00 fps (-6666 raw)
Flags:       0x00000000 (no loop)
Trees Size:  12,803 bytes
Audio:       Track 0: 22050 Hz, stereo, 16-bit, Bink Audio compression
             Max audio packet: 94,080 bytes

```

### Sample: Air Guild.smk Header (Might7.vid)

```text
Magic:       SMK2
Resolution:  460x344
Frames:      37
Frame Rate:  12.00 fps (-8333 raw)
Flags:       0x00000001 (ring/loop)
Trees Size:  32,986 bytes
Audio:       Track 1: 22050 Hz, stereo, 8-bit, Bink Audio compression
             Max audio packet: 47,776 bytes

```

---

## 5. Bink Header Format (BIKf)

### Base Header Structure (44 bytes)

```cpp
Offset  Size    Field                   Notes
------  ------  ----------------------  --------------------------------
0x00    4       char magic[4]           "BIKf" (all MM7 Bink videos)
0x04    4       uint32_t fileSize       Total file size minus 8
0x08    4       uint32_t frameCount
0x0C    4       uint32_t maxFrameSize   Largest single frame in bytes
0x10    4       uint32_t frameCount2    Duplicate of frameCount
0x14    4       uint32_t width          Pixels
0x18    4       uint32_t height         Pixels
0x1C    4       uint32_t fpsDividend    FPS = dividend / divider
0x20    4       uint32_t fpsDivider
0x24    4       uint32_t videoFlags     0x00000000 for all MM7 videos
0x28    4       uint32_t audioTrackCount

```

### Audio Track Info (immediately after base header, per track)

The audio metadata consists of **three separate arrays**, each with one
entry per audio track:

```cpp
Offset      Size            Field
----------  --------------  ----------------------------------
44          tracks * 4      uint32_t maxPacketSize[tracks]
44 + T*4    tracks * 4      uint32_t audioRateRaw[tracks]
44 + T*8    tracks * 4      uint32_t trackID[tracks]

```

Total audio info size: `audioTrackCount * 12` bytes.

**Important:** This is 12 bytes per track, not 8. The trackID array is
sometimes overlooked, causing the frame index table to be read at the
wrong offset.

### Audio Rate Raw Field Encoding

```text
Bits 0-23:  Sample rate in Hz
Bit 29:     Stereo flag
Bit 30:     DCT flag (vs RDFT)
Bit 31:     Unknown / has-audio flag

```

Observed values:

| Raw Value | Interpretation |
|-----------|---------------|
| 0xE0005622 | 22050 Hz, stereo, DCT (most videos) |
| 0x80005622 | 22050 Hz, mono, no DCT (JVC.bik only) |

### Frame Index Table

Immediately after all audio info:

```text
Offset                  Size                    Content
----------------------  ----------------------  ------------------
44 + tracks*12          (frameCount + 1) * 4    Frame offsets

```

Each entry is a `uint32_t` where:

- **Bit 0:** Keyframe flag (1 = keyframe)
- **Bits 1-31:** Byte offset (shifted left 1) from start of Bink frame data

The extra (N+1)th entry marks the end of the last frame.

### File Size Validation

For all 13 Bink entries in Magic7.vid, the internal `fileSize + 8` matches
the VID container-calculated size exactly.

### Sample: Intro.bik Header

```text
Magic:          BIKf
File Size:      45,226,896 bytes (internal: 45,226,888 + 8)
Resolution:     320x240
Frames:         2,640
FPS:            15.00 (15/1)
Duration:       176.0 seconds
Max Frame:      79,324 bytes
Video Flags:    0x00000000
Audio Tracks:   1
  Track 0:      22050 Hz, stereo, DCT compression
                Max packet: 69,120 bytes
Frame Index:    Starts at local offset 56 (0x38)
                First frame data at offset 10,620 (0x297C)

```

---

## 6. Smacker DLL Interface

The engine loads `SMACKW32.DLL` (string at 0x004dda7a) and imports:

| Import | Purpose |
|--------|---------|
| `_SmackOpen(file, flags, extraBuf)` | Open SMK file; flags 0x7140 include audio config |
| `_SmackSoundUseMSS(driver)` | Route Smacker audio through Miles |
| `_SmackDoFrame(handle)` | Decode current frame |
| `_SmackNextFrame(handle)` | Advance to next frame |
| `_SmackToBuffer(handle, x, y, pitch, h, w, f)` | Blit decoded frame to buffer |
| `_SmackToBufferRect(handle, area)` | Blit partial rectangle |
| `_SmackWait(handle)` | Wait for frame timing |
| `_SmackGoto(handle, frame)` | Seek to specific frame |
| `_SmackClose(handle)` | Close and free resources |
| `_SmackBlitOpen(flags)` | Open blit context |
| `_SmackBlit(ctx, dst, dPitch, x, y, src, sPitch, w, h, t, f)` | Palette blit |
| `_SmackBlitSetPalette(ctx, pal, transparency)` | Set blit palette |
| `_SmackBlitClear(ctx, ...)` | Clear blit area |
| `_SmackBlitClose(ctx)` | Close blit context |
| `_SmackBufferOpen(wnd, flags, w, h, zoom, rate)` | Open display buffer |
| `_SmackBufferNewPalette(buf, pal, flags)` | Update buffer palette |
| `_SmackBufferClose(buf)` | Close display buffer |
| `_SmackColorRemapWithTrans(remap, pal, newPal, trans, flags)` | Color remap |
| `_SmackVolumePan(handle, track, vol, pan)` | Set volume/pan |
| `_SmackSoundOnOff(handle, onOff)` | Toggle audio |
| `_SmackUseMMX(flag)` | Enable/disable MMX optimization |

---

## 7. Bink DLL Interface

The engine loads `BINKW32.DLL` (string at 0x004ddbe8) and imports:

| Import | Purpose |
|--------|---------|
| `_BinkOpen(file, flags)` | Open BIK file; flags 0x08800000 |
| `_BinkSetSoundSystem(openFunc, driver)` | Route Bink audio through Miles |
| `_BinkOpenMiles(driver)` | Miles audio callback for Bink |
| `_BinkDoFrame(handle)` | Decode current frame |
| `_BinkNextFrame(handle)` | Advance to next frame |
| `_BinkCopyToBuffer(handle, buf, pitch, h, x, y, flags)` | Copy frame to surface |
| `_BinkDDSurfaceType(surface)` | Query DirectDraw surface pixel format |
| `_BinkBufferSetOffset(buf, x, y)` | Set display offset |
| `_BinkBufferSetScale(buf, w, h)` | Set display scale |
| `_BinkGetRects(handle, rects)` | Get dirty rectangle list |
| `_BinkWait(handle)` | Wait for frame timing |
| `_BinkGoto(handle, frame, flags)` | Seek to specific frame |
| `_BinkPause(handle, pause)` | Pause/resume playback |
| `_BinkClose(handle)` | Close and free resources |

---

## 8. Video Player Logic

### Main player function

**FUN_004be671** at 0x004be671 (588 bytes) -- the main video playback entry point.

1. Hide cursor
2. Check format selector `DAT_00f8ba24`:
   - Value 2: Use Bink player path
   - Value 1: Use Smacker player path
3. Frame loop:
   - Process Windows messages (`PeekMessage`/`TranslateMessage`/`DispatchMessage`)
   - Call `FUN_00435737` (game event handler -- allows skip on keypress)
   - Wait for next frame (`_BinkWait` or `_SmackWait`)
   - Decode frame (`_BinkDoFrame`/`_SmackDoFrame`)
   - Blit frame to display surface
   - Advance frame (`_BinkNextFrame`/`_SmackNextFrame`)
   - Check for end-of-video or skip flag
4. Restore cursor on completion

### Movie playback dispatcher

**FUN_004a94bd** (CShow::Run) at 0x004a94bd (241 bytes):

- Error: `"Invalid movie requested in CShow::Run()"` (0x004efe28)
- Error: `"No movie"` (0x004efe98)

### VID archive opening

**FUN_004be93b** -- Opens VID archives and validates:

- Opens `anims\magic7.vid` (0x004f0ea4)
- Opens `anims\might7.vid` (0x004f0eec)
- Error: `"Video File Error"` (0x004f0eb8)
- Error: `"Smacker Error"` (0x004f0e8c)
- Error: `"Unsupported Bink playback!"` (0x004f0f18)

### Format detection

The format is determined by reading the first 4 bytes of the video data at
the entry's offset in the VID container:

- `"SMK2"` or `"SMK4"` -- Smacker format
- `"BIKf"` -- Bink format

---

## 9. Video Globals

| Address | Type | Purpose |
|---------|------|---------|
| `DAT_00f8b9b0` | HANDLE | Smacker video handle |
| `DAT_00f8ba08` | HANDLE | Bink video handle |
| `DAT_00f8b9f4` | i32 | Skip/abort flag (set on keypress) |
| `DAT_00f8b9e0` | i32 | Currently playing flag |
| `DAT_00f8ba24` | i32 | Format selector (1=Smacker, 2=Bink) |
| `DAT_00f8b9f8` | void* | Video display surface/context |
| `DAT_00f8b9cc` | i32 | Video mode parameter |

---

## 10. Playlist and Sequencing

### Startup sequence

During application startup (controlled by engine flags):

1. If `nointro` flag (bit 0x04) is NOT set:
   - Play `3DOLOGO.SMK` (from Magic7.vid)
   - Play `JVC.bik` (from Magic7.vid)
   - Play `New World Logo.bik` (from Magic7.vid)
2. If `nologo` flag (bit 0x08) is NOT set:
   - Play `Intro.bik` (from Magic7.vid)

### In-game video playback

NPC/location videos from Might7.vid are played when entering shops,
guilds, and other locations. These use the 460x344 viewport area,
which fits within the game's 640x480 screen with UI chrome surrounding.

### Cutscene triggers

Story cutscenes from Magic7.vid are triggered by game events:

- `Intro.bik` / `Intro Post.bik` -- Game introduction
- `Endgame 1 Good.bik` / `Endgame 2 Evil.bik` -- Ending sequences
- `Arbiter Good.bik` / `Arbiter Evil.bik` -- Arbiter scenes
- `Family Reunion.bik` -- Story event
- `LoseGame.bik` -- Game over
- `PCOut01.bik` -- Emerald Island exit

---

## 11. Known Bugs and Edge Cases

### Audio track mismatch

Might7.vid Smacker files store audio exclusively on **track 1**, while
3DOLOGO.SMK in Magic7.vid uses **track 0**. A video player that hardcodes
track 0 will play 3DOLOGO.SMK audio correctly but produce silence for all
148 audio-bearing Might7.vid entries.

**Fix:** Scan tracks 0-6 for the first active audio track, or use the
`audioSize[i]` array in the Smacker header to determine which tracks
carry data (non-zero `audioSize` indicates an active track).

### Bink frame index offset

The Bink audio info block uses 12 bytes per track (maxPacketSize + rateRaw +
trackID), not 8. Reading only 8 bytes per track causes the frame index table
to be parsed 4 bytes too early. For MM7 specifically, the trackID happens to
be 0x00000000, so:

- Frame 0 gets offset 0, pointing to the Bink header (garbage)
- All subsequent frames shift by one position

### Resolution patterns

- Cutscene Bink videos: all 320x240 (upscaled to 640x480 for display)
- 3DOLOGO.SMK: 640x480 (native resolution)
- NPC/location Smacker videos: all 460x344 (displayed in viewport area)

### Codec versions

- All Smacker files: SMK2 (version 2). No SMK4 files present.
- All Bink files: BIKf (version f). No other Bink versions present.

---

## 12. Key Functions

| Address | Size | Proposed Name | Purpose |
|---------|------|---------------|---------|
| 0x004a94bd | 241 | `CShow::Run` | Movie playback dispatcher |
| 0x004be671 | 588 | `Video::Play` | Main video player loop |
| 0x004be8bd | - | `Video::SmackerError` | Smacker error handler |
| 0x004be93b | - | `Video::OpenVIDArchives` | Open Magic7.vid and Might7.vid |
| 0x004becb2 | - | `Video::BinkDecodeFrame` | Decode single Bink frame |
| 0x004bedce | - | `Video::SmackerDecodeFrame` | Decode single Smacker frame |
| 0x004bf1f5 | - | `Video::BinkPlayback` | Bink playback handler |
| 0x004bf377 | - | `Video::SmackerPlayback` | Smacker playback handler |
| 0x004bf886 | - | `Video::LoadEndgameFont` | Load endgame.fnt for credits |
| 0x004bfd95 | - | `Video::Util` | Video utility function |

---

## Integration notes

### Decoder strategy

Rather than reimplementing the proprietary Smacker and Bink codecs, use
FFmpeg/libavcodec which has open-source implementations of both formats:

- Smacker: `AV_CODEC_ID_SMACKVIDEO` / `AV_CODEC_ID_SMACKAUDIO`
- Bink: `AV_CODEC_ID_BINKVIDEO` / `AV_CODEC_ID_BINKAUDIO`

### VID container

The VID container is trivial to parse (4-byte count + 44-byte entries).
Extract video data by offset/size and feed to the decoder.

### Audio track scanning

When opening a Smacker file, scan all 7 `audioSize` entries in the header.
The first non-zero entry indicates the active audio track. Do not assume
track 0.

### Display scaling

- Bink cutscenes (320x240) should be scaled 2x to 640x480
- Smacker NPC videos (460x344) display in the viewport area
- 3DOLOGO.SMK (640x480) displays at native resolution

*All trademarks belong to their respective owners.*
