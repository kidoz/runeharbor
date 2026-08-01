---
title: "Global Variables Reference"
summary: "This catalog records identified global variables, addresses, layouts, and subsystem uses."
doc_type: reference
status: partial
last_updated: 2026-08-01
---
# Global Variables Reference

This catalog records identified global variables, addresses, layouts, and subsystem uses.

Coverage is limited to MM7 v1.21 and the artifacts cited in Scope.

## Scope

This page covers MM7 v1.21 behavior and the binary or data artifacts cited below.
Unmarked findings are **observed**; deductions and gaps are labeled **inferred**
or **unknown** per the [documentation standard](../contributing/documentation-standard.md).
Artifacts and versions not cited on this page are out of scope.
RuneHarbor-specific decisions, when present, belong in Integration notes.

## Overview

MM7-Rel.exe is a 32-bit Windows PE executable compiled with Microsoft Visual C++ 6.0.
Global variables reside in the `.data` and `.bss` sections, spanning roughly from
`0x004D0000` to `0x00F9FFFF`. The Ghidra export identified 258 entries total, of which
the majority are either switch-table data (compiler-generated jump tables embedded in the
`.text` section), Windows TEB (Thread Environment Block) fields at `0xFFFxx000`, RTTI
type descriptors, or PE resource entries. After filtering these out, the game-specific
globals described in the system docs form the authoritative catalog below.

**Filtering criteria applied to the Ghidra TSV:**

- **Excluded:** All `switchdataD_*` entries (lines 2--142) -- compiler-generated jump
  tables for switch statements, not game state.
- **Excluded:** `vftable` at `0x004DB15C` -- C++ vtable pointer, not game state.
- **Excluded:** All `RTTI_Type_Descriptor` entries (lines 144--153) -- MSVC RTTI metadata.
- **Excluded:** All `Rsrc_*` entries (lines 154--171) -- PE resource data (dialogs,
  icons, bitmaps, menus, cursors, version info).
- **Excluded:** All TEB fields at `0xFFFxx000` (lines 173--258) -- Windows Thread
  Environment Block, OS-level, not game state.

**Remaining game-specific globals** are cataloged below, sourced from the reverse
engineering system documentation which provides richer context than the raw Ghidra export.

---

## Core / Game State

Primary state machine variables and engine-level globals controlling program flow.

| Address | Name | Type | Size | Description | Referenced By |
|---------|------|------|------|-------------|---------------|
| `0x006A0BC4` | GameFlowState | u32 | 4 | Top-level program flow (0=title, 1=new game, 2=credits, 3=load, 4=gameplay, 5=return to title, 6=level transition, 9=exit, 10=file browser) | WinMain (0x00462CD1), TitleScreenLoop (0x004627F4), GameplayLoop (0x00463186) |
| `0x004E28D8` | CurrentScreenMode | u32 | 4 | Active UI screen/mode (0=gameplay, 2=options, 3=modal, 4=NPC dialog, 5=chest, 10=char creation, 11=save, 12=load, 13=rest, 16=map, 17=spellbook, 18=quick spell, 19=autonotes, 21=awards, 22=text) | Referenced in hundreds of functions |
| `0x005067F8` | PreviousScreenMode | u32 | 4 | Saved screen mode before entering a modal screen | Screen mode transition functions |
| `0x006A0BC8` | GameplaySubState | u32 | 4 | Sub-states within gameplay loop (0=idle, 1=transitioning, 2=level loaded, 3=saving, 4=loading, 6=level change, 7=reset, 8=special, 9=exit to title) | GameplayLoop (0x00463186) |
| `0x006BE174` | MainWindowHandle | HWND | 4 | Win32 main game window handle | WndProc (0x00463828), InitializeEngine (0x00465245) |
| `0x006BE158` | WindowClassName | char[] | ~32 | Registered Win32 window class name string | InitializeEngine (0x00465245) |
| `0x006BE1E0` | MapType | u32 | 4 | Current map type: 1=indoor (BLV), 2=outdoor (ODM) | IndoorRenderer (0x00427DB8), OutdoorRenderer (0x004304D6), spell restriction checks |
| `0x006BE1E4` | EngineFlags | u32 | 4 | Engine flag bitfield (bit 2=nointro, bit 3=nologo, bit 4=nosound, bit 5=nowalksound, bit 6=noanim) | InitializeEngine, WndProc, audio init |
| `0x006BE1E5` | ActivityFlags | u8 | 1 | WM_ACTIVATEAPP pause/resume state | WndProc (0x00463828) |
| `0x006BE1ED` | RegistryFlag | u8 | 1 | Non-zero if Windows registry is used for settings | InitializeEngine (0x00465245) |
| `0x006BE1EE` | CDROMFlag | u8 | 1 | Non-zero if CD-ROM fallback is active | InitializeEngine (0x00465245) |
| `0x0071FE88` | UseDefsFlag | u32 | 4 | 1=load text (.txt/.def) data tables instead of binary (.bin) | WinMain (0x00462CD1), InitializeEngine |
| `0x00576EAC` | RedrawFlag | u32 | 4 | 1=trigger full screen redraw | EventDispatcher (0x00435737), rendering |

