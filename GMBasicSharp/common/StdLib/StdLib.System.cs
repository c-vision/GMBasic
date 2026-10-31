// FILE: Interpreter.StdLib.System.cs
// ReSharper disable CheckNamespace

/* ==== GMB-STDLIB-AUDIT-BEGIN ====
 * FILE: System — namespace 'SYSTEM'
 * 26 funzioni | 2 comandi | 1 tipi | 0 costanti
 *
 * FUNZIONI (26):
 *   CHOOSE
 *   COMMAND
 *   COMMANDCOUNT
 *   CTYPE
 *   Count
 *   EMPTY
 *   GOMAXPROCS
 *   IIF
 *   LBOUND
 *   NUMCPU
 *   NUMGOROUTINE
 *   Next
 *   SHELLCODE
 *   SHELLOUT
 *   SIZEOF
 *   STRPTR
 *   STRTI
 *   SYS
 *   TAB
 *   TI
 *   TYPENAME
 *   UBOUND
 *   UDTALIGNED
 *   UDTPACKED
 *   VALTYPE
 *   VARPTR
 * COMANDI (2):
 *   RANDOMIZE
 *   SHELL
 * TIPI (1):
 *   ErrType
    ==== GMB-STDLIB-AUDIT-END ==== */


using System.Collections.Immutable;

namespace Basic;

public sealed partial class StdLib
{
    // ========================================================================
    // --- SYSTEM NAMESPACE ---
    // ========================================================================

