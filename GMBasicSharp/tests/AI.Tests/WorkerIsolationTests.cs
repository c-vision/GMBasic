// WorkerIsolationTests.cs — P3 sostanza (asse C): `AI WORKER "<TASK>"` = INVOCAZIONE PULITA.
// La vista della chiamata è SOLO la memoria esterna del run (workspace), mai la chat history;
// normalizzata dal Context Engine, con esito esplicito (sentinella provider / rifiuto budget).
using System;
using System.IO;
using Basic.AI.Agents;
using Basic.AI.Context;
using Xunit;

namespace AI.Tests;

public sealed class WorkerIsolationTests : IDisposable
{
    private readonly string _root = Path.Combine(Path.GetTempPath(), "gmb_worker_" + Guid.NewGuid().ToString("N"));

    public WorkerIsolationTests() { Directory.CreateDirectory(_root); Reset(); }
    public void Dispose() { Reset(); try { Directory.Delete(_root, true); } catch { } }

    /// <summary>Reset dello stato di processo. Il completer di default è la SENTINELLA esplicita
    /// ("nessun provider"): il resolver globale (`LlmCompleterResolver.Default`) è condiviso con
    /// altre classi di test eseguite in parallelo, quindi un test del worker NON deve dipendere da
    /// esso per essere deterministico (misurato: due test passavano isolati e fallivano nella suite
    /// completa perché un'altra classe aveva registrato un completer globale).</summary>
    private static void Reset()
    {
        AiWorkerRuntime.Reset();
        AiContextEngine.Reset();
        AiWorkerRuntime.Completer = (_, _) => AiWorkerRuntime.NoCompleter;
        AiWorkerRuntime.RequireEvaluator = null;
        Basic.Agents.TaskGraphRegistry.Clear();   // grafo dei task: singleton di processo
        Basic.Agents.PlanRegistry.Clear();        // piani: singleton di processo
        Basic.Agents.ArtifactStore.Clear();       // artifact del best-so-far
        Basic.Agents.ReplayRegistry.Clear();      // sessioni di replay
        Basic.Agents.ReplayRegistry.Executor = null;
        Basic.AI.Agents.ObservationRegistry.Reset();
    }

    private string Ws(string name) => Path.Combine(_root, name).Replace("\\", "/");

    [Fact]
    public void Invoke_SenzaWorker_RitornaNoWorker()
    {
        Assert.Equal(AiWorkerRuntime.InvokeNoWorker, AiWorkerRuntime.WorkerInvoke("ANALYZE"));
        Assert.Equal(0, AiWorkerRuntime.WorkerRounds());
    }

    [Fact]
    public void VistaCurata_SoloMemoriaEsternaDelRun()
    {
        var ws = Ws("w1");
        AiWorkerRuntime.WorkerNew("w1", $"{{\"workspace\":\"{ws}\",\"budget\":4000}}");
        File.WriteAllText(Path.Combine(ws, "plan.md"), "1. leggi\n2. scrivi");
        File.WriteAllText(Path.Combine(ws, "solution"), "soluzione precedente");
        AiWorkerRuntime.StateSave("w1", "notes", "greedy algorithm");

        var w = new AiWorkerRuntime.AiWorker { Name = "w1", Workspace = ws };
        var view = AiWorkerRuntime.BuildCuratedView(w, "ANALYZE");

        Assert.Contains("TASK: ANALYZE", view);
        Assert.Contains("PLAN:", view);
        Assert.Contains("1. leggi", view);
        Assert.Contains("SOLUTION:", view);
        Assert.Contains("STATE notes:", view);
        Assert.Contains("greedy algorithm", view);
        // ISOLAMENTO: la vista non contiene nulla che non sia memoria del run.
        Assert.DoesNotContain("History", view);
        Assert.DoesNotContain("user:", view);
    }

    [Fact]
    public void Invoke_SenzaCompleter_ErroreEsplicitoNonSilenzioso()
    {
        var ws = Ws("w2");
        AiWorkerRuntime.WorkerNew("w2", $"{{\"workspace\":\"{ws}\",\"budget\":4000}}");
        AiWorkerRuntime.StateSave("w2", "notes", "nota di stato");

        var rc = AiWorkerRuntime.WorkerInvoke("IMPLEMENT");

        Assert.Equal(AiWorkerRuntime.InvokeProviderError, rc);
        Assert.Equal(1, AiWorkerRuntime.WorkerRounds());
        Assert.Equal(1, AiWorkerRuntime.WorkerRescued());
        Assert.True(AiWorkerRuntime.WorkerSpend() > 0);
        Assert.Equal(AiWorkerRuntime.NoCompleter, AiWorkerRuntime.StateLoad("w2", "last_error"));
        Assert.Equal(AiWorkerRuntime.NoCompleter, AiWorkerRuntime.StateLoad("w2", "solution"));
        // memoria esterna ispezionabile
        Assert.True(File.Exists(Path.Combine(ws, "context.md")));
    }