---

## Game Time / Calendar

64-bit game clock and frame timing variables.

| Address | Name | Type | Size | Description | Referenced By |
|---------|------|------|------|-------------|---------------|
| `0x00ACCE64` | GameTimeLow | u32 | 4 | 64-bit game clock, low dword (128 ticks/second) | All timer/buff/condition checks, event scheduler, spell durations |
| `0x00ACCE68` | GameTimeHigh | u32 | 4 | 64-bit game clock, high dword | Same as GameTimeLow |
| `0x006E2028` | FrameTimer | u32 | 4 | Real-time tick count (from GetTickCount/timeGetTime) | Frame delta computation |
| `0x006E202C` | FrameCounter | u32 | 4 | Incremented each frame | Frame counting |
| `0x0050BA54` | FrameDeltaTicks | i32 | 4 | Time elapsed this frame in game ticks (used for AI timers, overlay animation) | AI_UpdateAll (0x00401A91), SpellEffects_AdvanceOverlays |
| `0x0050BA7C` | FrameDeltaCast | i32 | 4 | Frame time delta for casting animation | SpellEffects_Update (0x004A8BB7) |

---

## Party Position / Physics

World-space position, orientation, and physical dimensions of the party.

| Address | Name | Type | Size | Description | Referenced By |
|---------|------|------|------|-------------|---------------|
| `0x00ACCE3C` | PartyHeight | i32 | 4 | Bounding cylinder height (default 192 / 0xC0) | Collision detection, INI loader |
| `0x00ACCE40` | PartyCurrentHeight | i32 | 4 | Runtime height (differs while jumping/falling) | Physics system |
| `0x00ACCE44` | PartyEyeLevel | i32 | 4 | Camera Z offset from ground (default 160 / 0xA0) | Camera::SetupViewParams (0x004407FC), INI loader |
| `0x00ACCE48` | PartyCurrentEyeLevel | i32 | 4 | Runtime eye level | Camera setup |
| `0x00ACCE50` | ViewDistance | i32 | 4 | View distance / zoom factor | Camera::SetupViewParams |
| `0x00ACD4EC` | PartyPosX | i32 | 4 | Party world X position | Movement, AI distance, spell targeting, camera |
| `0x00ACD4F0` | PartyPosY | i32 | 4 | Party world Y position | Same |
| `0x00ACD4F4` | PartyPosZ | i32 | 4 | Party world Z (altitude) | Same |
| `0x00ACD4F8` | PartyFacing | i32 | 4 | Yaw angle (0--2047, 0=East, 512=North) | Camera, projectile direction, AI |
| `0x00ACD4FC` | PartyPitch | i32 | 4 | Pitch angle (look up/down) | Camera, projectile targeting |
| `0x00ACD500` | SavedPosX | i32 | 4 | Respawn/recall X position | Teleport, Lloyd's Beacon |
| `0x00ACD504` | SavedPosY | i32 | 4 | Respawn/recall Y position | Same |
| `0x00ACD508` | SavedPosZ | i32 | 4 | Respawn/recall Z position | Same |
| `0x00ACD50C` | SavedFacing | i32 | 4 | Respawn/recall yaw | Same |
| `0x00ACD510` | SavedPitch | i32 | 4 | Respawn/recall pitch | Same |
| `0x00ACD520` | ViewZOffset | i32 | 4 | Additional camera Z offset | Camera system |
| `0x00ACD538` | PhysicsZ | i32 | 4 | Copy of party Z used by physics engine | Movement/collision |
| `0x00AD45B0` | MovementFlags | u32 | 4 | Movement mode bits (bit 3=fly, bits 4-5=water walk/special) | Spell effects (Fly, Water Walk) |

---

## Character & Party

Player character records, party-level state, and selection tracking.

