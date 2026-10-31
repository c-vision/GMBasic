// FILE: Interpreter.StdLib.Isam.cs
// ReSharper disable InconsistentNaming
// ReSharper disable CheckNamespace

/* ==== GMB-STDLIB-AUDIT-BEGIN ====
 * FILE: Isam — namespace 'ISAM'
 * 41 funzioni | 0 comandi | 0 tipi | 0 costanti
 *
 * FUNZIONI (41):
 *   APPENDMANY
 *   APPLYREPLICATION
 *   BOF
 *   DBFILTER
 *   DBRELATION
 *   DELETED
 *   EOF
 *   FLOCK
 *   FOLLOWREPLICATION
 *   FOUND
 *   INDEXKEY
 *   INDEXORD
 *   INFO
 *   INTEGRITY
 *   LASTREC
 *   LOG
 *   RECNO
 *   RECSIZE
 *   REPLICATIONRECV
 *   REPLICATIONSHIP
 *   RLOCK
 *   SELECTED
 *   SETREPLICATION
 *   USED
    ==== GMB-STDLIB-AUDIT-END ==== */


using System.Collections.Immutable;

namespace Basic;

public sealed partial class StdLib
{
    // ========================================================================
    // --- ISAM NAMESPACE ---
    // ========================================================================

    /// <summary>
    /// Crea e registra tutte le definizioni per il namespace 'ISAM'.
    /// VERSIONE CORRETTA E COMPLETA: Definisce ogni operazione sia come Comando (per la sintassi legacy)
    /// sia come Funzione (per la sintassi qualificata), risolvendo i `FunctionNotFound`.
    /// </summary>
    private void CreateIsamNamespace()
    {
        const string namespaceName = "ISAM";
        const SymbolFlags publicFuncFlag = SymbolFlags.IsPublic | SymbolFlags.IsFunction;
        const SymbolFlags optionalFlag = SymbolFlags.IsOptional;

        // --- REGISTRAZIONE DEL COMANDO DI NAMESPACE PER SINTASSI LEGACY ---
        // RegisterCommand(TokenKind.Isam, "ISAM", namespaceName,
        //     Parser.ParseIsamStatement,
        

        // --- I comandi ISAM sono SOLO legacy (sintassi spazio: ISAM OPEN, ISAM SET DEFAULT TO).
        // Non esiste sintassi qualificata per i comandi ISAM; le funzioni (che restituiscono
        // valore) restano qualificate (ISAM.EOF(), ISAM.RECNO(), ...) — vedi procedures qui sotto.
        // Le registrazioni qualificate per nome (registro qualificato) sono state RIMOSSE:
        // il dispatch passa da ParseIsamStatement (TokenKind).
        
        // --- Funzioni Native (per la sintassi qualificata che restituisce un valore) ---
        var procedures = new ProcedureDefinition[]
        {
            // Funzioni di stato che restituiscono un valore
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.EofFunc, "EOF"),
                [new Parameter(-1, T(TokenKind.Identifier, "fileNumber"), new LongType(), optionalFlag, null, false)],
                new BitType(), TokenKind.EofFunc),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.Bof, "BOF"),
                [new Parameter(-1, T(TokenKind.Identifier, "fileNumber"), new LongType(), optionalFlag, null, false)],
                new BitType(), TokenKind.Bof),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.Found, "FOUND"),
                [new Parameter(-1, T(TokenKind.Identifier, "fileNumber"), new LongType(), optionalFlag, null, false)],
                new BitType(), TokenKind.Found),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.Recno, "RECNO"),
                [new Parameter(-1, T(TokenKind.Identifier, "fileNumber"), new LongType(), optionalFlag, null, false)],
                new LongType(), TokenKind.Recno),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.RecSize, "RECSIZE"),
                [new Parameter(-1, T(TokenKind.Identifier, "fileNumber"), new LongType(), optionalFlag, null, false)],
                new LongType(), TokenKind.RecSize),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.Lastrec, "LASTREC"),
                [new Parameter(-1, T(TokenKind.Identifier, "fileNumber"), new LongType(), optionalFlag, null, false)],
                new LongType(), TokenKind.Lastrec),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.RLock, "RLOCK"),
                [new Parameter(-1, T(TokenKind.Identifier, "fileNumber"), new LongType(), optionalFlag, null, false)],
                new BitType(), TokenKind.RLock),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.FLock, "FLOCK"),
                [new Parameter(-1, T(TokenKind.Identifier, "fileNumber"), new LongType(), optionalFlag, null, false)],
                new BitType(), TokenKind.FLock),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.Used, "USED"),
                [new Parameter(-1, T(TokenKind.Identifier, "alias"), new StringType(), optionalFlag, null, false)],
                new BitType(), TokenKind.Used),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.Selected, "SELECTED"),
                [new Parameter(-1, T(TokenKind.Identifier, "area"), new InferredType(), optionalFlag, null, false)],
                new LongType(), TokenKind.Selected),

            // Overload per le funzioni che accettano sia handle che alias
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.DbFilter, "DBFILTER"),
                [new Parameter(-1, T(TokenKind.Identifier, "area"), new LongType(), optionalFlag, null, false)],
                new StringType(), TokenKind.DbFilter),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.DbFilter, "DBFILTER"),
                [new Parameter(-1, T(TokenKind.Identifier, "area"), new StringType(), optionalFlag, null, false)],
                new StringType(), TokenKind.DbFilter),

            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.DbRelation, "DBRELATION"),
                [new Parameter(-1, T(TokenKind.Identifier, "area"), new LongType(), optionalFlag, null, false)],
                new StringType(), TokenKind.DbRelation),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.DbRelation, "DBRELATION"),
                [new Parameter(-1, T(TokenKind.Identifier, "area"), new StringType(), optionalFlag, null, false)],
                new StringType(), TokenKind.DbRelation),

            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.IndexKey, "INDEXKEY"),
                [new Parameter(-1, T(TokenKind.Identifier, "area"), new LongType(), optionalFlag, null, false)],
                new StringType(), TokenKind.IndexKey),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.IndexKey, "INDEXKEY"),
                [new Parameter(-1, T(TokenKind.Identifier, "area"), new StringType(), optionalFlag, null, false)],
                new StringType(), TokenKind.IndexKey),

            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.IndexOrd, "INDEXORD"),
                [new Parameter(-1, T(TokenKind.Identifier, "area"), new LongType(), optionalFlag, null, false)],
                new LongType(), TokenKind.IndexOrd),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.IndexOrd, "INDEXORD"),
                [new Parameter(-1, T(TokenKind.Identifier, "area"), new StringType(), optionalFlag, null, false)],
                new LongType(), TokenKind.IndexOrd),

            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.Deleted, "DELETED"),
                [new Parameter(-1, T(TokenKind.Identifier, "area"), new LongType(), optionalFlag, null, false)],
                new BitType(), TokenKind.Deleted),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.Deleted, "DELETED"),
                [new Parameter(-1, T(TokenKind.Identifier, "area"), new StringType(), optionalFlag, null, false)],
                new BitType(), TokenKind.Deleted),

            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.IsamIntegrity, "INTEGRITY"),
                [new Parameter(-1, T(TokenKind.Identifier, "fileNumber"), new LongType(), optionalFlag, null, false)],
                new JsonType(), TokenKind.IsamIntegrity),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.IsamIntegrity, "INTEGRITY"),
                [new Parameter(-1, T(TokenKind.Identifier, "alias"), new StringType(), optionalFlag, null, false)],
                new JsonType(), TokenKind.IsamIntegrity),

            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.IsamInfo, "INFO"),
                [new Parameter(-1, T(TokenKind.Identifier, "fileNumber"), new LongType(), optionalFlag, null, false)],
                new JsonType(), TokenKind.IsamInfo),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.IsamInfo, "INFO"),
                [new Parameter(-1, T(TokenKind.Identifier, "alias"), new StringType(), optionalFlag, null, false)],
                new JsonType(), TokenKind.IsamInfo),

            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.IsamLog, "LOG"),
                [new Parameter(-1, T(TokenKind.Identifier, "fileNumber"), new LongType(), optionalFlag, null, false)],
                new StringType(), TokenKind.IsamLog),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.IsamLog, "LOG"),
                [new Parameter(-1, T(TokenKind.Identifier, "alias"), new StringType(), optionalFlag, null, false)],
                new StringType(), TokenKind.IsamLog),

            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.IsamAppendMany, "APPENDMANY"),
                [new Parameter(-1, T(TokenKind.Identifier, "records"), new InferredType(), SymbolFlags.None, null, false)],
                new LongType(), TokenKind.IsamAppendMany),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.IsamAppendMany, "APPENDMANY"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "fileNumber"), new LongType(), SymbolFlags.None, null,
                        false),
                    new Parameter(-1, T(TokenKind.Identifier, "records"), new InferredType(), SymbolFlags.None, null,
                        false)
                ],
                new LongType(), TokenKind.IsamAppendMany),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.IsamAppendMany, "APPENDMANY"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "alias"), new StringType(), SymbolFlags.None, null,
                        false),
                    new Parameter(-1, T(TokenKind.Identifier, "records"), new InferredType(), SymbolFlags.None, null,
                        false)
                ],
                new LongType(), TokenKind.IsamAppendMany),

            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.IsamSetReplication, "SETREPLICATION"),
                [new Parameter(-1, T(TokenKind.Identifier, "logPath"), new StringType(), SymbolFlags.None, null, false)],
                new StringType(), TokenKind.IsamSetReplication),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.IsamSetReplication, "SETREPLICATION"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "fileNumber"), new LongType(), SymbolFlags.None, null,
                        false),
                    new Parameter(-1, T(TokenKind.Identifier, "logPath"), new StringType(), SymbolFlags.None, null,
                        false)
                ],
                new StringType(), TokenKind.IsamSetReplication),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.IsamSetReplication, "SETREPLICATION"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "alias"), new StringType(), SymbolFlags.None, null,
                        false),
                    new Parameter(-1, T(TokenKind.Identifier, "logPath"), new StringType(), SymbolFlags.None, null,
                        false)
                ],
                new StringType(), TokenKind.IsamSetReplication),

            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.IsamApplyReplication, "APPLYREPLICATION"),
                [new Parameter(-1, T(TokenKind.Identifier, "logPath"), new StringType(), SymbolFlags.None, null, false)],
                new LongType(), TokenKind.IsamApplyReplication),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.IsamApplyReplication, "APPLYREPLICATION"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "fileNumber"), new LongType(), SymbolFlags.None, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "logPath"), new StringType(), SymbolFlags.None, null, false)
                ],
                new LongType(), TokenKind.IsamApplyReplication),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.IsamApplyReplication, "APPLYREPLICATION"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "alias"), new StringType(), SymbolFlags.None, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "logPath"), new StringType(), SymbolFlags.None, null, false)
                ],
                new LongType(), TokenKind.IsamApplyReplication),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.IsamFollowReplication, "FOLLOWREPLICATION"),
                [], new LongType(), TokenKind.IsamFollowReplication),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.IsamFollowReplication, "FOLLOWREPLICATION"),
                [new Parameter(-1, T(TokenKind.Identifier, "logPath"), new StringType(), SymbolFlags.None, null, false)],
                new LongType(), TokenKind.IsamFollowReplication),

            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.IsamReplicationShipTcp, "REPLICATIONSHIP"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "host"), new StringType(), SymbolFlags.None, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "port"), new LongType(), SymbolFlags.None, null, false)
                ],
                new LongType(), TokenKind.IsamReplicationShipTcp),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.IsamReplicationShipTcp, "REPLICATIONSHIP"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "fileNumber"), new LongType(), SymbolFlags.None, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "host"), new StringType(), SymbolFlags.None, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "port"), new LongType(), SymbolFlags.None, null, false)
                ],
                new LongType(), TokenKind.IsamReplicationShipTcp),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.IsamReplicationRecvTcp, "REPLICATIONRECV"),
                [new Parameter(-1, T(TokenKind.Identifier, "port"), new LongType(), SymbolFlags.None, null, false)],
                new LongType(), TokenKind.IsamReplicationRecvTcp),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.IsamReplicationRecvTcp, "REPLICATIONRECV"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "fileNumber"), new LongType(), SymbolFlags.None, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "port"), new LongType(), SymbolFlags.None, null, false)
                ],
                new LongType(), TokenKind.IsamReplicationRecvTcp)
        };

        RegisterBuiltinNamespace(namespaceName, procedures, ImmutableDictionary<string, DataType>.Empty);
    }
}