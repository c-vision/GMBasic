// FILE: AI.Eval.EvalCase
namespace AI.Eval;

/// <summary>Esito di un caso di eval: passato + dettaglio per il report.</summary>
public sealed record EvalResult(bool Passed, string Detail);

/// <summary>Un caso: setup + prompt + attese verificate. Eseguito dal runner.</summary>
public sealed record EvalCase(string Id, string Description, Func<EvalResult> Run);
