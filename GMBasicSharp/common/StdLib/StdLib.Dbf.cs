// FILE: Interpreter.StdLib.Dbf.cs
// ReSharper disable InconsistentNaming
// ReSharper disable CheckNamespace

/* ==== GMB-STDLIB-AUDIT-BEGIN ====
 * FILE: Dbf — namespace 'DBF'
 * 46 funzioni | 0 comandi | 0 tipi | 0 costanti
 *
 * FUNZIONI (46):
 *   ALIAS
 *   APPENDMANY
 *   APPLYREPLICATION
 *   BEGINGROUPCOMMIT
 *   BOF
 *   DBFILTER
 *   DBRELATION
 *   DELETED
 *   ENDGROUPCOMMIT
 *   EOF
 *   FIELDNAME
 *   FLOCK
 *   FLUSHDURABILITY
 *   FOLLOWREPLICATION
 *   FOUND
 *   INDEXKEY
 *   INDEXORD
 *   LASTREC
 *   LOOKUP
 *   LUPDATE
 *   NETERR
 *   RECNO
 *   RECSIZE
 *   REPLICATIONRECV
 *   REPLICATIONSHIP
 *   RLOCK
 *   SELECTED
 *   SETDURABILITY
 *   SETREPLICATION
 *   USED
    ==== GMB-STDLIB-AUDIT-END ==== */


using System.Collections.Immutable;

namespace Basic;

public sealed partial class StdLib
{
    // ========================================================================
    // --- DBF NAMESPACE ---
    // ========================================================================

