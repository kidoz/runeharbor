---
title: "Audio System"
summary: "MM7 routes digital, positional, and CD audio through the Miles Sound System."
doc_type: reference
status: partial
last_updated: 2026-08-01
---
# Audio System

MM7 routes digital, positional, and CD audio through the Miles Sound System.

Coverage is limited to MM7 v1.21 and the artifacts cited in Scope.

## Scope

This page covers MM7 v1.21 behavior and the binary or data artifacts cited below.
Unmarked findings are **observed**; deductions and gaps are labeled **inferred**
or **unknown** per the [documentation standard](contributing/documentation-standard.md).
Artifacts and versions not cited on this page are out of scope.
RuneHarbor-specific decisions, when present, belong in Integration notes.

> Original source file: `D:\mm7Src_eng\MM7\Code\Sound.cpp` (string at 0x004eff88)

---

## 1. Overview

All audio in MM7 is routed through **Miles Sound System** (MSS), accessed via
`AUDIO.DLL` which in turn loads `MSS32.DLL`. The engine supports three audio
subsystems:

| Subsystem | API Layer | Purpose |
|-----------|-----------|---------|
| 2D digital audio | `AIL_*_sample_*` | UI sounds, ambient, music stingers |
| 3D positional audio | `AIL_*_3D_sample_*` | Spatialized world sounds |
| CD audio (Redbook) | `AIL_redbook_*` | Background music from game disc |

The audio system also handles IMA ADPCM decompression for compressed WAV data
stored in LOD archives.

---

## 2. Initialization Sequence

Audio initialization occurs during the main startup (WinMain-equivalent at
0x00464e54). The sequence is:

1. **`_AIL_startup`** -- Initialize the Miles Sound System runtime
2. **`_AIL_set_preference`** -- Configure system-level preferences
3. **`_AIL_digital_configuration`** -- Query digital audio hardware
4. **`_AIL_waveOutOpen`** -- Open a digital audio output device
   - Parameters: driver handle, frequency, bits, channels, flags
5. **`_AIL_redbook_open_drive`** -- Open the CD-ROM drive for Redbook audio
   - Uses MCI command: `"open %c: type cdaudio alias CD"` (string at 0x004e9a4c)
6. **`_AIL_enumerate_3D_providers`** -- List available 3D audio providers
   - Checks for Aureal A3D via registry
7. **`_AIL_open_3D_provider`** -- Open selected 3D provider (if available)

### Shutdown

On exit, the engine calls in reverse order:

1. **`_AIL_close_3D_provider`**
2. **`_AIL_redbook_close`**
3. **`_AIL_waveOutClose`**
4. **`_AIL_shutdown`**

---

## 3. Sound Definition Table (dsounds.bin)

Sound definitions are loaded from `dsounds.bin` (binary) or `sounds.def` (text)
in the events LOD archive.

- **Binary loader:** `FUN_004a9e19` (referenced by string `"dsounds.bin"` at 0x004e9c38)
- **Text loader:** `FUN_004a9e93` (referenced by string `"data\sounds.def"` at 0x004e9ce8)
- **Serializer:** `FUN_004a9dcd` (referenced by `"data\dsounds.bin"` at 0x004f0004,
  `"Unable to save dsounds.bin!"` at 0x004effe8)

The sound table class is `SoundListClass`:

- Error: `"SoundListClass::load - Out of Memory!"` (0x004f0040)
- Error: `"SoundListClass::load - Unable to open file: %s."` (0x004f0068)

Each sound definition entry maps a logical sound name to:

- A WAV filename within the LOD or `Sounds\Audio.snd` archive
- Playback parameters (volume, loop count, 3D distance, flags)
- A `soundflag` field (string at 0x004e4730) and `WalkSound` type (0x004e470c)
- A `musicflag` field (string at 0x004e4724) for music-category sounds

---

## 4. 2D Digital Audio (Samples)

Standard 2D audio uses Miles "sample" handles for non-positional sounds such as
UI clicks, spell effects, and ambient layers.

### Lifecycle

