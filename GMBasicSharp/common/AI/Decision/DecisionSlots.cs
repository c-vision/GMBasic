// AI/Decision/DecisionSlots.cs
//
// I 24 slot canonici del decision model (spec upstream ms-swift, 2026-10-21):
//   NOUL  = false, true                (2)
//   SCORE = 0..5                       (6)
//   CHOICE= A..P                       (16)
// Sono i token/etichette che la testa di scoring conosce; il readout LLMEnv lavora su un
// token singolo per slot. Le etichette sono STABILI: cambiarle invalida bias/T allenati.

namespace Basic.Decision;

public static class DecisionSlots
{
    public static readonly IReadOnlyList<DecisionOption> Noul =
    [
        new("false", "false"),
        new("true", "true"),
    ];

    public static readonly IReadOnlyList<DecisionOption> Score =
    [
        new("0", "0"), new("1", "1"), new("2", "2"),
        new("3", "3"), new("4", "4"), new("5", "5"),
    ];

    public static readonly IReadOnlyList<DecisionOption> Choice =
    [
        new("A", "A"), new("B", "B"), new("C", "C"), new("D", "D"),
        new("E", "E"), new("F", "F"), new("G", "G"), new("H", "H"),
        new("I", "I"), new("J", "J"), new("K", "K"), new("L", "L"),
        new("M", "M"), new("N", "N"), new("O", "O"), new("P", "P"),
    ];

    public static DecisionRequest CreateNoul(string state, string? instructions = null)
        => new(state, DecisionKind.Noul, Noul, instructions);

    public static DecisionRequest CreateScore(string state, string? instructions = null)
        => new(state, DecisionKind.Score, Score, instructions);

    /// <summary>SCORE su una rubrica ESPLICITA `lo..hi` (oggi solo 0..5 canonica e 0..10 del
    /// contratto linguistico). Le opzioni sono gli interi come etichette/id; il mapping alla
    /// testa di scoring (24 slot) resta per gli id 0..5, gli altri hanno bias neutro.</summary>
    public static DecisionRequest CreateScoreRange(string state, int lo, int hi, string? instructions = null)
    {
        if (lo < 0 || hi < lo || hi > 10)
            throw new ArgumentException($"SCORE range non valido: {lo} TO {hi} (ammesso 0..10).");
        var options = new List<DecisionOption>(hi - lo + 1);
        for (var v = lo; v <= hi; v++)
        {
            var id = v.ToString(System.Globalization.CultureInfo.InvariantCulture);
            options.Add(new DecisionOption(id, id));
        }
        return new DecisionRequest(state, DecisionKind.Score, options, instructions);
    }

    /// <summary>CHOICE con dominio libero: le etichette utente mappano gli slot A..P in ordine
    /// (la prima etichetta è A, la seconda B, ...). Oltre 16 opzioni è errore esplicito.</summary>
    public static DecisionRequest CreateChoice(string state, IReadOnlyList<string> labels, string? instructions = null)
    {
        if (labels is null || labels.Count == 0)
            throw new ArgumentException("CHOICE: almeno un'etichetta.", nameof(labels));
        if (labels.Count > Choice.Count)
            throw new ArgumentException($"CHOICE: massimo {Choice.Count} opzioni (slot A..P).", nameof(labels));
        var options = new List<DecisionOption>(labels.Count);
        for (var i = 0; i < labels.Count; i++)
            options.Add(new DecisionOption(Choice[i].Id, labels[i]));
        return new DecisionRequest(state, DecisionKind.Choice, options, instructions);
    }
}