| Address | Name | Type | Size | Description | Referenced By |
|---------|------|------|------|-------------|---------------|
| `0x00ACD804` | PlayerRecordBase | struct[4] | 27,888 | Base of 4 player records (each 0x1B3C = 7,004 bytes; spans to 0x00AD44F4) | All player stat, combat, inventory, skill functions |
| `0x00AD44F4` | PlayerRecordEnd | -- | -- | Upper bound past Player 3's last byte (used in loop comparisons) | Player iteration loops |
| `0x00507A6C` | ActiveCharIndex | i32 | 4 | Currently selected character index (0--3) | UI, event processor, combat targeting |
| `0x005061C8` | SecondaryCharIndex | i32 | 4 | Secondary character selection index | Some UI operations |
| `0x00ACD59C` | AlignmentFlags | i32 | 4 | Alignment-related flags | Alignment checks |
| `0x00ACD6C0` | PartyAlignment | i32 | 4 | 0=good, 1=neutral, 2=evil (affects UI theme, quest outcomes, NPC reactions) | UI theme selection, event processor |
| `0x00ACD5F2` | ArtifactFoundFlags | byte[29] | 29 | Tracks which artifacts have been found (1=found, max 13 simultaneous) | ItemGen_Generate (0x0045664C) |
| `0x00AD458C` | CursorItemId | i32 | 4 | Currently selected/hovered item ID (item on cursor) | Inventory UI |
| `0x00AE3060` | PartyXPAccumulator | i32 | 4 | Party experience accumulator (clamped 0--4,000,000) | Monster_OnDeath (0x00438CE2) |

---

## Combat System

Turn-based/real-time combat state and combat sound IDs.

| Address | Name | Type | Size | Description | Referenced By |
|---------|------|------|------|-------------|---------------|
| `0x00ACD6B4` | CombatMode | i32 | 4 | 0=real-time, 1=turn-based combat | AI_UpdateAll, spell casting, recovery timer, combat functions |
| `0x00ACD554` | SpellSchoolRestrict | i32 | 4 | Active spell school restriction | Spell casting handler |
| `0x00ACD558` | SpellRestrictionFlag | i32 | 4 | Additional spell restriction flag | Spell casting handler |
| `0x004EA2B0` | NoDamageFlag | i32 | 4 | `[debug] nodamage` -- disables all damage when set | Combat damage functions |
| `0x0050C7EC` | CombatSoundTurn0 | i32 | 4 | Sound ID for "turn0" indicator | Combat_LoadSounds (0x0042F3B2) |
| `0x0050C7F0` | CombatSoundTurn1 | i32 | 4 | Sound ID for "turn1" indicator | Same |
| `0x0050C7F4` | CombatSoundTurn2 | i32 | 4 | Sound ID for "turn2" indicator | Same |
| `0x0050C7F8` | CombatSoundTurn3 | i32 | 4 | Sound ID for "turn3" indicator | Same |
| `0x0050C7FC` | CombatSoundTurn4 | i32 | 4 | Sound ID for "turn4" indicator | Same |
| `0x0050C800` | CombatSoundTurnStop | i32 | 4 | Sound ID for "turnstop" | Same |
| `0x0050C804` | CombatSoundTurnHour | i32 | 4 | Sound ID for "turnhour" | Same |
| `0x0050C810` | CombatSoundTurnStart | i32 | 4 | Sound ID for "turnstart" | Same |
| `0x004F86F4` | TurnBasedProjectileCounter | i32 | 4 | Turn-based projectile counter | Spell/combat turn processing |
| `0x004F86DC` | TurnBasedActionType | i32 | 4 | Current turn-based action type | Turn-based AI |
| `0x00AE2F74` | EarthquakeTimer | i32 | 4 | Earthquake active timer (damages all actors when > 0) | AI_UpdateAll (0x00401A91) |
| `0x00AE2F78` | EarthquakeDamageBase | i32 | 4 | Base damage for active earthquake | AI_UpdateAll |

---

## Monster / Actor System

Actor array, visibility, hostility, and AI-related globals.

| Address | Name | Type | Size | Description | Referenced By |
|---------|------|------|------|-------------|---------------|
| `0x005FEFFC` | ActorArrayBase | u32[] | variable | Actor array base (stride 0x344 = 836 bytes per actor) | All AI, combat, and monster functions |
| `0x005FF000` | ActorHPArray | i16[] | variable | Actor HP values (stride 0x1A2 shorts, offset +4 from base) | DamageMonsterFromParty (0x00439463), HP checks |
| `0x005FF088` | ActorAIStateArray | i16[] | variable | Actor AI state values (stride 0x1A2, offset +0x8C from base) | AI state machine |
| `0x006650A8` | ActorCount | i32 | 4 | Total active actor/monster count | AI_UpdateAll, combat encounter detection, visibility sorting |
| `0x005FEFD0` | MonsterTypeTable | ptr | 4 | Monster type table base (0x98 = 152 bytes per entry) | Condition expiry, sprite reset, stat lookups |
| `0x005CCCD1` | MonsterDefaultHostility | u8[] | variable | Per-monster-type default hostility (stride 0x58 = 88 bytes) | AI_FindNearestTarget, hostility reset |
| `0x005CCCF6` | MonsterBloodFlags | u8[] | variable | Per-monster-type blood flags (stride 0x58; bit 0 = has blood) | Bloodsplat on death (0x0049B419) |
| `0x004F7458` | VisibleActorCount | i32 | 4 | Count of visible/active actors (max 30) | AI visibility sort |
| `0x004F7C30` | VisibleActorIndices | i32[] | 120 | Sorted list of visible actor indices (30 entries) | AI_UpdateAll |
| `0x004F7460` | VisibleActorDistances | i32[] | 120 | Distances for sorted visible actors | AI_UpdateAll |
| `0x004F64B8` | SecondaryActorList | i32[] | variable | Secondary actor list (indoor LOS-verified) | Indoor AI processing |
| `0x004F5CE8` | SecondaryDistanceList | i32[] | variable | Secondary distance list | Indoor AI processing |
| `0x004F6C88` | ActorTargetCache | i32[] | variable | Per-actor target ID cache | AI target selection |
| `0x005C8B40` | HostilityMatrix | u8[] | ~7,921 | Hostility matrix (89 columns per row) from hostile.txt | AI_CheckHostility (0x0040104C) |