    /// <summary>
    /// Crea e registra tutte le definizioni per il namespace 'DBF'.
    /// VERSIONE CORRETTA: Aggiunge le NativeProcedureDefinition per tutti i comandi e le funzioni
    /// che supportano la sintassi qualificata.
    /// </summary>
    private void CreateDbfNamespace()
    {
        const string namespaceName = "DBF";
        const SymbolFlags publicFuncFlag =
            SymbolFlags.IsPublic | SymbolFlags.IsFunction;
        const SymbolFlags noFlags = SymbolFlags.None;
        const SymbolFlags optionalFlag = SymbolFlags.IsOptional;

        // // --- REGISTRAZIONE DEL COMANDO DI NAMESPACE PER SINTASSI LEGACY ---
        // RegisterCommand(TokenKind.Dbf, "DBF", namespaceName, Parser.ParseDbfStatement,
        

        // --- I comandi DBF sono SOLO legacy (sintassi spazio: DBF OPEN, DBF SET DEFAULT).
        // Non esiste sintassi qualificata per i comandi DBF; le funzioni (che restituiscono
        // valore) restano qualificate (DBF.EOF(), DBF.RECNO(), ...) — vedi procedures qui sotto.
        // Le registrazioni qualificate per nome (registro qualificato) sono state RIMOSSE:
        // il dispatch passa da ParseDbfStatement (TokenKind).

        // --- Funzioni Native (SOLO quelle che restituiscono un valore) ---
        var procedures = new ProcedureDefinition[]
        {
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.EofFunc, "EOF"), [], new BitType(),
                TokenKind.EofFunc),

            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.Bof, "BOF"), [], new BitType(),
                TokenKind.Bof),

            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.Found, "FOUND"), [], new BitType(),
                TokenKind.Found),

            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.Recno, "RECNO"), [], new LongType(),
                TokenKind.Recno),

            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.Lastrec, "LASTREC"), [], new LongType(),
                TokenKind.Lastrec),

            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.RLock, "RLOCK"),
                [new Parameter(-1, T(TokenKind.Identifier, "area"), new InferredType(), optionalFlag, null, false)],
                new BitType(), TokenKind.RLock),

            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.FLock, "FLOCK"),
                [new Parameter(-1, T(TokenKind.Identifier, "area"), new InferredType(), optionalFlag, null, false)],
                new BitType(), TokenKind.FLock),

            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.Deleted, "DELETED"),
                [new Parameter(-1, T(TokenKind.Identifier, "area"), new InferredType(), optionalFlag, null, false)],
                new BitType(), TokenKind.Deleted),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.Alias, "ALIAS"),
                [new Parameter(-1, T(TokenKind.Identifier, "area"), new InferredType(), optionalFlag, null, false)],
                new StringType(), TokenKind.Alias),

            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.Selected, "SELECTED"), [], new LongType(),
                TokenKind.Selected),

            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.Used, "USED"),
                [new Parameter(-1, T(TokenKind.Identifier, "area"), new InferredType(), optionalFlag, null, false)],
                new BitType(), TokenKind.Used),

            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.DbFilter, "DBFILTER"),
                [new Parameter(-1, T(TokenKind.Identifier, "area"), new InferredType(), optionalFlag, null, false)],
                new StringType(), TokenKind.DbFilter),

            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.DbRelation, "DBRELATION"),
                [new Parameter(-1, T(TokenKind.Identifier, "area"), new InferredType(), optionalFlag, null, false)],
                new StringType(), TokenKind.DbRelation),

            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.IndexKey, "INDEXKEY"),
                [new Parameter(-1, T(TokenKind.Identifier, "area"), new InferredType(), optionalFlag, null, false)],
                new StringType(), TokenKind.IndexKey),

            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.IndexOrd, "INDEXORD"),
                [new Parameter(-1, T(TokenKind.Identifier, "area"), new InferredType(), optionalFlag, null, false)],
                new LongType(), TokenKind.IndexOrd),

            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.FieldName, "FIELDNAME"),
                [new Parameter(-1, T(TokenKind.Identifier, "fieldNum"), new LongType(), noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "area"), new InferredType(), optionalFlag, null, false)],
                new StringType(), TokenKind.FieldName),

            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.LUpdate, "LUPDATE"), [], new StringType(),
                TokenKind.LUpdate),

            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.RecSize, "RECSIZE"),
                [new Parameter(-1, T(TokenKind.Identifier, "area"), new InferredType(), optionalFlag, null, false)],
                new LongType(), TokenKind.RecSize),

            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.NetErr, "NETERR"), [], new BitType(),
                TokenKind.NetErr),

            // <summary>
            // DEFINIZIONE DELLA FUNZIONE NATIVA: DBF.LOOKUP
            // Cerca un valore in una tabella DBF (work area) diversa da quella corrente e restituisce il valore di un campo corrispondente.
            // L'operazione è atomica dal punto di vista dell'utente: la work area originale viene ripristinata automaticamente.
            // </summary>
            // <param name="returnField">Espressione STRINGA: il nome del campo da cui estrarre il valore nella tabella di ricerca.</param>
            // <param name="searchExpr">Espressione: il valore da cercare. Deve essere compatibile con il tipo del campo di ricerca.</param>
            // <param name="searchField">Espressione STRINGA: il nome del campo indicizzato su cui eseguire il SEEK nella tabella di ricerca.</param>
            // <param name="searchAlias">Espressione STRINGA o numerica: l'alias o il numero della work area in cui cercare.</param>
            // <returns>
            // Il valore del campo 'returnField' se la ricerca ha successo; altrimenti, NIL.
            // Il tipo di ritorno è InferredType perché dipende dal tipo del campo 'returnField', che è noto solo a runtime.
            // </returns>
            // <remarks>
            // LOGICA PER L'EXECUTOR:
            // L'implementazione di questa funzione deve essere una macchina a stati non bloccante:
            // 1. Salvare la work area corrente.
            // 2. Selezionare la work area specificata da 'searchAlias'.
            // 3. Eseguire un'operazione SEEK utilizzando 'searchExpr' sul campo (o indice) 'searchField'.
            // 4. Se il record viene trovato (FOUND() = TRUE):
            //    a. Leggere il valore dal campo 'returnField'.
            //    b. Salvare questo valore come risultato.
            // 5. Se il record non viene trovato:
            //    a. Impostare il risultato a NIL.
            // 6. In ogni caso, ripristinare la work area originale salvata al punto 1.
            // 7. Restituire il valore salvato.
            // </remarks>
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.DbfLookup, "LOOKUP"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "returnField"), new StringType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "searchExpr"), new InferredType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "searchField"), new StringType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "searchAlias"), new InferredType(), noFlags, null, false)
            ], new InferredType(), TokenKind.DbfLookup),

            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.DbfAppendMany, "APPENDMANY"),
                [new Parameter(-1, T(TokenKind.Identifier, "records"), new InferredType(), noFlags, null, false)],
                new LongType(), TokenKind.DbfAppendMany),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.DbfAppendMany, "APPENDMANY"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "fileNumber"), new LongType(), noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "records"), new InferredType(), noFlags, null, false)
                ],
                new LongType(), TokenKind.DbfAppendMany),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.DbfAppendMany, "APPENDMANY"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "alias"), new StringType(), noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "records"), new InferredType(), noFlags, null, false)
                ],
                new LongType(), TokenKind.DbfAppendMany),

            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.DbfSetDurability, "SETDURABILITY"),
                [new Parameter(-1, T(TokenKind.Identifier, "mode"), new StringType(), noFlags, null, false)],
                new BitType(), TokenKind.DbfSetDurability),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.DbfSetDurability, "SETDURABILITY"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "fileNumber"), new LongType(), noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "mode"), new StringType(), noFlags, null, false)
                ],
                new BitType(), TokenKind.DbfSetDurability),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.DbfSetDurability, "SETDURABILITY"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "alias"), new StringType(), noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "mode"), new StringType(), noFlags, null, false)
                ],
                new BitType(), TokenKind.DbfSetDurability),

            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.DbfFlushDurability, "FLUSHDURABILITY"), [],
                new BitType(), TokenKind.DbfFlushDurability),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.DbfFlushDurability, "FLUSHDURABILITY"),
                [new Parameter(-1, T(TokenKind.Identifier, "fileNumber"), new LongType(), optionalFlag, null, false)],
                new BitType(), TokenKind.DbfFlushDurability),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.DbfFlushDurability, "FLUSHDURABILITY"),
                [new Parameter(-1, T(TokenKind.Identifier, "alias"), new StringType(), optionalFlag, null, false)],
                new BitType(), TokenKind.DbfFlushDurability),

            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.DbfBeginGroupCommit, "BEGINGROUPCOMMIT"), [],
                new BitType(), TokenKind.DbfBeginGroupCommit),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.DbfBeginGroupCommit, "BEGINGROUPCOMMIT"),
                [new Parameter(-1, T(TokenKind.Identifier, "fileNumber"), new LongType(), optionalFlag, null, false)],
                new BitType(), TokenKind.DbfBeginGroupCommit),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.DbfBeginGroupCommit, "BEGINGROUPCOMMIT"),
                [new Parameter(-1, T(TokenKind.Identifier, "alias"), new StringType(), optionalFlag, null, false)],
                new BitType(), TokenKind.DbfBeginGroupCommit),

            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.DbfEndGroupCommit, "ENDGROUPCOMMIT"), [],
                new BitType(), TokenKind.DbfEndGroupCommit),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.DbfEndGroupCommit, "ENDGROUPCOMMIT"),
                [new Parameter(-1, T(TokenKind.Identifier, "fileNumber"), new LongType(), optionalFlag, null, false)],
                new BitType(), TokenKind.DbfEndGroupCommit),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.DbfEndGroupCommit, "ENDGROUPCOMMIT"),
                [new Parameter(-1, T(TokenKind.Identifier, "alias"), new StringType(), optionalFlag, null, false)],
                new BitType(), TokenKind.DbfEndGroupCommit),

            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.DbfSetReplication, "SETREPLICATION"),
                [new Parameter(-1, T(TokenKind.Identifier, "logPath"), new StringType(), noFlags, null, false)],
                new StringType(), TokenKind.DbfSetReplication),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.DbfSetReplication, "SETREPLICATION"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "fileNumber"), new LongType(), noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "logPath"), new StringType(), noFlags, null, false)
                ],
                new StringType(), TokenKind.DbfSetReplication),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.DbfSetReplication, "SETREPLICATION"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "alias"), new StringType(), noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "logPath"), new StringType(), noFlags, null, false)
                ],
                new StringType(), TokenKind.DbfSetReplication),

            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.DbfApplyReplication, "APPLYREPLICATION"),
                [new Parameter(-1, T(TokenKind.Identifier, "logPath"), new StringType(), noFlags, null, false)],
                new LongType(), TokenKind.DbfApplyReplication),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.DbfApplyReplication, "APPLYREPLICATION"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "fileNumber"), new LongType(), noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "logPath"), new StringType(), noFlags, null, false)
                ],
                new LongType(), TokenKind.DbfApplyReplication),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.DbfFollowReplication, "FOLLOWREPLICATION"),
                [], new LongType(), TokenKind.DbfFollowReplication),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.DbfFollowReplication, "FOLLOWREPLICATION"),
                [new Parameter(-1, T(TokenKind.Identifier, "logPath"), new StringType(), noFlags, null, false)],
                new LongType(), TokenKind.DbfFollowReplication),

            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.DbfReplicationShipTcp, "REPLICATIONSHIP"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "host"), new StringType(), noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "port"), new LongType(), noFlags, null, false)
                ],
                new LongType(), TokenKind.DbfReplicationShipTcp),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.DbfReplicationShipTcp, "REPLICATIONSHIP"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "fileNumber"), new LongType(), noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "host"), new StringType(), noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "port"), new LongType(), noFlags, null, false)
                ],
                new LongType(), TokenKind.DbfReplicationShipTcp),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.DbfReplicationRecvTcp, "REPLICATIONRECV"),
                [new Parameter(-1, T(TokenKind.Identifier, "port"), new LongType(), noFlags, null, false)],
                new LongType(), TokenKind.DbfReplicationRecvTcp),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.DbfReplicationRecvTcp, "REPLICATIONRECV"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "fileNumber"), new LongType(), noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "port"), new LongType(), noFlags, null, false)
                ],
                new LongType(), TokenKind.DbfReplicationRecvTcp)
        };

        RegisterBuiltinNamespace(namespaceName, procedures, ImmutableDictionary<string, DataType>.Empty);
    }
}