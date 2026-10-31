// FILE: Basic.ISAM.IsamExpressionBridge

using Basic.Databases;
using Basic.ISAM;

namespace Basic.ISAM;

/// <summary>
/// Collega <see cref="Engine.Evaluator"/> e built-in ISAM al percorso unico di valutazione chiavi/filtri.
/// </summary>
internal static class IsamExpressionBridge
{
    /// <summary>
    /// Restituisce built-in → <paramref name="explicitEvaluator"/> → <see cref="Engine.Evaluator"/>.
    /// </summary>
    public static KeyExpressionEvaluatorCallback ResolveKeyEvaluator(
        Engine engine,
        KeyExpressionEvaluatorCallback? explicitEvaluator)
    {
        ArgumentNullException.ThrowIfNull(engine);
        var user = explicitEvaluator ?? engine.Evaluator;
        if (user == null)
        {
            throw new InvalidOperationException(
                "KeyExpressionEvaluatorCallback richiesto: passare a IsamQuery.From o impostare Engine.Evaluator.");
        }

        return IsamKeyExpressionEvaluator.Chain(user);
    }
}
