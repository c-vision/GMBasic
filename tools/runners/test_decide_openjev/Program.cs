// test_decide_openjev — valuta il decision engine locale su un dataset Open-Jev (MIT).
// Uso: test_decide_openjev <model_dir> <test.jsonl> [max_items]
// Righe: {"context":"…","options":["…"],"label":N}. Le opzioni sono multi-parola: si usa il
// verbalizer A..P (prompt "A) testo…", readout sul token dello slot) e si confronta l'indice.
using System.Globalization;
using System.Text.Json;
using Basic.AI.Rag;
using Basic.Decision;
using Basic.Providers;

if (args.Length < 2)
{
    Console.Error.WriteLine("uso: test_decide_openjev <model_dir> <test.jsonl> [max_items]");
    return 2;
}
var modelDir = args[0];
var path = args[1];
var maxItems = args.Length > 2 && int.TryParse(args[2], out var m) && m > 0 ? m : 0;

ProviderServer.Instance.ResetAll();
ProviderServer.Instance.Register(ProviderRole.Completion, "local", "mlx",
    new Dictionary<string, string>
    {
        ["modelDir"] = modelDir,
        ["maxTokens"] = "32",
        ["temperature"] = "0",
    });
ProviderServer.Instance.SetActive(ProviderRole.Completion, "local");
var completer = ProviderServer.Instance.GetActiveCompletion();
if (completer is not IDecisionCompleter decision)
{
    Console.Error.WriteLine("il provider attivo non implementa IDecisionCompleter");
    return 2;
}

int total = 0, correct = 0;
double brier = 0.0, nll = 0.0, ece = 0.0;
var binN = new int[10]; var binConf = new double[10]; var binAcc = new double[10];

foreach (var line in File.ReadLines(path))
{
    if (string.IsNullOrWhiteSpace(line)) continue;
    using var doc = JsonDocument.Parse(line);
    var root = doc.RootElement;
    var context = root.GetProperty("context").GetString() ?? "";
    var optionEls = root.GetProperty("options");
    var options = optionEls.EnumerateArray().Select(e => e.GetString() ?? "").ToList();
    var label = root.GetProperty("label").GetInt32();
    if (options.Count is < 2 or > 16 || label < 0 || label >= options.Count) continue;

    var request = DecisionSlots.CreateVerbalizedChoice(context, options);
    var results = decision.Decide(request);
    if (results.Count != options.Count) continue;

    var probs = results.Select(r => r.Probability).ToArray();
    var best = 0;
    for (var i = 1; i < probs.Length; i++) if (probs[i] > probs[best]) best = i;

    total++;
    if (best == label) correct++;
    for (var k = 0; k < probs.Length; k++)
    {
        var y = k == label ? 1.0 : 0.0;
        var d = probs[k] - y;
        brier += d * d;
    }
    nll -= Math.Log(Math.Max(probs[label], 1e-300));
    var conf = probs[best];
    var bin = Math.Clamp((int)(conf * 10), 0, 9);
    binN[bin]++;
    binConf[bin] += conf;
    binAcc[bin] += best == label ? 1.0 : 0.0;

    if (maxItems > 0 && total >= maxItems) break;
}

if (total == 0)
{
    Console.Error.WriteLine("nessun item valido");
    return 1;
}
for (var b = 0; b < 10; b++)
{
    if (binN[b] == 0) continue;
    ece += ((double)binN[b] / total) * Math.Abs(binAcc[b] / binN[b] - binConf[b] / binN[b]);
}
Console.WriteLine($"OPENJEV n={total} accuracy={((double)correct / total).ToString("0.000000", CultureInfo.InvariantCulture)} " +
                  $"brier={(brier / total).ToString("0.000000", CultureInfo.InvariantCulture)} " +
                  $"nll={(nll / total).ToString("0.000000", CultureInfo.InvariantCulture)} " +
                  $"ece={ece.ToString("0.000000", CultureInfo.InvariantCulture)}");
return 0;
