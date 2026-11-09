# SPATIAL — implementazione C# archiviata

Il namespace `SPATIAL` è migrato a **shared/libs/spatial** (lib `libgmbspatial`,
C puro). La superficie BASIC non è più builtin: IL/PCode/VM-C/C99Native chiamano
la libreria via FFI (P/Invoke lato C#).

Qui è conservata l'implementazione C# storica, NON più compilata:

- `StdLib.Spatial.cs` — registrazione del namespace builtin (rimossa).
- `CommonLib-Numerics-Spatial.cs` — metodi estratti da
  `CommonLib/Numerics/SciPyP1.cs`: `DistMetric` (helper), `Pdist`, `Cdist`,
  `DistanceMatrix`, `ConvexHull`.

La stessa migrazione ha rimosso dai backend IL/PCode/VM/C/C99Native i casi,
gli helper e gli handler dedicati (TokenKind `SpatialPdist`/`SpatialCdist`/
`SpatialDistanceMatrix`/`SpatialConvexHull`).