```cpp
AIL_allocate_sample_handle(driver)   --> sample handle
AIL_init_sample(handle)              --> reset state
AIL_set_sample_file(handle, data, 0) --> assign WAV data
AIL_set_sample_volume(handle, vol)   --> set volume (0-127)
AIL_set_sample_pan(handle, pan)      --> set stereo pan (0=left, 64=center, 127=right)
AIL_set_sample_loop_count(handle, n) --> loop count (0=infinite, 1=once)
AIL_start_sample(handle)             --> begin playback
...
AIL_sample_status(handle)            --> poll status (4 = playing)
AIL_end_sample(handle)               --> stop playback
AIL_release_sample_handle(handle)    --> return to pool

```

### Additional sample operations

- **`AIL_sample_volume`** -- Query current volume
- **`AIL_sample_ms_position`** -- Query playback position in milliseconds
- **`AIL_set_sample_playback_rate`** -- Adjust playback rate (pitch shift)
- **`AIL_set_digital_master_volume`** -- Global digital volume

### Channel allocation

The number of simultaneous mixer channels is configured via the INI setting
`[settings] mixerchannels` (string at 0x004ea2fc), with a maximum of 16 channels.
This value is stored at `DAT_00f79340`.

---

## 5. 3D Positional Audio

When a compatible 3D audio provider is available (e.g., Aureal A3D, DirectSound3D),
the engine uses spatialized sound for world objects.

### Provider selection

- **`AIL_enumerate_3D_providers`** -- Lists providers; the engine selects based
  on `3DSoundProvider` registry key (string at 0x004f0104)
- **`AIL_open_3D_provider`** -- Opens the selected provider
- **`AIL_set_3D_provider_preference`** -- Configures provider-specific settings
- **`AIL_3D_provider_attribute`** -- Queries provider capabilities
- **`Disable3DSound`** registry key (0x004f00d8) -- Disables 3D audio entirely

### 3D sample lifecycle

```cpp
AIL_allocate_3D_sample_handle(provider)         --> 3D handle
AIL_set_3D_sample_file(handle, data)            --> assign WAV data
AIL_set_3D_position(handle, x, y, z)            --> world position
AIL_set_3D_orientation(handle, fx, fy, fz, ux, uy, uz)  --> facing
AIL_set_3D_sample_distances(handle, maxDist, minDist)    --> attenuation
AIL_set_3D_sample_volume(handle, vol)           --> base volume
AIL_set_3D_sample_loop_count(handle, n)         --> loop count
AIL_start_3D_sample(handle)                     --> begin playback
...
AIL_3D_sample_status(handle)                    --> poll status
AIL_3D_position(handle, &x, &y, &z)            --> query position
AIL_end_3D_sample(handle)                       --> stop
AIL_release_3D_sample_handle(handle)            --> return to pool

```

### Listener orientation

The listener position and orientation are updated each frame using the party's
world position and facing direction. The `AIL_set_3D_orientation` call uses a
forward vector and an up vector derived from the party yaw and pitch.

---

## 6. CD Audio (Redbook)

Background music is played from the game CD via Redbook (audio CD) tracks.

### API usage

```cpp
AIL_redbook_open_drive(driveLetter)         --> CD handle
AIL_redbook_tracks(handle)                  --> number of audio tracks
AIL_redbook_track_info(handle, track, &start, &end)  --> track boundaries
AIL_redbook_set_volume(handle, vol)         --> CD volume
AIL_redbook_play(handle, startPos, endPos)  --> play range
AIL_redbook_stop(handle)                    --> stop
AIL_redbook_pause(handle)                   --> pause
AIL_redbook_resume(handle)                  --> resume
AIL_redbook_volume(handle)                  --> query current volume
AIL_redbook_close(handle)                   --> release

```

### Global state

- `DAT_00f791fc` -- Redbook CD handle
- The CD drive letter is obtained from the `use_cd` INI setting

### MCI fallback

The engine opens the CD drive with an MCI command string:

```text
open %c: type cdaudio alias CD

```

This is used for initial drive detection before handing off to the Miles
Redbook API.

---

## 7. ADPCM Decompression

Sound data stored in LOD archives may use IMA ADPCM compression. The engine
decompresses on load:

```cpp
AIL_file_type(data, size)             --> detect format (WAV, ADPCM, etc.)
AIL_WAV_info(data, &info)             --> parse WAV header
AIL_decompress_ADPCM(src, dst, info)  --> decompress to PCM
AIL_mem_free_lock(buffer)             --> free temporary buffers

```

### Sound data structure layout

Sound data loaded from LOD is stored in per-sound structures:

