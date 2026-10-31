# "146/140 not implemented" — CORRECTION

**They were not missing functions.** The audit deduplicated the surface by **name** only (1383),
while the canonical registry is keyed by **(namespace, name)** (1523). The same name in different
namespaces (e.g. `ISAM.EOF` / `DBF.EOF`, `TORCH.ABS` / `ABS`) was collapsed into a single entry:
the 140 "missing" ones were **namespace duplicates**, not omissions.

Verification performed (2026-10-24):
- Canonical set `(ns,name)`: **1523**
- Set registered in `StdLib.BuiltinProcedures`: **1523**
- Difference: **0** (nothing missing, in either direction)

With the correct surface (audit key = ns+name): **IL, PCodeVM, VMC = 1523/1523 (100%)**;
**C99Native = 1517/1523**. The 6 C99Native gaps are in **emission**, not in StdLib:
`SPEED.SETDURABILITY`, `SPEED.FLUSHDURABILITY`, `SPEED.BEGINGROUPCOMMIT`,
`SPEED.ENDGROUPCOMMIT`, `SPEED.INFO`, `MOUSE.CLICK`.

No change to StdLib (Sharp or the original GMBasic repo) is therefore needed.
