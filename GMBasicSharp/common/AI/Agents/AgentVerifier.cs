// FILE: Basic.Agents.AgentVerifier — Fase 4 GMBasic-Agent-Runtime: verifier/retry generico.
// Un agente (o una funzione) produce un risultato; un REQUIRE lo valida; se fallisce si ritenta
// con feedback dell'errore nel prompt, fino a maxRetries, poi fallimento ESPLICITO (mai silenzioso).
// Non duplica CriticEnsemble/AgentReflection (strumenti 'dentro'); è l'involucro riusabile fuori
// da RecursiveMAS. Nome AI.VERIFY/AGENT.VERIFY (mai 'VERIFY' nudo — occupato dalla crypto).
using System;

namespace Basic.Agents;

/// <summary>Risultato di un run verificato.</summary>
public sealed record VerifyOutcome(bool Ok, string Result, int Attempts, string? LastError);

public static class AgentVerifier
{
    /// <summary>Esegue `attempt(attemptIndex, feedback)` — che produce un risultato e ritorna
    /// anche un eventuale errore ("" = ok) — e valida il risultato. Su errore/validazione fallita
    /// ritenta con feedback (maxRetries totali). Ritorna esito esplicito: Ok=true con result,
    /// oppure Ok=false con LastError dopo i retry esauriti. Mai silenzioso.</summary>
    /// <param name="attempt">(attemptNumber 1-based, feedback) → (result, error). feedback="" al primo.
    /// error non vuoto = il tentativo non è andato a buon fine (es. compile failed / agente è fallito).</param>
    /// <param name="validate">Validazione del risultato prodotto (REQUIRE). true = accetta.</param>
    /// <param name="maxRetries">Massimo numero di TENTATIVI (≥1).</param>
    public static VerifyOutcome Verify(
        Func<int, string, (string result, string error)> attempt,
        Func<string, bool>? validate = null,
        int maxRetries = 3)
    {
        if (maxRetries < 1) maxRetries = 1;
        validate ??= static _ => true;
        string feedback = "";
        for (int i = 1; i <= maxRetries; i++)
        {
            var (result, error) = attempt(i, feedback);
            if (!string.IsNullOrEmpty(error))
            {
                feedback = $"Tentativo {i} fallito: {error}. Ritenta correggendo.";
                continue;   // retry
            }
            if (validate(result)) return new VerifyOutcome(true, result, i, null);
            feedback = $"Tentativo {i}: il risultato non soddisfa il REQUIRE. Risultato:\n{result}\nRitenta correggendo.";
        }
        return new VerifyOutcome(false, "", maxRetries, feedback);
    }
}
