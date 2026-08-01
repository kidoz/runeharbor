---
title: "RuneHarbor Documentation"
summary: "Entry point for RuneHarbor engine, format, rendering, and gameplay documentation."
doc_type: index
status: verified
last_updated: 2026-08-01
---
# RuneHarbor Documentation

RuneHarbor is a portable, open-source reimplementation of the **Might and Magic VII: For Blood and Honor** engine, written in C++23 with SDL3. It reads legally obtained user installations and does not distribute original game content.

The documentation covers engine architecture, data format decoding, world rendering,
and gameplay systems including shops, combat, quests, inventory, and spell casting.

## Explore the documentation

**Architecture and game flow:**

- [Engine overview](architecture.md) — the full subsystem map
- [Game flow](game-flow.md) — state machine and boot sequence

**Game mechanics:**

- [Combat system](combat-system.md) — damage, attack types, armor class
- [Turn-based combat](turn-based-combat.md) — initiative queue, round structure
- [Spell system](spell-system.md) — casting, mana, mastery levels
- [Shops and economy](shops-and-economy.md) — building registry, pricing formulas
- [Temple healing](temple-healing-resurrection.md) — heal/cure/resurrect services
- [Inventory and equipment](inventory-equipment.md) — equip validation, slot mapping
- [Usable items](usable-items.md) — potion/scroll/book consume dispatch
- [Quest journal](quest-journal.md) — quest log, autonotes, awards

**File formats:**

- [LOD archives](lod-archives.md) — container format
- [BLV indoor maps](blv-indoor-maps.md) / [ODM outdoor maps](odm-outdoor-maps.md)
- [Event engine](event-engine.md) — 60-opcode bytecode interpreter
- [Save and load](save-load.md) / [Video system](video-system.md)

**Rendering pipeline:**

- [Rendering pipeline](rendering-pipeline.md) — software + GPU paths
- [Indoor rendering](indoor-rendering.md) / [Outdoor rendering](outdoor-rendering.md)
- [Sprite billboard](sprite-billboard.md) / [Lighting](lighting.md) / [Visibility](visibility.md)

## Evidence status

Documentation separates claims into three categories:

- **Observed** — verified against the original binary or data via tools (radare2, Ghidra, hex dumps)
- **Inferred** — derived from observed patterns but not byte-confirmed
- **Unknown** — unresolved, flagged for further research

See the [open-question register](open-questions.md) for tracked unresolved items.

## Project

Source code, build instructions, and contribution guidelines are in the [RuneHarbor repository](https://github.com/apavlov/runeharbor).