    [Fact]
    public void Invoke_ConCompleterIniettato_EsegueESalvaLaSoluzione()
    {
        var ws = Ws("w3");
        AiWorkerRuntime.WorkerNew("w3", $"{{\"workspace\":\"{ws}\",\"budget\":4000}}");
        string? visto = null;
        AiWorkerRuntime.Completer = (system, user) => { visto = user; return "RISPOSTA"; };

        var rc = AiWorkerRuntime.WorkerInvoke("VERIFY");

        Assert.Equal(AiWorkerRuntime.InvokeOk, rc);
        Assert.Equal(1, AiWorkerRuntime.WorkerRounds());
        Assert.Equal(0, AiWorkerRuntime.WorkerRescued());
        Assert.Equal("RISPOSTA", AiWorkerRuntime.StateLoad("w3", "solution"));
        Assert.Contains("TASK: VERIFY", visto);
        Assert.Equal("RISPOSTA", File.ReadAllText(Path.Combine(ws, "solution")));
    }

    [Fact]
    public void Invoke_BudgetErrorPolicy_RifiutaPrimaDellaChiamata()
    {
        var ws = Ws("w4");
        AiWorkerRuntime.WorkerNew("w4", $"{{\"workspace\":\"{ws}\",\"budget\":8}}");
        AiContextEngine.SetPolicy(AiWorkerRuntime.ContextKey("w4"), "error");
        AiWorkerRuntime.StateSave("w4", "notes", "una nota molto piu lunga del budget del worker");
        var chiamato = false;
        AiWorkerRuntime.Completer = (_, _) => { chiamato = true; return "X"; };

        var rc = AiWorkerRuntime.WorkerInvoke("VERIFY");

        Assert.Equal(AiWorkerRuntime.InvokeBudgetRefused, rc);
        Assert.False(chiamato);                                   // rifiutata PRIMA della chiamata
        Assert.Equal(1, AiWorkerRuntime.WorkerRounds());           // il round è contato
        Assert.Equal(1, AiWorkerRuntime.WorkerRescued());
        Assert.Contains("overflow", AiWorkerRuntime.StateLoad("w4", "last_error"));
    }

    [Fact]
    public void Invoke_BudgetTruncate_ProcedeConVistaTagliata()
    {
        var ws = Ws("w5");
        AiWorkerRuntime.WorkerNew("w5", $"{{\"workspace\":\"{ws}\",\"budget\":40}}");
        AiContextEngine.SetPolicy(AiWorkerRuntime.ContextKey("w5"), "truncate");
        AiWorkerRuntime.StateSave("w5", "notes", new string('x', 500));
        string? visto = null;
        AiWorkerRuntime.Completer = (_, user) => { visto = user; return "OK"; };

        Assert.Equal(AiWorkerRuntime.InvokeOk, AiWorkerRuntime.WorkerInvoke("ANALYZE"));
        Assert.NotNull(visto);
        Assert.True(visto!.Length <= 40, $"vista {visto.Length} > budget 40");
        Assert.True(AiContextEngine.GetOverflow(AiWorkerRuntime.ContextKey("w5")) > 0);
    }

    [Fact]
    public void WorkerDiversi_WorkspaceVisteEStatiSeparati()
    {
        var a = Ws("a"); var b = Ws("b");
        AiWorkerRuntime.WorkerNew("a", $"{{\"workspace\":\"{a}\",\"budget\":4000}}");
        AiWorkerRuntime.WorkerNew("b", $"{{\"workspace\":\"{b}\",\"budget\":4000}}");
        AiWorkerRuntime.StateSave("a", "notes", "solo-di-a");
        AiWorkerRuntime.StateSave("b", "notes", "solo-di-b");
        AiWorkerRuntime.Completer = (_, _) => "OK";

        Assert.Equal(AiWorkerRuntime.InvokeOk, AiWorkerRuntime.WorkerInvoke("ANALYZE"));  // worker corrente = b
        Assert.Contains("solo-di-b", AiContextEngine.GetView(AiWorkerRuntime.ContextKey("b")));
        Assert.DoesNotContain("solo-di-a", AiContextEngine.GetView(AiWorkerRuntime.ContextKey("b")));
        Assert.Equal("solo-di-a", AiWorkerRuntime.StateLoad("a", "notes"));
        Assert.Equal("solo-di-b", AiWorkerRuntime.StateLoad("b", "notes"));
    }

