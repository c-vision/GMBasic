# KDTREE — implementazione C# archiviata

Il namespace `KDTREE` è migrato a **shared/libs/kdtree** (lib `libgmbkdtree`,
C puro). La superficie BASIC non è più builtin: IL/PCode/VM-C/C99Native
chiamano la libreria via FFI (P/Invoke lato C#).

Qui è conservata l'implementazione C# storica, NON più compilata:

- `StdLib.KdTree.cs` — registrazione del namespace builtin (rimossa).
- `CommonLib-Numerics-KdTree.cs` — `CommonLib/Numerics/KdTree.cs`
  (`KdTreeBuild`/`KdTreeQuery`/`KdTreeQueryBall`, era il core managed).
- `SciPyKdTreeTests.cs` — test xUnit sul core C# (rimosso dal progetto;
  la copertura ora è nei MiniTest `.bas` su 4 backend).

Sono stati rimossi anche i casi/handler dei 4 backend (TokenKind
`KdTreeCreate`/`KdTreeQuery`/`KdTreeQueryDist`/`KdTreeQueryBall`) e il
runtime C `gmb_kdtree.c`.