### Distance Constants (Monster AI)

| Address | Name | Type | Size | Description | Referenced By |
|---------|------|------|------|-------------|---------------|
| `0x004DF380` | AggressionRangeTable | int32[5] | 20 | Aggression range by hostility level (0, 0x400, 0xA00, 0x1400, 0x2800) | AI_FindNearestTarget |
| `0x004DF390` | MaxAggressionRange | i32 | 4 | Maximum aggression range | AI_FindNearestTarget |
| `0x004D8430` | BaseMeleeRange | double | 8 | Base melee engagement range | AI melee decision |
| `0x004D8440` | FleeThresholdAggressive | float | 4 | HP flee threshold multiplier (aggressive personality) | AI flee decision |
| `0x004D8444` | FleeThresholdNormal | float | 4 | HP flee threshold multiplier (normal personality) | AI flee decision |

---

## Spell System

Spell data tables, mana cost tables, and spell effect state.

| Address | Name | Type | Size | Description | Referenced By |
|---------|------|------|------|-------------|---------------|
| `0x005CBECC` | SpellDataTable | base | variable | Spell data table (0x24 = 36 byte stride per spell) -- damage type, school, descriptions | SpellData_GetDamageType (0x0048E189), SpellCast_ProcessAll (0x00427DB8) |
| `0x004E3C46` | ManaCostTable | u16[] | variable | Mana cost per spell indexed by (mastery + spell_id * 10) * 2 | Spell casting mana check |
| `0x004E3C4E` | RecoveryTimeTable | u16[] | variable | Spell recovery time table | Spell casting recovery |
| `0x004E3C52` | WeaponDamageTypeTable | u16[] | variable | Weapon damage type lookup table | Spell damage calculation |
| `0x004E3C58` | ResistanceTable | i32[] | variable | Base resistance table (indexed by damage type, stride 0x14 = 20 bytes) | Monster_CalculateResistance (0x0043B006) |
| `0x004E3C6E` | SpellTargetFlags | u8[] | variable | Target flags per spell (stride 0x14) -- bits: 0=monsters, 1=environment, 2=caster, 3=special | Spell targeting validation |
| `0x005E4AF4` | SpellsTxtBuffer | ptr | 4 | Loaded spells.txt file buffer | SpellData_LoadFromFile (0x00453876) |
| `0x005E4FC4` | SpellEffectRedrawFlag | i32 | 4 | Spell effect redraw flag | SpellEffects_Update (0x004A8BB7) |
| `0x005C84E8` | AngularConversionConst | i32 | 4 | Angular conversion constant for projectile spray | Multi-projectile spell targeting |

---

## Item System

Item description tables, enchantment tables, and item generation state.

