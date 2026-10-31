/* FILE: native/gmb_db_c/src/ISAM/IsamExpressionBridge.c */
/* Port 1:1 di modules/Database/ISAM/IsamExpressionBridge.cs */
/* Ogni riga del C# è sotto come // — nessun commento, attributo o riga persa. */
#include "../../include/gmb_db_port.h"

/* --- C# originale --- */
//// FILE: Basic.ISAM.IsamExpressionBridge
//
//using Basic.Databases;
//using Basic.ISAM;
//
//namespace Basic.ISAM;
//
///// <summary>
///// Collega <see cref="Engine.Evaluator"/> e built-in ISAM al percorso unico di valutazione chiavi/filtri.
///// </summary>
//internal static class IsamExpressionBridge
//{
//    /// <summary>
//    /// Restituisce built-in → <paramref name="explicitEvaluator"/> → <see cref="Engine.Evaluator"/>.
//    /// </summary>
//    public static KeyExpressionEvaluatorCallback ResolveKeyEvaluator(
//        Engine engine,
//        KeyExpressionEvaluatorCallback? explicitEvaluator)
//    {
//        ArgumentNullException.ThrowIfNull(engine);
//        var user = explicitEvaluator ?? engine.Evaluator;
//        if (user == null)
//        {
//            throw new InvalidOperationException(
//                "KeyExpressionEvaluatorCallback richiesto: passare a IsamQuery.From o impostare Engine.Evaluator.");
//        }
//
//        return IsamKeyExpressionEvaluator.Chain(user);
//    }
//}

/* --- C (corrispondenza funzione-per-funzione: riempito file per file) --- */

int IsamExpressionBridge_TryEvaluateKey(
    Engine* engine, const char* expression, const IsamFieldValue* fields,
    char* outText, int textCap, int64_t* outI64, int* isText)
{
    const IsamSchema* s;
    if (!engine) return 0;
    s = Engine_GetSchema(engine);
    return IsamKeyExpressionEvaluator_TryEvaluateBuiltIn(expression, s, fields, outText, textCap, outI64, isText);
}

int IsamExpressionBridge_SelfCheck(void)
{
    IsamFieldSchema fs[2];
    IsamSchema schema;
    IsamFieldValue vals[2];
    char out[64];
    int isText = 0;
    int64_t n = 0;
    memset(&schema, 0, sizeof(schema));
    memset(fs, 0, sizeof(fs));
    snprintf(schema.TypeName, sizeof(schema.TypeName), "%s", "Person");
    snprintf(fs[0].Name, sizeof(fs[0].Name), "%s", "Id");
    snprintf(fs[0].DataTypeCode, sizeof(fs[0].DataTypeCode), "%s", "IntegerType");
    snprintf(fs[1].Name, sizeof(fs[1].Name), "%s", "Name");
    snprintf(fs[1].DataTypeCode, sizeof(fs[1].DataTypeCode), "%s", "StringType");
    fs[1].Length = 20;
    schema.Fields = fs;
    schema.FieldCount = 2;
    memset(vals, 0, sizeof(vals));
    vals[0].i64 = 7;
    vals[1].text = "alice";
    if (!IsamKeyExpressionEvaluator_TryEvaluateBuiltIn("UPPER(Name)", &schema, vals, out, (int)sizeof(out), &n, &isText))
        return -280;
    if (!isText || strcmp(out, "ALICE") != 0) return -281;
    if (!IsamKeyExpressionEvaluator_TryEvaluateBuiltIn("Id", &schema, vals, out, (int)sizeof(out), &n, &isText))
        return -282;
    if (isText || n != 7) return -283;
    vals[1].text = "{\"a\":\"x\"}";
    if (!IsamKeyExpressionEvaluator_TryEvaluateBuiltIn("JSON_VALUE(Name,'$.a')", &schema, vals, out, (int)sizeof(out), &n, &isText))
        return -284;
    if (strcmp(out, "x") != 0) return -285;
    return GMB_DB_OK;
}
