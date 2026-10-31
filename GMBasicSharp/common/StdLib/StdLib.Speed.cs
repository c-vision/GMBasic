// FILE: Interpreter.StdLib.Speed.cs
// ReSharper disable InconsistentNaming
// ReSharper disable CheckNamespace

/* ==== GMB-STDLIB-AUDIT-BEGIN ====
 * FILE: Speed — namespace 'SPEED'
 * 18 funzioni | 0 comandi | 0 tipi | 0 costanti
 *
 * FUNZIONI (18):
 *   BEGINGROUPCOMMIT
 *   ENDGROUPCOMMIT
 *   EOF
 *   FLUSHDURABILITY
 *   INFO
 *   SELECTED
 *   SETDURABILITY
 *   SETMANY
 *   VALIDATE
    ==== GMB-STDLIB-AUDIT-END ==== */


using System.Collections.Immutable;

namespace Basic;

public sealed partial class StdLib
{
    // ========================================================================
    // --- SPEED NAMESPACE ---
    // ========================================================================

    /// <summary>
    /// Crea e registra tutte le definizioni per il namespace 'SPEED'.
    /// </summary>
    private void CreateSpeedNamespace()
    {
        const string namespaceName = "SPEED";
        const SymbolFlags publicFuncFlag =
            SymbolFlags.IsPublic | SymbolFlags.IsFunction;

        // --- I comandi SPEED sono SOLO legacy (sintassi spazio: SPEED OPEN, SPEED SET ORDER).
        // Non esiste sintassi qualificata per i comandi SPEED; le funzioni (che restituiscono
        // valore) restano qualificate (SPEED.SELECTED(), SPEED.INFO(), ...) — vedi procedures qui sotto.
        // Le registrazioni qualificate per nome (registro qualificato) sono state RIMOSSE:
        // il dispatch passa da ParseSpeedStatement (TokenKind).

        const SymbolFlags optionalFlag = SymbolFlags.IsOptional;

        var procedures = new ProcedureDefinition[]
        {
            // EOF condiviso (IO/DBF/ISAM/RDD/SPEED): registrato anche qui come in
            // StdLib.Dbf.cs — senza questa riga il C99Native non risolve SPEED.EOF
            // (ns_lookup senza riga ("SPEED","EOF") → fallback su Eof nudo → gmb_file_eof).
            // Fix 2026-09-22 (todo MiniTest-Oracolo §0.47): eof.bas dava eof-top: si su C99Native.
            // NOTA arity: a differenza di DBF.EOF (area corrente, zero arg), SPEED.EOF
            // prende l'alias (SPEED.EOF(T)) → parametro opzionale come RLOCK/FLOCK,
            // altrimenti la risoluzione C# rigetta la chiamata a 1 arg (visto live 09:39
            // con build mista: "overload non validi per 'EOF'").
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.EofFunc, "EOF"),
                [new Parameter(-1, T(TokenKind.Identifier, "area"), new InferredType(), optionalFlag, null, false)],
                new BitType(), TokenKind.EofFunc),

            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.Selected, "SELECTED"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "query"), new StringType(), SymbolFlags.None,
                        null, false)
                ],
                new InferredType(),
                TokenKind.Selected),

            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.SpeedSetDurability, "SETDURABILITY"),
                [new Parameter(-1, T(TokenKind.Identifier, "mode"), new StringType(), SymbolFlags.None, null, false)],
                new StringType(), TokenKind.SpeedSetDurability),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.SpeedSetDurability, "SETDURABILITY"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "dbAlias"), new StringType(), SymbolFlags.None, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "mode"), new StringType(), SymbolFlags.None, null, false)
                ],
                new StringType(), TokenKind.SpeedSetDurability),

            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.SpeedFlushDurability, "FLUSHDURABILITY"), [],
                new BitType(), TokenKind.SpeedFlushDurability),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.SpeedFlushDurability, "FLUSHDURABILITY"),
                [new Parameter(-1, T(TokenKind.Identifier, "dbAlias"), new StringType(), optionalFlag, null, false)],
                new BitType(), TokenKind.SpeedFlushDurability),

            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.SpeedBeginGroupCommit, "BEGINGROUPCOMMIT"), [],
                new BitType(), TokenKind.SpeedBeginGroupCommit),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.SpeedBeginGroupCommit, "BEGINGROUPCOMMIT"),
                [new Parameter(-1, T(TokenKind.Identifier, "dbAlias"), new StringType(), optionalFlag, null, false)],
                new BitType(), TokenKind.SpeedBeginGroupCommit),

            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.SpeedEndGroupCommit, "ENDGROUPCOMMIT"), [],
                new BitType(), TokenKind.SpeedEndGroupCommit),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.SpeedEndGroupCommit, "ENDGROUPCOMMIT"),
                [new Parameter(-1, T(TokenKind.Identifier, "dbAlias"), new StringType(), optionalFlag, null, false)],
                new BitType(), TokenKind.SpeedEndGroupCommit),

            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.SpeedSetMany, "SETMANY"),
                [new Parameter(-1, T(TokenKind.Identifier, "records"), new InferredType(), SymbolFlags.None, null, false)],
                new LongType(), TokenKind.SpeedSetMany),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.SpeedSetMany, "SETMANY"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "area"), new StringType(), SymbolFlags.None, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "records"), new InferredType(), SymbolFlags.None, null, false)
                ],
                new LongType(), TokenKind.SpeedSetMany),

            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.SpeedInfo, "INFO"), [],
                new StringType(), TokenKind.SpeedInfo),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.SpeedInfo, "INFO"),
                [new Parameter(-1, T(TokenKind.Identifier, "dbAlias"), new StringType(), SymbolFlags.None, null, false)],
                new StringType(), TokenKind.SpeedInfo),

            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.SpeedValidate, "VALIDATE"), [],
                new BitType(), TokenKind.SpeedValidate),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.SpeedValidate, "VALIDATE"),
                [new Parameter(-1, T(TokenKind.Identifier, "dbAlias"), new StringType(), SymbolFlags.None, null, false)],
                new BitType(), TokenKind.SpeedValidate),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.SpeedValidate, "VALIDATE"),
                [new Parameter(-1, T(TokenKind.Identifier, "full"), new BitType(), SymbolFlags.None, null, false)],
                new BitType(), TokenKind.SpeedValidate),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.SpeedValidate, "VALIDATE"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "dbAlias"), new StringType(), SymbolFlags.None, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "full"), new BitType(), SymbolFlags.None, null, false)
                ],
                new BitType(), TokenKind.SpeedValidate),
        };

        RegisterBuiltinNamespace(namespaceName, procedures, ImmutableDictionary<string, DataType>.Empty);
    }
}