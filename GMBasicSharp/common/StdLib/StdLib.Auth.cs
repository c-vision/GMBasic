// FILE: Interpreter.StdLib.Auth.cs
// ReSharper disable InconsistentNaming
// ReSharper disable CheckNamespace

/* ==== GMB-STDLIB-AUDIT-BEGIN ====
 * FILE: Auth — namespace 'AUTH'
 * 3 funzioni | 0 comandi | 0 tipi | 0 costanti
 *
 * FUNZIONI (3):
 *   JWTCREATE
 *   JWTPAYLOAD
 *   JWTVERIFY
 * ==== GMB-STDLIB-AUDIT-END ==== */

namespace Basic;

using System.Collections.Immutable;

public sealed partial class StdLib
{
    /// <summary>
    /// Crea e registra tutte le definizioni per il namespace 'AUTH' (Security P2, 2026-09-02):
    /// JWT HS256 di composizione sulle primitive CRYPTOGRAPHY (HMAC-SHA256 + base64url).
    /// Funzioni pure, firme stabili — core condiviso CommonLib.Jwt / gmb_jwt_* nel runtime C.
    /// </summary>
    private void CreateAuthNamespace()
    {
        const string namespaceName = "AUTH";
        const SymbolFlags noFlags = SymbolFlags.None;
        const SymbolFlags
            publicFlag = SymbolFlags.IsPublic | SymbolFlags.IsFunction;

        var procedures = new ProcedureDefinition[]
        {
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.JwtCreate, "JWTCREATE"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "claims"), new StringType(), noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "secret"), new StringType(), noFlags, null, false)
                ], new StringType(), TokenKind.JwtCreate, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.JwtVerify, "JWTVERIFY"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "token"), new StringType(), noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "secret"), new StringType(), noFlags, null, false)
                ], new BitType(), TokenKind.JwtVerify, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.JwtPayload, "JWTPAYLOAD"),
                [new Parameter(-1, T(TokenKind.Identifier, "token"), new StringType(), noFlags, null, false)],
                new StringType(), TokenKind.JwtPayload, ExecutionKind: NativeExecutionKind.Pure),
        };

        RegisterBuiltinNamespace(namespaceName, procedures, ImmutableDictionary<string, DataType>.Empty);
    }
}
