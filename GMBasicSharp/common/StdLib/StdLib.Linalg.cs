// RETIRED (2026-11-28).
//
// La surface builtin `LINALG` a valori `MATRIX` (decomposizioni con campi, FACTOR,
// norme, funzioni matriciali) non è più registrata. Tutte le operazioni sono ora
// FFI su array piatti in `shared/include/linalg.inc` (dylib `gmbmath`), compreso
// `LINALG.FACTOR(a, rows, cols, kind, out, meta)`.
//
// I tipi di decomposizione (`LuDecompositionType`, …) restano definiti in
// `Ast/DataTypes.cs` per compatibilità dei nodi AST, ma non sono più esposti come
// namespace del linguaggio.
