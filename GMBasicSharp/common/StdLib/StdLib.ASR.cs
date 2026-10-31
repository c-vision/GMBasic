// FILE: StdLib.ASR.cs
// Namespace ASR — speech-to-text (Qwen3-ASR, engine nativo nel runtime condiviso gmb_runtime_c).
// ASR.RECOGNIZE(model$, wav$): trascrive un file WAV e ritorna il testo riconosciuto.

/* ==== GMB-STDLIB-AUDIT-BEGIN ====
 * FILE: ASR — namespace 'ASR'
 * 1 funzioni | 0 comandi | 0 tipi | 0 costanti
 *
 * FUNZIONI (1):
 *   RECOGNIZE
    ==== GMB-STDLIB-AUDIT-END ==== */

using System.Collections.Generic;

namespace Basic;

public sealed partial class StdLib
{
    private void CreateAsrNamespace()
    {
        const string namespaceName = "ASR";
        const SymbolFlags publicFuncFlag = SymbolFlags.IsPublic | SymbolFlags.IsFunction;
        const SymbolFlags noFlags = SymbolFlags.None;

        var procedures = new ProcedureDefinition[]
        {
            // ASR.RECOGNIZE(model$, wav$[, outPath$]) -> STRING (testo trascritto). model = dir HF del
            // modello Qwen3-ASR; wav = percorso di un file WAV (16-bit PCM, mono/stereo, qualsiasi rate).
            // outPath$ opzionale = file di transcript scritto IN LIVE (streaming: chunk 2s + rollback).
            // (Nome "RECOGNIZE" per non collidere con AI.TRANSCRIBE, provider-based.)
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.AsrTranscribe, "RECOGNIZE"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "model"), new StringType(), noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "wav"), new StringType(), noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "outPath"), new StringType(), SymbolFlags.IsOptional, null, false)
                ],
                new StringType(), TokenKind.AsrTranscribe, ExecutionKind: NativeExecutionKind.Pure),
        };

        RegisterBuiltinNamespace(namespaceName, procedures, new Dictionary<string, DataType>());
    }
}
