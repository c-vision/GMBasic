using System;
using System.Collections.Generic;
using System.Globalization;
using System.Text;

namespace Basic.AI;

/// <summary>
/// Registro dei job training/apply lato MANAGED — speculare a
/// <c>native/gmb_runtime_c/src/gmb_job_registry.c</c> (stesso contratto, stesso JSON).
/// Serve la superficie <c>LLM.RUNSJSON()</c>/<c>LLM.RUNSCOUNT()</c> e <c>TRAIN RUNS …</c> sui
/// backend IL/PCodeVM, mantenendo la parità di output con VM/C e C99 (che usano il registro C).
/// Ring buffer di capacità <see cref="Cap"/> con ID auto-incrementante (come il C).
/// </summary>
public static class JobRegistry
{
    public const int Cap = 64;

    public enum Kind { Finetune = 0, LoraApply = 1 }
    public enum Status { Running = 0, Ok = 1, Error = 2 }

    public sealed class Record
    {
        public int Id;
        public Kind Kind;
        public Status Status;
        public double TrainLoss = -1;
        public double HoldLoss = -1;
        public double BaseTrain = -1;
        public double BaseHold = -1;
        public long GreedyBefore = -1;
        public long GreedyAfter = -1;
        public long DemoChanged;
        public string Ckpt = "";
        public string Message = "";
    }

    private static readonly List<Record> s_records = new();
    private static int s_nextId = 1;
    private static int s_lastId;

    /// <summary>Aggiunge un job; ritorna l'ID assegnato (>= 1).</summary>
    public static int Add(Kind kind, Status status, string message,
        double trainLoss, double holdLoss, double baseTrain, double baseHold,
        long greedyBefore, long greedyAfter, long demoChanged, string ckpt)
    {
        int id = s_nextId++;
        if (s_nextId <= 0) s_nextId = 1; // wrap
        var r = new Record
        {
            Id = id,
            Kind = kind,
            Status = status,
            TrainLoss = trainLoss,
            HoldLoss = holdLoss,
            BaseTrain = baseTrain,
            BaseHold = baseHold,
            GreedyBefore = greedyBefore,
            GreedyAfter = greedyAfter,
            DemoChanged = demoChanged,
            Ckpt = ckpt ?? "",
            Message = message ?? "",
        };
        if (s_records.Count < Cap) s_records.Add(r);
        else s_records[s_records.Count % Cap] = r;
        s_lastId = id;
        return id;
    }

    public static int LastId() => s_lastId;
    public static int Count() => Math.Min(s_records.Count, Cap);
    public static Record? At(int index) => index >= 0 && index < s_records.Count ? s_records[index] : null;
    /// <summary>Svuota il registro; ritorna il numero di run rimosse (TRAIN RUNS PRUNE).</summary>
    public static int Clear()
    {
        int n = s_records.Count;
        s_records.Clear();
        s_lastId = 0;
        return n;
    }

    private static string KindStr(Kind k) => k == Kind.LoraApply ? "lora_apply" : "finetune";
    private static string StatusStr(Status s) => s == Status.Ok ? "ok" : (s == Status.Running ? "running" : "error");

    /// <summary>JSON identico a <c>gmb_ai_runs_json()</c>:
    /// {"count":N,"runs":[{id,kind,status,train,hold,baseTrain,baseHold,greedyBefore,
    /// greedyAfter,demoChanged,ckpt},...]}.</summary>
    public static string Json()
    {
        int n = Count();
        var sb = new StringBuilder();
        sb.Append("{\"count\":").Append(n).Append(",\"runs\":[");
        for (int i = 0; i < n; i++)
        {
            var r = At(i);
            if (r is null) continue;
            if (i > 0) sb.Append(',');
            sb.Append("{\"id\":").Append(r.Id)
              .Append(",\"kind\":\"").Append(KindStr(r.Kind))
              .Append("\",\"status\":\"").Append(StatusStr(r.Status))
              .Append("\",\"train\":").Append(Num(r.TrainLoss))
              .Append(",\"hold\":").Append(Num(r.HoldLoss))
              .Append(",\"baseTrain\":").Append(Num(r.BaseTrain))
              .Append(",\"baseHold\":").Append(Num(r.BaseHold))
              .Append(",\"greedyBefore\":").Append(r.GreedyBefore.ToString(CultureInfo.InvariantCulture))
              .Append(",\"greedyAfter\":").Append(r.GreedyAfter.ToString(CultureInfo.InvariantCulture))
              .Append(",\"demoChanged\":").Append(r.DemoChanged.ToString(CultureInfo.InvariantCulture))
              .Append(",\"ckpt\":\"").Append(Escape(r.Ckpt)).Append("\"}");
        }
        sb.Append("]}");
        return sb.ToString();
    }