    // ── P4 residue: AI WORKER LOOP (loop isolato multi-round, budget CUMULATIVO, best-so-far) ──

    [Fact]
    public void Loop_SenzaProvider_SiFermaAlPrimoRound()
    {
        var ws = Ws("l1");
        AiWorkerRuntime.WorkerNew("l1", $"{{\"workspace\":\"{ws}\",\"budget\":4000}}");
        AiWorkerRuntime.StateSave("l1", "notes", "nota del run");

        var rc = AiWorkerRuntime.WorkerLoop("ANALYZE", 3);

        Assert.Equal(AiWorkerRuntime.InvokeProviderError, rc);
        Assert.Equal(1, AiWorkerRuntime.WorkerRounds());          // un solo round tentato
        Assert.Equal(1, AiWorkerRuntime.WorkerRescued());
        Assert.Equal("", AiWorkerRuntime.StateLoad("l1", "best")); // nessun best da un errore
    }

    [Fact]
    public void Loop_ConCompleter_EsegueTuttiIRound_EBestSoFarPromosso()
    {
        var ws = Ws("l2");
        AiWorkerRuntime.WorkerNew("l2", $"{{\"workspace\":\"{ws}\",\"budget\":100000}}");
        var viste = new System.Collections.Generic.List<string>();
        var n = 0;
        AiWorkerRuntime.Completer = (_, user) => { viste.Add(user); return "soluzione" + (++n); };

        Assert.Equal(AiWorkerRuntime.InvokeOk, AiWorkerRuntime.WorkerLoop("IMPLEMENT", 3));

        Assert.Equal(3, AiWorkerRuntime.WorkerRounds());
        Assert.Equal(0, AiWorkerRuntime.WorkerRescued());
        // ogni round vede la memoria esterna AGGIORNATA dal precedente (isolamento + accumulo)
        Assert.Contains("TASK: IMPLEMENT", viste[0]);
        Assert.DoesNotContain("soluzione1", viste[0]);
        Assert.Contains("soluzione1", viste[1]);
        Assert.Contains("soluzione2", viste[2]);
        Assert.Equal("soluzione3", AiWorkerRuntime.StateLoad("l2", "best"));
        Assert.Equal("soluzione3", File.ReadAllText(Path.Combine(ws, "best")));
    }

    [Fact]
    public void Loop_BudgetCumulativoDelRun_StopEsplicito()
    {
        var ws = Ws("l3");
        // budget del RUN = 40 char; la vista del run (TASK + STATE notes con 30 char) supera già
        // il budget ⇒ il loop NON tenta nemmeno il primo round (nessun round contato, rescue +1).
        AiWorkerRuntime.WorkerNew("l3", $"{{\"workspace\":\"{ws}\",\"budget\":40}}");
        AiContextEngine.SetPolicy(AiWorkerRuntime.ContextKey("l3"), "error");
        AiWorkerRuntime.StateSave("l3", "notes", new string('x', 30));
        AiWorkerRuntime.Completer = (_, _) => "OK";

        var rc = AiWorkerRuntime.WorkerLoop("ANALYZE", 5);

        Assert.Equal(AiWorkerRuntime.InvokeBudgetRefused, rc);
        Assert.Equal(0, AiWorkerRuntime.WorkerRounds());   // nessuna invocazione tentata
        Assert.Equal(1, AiWorkerRuntime.WorkerRescued());
        Assert.Contains("run budget exhausted", AiWorkerRuntime.StateLoad("l3", "last_error"));
        Assert.True(AiWorkerRuntime.WorkerSpend() <= 40, "lo spend cumulativo non può superare il budget del run");
    }

