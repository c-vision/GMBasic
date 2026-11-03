// FILE: StdLib.Env.cs
// Namespace ENV — lettura ESPLICITA dell'ambiente di processo (sola lettura).
// Regola AGENTS: il MOTORE non usa env per parametri/comportamento (registro opzioni);
// questa è una API di linguaggio che il programma utente chiama deliberatamente.
using System.Collections.Immutable;

namespace Basic;

public sealed partial class StdLib
{
    private void CreateEnvNamespace()
    {
        const string ns = "ENV";
        BuiltinNamespaceNames.Add(ns);
        SymbolTable.BuiltinNamespaceNames.Add(ns);

        const SymbolFlags publicFlag = SymbolFlags.IsPublic | SymbolFlags.IsFunction;
        var procs = new System.Collections.Generic.List<ProcedureDefinition>
        {
            // ENV$("NOME") — valore della variabile d'ambiente, "" se assente.
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.EnvGet, "ENV"),
                [new Parameter(-1, T(TokenKind.Identifier, "name"), new StringType(), 0, null, false)],
                new StringType(), TokenKind.EnvGet),
            // ENV.HAS("NOME") — -1 se presente, 0 se assente (convenzione booleana BASIC).
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.EnvHas, "HAS"),
                [new Parameter(-1, T(TokenKind.Identifier, "name"), new StringType(), 0, null, false)],
                new BitType(), TokenKind.EnvHas),
        };

        RegisterBuiltinNamespace(ns, procs, ImmutableDictionary<string, DataType>.Empty);
    }
}