| Address | Name | Type | Size | Description | Referenced By |
|---------|------|------|------|-------------|---------------|
| `0x005D2864` | ItemDescTable | base | ~38,400 | Item description table (stride 0x30 = 48 bytes, 800 entries max) -- icon, name, value, type, damage | All item functions, combat damage |
| `0x005D2868` | ItemNamePtrs | ptr[] | variable | Item name string pointers (within desc table) | Item display |
| `0x005D2874` | ItemBaseValue | i32[] | variable | Item base gold value (within desc table) | Item_GetFullPrice (0x0045646E) |
| `0x005D2878` | ItemSpriteId | i16[] | variable | Item sprite ID for world display (within desc table) | Ground item rendering |
| `0x005D2880` | ItemEquipCategory | u8[] | variable | Item equip type / enchant chance (within desc table) | Equipment logic, enchantment |
| `0x005D2881` | ItemWeaponSkill | u8[] | variable | Item weapon skill type (within desc table) | Combat: skill checks, damage type |
| `0x005D2882` | ItemDamageDiceCount | u8[] | variable | Damage dice count per item type | CalculateMeleeBaseDamage (0x0048CDC1) |
| `0x005D2883` | ItemDamageDiceSides | u8[] | variable | Damage dice sides per item type | Same |
| `0x005D2884` | ItemDamageBonus | u8[] | variable | Damage bonus per item type | Same |
| `0x005DBE50` | StdEnchantTable | base | ~480 | Standard enchantment table (stride 0x14 = 20 bytes, 24 entries) | Enchantment application, item display |
| `0x005DC028` | SpcEnchantTable | base | ~2,016 | Special enchantment table (stride 0x1C = 28 bytes, 72 entries) | Enchantment application, combat on-hit effects |
| `0x005DC03C` | SpcEnchantValueMult | i32[] | variable | Special enchant gold value multiplier (within spc table) | Item_GetFullPrice |
| `0x005E4AEC` | ItemsTxtBuffer | ptr | 4 | items.txt raw file buffer | ItemData_LoadAll (0x00456DBE) |
| `0x005E4B00` | PotionTxtBuffer | ptr | 4 | potion.txt raw file buffer | Potion_LoadMixingTable (0x00453B68) |
| `0x005E4B04` | PotNotesTxtBuffer | ptr | 4 | potnotes.txt raw file buffer | Potion_LoadAutonotes (0x00453D11) |
| `0x005CAA67` | MapTreasureLevelTable | u8[] | variable | Per-map treasure level table | MonsterLoot_Generate (0x00450244) |

---

## Character Data Tables

Stat modifier tables, class tables, condition tables, and skill descriptions.

| Address | Name | Type | Size | Description | Referenced By |
|---------|------|------|------|-------------|---------------|
| `0x004ED610` | ClassHPMultiplier | u8[] | ~36 | HP multiplier per level, indexed by classId | Player_GetMaxHP (0x0048E4F0) |
| `0x004ED634` | ClassSPMultiplier | u8[] | ~36 | SP multiplier per level, indexed by classId | Player_GetMaxSP (0x0048E55D) |
| `0x004ED5F8` | ClassTierBaseHP | u8[] | ~9 | Base HP per class tier (indexed by classId >> 2) | Player_GetMaxHP |
| `0x004ED604` | ClassTierBaseSP | u8[] | ~9 | Base SP per class tier | Player_GetMaxSP |
| `0x004EDEA4` | StatThresholdTable | u16[] | variable | Stat value to modifier threshold table | Player_StatToModifier (0x0048EA13) |
| `0x004EDEE0` | StatModifierValues | i8[] | variable | Stat threshold to modifier value mapping | Player_StatToModifier |
| `0x004EDE78` | AgeThresholds | int32[4] | 16 | Age thresholds for stat penalties (4 brackets) | Player stat bonus getters |
| `0x004EDDA0` | ConditionPriorityOrder | i32[] | ~72 | Condition check priority order (18 entries) | Player_GetWorstCondition (0x0048E9EC) |
| `0x004EDDF0` | CondMightMultTable | u8[] | ~19 | Condition -> Might stat multiplier % | Stat bonus calculation |
| `0x004EDE03` | CondIntellectMultTable | u8[] | ~19 | Condition -> Intellect stat multiplier % | Same |
| `0x004EDE16` | CondPersonalityMultTable | u8[] | ~19 | Condition -> Personality stat multiplier % | Same |
| `0x004EDE29` | CondEnduranceMultTable | u8[] | ~19 | Condition -> Endurance stat multiplier % | Same |
| `0x004EDE3C` | CondAccuracyMultTable | u8[] | ~19 | Condition -> Accuracy stat multiplier % | Same |
| `0x004EDE4F` | CondSpeedMultTable | u8[] | ~19 | Condition -> Speed stat multiplier % | Same |
| `0x004EDE62` | CondLuckMultTable | u8[] | ~19 | Condition -> Luck stat multiplier % | Same |
| `0x005C89DC` | SkillDescTextPtr | ptr | 4 | Skill description text buffer (from skilldes.txt) | Skill UI display |
| `0x00AE3070` | ClassNameStrings | char*[] | variable | Class name string pointer array | Class display, UI |
| `0x00AE3100` | ConditionNameStrings | char*[] | variable | Condition name string pointer array | Condition display, UI |
| `0x004EDD48` | RecoveryMultiplierTable | i32[] | variable | Weapon mastery -> recovery time multiplier | Recovery time calculation |
| `0x004EDD5C` | RecoveryRandomTable | i32[] | variable | Random recovery time modifiers (5 entries) | Recovery time randomization |

---

## Rendering System

DirectDraw/Direct3D state, framebuffer pointers, and pixel format globals.

