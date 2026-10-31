// FILE: Interpreter.StdLib.Json.cs
// ReSharper disable InconsistentNaming
// ReSharper disable CheckNamespace

/* ==== GMB-STDLIB-AUDIT-BEGIN ====
 * FILE: Json — namespace 'JSON'
 * 9 funzioni | 4 comandi | 0 tipi | 0 costanti
 *
 * FUNZIONI (9):
 *   Extract
 *   KEYS
 *   Load
 *   Parse
 *   ReadSection
 *   ReadValue
 *   Stringify
 *   ToJson
 *   ToObject
 * COMANDI (4):
 *   OPEN
 *   SAVE
 *   WRITESECTION
 *   WRITEVALUE
    ==== GMB-STDLIB-AUDIT-END ==== */


using System.Collections.Immutable;

namespace Basic;

public sealed partial class StdLib
{
    // ========================================================================
    // --- JSON NAMESPACE ---
    // ========================================================================

    /// <summary>
    /// Crea e registra tutte le definizioni per il namespace 'JSON'.
    /// VERSIONE CORRETTA: Trasforma JSON in un namespace con prefisso di comando,
    /// emulando il comportamento di DBF, ISAM, etc.
    /// </summary>
    private void CreateJsonNamespace()
    {
        const string namespaceName = "JSON";
        const SymbolFlags publicFlag = SymbolFlags.IsPublic;
        const SymbolFlags noFlags = SymbolFlags.None;
        const SymbolFlags optionalFlag = SymbolFlags.IsOptional;

        // --- 1. Registra 'JSON' come comando "padre" polimorfico ---
        // Questo permette al parser di riconoscere la sintassi "JSON COMANDO..."
        RegisterCommand(TokenKind.Json, "JSON", namespaceName, Parser.ParseJsonStatement);

        // --- 2. Registra i sotto-comandi come qualificati ---
        // Ora verranno riconosciuti solo dopo il prefisso "JSON" o "JSON."
        
        // --- 3. Definisce le FUNZIONI (che mantengono la sintassi JSON.FUNZIONE()) ---
        var procedures = new ProcedureDefinition[]
        {
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Parse, "Parse"),
                [new Parameter(-1, T(TokenKind.Identifier, "source"), new StringType(), noFlags, null, false)],
                new JsonType(),
                TokenKind.Parse),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Stringify, "Stringify"),
                [new Parameter(-1, T(TokenKind.Identifier, "value"), new InferredType(), noFlags, null, false)],
                new StringType(),
                TokenKind.Stringify),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Load, "Load"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "file_path"), new StringType(), noFlags, null, false)
            ], new JsonType(), TokenKind.Load),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.JsonToObject, "ToObject"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "targetType"), new InferredType(), noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "jsonSource"), new InferredType(), noFlags, null, false)
                ],
                new InferredType(),
                TokenKind.JsonToObject),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.ObjectToJson, "ToJson"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "objectInstance"), new InferredType(), noFlags, null, false)
                ],
                new JsonType(),
                TokenKind.ObjectToJson),

            // READVALUE/READSECTION accettano UNA firma con arity variabile (dispatch per count,
            // parità IL/PCode/VM): con 1-2 argomenti leggono dal file di settings IMPLICITO
            // (app.settings.json in CWD — forma storica usata dai sample); con 3-4 argomenti il
            // primo è il file ESPLICITO (es. SYSTEM.SYS("SETTINGSPATH") → system.settings.json).
            // I parametri extra sono dichiarati per ampliare totalParams (SymbolsByName indicizza
            // per NOME: gli overload con lo stesso nome NON convivono — mai duplicarli).
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.ReadValue, "ReadValue"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "keyOrFile"), new StringType(), noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "section"), new StringType(), optionalFlag, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "key"), new StringType(), optionalFlag, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "defaultValue"), new StringType(), optionalFlag, null, false)
                ],
                new InferredType(),
                TokenKind.ReadValue),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.ReadSection, "ReadSection"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "sectionOrFile"), new StringType(), noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "section"), new StringType(), optionalFlag, null, false)
                ],
                new JsonType(),
                TokenKind.ReadSection),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Extract, "Extract"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "json_object"), new JsonType(), noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "path"), new StringType(), noFlags, null, false)
                ],
                new InferredType(),
                TokenKind.Extract),
            
            // --- FUNZIONE: JSON.KEYS ---
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Keys, "KEYS"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "jsonObject"), new JsonType(), noFlags, null, false)
            ], new ArrayType(new StringType()), TokenKind.Keys)
        };
        
        // Registra il namespace con le sue funzioni. I comandi sono già stati registrati.
        RegisterBuiltinNamespace(namespaceName, procedures, ImmutableDictionary<string, DataType>.Empty);
    }

}