    [Fact]
    public void Loop_BudgetCumulativo_UnRoundPoiStop()
    {
        var ws = Ws("l3b");
        AiWorkerRuntime.WorkerNew("l3b", $"{{\"workspace\":\"{ws}\",\"budget\":60}}");
        AiContextEngine.SetPolicy(AiWorkerRuntime.ContextKey("l3b"), "truncate"); // prima vista tagliata
        AiWorkerRuntime.StateSave("l3b", "notes", new string('y', 100));
        AiWorkerRuntime.Completer = (_, _) => "OK";

        var rc = AiWorkerRuntime.WorkerLoop("ANALYZE", 5);

        // truncate: il PRIMO round parte (vista tagliata a 60), poi il budget del run è consumato
        // → stop esplicito senza tentare altri round.
        Assert.Equal(AiWorkerRuntime.InvokeBudgetRefused, rc);
        Assert.Equal(1, AiWorkerRuntime.WorkerRounds());
        Assert.Equal(1, AiWorkerRuntime.WorkerRescued());
        Assert.Contains("run budget exhausted", AiWorkerRuntime.StateLoad("l3b", "last_error"));
        Assert.True(AiWorkerRuntime.WorkerSpend() <= 60, $"spend {AiWorkerRuntime.WorkerSpend()} oltre il budget");
    }

    [Fact]
    public void Loop_SenzaWorker_RitornaNoWorker()
    {
        Assert.Equal(AiWorkerRuntime.InvokeNoWorker, AiWorkerRuntime.WorkerLoop("X", 3));
    }

    [Fact]
    public void Loop_RoundZero_UsaIlDefault()
    {
        var ws = Ws("l4");
        AiWorkerRuntime.WorkerNew("l4", $"{{\"workspace\":\"{ws}\",\"budget\":100000}}");
        var n = 0;
        AiWorkerRuntime.Completer = (_, _) => "s" + (++n);

        Assert.Equal(AiWorkerRuntime.InvokeOk, AiWorkerRuntime.WorkerLoop("ANALYZE", 0));
        Assert.Equal(AiWorkerRuntime.DefaultLoopRounds, AiWorkerRuntime.WorkerRounds());
    }

    // ── LOOP VERIFICATO: auditor indipendente (REQUIRE) + rework informato ────────────────

    [Fact]
    public void LoopVerificato_AuditSoddisfatto_StopAnticipato()
    {
        var ws = Ws("v1");
        AiWorkerRuntime.WorkerNew("v1", $"{{\"workspace\":\"{ws}\",\"budget\":100000}}");
        var rounds = 0;
        AiWorkerRuntime.Completer = (_, _) => "SOLUZIONE-" + (++rounds);
        AiWorkerRuntime.RequireEvaluator = (req, res) => res.Contains("SOLUZIONE-2");   // vera al 2° round

        Assert.Equal(AiWorkerRuntime.InvokeOk, AiWorkerRuntime.WorkerLoop("IMPLEMENT", 5, "RESULT$ contiene SOLUZIONE-2"));

        Assert.Equal(2, AiWorkerRuntime.WorkerRounds());        // stop anticipato appena verificato
        Assert.Equal(0, AiWorkerRuntime.WorkerRescued());
        Assert.Equal("1", AiWorkerRuntime.StateLoad("v1", "verified"));
        Assert.Equal("SOLUZIONE-2", AiWorkerRuntime.StateLoad("v1", "best"));
    }

    [Fact]
    public void LoopVerificato_AuditMaiSoddisfatto_EsitoAuditFailed_ConReworkInformato()
    {
        var ws = Ws("v2");
        AiWorkerRuntime.WorkerNew("v2", $"{{\"workspace\":\"{ws}\",\"budget\":100000}}");
        var viste = new System.Collections.Generic.List<string>();
        AiWorkerRuntime.Completer = (_, user) => { viste.Add(user); return "RISPOSTA"; };
        AiWorkerRuntime.RequireEvaluator = (_, _) => false;

        Assert.Equal(AiWorkerRuntime.InvokeAuditFailed, AiWorkerRuntime.WorkerLoop("VERIFY", 3, "MAI"));

        Assert.Equal(3, AiWorkerRuntime.WorkerRounds());
        Assert.Equal(1, AiWorkerRuntime.WorkerRescued());
        Assert.Contains("audit failed after 3 round(s)", AiWorkerRuntime.StateLoad("v2", "last_error"));
        Assert.Equal("", AiWorkerRuntime.StateLoad("v2", "verified"));
        // REWORK INFORMATO: dal 2° round la vista contiene l'esito dell'audit fallito
        Assert.DoesNotContain("STATE audit_error", viste[0]);
        Assert.Contains("STATE audit_error", viste[1]);
        Assert.Contains("STATE audit_error", viste[2]);
    }

