// FILE: Interpreter.StdLib.Collections.Methods.cs
// ReSharper disable InconsistentNaming
// ReSharper disable CheckNamespace

/* ==== GMB-STDLIB-AUDIT-BEGIN ====
 * FILE: Collections.Methods — namespace '—'
 * 0 funzioni | 0 comandi | 0 tipi | 0 costanti
 *
    ==== GMB-STDLIB-AUDIT-END ==== */

using System.Collections.Immutable;

namespace Basic;

public sealed partial class StdLib
{
    private void CreateCollectionsMethodsNamespace()
    {
        // I metodi di collection restano MEMBRI DI TIPO (MethodCallExpr su istanza) — il
        // namespace COLLECTIONS espone i tipi (StdLib.Collections.cs), non le funzioni.
        // I TokenKind Coll* (TokenKind.cs coda) sono la chiave di dispatch nei 4 backend
        // (compile-time IL/PCode, runtime VM/C/C99/C99Native), NON funzioni globali.
        //
        // Il mapping TokenKind → nome metodo runtime è in Compiler.Expressions.Functions.cs
        // (CollCount → "Count") così il runtime esistente continua a funzionare mentre i
        // backend migrano da lexeme a TokenKind.
        //
        // Questo file NON aggiunge procedure builtin: il dispatch avviene tramite il schema
        // dei tipi (RegisterTypeSchema) già registrato in CreateCollectionsNamespace, che
        // rende i metodi risolvibili dal linker come membri.
        //
        // Quando i 4 backend saranno migrati (F1d), il mapping lexeme stringa si ridurrà a
        // compatibilità legacy e il dispatch primario sarà per TokenKind.
    }
}
