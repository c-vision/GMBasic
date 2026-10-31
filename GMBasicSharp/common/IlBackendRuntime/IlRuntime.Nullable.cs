// FILE: IlRuntime.Nullable.cs — predicati sui valori nullable / NIL.
// `x HAS VALUE` come chiamata runtime opaca (invece della sequenza inline
// load; ldnull; ceq; ldc.i4.0; ceq): ILOptim/SimplifyInsts ripiegava il pattern
// inline a costante 0, rendendo `HAS VALUE` sempre falso su IL ottimizzato
// (2026-09-23, Parita-5-Backend-Residui §2 — TypeInfoFullCoverage; il minitest
// minitests/nullable/nullable_hasvalue.bas perdeva "val-has" su IL).
// Convenzione BASIC: -1 vero / 0 falso (parità PCodeVM e VM/C).

namespace Basic.IlBackend;

public static partial class IlRuntime
{
    public static int NullableHasValue(object? v) => v is not null ? -1 : 0;
}