    [Fact]
    public void LoopVerificato_SenzaValutatoreRegistrato_ErroreEsplicito()
    {
        var ws = Ws("v3");
        AiWorkerRuntime.WorkerNew("v3", $"{{\"workspace\":\"{ws}\",\"budget\":1000}}");
        AiWorkerRuntime.RequireEvaluator = null;
        AiWorkerRuntime.Completer = (_, _) => "X";

        var ex = Assert.Throws<InvalidOperationException>(() => AiWorkerRuntime.WorkerLoop("X", 2, "LEN(RESULT$) > 0"));
        Assert.Contains("nessun valutatore REQUIRE", ex.Message);
    }

    // ── MANAGER LOOP (P4): task graph + auditor + dissenso loggato ────────────────────────

    [Fact]
    public void Manage_SenzaTask_EsitoOkSenzaInvocazioni()
    {
        Basic.Agents.TaskGraphRegistry.Clear();
        var ws = Ws("mg0");
        AiWorkerRuntime.WorkerNew("mg0", $"{{\"workspace\":\"{ws}\",\"budget\":4000}}");
        Assert.Equal(1.0, AiWorkerRuntime.WorkerManage(3, ""));
        Assert.Equal("1", AiWorkerRuntime.StateLoad("mg0", "manage_exit"));
        Assert.Equal(0, AiWorkerRuntime.WorkerRounds());
    }

    [Fact]
    public void Manage_TaskConDipendenza_MarcaDoneInOrdine()
    {
        Basic.Agents.TaskGraphRegistry.Clear();
        var ws = Ws("mg1");
        AiWorkerRuntime.WorkerNew("mg1", $"{{\"workspace\":\"{ws}\",\"budget\":100000}}");
        Basic.Agents.TaskGraphRegistry.Define("t2", "secondo");
        Basic.Agents.TaskGraphRegistry.Define("t1", "primo");
        Basic.Agents.TaskGraphRegistry.Depends("t2", "t1");
        var ordine = new System.Collections.Generic.List<string>();
        AiWorkerRuntime.Completer = (_, user) =>
        {
            // il task corrente è la PRIMA riga della vista curata ("TASK: <id> — <desc>"): lo stato
            // dei task precedenti resta nella vista (state/*), quindi non si può cercare "t1" nel testo.
            var prima = user.Split('\n')[0];
            ordine.Add(prima.Contains("t1") ? "t1" : "t2");
            return "OK-" + ordine.Count;
        };
        AiWorkerRuntime.RequireEvaluator = (_, _) => true;

        Assert.Equal(AiWorkerRuntime.InvokeOk, AiWorkerRuntime.WorkerManage(5, "LEN(RESULT$) > 0"));

        Assert.Equal(new[] { "t1", "t2" }, ordine);                       // il manager rispetta il DAG
        Assert.Contains("\"status\":\"done\"", Basic.Agents.TaskGraphRegistry.Status("t1"));
        Assert.Contains("\"status\":\"done\"", Basic.Agents.TaskGraphRegistry.Status("t2"));
        Assert.Equal("1", AiWorkerRuntime.StateLoad("mg1", "manage_exit"));
    }

    [Fact]
    public void Manage_Dissenso_LoggatoEVisibileNelRework_PoiSuccesso()
    {
        Basic.Agents.TaskGraphRegistry.Clear();
        var ws = Ws("mg2");
        AiWorkerRuntime.WorkerNew("mg2", $"{{\"workspace\":\"{ws}\",\"budget\":100000}}");
        Basic.Agents.TaskGraphRegistry.Define("t1", "task");
        var viste = new System.Collections.Generic.List<string>();
        var n = 0;
        AiWorkerRuntime.Completer = (_, user) => { viste.Add(user); return "RISPOSTA-" + (++n); };
        // l'audit rifiuta il primo tentativo e accetta il secondo
        AiWorkerRuntime.RequireEvaluator = (_, res) => res.Contains("RISPOSTA-2");

        Assert.Equal(AiWorkerRuntime.InvokeOk, AiWorkerRuntime.WorkerManage(4, "accetta RISPOSTA-2"));

        Assert.Equal(2, AiWorkerRuntime.WorkerRounds());
        Assert.Equal("1", AiWorkerRuntime.StateLoad("mg2", "dissents"));           // dissenso contato
        Assert.Equal("RISPOSTA-1", AiWorkerRuntime.StateLoad("mg2", "dissent"));   // esito scartato loggato
        // (il risultato FINALE del task è quello accettato: il dissenso resta nel log `state/dissent`)
        // REWORK INFORMATO: dal 2° tentativo la vista contiene il dissenso (state/* nella vista)
        Assert.DoesNotContain("STATE dissent", viste[0]);
        Assert.Contains("STATE dissent", viste[1]);
        Assert.Contains("RISPOSTA-1", viste[1]);
        Assert.Contains("\"status\":\"done\"", Basic.Agents.TaskGraphRegistry.Status("t1"));
    }

