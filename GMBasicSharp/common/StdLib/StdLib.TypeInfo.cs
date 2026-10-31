// FILE: Interpreter.StdLib.TypeInfo.cs

/* ==== GMB-STDLIB-AUDIT-BEGIN ====
 * FILE: TypeInfo — namespace 'TYPEINFO'
 * 4 funzioni | 0 comandi | 0 tipi | 0 costanti
 *
 * FUNZIONI (4):
 *   HAS
 *   METHODINFO
 *   PROPERTYINFO
 *   TYPEINFO
    ==== GMB-STDLIB-AUDIT-END ==== */


using System.Collections.Immutable;

namespace Basic;

public sealed partial class StdLib
{
    private void CreateTypeInfoNamespace()
    {
        const string namespaceName = "TYPEINFO";
        const SymbolFlags publicFlag = SymbolFlags.IsPublic | SymbolFlags.IsFunction;
        const SymbolFlags noFlags = SymbolFlags.None;
        const SymbolFlags optionalFlag = SymbolFlags.IsOptional;

        var procedures = new ProcedureDefinition[]
        {
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TypeInfo, "TYPEINFO"),
                [new Parameter(-1, T(TokenKind.Identifier, "value"), new InferredType(), noFlags, null, false)],
                new StringType(), TokenKind.TypeInfo),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TypeInfoHas, "HAS"),
                [new Parameter(-1, T(TokenKind.Identifier, "value"), new InferredType(), noFlags, null, false)],
                new DoubleType(), TokenKind.TypeInfoHas),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.MethodInfo, "METHODINFO"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "typeName"), new StringType(), noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "methodName"), new StringType(), optionalFlag, null, false)
                ],
                new StringType(), TokenKind.MethodInfo),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.PropertyInfo, "PROPERTYINFO"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "typeName"), new StringType(), noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "propertyName"), new StringType(), optionalFlag, null, false)
                ],
                new StringType(), TokenKind.PropertyInfo),
        };

        RegisterBuiltinNamespace(namespaceName, procedures, ImmutableDictionary<string, DataType>.Empty);
    }
}
