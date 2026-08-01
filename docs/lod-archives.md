---
title: "LOD Archive System"
summary: "LOD archives provide the container format for assets, maps, events, and save data."
doc_type: reference
status: partial
last_updated: 2026-08-01
---
# LOD Archive System

LOD archives provide the container format for assets, maps, events, and save data.

Coverage is limited to MM7 v1.21 and the artifacts cited in Scope.

## Scope

This page covers MM7 v1.21 behavior and the binary or data artifacts cited below.
Unmarked findings are **observed**; deductions and gaps are labeled **inferred**
or **unknown** per the [documentation standard](contributing/documentation-standard.md).
Artifacts and versions not cited on this page are out of scope.
All multi-byte file fields are little-endian unless stated otherwise. RuneHarbor-specific
decisions, when present, belong in Integration notes.

---

## Overview

LOD (Library Of Data) is the primary archive container format used by the engine. All
game assets -- textures, sprites, icons, sounds, maps, events, and save games -- are
stored in LOD archives. The format supports hierarchical chapters, per-file zlib
compression, and a flat directory for O(1) lookup by name.

**Source file reference:** `LOD CArray`, `LOD Index`, `LODio`, `LODsub` (allocation
debug names at `004e970c`-`004e9748`)

---

## 1. LOD File Header (256 bytes)

All LOD archives begin with a 256-byte (0x100) header, but the internal layout varies
by archive type. Three distinct header variants exist in the codebase:

### 1a. Game LOD Header (GAMES.LOD, save###.mm7)

The most detailed variant, used for map archives and save files:

```text
Offset  Size   Field               Description
------  -----  ------------------  -------------------------------------------------
0x000   4      magic               "LOD\0" (4 bytes)
0x004   4      gameId              "MMVII" or similar game identifier
0x008   80     description         Human-readable archive description string
                                   Examples: "newmaps for MMVII", "MMVII"
0x058   80     chapterName         Default chapter name (e.g., "chapter")
0x0A8   4      fileSize            Total archive file size (optional, may be 0)
0x0AC   4      dataStart           Byte offset where file data section begins
0x0B0   4      numDirEntries       Total number of top-level directory entries
0x0B4   76     reserved            Padding / reserved fields (zeroed)

```

The `dataStart` and `numDirEntries` fields are critical for directory parsing.

### 1b. Image/Sprite LOD Header (BITMAPS.LOD, ICONS.LOD, SPRITES.LOD)

A simpler variant used for asset archives:

```text
Offset  Size   Field               Description
------  -----  ------------------  -------------------------------------------------
0x000   4      magic               "LOD\0" (4 bytes)
0x004   4      gameId              "MMVI" (even for MM7 — legacy compatibility)
0x008   248    unknown             Reserved / format-specific metadata

```

These archives derive directory entry count and data offsets from the file structure
rather than from explicit header fields.

### 1c. Generic LOD Header (Events.lod)

The base text-file archive variant:

```text
Offset  Size   Field               Description
------  -----  ------------------  -------------------------------------------------
0x000   4      magic               "LOD\0" (4 bytes)
0x004   4      gameId              Game identifier string
0x008   248    reserved            Reserved space

```

**All variants: 0x100 = 256 bytes total.**

The original binary also uses a signature value `0x16741` + ASCII tag `"mvii"` in some
code paths (verified at `0x0045FB1D`). The `"LOD\0"` magic and game ID are the primary
validation fields.

### Known Archives

| Filename            | Chapter Name | Contents                          |
|---------------------|--------------|-----------------------------------|
| `data\bitmaps.lod`  | `bitmaps`    | Textures, PCX backgrounds, palettes |
| `data\icons.lod`    | `icons`      | UI icons, button graphics, fonts  |
| `data\sprites.lod`  | (sprites)    | Sprite frame images (8-direction) |
| `data\spriteLO.lod` | (sprites)    | Low-resolution sprite variant     |
| `data\events.lod`   | (events)     | Event scripts, binary data tables |
| `data\games.lod`    | (save)       | Save game template data           |
| `data\new.lod`      | (new game)   | New game initial state template   |
| `saves\save###.mm7` | (save)       | Per-slot save archives (LOD format) |

---

## 2. Directory Entry (32 bytes)

Immediately following the header is an array of `numDirectoryEntries` entries, each 32
bytes (0x20):

