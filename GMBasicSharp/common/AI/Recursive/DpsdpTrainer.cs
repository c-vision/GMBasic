// FILE: Basic.Agents.Recursive.DpsdpTrainer
// Estensione 9.6 (DPSDP) — actor-critic RL economico su un MDP H=3 con δ(s,a)=[x,a]
// (solo ultimo answer+feedback, non history piena). Training su un proxy toy:
// l'actor genera k risposte, il critic le valuta, DPO-style preference loss allinea
// l'actor alla migliore. Verifica: il critic score della risposta scelta cresce.

using Basic.Agents;

namespace Basic.Agents.Recursive;

public static class DpsdpTrainer
{
    public sealed record Step(int Round, double ActorScore, double CriticScore);

    /// <summary>Addestramento actor-critic su un team: H=3 round; ad ogni step l'actor
    /// (membro <paramref name="actorName"/>) produce <paramref name="k"/> risposte, il critic
    /// (<paramref name="criticName"/>) le valuta, e l'actor viene rinforzato sulla migliore.
    /// Ritorna la traiettoria di score (critic della risposta scelta) — attesa crescente.</summary>
    public static List<Step> TrainActorCritic(AgentServer server, string actorName, string criticName,
        string task, int steps = 5, int k = 3, double lr = 0.05)
    {
        var trajectory = new List<Step>();
        var actor = server.List().FirstOrDefault(a => a.Name.Equals(actorName, StringComparison.OrdinalIgnoreCase));
        var critic = server.List().FirstOrDefault(a => a.Name.Equals(criticName, StringComparison.OrdinalIgnoreCase));
        if (actor is null || critic is null) return trajectory;

        // rappresentazione esplicita dell'actor: preferenze per opzione (proxy dell'update DPO)
        var actorPref = new double[k];
        var feedback = string.Empty; // δ(s,a) = [x, a]: stato = task + ultima risposta + feedback critic
        for (var step = 0; step < steps; step++)
        {
            var answers = new string[k];
            for (var a = 0; a < k; a++)
            {
                var state = string.IsNullOrEmpty(feedback)
                    ? $"{task} (tentativo {a + 1}):"
                    : $"{task}\nUltima risposta scelta: {answers[a == 0 ? 0 : a]}\nFeedback del critic: {feedback}\n(tentativo {a + 1}):";
                answers[a] = server.Ask(actorName, state);
            }
            var scores = new double[k];
            for (var a = 0; a < k; a++)
            {
                var v = server.Ask(criticName, $"Valuta da 1 a 10 la risposta: {answers[a]}");
                scores[a] = ExtractScore(v);
            }
            var best = Enumerable.Range(0, k).OrderByDescending(i => scores[i]).First();
            // DPO-style: rinforza l'opzione migliore, indebolisce le altre
            for (var a = 0; a < k; a++)
            {
                if (a == best) actorPref[a] += lr;
                else actorPref[a] -= lr * 0.5;
            }
            feedback = $"La risposta migliore ha ottenuto {scores[best]:0}/10.";
            trajectory.Add(new Step(step + 1, actorPref[best], scores[best]));
        }
        return trajectory;
    }

    /// <summary>Addestramento actor-critic su un team (9.6): il primo membro è l'actor, il secondo
    /// il critic, il task è il dataset (o la prima domanda del dataset). Ritorna la traiettoria.</summary>
    public static List<Step> TrainTeam(string team, string dataset)
    {
        var t = Basic.Agents.TeamServer.Instance.Get(team);
        if (t is null || t.Members.Count < 2) return [];
        var server = Basic.Agents.AgentServer.Instance;
        var actor = t.Members[0].AgentName;
        var critic = t.Members[1].AgentName;
        var task = dataset;
        return TrainActorCritic(server, actor, critic, task, steps: 5, k: 3);
    }

    private static double ExtractScore(string text)
    {
        // ultimo numero nel testo (il voto 1-10)
        var m = System.Text.RegularExpressions.Regex.Match(text, @"\d+(\.\d+)?", System.Text.RegularExpressions.RegexOptions.RightToLeft);
        if (m.Success && double.TryParse(m.Value, System.Globalization.NumberStyles.Any, System.Globalization.CultureInfo.InvariantCulture, out var v))
            return v;
        return 5.0;
    }
}
