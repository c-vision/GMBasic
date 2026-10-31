# RuleFlow (GMBasic port) — discrete systems with a causal graph

**RuleFlow** models and evolves complex discrete systems (Sequential Substitution
Systems, cellular automaton, string rewrites) keeping track **not** only of the final
state, but of **every event** that produced it: which cells were destroyed, which
created, and from which event they descend. From there it extracts a **causal DAG** with its metrics
(nodes, edges, density, max path).

This is the **GMBasic port** of its **headless skeleton**, written in BASIC and verified on the
four backends of the monorepo.

## Upstream (source of the port)

| Item | Reference |
|---|---|
| Repository | <https://github.com/RuleFlow-OSS/RuleFlow> |
| Site | <https://www.ruleflow.org/> |
| Licence | **MIT** (attribution required: see below) |
| Nature | Python research framework + terminal IDE (*RuleFlow Studio*, Textual) |
| Core | `Cell`/`DeltaCell`/`Event`/`Flow`/`RuleSet`, vector topologies, **FlowLang** DSL |
| Analysis | `EventCausalityGraph` → NetworkX `MultiDiGraph` + metrics |

Attribution (MIT): RuleFlow © RuleFlow-OSS (Southern Adventist University research team),
<https://github.com/RuleFlow-OSS/RuleFlow>. The original code is MIT; this port takes over
**the architecture and the observable behaviour**, not the Python code.

## What this port does (M1 — headless core)

```
script .flow ──► FlowLang parser (M1) ──► universe of CELLS (value + id + origin)
                                             │
                            per tick: apply the rules
                                             │
                       one application = one EVENT with its DELTA
                       (cells destroyed / created, each with its origin)
                                             │
                        causal edges: origin(cell touched) ──► event
                                             │
                       DAG metrics: nodes, edges, density, max path
```

- **Cells**: the universe as a sequence of symbols; every cell has a **unique id** and an **origin**
  (the event that created it; `0` = generation zero).
- **Events**: a rule application records tick, rule, position and the delta
  (the cells touched/before and after).
- **Cause**: an event is linked to the **origins of the cells it touches** (for
  deletion: the destroyed cells; for insertion: the context cells).
  Since the origins are always **preceding** events, the graph is a DAG and the order of the
  event ids is a topological order.
- **Metrics** (parity with the upstream `EventCausalityGraph`):

| metric | definition in this port |
|---|---|
| `nodi` | number of events **+ 1** (root node = generation zero) |
| `archi` | distinct causal edges `(origin → event)` |
| `densità` | `archi / (nodi * (nodi - 1))` |
| `cammino max` | length of the maximum path in the DAG (DP in event order) |

### Modes

| Mode | How it is activated | What it does |
|---|---|---|
| `status` (default) | — | banner, config, presence of the flow, limits |
| `run` | `#DEF RULEFLOW_MODE_RUN` at the top of the `.bas` | evolve, print rules, counters, final space, event trace |
| `graph` | `#DEF RULEFLOW_MODE_GRAPH` | evolve and print the metrics of the causal graph |
| `check` | `#DEF RULEFLOW_MODE_CHECK` | diagnostics: rules, evolution, DAG, metrics, space (`RULEFLOW-CHECK-OK`) |

> BASIC has no argv: the mode is chosen with `#IFDEF` (the `tools/runners/` runners do not
> pass `-D`) or with `[settings] mode` in `.ruleflow.toml`.

## Supported FlowLang (M1)

| operator | meaning | example |
|---|---|---|
| `pat -> dst` | **substitution**: the selector is replaced by the target | `ABA -> AAB;` |
| `pat --> dst` | **overwrite**: overwrites the cells of the selector (same length) | `B --> Z;` |
| `pat > dst` | **insertion**: inserts `dst` **before** the selector | `Y > W;` |
| `pat ><` | **deletion**: removes the selector | `W ><;` |

Directives: `@init("TEST0")` (initial universe), `@evolve(n)` (tick; **wins** over
`[flow] steps`). Comments: `#`. Case-insensitive (the symbols are normalized to uppercase).

**M1 semantics (deterministic):** in every tick, every rule applies **once**, at the
first match from the left (after an application the search restarts past the target).
No randomness, no branching: two runs give the same result.

**NOT supported in M1 — explicit error, never ignored:**

- FlowLang flags `-pl`, `-bl`, `-cmp`, `-crp`, `-p_rule`, `-p_space`, `-life`, `-sr`
  (parallelism, multiway branch, conflict protocols);
- directives `@regress`, `@merge`, `@compress`, `@macro`;
- Python/Wolfram bootstrap (`.pflow`/`.wpflow`);
- `.npy`/binary reader for `@init`.

A flow that uses these forms **fails** with a message that indicates the form and the TODO.

## Configuration (`.ruleflow.toml`, next to the app or in a parent folder)

```toml
[settings]
mode = "status"          # status | run | graph | check

[flow]
file = "flows/sss.flow"  # FlowLang-like script, relative to the app folder
steps = 12               # ticks if the flow does NOT declare @evolve(n)

[run]
verbose = "true"         # "no applicable rule" at the end of the evolution
```

## Fixtures (versioned inputs)

| flow | what it shows |
|---|---|
| `flows/sss.flow` | the **Sequential Substitution System of the upstream README** (`ABA -> AAB; A -> ABA;`, 12 ticks → 24 events, 27 cells) |
| `flows/lsystem.flow` | Lindenmayer L-system in sequential semantics (grows by 1 cell/tick) |
| `flows/ops.flow` | **all four** operators + the insertion/deletion churn |

## How to run

From the **monorepo root** (`GMBasicMonoRepo/`):

```sh
# status (default)
tools/sh/harness.sh pcode shared/APPS/RuleFlow/src/RuleFlow.bas
tools/sh/harness.sh il    shared/APPS/RuleFlow/src/RuleFlow.bas

# run / graph / check: add at the top of the .bas   #DEF RULEFLOW_MODE_RUN (or _GRAPH/_CHECK)

# native leg (C99Native), path relative to GMBasic/
bash tools/sh/gmb_c99native_run.sh "$PWD/shared/APPS/RuleFlow/src/RuleFlow.bas"
```

No output on disk: the app is pure in-memory computation (`bin/` is only created,
as per the compiler convention).

## Recorded verification (2026-11-01, 4 legs)

Flow `flows/sss.flow` (2 rules, `@evolve(12)`):

| step | outcome |
|---|---|
| `status` | config read, flow present, limits |
| `run` | 24 events / 24 applications / 24 edges; final space `ABAAB…AB` (27 cells) |
| `graph` | nodes **25**, edges **24**, density **0.0400**, max path **24** |
| `check` | `check-rules/evolve/dag/metric/space` OK + `RULEFLOW-CHECK-OK` |

Output **identical** on IL, PCode, VM/C and C99Native (verified with `diff` on the lines
`flow/eventi/nodi/archi/densità/cammino/spazio finale`).
