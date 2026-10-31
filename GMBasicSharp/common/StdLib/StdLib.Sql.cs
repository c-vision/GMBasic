// FILE: Interpreter.StdLib.Sql.cs

/* ==== GMB-STDLIB-AUDIT-BEGIN ====
 * FILE: Sql — namespace 'SQL'
 * 8 funzioni | 0 comandi | 1 tipi | 0 costanti
 *
 * FUNZIONI (8):
 *   CHANGES
 *   COL
 *   DFSCHEMA
 *   EXEC
 *   FETCH
 *   FROMCSV
 *   LASTINSERTROWID
 *   TOCSV
 * TIPI (1):
 *   SqlResultSetType
    ==== GMB-STDLIB-AUDIT-END ==== */


using System.Collections.Immutable;

namespace Basic;

public sealed partial class StdLib
{
    private void CreateSqlNamespace()
    {
        const string namespaceName = "SQL";
        const SymbolFlags publicFuncFlag = SymbolFlags.IsPublic | SymbolFlags.IsFunction;


        var procedures = new ProcedureDefinition[]
        {
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.SqlFetch, "FETCH"),
                [new Parameter(-1, T(TokenKind.Identifier, "resultSet"), new SqlResultSetType(), SymbolFlags.None, null, false)],
                new BitType(), TokenKind.SqlFetch),


            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.SqlExec, "EXEC"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "dbAlias"), new StringType(), SymbolFlags.None, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "sql"), new StringType(), SymbolFlags.None, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "params"), new ArrayType(new InferredType()),
                        SymbolFlags.IsArray | SymbolFlags.IsParamArray, null, false)
                ],
                new LongType(), TokenKind.SqlExec),

            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.SqlLastInsertRowId, "LASTINSERTROWID"),
                [new Parameter(-1, T(TokenKind.Identifier, "dbAlias"), new StringType(), SymbolFlags.None, null, false)],
                new LongType(), TokenKind.SqlLastInsertRowId),

            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.SqlChanges, "CHANGES"),
                [new Parameter(-1, T(TokenKind.Identifier, "dbAlias"), new StringType(), SymbolFlags.None, null, false)],
                new LongType(), TokenKind.SqlChanges),

            // G10 (2026-10-09): ergonomia DataFrame.
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.SqlDfSchema, "DFSCHEMA"),
                [new Parameter(-1, T(TokenKind.Identifier, "resultSet"), new SqlResultSetType(), SymbolFlags.None, null, false)],
                new StringType(), TokenKind.SqlDfSchema),

            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.SqlCol, "COL"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "resultSet"), new SqlResultSetType(), SymbolFlags.None, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "column"), new StringType(), SymbolFlags.None, null, false)
                ],
                new ArrayType(new StringType()), TokenKind.SqlCol),

            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.SqlToCsv, "TOCSV"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "resultSet"), new SqlResultSetType(), SymbolFlags.None, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "path"), new StringType(), SymbolFlags.None, null, false)
                ],
                new LongType(), TokenKind.SqlToCsv),

            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.SqlFromCsv, "FROMCSV"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "dbAlias"), new StringType(), SymbolFlags.None, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "table"), new StringType(), SymbolFlags.None, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "path"), new StringType(), SymbolFlags.None, null, false)
                ],
                new LongType(), TokenKind.SqlFromCsv),
        };

        RegisterBuiltinNamespace(namespaceName, procedures, ImmutableDictionary<string, DataType>.Empty);

        // SQLRESULTSET membri letti con il punto (rs.COUNT / rs.COLUMNS): senza uno schema
        // registrato, ValidateFieldAccess (Linker) li respinge come "membro non accessibile su
        // SQLRESULTSET" quando l'accesso avviene fuori da un PRINT a argomento singolo
        // (assegnazione, FOR bound, argomento di funzione). Il backend IL emette
        // SqlResultSetCount/SqlResultSetColumnCount per questi membri (IlProgramCompiler
        // .Expressions.cs), quindi lo schema dichiara esattamente quei due campi.
        RegisterTypeSchema(typeof(SqlResultSetType),
            new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase)
            {
                ["COUNT"] = new DoubleType(),
                ["COLUMNS"] = new DoubleType(),
            });
    }
}
