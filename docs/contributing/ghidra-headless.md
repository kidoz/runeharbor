---
title: "Ghidra Headless Analysis"
summary: "Setup and operation guide for reproducible headless Ghidra analysis."
doc_type: how-to
status: verified
last_updated: 2026-08-01
---
# Ghidra Headless Analysis

Reverse-engineering reference workflow for RuneHarbor. Ghidra runs **headless**
(no GUI) via `analyzeHeadless`, driven by the wrapper `tools/ghidra_headless.sh`.

Reference: <https://maxkersten.nl/2024/06/30/ghidra-tip-0x05-headless-execution/>

## Prerequisites

- **Ghidra** — installed via Homebrew at `/opt/homebrew/opt/ghidra/libexec`
  (`brew install --cask ghidra`, or `brew install ghidra`).
- **JDK 21** — Ghidra 12.x targets JDK 21. The wrapper auto-selects
  `openjdk@21` (`brew install openjdk@21`) even if a newer JDK is on `PATH`.

Confirm the analyzer is reachable:

```bash
/opt/homebrew/opt/ghidra/libexec/support/analyzeHeadless   # prints usage

```

## Quick start

```bash
tools/ghidra_headless.sh                       # ExportAnalysis.java on tmp/MM7/MM7-Rel.exe
tools/ghidra_headless.sh DecompileTarget.java  # run a different script

```

First run **imports + analyzes** the binary (~40s) and caches the result in a
Ghidra project under `tmp/ghidra/` (gitignored). Later runs reuse that project
in fast **process mode** — only the script re-runs, no re-analysis.

## Configuration (env vars)

| Var                  | Default                              | Purpose                              |
| -------------------- | ------------------------------------ | ------------------------------------ |
| `GHIDRA_INSTALL_DIR` | `/opt/homebrew/opt/ghidra/libexec`   | Ghidra home (dir containing `support/`) |
| `JAVA_HOME`          | `openjdk@21`                         | JDK used to launch Ghidra            |
| `GH_BINARY`          | `tmp/MM7/MM7-Rel.exe`                | Target to analyze                    |
| `GH_PROJECT_DIR`     | `tmp/ghidra`                         | Project *location* (parent dir)      |
| `GH_PROJECT_NAME`    | `MM7`                                | Project *name*                       |
| `GH_SCRIPT_PATH`     | `tools/ghidra/scripts`                     | Directory holding the Ghidra script  |
| `GH_REIMPORT=1`      | —                                    | Force clean re-import + re-analysis  |

> **Convention:** `analyzeHeadless` takes the project *location* (the parent
> directory) and project *name* as **separate** arguments — not the `.gpr` path.
> The wrapper handles this for you.

Examples:

```bash
# Analyze a different binary (Bink video codec DLL)
GH_BINARY=tmp/MM7/Binkw32.dll GH_PROJECT_NAME=BINK tools/ghidra_headless.sh

# Re-run analysis from scratch (e.g. after upgrading Ghidra)
GH_REIMPORT=1 tools/ghidra_headless.sh

# Point at a Ghidra install elsewhere
GHIDRA_INSTALL_DIR=/opt/ghidra_12.1.2 tools/ghidra_headless.sh

```

## Scripts

Ghidra scripts live in `tools/ghidra/scripts/` (this is `GH_SCRIPT_PATH`):

- **`ExportAnalysis.java`** — dumps `functions.txt`, `strings.txt`,
  `imports.txt`, and a sample `decompiled.c`.
- **`DecompileTarget.java`** — decompiles a named set of functions.

A `-postScript` runs **after** auto-analysis with full API access; use
`-preScript` for scripts that must tweak analysis options beforehand.

## Outputs

Text artifacts are written to `tools/ghidra/output/` (committed). The Ghidra project
itself (`tmp/ghidra/MM7.gpr`, `MM7.rep`) stays under `tmp/` and is gitignored —
it can be deleted any time; the next run rebuilds it.
