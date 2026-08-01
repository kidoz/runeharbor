---
title: "Function Index"
summary: "This index groups analyzed functions by subsystem, address, size, imports, and strings."
doc_type: reference
status: partial
last_updated: 2026-08-01
---
# Function Index

This index groups analyzed functions by subsystem, address, size, imports, and strings.

Coverage is limited to MM7 v1.21 and the artifacts cited in Scope.

## Scope

This page covers MM7 v1.21 behavior and the binary or data artifacts cited below.
Unmarked findings are **observed**; deductions and gaps are labeled **inferred**
or **unknown** per the [documentation standard](../contributing/documentation-standard.md).
Artifacts and versions not cited on this page are out of scope.
RuneHarbor-specific decisions, when present, belong in Integration notes.

## Overview

- **Total functions analyzed:** 2300
- **Classified into game systems:** 1061 (46%)
- **CRT / runtime library:** 39 (excluded from listings below)
- **Unclassified:** 1200 (excluded from listings below)
- **System categories:** 21
- **Source data:** `tools/ghidra/output/functions.txt`, `tools/ghidra/output/functions.txt`

## Classification Summary

| Category | Count | Description |
|----------|------:|-------------|
| Combat System | 1 | Turn-based combat mechanics, item/spell tables, damage calculations |
| Character System | 23 | Character creation, stats, map stats, async input hooks |
| Party System | 2 | Party positioning, spawn points, item management |
| Spell System | 19 | Spell definitions, visual effects, sound loading, registry config |
| Item System | 10 | Item rendering, paperdoll display, potion tables, object descriptions |
| Monster AI | 1 | Monster hostility tables and AI behavior data |
| NPC System | 4 | NPC data loading, dialogue text, greeting/profession tables |
| Event System | 10 | Event triggers, 2D events, decoration/chest/object description loading |
| Indoor Maps (BLV) | 4 | BLV indoor map file loading and special map checks |
| Outdoor Maps (ODM) | 5 | ODM outdoor map loading, LOD archive management, INI config |
| Save/Load System | 26 | Save game serialization, data table binary I/O, LOD operations |
| User Interface | 856 | Menu screens, HUD, character sheets, dialog windows, text rendering |
| Rendering | 36 | DirectDraw/Direct3D setup, PCX loading, blitting, texture management |
| Video Playback | 15 | Smacker and Bink video playback, frame decoding, buffer management |
| Audio System | 10 | Miles Sound System (AIL) integration, sound loading, 3D audio |
| Input System | 22 | DirectInput keyboard/mouse, async input, cursor management |
| Lighting | 4 | Lightmap management, light element definitions |
| Visibility System | 4 | Object picking, z-buffer queries, LOD array management |
| Gamma Control | 2 | Display gamma correction |
| Time/Timing | 5 | Key bindings, keyboard polling, timer management, credits scroll |
| Configuration | 2 | Exception handling frame setup |
| *CRT / Runtime* | *39* | *Standard C runtime functions (excluded)* |
| *Unclassified* | *1200* | *Not yet assigned to a system (excluded)* |
| **Total** | **2300** | |

---

## Functions by System

### Combat System (1 functions)

> Turn-based combat mechanics, item/spell tables, damage calculations

| Address | Name | Size | Key Callees | Referenced Strings |
|---------|------|-----:|-------------|-------------------|
| `0x00456dbe` | `FUN_00456dbe` | 4893 | _memset, _strlen, FUN_00453d11, FUN_00453876, FUN_004cc91... | `stditems.txt, spcitems.txt, items.txt, weapon, weapon2, ... |

### Character System (23 functions)

> Character creation, stats, map stats, async input hooks

| Address | Name | Size | Key Callees | Referenced Strings |
|---------|------|-----:|-------------|-------------------|
| `0x00409c8c` | `FUN_00409c8c` | 434 | FUN_0040d7c6, FUN_0040df4d, FUN_00409ff8, FUN_0049fbc7, F... | `Player, Enemy` |
| `0x0040a392` | `FUN_0040a392` | 401 | FUN_0040deb4, FUN_0040d76c, FUN_0040df4d, FUN_0040dece, F... | `Next player is:` |
| `0x00453f8e` | `FUN_00453f8e` | 1997 | _strlen, _strcmp, FUN_0042641d, FUN_00452c5c, FUN_004cbb5... | `MapStats.txt, GENERIC, PADDEDCELL, BATHROOM, LIVINGROOM,... |
| `0x00459cc6` | `FUN_00459cc6` | 55 | GetAsyncKeyState, FUN_0045a035, FUN_00459cfd |  |
| `0x00459e78` | `FUN_00459e78` | 27 | GetAsyncKeyState |  |
| `0x0045b0a7` | `FUN_0045b0a7` | 59 | GetAsyncKeyState, FUN_0045b525 |  |
| `0x0045b0e2` | `FUN_0045b0e2` | 37 | GetAsyncKeyState, FUN_0045b4d8 |  |
| `0x0045b107` | `FUN_0045b107` | 32 | GetAsyncKeyState, FUN_0045b569 |  |
| `0x0046479e` | `FUN_0046479e` | 74 | SetPriorityClass, GetCurrentProcess, DestroyWindow, GetLa... |  |
| `0x004aa1ed` | `FUN_004aa1ed` | 174 | _AIL_end_sample@4, _AIL_end_3D_sample@4, _AIL_3D_sample_s... |  |
| `0x004aa29b` | `FUN_004aa29b` | 2955 | _AIL_sample_volume@4, _AIL_end_sample@4, _AIL_set_3D_orie... |  |
| `0x004aaf4f` | `FUN_004aaf4f` | 1693 | _AIL_end_sample@4, _AIL_set_sample_pan@8, _AIL_set_3D_ori... |  |
| `0x004abdcd` | `FUN_004abdcd` | 206 | _AIL_sample_status@4, FUN_004a9d5d |  |
| `0x004abe9b` | `FUN_004abe9b` | 184 | _AIL_3D_sample_status@4, FUN_004a9d5d |  |
| `0x004cbef4` | `FUN_004cbef4` | 9 | __fload_withFB |  |
| `0x004cbefd` | `FUN_004cbefd` | 149 | __math_exit, __startOneArgErrorHandling, FUN_004ce66c |  |
| `0x004cbfb4` | `FUN_004cbfb4` | 9 | __fload_withFB |  |
| `0x004cbfbd` | `FUN_004cbfbd` | 145 | __math_exit, __startOneArgErrorHandling, FUN_004ce66c |  |
| `0x004cc064` | `FUN_004cc064` | 9 | __fload_withFB |  |
| `0x004cc06d` | `FUN_004cc06d` | 145 | __math_exit, __startOneArgErrorHandling, FUN_004ce66c |  |
| `0x004ce729` | `FUN_004ce729` | 163 | __startOneArgErrorHandling, FUN_004ce5e0 |  |
| `0x004d284e` | `FUN_004d284e` | 14 |  |  |
| `0x004d285c` | `FUN_004d285c` | 15 |  |  |

### Party System (2 functions)

> Party positioning, spawn points, item management

| Address | Name | Size | Key Callees | Referenced Strings |
|---------|------|-----:|-------------|-------------------|
| `0x004498f8` | `FUN_004498f8` | 372 | FUN_004cac80, FUN_004488d9 | `West Start, East Start, South Start, North Start, Party ... |
| `0x0048c6dc` | `FUN_0048c6dc` | 351 | FUN_004aa29b, FUN_0040e2d4, FUN_004948a9, FUN_004927a0, F... | `D:\mm7Src_eng\MM7\Code\Party.cpp, Invalid picture_name d... |

### Spell System (19 functions)

> Spell definitions, visual effects, sound loading, registry config

| Address | Name | Size | Key Callees | Referenced Strings |
|---------|------|-----:|-------------|-------------------|
| `0x0042f3b2` | `FUN_0042f3b2` | 260 | FUN_00494f50, FUN_00494fd5, FUN_0044d83f | `turn0, turn1, turn2, turn3, turn4, turnstop, turnhour, t... |
| `0x004415e8` | `FUN_004415e8` | 104 | FUN_0040fb2c, FUN_004cad70, FUN_00494f50 | `isn-%02d, spell21, spell27` |
| `0x00443d04` | `FUN_00443d04` | 192 | _memset, FUN_0042641d, FUN_00466be9, FUN_004cad70, FUN_00... | `Unable to load %s, File %s Size %lu - Buffer size %lu` |
| `0x00453876` | `FUN_00453876` | 718 | _strlen, FUN_0042641d, FUN_00452c5c, FUN_004cc91e, FUN_00... | `spells.txt, water, earth, spirit, light, magic` |
| `0x00461991` | `FUN_00461991` | 189 | MessageBoxA, FUN_0042641d, FUN_004266fe | `Attempt to reset a LOD subindex!, LODsub, Attempt to res... |
| `0x00464a2c` | `FUN_00464a2c` | 275 | RegOpenKeyExA, RegSetValueExA, RegCreateKeyExA, RegQueryV... | `SOFTWARE, New World Computing, Might and Magic VII` |
| `0x00464b3f` | `FUN_00464b3f` | 237 | RegOpenKeyExA, RegSetValueExA, _strlen, RegCreateKeyExA, ... | `SOFTWARE, New World Computing, Might and Magic VII` |
| `0x00464c2c` | `FUN_00464c2c` | 323 | RegOpenKeyExA, RegCreateKeyExA, RegQueryValueExA, RegClos... | `SOFTWARE, New World Computing, Might and Magic VII` |
| `0x00464d6f` | `FUN_00464d6f` | 229 | RegOpenKeyExA, RegSetValueExA, RegCreateKeyExA, RegCloseKey | `SOFTWARE, New World Computing, Might and Magic VII` |
| `0x0049dda4` | `FUN_0049dda4` | 1584 | _memset, DirectDrawCreate, FUN_004cad70 | `Init - Failed to create DirectDraw interface.\n, Init - ... |
| `0x004a894d` | `FUN_004a894d` | 303 | FUN_00494f50 | `spboost1, spboost2, spboost3, spheal1, spheal2, spheal3,... |
| `0x004a8b8c` | `FUN_004a8b8c` | 43 | FUN_0044d83f | `spell84` |
| `0x004a8bb7` | `FUN_004a8bb7` | 1027 | FUN_0044d83f, FUN_004a7228, FUN_004acb9b, FUN_00466cc6, F... | `spell84, D:\mm7Src_eng\MM7\Code\seffects.cpp` |
| `0x004a9030` | `FUN_004a9030` | 1099 | FUN_0044d83f, FUN_00494fd5, FUN_0040fb2c, FUN_00494f50, F... | `effpar01, effpar02, effpar03, sp57c, spheal1, spheal2, s... |
| `0x004a9b4d` | `FUN_004a9b4d` | 385 | SetFilePointer, ReadFile, _malloc, FUN_0040e2d4, FUN_0046... | `Sound %s is size %i bytes, sound buffer size is %i bytes... |
| `0x004ab798` | `FUN_004ab798` | 182 | MessageBoxA, ReadFile, CreateFileA, FUN_004cad70, FUN_004... | `Sounds\Audio.snd, Can't open file - %s, Sound File Error` |
| `0x004be93b` | `FUN_004be93b` | 361 | MessageBoxA, ReadFile, CreateFileA, FUN_004cad70, FUN_004... | `anims\might7.vid, Can't open file - anims\%s.smk, Video ... |
| `0x004d2e55` | `FUN_004d2e55` | 147 | GetLastError, FlushFileBuffers, FUN_004d3e2e, FUN_004d1be... |  |
| `0x004d39fa` | `FUN_004d39fa` | 339 | _strlen, GetStdHandle, GetModuleFileNameA, WriteFile, _st... | `<program name unknown>, Runtime Error!\n\nProgram: , R60...` |

### Item System (10 functions)

> Item rendering, paperdoll display, potion tables, object descriptions

| Address | Name | Size | Key Callees | Referenced Strings |
|---------|------|-----:|-------------|-------------------|
| `0x0043bcca` | `FUN_0043bcca` | 3190 | wsprintfA, FUN_00490101, FUN_0043eddc, FUN_0043ee38, FUN_... | `MAGNIF-B, BACKDOLL, BACKHAND, pc23v%dBod, pc23v%dlad, pc... |
| `0x0043c940` | `FUN_0043c940` | 744 | FUN_004cad70 | `item%3.3dv%d, item%3.3dv%da1, item%3.3dv%da2` |
| `0x0043f0e0` | `FUN_0043f0e0` | 365 | wsprintfA, FUN_0040fb2c | `pc%02dbrd, item281pc%02d` |
| `0x00453b68` | `FUN_00453b68` | 425 | wsprintfA, MessageBoxA, _strcmp, FUN_0042641d, FUN_004cbb... | `potion.txt, Load Error, Error Pre-Parsing Potion Table, ... |
| `0x00453d11` | `FUN_00453d11` | 392 | wsprintfA, MessageBoxA, _strcmp, FUN_0042641d, FUN_004cbb... | `potnotes.txt, Load Error, Error Pre-Parsing Potion Table... |
| `0x0045490e` | `FUN_0045490e` | 978 | FUN_0040e2d4, FUN_00466d0d, FUN_004caaf0, FUN_004cad70 | `Dispel, Shield, Spirit, Power, Meteor, Lightning, Implos... |
| `0x00461fae` | `FUN_00461fae` | 99 | MessageBoxA, FUN_004cb4ec | `LODFile, Unable to append item!` |
| `0x00464f58` | `FUN_00464f58` | 346 | EndDialog, SetDlgItemTextA, GetUserDefaultLangID, SetWind... | `Anuluj, Inserire il secondo CD, Annulla, Supprimer, Por ... |
| `0x004764c6` | `FUN_004764c6` | 206 | FUN_0042641d, FUN_00452c5c, FUN_004cc17b, FUN_00410897 | `scroll.txt` |
| `0x00476754` | `FUN_00476754` | 345 | FUN_0042641d, FUN_00452c5c, FUN_004cc17b, FUN_004caaf0, F... | `autonote.txt, potion, obelisk, teacher` |

### Monster AI (1 functions)

> Monster hostility tables and AI behavior data

| Address | Name | Size | Key Callees | Referenced Strings |
|---------|------|-----:|-------------|-------------------|
| `0x00454810` | `FUN_00454810` | 254 | FUN_0042641d, FUN_004cbb55, FUN_004cc17b, FUN_00410897 | `hostile.txt` |

### NPC System (4 functions)

> NPC data loading, dialogue text, greeting/profession tables

| Address | Name | Size | Key Callees | Referenced Strings |
|---------|------|-----:|-------------|-------------------|
| `0x00422698` | `FUN_00422698` | 4628 | FUN_0040f420, FUN_0040fb2c, FUN_0040df03, FUN_00494f50, F... | `ib-r-C.pcx, ib-b-c.pcx, ib-t-C.pcx, ib-l-C.pcx, IB-Foot-... |
| `0x0047697b` | `FUN_0047697b` | 745 | FUN_0042641d, FUN_00452c5c, FUN_004cbb55, FUN_004cc17b, F... | `npctext.txt, npctopic.txt, npcdist.txt` |
| `0x00476cb9` | `FUN_00476cb9` | 830 | FUN_00452c5c, FUN_004cbb55, FUN_004cc17b, FUN_00410897 | `npcdata.txt, npcgreet.txt, npcgroup.txt, npcnews.txt` |
| `0x00477033` | `FUN_00477033` | 567 | FUN_00476cb9, FUN_00452c5c, FUN_0047697b, FUN_004768ad, F... | `npcnames.txt, npcprof.txt` |

### Event System (10 functions)

> Event triggers, 2D events, decoration/chest/object description loading

| Address | Name | Size | Key Callees | Referenced Strings |
|---------|------|-----:|-------------|-------------------|
| `0x00443824` | `FUN_00443824` | 1160 | FUN_0042641d, FUN_004cc5bf, FUN_00410897, FUN_004cbb55, F... | `2dEvents.txt` |
| `0x00443e54` | `FUN_00443e54` | 199 | _memset, FUN_00466be9, FUN_004cad70, FUN_00452c5c | `MAX_EVENT_TEXT_LENGTH needs to be increased to %lu` |
| `0x00444383` | `FUN_00444383` | 117 | FUN_00443e54, FUN_004cad70, FUN_00443d04 | `%s.evt, %s.str` |
| `0x0044485c` | `FUN_0044485c` | 536 | FUN_00444833, FUN_0044c2b7, FUN_004948a9, FUN_004547cf, F... | `evt%02d` |
| `0x00444cb2` | `FUN_00444cb2` | 241 | FUN_004262f2, FUN_0044c2b7, FUN_004cac80, FUN_004547cf, F... | `evt%02d, outside` |
| `0x004586e9` | `FUN_004586e9` | 1107 | _strchr, FUN_004cb7ec, FUN_004cb656, FUN_004266fe, FUN_00... | `DecorationDescriptionList::load - Unable to open file: %... |
| `0x00458bd5` | `FUN_00458bd5` | 507 | _memset, _strchr, FUN_0042641d, FUN_004be322, FUN_004cb7e... | `ChestDescriptionList::load - Unable to open file: %s., C... |
| `0x00458e88` | `FUN_00458e88` | 533 | _memset, _strchr, FUN_004cb7ec, FUN_004cb656, FUN_004266f... | `ObjectDescriptionList::load - Unable to open file: %s., ... |
| `0x0045915c` | `FUN_0045915c` | 1143 | _memset, _strchr, FUN_004cb7ec, FUN_004cb656, FUN_004266f... | `ObjectDescriptionList::load - Unable to open file: %s., ... |
| `0x00461401` | `FUN_00461401` | 81 | FUN_004488d9 | `Event Trigger` |

### Indoor Maps (BLV) (4 functions)

> BLV indoor map file loading and special map checks

| Address | Name | Size | Key Callees | Referenced Strings |
|---------|------|-----:|-------------|-------------------|
| `0x00408768` | `FUN_00408768` | 302 | FUN_0040894b, FUN_004031c1, FUN_00403eb6, FUN_00438b8a, F... | `d25.blv, d26.blv` |
| `0x00444833` | `FUN_00444833` | 41 | FUN_004caaf0 | `mdt12.blv, d18.blv` |
| `0x0044c2bb` | `FUN_0044c2bb` | 101 | FUN_0044989e, FUN_004caaf0 | `nwc.blv` |
| `0x00462759` | `FUN_00462759` | 155 |  | `Indoor  BLV Files (*.blv), levels` |

### Outdoor Maps (ODM) (5 functions)

> ODM outdoor map loading, LOD archive management, INI config

| Address | Name | Size | Key Callees | Referenced Strings |
|---------|------|-----:|-------------|-------------------|
| `0x004608a7` | `FUN_004608a7` | 526 | FUN_004ccc14, FUN_00461fae, FUN_004617f3, FUN_00461a80, F... | `data\new.lod, MMVII, newmaps for MMVII, current, out01.o... |
| `0x00466086` | `FUN_00466086` | 1619 | _strlen, GetPrivateProfileStringA, GetPrivateProfileIntA,... | `%s\mm6.ini, screen, settings, mixerchannels, nointro, no... |
| `0x0047cde6` | `FUN_0047cde6` | 442 | _memset, FUN_0047f424, FUN_0042641d, FUN_0047f3ee, FUN_00... | `blank, i6.odm, MM6 Outdoor v1.00, IDLIST, Invalid Sky Te... |
| `0x0047cfa0` | `FUN_0047cfa0` | 266 | FUN_0042641d, FUN_0047c7c6, FUN_004cac80, FUN_0047838d | `blank, default.odm, MM6 Outdoor v1.00, sky043, hm005` |
| `0x0047f13c` | `FUN_0047f13c` | 235 | FUN_004a99f7, FUN_0044c320, FUN_004586cc, FUN_004caaf0, F... | `out09.odm` |

### Save/Load System (26 functions)

> Save game serialization, data table binary I/O, LOD operations

