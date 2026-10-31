// Diagnostics condivisi per le chiamate non risolte (UnresolvedCallExpr).
// Un vero compilatore distingue i diagnostici: "simbolo non trovato" (nessun candidato)
// da "simbolo ambiguo" (piu' candidati ugualmente validi, con suggerimento di qualifica).
// La risoluzione C# produce gia' UnresolvedReason; qui i diversi esiti diventano messaggi
// distinti e chiari (mai un errore generico), condivisi da bytecode-compiler e codegen IL.
using System.Collections.Generic;
using System.Linq;

namespace Basic;

public static class UnresolvedCallDiagnostics
{
    /// <summary>Messaggio d'errore distinto per esito: non trovata / namespace non importato /
    /// ambigua (builtin, utente, builtin+utente). Elenca i candidati dove utili.</summary>
    public static string Build(UnresolvedCallExpr uce, IEnumerable<string>? candidateNamespaces = null)
    {
        var name = uce.Name.Lexeme;
        var n = uce.Candidates.Length;
        string Hints() =>
            candidateNamespaces is null ? "" : " Candidati: " + string.Join(", ", candidateNamespaces) + ".";
        switch (uce.Reason)
        {
            case UnresolvedReason.AmbiguousBuiltinFunctions:
                return
                    $"Chiamata ambigua: '{name}' corrisponde a più funzioni di libreria di namespace diversi ({n} candidati compatibili)." +
                    Hints() + " Qualifica con NS.NOME (es. <namespace>." + name + "(...)) o limita gli IMPORT.";
            case UnresolvedReason.AmbiguousUserFunctions:
                return
                    $"Chiamata ambigua: '{name}' corrisponde a più procedure utente in scope diversi ({n} candidati)." +
                    Hints() + " Qualifica o rinomina le procedure.";
            case UnresolvedReason.AmbiguousUserAndBuiltinFunction:
                return
                    $"Chiamata ambigua: '{name}' corrisponde sia a una funzione di libreria sia a una procedura utente." +
                    Hints() + " Qualifica con NS.NOME o rinomina la procedura.";
            case UnresolvedReason.BuiltinNamespaceNotImported:
                return
                    $"Simbolo non trovato: '{name}' è una funzione di libreria in un namespace non importato. " +
                    "Usa la forma qualificata <NS>." + name + "(...) o aggiungi IMPORT <NS>.";
            default: // FunctionNotFound
                return
                    $"Simbolo non trovato: funzione o subroutine non definita: '{name}' (chiamata non risolta dal linker). " +
                    "Verifica l'ortografia, la dichiarazione della procedura o l'IMPORT del namespace.";
        }
    }
}
