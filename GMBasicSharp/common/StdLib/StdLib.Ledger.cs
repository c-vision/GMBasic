// FILE: Interpreter.StdLib.Ledger.cs
// ReSharper disable InconsistentNaming
// ReSharper disable CheckNamespace

/* ==== GMB-STDLIB-AUDIT-BEGIN ====
 * FILE: Ledger — namespace 'LEDGER'
 * 17 funzioni | 0 comandi | 1 tipi | 0 costanti
 *
 * FUNZIONI (17):
 *   ACCOUNTS
 *   ADD
 *   BALANCE
 *   BOF
 *   COUNT
 *   EDIT
 *   EOF
 *   FOUND
 *   GET
 *   IMPORT
 *   LASTREC
 *   OPEN
 *   PLOT
 *   RECNO
 *   REGISTER
 *   VALIDATE
 *   VARIANCE
 * TIPI (1):
 *   LEDGERENTRY
    ==== GMB-STDLIB-AUDIT-END ==== */


using System.Collections.Immutable;

namespace Basic;

public sealed partial class StdLib
{
    // ========================================================================
    // --- LEDGER NAMESPACE ---
    // ========================================================================

    /// <summary>
    /// Crea e registra tutte le definizioni per il namespace 'LEDGER'.
    /// VERSIONE DEFINITIVA: Ripristina la doppia registrazione per supportare
    /// sia la sintassi a comando (`LEDGER.GET(h, var)`) sia quella a funzione (`var = LEDGER.GET(h)`).
    /// </summary>
    private void CreateLedgerNamespace()
    {
        const string namespaceName = "LEDGER";
        var publicFuncFlag = SymbolFlags.IsPublic | SymbolFlags.IsFunction;
        var noFlags = SymbolFlags.None;
        var optionalFlag = SymbolFlags.IsOptional;

        // --- 1. I comandi LEDGER sono SOLO legacy (sintassi spazio: LEDGER OPEN, LEDGER ADD).
        // Non esiste sintassi qualificata per i comandi LEDGER; le funzioni (che restituiscono
        // valore) restano qualificate con parentesi — vedi procedures qui sotto.
        // Le registrazioni qualificate per nome (registro qualificato) sono state RIMOSSE:
        // il dispatch passa da ParseLedgerStatement (TokenKind).

        // --- 2. Definisce il tipo built-in LEDGERENTRY (legacy DIM/GET INTO) ---
        var ledgerEntryFields = ImmutableArray.CreateBuilder<TypeField>();
        ledgerEntryFields.Add(new TypeField(-1, T(TokenKind.Identifier, "TransactionId"), new LongType(), [], null));
        ledgerEntryFields.Add(new TypeField(-1, T(TokenKind.Identifier, "Date"), new DateType(), [], null));
        ledgerEntryFields.Add(new TypeField(-1, T(TokenKind.Identifier, "Status"), new StringType(), [], null));
        ledgerEntryFields.Add(new TypeField(-1, T(TokenKind.Identifier, "Description"), new StringType(), [], null));
        ledgerEntryFields.Add(new TypeField(-1, T(TokenKind.Identifier, "Account"), new StringType(), [], null));
        ledgerEntryFields.Add(new TypeField(-1, T(TokenKind.Identifier, "Amount"), new CurrencyType(), [], null));
        ledgerEntryFields.Add(new TypeField(-1, T(TokenKind.Identifier, "Comment"), new StringType(), [], null));
        ledgerEntryFields.Add(new TypeField(-1, T(TokenKind.Identifier, "AmountIsExplicit"), new BitType(), [], null));
        ledgerEntryFields.Add(new TypeField(-1, T(TokenKind.Identifier, "Currency"), new StringType(), [], null));
        ledgerEntryFields.Add(new TypeField(-1, T(TokenKind.Identifier, "ExchangeRate"), new CurrencyType(), [], null));
        ledgerEntryFields.Add(new TypeField(-1, T(TokenKind.Identifier, "BaseAmount"), new CurrencyType(), [], null));

        var ledgerEntryUdt = new UserDefinedType(
            -1,
            T(TokenKind.Identifier, "LEDGERENTRY"),
            ledgerEntryFields.ToImmutable(),
            SymbolFlags.IsPublic | SymbolFlags.IsStruct);

        var types = new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase)
        {
            { "LEDGERENTRY", ledgerEntryUdt }
        }.ToImmutableDictionary(StringComparer.OrdinalIgnoreCase);