```text
Offset  Size   Field               Description
------  -----  ------------------  -------------------------------------------------
0x00    16     name[16]            Null-terminated filename (case-insensitive lookup)
0x10    4      dataOffset          Byte offset to file data (relative to data start)
0x14    4      dataSize            Compressed data size in bytes
0x18    4      decompressedSize    Uncompressed size (0 = data is not compressed)
0x1C    2      numSubItems         Sub-item count (used as chapter page count)
0x1E    2      flags               Padding / flags

```

**Total: 0x20 = 32 bytes per entry**

The directory supports up to **300 chapter pages** (`"LODchapterPages exceed 300"`
error at `004e9778`).

---

## 3. Chapter Navigation

LOD archives use a two-level hierarchy:

1. **Top-level directory**: Contains chapter entries (e.g., "bitmaps", "icons")
2. **Chapter sub-directory**: Contains file entries within that chapter

### SetChapter Operation

When the engine opens a LOD, it calls `SetChapter(name)` to navigate to a named
chapter:

1. Scan top-level directory entries for a name match (case-insensitive)
2. On match, record the chapter's data offset and sub-entry count
3. Read the chapter's own sub-directory (also 32-byte entries)
4. File lookups within the chapter use this sub-directory

### File Lookup

Two lookup modes exist:

- **Sorted index** (`mode 0`): Uses a pre-sorted binary search index, storing the
  result in a global last-lookup cache
- **Linear scan** (`mode 1`): Iterates entries sequentially, comparing names via
  case-insensitive string compare

On match, the engine seeks to `entry.dataOffset + chapterDataOffset` and reads.

---

## 4. File Data Extraction

Each file within a LOD chapter has a 48-byte (0x30) internal file header followed by
the payload and (for image entries) an embedded palette:

```text
[48-byte FileHeader][compressed/raw pixels (dataSize bytes)][palette (768 bytes)]

```

The palette is present for 8-bit paletted image entries (ICONS.LOD, SPRITES.LOD,
BITMAPS.LOD textures). It is stored as 256 × 3 bytes (R, G, B) immediately after
the compressed pixel data. The total entry size relationship is:

```text
entry.size = sizeof(ImageFileHeader) + dataSize + 768

```

**Important:** The original engine (Ghidra FUN_00410897) reads exactly `dataSize`
bytes of pixel data, NOT `entry.size - 48`. The trailing 768 bytes are the palette.

### ImageFileHeader (48 bytes)

For texture/image entries in external-only archives (ICONS.LOD, SPRITES.LOD):

```text
Offset  Size   Field               Description
------  -----  ------------------  -------------------------------------------------
0x00    16     name[16]            Texture name (null-terminated)
0x10    4      size                Total entry size (header + pixels + palette)
0x14    4      dataSize            Compressed pixel data size (excludes palette)
0x18    2      width               Image width in pixels
0x1A    2      height              Image height in pixels
0x1C    2      widthLn2            Log2 of width (power-of-two dimension)
0x1E    2      heightLn2           Log2 of height (power-of-two dimension)
0x20    2      widthMinus1         Width - 1 (used for bit masking)
0x22    2      heightMinus1        Height - 1 (used for bit masking)
0x24    2      paletteId           Primary palette index (i16)
0x26    2      paletteId2          Secondary palette index (i16)
0x28    4      decompressedSize    Uncompressed size (0 = not compressed)
0x2C    4      flags               Image flags / attributes

```

**Correction (v2):** Offsets 0x20-0x27 were previously documented as two u32 fields
(`paletteId1`, `paletteId2`). Actual layout is four i16 fields: `widthMinus1`,
`heightMinus1`, `paletteId`, `paletteId2`. Confirmed via OpenEnroth cross-reference
and runtime pixel data verification.

### Decompression Logic

1. Read the 48-byte header
2. If `decompressedSize == 0`: data is stored raw; read `dataSize` bytes directly
3. If `decompressedSize != 0`: read `dataSize` bytes, then zlib inflate to
   `decompressedSize` bytes
4. Read the trailing 768 bytes as the embedded RGB palette

The engine uses **zlib 1.1.3** (embedded in the binary) for inflate/deflate.

---

## 5. Archive Format Variants

### Mixed Format (BITMAPS.LOD)

BITMAPS.LOD uses a "mixed" format where some entries contain a UTF-16 `"LIB."` marker
indicating internal library data alongside normal external entries. The delta
calculation must account for the first entry's offset field.