    [Fact]
    public void Manage_CicliEsauritiConTaskPendenti_EsitoMenoTre()
    {
        Basic.Agents.TaskGraphRegistry.Clear();
        var ws = Ws("mg3");
        AiWorkerRuntime.WorkerNew("mg3", $"{{\"workspace\":\"{ws}\",\"budget\":100000}}");
        Basic.Agents.TaskGraphRegistry.Define("t1", "task");
        AiWorkerRuntime.Completer = (_, _) => "X";
        AiWorkerRuntime.RequireEvaluator = (_, _) => false;      // mai soddisfatto

        Assert.Equal(AiWorkerRuntime.InvokeCyclesExhausted, AiWorkerRuntime.WorkerManage(2, "MAI"));

        Assert.Equal(2, AiWorkerRuntime.WorkerRounds());
        Assert.Contains("cicli esauriti", AiWorkerRuntime.StateLoad("mg3", "last_error"));
        Assert.Equal("-3", AiWorkerRuntime.StateLoad("mg3", "manage_exit"));
    }

    [Fact]
    public void Manage_DeadlockDipendenze_EsitoMenoQuattro()
    {
        Basic.Agents.TaskGraphRegistry.Clear();
        var ws = Ws("mg4");
        AiWorkerRuntime.WorkerNew("mg4", $"{{\"workspace\":\"{ws}\",\"budget\":4000}}");
        Basic.Agents.TaskGraphRegistry.Define("a", "A");
        Basic.Agents.TaskGraphRegistry.Define("b", "B");
        Basic.Agents.TaskGraphRegistry.Depends("a", "b");
        Basic.Agents.TaskGraphRegistry.Depends("b", "a");
        var chiamato = false;
        AiWorkerRuntime.Completer = (_, _) => { chiamato = true; return "X"; };

        Assert.Equal(AiWorkerRuntime.InvokeDeadlock, AiWorkerRuntime.WorkerManage(3, ""));
        Assert.False(chiamato);                                   // nessuna invocazione su un grafo bloccato
        Assert.Equal("-4", AiWorkerRuntime.StateLoad("mg4", "manage_exit"));
    }

    [Fact]
    public void Manage_BestSoFar_RegistratoAncheComeArtifact()
    {
        Basic.Agents.TaskGraphRegistry.Clear();
        Basic.Agents.ArtifactStore.Clear();
        var ws = Ws("mg5");
        AiWorkerRuntime.WorkerNew("mg5", $"{{\"workspace\":\"{ws}\",\"budget\":100000}}");
        Basic.Agents.TaskGraphRegistry.Define("t1", "task");
        AiWorkerRuntime.Completer = (_, _) => "SOLUZIONE-OK";
        AiWorkerRuntime.RequireEvaluator = (_, _) => true;

        Assert.Equal(AiWorkerRuntime.InvokeOk, AiWorkerRuntime.WorkerManage(2, "accetta"));

        // P4 "best-so-far checkpoint+artifact": file del run + RIFERIMENTO (path) nell'ArtifactStore
        Assert.Equal(Path.Combine(ws, "best"), Basic.Agents.ArtifactStore.Get("best:mg5"));
        Assert.Equal("file", Basic.Agents.ArtifactStore.Kind("best:mg5"));
        Assert.Equal("SOLUZIONE-OK", File.ReadAllText(Path.Combine(ws, "best")));
        Assert.Equal("SOLUZIONE-OK", AiWorkerRuntime.StateLoad("mg5", "best"));
    }

