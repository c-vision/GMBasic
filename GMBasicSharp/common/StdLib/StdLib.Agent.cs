// StdLib.Agent.cs — AGENT namespace: loop Brain→Azione (CUA)
// AGENT.RUN goal [, max_steps] → String (log del loop). Implementazione reale nel
// runtime C (gmb_agent_loop_run/gmb_agent_run_goal, VM/C builtin 1387).

/* ==== GMB-STDLIB-AUDIT-BEGIN ====
 * FILE: Agent — namespace 'AGENT'
 * 1 funzioni | 0 comandi | 0 tipi | 0 costanti
 *
 * FUNZIONI (1):
 *   Run
    ==== GMB-STDLIB-AUDIT-END ==== */

using System.Collections.Immutable;

namespace Basic;

public sealed partial class StdLib
{
    private void CreateAgentNamespace()
    {
        const string ns = "AGENT";
        var pub = SymbolFlags.IsPublic;
        var stepsP = new Parameter(-1, T(TokenKind.Identifier, "max_steps"), new DoubleType(),
            SymbolFlags.IsOptional, null, true);
        var procedures = new ProcedureDefinition[]
        {
            new NativeProcedureDefinition(-1, pub, T(TokenKind.AgentRun, "Run"), [P("goal", false), stepsP], new StringType(), TokenKind.AgentRun, ExecutionKind: NativeExecutionKind.Pure),
        };
        RegisterBuiltinNamespace(ns, procedures, ImmutableDictionary<string, DataType>.Empty);
    }
}