| Address | Name | Size | Key Callees | Referenced Strings |
|---------|------|-----:|-------------|-------------------|
| `0x0040f601` | `FUN_0040f601` | 363 | _malloc, FUN_0040f043, FUN_0042641d, FUN_00466be9, FUN_00... | `Unable to load %s` |
| `0x00410897` | `FUN_00410897` | 242 | _malloc, FUN_0042641d, FUN_00466be9, FUN_0040f5ca, FUN_00... | `Unable to load %s` |
| `0x004434a7` | `FUN_004434a7` | 311 | _memset, _rand, FUN_0040f420, FUN_0041052e, FUN_004cad70 ... | `loading%d.pcx, loadprog, bardata-c, bardata, bardata-b` |
| `0x0044d999` | `FUN_0044d999` | 106 | FUN_004cb656, FUN_00466be9, FUN_004cb4ec, FUN_004cb46f | `data\dsft.bin, Unable to save dsft.bin!` |
| `0x0044e080` | `FUN_0044e080` | 76 | FUN_004cb4ec, FUN_00466be9, FUN_004cb656, FUN_004cb46f | `data\dtft.bin, Unable to save dtft.bin!` |
| `0x0044e218` | `FUN_0044e218` | 687 | _strchr, FUN_004be322, FUN_004cb7ec, FUN_00494ae5, FUN_00... | `CTextureFrameTable::load - Unable to open file: %s., Txt... |
| `0x00458639` | `FUN_00458639` | 76 | FUN_004cb4ec, FUN_00466be9, FUN_004cb656, FUN_004cb46f | `data\ddeclist.bin, Unable to save ddeclist.bin!` |
| `0x00458b3c` | `FUN_00458b3c` | 76 | FUN_004cb4ec, FUN_00466be9, FUN_004cb656, FUN_004cb46f | `data\dchest.bin, Unable to save dchest.bin!` |
| `0x00458df5` | `FUN_00458df5` | 76 | FUN_004cb4ec, FUN_00466be9, FUN_004cb656, FUN_004cb46f | `data\doverlay.bin, Unable to save doverlay.bin!` |
| `0x004590c9` | `FUN_004590c9` | 76 | FUN_004cb4ec, FUN_00466be9, FUN_004cb656, FUN_004cb46f | `data\dobjlist.bin, Unable to save dobjlist.bin!` |
| `0x00459899` | `FUN_00459899` | 79 | FUN_004cb4ec, FUN_00466be9, FUN_004cb656, FUN_004cb46f | `data\dmonlist.bin, Unable to save dmonlist.bin!` |
| `0x0045e2d0` | `FUN_0045e2d0` | 202 | _memset, FUN_004d6cd6, FUN_004cac80, FUN_004d6d1c, FUN_00... | `saves, save%03d.mm7` |
| `0x0045f4a2` | `FUN_0045f4a2` | 3087 | _malloc, operator_new, CopyFileA, GetLastError, FormatMes... | `d05.blv, image.pcx, D:\mm7Src_eng\MM7\Code\LoadSave.cpp,... |
| `0x004600b1` | `FUN_004600b1` | 325 | CopyFileA, FUN_0041c213, FUN_0042630c, FUN_0040e52b, FUN_... | `d05.blv, header.bin, saves\save%03d.mm7, data\new.lod` |
| `0x00487f9a` | `FUN_00487f9a` | 76 | FUN_004cb4ec, FUN_00466be9, FUN_004cb656, FUN_004cb46f | `data\dtile.bin, Unable to save dtile.bin!` |
| `0x0048802d` | `FUN_0048802d` | 3520 | _memset, _strchr, FUN_004cb7ec, FUN_004cb656, FUN_004266f... | `TileTable::load - Unable to open file: %s., Tile Descrip... |
| `0x00488f42` | `FUN_00488f42` | 191 | MessageBoxA, FUN_0046084a, FUN_00460745, FUN_0047cde6, FU... | `Couldn't Load Map!, Error!` |
| `0x00494bd9` | `FUN_00494bd9` | 76 | FUN_004cb46f, FUN_004cb656, FUN_004cb4ec, FUN_00466be9 | `data\dpft.bin, Unable to save dpft.bin!` |
| `0x00494c70` | `FUN_00494c70` | 736 | _strchr, FUN_004cb46f, FUN_004cb7ec, FUN_004cb656, FUN_00... | `PlayerFrameTable::load - Unable to open file: %s., P Fra... |
| `0x00495020` | `FUN_00495020` | 76 | FUN_004cb46f, FUN_004cb656, FUN_004cb4ec, FUN_00466be9 | `data\dift.bin, Unable to save dift.bin!` |
| `0x004950b3` | `FUN_004950b3` | 713 | _strchr, FUN_004cb46f, FUN_004cb7ec, FUN_004be3e8, FUN_00... | `IconFrameTable::load - Unable to open file: %s., I Frame... |
| `0x004a9756` | `FUN_004a9756` | 397 | SetFilePointer, ReadFile, _malloc, FUN_0040e2d4, FUN_0046... | `D:\mm7Src_eng\MM7\Code\Sound.cpp, Can't load sound file!` |
| `0x004a9e60` | `FUN_004a9e60` | 623 | _memset, _strchr, FUN_004cb656, FUN_004cad70, FUN_004be32... | `SoundListClass::load - Unable to open file: %s., Snd Des... |
| `0x004bd818` | `FUN_004bd818` | 506 | FUN_004b3aa5, FUN_0041c213, FUN_0040f9d1, FUN_004b39d5, F... |  |
| `0x004cc484` | `FUN_004cc484` | 9 | __fload_withFB |  |
| `0x004d622f` | `FUN_004d622f` | 137 | GetProcAddress, LoadLibraryA | `user32.dll, MessageBoxA, GetActiveWindow, GetLastActiveP... |

### User Interface (856 functions)

> Menu screens, HUD, character sheets, dialog windows, text rendering