    [Fact]
    public void Manage_PianoNonApprovato_EsitoMenoCinque()
    {
        Basic.Agents.TaskGraphRegistry.Clear();
        Basic.Agents.PlanRegistry.Clear();
        var ws = Ws("mg6");
        AiWorkerRuntime.WorkerNew("mg6", $"{{\"workspace\":\"{ws}\",\"budget\":4000}}");
        Basic.Agents.TaskGraphRegistry.Define("t1", "task");
        Basic.Agents.PlanRegistry.Define("p1", "obiettivo");
        Basic.Agents.PlanRegistry.Add("p1", "t1");
        var chiamato = false;
        AiWorkerRuntime.Completer = (_, _) => { chiamato = true; return "X"; };

        // piano in bozza → nessun lavoro sul piano
        Assert.Equal(AiWorkerRuntime.InvokePlanNotApproved, AiWorkerRuntime.WorkerManage(2, "", "p1"));
        Assert.False(chiamato);
        Assert.Contains("non approvato", AiWorkerRuntime.StateLoad("mg6", "last_error"));

        // approvato → lavora SOLO sui task del piano
        Basic.Agents.PlanRegistry.Approve("p1");
        Basic.Agents.TaskGraphRegistry.Define("fuori", "task fuori piano");
        AiWorkerRuntime.RequireEvaluator = (_, _) => true;
        Assert.Equal(AiWorkerRuntime.InvokeOk, AiWorkerRuntime.WorkerManage(3, "ok", "p1"));
        Assert.Contains("\"status\":\"done\"", Basic.Agents.TaskGraphRegistry.Status("t1"));
        Assert.Contains("\"status\":\"pending\"", Basic.Agents.TaskGraphRegistry.Status("fuori"));
    }

    // ── E1: riesecuzione AUTOMATICA degli eventi (AI REPLAY AUTO) ─────────────────────────

    [Fact]
    public void ReplayAuto_RiesegueEventi_EsitiPerEvento()
    {
        Basic.Agents.ReplayRegistry.Clear();
        Basic.Agents.ReplayRegistry.Start("s1");
        Basic.Agents.ReplayRegistry.Record("1 + 1");
        Basic.Agents.ReplayRegistry.Record("LEN(\"abc\")");
        Basic.Agents.ReplayRegistry.Record("EVENTO_NON_VALIDO(");
        Basic.Agents.ReplayRegistry.Stop();
        // executor deterministico: "eseguibile" = l'evento non contiene "NON_VALIDO"
        Basic.Agents.ReplayRegistry.Executor = ev => ev.Contains("NON_VALIDO") ? "error: non eseguibile" : "";

        Assert.Equal(2, Basic.Agents.ReplayRegistry.Auto("s1"));

        var auto = Basic.Agents.ReplayRegistry.Get(Basic.Agents.ReplayRegistry.AutoSessionId("s1"));
        Assert.Contains("ok: 1 + 1", auto);
        Assert.Contains("ok: LEN(", auto);
        Assert.Contains("error: EVENTO_NON_VALIDO(", auto);      // MAI silenzioso
        Assert.Equal(2, auto.Split("ok: ").Length - 1);   // 2 eventi eseguiti
    }

    [Fact]
    public void ReplayAuto_SenzaExecutor_ErroreEsplicito()
    {
        Basic.Agents.ReplayRegistry.Clear();
        Basic.Agents.ReplayRegistry.Start("s2");
        Basic.Agents.ReplayRegistry.Record("1 + 1");
        Basic.Agents.ReplayRegistry.Stop();
        Basic.Agents.ReplayRegistry.Executor = null;

        var ex = Assert.Throws<InvalidOperationException>(() => Basic.Agents.ReplayRegistry.Auto("s2"));
        Assert.Contains("nessun executor", ex.Message);
    }

    [Fact]
    public void ReplayAuto_SessioneInesistente_NessunEvento()
    {
        Basic.Agents.ReplayRegistry.Clear();
        Basic.Agents.ReplayRegistry.Executor = _ => "";
        Assert.Equal(0, Basic.Agents.ReplayRegistry.Auto("non-esiste"));
    }

    // ── A5: fonti OBSERVE di fase 2 (DATABASE/PROCESS verificati; TEST/BROWSER parcheggiati) ──