    /// <summary>TRAIN LORA LIST: JSON {"count":N,"adapters":[...]} degli adapter (ckpt) delle run
    /// LORA APPLY registrate (parità col runtime C gmb_ai_lora_list).</summary>
    public static string LoraList()
    {
        var ckpts = new List<string>();
        foreach (var r in s_records)
            if (r.Kind == Kind.LoraApply && !string.IsNullOrEmpty(r.Ckpt)) ckpts.Add(r.Ckpt);
        var sb = new StringBuilder();
        sb.Append("{\"count\":").Append(ckpts.Count).Append(",\"adapters\":[");
        for (int i = 0; i < ckpts.Count; i++) { if (i > 0) sb.Append(','); sb.Append('"').Append(Escape(ckpts[i])).Append('"'); }
        sb.Append("]}");
        return sb.ToString();
    }

    /// <summary>TRAIN STOP &lt;id&gt;: rimuove la run dal registro (true se trovata).</summary>
    public static bool Remove(int id)
    {
        int idx = s_records.FindIndex(x => x.Id == id);
        if (idx < 0) return false;
        s_records.RemoveAt(idx);
        if (s_lastId == id) s_lastId = s_records.Count > 0 ? s_records[^1].Id : 0;
        return true;
    }

    /// <summary>Ckpt della run `id` ("" se assente) — TRAIN EXPORT &lt;run&gt;, &lt;out&gt;.</summary>
    public static string CkptOf(int id)
    {
        var r = s_records.Find(x => x.Id == id);
        return r is null ? "" : r.Ckpt;
    }

    /// <summary>Ckpt dell'ultima run che ne ha uno ("" se nessuna) — TRAIN EXPORT.</summary>
    public static string LastCkpt()
    {
        for (int i = s_records.Count - 1; i >= 0; i--)
            if (!string.IsNullOrEmpty(s_records[i].Ckpt)) return s_records[i].Ckpt;
        return "";
    }

    /// <summary>TRAIN RUNS SHOW &lt;id&gt;: JSON di una run (stesso formato di un elemento di runs[]),
    /// stringa vuota se l'id non esiste.</summary>
    public static string Item(int id)
    {
        var r = s_records.Find(x => x.Id == id);
        if (r is null) return "";
        return "{\"id\":" + r.Id
            + ",\"kind\":\"" + KindStr(r.Kind)
            + "\",\"status\":\"" + StatusStr(r.Status)
            + "\",\"train\":" + Num(r.TrainLoss)
            + ",\"hold\":" + Num(r.HoldLoss)
            + ",\"baseTrain\":" + Num(r.BaseTrain)
            + ",\"baseHold\":" + Num(r.BaseHold)
            + ",\"greedyBefore\":" + r.GreedyBefore.ToString(CultureInfo.InvariantCulture)
            + ",\"greedyAfter\":" + r.GreedyAfter.ToString(CultureInfo.InvariantCulture)
            + ",\"demoChanged\":" + r.DemoChanged.ToString(CultureInfo.InvariantCulture)
            + ",\"ckpt\":\"" + Escape(r.Ckpt) + "\"}";
    }

    private static string Num(double d) => d.ToString("0.00000", CultureInfo.InvariantCulture);
    private static string Escape(string s) =>
        s.Replace("\\", "\\\\").Replace("\"", "\\\"");
}