| Address | Name | Type | Size | Description | Referenced By |
|---------|------|------|------|-------------|---------------|
| `0x00E31AF0` | D3DRendererPtr | ptr | 4 | Global renderer switch: NULL=software, non-NULL=D3D renderer object. Checked hundreds of times. | Every rendering function |
| `0x00E31B54` | BackBufferPixels | uint16* | 4 | Back buffer pixel data pointer (locked surface) | SW blitter, GUIWindow::DrawBackBuffer |
| `0x00E31B58` | ScreenStride | i32 | 4 | Screen stride in pixels (640) | All 2D blitting operations |
| `0x00E31B4C` | RenderingEnabled | i32 | 4 | Rendering enabled flag | BeginScene/EndScene |
| `0x00E31B34` | RedBitsCount | i32 | 4 | Red channel bit count (always 5) | Pixel format detection |
| `0x00E31B38` | GreenBitsCount | i32 | 4 | Green channel bit count (5 for RGB555, 6 for RGB565) | Pixel format detection, blend mask |
| `0x00E31B3C` | BlueBitsCount | i32 | 4 | Blue channel bit count (always 5) | Pixel format detection |
| `0x00E31B40` | RedChannelMask | u32 | 4 | Red channel pixel mask | Color computation |
| `0x00E31B44` | GreenChannelMask | u32 | 4 | Green channel pixel mask | Color computation |
| `0x0071FE94` | RenderFlagsPtr | ptr | 4 | Pointer to render flags structure (+0xE24 bit 5 = bloodsplat rendering enabled) | Bloodsplat check in combat |
| `0x00505754` | SurfaceFormat1 | i32 | 4 | First known surface format | Blit_Chroma, Blit_Copy |
| `0x00505758` | SurfaceFormat2 | i32 | 4 | Second known surface format | Blit_Chroma, Blit_Copy |
| `0x00505784` | SurfaceStride1 | i32 | 4 | Stride for first surface format | Same |
| `0x005057AC` | SurfaceStride2 | i32 | 4 | Stride for second surface format | Same |
| `0x0050575C` | CurrentSurfacePtr | ptr | 4 | Current source surface pointer | Blitting operations |

---

## Lighting System

Ambient light, mobile/stationary light stacks, and sky colors.

| Address | Name | Type | Size | Description | Referenced By |
|---------|------|------|------|-------------|---------------|
| `0x00AE3064` | AmbientLightBlue | i32 | 4 | Indoor ambient light blue component | MobileLightStack::SetAmbient (0x00467D8C) |
| `0x00AE3068` | AmbientLightGreen | i32 | 4 | Indoor ambient light green component | Same |
| `0x00AE306C` | AmbientLightRed | i32 | 4 | Indoor ambient light red component | Same |
| `0x006BDF88` | SkyColors | byte[12] | 12 | Sky color data: 6 bytes day (top R,G,B + bottom R,G,B), 6 bytes night | INI loader, outdoor renderer |

---

## Audio System

Miles Sound System handles and audio configuration.

| Address | Name | Type | Size | Description | Referenced By |
|---------|------|------|------|-------------|---------------|
| `0x00F791FC` | RedbookCDHandle | HANDLE | 4 | Miles Redbook CD audio handle | AIL_redbook_* calls, WndProc (pause/resume) |
| `0x00F79200` | DigitalAudioDriver | HANDLE | 4 | Miles digital audio driver handle | AIL_waveOutOpen, BinkSetSoundSystem, sample allocation |
| `0x00F79340` | MixerChannelCount | i32 | 4 | Audio mixer channel count (from INI, max 16) | Audio initialization |
| `0x00F79BE0` | SoundDataPool | void* | 4 | Sound data pool base address | Sound loading, ADPCM decompression |

---

## Event Scripting Engine

Event bytecode buffers, index tables, string tables, and interpreter state.

### Global Events

| Address | Name | Type | Size | Description | Referenced By |
|---------|------|------|------|-------------|---------------|
| `0x005A53B8` | GlobalEvtBuffer | u8[] | 46,080 | Raw bytecode from global.evt (max 0xB400 bytes) | EventProcessor_Execute (0x0044686D) |
| `0x00598570` | GlobalEvtIndex | struct[] | 52,800 | Index table (12-byte entries, up to ~4,400 entries) | Event lookup |
| `0x005A53B0` | GlobalEvtCount | i32 | 4 | Number of indexed global events | Event iteration |
| `0x005A53B4` | GlobalEvtSize | i32 | 4 | Actual loaded size of global.evt | Buffer validation |

### Map Events

