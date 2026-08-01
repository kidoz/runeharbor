---
title: RuneHarbor documentation standard
summary: Authoring rules for maintainable, evidence-backed, MkDocs-compatible documentation.
doc_type: reference
status: verified
last_updated: 2026-08-01
tags:
  - documentation
  - mkdocs
  - re
  - mm7
---

# RuneHarbor documentation standard

This standard keeps `docs/` readable as plain Markdown, buildable by MkDocs,
and useful as an evidence-backed engine reference. It applies to new pages and
to existing pages when they receive substantial edits.

Published pages use compact metadata. The header records only information that
improves discovery, evidence tracking, and maintenance.

## Scope

This standard covers published Markdown under `docs/`, its metadata, navigation,
evidence labels, and validation workflow. It does not define C++ source style,
version-control policy, or unpublished research-record structure.

## Goals

- Keep one authoritative place for each fact.
- Preserve stable paths, headings, and cross-page links.
- Separate artifact facts, interpretation, and implementation status.
- Make sections self-contained enough for search and targeted retrieval.
- Keep the source portable across MkDocs, repository viewers, and ordinary
  Markdown editors.
- Make documentation defects fail `mkdocs build --strict`.

## Research basis

RuneHarbor follows established documentation practices:

- [MkDocs writing guidance](https://www.mkdocs.org/user-guide/writing-your-docs/)
  defines the source layout, relative-link behavior, heading anchors, and YAML
  metadata support.
- [MkDocs configuration guidance](https://www.mkdocs.org/user-guide/configuration/)
  defines strict validation for navigation, links, and anchors.
- [Diataxis](https://diataxis.fr/) separates tutorials, how-to guides,
  reference, and explanation by reader need.

RuneHarbor adds evidence labels because binary and runtime documentation must
separate confirmed facts, deductions, and unresolved details.

## Information architecture

Choose the document type before choosing its headings. A page should answer one
kind of reader need.

| Type | Reader need | Preferred location | Shape |
| --- | --- | --- | --- |
| `tutorial` | Learn through a complete exercise | Future `docs/tutorials/` | Ordered lesson |
| `how-to` | Complete a specific task | `docs/contributing/` or future `docs/how-to/` | Goal, prerequisites, steps, verification |
| `reference` | Look up authoritative facts | `docs/` and `docs/data/` | Neutral, structured description |
| `explanation` | Understand behavior or design | `docs/` | Context, reasoning, consequences |
| `index` | Discover related pages | `index.md` | Short summaries and links |

Existing paths and heading anchors are compatibility surfaces. Move or rename a
page only with updated navigation and links in the same change.

### Content boundaries

- `docs/**` contains published engine, format, and contributor knowledge.
- Investigation logs, rejected hypotheses, and chronological research history
  are working records rather than published reference material.
- Source comments explain local implementation decisions; they do not replace a
  format or behavior specification.
- Tests are executable evidence. Link to relevant source or tests instead of
  duplicating large code fragments.

When new evidence settles a fact, update its canonical page and the
[open-question register](../open-questions.md). Replace obsolete claims instead
of appending a newer, contradictory answer.

## Page metadata

Every new or substantially revised page uses MkDocs-compatible YAML front matter:

```yaml
---
title: LOD archive format
summary: Binary layout and validation rules for MM7 LOD archives.
doc_type: reference
status: partial
last_updated: 2026-08-01
source_files:
  - src/formats/lod_archive.cpp
  - tests/formats/lod_archive_test.cpp
tags:
  - lod
  - archive
  - mm7
---
```

Required keys:

| Key | Rule |
| --- | --- |
| `title` | Human-readable title that semantically matches the body H1 and navigation label. |
| `summary` | One sentence naming the subject and coverage without relying on the title. |
| `doc_type` | One of `tutorial`, `how-to`, `reference`, `explanation`, or `index`. |
| `status` | One of `draft`, `verified`, `partial`, `deprecated`, or `historical`. |
| `last_updated` | ISO date of the last substantive content change. |

Optional keys:

| Key | Use |
| --- | --- |
| `source_files` | Repository paths that implement or test the documented behavior. |
| `tags` | Three to eight stable discovery terms. |
| `replaces` | Relative path or stable page name superseded by this page. |

Use lowercase keys and plain YAML scalars or lists. Do not place executable
configuration or secrets in front matter. Remove redundant prose status lines
after adopting metadata unless they describe a narrower evidence boundary.

## Evidence vocabulary

Every factual claim carries an evidence status:

| Status | Meaning |
|--------|---------|
| **observed** | Verified against the original binary or data via tools (radare2, Ghidra, hex dumps) |
| **inferred** | Derived from observed patterns but not byte-confirmed |
| **unknown** | Unresolved, flagged for further research |

## Common Markdown contract

Every page must:

1. Use UTF-8 Markdown with the `.md` extension.
2. Have exactly one H1.
3. Increase heading depth one level at a time; never jump from H2 to H4.
4. Use unique, descriptive headings. Heading anchors are public links.
5. Use relative Markdown links ending in `.md` for documentation sources.
6. Give fenced code blocks a language such as `cpp`, `bash`, or `text`.
7. Put a blank line before and after lists, tables, and fenced code blocks.
8. Avoid raw HTML because MkDocs cannot reliably validate links inside it.
9. Name new files and directories with lowercase kebab-case.
10. Appear explicitly in `mkdocs.yml` unless exclusion is intentional and
    documented in configuration.

Prefer ordinary Markdown supported by MkDocs and repository viewers. Add an
extension only when several pages need it and the locked toolchain validates it.

## Section retrieval contract

A reader or search tool may encounter one section without the rest of its page.
Write each H2 so that it remains useful in that context.

- Begin with a two-to-four sentence synopsis that states the subject, coverage,
  evidence boundary, and important limitation.
- Give every reference page an early `## Scope` section.
- Prefer explicit nouns when a pronoun would depend on surrounding sections.
- Repeat units, byte order, coordinate system, and version where ambiguity is
  costly.
- Put exact layouts and mappings in tables with stable column names.
- Keep evidence, interpretation, and implementation status separate.
- Use only the evidence labels `observed`, `inferred`, and `unknown`.
- Link to canonical definitions instead of copying them.
- Preserve searchable identifiers such as archive names, opcodes, offsets,
  symbols, paths, and commands.
- Avoid time-relative wording unless it includes a date, version, commit, or
  explicit status.

Review a page for splitting when it exceeds roughly 500 lines, combines
independently useful subjects, or mixes sections with different evidence bases.
Split by stable concepts and add an index when a subject becomes a page family.

## RE documentation profile

Reverse-engineering reference pages in `docs/` and `docs/data/` follow this
order when the sections apply:

1. Synopsis below the H1.
2. `## Scope` — included versions, files, and explicit exclusions.
3. `## Compatibility` — version differences and rejection policy.
4. `## Source provenance` — artifact identity and reproduction commands.
5. Container or top-level layout.
6. Record layouts and field semantics.
7. Cross-record joins and runtime behavior.
8. `## Invalid-input behavior` — bounds and deterministic failure rules.
9. `## Integration notes` — implementation links and known implementation gaps.
10. `## Open questions` — only unresolved, bounded questions linked to the
    [open-question register](../open-questions.md).

Use `0x` hexadecimal offsets, decimal sizes with hexadecimal equivalents when
useful, explicit signedness such as `u16` and `i32`, and explicit byte order.
Every unknown field remains represented so a parser can preserve or reject it
without guessing.

Keep chronological investigation out of published reference pages. Retain a
short historical note only when it prevents a known misreading from recurring.

## RE reference template

Copy this skeleton for a new binary or runtime reference page. Replace every
placeholder and remove sections that genuinely do not apply.

````md
---
title: <Format or runtime structure name>
summary: <One sentence naming the artifact and coverage.>
doc_type: reference
status: draft
last_updated: YYYY-MM-DD
source_files:
  - src/<parser-or-runtime-source>
  - tests/<test-source>
tags:
  - mm7
  - <subject>
---
# <Format or runtime structure name>

<Two to four sentences summarizing the result, evidence boundary, and most
important compatibility limitation.>

## Scope

This page covers:

- <included artifact, version, or behavior>.

This page does not cover:

- <explicit exclusion and canonical link when one exists>.

## Compatibility

| Variant | Support | Notes |
| --- | --- | --- |
| <MM7 edition or version> | <supported, unsupported, or unknown> | <boundary> |

## Source provenance

| Field | Value |
| --- | --- |
| Product and edition | <legally obtained edition> |
| Artifact | `<relative path or archive entry>` |
| Size or record count | <non-expressive fact> **observed** |
| Digest | `<digest when useful>` |
| Research tool | <tool and pinned version> |

## Container layout

State byte order, framing, offsets, sizes, and alignment.

| Offset | Size | Type | Field | Evidence | Notes |
| ---: | ---: | --- | --- | --- | --- |
| `+0x00` | 4 | `u32` | `<field>` | **observed** | <meaning> |

## Record layout

Describe one record and how records are counted or terminated.

| Offset | Size | Type | Field | Evidence | Notes |
| ---: | ---: | --- | --- | --- | --- |
| `+0x00` | 2 | `u16` | `<field>` | **unknown** | Preserve without interpretation. |

## Semantics and joins

Describe relationships to other records, tables, assets, or runtime state.

## Invalid-input behavior

List deterministic rejection cases, including truncation, overflow, invalid
discriminators, and inconsistent counts.

## Integration notes

- Parser: `src/<path>`
- Tests: `tests/<path>`
- Known implementation gap: <gap or “None known for the documented scope.”>

## Open questions

Mirror bounded unresolved questions in the
[open-question register](open-questions.md).

- <question and evidence needed to resolve it> **unknown**
````

## Source and evidence rules

- Identify proprietary samples only with facts needed for reproducibility:
  product or edition, path, size, digest, counts, and tool version.
- Do not add extracted game data, executable bytes, bulk strings, or proprietary
  media to published documentation.
- Prefer primary sources for tool behavior and public compatibility research.
- Record an external source's title, publisher, URL, access date, and pinned
  revision when it can change.
- State what each source proves. Corroboration does not turn an inference into
  direct observation of the original artifact.
- Resolve contradictions in canonical text rather than leaving both claims active.

## Navigation

`mkdocs.yml` is the authoritative navigation manifest. Explicit navigation is
intentional: it gives readers a curated order and makes omitted pages fail the
strict build.

- Add a page to `nav` in the same change that creates it.
- Remove its nav entry in the same change that removes it.
- Keep labels concise; put the full title in metadata and the H1.
- Prefer two or three navigation levels and add an index before deeper nesting.
- Link to `.md` source paths and anchors so MkDocs can validate and rewrite them.
- Search for an old anchor after renaming a linked heading.

## Change workflow

1. Decide the document type and canonical page before writing.
2. Read related pages and the open-question register to avoid duplication.
3. Update metadata after a substantive change.
4. Replace stale statements rather than appending chronology.
5. Update `mkdocs.yml` when pages are added, removed, or reordered.
6. Run:

   ```bash
   just docs-check
   ```

7. When a specification changes executable behavior, update or add the relevant
   parser test in the same feature change.

## Review checklist

- [ ] The page has one purpose and one H1.
- [ ] Required metadata is present and matches the H1 and navigation label.
- [ ] Scope and compatibility boundaries are explicit.
- [ ] Exact claims carry `observed`, `inferred`, or `unknown` evidence status.
- [ ] Implementation status is separate from artifact truth.
- [ ] Local links use relative `.md` paths and valid anchors.
- [ ] New pages appear in `mkdocs.yml`.
- [ ] Stale or contradictory text was replaced.
- [ ] Large independent subjects were split or intentionally kept together.
- [ ] No proprietary payload or secret was added.
- [ ] `just docs-check` passes.