        // --- 3. Definisce le FUNZIONI NATIVE per la sintassi a espressione ---
        // Queste definizioni permettono di usare i comandi sul lato destro di un'assegnazione.
        var procedures = new ProcedureDefinition[]
        {
            // --- Funzioni che restituiscono un valore scalare ---
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.Open, "OPEN"),
                [new Parameter(-1, T(TokenKind.Identifier, "filePath"), new StringType(), noFlags, null, false)],
                new IntegerType(), TokenKind.Open),

            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.Add, "ADD"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "handle"), new IntegerType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "date"), new DateType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "description"), new StringType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "postings"),
                    new DictType(new StringType(), new CurrencyType()), noFlags, null, false)
            ], new IntegerType(), TokenKind.Add),

            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.Edit, "EDIT"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "handle"), new IntegerType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "transactionId"), new IntegerType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "date"), new DateType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "description"), new StringType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "postings"),
                    new DictType(new StringType(), new CurrencyType()), noFlags, null, false)
            ], new IntegerType(), TokenKind.Edit),

            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.Validate, "VALIDATE"),
                [new Parameter(-1, T(TokenKind.Identifier, "handle"), new IntegerType(), noFlags, null, false)],
                new BitType(), TokenKind.Validate),

            // --- Funzioni che restituiscono collezioni (ARRAY) ---
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.Register, "REGISTER"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "handle"), new IntegerType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "query"), new StringType(), optionalFlag, null, false)
            ], new ArrayType(new JsonType()), TokenKind.Register),

            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.Balance, "BALANCE"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "handle"), new IntegerType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "query"), new StringType(), optionalFlag, null, false)
            ], new ArrayType(new JsonType()), TokenKind.Balance),

            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.Get, "GET"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "handle"), new IntegerType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "transactionId"), new IntegerType(), optionalFlag, null, false)
            ], new ArrayType(new JsonType()), TokenKind.Get),

            // --- Funzioni cursore (pattern DBF/ISAM): LEDGER.EOF(h), LEDGER.BOF(h), ecc. ---
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.Eof, "EOF"),
                [new Parameter(-1, T(TokenKind.Identifier, "handle"), new IntegerType(), noFlags, null, false)],
                new BitType(), TokenKind.Eof),

            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.Bof, "BOF"),
                [new Parameter(-1, T(TokenKind.Identifier, "handle"), new IntegerType(), noFlags, null, false)],
                new BitType(), TokenKind.Bof),

            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.Recno, "RECNO"),
                [new Parameter(-1, T(TokenKind.Identifier, "handle"), new IntegerType(), noFlags, null, false)],
                new LongType(), TokenKind.Recno),

            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.Found, "FOUND"),
                [new Parameter(-1, T(TokenKind.Identifier, "handle"), new IntegerType(), noFlags, null, false)],
                new BitType(), TokenKind.Found),

            // --- Conteggio record (pattern DBF.RECCOUNT/LASTREC): LEDGER.COUNT(h) /
            //     LEDGER.LASTREC(h) → numero di transazioni del libro. ---
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.Count, "COUNT"),
                [new Parameter(-1, T(TokenKind.Identifier, "handle"), new IntegerType(), noFlags, null, false)],
                new LongType(), TokenKind.Count),

            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.Lastrec, "LASTREC"),
                [new Parameter(-1, T(TokenKind.Identifier, "handle"), new IntegerType(), noFlags, null, false)],
                new LongType(), TokenKind.Lastrec),

            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.Accounts, "ACCOUNTS"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "handle"), new IntegerType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "rootAccount"), new StringType(), optionalFlag, null, false)
            ], new ArrayType(new StringType()), TokenKind.Accounts),


            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.Variance, "VARIANCE"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "handle"), new IntegerType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "account"), new StringType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "period"), new StringType(), noFlags, null, false)
            ], new DoubleType(), TokenKind.Variance),

            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.Plot, "PLOT"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "handle"), new IntegerType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "query"), new StringType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "chartType"), new StringType(), noFlags, null, false)
            ], new IntegerType(), TokenKind.Plot),

            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.Import, "IMPORT"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "handle"), new IntegerType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "csvPath"), new StringType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "mappingRules"), new StringType(), noFlags, null, false)
            ], new IntegerType(), TokenKind.Import)
        };

        RegisterBuiltinNamespace(namespaceName, procedures, types);
    }
}