### External-Only Format (ICONS.LOD, SPRITES.LOD)

These archives store all file data externally (after the directory). The offset delta
equals the first entry's `dataOffset` value:

```text
delta = entries[0].offset

```

File names span the full 16 bytes in external-only archives.

### Game Save Format (GAMES.LOD, save###.mm7)

Save archives are full LOD containers that store serialized game state. They are
created by copying `data\new.lod` as a template and then writing/replacing internal
files. The LOD write function uses a temporary file (`lod.tmp` / `lodapp.tmp`) during
modifications.

---

## 6. LOD Open Wrappers

The engine provides specialized wrappers for each archive:

| Function     | Archive Path          | Chapter    | Notes                     |
|--------------|-----------------------|------------|---------------------------|
| `0x0040fa3a` | `data\bitmaps.lod`    | "bitmaps"  | Textures, backgrounds     |
| `0x0040fafa` | `data\icons.lod`      | "icons"    | UI icons, fonts           |
| `0x004ac6f8` | `data\sprites.lod`    | (sprites)  | Or `data\spriteLO.lod`    |
| `0x00460869` | `data\games.lod`      | (save)     | Save game data            |
| (inline)     | `data\events.lod`     | (events)   | Event scripts, tables     |
| (inline)     | `data\new.lod`        | (new)      | New game template         |

---

## 7. LOD Class Memory Layout

```text
Offset  Size   Field                 Description
------  -----  --------------------  -------------------------------------------
0x000   4      fileHandle            File handle (from fopen)
0x004   256    filename[256]         Path to LOD file
0x104   4      isOpen                Non-zero if file is open
0x110   256    headerData            LOD header buffer (256 bytes)
0x1BC   4      numChapterEntries     Chapter sub-directory entry count
0x210   4      chapterEntries        Pointer to chapter entry array
0x214   256    currentChapterName    Name of currently active chapter
0x224   4      currentChapterIndex   Index of current chapter
0x228   4      currentChapterStart   Start index within directory
0x22C   4      numFilesInChapter     Number of files in active chapter
0x230   4      fileIndex             Pointer to file index array
0x234   4      chapterDataOffset     Base offset for chapter data
0x420   4      ioBuffer              I/O buffer pointer ("LODio")
0x424   4      ioBufferSize          I/O buffer size
0x8C0   4      subIndexBuffer        Sub-index buffer ("LODsub")

```

---

## 8. Error Messages

| Error String                                | Condition                      |
|---------------------------------------------|--------------------------------|
| `"Can't load file!"`                        | Invalid LOD signature          |
| `"LODchapterPages exceed 300"`              | Too many pages in chapter      |
| `"Attempt to reset a LOD IObuffer!"`        | Double-init of I/O buffer      |
| `"Attempt to reset a LOD subindex!"`        | Double-init of sub-index       |
| `"Unable to find %s in Games.LOD"`          | Map file not found in archive  |

---

## Integration notes

- Names are compared case-insensitively throughout the LOD system
- The offset delta for external-only archives is simply `entries[0].offset`
- Palette index 0 in LOD textures is the transparent color (alpha = 0)
- LOD write operations use a temp file + rename pattern for atomicity
- The maximum chapter page limit of 300 should be treated as a guideline
- zlib decompression uses the standard inflate API (no custom dictionary)
- **Embedded palette extraction**: Each image LOD entry contains a 768-byte RGB
  palette stored after the compressed pixel data. When extracting, read exactly
  `dataSize` bytes for zlib decompression, then read the next 768 bytes as the
  palette. Do NOT feed `entry.size - 48` bytes to zlib — the trailing palette
  bytes would be silently ignored by zlib but the palette data would be lost.
- **Header Creation Verification**: The header creation logic was verified at `0x0045FB1D` in MM7-Rel.exe.
  The signature and tag are manually constructed on the stack:

  ```assembly
  mov dword [ebp - 0x58], 0x16741  ; Signature (91,969)
  mov byte [ebp - 0x54], 0x6d      ; 'm'
  mov byte [ebp - 0x53], 0x76      ; 'v'
  mov byte [ebp - 0x52], 0x69      ; 'i'
  mov byte [ebp - 0x51], 0x69      ; 'i'

```text
- **Palette priority**: Use the embedded per-entry palette first. Fall back to
  `PAL###` entries from BITMAPS.LOD only if no embedded palette is present.
  `paletteId == 0` means "default palette" (use PAL001 as last resort).