| Address | Name | Type | Size | Description | Referenced By |
|---------|------|------|------|-------------|---------------|
| `0x005B33A0` | MapEvtBuffer | u8[] | 9,216 | Raw bytecode from `<map>.evt` (max 0x2400 bytes) | EventProcessor_Execute |
| `0x005B6458` | MapEvtIndex | struct[] | 52,800 | Map event index table (12-byte entries) | Event lookup |
| `0x005B0F90` | MapEvtCount | i32 | 4 | Number of indexed map events | Event iteration |
| `0x005B0F94` | MapEvtSize | i32 | 4 | Actual loaded size of `<map>.evt` | Buffer validation |
| `0x005B0FA0` | MapStrBuffer | u8[] | 9,216 | Raw string data from `<map>.str` (max 0x2400 bytes) | String table access |
| `0x005B0F88` | MapStrCount | i32 | 4 | Number of strings in map string table | String indexing |
| `0x00597DA0` | MapStrIndex | i32[] | 2,000 | Byte offsets into string buffer (500 entries max) | String lookup |
| `0x005B0F8C` | MapStrSize | i32 | 4 | Actual loaded size of `<map>.str` | Buffer validation |

### Interpreter State

| Address | Name | Type | Size | Description | Referenced By |
|---------|------|------|------|-------------|---------------|
| `0x005C32A0` | EventContext | i32 | 4 | 0=map events, 1=global events | EventProcessor_Execute |
| `0x005B6444` | EventAbortFlag | i32 | 4 | Non-zero = stop processing current event | All opcodes |
| `0x005B57A0` | EventParameter | i32 | 4 | Event parameter passed from caller | EventProcessor_Execute |
| `0x00597D98` | SubEventCounter | i32 | 4 | Current command sequence position within event | EventProcessor_Execute |
| `0x00590EF8` | ActiveEvtCount | i32 | 4 | Number of events in current scope | Event iteration |
| `0x00590EFC` | ActiveEvtBuffer | ptr | 4 | Pointer to current bytecode buffer | Opcode handlers |
| `0x005840B8` | WorkingIndexBuffer | u8[] | 52,800 | Working copy of event index (0xCE40 bytes) | EventProcessor_Execute |
| `0x005C3298` | SavedEventId | i32 | 4 | Saved event ID for resume after map transition | Map transition |
| `0x005C329C` | SavedSubEventId | i32 | 4 | Saved sub-event ID for resume | Map transition |
| `0x005C3438` | DialogModeFlag | i32 | 4 | Non-zero when in NPC dialog state | Dialog opcodes |
| `0x005C3444` | TwoD_EventParam | i32 | 4 | Building/shop type for 2D events | EVT_SHOW_BUILDING |
| `0x005C344C` | TwoD_EventsData | ptr | 4 | Loaded 2dEvents.txt building/shop data | Shop UI |
| `0x005B6448` | TimerEntryCount | i32 | 4 | Number of active timer entries | ProcessTimerEvents (0x00443FFF) |
| `0x005B57A4` | TransitionPending | i32 | 4 | 1 when map transition text has been shown | Map transition flow |
| `0x005B57A8` | TimerEntryArray | struct[] | variable | Timer entries (32-byte stride) for periodic/absolute events | ProcessTimerEvents |
| `0x006BDEA4` | GlobalFlagStore | i32 | 4 | Written by opcode 0x0A (EVT_SET_FLAG) | Event flag checks |
| `0x00507A40` | ActiveDialogHandle | ptr | 4 | Pointer to current dialog/window | NPC dialog flow |

### Teleport Overrides

| Address | Name | Type | Size | Description | Referenced By |
|---------|------|------|------|-------------|---------------|
| `0x005B6428` | TeleportOverrideX | i32 | 4 | Override X for teleport | EVT_TELEPORT (opcode 0x06) |
| `0x005B642C` | TeleportOverrideY | i32 | 4 | Override Y | Same |
| `0x005B6430` | TeleportOverrideZ | i32 | 4 | Override Z | Same |
| `0x005B6434` | TeleportOverrideYaw | i32 | 4 | Override yaw (-1=none) | Same |
| `0x005B6438` | TeleportOverridePitch | i32 | 4 | Override pitch | Same |
| `0x005B643C` | TeleportOverrideViewZ | i32 | 4 | Override view Z | Same |
| `0x005B6440` | TeleportOverrideActive | i32 | 4 | Override active flag | Same |

---

## Global Variable / QBit System

Quest state tracking and NPC variable table used by the event engine.

