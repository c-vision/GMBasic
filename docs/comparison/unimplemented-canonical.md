# "146/140 non implementate" — RETTIFICA

**Non erano funzioni mancanti.** L'audit deduplicava la superficie per solo **nome** (1383),
mentre il registry canonico è per **(namespace, nome)** (1523). Lo stesso nome in namespace
diversi (es. `ISAM.EOF` / `DBF.EOF`, `TORCH.ABS` / `ABS`) veniva collassato in una sola voce:
le 140 "mancanti" erano **duplicati di namespace**, non omissioni.

Verifica eseguita (2026-10-24):
- Insieme canonico `(ns,nome)`: **1523**
- Insieme registrato in `StdLib.BuiltinProcedures`: **1523**
- Differenza: **0** (nessuna assenza, in nessuna delle due direzioni)

Con la superficie corretta (audit key = ns+nome): **IL, PCodeVM, VMC = 1523/1523 (100%)**;
**C99Native = 1517/1523**. I 6 gap C99Native sono di **emissione**, non di StdLib:
`SPEED.SETDURABILITY`, `SPEED.FLUSHDURABILITY`, `SPEED.BEGINGROUPCOMMIT`,
`SPEED.ENDGROUPCOMMIT`, `SPEED.INFO`, `MOUSE.CLICK`.

Nessuna modifica a StdLib (Sharp o repo originale GMBasic) è quindi necessaria.