| Address | Name | Size | Key Callees | Referenced Strings |
|---------|------|-----:|-------------|-------------------|
| `0x0040104c` | `FUN_0040104c` | 469 |  |  |
| `0x00401221` | `FUN_00401221` | 709 | FUN_004089c7, FUN_0040104c, FUN_004ca62e, FUN_004070ef |  |
| `0x004014e6` | `FUN_004014e6` | 532 | FUN_0040894b, FUN_0040104c, FUN_004ca62e |  |
| `0x004016fa` | `FUN_004016fa` | 919 | FUN_0040894b, FUN_0040104c, FUN_004ca780, FUN_0049aba0, F... |  |
| `0x00401a91` | `FUN_00401a91` | 2956 | _rand, __ftol, FUN_00403eb6, FUN_004030ad, FUN_00403476 (... |  |
| `0x00402968` | `FUN_00402968` | 367 | _rand, FUN_00438bce, FUN_00403f58, FUN_0040894b, FUN_0040... |  |
| `0x00402ad7` | `FUN_00402ad7` | 471 | FUN_00438bce, FUN_00403f58, FUN_00403eb6, FUN_004040e9, F... |  |
| `0x00402cae` | `FUN_00402cae` | 63 |  |  |
| `0x00402d6e` | `FUN_00402d6e` | 409 | _rand, FUN_0042f7c7, FUN_004597a6, FUN_00402ced, FUN_0040... |  |
| `0x00402f87` | `FUN_00402f87` | 294 | _rand, FUN_00403eb6, FUN_004040e9, FUN_004597a6, FUN_0040... |  |
| `0x004031c1` | `FUN_004031c1` | 241 | FUN_0040894b |  |
| `0x004032b2` | `FUN_004032b2` | 452 | _rand, FUN_00438bce, FUN_00403f58, FUN_00403eb6, FUN_0045... |  |
| `0x00403476` | `FUN_00403476` | 533 | __ftol, FUN_004040e9, FUN_004597a6, FUN_00402ced, FUN_004... |  |
| `0x0040368b` | `FUN_0040368b` | 457 | __ftol, FUN_004040e9, FUN_004597a6, FUN_00402ced, FUN_004... |  |
| `0x00403854` | `FUN_00403854` | 524 | __ftol, FUN_004040e9, FUN_004597a6, FUN_00402ced, FUN_004... |  |
| `0x00403a60` | `FUN_00403a60` | 524 | __ftol, FUN_004040e9, FUN_004597a6, FUN_00402ced, FUN_004... |  |
| `0x00403c6c` | `FUN_00403c6c` | 501 | _rand, __ftol, FUN_00403eb6, FUN_004040e9, FUN_004597a6 (... |  |
| `0x004040e9` | `FUN_004040e9` | 1115 | __ftol, FUN_0043aabc, FUN_004ca654, FUN_0045284a |  |
| `0x00404544` | `FUN_00404544` | 472 | __ftol, FUN_0040894b |  |
| `0x00404ac7` | `FUN_00404ac7` | 3860 | _rand, __ftol, FUN_0048ca25, FUN_004aa29b, FUN_0048c9a8 (... |  |
| `0x004059db` | `FUN_004059db` | 804 | _rand, __ftol, FUN_0042632f, FUN_00403f58, FUN_004aa29b (... |  |
| `0x00405cff` | `FUN_00405cff` | 277 | __ftol, FUN_004aa29b, FUN_00426349, FUN_004ab69f |  |
| `0x00405e14` | `FUN_00405e14` | 573 | FUN_004061ca, FUN_00403f58, FUN_00406afe, FUN_004065b0, F... |  |
| `0x004061ca` | `FUN_004061ca` | 471 | FUN_00403f58, FUN_0049402d, FUN_004597a6, FUN_00404544 |  |
| `0x0040680f` | `FUN_0040680f` | 596 | FUN_00403eb6, FUN_004040e9, FUN_00403854, FUN_00427002, F... |  |
| `0x004070ef` | `FUN_004070ef` | 1260 | FUN_0049aba0, FUN_004075db, FUN_004ca62e, FUN_00452aca |  |
| `0x004075db` | `FUN_004075db` | 534 |  |  |
| `0x004077f1` | `FUN_004077f1` | 555 |  |  |
| `0x00407a1c` | `FUN_00407a1c` | 3277 | FUN_0049aba0, FUN_0045284a, FUN_004075db, FUN_004ca62e, F... |  |
| `0x004086e9` | `FUN_004086e9` | 127 | _rand, FUN_0045664c |  |
| `0x00408a27` | `FUN_00408a27` | 87 | FUN_00450dcf, FUN_004089c7 |  |
| `0x00408a7e` | `FUN_00408a7e` | 105 | FUN_00450dcf, FUN_004089c7 |  |
| `0x00408ae7` | `FUN_00408ae7` | 109 | FUN_00450dcf, FUN_004089c7 |  |
| `0x00408b54` | `FUN_00408b54` | 68 | FUN_00450dcf, FUN_004089c7 |  |
| `0x00409be9` | `FUN_00409be9` | 163 |  |  |
| `0x00409e3e` | `FUN_00409e3e` | 45 |  |  |
| `0x0040a56f` | `FUN_0040a56f` | 1103 | FUN_0040bb58, FUN_0049fbc7, FUN_0040deb5, FUN_0040d7e4, F... | `Are you sure you want to quit?, Cancel` |
| `0x0040b111` | `FUN_0040b111` | 124 | FUN_0040d7fb, FUN_0040db1f, FUN_0040d7e4 |  |
| `0x0040b2ec` | `FUN_0040b2ec` | 291 | FUN_0040d7fb, FUN_0040d9c0 |  |
| `0x0040b40f` | `FUN_0040b40f` | 185 | FUN_0040d7fb |  |
| `0x0040b4c8` | `FUN_0040b4c8` | 694 | FUN_0040d7fb, FUN_0040bb21, FUN_0040bb58, FUN_0040bf24, F... |  |
| `0x0040bb76` | `FUN_0040bb76` | 404 | FUN_0040deeb, FUN_0040bb58, FUN_0040bf24, FUN_0040dd3e, F... |  |
| `0x0040bd0a` | `FUN_0040bd0a` | 275 | FUN_0040bb58, FUN_0040ddd8 |  |
| `0x0040be1d` | `FUN_0040be1d` | 263 | FUN_0040bf24, FUN_0040bb58, FUN_0040ddd8 | `Brick Shortage` |
| `0x0040bf86` | `FUN_0040bf86` | 4841 | FUN_0040bb58, FUN_0040a292, FUN_0040d411, FUN_0040ddd8, F... | `Brick Shortage` |
| `0x0040dd3e` | `FUN_0040dd3e` | 100 |  |  |
| `0x0040dda2` | `FUN_0040dda2` | 30 |  |  |
| `0x0040df03` | `FUN_0040df03` | 74 |  |  |
| `0x0040e36f` | `FUN_0040e36f` | 134 | FUN_0040e3f5, FUN_004ca2d8, FUN_0040e302 |  |
| `0x0040e3f5` | `FUN_0040e3f5` | 63 | operator_new, FUN_004cb450, FUN_0040e459 |  |
| `0x0040e434` | `Catch@0040e434` | 31 | operator_new |  |
| `0x0040e459` | `FUN_0040e459` | 89 | FUN_0040e302, FUN_004ca780 |  |
| `0x0040e56a` | `FUN_0040e56a` | 1314 | operator_new, FUN_004cb656, FUN_0042641d, FUN_00466be9, F... | `24bit PCX Only!` |
| `0x0040eae4` | `FUN_0040eae4` | 1287 | operator_new, FUN_0042641d, FUN_00466be9, FUN_004cb8a5, F... | `24bit PCX Only!` |
| `0x0040f043` | `FUN_0040f043` | 901 | FUN_004ca780 |  |
| `0x0040f420` | `FUN_0040f420` | 426 | _malloc, FUN_0040f043, FUN_0042641d, FUN_00466be9, FUN_00... | `Unable to load %s, 16bit PCX` |
| `0x0041052e` | `FUN_0041052e` | 785 | operator_new, FUN_004cac80, FUN_004ca2cd, FUN_00466be9, F... | `pending, Can't find %s!` |
| `0x00410b34` | `FUN_00410b34` | 625 | FUN_0040df03, FUN_0044ce34, FUN_004cad70, FUN_0044d432, F... | `%s\n\n%s %03d: %03d%s 000\n%s %03d: %03d%s 000\n%s %03d:... |
| `0x00410df8` | `FUN_00410df8` | 868 | __ftol, __alldiv, FUN_00410da5, FUN_004cad70, FUN_0044d43... | `%lu %s` |
| `0x0041115c` | `FUN_0041115c` | 432 | FUN_004a0e10, FUN_0044d432, FUN_00469c3d, FUN_004a6204, F... |  |
| `0x0041130c` | `FUN_0041130c` | 267 | FUN_0040fb2c, FUN_004cad70 | `SB%sS%02d, SB%sC%02d` |
| `0x0041162d` | `FUN_0041162d` | 779 | FUN_0041d038, FUN_0041d0d8, FUN_0041130c |  |
| `0x00411c07` | `FUN_00411c07` | 3837 | _memset, FUN_0040fb2c, FUN_0041d0d8, FUN_00495477, FUN_00... | `sbplayrnot, tab-an-6b, tab-an-7b, tab-an-6a, tab-an-7a, ... |
| `0x00412b04` | `FUN_00412b04` | 95 | FUN_004a5e42 |  |
| `0x00412b63` | `FUN_00412b63` | 741 | FUN_0040f9fc, FUN_004a0e10, FUN_00412b04, FUN_00469c3d, F... | `Pending` |
| `0x00412e90` | `FUN_00412e90` | 673 | FUN_0044ce34, FUN_00495477, FUN_0044d432, FUN_0044c6ee, F... |  |
| `0x00413131` | `FUN_00413131` | 616 | FUN_0044ce34, FUN_0044d432, FUN_004a5e42, FUN_004a6204, F... |  |
| `0x00413399` | `FUN_00413399` | 1522 | FUN_0044ce34, FUN_0044d432, FUN_004a5e42, FUN_004a6204, F... |  |
| `0x00413cd1` | `FUN_00413cd1` | 118 | FUN_00413d7a, FUN_00413131, FUN_0041115c, FUN_004a5e42, F... |  |
| `0x00413d7a` | `FUN_00413d7a` | 609 | FUN_004547cf, FUN_0040df03, FUN_0044ce34, FUN_004cad70, F... | `%s 100: 110%d:%02d %s - %s, %s 100: 110%d - %s, %s 100: ... |
| `0x004142de` | `FUN_004142de` | 2641 | FUN_0040df03, FUN_0044ce34, FUN_00414d2f, FUN_004a5e42, F... | `FORWARD, BACKWARD, RIGHT, COMBAT, CAST READY, ATTACK, TR... |
| `0x00414da5` | `FUN_00414da5` | 488 | FUN_0040df03, FUN_0044d432, FUN_004a5e42, FUN_0044f2de, F... |  |
| `0x00414f8d` | `FUN_00414f8d` | 510 | FUN_004a5e42 |  |
| `0x0041518b` | `FUN_0041518b` | 89 | _strlen, FUN_0044ce34, FUN_0044c54a |  |
| `0x004151e4` | `FUN_004151e4` | 684 | FUN_004a5b11, FUN_004a5b46, FUN_004a5e42, FUN_004a6204 |  |
| `0x00415490` | `FUN_00415490` | 204 | _memset, FUN_0044d432, FUN_0041555c, FUN_0044c5c9 |  |
| `0x004156fb` | `FUN_004156fb` | 2534 | _memset, _rand, FUN_00460736, FUN_00444a74, FUN_0040df03 ... | `Making item number` |
| `0x004160e1` | `FUN_004160e1` | 204 | _rand |  |
| `0x004161ad` | `FUN_004161ad` | 2265 | __ftol, _rand, FUN_004160e1, FUN_004948a9, FUN_00458299 (... |  |
| `0x00416aaa` | `FUN_00416aaa` | 609 | _strcmp, FUN_004cac80, FUN_0040fb2c, FUN_0040df03, FUN_00... | `NPC%03d` |
| `0x00416d0b` | `FUN_00416d0b` | 2142 | ScreenToClient, GetCursorPos, FUN_00416aaa, FUN_0040df03,... |  |
| `0x00417569` | `FUN_00417569` | 689 | __ftol, FUN_00469864, FUN_0046381d, FUN_00466ca4, FUN_004... |  |
| `0x00417965` | `FUN_00417965` | 280 | _memset, FUN_0040df03, FUN_0044ce34, FUN_004cad70, FUN_00... |  |
| `0x00417a7d` | `FUN_00417a7d` | 225 | FUN_0040df03 |  |
| `0x00417b5e` | `FUN_00417b5e` | 1072 | FUN_004cac80, FUN_00417a7d, FUN_0040df03, FUN_004cad70, F... | `%s %03d: %03d%s 000\n, %s %03d: %03d%s 000\n\n, %s: +%d` |
| `0x0041802c` | `FUN_0041802c` | 1058 | __ftol, __alldiv, __allrem, FUN_004b465b, FUN_004cac80 (+... | `%lu %s, %lu %s, %s\n \n%s` |
| `0x004184ba` | `FUN_004184ba` | 3055 | FUN_0048d47e, FUN_0040df03, FUN_0048c8f3, FUN_0048e687, F... | `fr_stats, %s: %s, %s 100%+d\n, %s 100 %s\n, %s 100 %s\n\n` |
| `0x004191c9` | `FUN_004191c9` | 345 | FUN_0041d0d8 |  |
| `0x004193aa` | `FUN_004193aa` | 671 | FUN_0041d038, FUN_0041d0d8, FUN_00419649 |  |
| `0x004196c2` | `FUN_004196c2` | 2279 | FUN_0040df03, FUN_0040fb2c, FUN_0044ce34, FUN_004cad70, F... | `fr_skill, %s400%s, %s400%2d, %s177%s, %s177%2d` |
| `0x00419fa9` | `FUN_00419fa9` | 705 | FUN_0040df03, FUN_0040fb2c, FUN_0044ce34, FUN_004cad70, F... | `fr_award` |
| `0x0041a27a` | `FUN_0041a27a` | 645 | __ftol, GetTickCount, FUN_004a6376, FUN_004a687f, FUN_004... | `fr_strip, sp91a, sp30a, sp28a, sptext01` |
| `0x0041a4ff` | `FUN_0041a4ff` | 40 | FUN_004a5e42 |  |
| `0x0041a527` | `FUN_0041a527` | 1663 | FUN_0048d47e, FUN_0048e55d, FUN_0040df03, FUN_0048c8f3, F... | `quikref, 261%s: %d` |
| `0x0041aba6` | `FUN_0041aba6` | 369 | FUN_00419fa9, FUN_004a5e42, FUN_0043e848, FUN_00419322, F... | `ib-cd3-d, ib-cd4-d, ib-cd2-d, ib-cd1-d` |
| `0x0041ad17` | `FUN_0041ad17` | 333 | FUN_00491f7f, FUN_004a6204, FUN_004a5b73 |  |
| `0x0041ae64` | `FUN_0041ae64` | 151 | FUN_00441030, FUN_0044ce34, FUN_004cad70 | `087%lu, 028%lu` |
| `0x0041aefb` | `FUN_0041aefb` | 375 | __ftol, FUN_004a5b11, FUN_004a5b46, FUN_004a5e42 |  |
| `0x0041b072` | `FUN_0041b072` | 749 | __ftol, FUN_0048e55d, FUN_004a5b11, FUN_004a5b46, FUN_004... |  |
| `0x0041b35f` | `FUN_0041b35f` | 44 | FUN_004a6204 |  |
| `0x0041b639` | `FUN_0041b639` | 2487 | FUN_0040fb2c, FUN_0041d0d8, FUN_00494f50, FUN_00494fd5, F... | `wizeyeC, wizeyeB, wizeyeA, torchC, torchB, torchA, MAPDI... |
| `0x0041c3db` | `FUN_0041c3db` | 2313 | FUN_0041d038, FUN_0041d0d8, FUN_00445cae, FUN_0041162d, F... | `MICON2` |
| `0x0041d0d8` | `FUN_0041d0d8` | 222 | _strlen, FUN_004cac80, FUN_004266fe | `BUTTON` |
| `0x0041d1b6` | `FUN_0041d1b6` | 426 | __ftol, __alldiv, __allrem, FUN_004cac80, FUN_0044ce34 (+... | `%d %s` |
| `0x0041d360` | `FUN_0041d360` | 902 | FUN_0048e55d, FUN_00494b03, FUN_0040df03, FUN_0041d1b6, F... | `%s: %s` |
| `0x0041d6e6` | `FUN_0041d6e6` | 344 | GetTickCount, FUN_0040df03, FUN_0044ce34, FUN_0041d1b6, F... |  |
| `0x0041d83e` | `FUN_0041d83e` | 2763 | FUN_0040f9d1, FUN_0040df03, FUN_004564df, FUN_004a6204, F... | `%s: +%d, %s: +%d   %s: %dd%d, %s: %d, %s: %s +%d, %s: %s... |
| `0x0041e309` | `FUN_0041e309` | 4365 | _memset, _strncpy, _rand, FUN_004948a9, FUN_0045827d (+18... | `%s: %d` |
| `0x0041f66a` | `FUN_0041f66a` | 832 | FUN_004262f2, FUN_00476399, FUN_004893b5, FUN_0040fb2c, F... | `d29.blv, restmain, restb1, restb2, restb3, restb4, reste... |
| `0x0041f9aa` | `FUN_0041f9aa` | 1136 | _memset, FUN_0040f9d1, FUN_0041518b, FUN_0040df03, FUN_00... | `hglas%03d, 408%d, %d:%02d %s, %s190%d` |
| `0x0041fe1a` | `FUN_0041fe1a` | 243 | FUN_0040f9d1, FUN_0040fb2c, FUN_0040f788 |  |
| `0x0041ff4b` | `FUN_0041ff4b` | 412 | FUN_0040f9d1, FUN_004948a9, FUN_0040fb2c, FUN_0041fe1a, F... |  |
| `0x004200e7` | `FUN_004200e7` | 326 | _rand, FUN_0040f9d1, FUN_00456d51, FUN_0040fb2c, FUN_0040... |  |
| `0x0042022d` | `FUN_0042022d` | 265 | _memset, _rand, FUN_004a0e10, FUN_004200e7, FUN_0041fe1a |  |
| `0x004203c7` | `FUN_004203c7` | 1295 | __ftol, _rand, FUN_0049aba0, FUN_004948a9, FUN_004547cf (... |  |
| `0x004208d6` | `FUN_004208d6` | 486 | FUN_0040fb2c, FUN_004cad70, FUN_004a0e10, FUN_0040f936, F... | `chest%02d` |
| `0x00420abc` | `FUN_00420abc` | 242 | FUN_0040f9d1, FUN_0040fb2c, FUN_00402f07, FUN_0040f788 |  |
| `0x00420bae` | `FUN_00420bae` | 508 | _strcmp, FUN_00476399, FUN_004cad70, FUN_004aa29b, FUN_00... |  |
| `0x00420daa` | `FUN_00420daa` | 254 | FUN_00420abc, FUN_00404828, FUN_00469c3d, FUN_00420bae, F... |  |
| `0x00420ea8` | `FUN_00420ea8` | 1831 | _strncpy, FUN_004564c5, FUN_00469864, FUN_004cad70, FUN_0... |  |
| `0x004215cf` | `FUN_004215cf` | 920 | FUN_004262f2, FUN_0043bcca, FUN_004190a9, FUN_0041d0d8, F... |  |
| `0x00421967` | `FUN_00421967` | 366 | FUN_004262f2, FUN_0043bcca, FUN_0041d0d8, FUN_004ab69f, F... |  |
| `0x00421e4f` | `FUN_00421e4f` | 662 | FUN_00469864, FUN_0049281e, FUN_00469c3d, FUN_00469907, F... | `MICON1` |
| `0x004220e5` | `FUN_004220e5` | 1414 | GetAsyncKeyState, FUN_0040f9d1, FUN_00420bae, FUN_0046a0a... |  |
| `0x00423b06` | `FUN_00423b06` | 2588 | FUN_0043668c, FUN_004ca62e |  |
| `0x00424522` | `FUN_00424522` | 688 | FUN_00424c80 |  |
| `0x004247d2` | `FUN_004247d2` | 1198 |  |  |
| `0x0042547b` | `FUN_0042547b` | 3384 |  |  |
| `0x004262c0` | `FUN_004262c0` | 50 | timeGetTime |  |
| `0x00426676` | `FUN_00426676` | 136 | _malloc, FUN_0042683b |  |
| `0x004266fe` | `FUN_004266fe` | 317 | _strncpy, _malloc, FUN_00466b90, FUN_004cc13a, FUN_004264f5 | `Id: %s  Size: %i\n, Memory` |
| `0x0042683b` | `FUN_0042683b` | 53 | GetSystemInfo |  |
| `0x0042688c` | `FUN_0042688c` | 42 |  |  |
| `0x004268b6` | `FUN_004268b6` | 30 |  |  |
| `0x0042694b` | `FUN_0042694b` | 184 | FUN_0049130f |  |
| `0x00426a03` | `FUN_00426a03` | 964 | _rand, FUN_004596a0, FUN_00456d51, FUN_004cad70, FUN_0045... |  |
| `0x00426dc7` | `FUN_00426dc7` | 571 | _rand, FUN_00490101 |  |
| `0x00427002` | `FUN_00427002` | 183 | _rand, FUN_0044fd55, FUN_004270b9 |  |
| `0x004272ac` | `FUN_004272ac` | 198 | _rand, FUN_0048d09f, FUN_0048ccdb |  |
| `0x00427372` | `FUN_00427372` | 242 | _rand, FUN_004585be |  |
| `0x00427464` | `FUN_00427464` | 153 | _rand, FUN_0048e687, FUN_004585be |  |
| `0x00427522` | `FUN_00427522` | 203 | _rand |  |
| `0x00427619` | `FUN_00427619` | 162 | _rand |  |
| `0x00427734` | `FUN_00427734` | 1339 | FUN_0045827d, FUN_00427d57, FUN_0041d0d8, FUN_00469907, F... | `MICON1` |
| `0x00427db8` | `FUN_00427db8` | 27569 | __ftol, _memset, _strcmp, _rand, FUN_00402f27 (+74 more) | `d05.blv, (%s), and %d gold, %d gold, nothing, spell96` |
| `0x0042eb1e` | `FUN_0042eb1e` | 40 |  |  |
| `0x0042eb54` | `FUN_0042eb54` | 21 |  |  |
| `0x0042eb9a` | `FUN_0042eb9a` | 29 |  |  |
| `0x0042ec91` | `FUN_0042ec91` | 1149 | __ftol, FUN_004948a9, FUN_00452aca, FUN_00492c03, FUN_004... | `\n\n\n\nxxxx, 33333sy@` |
| `0x0042f160` | `FUN_0042f160` | 594 | FUN_00438bce, FUN_004c18b6, FUN_0040104c |  |
| `0x0042f4b6` | `FUN_0042f4b6` | 275 | FUN_004ca62e, FUN_0040104c |  |
| `0x0042f5c9` | `FUN_0042f5c9` | 510 | FUN_00402cae, FUN_0043aabc |  |
| `0x0042f7c7` | `FUN_0042f7c7` | 328 | _rand, FUN_0049aba0, FUN_0042f5c9, FUN_00404828, FUN_0040... |  |
| `0x0042fc2a` | `FUN_0042fc2a` | 2100 | __ftol, GetKeyState, GetAsyncKeyState, FUN_0043ad57, FUN_... |  |
| `0x004304d6` | `FUN_004304d6` | 19695 | __ftol, _memset, _strcmp, GetAsyncKeyState, __allmul (+14... | `ControlBG, con_16x, con_32x, con_ArrL, con_ArrR, con_Smo... |
| `0x00435683` | `FUN_00435683` | 37 |  |  |
| `0x00436444` | `FUN_00436444` | 58 | FUN_00438247, FUN_0043647e |  |
| `0x0043668c` | `FUN_0043668c` | 661 |  |  |
| `0x00436a89` | `FUN_00436a89` | 285 |  |  |
| `0x00437274` | `FUN_00437274` | 240 | FUN_00498584, FUN_004cac3e |  |
| `0x00437365` | `FUN_00437365` | 370 |  |  |
| `0x004378f5` | `FUN_004378f5` | 231 | FUN_004379dd, FUN_004cac3e |  |
| `0x00437d39` | `FUN_00437d39` | 1015 | FUN_00436512, FUN_00466cc6, FUN_00437274, FUN_00436ba6 | `D:\mm7Src_eng\MM7\Code\Core3D.cpp` |
| `0x00438247` | `FUN_00438247` | 72 |  |  |
| `0x004382ab` | `FUN_004382ab` | 305 | FUN_00438469 |  |
| `0x004383dc` | `FUN_004383dc` | 141 |  | `------------, GenuineIntel` |
| `0x00438469` | `FUN_00438469` | 144 |  | `------------, GenuineIntel` |
| `0x004385a4` | `FUN_004385a4` | 164 | FUN_00438648 |  |
| `0x00438648` | `FUN_00438648` | 456 | _strcmp, FUN_004cac80, FUN_00438981, FUN_00438a56, FUN_00... | `GenuineIntel, AuthenticAMD, CyrixInstead, CentaurHauls` |
| `0x00438810` | `FUN_00438810` | 369 |  |  |
| `0x00438981` | `FUN_00438981` | 213 |  |  |
| `0x00438a56` | `FUN_00438a56` | 136 |  |  |
| `0x00438b8a` | `FUN_00438b8a` | 68 |  |  |
| `0x00438ce2` | `FUN_00438ce2` | 322 | FUN_00438b8a, FUN_00449ba1, FUN_0047752f, FUN_00449b7a |  |
| `0x00438e24` | `FUN_00438e24` | 346 | _rand, FUN_004948a9, FUN_004547cf, FUN_004ca62e, FUN_0048... |  |
| `0x00438f7e` | `FUN_00438f7e` | 1253 | FUN_00407a1c, FUN_0040894b, FUN_00439fee, FUN_0043b1d3, F... |  |
| `0x00439463` | `FUN_00439463` | 2483 | __ftol, _rand, FUN_0045827d, FUN_0048d65c, FUN_00439e16 (... | `D:\mm7Src_eng\MM7\Code\Damage.cpp` |
| `0x00439fee` | `FUN_00439fee` | 2483 | __ftol, _rand, FUN_0045827d, FUN_0049b419, FUN_00492c03 (... |  |
| `0x0043a9a1` | `FUN_0043a9a1` | 283 | FUN_00439fee, FUN_0043b1d3, FUN_0043b07a, FUN_00439463, F... |  |
| `0x0043aabc` | `FUN_0043aabc` | 200 | FUN_00402cae |  |
| `0x0043ab84` | `FUN_0043ab84` | 79 | FUN_00452aca |  |
| `0x0043b006` | `FUN_0043b006` | 116 | FUN_00452b5a |  |
| `0x0043b07a` | `FUN_0043b07a` | 345 | FUN_00402d6e, FUN_0043b006, FUN_00438bce, FUN_00427522, F... |  |
| `0x0043b1d3` | `FUN_0043b1d3` | 560 | FUN_00427372, FUN_00402d6e, FUN_00438bce, FUN_00427522, F... |  |
| `0x0043b403` | `FUN_0043b403` | 256 | _rand, FUN_0045827d, FUN_0043b006 |  |
| `0x0043b593` | `FUN_0043b593` | 128 | FUN_004262c0 |  |
| `0x0043b712` | `FUN_0043b712` | 98 |  |  |
| `0x0043bbac` | `FUN_0043bbac` | 216 | FUN_00466cc6 | `D:\mm7Src_eng\MM7\Code\DirectInputMouse.cpp` |
| `0x0043cc9f` | `FUN_0043cc9f` | 7081 | __ftol, GetTickCount, FUN_004a6376, FUN_0048d612, FUN_004... | `sptext01, item64v1, sp91a, sp30a, sp28a, pending, Pending` |
| `0x0043e848` | `FUN_0043e848` | 1354 | __ftol, GetTickCount, FUN_004a5fae, FUN_004a6376, FUN_004... | `sptext01, sp91a, sp30a, sp28a` |
| `0x0043ee9a` | `FUN_0043ee9a` | 124 | FUN_0043ee38 |  |
| `0x0043ef16` | `FUN_0043ef16` | 56 |  |  |
| `0x0043f3c1` | `FUN_0043f3c1` | 375 | FUN_0043fa56, FUN_004407fc, FUN_0043f538, FUN_0043fe10, F... |  |
| `0x0043f5eb` | `FUN_0043f5eb` | 907 | __ftol, FUN_004ca62e |  |
| `0x0043fa56` | `FUN_0043fa56` | 954 | _memset, _rand, FUN_0045284a, FUN_0043668c, FUN_0040fb2c ... | `effpar01` |
| `0x0043fe10` | `FUN_0043fe10` | 1186 | FUN_0045284a, FUN_0043668c, FUN_004ca62e, FUN_0044d94b, F... |  |
| `0x004402b2` | `FUN_004402b2` | 938 | FUN_0045284a, FUN_0043668c, FUN_00436a89, FUN_00467d8c, F... |  |
| `0x004407fc` | `FUN_004407fc` | 847 | __ftol, FUN_0049aba0, FUN_00402cae, FUN_004cbef4, FUN_004... |  |
| `0x00440cdb` | `FUN_00440cdb` | 317 | FUN_004a4341, FUN_0041f4b6, FUN_004acb9b |  |
| `0x0044105f` | `FUN_0044105f` | 1116 | FUN_00402cae, FUN_00469e3f, FUN_0041b35f, FUN_00441d5b, F... |  |
| `0x004414bb` | `FUN_004414bb` | 247 | FUN_004a6204, FUN_00494f86, FUN_004262c0 |  |
| `0x00441650` | `FUN_00441650` | 644 | __ftol, GetTickCount, FUN_004a5e42, FUN_004a6204, FUN_004... |  |
| `0x00441987` | `FUN_00441987` | 234 | FUN_004a5e42, FUN_004a6204, FUN_00494f86 |  |
| `0x00441a71` | `FUN_00441a71` | 390 | FUN_0041f4b6, FUN_0044d8fc, FUN_004ad234 |  |
| `0x00441d5b` | `FUN_00441d5b` | 3101 | FUN_00476399, FUN_0040df03, FUN_004ca62e, FUN_004a5b11, F... |  |
| `0x00442978` | `FUN_00442978` | 2244 | __ftol, FUN_0047f050, FUN_004cc590, FUN_0040df03, FUN_004... |  |
| `0x0044326c` | `FUN_0044326c` | 72 | FUN_0044330a |  |
| `0x004432b4` | `FUN_004432b4` | 86 | FUN_0044330a |  |
| `0x00443693` | `FUN_00443693` | 373 | __ftol, FUN_0049fcca, FUN_004a6204, FUN_004a5e42, FUN_004... |  |
| `0x00443dc4` | `FUN_00443dc4` | 144 | _memset, FUN_00443d04 | `global.evt` |
| `0x00443f1b` | `FUN_00443f1b` | 157 | _memset |  |
| `0x00443fff` | `FUN_00443fff` | 900 | __allmul, __ftol, __allrem, FUN_004a99f7, FUN_0044686d |  |
| `0x00444755` | `FUN_00444755` | 222 |  |  |
| `0x00444a74` | `FUN_00444a74` | 574 | FUN_00444833, FUN_004547cf, FUN_004cad70, FUN_0044d432, F... | `D:\mm7Src_eng\MM7\Code\Events.cpp, No transition text fo... |
| `0x00444ded` | `FUN_00444ded` | 500 | FUN_004547cf, FUN_004cad70, FUN_004cac90, FUN_00444da3, F... |  |
| `0x00444fe1` | `FUN_00444fe1` | 490 | _memset, FUN_00459f0a, FUN_004a5e42, FUN_0044c54a, FUN_00... | `%s %s` |
| `0x00445373` | `FUN_00445373` | 1705 | FUN_0040df03, FUN_00495477, FUN_00445cae, FUN_0041cce4, F... |  |
| `0x00445a1c` | `FUN_00445a1c` | 307 | _strcmp, FUN_004ca62e, FUN_0040e2d4, FUN_00466d0d | `D:\mm7Src_eng\MM7\Code\Events.cpp, NPC id exceeds MAX_DA... |
| `0x00445b4f` | `FUN_00445b4f` | 351 | _strcmp, FUN_004ca62e, FUN_0040e2d4, FUN_00466d0d | `D:\mm7Src_eng\MM7\Code\Events.cpp, NPC id exceeds MAX_DA... |
| `0x00445d6d` | `FUN_00445d6d` | 755 | FUN_004262f2, FUN_0044c2b7, FUN_004948a9, FUN_0040fb2c, F... | `evt%02d, npc%03u` |
| `0x0044608d` | `FUN_0044608d` | 431 | _memset, FUN_0040fb2c, FUN_004cad70 | `npc%03u` |
| `0x00446251` | `FUN_00446251` | 945 | _memset, FUN_00459f0a, FUN_0044c2b7, FUN_004948a9, FUN_00... | `evt%02d` |
| `0x0044686d` | `FUN_0044686d` | 7134 | _memset, _strlen, _rand, FUN_0040f9d1, FUN_0049fcca (+55 ... | `arbiter good, arbiter evil, pcout01` |
| `0x00448641` | `FUN_00448641` | 529 | FUN_0044e18f, FUN_0040fb2c, FUN_0044e119, FUN_0048a3a2 |  |
| `0x004488d9` | `FUN_004488d9` | 65 | FUN_004caaf0 |  |
| `0x00448951` | `FUN_00448951` | 233 |  |  |
| `0x00448a63` | `FUN_00448a63` | 88 |  |  |
| `0x00448abb` | `FUN_00448abb` | 173 | FUN_004597a6 |  |
| `0x00448b8a` | `FUN_00448b8a` | 397 | __allmul, __ftol, __allshr, FUN_0044686d |  |
| `0x00448e1b` | `FUN_00448e1b` | 2605 | __ftol, FUN_0049aba0, FUN_0045284a, FUN_0042f5c9, FUN_004... |  |
| `0x00449b7a` | `FUN_00449b7a` | 39 |  |  |
| `0x00449bd7` | `FUN_00449bd7` | 2070 | __ftol, __alldiv, __allrem, FUN_0048e687, FUN_0048e724 (+... |  |
| `0x0044b01e` | `FUN_0044b01e` | 2294 | _memset, _rand, FUN_0048e55d, FUN_004948a9, FUN_00420bae ... |  |
| `0x0044b9f0` | `FUN_0044b9f0` | 1646 | _memset, _rand, FUN_004948a9, FUN_00492bae, FUN_004698aa ... |  |
| `0x0044c474` | `FUN_0044c474` | 150 | FUN_0040fb2c, FUN_00466be9, FUN_004cad70, FUN_00410897 | `Unable to open %s` |
| `0x0044c54a` | `FUN_0044c54a` | 127 | _strlen, FUN_0044c50a |  |
| `0x0044c5c9` | `FUN_0044c5c9` | 145 | _strlen, FUN_0044c50a, FUN_0044c794 |  |
| `0x0044c65a` | `FUN_0044c65a` | 148 | _strlen, FUN_0044c95f, FUN_0044c50a |  |
| `0x0044c6ee` | `FUN_0044c6ee` | 166 | _strlen, FUN_0044c50a, FUN_0044c794 |  |
| `0x0044c794` | `FUN_0044c794` | 459 | _strlen, _strncpy, FUN_004cac80, FUN_0040e2d4, FUN_004cbb... | `D:\mm7Src_eng\MM7\Code\Font.cpp, Invalid string passed !` |
| `0x0044c95f` | `FUN_0044c95f` | 540 | _strlen, _strncpy, FUN_004cac80, FUN_0040e2d4, FUN_004cbb... | `D:\mm7Src_eng\MM7\Code\Font.cpp, Invalid string passed !` |
| `0x0044cb7b` | `FUN_0044cb7b` | 697 | _strlen, _strncpy, __strrev, FUN_004a6bdf, FUN_004cac80 (... |  |
| `0x0044ce34` | `FUN_0044ce34` | 685 | _strlen, _strncpy, _strcmp, FUN_004a6bdf, FUN_0040e2d4 (+... | `D:\mm7Src_eng\MM7\Code\Font.cpp, Invalid string passed!` |
| `0x0044d0e1` | `FUN_0044d0e1` | 306 | _strlen, _strncpy, FUN_0044c50a, FUN_0040f851, FUN_004cbb55 |  |
| `0x0044d213` | `FUN_0044d213` | 278 | _strlen, _strncpy, FUN_004a6bdf, FUN_0044c50a, FUN_004cbb55 |  |
| `0x0044d329` | `FUN_0044d329` | 265 | FUN_0044c95f, FUN_0044c54a, FUN_004cc17b, FUN_0044d0e1 |  |
| `0x0044d432` | `FUN_0044d432` | 152 | FUN_0044c54a, FUN_004cc17b, FUN_0044c794, FUN_0044d213 |  |
| `0x0044d53f` | `FUN_0044d53f` | 704 | _strlen, FUN_004cac80, FUN_004cad70, FUN_0048a3a2, FUN_00... |  |
| `0x0044dabe` | `FUN_0044dabe` | 1474 | __ftol, _strchr, FUN_00466be9, FUN_004ca62e, FUN_004cc5bf... | `CSpriteFrameTable::load - Unable to open file: %s., S Fr... |
| `0x0044e119` | `FUN_0044e119` | 118 | FUN_0048a3a2, FUN_0040fb2c |  |
| `0x0044e930` | `FUN_0044e930` | 275 | FUN_004262c0 |  |
| `0x0044eb86` | `FUN_0044eb86` | 198 | FUN_0040e2d4, FUN_00466d0d, FUN_00467838 | `D:\mm7Src_eng\MM7\Code\Game.cpp, Undefined CObjectInfo t... |
| `0x0044ec4f` | `FUN_0044ec4f` | 231 |  |  |
| `0x0044ed36` | `FUN_0044ed36` | 218 |  |  |
| `0x0044eed3` | `FUN_0044eed3` | 467 | FUN_0046381d, FUN_0048ac4b, FUN_0044ea8a, FUN_00469c3d, F... |  |
| `0x0044f104` | `FUN_0044f104` | 37 |  |  |
| `0x0044f14c` | `FUN_0044f14c` | 114 | __ftol, FUN_004ad32b |  |
| `0x0044f2de` | `FUN_0044f2de` | 114 | _memset, FUN_00466cc6 | `D:\mm7Src_eng\MM7\Code\GammaControl.cpp` |
| `0x0044f5a8` | `FUN_0044f5a8` | 1116 | _rand, FUN_004596aa, FUN_0045642b, FUN_0040e2d4, FUN_0046... | `Can't create random monster: '%s'! See MapStats.txt and ... |
| `0x0044fa34` | `FUN_0044fa34` | 68 | FUN_004caaf0 |  |
| `0x0044fa78` | `FUN_0044fa78` | 733 | _rand, __ftol, FUN_004596aa, FUN_004597a6, FUN_004ca62e (... | `Elemental Light C, Elemental Light B, Elemental Light A` |
| `0x0044fd55` | `FUN_0044fd55` | 687 | _rand, __ftol, FUN_004596aa, FUN_004ca780, FUN_004597a6 (... |  |
| `0x00450004` | `FUN_00450004` | 576 | _rand, FUN_00404828, FUN_004505f8, FUN_00402f07, FUN_0045... |  |
| `0x00450244` | `FUN_00450244` | 777 | _rand, FUN_004505f8, FUN_004547cf, FUN_00402f07, FUN_004c... |  |
| `0x00450667` | `FUN_00450667` | 750 | __ftol, _rand, FUN_0040894b, FUN_00448df9, FUN_0046d4a2 (... |  |
| `0x00450dcf` | `FUN_00450dcf` | 31 |  |  |
| `0x00450e1d` | `FUN_00450e1d` | 356 | FUN_004ca780 |  |
| `0x00450f81` | `FUN_00450f81` | 92 |  |  |
| `0x00450fdd` | `FUN_00450fdd` | 86 |  |  |
| `0x00451033` | `FUN_00451033` | 4900 | FUN_00450fdd, FUN_00450f81 |  |
| `0x0045246e` | `FUN_0045246e` | 150 |  |  |
| `0x00452504` | `FUN_00452504` | 583 | operator_new, FUN_004cb7ec, FUN_004ca2cd, FUN_004c2f60, F... |  |
| `0x00452aca` | `FUN_00452aca` | 68 |  |  |
| `0x00452b1f` | `FUN_00452b1f` | 59 |  |  |
| `0x00454ce0` | `FUN_00454ce0` | 201 | FUN_004cc91e |  |
| `0x0045504a` | `FUN_0045504a` | 4904 | _strlen, _strstr, FUN_00454ce0, FUN_004cc91e, FUN_0040e2d... | `monsters.txt, WEAPON, ARMOR, SWORD, DAGGER, SPEAR, STAFF... |
| `0x0045646e` | `FUN_0045646e` | 87 | FUN_00456d98 |  |
| `0x004564df` | `FUN_004564df` | 365 | _strlen, FUN_00456d98, FUN_004cad70, FUN_004cac90 | `%s %s` |
| `0x0045664c` | `FUN_0045664c` | 1689 | _memset, _rand, FUN_00456d51, FUN_00449b7a, FUN_004266fe | `newItemGen` |
| `0x00456d51` | `FUN_00456d51` | 44 |  |  |
| `0x0045827d` | `FUN_0045827d` | 28 |  |  |
| `0x00458299` | `FUN_00458299` | 43 |  |  |
| `0x00458519` | `FUN_00458519` | 127 | FUN_00458598 |  |
| `0x00458603` | `FUN_00458603` | 54 |  |  |
| `0x004597a6` | `FUN_004597a6` | 167 |  |  |
| `0x00459935` | `FUN_00459935` | 777 | _strchr, FUN_004cb7ec, FUN_00494ae5, FUN_004be3e8, FUN_00... | `MonsterRaceListStruct::load - Unable to open file: %s., ... |
| `0x00459f49` | `FUN_00459f49` | 236 | FUN_00459f0a |  |
| `0x0045a035` | `FUN_0045a035` | 2404 | _strcmp, FUN_0045ac03, FUN_00464a2c, FUN_00464c2c | `DEFAULT, KEY_FORWARD, KEY_BACKWARD, KEY_LEFT, KEY_RIGHT,... |
| `0x0045ac03` | `FUN_0045ac03` | 610 | _strlen, _strcmp | `RIGHT, RETURN, SPACE, PAGE_DOWN, PAGE_UP, DELETE, INSERT... |
| `0x0045b4d8` | `FUN_0045b4d8` | 77 | EnterCriticalSection, LeaveCriticalSection, FUN_0045b5b6 |  |
| `0x0045b569` | `FUN_0045b569` | 77 | EnterCriticalSection, LeaveCriticalSection, FUN_0045b5b6 |  |
| `0x0045baef` | `FUN_0045baef` | 51 | _strlen, FUN_0040e33a, FUN_0040e302 | `effpar03` |
| `0x0045bc40` | `FUN_0045bc40` | 628 | FUN_0040e2d4, FUN_00466d0d, FUN_00489b46, FUN_00436921, F... | `D:\mm7Src_eng\MM7\Code\Light.cpp, Error: Failed to get t... |
| `0x0045bebf` | `FUN_0045bebf` | 1587 | __ftol, FUN_00436ba6, FUN_0040e2d4, FUN_00466d0d, FUN_004... | `D:\mm7Src_eng\MM7\Code\Light.cpp, Invalid light type!, U... |
| `0x0045c94a` | `FUN_0045c94a` | 375 | FUN_004ca62e |  |
| `0x0045cac1` | `FUN_0045cac1` | 257 | FUN_0045cc45, FUN_0045cc0d |  |
| `0x0045cc45` | `FUN_0045cc45` | 427 | FUN_0040e2d4, FUN_00466d0d, FUN_004369ca, FUN_004ca62e, F... | `D:\mm7Src_eng\MM7\Code\Light.cpp, Invalid light type det... |
| `0x0045cdf0` | `FUN_0045cdf0` | 153 | FUN_0045ce89 |  |
| `0x0045ce89` | `FUN_0045ce89` | 486 | FUN_004ca62e |  |
| `0x0045d10e` | `FUN_0045d10e` | 754 | FUN_0040e2d4, FUN_00466d0d, FUN_004ca62e, FUN_00436a5c, F... | `D:\mm7Src_eng\MM7\Code\Light.cpp, Uknown strip type dete... |
| `0x0045d45f` | `FUN_0045d45f` | 541 | FUN_0045d67c |  |
| `0x0045d6d1` | `FUN_0045d6d1` | 167 | FUN_004379dd |  |
| `0x0045db21` | `FUN_0045db21` | 449 | FUN_00466cc6 | `D:\mm7Src_eng\MM7\Code\Light.cpp` |
| `0x0045e073` | `FUN_0045e073` | 562 | operator_new, _memset, __ftol, FUN_00441d22, FUN_004a1e46... | `D:\mm7Src_eng\MM7\Code\LoadSave.cpp` |
| `0x0045e39a` | `FUN_0045e39a` | 1501 | _memset, FUN_00461a4e, FUN_004a5b73, FUN_0049fbc7, FUN_00... | `loadsave, load_up, save_up, LS_loadU, LS_saveU, saves\%s... |
| `0x0045e977` | `FUN_0045e977` | 1356 | _memset, FUN_00461a4e, FUN_0049fbc7, FUN_0044ce34, FUN_00... | `loadsave, load_up, save_up, LS_loadU, LS_saveU, 1.mm7, s... |
| `0x0045eec3` | `FUN_0045eec3` | 1503 | __ftol, CopyFileA, GetLastError, FUN_004aa29b, FUN_0041c2... | `data\lloyd%d%d.pcx, saves\%s, data\new.lod, header.bin, ... |
| `0x004601f6` | `FUN_004601f6` | 1344 | __allrem, _memset, __ftol, __alldiv, FUN_004a5b73 (+13 more) | `%s %d:%02d%s\n%d %s %d` |
| `0x00460ab5` | `FUN_00460ab5` | 1586 | __ftol, _rand, FUN_00498d93, FUN_00443388, FUN_00464876 (... | `out15.odm, d23.blv, Unable to open %s, File %s is not a ... |
| `0x00461140` | `FUN_00461140` | 660 | FUN_004a99f7, FUN_0048a8b2, FUN_004698dc, FUN_00450244, F... |  |
| `0x00461f5b` | `FUN_00461f5b` | 83 | FUN_004cb656 | `lodapp.tmp` |
| `0x004627f4` | `FUN_004627f4` | 1245 | PeekMessageA, WaitMessage, TranslateMessage, SetForegroun... | `title_new, title_load, title_cred, title_exit, title.pcx... |
| `0x00462cd1` | `FUN_00462cd1` | 1205 | FindWindowA, __ftol, _AIL_redbook_play@12, SetWindowPos, ... | `-usedefs, -window, -nosound, -noanim, Ran once` |
| `0x00463828` | `FUN_00463828` | 3144 | BeginPaint, SetWindowPos, EndPaint, _BinkBufferSetOffset@... | `You must be running in 256 color mode or higher. You can... |
| `0x00464637` | `FUN_00464637` | 246 | ClipCursor, GetWindowRect, FUN_00464d6f, FUN_00461a4e, FU... | `startinwindow, window X, window Y, debug flags, valAlway... |
| `0x00464876` | `FUN_00464876` | 24 | FUN_004caaf0 | `out15.odm` |
| `0x004648a3` | `FUN_004648a3` | 393 | __strrev, FUN_00443f1b, FUN_004abfb9, FUN_00460ab5, FUN_0... | `out15.odm, d11.blv, d10.blv` |
| `0x00464e54` | `FUN_00464e54` | 260 | wsprintfA, mciSendStringA, FUN_004cb7ec, FUN_004cb8a5, FU... | `X:\anims\magic7.vid, open %c: type cdaudio alias CD, inf... |
| `0x004650b2` | `FUN_004650b2` | 402 | RegOpenKeyExA, DialogBoxParamA, GetDriveTypeA, GetLogical... | `SOFTWARE, New World Computing, Might and Magic VII, CDDr... |
| `0x00465245` | `FUN_00465245` | 2539 | CreateWindowExA, GetDiskFreeSpaceA, SetWindowLongA, Messa... | `MM7_ICON, window X, window Y, debug flags, %s\mm6.ini, M... |
| `0x00465d0f` | `FUN_00465d0f` | 591 | FUN_004ccc14, FUN_004cd09d, FUN_00465ff4, FUN_00460869, F... | `glow03, glow05, HDWTR%03u, Saves, data\lloyd%d%d.pcx` |
| `0x004667ed` | `FUN_004667ed` | 931 | ShowWindow, SetMenu, SetWindowLongA, InvalidateRect, Clip... | `window X, window Y, makeme.pcx, title.pcx, lsave640.pcx` |
| `0x00466d87` | `FUN_00466d87` | 316 | MessageBoxA, SetWindowPos, ClipCursor, FUN_004678e6, FUN_... |  |
| `0x004679e6` | `FUN_004679e6` | 205 | FUN_00467b3c, FUN_0040e302, FUN_004ca780, FUN_0040e36f, F... |  |
| `0x00467ab3` | `FUN_00467ab3` | 137 | FUN_004ca2d8, FUN_004ca780, FUN_0040e36f, FUN_004ca504 |  |
| `0x00467b3c` | `FUN_00467b3c` | 103 | FUN_00467ba3, FUN_004cd160, FUN_0040e36f, FUN_004ca504 |  |
| `0x00467cd5` | `FUN_00467cd5` | 88 | FUN_004ca2d8, FUN_004ca780, FUN_0040e36f |  |
| `0x00467d8c` | `FUN_00467d8c` | 170 | FUN_0040e2d4, FUN_00467838 | `D:\mm7Src_eng\MM7\Code\MobileLightStack.cpp, Too many mo... |
| `0x00467e83` | `FUN_00467e83` | 201 | FUN_004936d9, FUN_00402f07, FUN_004698aa, FUN_004925de |  |
| `0x00467fba` | `FUN_00467fba` | 311 | FUN_0041555c, FUN_0044ce34, FUN_0040df03, FUN_0044d432, F... |  |
| `0x004680f1` | `FUN_004680f1` | 3485 | _memset, _rand, __ftol, FUN_004aa29b, FUN_0041c213 (+22 m... | `+%u %s, +%u %s %s` |
| `0x00468f8e` | `FUN_00468f8e` | 2229 | FUN_004aa29b, FUN_004948a9, FUN_00402f07, FUN_0043f0e0, F... | `\n\n\n\nxxxx, MICON1` |
| `0x00469c11` | `FUN_00469c11` | 44 | _malloc |  |
| `0x00469ea8` | `FUN_00469ea8` | 476 | operator_new, FUN_004a6706, FUN_004a687f, FUN_004a6204, F... |  |
| `0x0046a14f` | `FUN_0046a14f` | 489 | GetAsyncKeyState, FUN_004c1b63, FUN_0044eb3e, FUN_0046a338 |  |
| `0x0046a338` | `FUN_0046a338` | 888 | FUN_0044c1fc, FUN_00449ba1, FUN_00404030, FUN_0040e2d4, F... | `D:\mm7Src_eng\MM7\Code\Mouse.cpp, Warning: Invalid ID re... |
| `0x0046a6b0` | `FUN_0046a6b0` | 284 | FUN_0046a7cc |  |
| `0x0046a7cc` | `FUN_0046a7cc` | 214 | FUN_004a1e2d, FUN_004a1e33, FUN_004c18a2 |  |
| `0x0046a8a2` | `FUN_0046a8a2` | 253 | FUN_004ca62e |  |
| `0x0046a99f` | `FUN_0046a99f` | 657 | FUN_0044c320, FUN_0041c061, FUN_00444755, FUN_004564c5 |  |
| `0x0046ac5d` | `FUN_0046ac5d` | 51 | _strlen, FUN_0040e33a, FUN_0040e302 | `micon1` |
| `0x0046ade6` | `FUN_0046ade6` | 140 | TerminateThread, SetWindowPos, FUN_0046bc77, FUN_004cb450 |  |
| `0x0046b1f0` | `FUN_0046b1f0` | 157 | FUN_004a0ed0 |  |
| `0x0046b424` | `FUN_0046b424` | 114 | FUN_0046ba91, FUN_0046b95c, FUN_0046b380, FUN_0046b496, F... |  |
| `0x0046b496` | `FUN_0046b496` | 287 | EnterCriticalSection, LeaveCriticalSection, FUN_004cb450,... |  |
| `0x0046b773` | `FUN_0046b773` | 266 | EnterCriticalSection, LeaveCriticalSection, FUN_0041ccf8,... |  |
| `0x0046b87d` | `FUN_0046b87d` | 203 | EnterCriticalSection, LeaveCriticalSection, FUN_0041786d,... |  |
| `0x0046bef5` | `FUN_0046bef5` | 265 | __ftol, FUN_0040894b, FUN_00458519, FUN_00427619 |  |
| `0x0046bffe` | `FUN_0046bffe` | 3090 | __ftol, FUN_004aa29b, FUN_00440eb4, FUN_004597a6, FUN_004... |  |
| `0x0046cc4f` | `FUN_0046cc4f` | 632 | FUN_0044686d, FUN_004ca62e |  |
| `0x0046cec7` | `FUN_0046cec7` | 1499 |  |  |
| `0x0046d4a2` | `FUN_0046d4a2` | 1093 | FUN_0048257e |  |
| `0x0046d8e7` | `FUN_0046d8e7` | 997 |  |  |
| `0x0046df1e` | `FUN_0046df1e` | 408 | FUN_00452aca, FUN_004ca62e |  |
| `0x0046e0b6` | `FUN_0046e0b6` | 443 | FUN_00452aca, FUN_004ca62e |  |
| `0x0046e271` | `FUN_0046e271` | 481 | FUN_00452aca, FUN_004ca62e |  |
| `0x0046e88d` | `FUN_0046e88d` | 1145 | FUN_0046ed06, FUN_004754c3, FUN_00475f34 |  |
| `0x0046ed8e` | `FUN_0046ed8e` | 375 | FUN_0046def6, FUN_004ca62e |  |
| `0x0046ef05` | `FUN_0046ef05` | 333 | FUN_00452aca, FUN_004ca62e |  |
| `0x0046f22c` | `FUN_0046f22c` | 1764 | FUN_004aa29b, FUN_00466be9, FUN_004ca62e, FUN_004989ca | `Door Error\ndoor id: %i\nfacet no: %i\n\nOverflow dividi... |
| `0x0046f910` | `FUN_0046f910` | 3070 | __ftol, FUN_0046e0b6, FUN_0046ef05, FUN_00404030, FUN_004... |  |
| `0x0047050e` | `FUN_0047050e` | 444 | FUN_00452aca |  |
| `0x004706ca` | `FUN_004706ca` | 3238 | __ftol, _rand, FUN_0046ef05, FUN_0042f93c, FUN_0047edb7 (... |  |
| `0x00471370` | `FUN_00471370` | 2199 | _memset, _rand, __ftol, FUN_0046e0b6, FUN_0046ef05 (+16 m... | `effpar01, effpar03` |
| `0x00471c07` | `FUN_00471c07` | 2363 | _memset, _rand, FUN_0042f93c, FUN_0046ef05, FUN_0046e271 ... | `effpar01, effpar03` |
| `0x00472542` | `FUN_00472542` | 494 | FUN_00471c07, FUN_0046bffe, FUN_0042f90f, FUN_00471370, F... |  |
| `0x0047286a` | `FUN_0047286a` | 4065 | __ftol, GetTickCount, FUN_00472730, FUN_004aa29b, FUN_004... |  |
| `0x00473897` | `FUN_00473897` | 6717 | __ftol, GetTickCount, FUN_004aa29b, FUN_0048e962, FUN_004... |  |
| `0x00475320` | `FUN_00475320` | 419 | FUN_00475669, FUN_004ca62e |  |
| `0x004754c3` | `FUN_004754c3` | 422 | FUN_004759cd, FUN_004ca62e |  |
| `0x00475669` | `FUN_00475669` | 868 |  |  |
| `0x004759cd` | `FUN_004759cd` | 956 |  |  |
| `0x00475d89` | `FUN_00475d89` | 427 | FUN_00475669, FUN_004ca62e |  |
| `0x00475f34` | `FUN_00475f34` | 401 | FUN_004759cd, FUN_004ca62e |  |
| `0x00477330` | `FUN_00477330` | 511 | __ftol, _rand, FUN_00477310 |  |
| `0x004775f1` | `FUN_004775f1` | 826 |  |  |
| `0x0047792b` | `FUN_0047792b` | 826 |  |  |
| `0x00478411` | `FUN_00478411` | 1479 | GetTickCount, FUN_00436ba6, FUN_00424c80, FUN_00481e59, F... | `D:\mm7Src_eng\MM7\Code\Odbuild.cpp, D3D version of Rende... |
| `0x004789e2` | `FUN_004789e2` | 1697 | FUN_00425291, FUN_00436ba6, FUN_0045cac1, FUN_00481e59, F... |  |
| `0x0047908d` | `FUN_0047908d` | 524 | FUN_004ca62e, FUN_00402cae, FUN_00462217 |  |
| `0x00479336` | `FUN_00479336` | 529 | FUN_0044e1c6 |  |
| `0x00479547` | `FUN_00479547` | 1296 | __ftol, FUN_004cc064, FUN_004a2d33, FUN_004cbfb4, FUN_004... |  |
| `0x00479a57` | `FUN_00479a57` | 2325 | __ftol, GetTickCount, FUN_004cc064, FUN_004a2e65, FUN_004... |  |
| `0x0047a388` | `FUN_0047a388` | 538 | __ftol, _strlen, FUN_004892cc, FUN_00450dcf, FUN_004a0e10... | `levels\` |
| `0x0047a5a2` | `FUN_0047a5a2` | 631 | FUN_004892cc, FUN_0043822f, FUN_00485f57, FUN_00487dad, F... |  |
| `0x0047a962` | `FUN_0047a962` | 1459 | _memset, _rand, FUN_0047a829, FUN_0044d8fc, FUN_0048ab09 ... | `effpar01` |
| `0x0047af15` | `FUN_0047af15` | 1307 | FUN_004a815a, FUN_0044d8fc, FUN_00467d8c, FUN_0045284a, F... |  |
| `0x0047b430` | `FUN_0047b430` | 1699 | FUN_004a7f04, FUN_0044d8fc, FUN_004a7fff, FUN_00467d8c, F... |  |
| `0x0047bad3` | `FUN_0047bad3` | 416 | GetTickCount, FUN_004acb9b, FUN_004a3fb3, FUN_0047bc73, F... |  |
| `0x0047beb5` | `FUN_0047beb5` | 711 |  |  |
| `0x0047c1ce` | `FUN_0047c1ce` | 130 | FUN_0044ec4f, FUN_0047be76, FUN_0047be6b, FUN_0041f4b6 |  |
| `0x0047c290` | `FUN_0047c290` | 130 | FUN_0044ec4f, FUN_0047c312, FUN_0047c343 |  |
| `0x0047c374` | `FUN_0047c374` | 103 | __ftol |  |
| `0x0047c500` | `FUN_0047c500` | 636 | FUN_0043f582 |  |
| `0x0047c80e` | `FUN_0047c80e` | 845 | __ftol, FUN_004ca654 |  |
| `0x0047cb5b` | `FUN_0047cb5b` | 395 | __ftol, FUN_0041f4b6, FUN_004ca62e |  |
| `0x0047d0aa` | `FUN_0047d0aa` | 7195 | _memset, __ftol, _rand, _malloc, _strlen (+33 more) | `levels\%s, grastyl, TerNorm, BDdata, IDLIST, Spawn, Unab... |
| `0x0047ed0c` | `FUN_0047ed0c` | 123 |  |  |
| `0x0047edb7` | `FUN_0047edb7` | 99 |  |  |
| `0x0047ee1a` | `FUN_0047ee1a` | 51 |  |  |
| `0x0047ee4d` | `FUN_0047ee4d` | 279 | FUN_0047ecc5 |  |
| `0x0047f0e6` | `FUN_0047f0e6` | 86 | FUN_0047cb5b |  |
| `0x0047f2d7` | `FUN_0047f2d7` | 279 | FUN_00450dcf, FUN_004597a6, FUN_0041f46a, FUN_004595d3, F... |  |
| `0x0047f5ca` | `FUN_0047f5ca` | 3432 | FUN_00480352, FUN_00436ba6, FUN_00481216, FUN_00436512, F... |  |
| `0x00480352` | `FUN_00480352` | 3639 | FUN_0049b052, FUN_00436ba6, FUN_00424c80, FUN_00481e59, F... |  |
| `0x00481216` | `FUN_00481216` | 2748 | FUN_00425291, FUN_0045cac1, FUN_004829bd, FUN_00481e59, F... |  |
| `0x00481db6` | `FUN_00481db6` | 163 | FUN_00486b52 |  |
| `0x00481fcd` | `FUN_00481fcd` | 423 | FUN_0048608d |  |
| `0x00482174` | `FUN_00482174` | 644 | FUN_0048608d |  |
| `0x0048257e` | `FUN_0048257e` | 501 | FUN_0047f47a, FUN_0047edb7, FUN_0047ee1a, FUN_0047f45c, F... |  |
| `0x00482a98` | `FUN_00482a98` | 883 | FUN_00485979, FUN_00485e23, FUN_004ca62e, FUN_004d6f50 |  |
| `0x00482e0b` | `FUN_00482e0b` | 2998 | FUN_00485979, FUN_00485e23, FUN_004d6ffa, FUN_00485b03, F... |  |
| `0x004839c1` | `FUN_004839c1` | 1741 | FUN_00485979, FUN_00485e23, FUN_004ca62e, FUN_00402cae, F... |  |
| `0x0048408e` | `FUN_0048408e` | 952 | FUN_00485979, FUN_00485e23, FUN_004d6ffa, FUN_004ca62e, F... |  |
| `0x00484446` | `FUN_00484446` | 937 | FUN_00485e23, FUN_00485d42, FUN_004d737f, FUN_00485c8d, F... |  |
| `0x004847ef` | `FUN_004847ef` | 2137 | FUN_00485979, FUN_00485e23, FUN_004d6ffa, FUN_004ca62e, F... |  |
| `0x00485048` | `FUN_00485048` | 963 | FUN_0047c17c, FUN_004ca62e, FUN_004cbef4 |  |
| `0x0048540b` | `FUN_0048540b` | 1109 | FUN_00485979, FUN_00485e23, FUN_004ca62e, FUN_00402cae, F... |  |
| `0x00485860` | `FUN_00485860` | 281 | FUN_0047c17c, FUN_00452b1f, FUN_004ca62e, FUN_00452b84 |  |
| `0x00485979` | `FUN_00485979` | 175 |  |  |
| `0x00485a28` | `FUN_00485a28` | 219 |  |  |
| `0x00485b03` | `FUN_00485b03` | 175 |  |  |
| `0x00485bb2` | `FUN_00485bb2` | 219 |  |  |
| `0x00485c8d` | `FUN_00485c8d` | 181 |  |  |
| `0x00485d42` | `FUN_00485d42` | 225 |  |  |
| `0x00485e23` | `FUN_00485e23` | 280 | FUN_0047c1ce, FUN_0047c290, FUN_0047beb5 |  |
| `0x00485f68` | `FUN_00485f68` | 165 | FUN_00486012 |  |
| `0x0048616f` | `FUN_0048616f` | 978 |  |  |
| `0x00486541` | `FUN_00486541` | 1038 | FUN_00402cae |  |
| `0x0048694f` | `FUN_0048694f` | 193 |  |  |
| `0x0048738d` | `FUN_0048738d` | 2592 | __ftol, GetTickCount, FUN_004cc064, FUN_0047edb7, FUN_004... | `wtrtyl, wtrtyla` |
| `0x00487e3e` | `FUN_00487e3e` | 126 | FUN_0048a3a2, FUN_0040fb2c |  |
| `0x00489014` | `FUN_00489014` | 336 | _strlen, FUN_004cbb55, FUN_004ccae0, FUN_004cc17b, FUN_00... | `out15.odm, out14.odm, out%02d.odm` |
| `0x0048946d` | `FUN_0048946d` | 235 | _rand, FUN_004547cf, FUN_00464876, FUN_0046488e, FUN_004c... |  |
| `0x0048959d` | `FUN_0048959d` | 601 | operator_new, FUN_00489574, FUN_004cb450 |  |
| `0x00489bc6` | `FUN_00489bc6` | 1824 | __ftol, FUN_0048a790, FUN_0048a629 |  |
| `0x0048a93f` | `FUN_0048a93f` | 336 | FUN_0048a790, FUN_0048a629 |  |
| `0x0048ab09` | `FUN_0048ab09` | 208 | _rand |  |
| `0x0048ac4b` | `FUN_0048ac4b` | 527 | _rand |  |
| `0x0048ae5a` | `FUN_0048ae5a` | 1773 | FUN_0043668c, FUN_00436a89, FUN_0048b547, FUN_004ca62e |  |
| `0x0048b547` | `FUN_0048b547` | 82 |  |  |
| `0x0048b599` | `FUN_0048b599` | 1523 | FUN_0048b547, FUN_004ca62e, FUN_00402cae |  |
| `0x0048bb8c` | `FUN_0048bb8c` | 841 | FUN_004a34df, FUN_0048ae5a, FUN_004a4874 |  |
| `0x0048bed5` | `FUN_0048bed5` | 799 | FUN_004a3a69, FUN_004a4874, FUN_0048b599 |  |
| `0x0048c922` | `FUN_0048c922` | 134 | FUN_0048eaa6, FUN_0048f734, FUN_0048e6d4, FUN_0048e9ec |  |
| `0x0048c9a8` | `FUN_0048c9a8` | 125 | FUN_0048eaa6, FUN_0048f734, FUN_0048e6d4, FUN_0048e9ec |  |
| `0x0048ca25` | `FUN_0048ca25` | 125 | FUN_0048eaa6, FUN_0048f734, FUN_0048e6d4, FUN_0048e9ec |  |
| `0x0048caa2` | `FUN_0048caa2` | 125 | FUN_0048eaa6, FUN_0048f734, FUN_0048e6d4, FUN_0048e9ec |  |
| `0x0048cb1f` | `FUN_0048cb1f` | 125 | FUN_0048eaa6, FUN_0048f734, FUN_0048e6d4, FUN_0048e9ec |  |
| `0x0048cb9c` | `FUN_0048cb9c` | 125 | FUN_0048eaa6, FUN_0048f734, FUN_0048e6d4, FUN_0048e9ec |  |
| `0x0048cc19` | `FUN_0048cc19` | 194 | FUN_00476399, FUN_0048eaa6, FUN_0048f734, FUN_0048e6d4, F... |  |
| `0x0048cdc1` | `FUN_0048cdc1` | 734 | _rand, FUN_0048fbf8, FUN_0048c922, FUN_0048ea13, FUN_0048... |  |
| `0x0048d1e4` | `FUN_0048d1e4` | 236 | _rand, FUN_0048d690, FUN_00438bce, FUN_0045827d |  |
| `0x0048d440` | `FUN_0048d440` | 62 |  |  |
| `0x0048d752` | `FUN_0048d752` | 287 | _rand, FUN_0045646e, FUN_0045827d |  |
| `0x0048d871` | `FUN_0048d871` | 814 | _rand, FUN_004698dc, FUN_0043ac68, FUN_0044c1a1, FUN_0040... |  |
| `0x0048dc04` | `FUN_0048dc04` | 216 | FUN_00492d5d, FUN_00492c03, FUN_004948a9, FUN_0048eaa6, F... |  |
| `0x0048dcdc` | `FUN_0048dcdc` | 1013 | _rand, FUN_004aa29b, FUN_00492a2e, FUN_0048cb1f, FUN_0048... |  |
| `0x0048e19b` | `FUN_0048e19b` | 853 | __ftol, FUN_0048cb9c, FUN_0048ea13, FUN_0048d690, FUN_004... | `\n\n\n\nxxxx` |
| `0x0048e4f0` | `FUN_0048e4f0` | 109 | FUN_0048fbf8, FUN_0048ea13, FUN_0048eaa6, FUN_0048caa2, F... |  |
| `0x0048e55d` | `FUN_0048e55d` | 194 | FUN_0048fbf8, FUN_0048ea13, FUN_0048eaa6, FUN_0048c9a8, F... |  |
| `0x0048e6d4` | `FUN_0048e6d4` | 80 | __ftol, __alldiv |  |
| `0x0048e8ed` | `FUN_0048e8ed` | 117 | __ftol, FUN_0048ea3e, FUN_00493707 |  |
| `0x0048e962` | `FUN_0048e962` | 77 | FUN_00493707 |  |
| `0x0048e9af` | `FUN_0048e9af` | 61 | _rand, FUN_004cac80 |  |
| `0x0048ea13` | `FUN_0048ea13` | 43 |  |  |
| `0x0048eaa6` | `FUN_0048eaa6` | 2938 | FUN_0048d690, FUN_00456d98, FUN_00456d7d, FUN_0048d6b6, F... |  |
| `0x0048f734` | `FUN_0048f734` | 286 |  |  |
| `0x0048f87a` | `FUN_0048f87a` | 894 | FUN_00476399, FUN_0048eaa6, FUN_0047638b |  |
| `0x0048fbf8` | `FUN_0048fbf8` | 1289 | FUN_00491075, FUN_0048d690, FUN_0048d65c, FUN_004910a0, F... |  |
| `0x00490242` | `FUN_00490242` | 347 | _memset, _rand, FUN_0048e55d, FUN_0048e4f0 |  |
| `0x004903c1` | `FUN_004903c1` | 196 |  |  |
| `0x00490485` | `FUN_00490485` | 360 | FUN_00490101 |  |
| `0x004905ed` | `FUN_004905ed` | 282 | FUN_00490101, FUN_0049090b |  |
| `0x004907df` | `FUN_004907df` | 193 | FUN_00490101, FUN_0040df03 |  |
| `0x004908a0` | `FUN_004908a0` | 54 |  |  |
| `0x0049090b` | `FUN_0049090b` | 225 | FUN_00490101 |  |
| `0x004909ec` | `FUN_004909ec` | 714 | _memset, _rand, FUN_0044a58d, FUN_0048e9ec |  |
| `0x00490cfa` | `FUN_00490cfa` | 492 | FUN_004909ec, FUN_0048e55d, FUN_004585be, FUN_00490707, F... |  |
| `0x00490ee6` | `FUN_00490ee6` | 399 | FUN_004b80dc, FUN_004b8065, FUN_0045646e, FUN_004b80a5, F... |  |
| `0x00491075` | `FUN_00491075` | 43 | FUN_0048f87a |  |
| `0x004910a0` | `FUN_004910a0` | 43 | FUN_0048f87a |  |
| `0x004910cb` | `FUN_004910cb` | 126 | FUN_00476399, FUN_0045827d, FUN_0048f87a |  |
| `0x00491149` | `FUN_00491149` | 162 | FUN_00476399, FUN_0045827d, FUN_0048f87a |  |
| `0x004911eb` | `FUN_004911eb` | 103 | FUN_0045827d, FUN_0047752f, FUN_0048f87a |  |
| `0x00491252` | `FUN_00491252` | 86 | FUN_0045827d, FUN_0048f87a |  |
| `0x004912a8` | `FUN_004912a8` | 103 | FUN_0048d6b6, FUN_0045827d, FUN_0048f87a |  |
| `0x0049130f` | `FUN_0049130f` | 71 | FUN_0048f87a |  |
| `0x00491375` | `FUN_00491375` | 957 | _rand, FUN_00402f07, FUN_004927a0, FUN_0049273d, FUN_0048... |  |
| `0x00491cad` | `FUN_00491cad` | 306 | FUN_004a99f7, FUN_0040fb2c, FUN_004cad70 | `%s%02d, ERADCATE, FACEMASK` |
| `0x00491e32` | `FUN_00491e32` | 333 | FUN_004a9d5d, FUN_0040f788 |  |
| `0x00491f7f` | `FUN_00491f7f` | 570 | _strcmp, FUN_004a6204, FUN_004a5e42, FUN_0040fb2c, FUN_00... | `NPC%03d, spell96` |
| `0x004921b9` | `FUN_004921b9` | 871 | FUN_00494b74, FUN_00492c03, FUN_004948a9, FUN_004a6204, F... |  |
| `0x0049273d` | `FUN_0049273d` | 99 |  |  |
| `0x00492bae` | `FUN_00492bae` | 85 | FUN_004aa29b |  |
| `0x00493273` | `FUN_00493273` | 982 | FUN_00493273, FUN_00449b7a |  |
| `0x00493707` | `FUN_00493707` | 363 | _memset |  |
| `0x004938c9` | `FUN_004938c9` | 103 | __ftol, FUN_00408768, FUN_0049402d, FUN_0048e8ed |  |
| `0x00493930` | `FUN_00493930` | 1601 | __ftol, FUN_0048e4f0, FUN_0048eaa6, FUN_00439463, FUN_004... |  |
| `0x00493f71` | `FUN_00493f71` | 188 | __alldiv, __ftol, __allrem |  |
| `0x0049402d` | `FUN_0049402d` | 2027 | __alldiv, _memset, __ftol, __allrem, _rand (+18 more) |  |
| `0x00494818` | `FUN_00494818` | 22 |  |  |
| `0x004948a9` | `FUN_004948a9` | 372 | _rand, __ftol, FUN_00494a1d, FUN_004aa29b |  |
| `0x00494a1d` | `FUN_00494a1d` | 200 |  |  |
| `0x00494b03` | `FUN_00494b03` | 35 |  |  |
| `0x00494b26` | `FUN_00494b26` | 78 |  |  |
| `0x00494b74` | `FUN_00494b74` | 101 | _rand |  |
| `0x00494f86` | `FUN_00494f86` | 79 |  |  |
| `0x0049537c` | `FUN_0049537c` | 202 | _rand, FUN_004cc91e |  |
| `0x00495477` | `FUN_00495477` | 1696 | _memset, __alldiv, __ftol, __allrem, _strncpy (+23 more) | `Invalid String Passed` |
| `0x00495b4f` | `FUN_00495b4f` | 3475 | _strlen, GetTickCount, FUN_004a515b, FUN_004a6204, FUN_00... | `%s%03d%d,  %03u%s` |
| `0x004968e2` | `FUN_004968e2` | 2996 | __ftol, FUN_0041d038, FUN_004aa185, FUN_0040fb2c, FUN_004... | `IC_KNIGHT, IC_THIEF, IC_MONK, IC_PALAD, IC_ARCH, IC_RANG... |
| `0x004974ae` | `FUN_004974ae` | 1071 | _rand, WaitMessage, DispatchMessageA, PeekMessageA, Trans... | `makeme.pcx` |
| `0x00498042` | `FUN_00498042` | 673 | FUN_004986c0, FUN_004982e3, FUN_0049896a, FUN_004cac3e, F... |  |
| `0x00498300` | `FUN_00498300` | 641 | FUN_004986c0, FUN_004982e3, FUN_0049896a, FUN_004cac3e, F... |  |
| `0x004988e3` | `FUN_004988e3` | 135 | FUN_004cac3e, FUN_004be634 |  |
| `0x00498d93` | `FUN_00498d93` | 7693 | _memset, _malloc, _strlen, FUN_00461659, FUN_004d6cd6 (+2... | `levels\%s, L.FData, L.RData, L.RLData, L.DData, Spawn, U... |
| `0x0049aba0` | `FUN_0049aba0` | 974 |  |  |
| `0x0049b0c6` | `FUN_0049b0c6` | 496 | FUN_0049b2b6, FUN_004369ca |  |
| `0x0049b341` | `FUN_0049b341` | 51 | _strlen, FUN_0040e33a, FUN_0040e302 | `hwsplat04` |
| `0x0049b4c9` | `FUN_0049b4c9` | 581 | __ftol, FUN_00436921, FUN_0040e2d4, FUN_0043f5eb, FUN_004... | `D:\mm7Src_eng\MM7\Code\PolyProjector.cpp, Error: Failed ... |
| `0x0049b719` | `FUN_0049b719` | 1069 | __ftol, FUN_004ca654, FUN_0040e2d4, FUN_00436ef8, FUN_004... | `D:\mm7Src_eng\MM7\Code\PolyProjector.cpp, Undefined clip... |
| `0x0049bb46` | `FUN_0049bb46` | 302 |  |  |
| `0x0049be13` | `FUN_0049be13` | 523 | FUN_0040e2d4, FUN_00466d0d, FUN_00436a2f, FUN_004262c0, F... | `D:\mm7Src_eng\MM7\Code\PolyProjector.cpp, Uknown strip t... |
| `0x0049c01e` | `FUN_0049c01e` | 568 | FUN_00466cc6, FUN_00436416, FUN_0043b593, FUN_004a1fc1 | `D:\mm7Src_eng\MM7\Code\PolyProjector.cpp` |
| `0x0049c96c` | `FUN_0049c96c` | 1211 | FUN_004379dd, FUN_004369ca, FUN_0049c865, FUN_00437aa4 |  |
| `0x0049ce27` | `FUN_0049ce27` | 1243 | FUN_0049d302 |  |
| `0x0049d302` | `FUN_0049d302` | 847 |  |  |
| `0x0049d689` | `FUN_0049d689` | 30 |  |  |
| `0x0049d714` | `FUN_0049d714` | 568 | _strlen, _strcmp, operator_new, FUN_004ca780, FUN_004cac80 | `RGB Emulation, Reference Rasterizer` |
| `0x0049e3d4` | `FUN_0049e3d4` | 184 | _memset |  |
| `0x0049ed46` | `FUN_0049ed46` | 1030 | _memset, FUN_004a515b, FUN_004cb46f, FUN_004a520d, FUN_00... | `screen%0.2i.pcx, D:\mm7Src_eng\MM7\Code\Screen16.cpp` |
| `0x0049f14c` | `FUN_0049f14c` | 998 | _memset, FUN_004a515b, FUN_004cb46f, FUN_004a520d, FUN_00... | `D:\mm7Src_eng\MM7\Code\Screen16.cpp` |
| `0x0049f532` | `FUN_0049f532` | 787 | FUN_004ca780, FUN_0042641d, FUN_004266fe |  |
| `0x0049f845` | `FUN_0049f845` | 792 | FUN_004cb46f, FUN_0042641d, FUN_004cb656, FUN_004cb4ec, F... |  |
| `0x0049ff8b` | `FUN_0049ff8b` | 1528 | _memset, operator_new, FUN_0049ff67, FUN_0049dc20, FUN_00... | `D3Drend->Init failed., Direct3D renderer:  The device fa... |
| `0x004a0583` | `FUN_004a0583` | 1531 | _memset, operator_new, FUN_0049ff67, FUN_0049dc20, FUN_00... | `D3Drend->Init failed., Direct3D renderer:  The device fa... |
| `0x004a0b7e` | `FUN_004a0b7e` | 658 |  |  |
| `0x004a0e46` | `FUN_004a0e46` | 138 |  |  |
| `0x004a0fc2` | `FUN_004a0fc2` | 178 | _memset, FUN_00466cc6 | `D:\mm7Src_eng\MM7\Code\Screen16.cpp` |
| `0x004a10f9` | `FUN_004a10f9` | 93 | FUN_00466cc6 | `D:\mm7Src_eng\MM7\Code\Screen16.cpp` |
| `0x004a1968` | `FUN_004a1968` | 330 |  |  |
| `0x004a1bae` | `FUN_004a1bae` | 394 | FUN_0047c374, FUN_004a1d38 |  |
| `0x004a1d38` | `FUN_004a1d38` | 245 | FUN_0047c374 |  |
| `0x004a1e46` | `FUN_004a1e46` | 299 | FUN_0049e48c, FUN_0040e2d4, FUN_0044ec4c, FUN_00466d0d, F... | `D:\mm7Src_eng\MM7\Code\screen16_3d.cpp, Error executing ... |
| `0x004a34df` | `FUN_004a34df` | 1418 | FUN_004a1ab2, FUN_00402cae |  |
| `0x004a3a69` | `FUN_004a3a69` | 1354 | FUN_004a1ab2, FUN_00402cae |  |
| `0x004a3fb3` | `FUN_004a3fb3` | 910 | FUN_0047c500, FUN_0047c3db, FUN_004a1ab2, FUN_004a1968 |  |
| `0x004a4341` | `FUN_004a4341` | 821 | FUN_0047c500, FUN_004a1ab2, FUN_004a1968 |  |
| `0x004a4676` | `FUN_004a4676` | 510 |  |  |
| `0x004a4960` | `FUN_004a4960` | 761 | FUN_004ca62e |  |
| `0x004a4c59` | `FUN_004a4c59` | 280 | FUN_004a1ab2 |  |
| `0x004a5281` | `FUN_004a5281` | 1272 | FUN_004268b6, FUN_00466cc6, FUN_0042688c | `D:\mm7Src_eng\MM7\Code\screen16blt.cpp` |
| `0x004a590d` | `FUN_004a590d` | 516 | _memset, FUN_00466cc6, FUN_004a0ed0 | `D:\mm7Src_eng\MM7\Code\screen16blt.cpp` |
| `0x004a5cc3` | `FUN_004a5cc3` | 383 | FUN_0040df03 |  |
| `0x004a5e42` | `FUN_004a5e42` | 364 |  |  |
| `0x004a6204` | `FUN_004a6204` | 370 |  |  |
| `0x004a6376` | `FUN_004a6376` | 486 |  |  |
| `0x004a655c` | `FUN_004a655c` | 426 |  |  |
| `0x004a6706` | `FUN_004a6706` | 377 |  |  |
| `0x004a687f` | `FUN_004a687f` | 377 |  |  |
| `0x004a6a41` | `FUN_004a6a41` | 414 |  |  |
| `0x004a6d17` | `FUN_004a6d17` | 110 |  |  |
| `0x004a6e0e` | `FUN_004a6e0e` | 457 |  |  |
| `0x004a6ff3` | `FUN_004a6ff3` | 242 |  |  |
| `0x004a733a` | `FUN_004a733a` | 546 | _memset, _rand, FUN_0048ab09 |  |
| `0x004a755c` | `FUN_004a755c` | 188 | _memset, _rand, FUN_0048ab09 |  |
| `0x004a7618` | `FUN_004a7618` | 373 | _memset, _rand, FUN_004782a3, FUN_0040fb2c, FUN_004a6ff3 ... | `effpar01` |
| `0x004a78d8` | `FUN_004a78d8` | 223 | _memset, _rand, FUN_0044d8fc, FUN_0048ab09 |  |
| `0x004a79b7` | `FUN_004a79b7` | 63 | FUN_00467d8c |  |
| `0x004a79f6` | `FUN_004a79f6` | 417 | _memset, _rand, FUN_0048ab09 |  |
| `0x004a7b97` | `FUN_004a7b97` | 510 | _memset, _rand, FUN_0044d8fc, FUN_0048ab09 |  |
| `0x004a7d95` | `FUN_004a7d95` | 132 | FUN_004a718e |  |
| `0x004a7e19` | `FUN_004a7e19` | 235 | _memset, _rand, FUN_0040fb2c, FUN_0048ab09 | `effpar02` |
| `0x004a7f04` | `FUN_004a7f04` | 251 | _memset, _rand, FUN_004be514, FUN_0048ab09 |  |
| `0x004a806c` | `FUN_004a806c` | 238 | _memset, _rand, FUN_004be514, FUN_0048ab09 |  |
| `0x004a815a` | `FUN_004a815a` | 1925 | FUN_004be586, FUN_004a755c, FUN_004a79b7, FUN_0040fb2c, F... |  |
| `0x004a8fba` | `FUN_004a8fba` | 118 | FUN_004a6204, FUN_00494f86 |  |
| `0x004a99f7` | `FUN_004a99f7` | 342 | _AIL_WAV_info@8, _AIL_decompress_ADPCM@12, _AIL_file_type... |  |
| `0x004a9cce` | `FUN_004a9cce` | 59 | FUN_004a964e |  |
| `0x004a9d09` | `FUN_004a9d09` | 84 | FUN_004a964e |  |
| `0x004a9d5d` | `FUN_004a9d5d` | 112 | _AIL_mem_free_lock@4, FUN_004a964e |  |
| `0x004aa0cf` | `FUN_004aa0cf` | 182 | _AIL_redbook_track_info@16, _AIL_redbook_play@12, __ftol,... |  |
| `0x004ab5ec` | `FUN_004ab5ec` | 69 | FUN_0045284a |  |
| `0x004abfb9` | `FUN_004abfb9` | 97 | _AIL_set_3D_provider_preference@12, FUN_004abf7c, FUN_004... | `EAX environment selection` |
| `0x004ac12c` | `FUN_004ac12c` | 174 | _memset, FUN_004382ab, FUN_004ac1da, FUN_004ac29d, FUN_00... |  |
| `0x004ac1da` | `FUN_004ac1da` | 195 | _memset, QueryPerformanceFrequency, QueryPerformanceCounter |  |
| `0x004ac29d` | `FUN_004ac29d` | 451 | _memset, GetThreadPriority, GetCurrentThread, QueryPerfor... |  |
| `0x004ac460` | `FUN_004ac460` | 357 | _memset, GetThreadPriority, GetCurrentThread, SetThreadPr... |  |
| `0x004ac5e1` | `FUN_004ac5e1` | 279 | FUN_004615bd, FUN_004c2f60, FUN_0042641d, FUN_004cb8a5, F... |  |
| `0x004acb9b` | `FUN_004acb9b` | 1689 |  |  |
| `0x004ad234` | `FUN_004ad234` | 152 |  |  |
| `0x004ad32b` | `FUN_004ad32b` | 131 | FUN_0040e2d4, FUN_00467838 | `D:\mm7Src_eng\MM7\Code\StationaryLightStack.cpp, Too man... |
| `0x004ad3f2` | `FUN_004ad3f2` | 43 | GetVersionExA |  |
| `0x004ad467` | `FUN_004ad467` | 2073 | FUN_004247d2, FUN_004ae554, FUN_004ae3f4, FUN_0047c250, F... |  |
| `0x004adc80` | `FUN_004adc80` | 1226 | GetTickCount, FUN_004247d2, FUN_0044e1c6, FUN_0047c250, F... |  |
| `0x004ae14a` | `FUN_004ae14a` | 300 | FUN_004ca62e |  |
| `0x004ae276` | `FUN_004ae276` | 382 | FUN_004ca62e |  |
| `0x004ae3f4` | `FUN_004ae3f4` | 297 | FUN_004ca62e |  |
| `0x004ae51d` | `FUN_004ae51d` | 55 | FUN_0044e1c6 |  |
| `0x004ae554` | `FUN_004ae554` | 3617 | GetTickCount, FUN_004ca62e, FUN_00452aca, FUN_004989ca |  |
| `0x004af375` | `FUN_004af375` | 1908 |  |  |
| `0x004afae9` | `FUN_004afae9` | 1011 | FUN_004247d2, FUN_0044065c, FUN_004ca62e, FUN_00423b06, F... |  |
| `0x004afedc` | `FUN_004afedc` | 2542 | FUN_004247d2, FUN_004adc80, FUN_004ad467, FUN_004ae554, F... |  |
| `0x004b0988` | `FUN_004b0988` | 982 | GetTickCount, FUN_00437274, FUN_004364b4, FUN_0049af8a, F... |  |
| `0x004b0d6a` | `FUN_004b0d6a` | 161 | GetTickCount |  |
| `0x004b0e0b` | `FUN_004b0e0b` | 1409 | FUN_004247d2, FUN_0044065c, FUN_00437274, FUN_0049c60a, F... |  |
| `0x004b13aa` | `FUN_004b13aa` | 220 | FUN_00449ba1, FUN_00449b7a |  |
| `0x004b1486` | `FUN_004b1486` | 609 | FUN_0044c5c9, FUN_0044ce34, FUN_0044d432, FUN_0044c54a, F... | `%s%03d, %s\n\n%s %03d: %03d%s 000\n%s %03d: %03d%s 000\n... |
| `0x004b17b7` | `FUN_004b17b7` | 473 | __alldiv, __ftol, __allrem, FUN_0044c5c9, FUN_004cac90 (+... | `%d %s` |
| `0x004b1990` | `FUN_004b1990` | 430 | FUN_00421e1e, FUN_00469c3d, FUN_0041d83e, FUN_004b1486 |  |
| `0x004b1b3e` | `FUN_004b1b3e` | 332 | __alldiv, _memset, __ftol, __allrem, __allmul (+2 more) |  |
| `0x004b1c8a` | `FUN_004b1c8a` | 363 | _rand, FUN_004b1df5, FUN_00492c03 |  |
| `0x004b1e31` | `FUN_004b1e31` | 307 | FUN_0044b01e, FUN_00449bd7, FUN_00449b7a |  |
| `0x004b1f64` | `FUN_004b1f64` | 1356 | FUN_0044686d, FUN_0044608d, FUN_004b1e31, FUN_004bf518, F... |  |
| `0x004b24b0` | `FUN_004b24b0` | 1058 | FUN_0048c880, FUN_0048c869, FUN_0048c852, FUN_004cad70, F... |  |
| `0x004b2955` | `FUN_004b2955` | 130 | FUN_00492c03, FUN_00449b7a |  |
| `0x004b29d7` | `FUN_004b29d7` | 1763 | FUN_0044c5c9, FUN_0041cce4, FUN_0044c794, FUN_0044ce34, F... |  |
| `0x004b30ba` | `FUN_004b30ba` | 1397 | FUN_0044c5c9, FUN_004b5cdf, FUN_004b68a6, FUN_0044c794, F... |  |
| `0x004b3d81` | `FUN_004b3d81` | 210 | FUN_0041d038, FUN_0041c213, FUN_00445a1c, FUN_0041c3db, F... |  |
| `0x004b3e53` | `FUN_004b3e53` | 245 | FUN_0041d038, FUN_0041c213, FUN_004b2955, FUN_0041c3db, F... |  |
| `0x004b3f48` | `FUN_004b3f48` | 257 | FUN_0041d038, FUN_004b24b0, FUN_0041c213, FUN_0041c3db, F... |  |
| `0x004b4049` | `FUN_004b4049` | 318 | FUN_0041d038, FUN_0041c213, FUN_0041c3db, FUN_0041d0d8 |  |
| `0x004b4187` | `FUN_004b4187` | 1153 | FUN_0041d038, FUN_0041c213, FUN_004b3aa5, FUN_0041d081, F... |  |
| `0x004b4673` | `FUN_004b4673` | 2111 | __ftol, FUN_0044c5c9, FUN_0048e4f0, FUN_004cac90, FUN_004... | `%s\n \n%s` |
| `0x004b4f32` | `FUN_004b4f32` | 3501 | __ftol, GetAsyncKeyState, FUN_0044c5c9, FUN_00421e1e, FUN... |  |
| `0x004b5cdf` | `FUN_004b5cdf` | 1788 | __ftol, GetAsyncKeyState, FUN_0044c5c9, FUN_004b17b7, FUN... |  |
| `0x004b63db` | `FUN_004b63db` | 1138 | __ftol, FUN_0044c5c9, FUN_004cac90, FUN_004cad70, FUN_004... |  |
| `0x004b684d` | `FUN_004b684d` | 89 | FUN_00449b7a |  |
| `0x004b68a6` | `FUN_004b68a6` | 1718 | __ftol, GetTickCount, FUN_0044c5c9, FUN_004cac90, FUN_004... | `%s\n \n%s%s%s%s%s` |
| `0x004b6fc1` | `FUN_004b6fc1` | 2227 | _memset, __ftol, FUN_0044c5c9, FUN_0048e55d, FUN_0044c1a1... | `%s %d %s` |
| `0x004b7874` | `FUN_004b7874` | 1133 | FUN_0044c5c9, FUN_0044c794, FUN_0044c54a, FUN_004cad70, F... | `%s: %d, %s\n%s` |
| `0x004b7ce1` | `FUN_004b7ce1` | 706 | FUN_004b1df5, FUN_00492b68, FUN_0044d432, FUN_0040df03, F... | `%s: %d, %s\n%s` |
| `0x004b7fa3` | `FUN_004b7fa3` | 60 | __alldiv, __ftol |  |
| `0x004b81e8` | `FUN_004b81e8` | 3000 | __ftol, FUN_0044c5c9, FUN_004b1c8a, FUN_0044c794, FUN_004... | `%s%s%s%s` |
| `0x004b8da0` | `FUN_004b8da0` | 343 | _rand, FUN_0045664c, FUN_00402f07 |  |
| `0x004b8ef7` | `FUN_004b8ef7` | 379 | _rand, FUN_0045664c, FUN_00402f07 |  |
| `0x004b9072` | `FUN_004b9072` | 2999 | __ftol, GetAsyncKeyState, FUN_0044c5c9, FUN_00421e1e, FUN... |  |
| `0x004b9c29` | `FUN_004b9c29` | 3170 | __ftol, GetAsyncKeyState, FUN_0044c5c9, FUN_00421e1e, FUN... |  |
| `0x004ba88b` | `FUN_004ba88b` | 3630 | __ftol, GetAsyncKeyState, FUN_0044c5c9, FUN_00421e1e, FUN... |  |
| `0x004bb6b9` | `FUN_004bb6b9` | 815 | _memset, FUN_0048e4f0, FUN_0044c1a1, FUN_00492b3a, FUN_00... |  |
| `0x004bb9e8` | `FUN_004bb9e8` | 600 | _rand, __ftol, __allmul, FUN_0041d038, FUN_0041c213 (+4 m... |  |
| `0x004bbc40` | `FUN_004bbc40` | 644 | FUN_0041d038, FUN_004aa29b, FUN_00420bae, FUN_0044a5ee, F... |  |
| `0x004bbec4` | `FUN_004bbec4` | 424 | FUN_004a99f7, FUN_004596aa, FUN_004595d3, FUN_004ca780, F... |  |
| `0x004bc3fe` | `FUN_004bc3fe` | 1996 | _memset, __ftol, _rand, _strcmp, FUN_0044c5c9 (+32 more) |  |
| `0x004bc996` | `FUN_004bc996` | 153 | FUN_0041c213, FUN_0041c3db, FUN_0041d0d8 |  |
| `0x004bca2f` | `FUN_004bca2f` | 3561 | __ftol, __allmul, _rand, FUN_004b8da0, FUN_0040fb2c (+20 ... |  |
| `0x004bda12` | `FUN_004bda12` | 167 |  |  |
| `0x004bdab9` | `FUN_004bdab9` | 1927 | __ftol, GetAsyncKeyState, FUN_00421e1e, FUN_004b80dc, FUN... |  |
| `0x004be5c0` | `FUN_004be5c0` | 88 | FUN_004be514 |  |
| `0x004bf886` | `FUN_004bf886` | 1295 | __alldiv, __ftol, GetTickCount, DispatchMessageA, PeekMes... | `winbg.pcx, FONTPAL, endgame.fnt,  %lu %s, %lu %s, %lu %s... |
| `0x004c01d4` | `FUN_004c01d4` | 150 | FUN_004c0284 |  |
| `0x004c0703` | `FUN_004c0703` | 396 | FUN_0040e2d4, FUN_00466d0d, FUN_00438bce, FUN_0040104c, F... | `D:\mm7Src_eng\MM7\Code\Vis.cpp, Unsupported "exclusion i... |
| `0x004c088f` | `FUN_004c088f` | 1034 | FUN_004c1741, FUN_004c1626, FUN_004c04ce, FUN_004c2503, F... |  |
| `0x004c0d5c` | `FUN_004c0d5c` | 264 | FUN_0046ed06, FUN_004c0e64, FUN_004c0f98, FUN_004c0703, F... |  |
| `0x004c0e64` | `FUN_004c0e64` | 308 |  |  |
| `0x004c0f98` | `FUN_004c0f98` | 657 | FUN_004c1a2c, FUN_004c24c3, FUN_004c1626, FUN_004cac3e, F... |  |
| `0x004c1235` | `FUN_004c1235` | 340 | FUN_004c1579 |  |
| `0x004c1579` | `FUN_004c1579` | 173 |  |  |
| `0x004c1626` | `FUN_004c1626` | 283 | FUN_00436512, FUN_004c0703, FUN_00436444, FUN_0048b547, F... |  |
| `0x004c1741` | `FUN_004c1741` | 353 | FUN_0046ed06, FUN_004c0e64, FUN_00436512, FUN_004c0703, F... |  |
| `0x004c18b6` | `FUN_004c18b6` | 179 | FUN_004c04ce, FUN_004c066a, FUN_004c2503, FUN_004cac3e, F... |  |
| `0x004c1b7e` | `FUN_004c1b7e` | 287 | __ftol, FUN_004c1c9d |  |
| `0x004c1c9d` | `FUN_004c1c9d` | 442 | FUN_004c1e57, FUN_004c20f8 |  |
| `0x004c1e57` | `FUN_004c1e57` | 673 |  |  |
| `0x004c20f8` | `FUN_004c20f8` | 776 |  |  |
| `0x004c24c3` | `FUN_004c24c3` | 64 |  |  |
| `0x004c25bc` | `FUN_004c25bc` | 134 | FUN_004c25bc |  |
| `0x004c30b0` | `FUN_004c30b0` | 59 | FUN_004c4960 |  |
| `0x004c3270` | `FUN_004c3270` | 1012 | FUN_004c4960, FUN_004c4a70 | `unknown compression method, invalid window size, incorre... |
| `0x004c36c0` | `FUN_004c36c0` | 505 | FUN_004c5890, FUN_004c38c0, FUN_004c3c80 | `1.1.3, insufficient memory` |
| `0x004c38c0` | `FUN_004c38c0` | 110 | FUN_004c58c0, FUN_004c3d10 |  |
| `0x004c3930` | `FUN_004c3930` | 679 | FUN_004c5a60, FUN_004c59b0, FUN_004c3c10, FUN_004c3be0 | `buffer error, stream error` |
| `0x004c3be0` | `FUN_004c3be0` | 41 |  |  |
| `0x004c3c10` | `FUN_004c3c10` | 112 |  |  |
| `0x004c3c80` | `FUN_004c3c80` | 137 |  |  |
| `0x004c3d10` | `FUN_004c3d10` | 145 |  |  |
| `0x004c3db0` | `FUN_004c3db0` | 326 | FUN_004c3f00, FUN_004c5cb0, FUN_004c3c10 |  |
| `0x004c3f00` | `FUN_004c3f00` | 284 | FUN_004c4020 |  |
| `0x004c4020` | `FUN_004c4020` | 105 | FUN_004c5760 |  |
| `0x004c4a70` | `FUN_004c4a70` | 3201 | FUN_004c7c00, FUN_004c8180, FUN_004c7bf0, FUN_004c7400, F... | `invalid block type, invalid stored block lengths, too ma... |
| `0x004c5760` | `FUN_004c5760` | 300 |  |  |
| `0x004c5a60` | `FUN_004c5a60` | 591 | FUN_004c7250 |  |
| `0x004c5cb0` | `FUN_004c5cb0` | 484 | FUN_004c6590, FUN_004c6d80, FUN_004c71c0, FUN_004c6440, F... |  |
| `0x004c5ea0` | `FUN_004c5ea0` | 559 | FUN_004c60d0, FUN_004c63d0, FUN_004c61b0 |  |
| `0x004c61b0` | `FUN_004c61b0` | 539 |  |  |
| `0x004c63d0` | `FUN_004c63d0` | 111 | FUN_004c7230 |  |
| `0x004c6440` | `FUN_004c6440` | 104 | FUN_004c64b0, FUN_004c5ea0 |  |
| `0x004c64b0` | `FUN_004c64b0` | 224 |  |  |
| `0x004c6590` | `FUN_004c6590` | 610 | FUN_004c6800 |  |
| `0x004c6800` | `FUN_004c6800` | 1399 |  |  |
| `0x004c6d80` | `FUN_004c6d80` | 1075 |  |  |
| `0x004c71c0` | `FUN_004c71c0` | 112 |  |  |
| `0x004c7230` | `FUN_004c7230` | 23 |  |  |
| `0x004c7360` | `FUN_004c7360` | 147 | FUN_004c72e0 |  |
| `0x004c7440` | `FUN_004c7440` | 1914 | FUN_004c8460, FUN_004c8310 | `invalid literal/length code, invalid distance code` |
| `0x004c7cb0` | `FUN_004c7cb0` | 1218 |  |  |
| `0x004c8180` | `FUN_004c8180` | 349 | FUN_004c7cb0 | `oversubscribed distance tree, incomplete distance tree, ... |
| `0x004c8310` | `FUN_004c8310` | 322 |  |  |
| `0x004c8460` | `FUN_004c8460` | 946 |  | `invalid distance code, invalid literal/length code` |
| `0x004c9a13` | `FUN_004c9a13` | 34 |  |  |
| `0x004c9b17` | `FUN_004c9b17` | 55 | FUN_004c9b6f, FUN_004c9b64 |  |
| `0x004c9c3e` | `FUN_004c9c3e` | 47 |  |  |
| `0x004c9d62` | `FUN_004c9d62` | 113 | FUN_004c9a13, FUN_004cdc7c, FUN_004c9eb7, FUN_004ca1b9 |  |
| `0x004c9dd3` | `FUN_004c9dd3` | 144 | FUN_004cde6c, FUN_004c9eb7, FUN_004ca1b9, FUN_004c9e63 |  |
| `0x004c9e63` | `FUN_004c9e63` | 16 |  |  |
| `0x004c9eb7` | `FUN_004c9eb7` | 219 | FUN_004ce0aa, FUN_004cdc7c, FUN_004cd160, FUN_004c9e63 |  |
| `0x004ca1a2` | `FUN_004ca1a2` | 23 |  |  |
| `0x004ca1b9` | `FUN_004ca1b9` | 30 |  |  |
| `0x004ca24c` | `FUN_004ca24c` | 92 | FUN_004ca2a8, FUN_004cd160 |  |
| `0x004ca2a8` | `FUN_004ca2a8` | 37 |  |  |
| `0x004ca654` | `FUN_004ca654` | 9 | __fload_withFB |  |
| `0x004ca65d` | `FUN_004ca65d` | 157 | __math_exit, __startOneArgErrorHandling, FUN_004ce655, FU... |  |
| `0x004ca780` | `FUN_004ca780` | 664 |  |  |
| `0x004caaf0` | `FUN_004caaf0` | 208 | FUN_004ced62, FUN_004cc98d, FUN_004cedc3 |  |
| `0x004cabc0` | `FUN_004cabc0` | 126 | FUN_004cedd8, FUN_004cd07a, FUN_004cd071, FUN_004cf107 |  |
| `0x004cac80` | `FUN_004cac80` | 7 |  |  |
| `0x004cac90` | `FUN_004cac90` | 224 |  |  |
| `0x004cae00` | `FUN_004cae00` | 231 | HeapAlloc, FUN_004d0b43, FUN_004ced62, FUN_004d00a0, FUN_... |  |
| `0x004cb26f` | `FUN_004cb26f` | 114 | FUN_004d0ff8, FUN_004cb0bb |  |
| `0x004cb2e4` | `FUN_004cb2e4` | 123 | FUN_004d18bd |  |
| `0x004cb51b` | `FUN_004cb51b` | 266 | FUN_004cce74, FUN_004ca780, FUN_004cdc7c, FUN_004cf1f5 |  |
| `0x004cb68b` | `FUN_004cb68b` | 353 | FUN_004ce0aa, FUN_004d1be9 |  |
| `0x004cb818` | `FUN_004cb818` | 141 | FUN_004cce74, FUN_004ce0aa, FUN_004d1be9, FUN_004cb68b |  |
| `0x004cb8d4` | `FUN_004cb8d4` | 232 | FUN_004ca780, FUN_004d1bfb, FUN_004cde6c |  |
| `0x004cbaca` | `FUN_004cbaca` | 139 | FUN_004d1cd7 |  |
| `0x004cbd11` | `FUN_004cbd11` | 204 | FUN_004d1cd7, FUN_004d1d4c |  |
| `0x004cc17b` | `FUN_004cc17b` | 164 | FUN_004cecd2 |  |
| `0x004cc48d` | `FUN_004cc48d` | 125 | __math_exit, __startOneArgErrorHandling, FUN_004ce66c |  |
| `0x004cc590` | `FUN_004cc590` | 47 |  |  |
| `0x004cc5bf` | `FUN_004cc5bf` | 87 | _strlen, FUN_004d2948, FUN_004d1cd7 |  |
| `0x004cc620` | `FUN_004cc620` | 257 | FUN_004cedc3, FUN_004ced62, FUN_004cc98d |  |
| `0x004cc8bc` | `FUN_004cc8bc` | 98 | FUN_004cdaf8, FUN_004d1bfb, FUN_004cdb4a |  |
| `0x004cc98d` | `FUN_004cc98d` | 203 | FUN_004d1cd7, FUN_004d1d4c |  |
| `0x004ccc3e` | `FUN_004ccc3e` | 327 | _strlen, FUN_004d2dbb |  |
| `0x004ccd85` | `FUN_004ccd85` | 100 | FUN_004ce0aa, FUN_004cdaf8, FUN_004cce74, FUN_004cdb4a |  |
| `0x004cce46` | `FUN_004cce46` | 46 | FUN_004cce74, FUN_004d2e55 |  |
| `0x004cced9` | `FUN_004cced9` | 164 | FUN_004cedc3, FUN_004cdb79, FUN_004cce46, FUN_004cdb27, F... |  |
| `0x004ccfcc` | `FUN_004ccfcc` | 163 | GetCurrentProcess, TerminateProcess, ExitProcess, FUN_004... |  |
| `0x004cd0f6` | `FUN_004cd0f6` | 92 |  |  |
| `0x004cd160` | `FUN_004cd160` | 664 |  |  |
| `0x004cd4b9` | `FUN_004cd4b9` | 9 | __fload_withFB |  |
| `0x004cd4c2` | `FUN_004cd4c2` | 467 | __fload_withFB, FUN_004cd695, FUN_004ce640, FUN_004ce6de,... |  |
| `0x004cd6df` | `FUN_004cd6df` | 207 | __frnd, FUN_004d2052, FUN_004d20a6, FUN_004d2733, FUN_004... |  |
| `0x004cd8ff` | `FUN_004cd8ff` | 289 | HeapAlloc, _memset, FUN_004cda21, FUN_004cd998, FUN_004d0... |  |
| `0x004cdaf8` | `FUN_004cdaf8` | 47 | EnterCriticalSection, FUN_004ced62 |  |
| `0x004cdb4a` | `FUN_004cdb4a` | 47 | LeaveCriticalSection, FUN_004cedc3 |  |
| `0x004cdb9c` | `FUN_004cdb9c` | 93 | FUN_004d3e2e, FUN_004d1be9, FUN_004d1bf2, FUN_004cdbf9, F... |  |
| `0x004cdbf9` | `FUN_004cdbf9` | 131 | GetLastError, CloseHandle, FUN_004d3dec, FUN_004d3d6d, FU... |  |
| `0x004cdc7c` | `FUN_004cdc7c` | 101 | FUN_004d3e2e, FUN_004d1be9, FUN_004d1bf2, FUN_004cdce1, F... |  |
| `0x004cdce1` | `FUN_004cdce1` | 395 | GetLastError, WriteFile, FUN_004d1be9, FUN_004ce10f, FUN_... |  |
| `0x004cde6c` | `FUN_004cde6c` | 101 | FUN_004d3e2e, FUN_004d1be9, FUN_004d1bf2, FUN_004cded1, F... |  |
| `0x004cded1` | `FUN_004cded1` | 473 | GetLastError, ReadFile, FUN_004d1be9, FUN_004ce10f, FUN_0... |  |
| `0x004ce0aa` | `FUN_004ce0aa` | 101 | FUN_004d3e2e, FUN_004d1be9, FUN_004ce10f, FUN_004d1bf2, F... |  |
| `0x004ce10f` | `FUN_004ce10f` | 115 | SetFilePointer, GetLastError, FUN_004d1be9, FUN_004d3dec,... |  |
| `0x004ce182` | `FUN_004ce182` | 444 | GetFileType, __amsg_exit, GetStdHandle, GetStartupInfoA, ... |  |
| `0x004ce66c` | `FUN_004ce66c` | 25 |  |  |
| `0x004ce6c8` | `FUN_004ce6c8` | 22 |  |  |
| `0x004ce845` | `FUN_004ce845` | 90 | FUN_004d1cd7, FUN_004cc91e |  |
| `0x004ce943` | `FUN_004ce943` | 97 | FUN_004d44d1, FUN_004ce9a4, FUN_004d4548 |  |
| `0x004ce9a4` | `FUN_004ce9a4` | 194 | FUN_004cac80, FUN_004cec46 | `e+000` |
| `0x004cea66` | `FUN_004cea66` | 85 | FUN_004ceabb, FUN_004d44d1, FUN_004d4548 |  |
| `0x004ceabb` | `FUN_004ceabb` | 167 | _memset, FUN_004cec46 |  |
| `0x004ceb62` | `FUN_004ceb62` | 147 | FUN_004ceabb, FUN_004d44d1, FUN_004ce9a4, FUN_004d4548 |  |
| `0x004cedd8` | `FUN_004cedd8` | 781 | _malloc, HeapReAlloc, HeapAlloc, FUN_004cfd77, FUN_004ca7... |  |
| `0x004cf107` | `FUN_004cf107` | 214 | HeapSize, FUN_004cf171, FUN_004cf1ec, FUN_004cfd4c, FUN_0... |  |
| `0x004cf1f5` | `FUN_004cf1f5` | 280 | FUN_004ce0aa, FUN_004cdc7c, FUN_004d476e, FUN_004d472a |  |
| `0x004cf30d` | `FUN_004cf30d` | 1825 | _strlen, __aullrem, __aulldiv, FUN_004cfa4e, FUN_004cfaf9... | `null)` |
| `0x004cfa4e` | `FUN_004cfa4e` | 53 | FUN_004cf1f5 |  |
| `0x004cfb09` | `FUN_004cfb09` | 14 |  |  |
| `0x004cfb5f` | `FUN_004cfb5f` | 328 | GetVersionExA, GetEnvironmentVariableA, _strchr, _strncmp... | `__MSVCRT_HEAP_SELECT, __GLOBAL_HEAP_SELECTED` |
| `0x004cfca7` | `FUN_004cfca7` | 93 | HeapDestroy, HeapCreate, FUN_004d084b, FUN_004cfd04, FUN_... |  |
| `0x004cfd4c` | `FUN_004cfd4c` | 43 |  |  |
| `0x004cfd77` | `FUN_004cfd77` | 809 | VirtualFree, HeapFree, FUN_004cd160 |  |
| `0x004d00a0` | `FUN_004d00a0` | 777 | FUN_004d03a9, FUN_004d045a |  |
| `0x004d045a` | `FUN_004d045a` | 251 | VirtualAlloc |  |
| `0x004d0555` | `FUN_004d0555` | 758 |  |  |
| `0x004d0aa7` | `FUN_004d0aa7` | 87 |  |  |
| `0x004d0afe` | `FUN_004d0afe` | 69 | FUN_004d09e5 |  |
| `0x004d0b43` | `FUN_004d0b43` | 520 | VirtualAlloc, _memset, FUN_004d084b, FUN_004d0d4b |  |
| `0x004d0d4b` | `FUN_004d0d4b` | 292 |  |  |
| `0x004d0e6f` | `FUN_004d0e6f` | 169 |  |  |
| `0x004d0ff8` | `FUN_004d0ff8` | 155 | FUN_004d18bd, FUN_004d134d, FUN_004d1093 |  |
| `0x004d1093` | `FUN_004d1093` | 435 | FUN_004d1246, FUN_004cecd2, FUN_004d4a75, FUN_004d12f0, F... |  |
| `0x004d1246` | `FUN_004d1246` | 170 | FUN_004cecd2, FUN_004d1401, FUN_004cb2e4, FUN_004cb1b9 |  |
| `0x004d12f0` | `FUN_004d12f0` | 93 | _strcmp |  |
| `0x004d193e` | `FUN_004d193e` | 368 | FUN_004d4adc |  |
| `0x004d1b76` | `FUN_004d1b76` | 115 | FUN_004d1be9, FUN_004d1bf2 |  |
| `0x004d1bfb` | `FUN_004d1bfb` | 220 | FUN_004cde6c, FUN_004d472a |  |
| `0x004d1cd7` | `FUN_004d1cd7` | 117 | FUN_004d4dab |  |
| `0x004d1d4c` | `FUN_004d1d4c` | 511 | LCMapStringA, WideCharToMultiByte, LCMapStringW, MultiByt... |  |
| `0x004d213e` | `FUN_004d213e` | 691 | RaiseException, FUN_004d285c, FUN_004d284e |  |
| `0x004d23f1` | `FUN_004d23f1` | 535 | FUN_004d288e, FUN_004d278d |  |
| `0x004d2733` | `FUN_004d2733` | 90 |  |  |
| `0x004d278d` | `FUN_004d278d` | 193 | FUN_004d270a |  |
| `0x004d2948` | `FUN_004d2948` | 127 | FUN_004d444b, FUN_004d4ef4 |  |
| `0x004d29c7` | `FUN_004d29c7` | 429 | GetCPInfo, FUN_004d2bf1, FUN_004d2bbe, FUN_004cedc3, FUN_... |  |
| `0x004d2b74` | `FUN_004d2b74` | 74 |  |  |
| `0x004d2c1a` | `FUN_004d2c1a` | 389 | GetCPInfo, FUN_004d4dab, FUN_004d1d4c |  |
| `0x004d2dbb` | `FUN_004d2dbb` | 154 | _strncpy, FUN_004cedc3, FUN_004ced62 |  |
| `0x004d3317` | `FUN_004d3317` | 101 | __frnd, FUN_004d615b |  |
| `0x004d36db` | `FUN_004d36db` | 436 |  |  |
| `0x004d3cf1` | `FUN_004d3cf1` | 124 | SetStdHandle, FUN_004d1be9, FUN_004d1bf2 |  |
| `0x004d3d6d` | `FUN_004d3d6d` | 127 | SetStdHandle, FUN_004d1be9, FUN_004d1bf2 |  |
| `0x004d3dec` | `FUN_004d3dec` | 66 | FUN_004d1be9, FUN_004d1bf2 |  |
| `0x004d3e2e` | `FUN_004d3e2e` | 95 | EnterCriticalSection, InitializeCriticalSection, FUN_004c... |  |
| `0x004d3e8d` | `FUN_004d3e8d` | 34 | LeaveCriticalSection |  |
| `0x004d3eaf` | `FUN_004d3eaf` | 208 | FUN_004d23f1, FUN_004d2690, FUN_004d286b, FUN_0040dee8, F... |  |
| `0x004d3f7f` | `FUN_004d3f7f` | 53 | FUN_004d3fca, FUN_004d405c |  |
| `0x004d3fb4` | `FUN_004d3fb4` | 22 | FUN_004d3f7f |  |
| `0x004d3fca` | `FUN_004d3fca` | 146 |  |  |
| `0x004d405c` | `FUN_004d405c` | 137 |  |  |
| `0x004d40e5` | `FUN_004d40e5` | 73 |  |  |
| `0x004d4184` | `FUN_004d4184` | 140 | FUN_004d40e5, FUN_004d412e |  |
| `0x004d4252` | `FUN_004d4252` | 141 |  |  |
| `0x004d42df` | `FUN_004d42df` | 364 | FUN_004d4252, FUN_004d4210, FUN_004d4237, FUN_004d422b, F... |  |
| `0x004d45a4` | `FUN_004d45a4` | 182 |  |  |
| `0x004d4670` | `FUN_004d4670` | 62 |  |  |
| `0x004d46f0` | `FUN_004d46f0` | 58 |  |  |
| `0x004d476e` | `FUN_004d476e` | 41 |  |  |
| `0x004d4870` | `FUN_004d4870` | 517 | FUN_004d1be9, FUN_004d1cd7, FUN_004cbca2 |  |
| `0x004d4a75` | `FUN_004d4a75` | 28 | IsBadReadPtr |  |
| `0x004d4a91` | `FUN_004d4a91` | 28 | IsBadWritePtr |  |
| `0x004d4adc` | `FUN_004d4adc` | 719 | CreateFileA, GetLastError, GetFileType, CloseHandle, FUN_... |  |
| `0x004d4dab` | `FUN_004d4dab` | 318 | GetStringTypeA, MultiByteToWideChar, _memset, GetStringTy... |  |
| `0x004d4ef4` | `FUN_004d4ef4` | 1185 | FUN_004d6bf0, FUN_004d6551, FUN_004d1cd7 |  |
| `0x004d615b` | `FUN_004d615b` | 146 | FUN_004d2733 |  |
| `0x004d61fe` | `FUN_004d61fe` | 49 |  |  |
| `0x004d643a` | `FUN_004d643a` | 61 |  |  |
| `0x004d6477` | `FUN_004d6477` | 33 |  |  |
| `0x004d64f6` | `FUN_004d64f6` | 46 |  |  |
| `0x004d6524` | `FUN_004d6524` | 45 |  |  |
| `0x004d6551` | `FUN_004d6551` | 199 | ___add_12, FUN_004d64f6 |  |
| `0x004d6618` | `FUN_004d6618` | 659 | ___add_12, FUN_004cac80, FUN_004d6bf0, FUN_004d64f6, FUN_... | `1#SNAN, 1#IND, 1#INF, 1#QNAN` |
| `0x004d69d0` | `FUN_004d69d0` | 544 | FUN_004d64f6, FUN_004d6524, FUN_004d6477 |  |
| `0x004d6bf0` | `FUN_004d6bf0` | 124 | FUN_004d69d0 |  |
| `0x004d6c6c` | `FUN_004d6c6c` | 97 | FUN_004d1be9 |  |
| `0x004d6d1c` | `FUN_004d6d1c` | 134 | GetCurrentDirectoryA, GetLastError, SetEnvironmentVariabl... |  |
| `0x004d6dc9` | `FUN_004d6dc9` | 213 | GetFullPathNameA, GetCurrentDirectoryA, _malloc, FUN_004d... |  |
| `0x004d6e9e` | `FUN_004d6e9e` | 55 | GetDriveTypeA |  |
| `0x004d6ed5` | `FUN_004d6ed5` | 123 | FUN_004d1d4c |  |
| `0x004d6f50` | `FUN_004d6f50` | 170 |  |  |
| `0x004d6ffa` | `FUN_004d6ffa` | 242 |  |  |
| `0x004d70ec` | `FUN_004d70ec` | 172 |  |  |
| `0x004d7198` | `FUN_004d7198` | 244 |  |  |
| `0x004d728c` | `FUN_004d728c` | 243 |  |  |
| `0x004d737f` | `FUN_004d737f` | 175 |  |  |
| `0x004d742e` | `FUN_004d742e` | 189 |  |  |
| `0x004d74eb` | `FUN_004d74eb` | 229 |  |  |
| `0x004d75d0` | `FUN_004d75d0` | 189 |  |  |
| `0x004d768d` | `FUN_004d768d` | 229 |  |  |
| `0x004d7772` | `FUN_004d7772` | 200 |  |  |
| `0x004d783a` | `FUN_004d783a` | 240 |  |  |
| `0x004d7c60` | `Unwind@004d7c60` | 23 | FUN_004679de |  |
| `0x004d7e84` | `Unwind@004d7e84` | 26 | FUN_004c9bf4 |  |
| `0x004d7ea8` | `Unwind@004d7ea8` | 26 | FUN_004c9bf4 |  |

### Rendering (36 functions)

> DirectDraw/Direct3D setup, PCX loading, blitting, texture management

| Address | Name | Size | Key Callees | Referenced Strings |
|---------|------|-----:|-------------|-------------------|
| `0x0040d7c6` | `FUN_0040d7c6` | 30 | FUN_0040f420 | `sprites.pcx` |
| `0x0040d7fb` | `FUN_0040d7fb` | 453 | FUN_0040e2d4, FUN_00466d0d | `D:\mm7Src_eng\MM7\Code\am_nw.cpp, Problem in Blit_Chroma` |
| `0x0040d9c0` | `FUN_0040d9c0` | 351 | FUN_0040e2d4, FUN_00466d0d | `D:\mm7Src_eng\MM7\Code\am_nw.cpp, Problem in Blit_Copy` |
| `0x00437aa4` | `FUN_00437aa4` | 481 | FUN_00436512, FUN_0040e2d4, FUN_00466d0d, FUN_00466cc6, F... | `D:\mm7Src_eng\MM7\Code\Core3D.cpp, draw_debug_line() not... |
| `0x00437c85` | `FUN_00437c85` | 180 | FUN_0040e2d4, FUN_00466d0d, FUN_00466cc6 | `D:\mm7Src_eng\MM7\Code\Core3D.cpp, draw_debug_line() not... |
| `0x0045274b` | `FUN_0045274b` | 245 | FUN_004cb7ec, FUN_004cc0fe, FUN_004cb669, FUN_004cb4ec, F... | `logd3d.txt, D3D texture name:  %s  offset: %x\n` |
| `0x00466ec3` | `FUN_00466ec3` | 1794 | _strlen, FUN_0040e33a, FUN_0040e302, FUN_00466d87, FUN_00... | `DDERR_ALREADYINITIALIZED, DDERR_INVALIDPARAMS, DDERR_OUT... |
| `0x00469ae8` | `FUN_00469ae8` | 191 | GetCursorPos, ScreenToClient |  |
| `0x00486b52` | `FUN_00486b52` | 1092 | FUN_0040e2d4, FUN_00466d0d, FUN_0045cbc2 | `D:\mm7Src_eng\MM7\Code\Odspan.cpp, The Texture Frame Tab... |
| `0x0049d94c` | `FUN_0049d94c` | 620 | _memset, _strlen, DirectDrawCreate, operator_new, FUN_004... |  |
| `0x0049dbb8` | `FUN_0049dbb8` | 48 | _memset, DirectDrawEnumerateA, operator_new |  |
| `0x0049e4dd` | `FUN_0049e4dd` | 135 | GetClientRect, ClientToScreen, OffsetRect |  |
| `0x0049e922` | `FUN_0049e922` | 487 | FUN_00452373, FUN_00464a2c, FUN_004523ab, FUN_004cb450 | `Use D3D, startinwindow, D3D Device, Colored Lights, Deta... |
| `0x0049fbc7` | `FUN_0049fbc7` | 259 | GetClientRect, ClientToScreen, OffsetRect, FUN_004a18da, ... |  |
| `0x004a0ed0` | `FUN_004a0ed0` | 242 | _memset, FUN_0046b153, FUN_00466cc6, FUN_0046b109, FUN_00... | `D:\mm7Src_eng\MM7\Code\Screen16.cpp` |
| `0x004a1074` | `FUN_004a1074` | 133 | DirectDrawCreate, FUN_00466cc6 | `D:\mm7Src_eng\MM7\Code\Screen16.cpp` |
| `0x004a1156` | `FUN_004a1156` | 86 | FUN_00466cc6 | `D:\mm7Src_eng\MM7\Code\Screen16.cpp` |
| `0x004a11ac` | `FUN_004a11ac` | 177 | _memset, FUN_00466cc6 | `D:\mm7Src_eng\MM7\Code\Screen16.cpp` |
| `0x004a125d` | `FUN_004a125d` | 205 | _memset, FUN_00466cc6 | `D:\mm7Src_eng\MM7\Code\Screen16.cpp` |
| `0x004a132a` | `FUN_004a132a` | 346 | _memset, FUN_00466cc6 | `D:\mm7Src_eng\MM7\Code\Screen16.cpp` |
| `0x004a1484` | `FUN_004a1484` | 228 | FUN_00466cc6 | `D:\mm7Src_eng\MM7\Code\Screen16.cpp` |
| `0x004a1671` | `FUN_004a1671` | 77 | FUN_00466cc6 | `D:\mm7Src_eng\MM7\Code\Screen16.cpp` |
| `0x004a1757` | `FUN_004a1757` | 77 | FUN_00466cc6 | `D:\mm7Src_eng\MM7\Code\Screen16.cpp` |
| `0x004a1814` | `FUN_004a1814` | 113 | FUN_00466cc6 | `D:\mm7Src_eng\MM7\Code\Screen16.cpp` |
| `0x004a1885` | `FUN_004a1885` | 85 | FUN_00466cc6 | `D:\mm7Src_eng\MM7\Code\Screen16.cpp` |
| `0x004a1fe0` | `FUN_004a1fe0` | 1644 | FUN_0047c500, FUN_0044ee5c, FUN_0045d788, FUN_0047c3db, F... | `D:\mm7Src_eng\MM7\Code\screen16_3d.cpp` |
| `0x004a264c` | `FUN_004a264c` | 1767 | FUN_0047c500, FUN_0045d788, FUN_0047c3db, FUN_00437a44, F... | `D:\mm7Src_eng\MM7\Code\screen16_3d.cpp` |
| `0x004a2f50` | `FUN_004a2f50` | 1423 | FUN_0045d788, FUN_00466cc6, FUN_0044ee10 | `D:\mm7Src_eng\MM7\Code\screen16_3d.cpp` |
| `0x004a4d71` | `FUN_004a4d71` | 615 | _memset, FUN_00452504, FUN_0049e564, FUN_00450e1d, FUN_00... | `HiScreen16::LoadTexture - D3Drend->CreateTexture() faile... |
| `0x004a4fd8` | `FUN_004a4fd8` | 328 | _memset, FUN_00452504, FUN_0049e564, FUN_004ca2cd, FUN_00... | `HiScreen16::LoadTexture - D3Drend->CreateTexture() faile... |
| `0x004a520d` | `FUN_004a520d` | 116 | FUN_00466cc6, FUN_004a1671 | `D:\mm7Src_eng\MM7\Code\screen16blt.cpp` |
| `0x004a5779` | `FUN_004a5779` | 404 | FUN_00466cc6 | `D:\mm7Src_eng\MM7\Code\screen16blt.cpp` |
| `0x004abc13` | `FUN_004abc13` | 192 | RegOpenKeyExA, RegSetValueExA, RegQueryValueExA, RegCloseKey | `SOFTWARE\Aureal\A3D, SplashAudio, SplashScreen` |
| `0x004ac6f8` | `FUN_004ac6f8` | 43 | FUN_00461812, FUN_004618c7 | `sprites08` |
| `0x004ac723` | `FUN_004ac723` | 794 | FUN_004615bd, FUN_004624a0, FUN_004ac5e1, FUN_0048a3a2, F... | `hardSprites, pending` |
| `0x004be8bd` | `FUN_004be8bd` | 126 | MessageBoxA, FUN_004a1568 | `Smacker Error, Unsupported pixel format.` |

### Video Playback (15 functions)

> Smacker and Bink video playback, frame decoding, buffer management

| Address | Name | Size | Key Callees | Referenced Strings |
|---------|------|-----:|-------------|-------------------|
| `0x00463186` | `FUN_00463186` | 1687 | _memset, __ftol, PeekMessageA, _SmackWait@4, TranslateMes... | `data\lloyd%d%d.pcx, out02.odm, out01.odm` |
| `0x004a94bd` | `FUN_004a94bd` | 241 | FUN_0040e2d4, FUN_00467838, FUN_004be671, FUN_00466d0d, F... | `D:\mm7Src_eng\MM7\Code\Show.cpp, No movie, 3dologo, new ... |
| `0x004be671` | `FUN_004be671` | 588 | ShowCursor, DispatchMessageA, PeekMessageA, _SmackWait@4,... |  |
| `0x004beaa4` | `FUN_004beaa4` | 150 | _BinkDDSurfaceType@4, _SmackUseMMX@4, _BinkSetSoundSystem... |  |
| `0x004beb3a` | `FUN_004beb3a` | 230 | _memset, _SmackBufferClose@4, _BinkPause@8, Sleep, _BinkC... |  |
| `0x004bec38` | `FUN_004bec38` | 122 | _BinkDoFrame@4, _SmackDoFrame@4, _BinkNextFrame@4, _Smack... |  |
| `0x004becb2` | `FUN_004becb2` | 284 | _BinkGetRects@8, _BinkDoFrame@4, _BinkCopyToBuffer@28, _B... |  |
| `0x004bedce` | `FUN_004bedce` | 544 | GetFocus, _SmackBlitSetPalette@12, _SmackDoFrame@4, _Smac... |  |
| `0x004beff1` | `FUN_004beff1` | 182 | _SmackDoFrame@4, _SmackToBuffer@28, _SmackBufferNewPalett... |  |
| `0x004bf0a7` | `FUN_004bf0a7` | 165 | SetFilePointer, _BinkOpen@8, FUN_004caaf0 |  |
| `0x004bf14c` | `FUN_004bf14c` | 169 | SetFilePointer, _SmackOpen@12, FUN_004caaf0 |  |
| `0x004bf1f5` | `FUN_004bf1f5` | 362 | __ftol, _SmackToBuffer@28, _SmackBufferOpen@24, _SmackVol... | `D:\mm7Src_eng\MM7\Code\Video.cpp, Unsupported Bink playb... |
| `0x004bf377` | `FUN_004bf377` | 417 | MessageBoxA, radmalloc, _SmackToBuffer@28, _SmackBlitOpen... | `%s.bik, %s.smk, Can't load file - anims\%s.smk, Can't al... |
| `0x004bf518` | `FUN_004bf518` | 394 | _BinkGoto@12, _SmackDoFrame@4, _SmackNextFrame@4, _BinkDo... |  |
| `0x004bfd95` | `FUN_004bfd95` | 784 | _memset, _BinkDDSurfaceType@4, _BinkBufferSetOffset@12, G... | `D:\mm7Src_eng\MM7\Code\Video.cpp` |

### Audio System (10 functions)

> Miles Sound System (AIL) integration, sound loading, 3D audio

| Address | Name | Size | Key Callees | Referenced Strings |
|---------|------|-----:|-------------|-------------------|
| `0x004264f5` | `FUN_004264f5` | 385 | GlobalMemoryStatus, FUN_004cb656, FUN_004cad70, FUN_004cb... | `Mem%03i.txt, Windows Memory Status, dwLength:         %d... |
| `0x004a98e3` | `FUN_004a98e3` | 276 | _AIL_WAV_info@8, _AIL_decompress_ADPCM@12, _AIL_file_type... |  |
| `0x004a9dcd` | `FUN_004a9dcd` | 76 | FUN_004cb46f, FUN_004cb656, FUN_004cb4ec, FUN_00466be9 | `data\dsounds.bin, Unable to save dsounds.bin!` |
| `0x004aa185` | `FUN_004aa185` | 36 | _AIL_redbook_set_volume@8 |  |
| `0x004aa1a9` | `FUN_004aa1a9` | 68 | __ftol, _AIL_set_digital_master_volume@8 |  |
| `0x004ab84e` | `FUN_004ab84e` | 566 | __ftol, _AIL_redbook_tracks@4, _AIL_startup@0, _SmackSoun... | `Disable3DSound, 3DSoundProvider, EAX environment selection` |
| `0x004aba84` | `FUN_004aba84` | 399 | _strlen, _AIL_waveOutOpen@16, _AIL_digital_configuration@... | `Device: , Emulated` |
| `0x004abcd3` | `FUN_004abcd3` | 250 | _AIL_shutdown@0, _AIL_redbook_close@4, CloseHandle, _AIL_... | `Disable3DSound` |
| `0x004abf7c` | `FUN_004abf7c` | 61 | _AIL_set_3D_provider_preference@12 | `EAX effect volume, EAX damping` |
| `0x004ac01a` | `FUN_004ac01a` | 246 | _AIL_allocate_3D_sample_handle@4, _AIL_set_3D_sample_dist... | `Maximum supported samples, EAX environment selection` |

### Input System (22 functions)

> DirectInput keyboard/mouse, async input, cursor management

| Address | Name | Size | Key Callees | Referenced Strings |
|---------|------|-----:|-------------|-------------------|
| `0x0043b790` | `FUN_0043b790` | 118 | DirectInputCreateA, FUN_0043b89e, FUN_00466cc6, FUN_0043b854 | `D:\mm7Src_eng\MM7\Code\DirectInputKeyboard.cpp` |
| `0x0043b854` | `FUN_0043b854` | 74 | __CxxThrowException@8, FUN_00466cc6 | `D:\mm7Src_eng\MM7\Code\DirectInputKeyboard.cpp, Error: N... |
| `0x0043b89e` | `FUN_0043b89e` | 113 | FUN_00466cc6 | `D:\mm7Src_eng\MM7\Code\DirectInputKeyboard.cpp` |
| `0x0043b90f` | `FUN_0043b90f` | 130 | FUN_0040e2d4, FUN_00467838, FUN_00466cc6 | `D:\mm7Src_eng\MM7\Code\DirectInputKeyboard.cpp, Invalid ... |
| `0x0043b991` | `FUN_0043b991` | 75 | FUN_0040e2d4, FUN_00467838 | `D:\mm7Src_eng\MM7\Code\DirectInputKeyboard.cpp, Invalid ... |
| `0x0043ba22` | `FUN_0043ba22` | 129 | DirectInputCreateA, FUN_0043bb3b, FUN_0043baf1, FUN_00466cc6 | `D:\mm7Src_eng\MM7\Code\DirectInputMouse.cpp` |
| `0x0043baf1` | `FUN_0043baf1` | 74 | __CxxThrowException@8, FUN_00466cc6 | `D:\mm7Src_eng\MM7\Code\DirectInputMouse.cpp, Error: No m... |
| `0x0043bb3b` | `FUN_0043bb3b` | 113 | FUN_00466cc6 | `D:\mm7Src_eng\MM7\Code\DirectInputMouse.cpp` |
| `0x0045b143` | `FUN_0045b143` | 56 | __CxxThrowException@8, FUN_0045b1c7 | `Could not initialize asynchronos keyboard object` |
| `0x0045b2e0` | `FUN_0045b2e0` | 130 | EnterCriticalSection, LeaveCriticalSection, ResumeThread,... | `D:\mm7Src_eng\MM7\Code\KeyboardAsync.cpp, Invalid DI_Key... |
| `0x0045b362` | `FUN_0045b362` | 123 | EnterCriticalSection, SuspendThread, LeaveCriticalSection... | `D:\mm7Src_eng\MM7\Code\KeyboardAsync.cpp, Invalid DI_Key... |
| `0x0045b5b6` | `FUN_0045b5b6` | 692 | FUN_0040e2d4, FUN_00467838 | `D:\mm7Src_eng\MM7\Code\KeyboardAsync.cpp, Uknown key det... |
| `0x00466b90` | `FUN_00466b90` | 89 | MessageBoxA, ClipCursor, FUN_0045f4a2, FUN_004667ed, FUN_... |  |
| `0x00466be9` | `FUN_00466be9` | 91 | MessageBoxA, ClipCursor, FUN_004cc790, FUN_004667ed, FUN_... | `Error` |
| `0x00467791` | `FUN_00467791` | 167 | _strlen, FUN_0040e33a, FUN_0040e302, FUN_00466d87, FUN_00... | `Unknown Direct Input error, Direct Input Error` |
| `0x00469907` | `FUN_00469907` | 481 | GetCursorPos, _strcmp, SetCursorPos, LoadCursorA, SetClas... | `MICON2, MICON1, Arrow, Target, MICON3` |
| `0x0046acad` | `FUN_0046acad` | 285 | __CxxThrowException@8, FUN_0046ae72, FUN_0046bd0d, FUN_00... | `Could not initialize CMouseAsync object` |
| `0x0046ae9b` | `FUN_0046ae9b` | 185 | EnterCriticalSection, LeaveCriticalSection, FUN_00466cc6,... | `D:\mm7Src_eng\MM7\Code\MouseAsync.cpp` |
| `0x0046af54` | `FUN_0046af54` | 290 | _memset, FUN_0040e2d4, FUN_00466d0d, FUN_0046ae9b | `D:\mm7Src_eng\MM7\Code\MouseAsync.cpp, Could not load as... |
| `0x0046b153` | `FUN_0046b153` | 142 | EnterCriticalSection, SuspendThread, LeaveCriticalSection... | `D:\mm7Src_eng\MM7\Code\MouseAsync.cpp, DI_Mouse pointer ... |
| `0x0046b380` | `FUN_0046b380` | 164 | EnterCriticalSection, LeaveCriticalSection, FUN_0046b28d,... | `D:\mm7Src_eng\MM7\Code\MouseAsync.cpp, DI_Mouse pointer ... |
| `0x0046bbd4` | `FUN_0046bbd4` | 126 | SetWindowPos, ClipCursor, FUN_0040e2d4, FUN_00466d0d | `D:\mm7Src_eng\MM7\Code\MouseAsync.cpp, Could not clip cu... |

### Lighting (4 functions)

> Lightmap management, light element definitions

| Address | Name | Size | Key Callees | Referenced Strings |
|---------|------|-----:|-------------|-------------------|
| `0x00454e66` | `FUN_00454e66` | 276 | FUN_004caaf0 | `ARROW, ARROWF, WATER, EARTH, SPIRIT, LIGHT` |
| `0x0045d788` | `FUN_0045d788` | 775 | FUN_00466cc6, FUN_00436a13, FUN_0040e2d4, FUN_00467838, F... | `D:\mm7Src_eng\MM7\Code\Light.cpp, Invalid lightmap detec... |
| `0x0045da8f` | `FUN_0045da8f` | 146 | FUN_0040e2d4, FUN_00467838, FUN_0045db21 | `D:\mm7Src_eng\MM7\Code\Light.cpp, Invalid lightmap detec... |
| `0x0045dce2` | `FUN_0045dce2` | 561 | FUN_0045da8f, FUN_00466cc6, FUN_00436a13 | `D:\mm7Src_eng\MM7\Code\Light.cpp` |

### Visibility System (4 functions)

> Object picking, z-buffer queries, LOD array management

| Address | Name | Size | Key Callees | Referenced Strings |
|---------|------|-----:|-------------|-------------------|
| `0x0044ea8a` | `FUN_0044ea8a` | 180 | FUN_0040e2d4, FUN_00466d0d, FUN_0044eb86, FUN_004c05b8 | `D:\mm7Src_eng\MM7\Code\Game.cpp, The 'Vis' object pointe... |
| `0x00461812` | `FUN_00461812` | 181 | FUN_004cb7ec, FUN_0046146e, FUN_004cac80, FUN_004cb8a5, F... | `LOD CArray` |
| `0x004c1b1c` | `FUN_004c1b1c` | 71 | FUN_0040e2d4, FUN_00466d0d | `D:\mm7Src_eng\MM7\Code\Vis.cpp, Undefined type requested... |
| `0x004c2503` | `FUN_004c2503` | 185 | FUN_0040e2d4, FUN_00466d0d | `D:\mm7Src_eng\MM7\Code\Vis.cpp, Unknown pointer creation... |

### Gamma Control (2 functions)

> Display gamma correction

| Address | Name | Size | Key Callees | Referenced Strings |
|---------|------|-----:|-------------|-------------------|
| `0x0044f350` | `FUN_0044f350` | 83 | FUN_00466cc6, FUN_0040e2d4, FUN_00467838 | `D:\mm7Src_eng\MM7\Code\GammaControl.cpp, Gamma control n... |
| `0x0044f434` | `FUN_0044f434` | 83 | FUN_00466cc6, FUN_0040e2d4, FUN_00467838 | `D:\mm7Src_eng\MM7\Code\GammaControl.cpp, Gamma control n... |

### Time/Timing (5 functions)

> Key bindings, keyboard polling, timer management, credits scroll

| Address | Name | Size | Key Callees | Referenced Strings |
|---------|------|-----:|-------------|-------------------|
| `0x0045a999` | `FUN_0045a999` | 618 | FUN_00464b3f, FUN_0045ae65 | `KEY_FORWARD, KEY_BACKWARD, KEY_LEFT, KEY_RIGHT, KEY_ATTA... |
| `0x0045b3ef` | `FUN_0045b3ef` | 233 | EnterCriticalSection, LeaveCriticalSection, timeGetTime, ... | `D:\mm7Src_eng\MM7\Code\KeyboardAsync.cpp, Invalid DI_Key... |
| `0x0046b5d9` | `FUN_0046b5d9` | 353 | EnterCriticalSection, LeaveCriticalSection, timeGetTime, ... |  |
| `0x0046ba91` | `FUN_0046ba91` | 95 | timeGetTime |  |
| `0x0049795a` | `FUN_0049795a` | 1524 | _memset, _strncpy, WaitMessage, operator_new, timeGetTime... | `FONTPAL, quick.fnt, cchar.fnt, mm6title.pcx, credits.txt... |

### Configuration (2 functions)

> Exception handling frame setup

| Address | Name | Size | Key Callees | Referenced Strings |
|---------|------|-----:|-------------|-------------------|
| `0x004cb140` | `FUN_004cb140` | 84 | __CallSettingFrame@12 |  |
| `0x004d134d` | `FUN_004d134d` | 132 | __CallSettingFrame@12, FUN_004d18bd |  |

---

## Notes

- Addresses are virtual addresses from the original MM7-Rel.exe PE binary.
- Function names prefixed with `FUN_` are auto-generated by Ghidra and have not been renamed.
- The "Key Callees" column shows notable functions called by each function (named APIs prioritized, up to 5 shown).
- The "Referenced Strings" column shows string literals found in the function body.
- Source file paths appearing in strings (e.g., `D:\mm7Src_eng\...`) are from the original debug info embedded in the binary.
- CRT functions (39) and unclassified functions (1,200) are omitted from the detailed listings to keep this index focused on game systems.

---

*Trademark Notice: Not affiliated with or endorsed by the IP holder. All trademarks belong to their respective owners.*