| Address | Name | Type | Size | Description | Referenced By |
|---------|------|------|------|-------------|---------------|
| `0x0072D50C` | QBitTableBase | struct[] | variable | Global variable/QBit table (stride 0x4C = 76 bytes per entry, multiple subfields) | EVT_SET_GLOBAL_VAR (opcode 0x27), EVT_SET_GLOBAL_VAR2 (opcode 0x28) |
| `0x0072D514` | QBitFlagsBase | u8[] | variable | Per-entry flags byte (offset +8 from QBitTableBase) -- bit 7=active/completed | Monster AI, condition checks |
| `0x0072D520` | QBitAltField | i32[] | variable | Alternate field (offset +0x14 per entry) -- written by opcode 0x28 | EVT_SET_GLOBAL_VAR2 |
| `0x0072D534` | QBitSubfield0 | i32[] | variable | Subfield 0 (offset +0x28 per entry) | EVT_SET_GLOBAL_VAR subfield 0 |
| `0x007214E4` | NPCTextLookup | struct[] | variable | NPC text/portrait lookup table (8 bytes per entry) | EVT_SET_NPC_PORTRAIT/NAME (global scope) |

---

## UI Windows

Window array, event queue, and UI state globals.

| Address | Name | Type | Size | Description | Referenced By |
|---------|------|------|------|-------------|---------------|
| `0x00506DD0` | WindowArray | struct[] | ~1,680 | Static array of UI windows (84 bytes per entry, ~20 windows) | GUIWindow::Create (0x0041C3DB), GUIWindow::Destroy |
| `0x00507478` | WindowArrayEnd | -- | -- | End boundary of window array | Window iteration |
| `0x00507A4C` | ActiveUIWindowPtr | ptr | 4 | Current active UI window pointer | UI event dispatch |
| `0x00507A5C` | ModalDialogActive | i32 | 4 | Modal dialog active flag | UI flow control |
| `0x00507A64` | CleanupNeededFlag | i32 | 4 | UI cleanup needed flag | Window lifecycle |
| `0x0050CA50` | EventQueueCount | i32 | 4 | Input/UI event queue count | GameplayLoop, EventDispatcher (0x00435737) |
| `0x0050CA54` | EventQueueArray | i32[] | variable | Event queue entries (3 ints per entry: type, param1, param2) | EventDispatcher |
| `0x00576EB0` | NeedUpdateFlag | i32 | 4 | UI needs update/refresh flag | EventDispatcher |
| `0x005B07B8` | NPCNameBuffer | char[] | variable | NPC name display buffer | EVT_SET_NPC_NAME (opcode 0x1E) |
| `0x005C32A8` | TextPresentationBuffer | char[] | variable | Text display buffer for EVT_SHOW_TEXT | EVT_SHOW_TEXT (opcode 0x1A) |

---

## Map Data

Map decorations, spawn points, and map-specific globals.

| Address | Name | Type | Size | Description | Referenced By |
|---------|------|------|------|-------------|---------------|
| `0x00683550` | DecorationArray | struct[] | variable | Map decorations array (32-byte stride) | SetPartyStartPoint (0x004498F8) |
| `0x0069AC50` | DecorationCount | i32 | 4 | Number of map decorations | SetPartyStartPoint |
| `0x006BE1C4` | CurrentMapName | char[] | variable | Current map filename | MapTransition_Execute (0x0044989E) |
| `0x00510604` | StartMapName | char[] | variable | Starting map name from INI (default "out01.odm") | INI loader |
| `0x005E4FD0` | ActorMonsterArray | base | variable | Full actor/monster array (stride 0x14CC = 5,324 bytes per actor with items) | MonsterLoot_Generate (0x00450244) |

---

## INI Configuration

Configuration globals loaded from mm6.ini.

| Address | Name | Type | Size | Description | Referenced By |
|---------|------|------|------|-------------|---------------|
| `0x006BE1E8` | DebugFlags | u32 | 4 | Debug flags bitfield (bit 0=windowed, bit 1=showFR, bit 2=nomonster, bit 3=nodecoration, bit 4=nodamage) | INI reader/writer, game loop |

---

## Notes

- All addresses are virtual addresses from MM7-Rel.exe v1.21 with PE base `0x00400000`.
- `DAT_` prefix in system docs corresponds to Ghidra's auto-generated global references.
- `FUN_` prefix indicates Ghidra auto-generated function identifiers.
- Player record offsets (e.g., skills at `+0x108`, HP at `+0x193C`) are relative to
  each player's base within the `PlayerRecordBase` array at `0x00ACD804`.
- Actor record offsets (e.g., AI state at `+0x88`, HP at `+0x8C`) are relative to
  each actor's base within the `ActorArrayBase` at `0x005FEFFC`.
- The game's 64-bit time system at `0x00ACCE64:68` uses 128 ticks per second with a
  custom calendar (28 days/month, 336 days/year).
- The Ghidra TSV export contained no game-specific globals with identified names --
  all entries were either compiler artifacts (switch tables, vtables, RTTI) or OS
  structures (TEB, PE resources). The catalog above is therefore sourced entirely
  from the richer reverse engineering documentation.
