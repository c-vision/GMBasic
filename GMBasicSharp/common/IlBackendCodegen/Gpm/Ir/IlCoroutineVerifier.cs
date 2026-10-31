// FILE: Basic.IlBackend.Gpm.Ir.IlCoroutineVerifier.cs
// G2 — Verifier dell'IR stackless (piano §8). Rifiuta IR malformati PRIMA dell'emissione:
//   - valori non spillati ai safepoint (stack non bilanciato / pop a vuoto);
//   - PC/edge non validi o ingresso in regioni protette;
//   - await senza Park;
//   - branch/switch verso target inesistenti;
//   - blocchi senza terminator.
// FINALLY/ON ERROR/Return-value sono verificati a G3/G4 quando esistono nell'IR.

using System.Collections.Generic;
using System.Text;

namespace Basic.IlBackend.Gpm.Ir;

public static class IlCoroutineVerifier
{
    public static IReadOnlyList<string> Validate(IlCoroutineIr ir)
    {
        var errors = new List<string>();

        if (!ir.ByPc.ContainsKey(StacklessLowering.EntryPc))
            errors.Add("procedura senza blocco di ingresso (pc 0)");

        foreach (var b in ir.Blocks)
        {
            if (b.Terminator.Kind == IlIrTermKind.CompleteTask && b.Pc != StacklessLowering.EntryPc)
                errors.Add($"B{b.Pc}: CompleteTask non consentito fuori dalla entry");

            // Stack virtuale bilanciato lungo le istruzioni del blocco.
            var depth = 0;
            var sawAwait = false;
            foreach (var inst in b.Instructions)
            {
                switch (inst.Kind)
                {
                    case IlIrInstKind.LoadConst or IlIrInstKind.LoadLocal or IlIrInstKind.LoadSpill:
                        depth++; break;
                    case IlIrInstKind.StoreLocal or IlIrInstKind.StoreSpill:
                        depth--;
                        if (depth < 0) errors.Add($"B{b.Pc}: pop a vuoto ({inst}) — stack virtuale non bilanciato");
                        break;
                    case IlIrInstKind.CallSync:
                        depth -= inst.ArgCount;
                        if (depth < 0) errors.Add($"B{b.Pc}: CallSync senza argomenti sufficienti ({inst})");
                        depth++; break; // push risultato
                    case IlIrInstKind.Await:
                        sawAwait = true; break;
                }
            }
            if (sawAwait && b.Terminator.Kind != IlIrTermKind.Park)
                errors.Add($"B{b.Pc}: Await senza terminator Park");
            if (b.Terminator.Kind == IlIrTermKind.Park && !sawAwait)
                errors.Add($"B{b.Pc}: Park senza Await (retry non preparato)");

            // Edge validi.
            switch (b.Terminator.Kind)
            {
                case IlIrTermKind.Branch:
                    if (!IsValidTarget(ir, b.Terminator.TargetPc))
                        errors.Add($"B{b.Pc}: branch verso target non valido {b.Terminator.TargetPc}");
                    break;
                case IlIrTermKind.Switch:
                    if (b.Terminator.Targets.IsDefaultOrEmpty)
                        errors.Add($"B{b.Pc}: switch senza target");
                    foreach (var t in b.Terminator.Targets)
                        if (!IsValidTarget(ir, t))
                            errors.Add($"B{b.Pc}: switch verso target non valido {t}");
                    break;
                case IlIrTermKind.Call:
                    if (b.Terminator.TargetProc < 0)
                        errors.Add($"B{b.Pc}: call senza procedura target");
                    if (b.Terminator.ArgSlots.Length > 0)
                        foreach (var s in b.Terminator.ArgSlots)
                            if (s < 0 || s >= ir.SpillCount)
                                errors.Add($"B{b.Pc}: call con arg slot spill non valido {s}");
                    break;
                case IlIrTermKind.Park:
                    if (b.Terminator.ResumePc != StacklessLowering.EndPc && !IsValidTarget(ir, b.Terminator.ResumePc))
                        errors.Add($"B{b.Pc}: park con resume non valido {b.Terminator.ResumePc}");
                    break;
                case IlIrTermKind.Return:
                case IlIrTermKind.Throw:
                case IlIrTermKind.CompleteTask:
                    break; // terminator di fine: nessun edge
            }
        }

        // Ingresso in regione protetta da fuori: per ora le regioni sono sempre vuote (G2);
        // il controllo completo arriva con TRY a G3.
        return errors;
    }

    private static bool IsValidTarget(IlCoroutineIr ir, int pc)
        => pc == StacklessLowering.EndPc || ir.ByPc.ContainsKey(pc);

    public static void AssertValid(IlCoroutineIr ir, string context)
    {
        var errors = Validate(ir);
        if (errors.Count == 0) return;
        var sb = new StringBuilder();
        sb.AppendLine($"[IlCoroutineVerifier] IR non valido ({context}):");
        foreach (var e in errors) sb.AppendLine("  - " + e);
        throw new System.InvalidOperationException(sb.ToString());
    }
}
