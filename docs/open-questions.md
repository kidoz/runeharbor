---
title: "Open Questions"
summary: "Register of unresolved reverse-engineering questions and implementation gaps."
doc_type: reference
status: partial
last_updated: 2026-08-01
---
# Open Questions

This register tracks unresolved reverse-engineering questions and known
implementation gaps. Each entry links to its canonical documentation page.

## Scope

This register contains unresolved or bounded questions only. Verified answers
belong on their canonical reference pages and must be removed from this register.

## Rendering

- **[Stencil-clipped portals](indoor-rendering.md)**: Indoor visibility uses sector flood-fill, not
  stencil-clipped portals. You can see through doorways into occluded sectors.
  MM7 clips portal geometry against the doorway opening. **unknown**

- **[Outdoor GPU path](outdoor-rendering.md)**: Outdoor rendering is software-only (no GPU terrain).
  MM7 has a D3D outdoor path. **inferred** — modern software is acceptable.

- **[Skydome](outdoor-rendering.md)**: The sky is a 2-color gradient. No sun, moon, stars, or clouds.
  **unknown**

- **[Particle system](sprite-billboard.md)**: Fire/light decorations do not spawn particles
  (`dpft.bin` + `effpar` sprites). **unknown**

- **[Full per-pixel lighting](lighting.md)**: Indoor lighting uses per-vertex dynamic light
  approximation. MM7 achieves per-pixel gradients via D3D fixed-function
  lighting. **inferred**

## Gameplay

- **[Weapon skills in damage calc](combat-system.md)**: `playerAttack` does not factor Sword,
  Armsmaster, or other weapon skill bonuses into hit/damage. MM7 does.
  **observed** gap.

- **[Skill trainer (type 0x1F)](training-and-travel.md)**: RE'd but not implemented. Raises individual
  skills for gold (distinct from level training). **observed**

- **[NPCs as world actors](npc-dialogue.md)**: NPCs exist as dialogue text only. None walk,
  pathfind, or react spatially. MM7 has roaming NPCs in towns. **unknown**

- **[Faction/reputation consequences](npc-dialogue.md)**: Alignment + reputation fields exist but
  drive no NPC reactions or quest gating beyond raw event flags. **inferred**

## Data formats

- **[Original MM7 save loading](save-load.md)**: Only the custom `RHBV` format is supported.
  No `party.bin` / `save###.mm7` binary parser. **unknown**

- **[Music/MIDI playback](audio-system.md)**: Audio is WAV-only. No MP3/OGG/MIDI soundtrack.
  **unknown**

- **[Full 52-case potion table](usable-items.md)**: Only heal/cure/mana cases implemented; the
  other 49 default to heal. **observed**

- **[SCROLL.TXT / POTION.TXT parsers](usable-items.md)**: Runtime mapping tables used; proper
  binary format parsers missing. **observed**

- **[Trainer level-cap table](training-and-travel.md)** (`0x4F06E6`): Values read as 0xA0–0xA3,
  implausible as MM7 level caps. May be effectively uncapped. **unknown**
