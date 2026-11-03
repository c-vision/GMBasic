# TODO — RuleFlow (GMBasic port)

Status: **M1** — working headless core: FlowLang parser (subset), cell universe
with id/origin, events with delta, **causal graph** and metrics (nodes, edges, density,
max path). Verified on IL/PCode/VM-C/C99Native.
Upstream: <https://github.com/RuleFlow-OSS/RuleFlow> (MIT).

## M2 — fidelity to the FlowLang engine

- [ ] **Per-rule parallelism** (`-pl[n]`): today a rule applies **once per
      tick**. With `-pl[inf]` (upstream README default) all non-overlapping occurrences
      apply. Requires: search of all matches, handling of spatial conflicts,
      multiple events in the same tick.
- [ ] **Multiway branch** (`-bl[n]`): when several rules/matches compete, generate alternative
      universes and keep the branch history (`parent_delta`), with merge/exclusion.
- [ ] **Conflict protocols** (`-cmp`, `-crp`): marking/skip/branch on overlapping spans.
- [ ] **Stochastic probabilities** (`-p_rule`, `-p_space`) with a deterministic PRNG (seed in the
      settings, so runs stay reproducible).
- [ ] **`@regress(n)`**: requires the full history of mutations (today only
      the events are kept, not the previous state). With M2 the graph becomes reversible.
- [ ] **`@merge` / `@compress` / `@macro`**: atomic composition of rules and import of external
      flows.
- [ ] **Per-instruction constraints** (`-life[n]`, `-sr[...]`, `-g[...]`): life counters and
      spatial ranges per rule.

## M3 — bootstrap and analysis

- [ ] **`.pflow` (Python) / `.wpflow` (Wolfram) bootstrap**: in the BASIC world the role of the
      bootstrap is that of a *rule generator*; candidates: `#DEF` macros, `INCLUDE`, or a
      small template language. Decide the equivalent (never run Python/Wolfram).
- [ ] **Graph import/export**: GEXF (Gephi), GraphML, GML, Graph6/Sparse6 — the format
      expected by the NetworkX world. The DAG is already in memory: the writer is needed.
- [ ] **Topologies** (upstream `core/topologies`): today the universe is 1D (string). Add
      2D/ND and toroidal topologies; `@init` from `.npy`/binary files.
- [ ] **Performance pattern search** (Boyer-Moore-Horspool/KMP/Rabin-Karp as
      upstream): today `INSTR`; useful for large universes.
- [ ] **Additional metrics**: degree assortativity, flow hierarchy, density per generation,
      distribution of path lengths.
- [ ] **System enumeration** (Studio "System Enumeration"): evaluate whole rule
      spaces with properties (identity, shortening, unbalancedness) — with a cell/event budget.

## M4 — user experience

- [ ] **TUI** (the monorepo has `MENU`/`TBROWSE`): workspace, step/regress, hover on the cells
      (quantity/generation/id), graph with sparkline. Today: 4 CLI modes.
- [ ] **Snapshot/restore** of a session (universe + events) to resume an experiment.
- [ ] **Workspace variants** (upstream "Workspace Variants"): temporary copies of the flow.

## Constraints (non-negotiable)

- **No stubs**: every capability either works or returns a documented **explicit error**
  (today: flags, directives and non-M1 bootstrap fail with a message).
- **Determinism**: no unseeded randomness; two runs = same result.
- **4-leg parity** of the BASIC surface used (AGENTS §4.1): `check` identical on
  IL/PCode/VM-C/C99Native.
- **Licence**: MIT attribution of RuleFlow in `README.md`; no Python code copied.