    /// <summary>
    /// Crea e registra tutte le definizioni per il namespace 'SYSTEM'.
    /// </summary>
    private void CreateSystemNamespace()
    {
        const string namespaceName = "SYSTEM";
        const SymbolFlags publicFlag = SymbolFlags.IsPublic;
        const SymbolFlags noFlags = SymbolFlags.None;
        const SymbolFlags optionalFlag = SymbolFlags.IsOptional;
        const SymbolFlags byRefFlag = SymbolFlags.IsByRef;
        const SymbolFlags arrayFlag = SymbolFlags.IsArray;

        RegisterCommand(TokenKind.Randomize, "RANDOMIZE", namespaceName,
            (parser, ns) => Parser.ParseRandomizeStatement(parser));
        RegisterCommand(TokenKind.Shell, "SHELL", namespaceName,
            (parser, ns) => Parser.ParseSystemShellStatement(parser, ns));


        var procedures = new ProcedureDefinition[]
        {
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Next, "Next"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "generator"), new GeneratorType(new InferredType()),
                        noFlags, null, false)
                ],
                new InferredType(), TokenKind.Next),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Tab, "TAB"),
                [new Parameter(-1, T(TokenKind.Identifier, "column"), new IntegerType(), noFlags, null, false)],
                new StringType(),
                TokenKind.Tab),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Sys, "SYS"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "info_string"), new StringType(), noFlags, null, false)
                ],
                new StringType(), TokenKind.Sys),
            // SYSTEM.SHELL "<cmd>" — COMANDO (sintassi legacy, senza parentesi): registrato con
            // RegisterCommand sopra. NON deve essere una NativeProcedure (comando ≠ funzione).
            // SYSTEM.SHELLOUT("<cmd>") — funzione: esegue e ritorna stdout+stderr.
            new NativeProcedureDefinition(-1, publicFlag | SymbolFlags.IsFunction, T(TokenKind.ShellOut, "SHELLOUT"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "command"), new StringType(), noFlags, null, false)
                ],
                new StringType(), TokenKind.ShellOut),
            // SYSTEM.SHELLCODE("<cmd>") — funzione: ritorna l'exit code.
            new NativeProcedureDefinition(-1, publicFlag | SymbolFlags.IsFunction, T(TokenKind.ShellCode, "SHELLCODE"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "command"), new StringType(), noFlags, null, false)
                ],
                new LongType(), TokenKind.ShellCode),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Iif, "IIF"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "condition"), new BitType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "true_part"), new InferredType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "false_part"), new InferredType(), noFlags, null, false)
            ], new InferredType(), TokenKind.Iif),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Empty, "EMPTY"),
                [new Parameter(-1, T(TokenKind.Identifier, "value"), new InferredType(), noFlags, null, false)],
                new BitType(), TokenKind.Empty),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SizeOf, "SIZEOF"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "variable_or_type"), new InferredType(), noFlags, null,
                        false)
                ],
                new LongType(), TokenKind.SizeOf),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.VarPtr, "VARPTR"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "variable"), new InferredType(), byRefFlag, null, false)
                ],
                new LongType(), TokenKind.VarPtr),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.StrPtr, "STRPTR"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "string_variable"), new StringType(), noFlags, null,
                        false)
                ],
                new LongType(), TokenKind.StrPtr),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.UBound, "UBOUND"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "array"), new InferredType(), byRefFlag | arrayFlag, null,
                    false),
                new Parameter(-1, T(TokenKind.Identifier, "dimension"), new IntegerType(), optionalFlag, null, false)
            ], new LongType(), TokenKind.UBound),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.LBound, "LBOUND"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "array"), new InferredType(), byRefFlag | arrayFlag, null,
                    false),
                new Parameter(-1, T(TokenKind.Identifier, "dimension"), new IntegerType(), optionalFlag, null, false)
            ], new LongType(), TokenKind.LBound),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Count, "Count"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "source"), new StringType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "substring"), new StringType(), noFlags, null, false)
            ], new LongType(), TokenKind.Count),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.UdtPacked, "UDTPACKED"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "udt_variable"),
                        new UserDefinedType(-1, T(TokenKind.Identifier, "dummy"), [], SymbolFlags.None),
                        noFlags, null, false)
                ],
                new DoubleType(), TokenKind.UdtPacked),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.UdtAligned, "UDTALIGNED"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "udt_variable"),
                        new UserDefinedType(-1, T(TokenKind.Identifier, "dummy"), [], SymbolFlags.None),
                        noFlags, null, false)
                ],
                new DoubleType(), TokenKind.UdtAligned),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Ti, "TI"), [], new LongType(), TokenKind.Ti),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.StrTi, "STRTI"), [], new StringType(),
                TokenKind.StrTi),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TypeName, "TYPENAME"),
                [new Parameter(-1, T(TokenKind.Identifier, "variable"), new AnyType(), noFlags, null, false)],
                new StringType(), TokenKind.TypeName),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.ValType, "VALTYPE"),
                [new Parameter(-1, T(TokenKind.Identifier, "variable"), new AnyType(), noFlags, null, false)],
                new StringType(), TokenKind.ValType),
            new NativeProcedureDefinition(-1, publicFlag | SymbolFlags.IsFunction, T(TokenKind.CType, "CTYPE"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "value"), new AnyType(), noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "typeName"), new StringType(), noFlags, null, false)
                ],
                new AnyType(), TokenKind.CType),
            // CHOOSE(index, val1, val2, ...) — 1-based index into the value list.
            // Implemented as (index, values() ParamArray) for native dispatch.
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Choose, "CHOOSE"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "index"), new IntegerType(), noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "values"), new ArrayType(new InferredType()),
                        SymbolFlags.IsArray | SymbolFlags.IsParamArray, null, false)
                ],
                new InferredType(), TokenKind.Choose),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.CommandCount, "COMMANDCOUNT"),
                [], new IntegerType(), TokenKind.CommandCount),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Command, "COMMAND"),
                [new Parameter(-1, T(TokenKind.Identifier, "index"), new IntegerType(), noFlags, null, false)],
                new StringType(), TokenKind.Command),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.GoMaxProcs, "GOMAXPROCS"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "n"), new IntegerType(), optionalFlag, null, false)
                ],
                new LongType(), TokenKind.GoMaxProcs),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.NumGoroutine, "NUMGOROUTINE"),
                [], new LongType(), TokenKind.NumGoroutine),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.NumCpu, "NUMCPU"),
                [], new LongType(), TokenKind.NumCpu),
        };

        var globals = new List<(string Name, DataType Type, SymbolFlags Flags)>
        {
            ("ERR", new ErrType(), SymbolFlags.IsGlobal | SymbolFlags.IsPublic)
        };

        RegisterTypeSchema(typeof(ErrType), new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase)
        {
            { "Number", new LongType() },
            { "Description", new StringType() },
            { "LineNumber", new LongType() },
            { "SourceLineText", new StringType() },
            { "StackTrace", new StringType() }
        });
        
        // Aggiunge le definizioni delle procedure al provider del namespace.
        // E popola la mappa per la risoluzione delle funzioni built-in.
        RegisterBuiltinNamespace(namespaceName, procedures, ImmutableDictionary<string, DataType>.Empty, globals);
    }
}