    [Fact]
    public void ObserveDatabase_RegistraRiferimentoConTipo()
    {
        Basic.AI.Agents.ObservationRegistry.Reset();
        var db = Path.Combine(_root, "obs.sqlite");
        using (var c = new Microsoft.Data.Sqlite.SqliteConnection($"Data Source={db}"))
        {
            c.Open();
            using var cmd = c.CreateCommand();
            cmd.CommandText = "CREATE TABLE note(id INTEGER, testo TEXT); INSERT INTO note VALUES (1,'alfa'); INSERT INTO note VALUES (2,'beta');";
            cmd.ExecuteNonQuery();
        }
        var ws = Path.Combine(_root, "wdb");
        Directory.CreateDirectory(ws);
        var prev = Directory.GetCurrentDirectory();
        try
        {
            Directory.SetCurrentDirectory(_root);
            // hook minimale ma REALE: conta le righe via sqlite (stesso contratto del runtime C)
            var conns = new System.Collections.Generic.Dictionary<long, Microsoft.Data.Sqlite.SqliteConnection>();
            Basic.AI.Agents.ObservationSources.SqlOpenHandle = path =>
            {
                var cn = new Microsoft.Data.Sqlite.SqliteConnection($"Data Source={path}");
                cn.Open();
                conns[1] = cn;
                return 1;
            };
            Basic.AI.Agents.ObservationSources.SqlQueryRows = (h, sql) =>
            {
                using var cmd = conns[h].CreateCommand();
                cmd.CommandText = sql;
                using var r = cmd.ExecuteReader();
                var n = 0; while (r.Read()) n++;
                return n;
            };
            Basic.AI.Agents.ObservationSources.SqlCloseHandle = h => { conns[h].Close(); conns.Remove(h); };

            var id = Basic.AI.Agents.ObservationSources.ObserveDatabase(db, "SELECT * FROM note");
            Assert.True(id > 0);
            var json = Basic.AI.Agents.ObservationRegistry.Json(id);
            Assert.Contains("database", json);
            Assert.Contains("ai_observe_database.json", Basic.AI.Agents.ObservationRegistry.FileOf(id));
            var artifact = File.ReadAllText(Path.Combine(_root, "work", "ai_observe_database.json"));
            Assert.Contains("\"rows\":2", artifact);
            Assert.Contains("SELECT * FROM note", artifact);
        }
        finally
        {
            Directory.SetCurrentDirectory(prev);
            Basic.AI.Agents.ObservationSources.SqlOpenHandle = null;
            Basic.AI.Agents.ObservationSources.SqlQueryRows = null;
            Basic.AI.Agents.ObservationSources.SqlCloseHandle = null;
        }
    }

    [Fact]
    public void ObserveProcess_Inesistente_ErroreEsplicito()
    {
        Basic.AI.Agents.ObservationRegistry.Reset();
        Assert.Equal(-1, Basic.AI.Agents.ObservationSources.ObserveProcess("gmbasic_no_such_process_xyz"));
        Assert.Equal(0, Basic.AI.Agents.ObservationRegistry.Count());
    }

    [Fact]
    public void ObserveTest_ContrattoDellaFonte()
    {
        Basic.AI.Agents.ObservationRegistry.Reset();
        var prev = Directory.GetCurrentDirectory();
        // trova la root del repo risalendo dall'output del test
        var dir = new DirectoryInfo(AppContext.BaseDirectory);
        while (dir is not null && !Directory.Exists(Path.Combine(dir.FullName, "samples", "Fixtures")))
            dir = dir.Parent;
        Assert.NotNull(dir);
        var fixture = Path.Combine(dir!.FullName, "samples", "Fixtures", "AIVerifyFixtureOk.bas");
        Directory.CreateDirectory(Path.Combine(_root, "work"));
        try
        {
            Directory.SetCurrentDirectory(_root);
            // file inesistente → -1 SEMPRE (errore esplicito, nessuna osservazione inventata)
            Assert.Equal(-1, Basic.AI.Agents.ObservationSources.ObserveTest(Path.Combine(_root, "manca.bas")));
            Assert.Equal(0, Basic.AI.Agents.ObservationRegistry.Count());
            // fixture presente: o compila (id > 0) o lo strumento di compilazione non è disponibile
            // in-process (in xUnit l'Orchestrator può non essere caricabile) — mai un'osservazione finta.
            var id = Basic.AI.Agents.ObservationSources.ObserveTest(fixture);
            if (id > 0)
            {
                Assert.Contains("test", Basic.AI.Agents.ObservationRegistry.Json(id));
                Assert.Contains("ai_observe_test.json", Basic.AI.Agents.ObservationRegistry.FileOf(id));
            }
            else
            {
                Assert.Equal(0, Basic.AI.Agents.ObservationRegistry.Count());
            }
        }
        finally { Directory.SetCurrentDirectory(prev); }
    }

    [Fact]
    public void ObserveBrowser_UrlNonValido_ErroreEsplicitoImmediato()
    {
        Basic.AI.Agents.ObservationRegistry.Reset();
        // URL vuoto → -1 immediato (nessun accesso di rete): è il caso deterministico del MiniTest
        Assert.Equal(-1, Basic.AI.Agents.ObservationSources.ObserveBrowser(""));
        Assert.Equal(0, Basic.AI.Agents.ObservationRegistry.Count());
    }
}