- Offset 0x2C: Pointer to sound metadata
- Offset 0x70: PCM audio data buffer (target for ADPCM decompression)
- Sound pool base address: `DAT_00f79be0`

### Error handling

- `"Can't load sound file!"` (0x004eff70) -- LOD file not found or corrupt
- `"Sound %s is size %i bytes, sound buffer size is %i bytes"` (0x004effac) --
  Size validation warning
- `"Sound File Error"` (0x004f0098) -- General audio file error

### Sound archive

Sounds are loaded from `Sounds\Audio.snd` (string at 0x004f00c4), which is a
separate archive file outside the LOD system.

---

## 8. Audio Globals

| Address | Type | Purpose |
|---------|------|---------|
| `DAT_00f791fc` | HANDLE | Redbook CD audio handle |
| `DAT_00f79200` | HANDLE | Miles digital audio driver handle |
| `DAT_00f79340` | i32 | Mixer channel count (from INI, max 16) |
| `DAT_00f79be0` | void* | Sound data pool base address |

---

## 9. Configuration

### INI settings (mm6.ini)

| Section | Key | Default | Purpose |
|---------|-----|---------|---------|
| `[settings]` | `nosound` | 0 | Disable all audio (flag 0x10 in engine flags) |
| `[settings]` | `nowalksound` | 0 | Disable footstep sounds (flag 0x20) |
| `[settings]` | `mixerchannels` | 16 | Number of digital mixer channels (0-16) |
| `[settings]` | `use_cd` | 1 | Enable CD-ROM audio |

### Registry settings

| Key | Purpose |
|-----|---------|
| `3DSoundProvider` | Selected 3D audio provider name |
| `Disable3DSound` | Boolean to disable 3D positional audio |

### Command-line flags

- `-nosound` (string at 0x004e9908) -- Disable audio at startup

---

## 10. Key Functions

| Address | Size | Proposed Name | Purpose |
|---------|------|---------------|---------|
| 0x004a9756 | - | `Sound::LoadFromLOD` | Load sound from LOD, ADPCM decompress |
| 0x004a9b4d | - | `Sound::LoadAndValidate` | Load with size validation |
| 0x004a9dcd | - | `SoundList::SaveBinary` | Serialize dsounds.bin |
| 0x004a9e19 | - | `SoundList::LoadBinary` | Load dsounds.bin |
| 0x004a9e60 | - | `SoundList::Load` | SoundListClass general loader |
| 0x004a9e93 | - | `SoundList::LoadText` | Parse sounds.def text |
| 0x004ab798 | - | `Audio::OpenSoundArchive` | Open Sounds\Audio.snd |
| 0x004ab84e | - | `Audio::Init3D` | Initialize 3D audio provider |
| 0x004abc13 | - | `Audio::InitSplash` | SplashAudio / SplashScreen init |
| 0x004abcd3 | - | `Audio::Shutdown3D` | Close 3D audio provider |

---

## Integration notes

### Architecture mapping

| Original (Miles) | RuneHarbor (SDL3) |
|-------------------|-------------------|
| `AIL_waveOutOpen` | `SDL_OpenAudioDevice` |
| `AIL_allocate_sample_handle` | Internal mixer channel allocation |
| `AIL_set_sample_file` | Queue decoded PCM into mixer |
| `AIL_*_3D_sample_*` | SDL3 spatial audio or OpenAL |
| `AIL_redbook_*` | CD audio tracks ripped to OGG/FLAC |
| `AIL_decompress_ADPCM` | Custom IMA ADPCM decoder or SDL codec |

### Key considerations

1. **Channel limit:** The original supports up to 16 simultaneous channels.
   Modern audio systems have no practical limit, but the game logic assumes
   a fixed channel pool.

2. **3D audio coordinates:** The original uses the game's fixed-point world
   coordinate system. The reimplementation must transform to the audio
   library's coordinate space.

3. **CD audio replacement:** Modern systems have no CD drive. Music tracks
   should be loadable from OGG/FLAC files with the same track numbering.

4. **ADPCM in LOD:** WAV files in the LOD may use IMA ADPCM format 0x0011.
   The decoder must handle this transparently when loading sound data.

5. **Sound table:** The `dsounds.bin` / `sounds.def` table provides the
   mapping between logical sound IDs and actual WAV filenames. This must
   be parsed to know which sounds to load.

*All trademarks belong to their respective owners.*
