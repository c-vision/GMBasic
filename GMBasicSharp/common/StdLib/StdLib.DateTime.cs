// FILE: Interpreter.StdLib.DateTime.cs
// ReSharper disable InconsistentNaming
// ReSharper disable CheckNamespace

/* ==== GMB-STDLIB-AUDIT-BEGIN ====
 * FILE: DateTime — namespace 'DATETIME'
 * 36 funzioni | 0 comandi | 0 tipi | 0 costanti
 *
 * FUNZIONI (36):
 *   CDOW
 *   CMONTH
 *   CTOD
 *   DATEADD
 *   DATEDIFF
 *   DATEFROMUNIX
 *   DATESERIAL
 *   DATEVALUE
 *   DAY
 *   DAYSINMONTH
 *   DOW
 *   DTOC
 *   DTOS
 *   Date
 *   EOMONTH
 *   FIRSTDAYOFMONTH
 *   HOUR
 *   ISDATE
 *   ISLEAPYEAR
 *   ISOWEEKOFYEAR
 *   MINUTE
 *   MONTH
 *   NOW
 *   SECOND
 *   SECONDS
 *   STOD
 *   STRDATE
 *   STRTI
 *   STRTIME
 *   TI
 *   TIMESERIAL
 *   TODAY
 *   Time
 *   UNIXTIMESTAMP
 *   WEEKDAY
 *   YEAR
    ==== GMB-STDLIB-AUDIT-END ==== */


using System.Collections.Immutable;

namespace Basic;

public sealed partial class StdLib
{
    // ========================================================================
    // --- DATETIME NAMESPACE ---
    // ========================================================================

    /// <summary>
    /// Crea e registra tutte le definizioni per il namespace 'DATETIME'.
    /// </summary>
    private void CreateDateTimeNamespace()
    {
        const string namespaceName = "DATETIME";
        var publicFlag = SymbolFlags.IsPublic;
        var noFlags = SymbolFlags.None;

        /* Default del parametro data/ora: la data/ora CORRENTI, così `YEAR()` ≡ `YEAR(NOW())`.
         * Il parametro è OPTIONAL perché due overload omonimi non sono registrabili (il symbol
         * table tiene UN simbolo per nome): PCode riempie l'argomento mancante col DefaultValue,
         * mentre IL/VM/C/C99 ricevono 0 argomenti e usano la data/ora corrente. */
        Expr nowDefault() => new BuiltinFunctionCallExpr(
            new Token(TokenKind.Identifier, "DATETIME", null, SourcePosition.Unknown),
            T(TokenKind.Now, "NOW"),
            ImmutableArray<Expr>.Empty,
            new DateTimeType());

        var procedures = new ProcedureDefinition[]
        {
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Now, "NOW"), [], new DateTimeType(),
                TokenKind.Now),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Today, "TODAY"), [], new DateType(),
                TokenKind.Today),
            // Canone data/ora (UTC): DATE() = seriale giorno (numerico), TIME() = seriale data+ora
            // (numerico), STRTIME()/TIME$/TI$ = "HH:MM:SS", STRDATE()/DATE$ = "dd/MM/yyyy".
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Date, "Date"), [], new DoubleType(),
                TokenKind.Date),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Time, "Time"), [], new DoubleType(),
                TokenKind.Time),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.StrTime, "STRTIME"), [], new StringType(),
                TokenKind.StrTime),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.StrDate, "STRDATE"), [], new StringType(),
                TokenKind.StrDate),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.DateValue, "DATEVALUE"),
                [new Parameter(-1, T(TokenKind.Identifier, "dateString"), new StringType(), noFlags, null, false)],
                new DateType(),
                TokenKind.DateValue),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.DateSerial, "DATESERIAL"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "year"), new IntegerType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "month"), new IntegerType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "day"), new IntegerType(), noFlags, null, false)
            ], new DateType(), TokenKind.DateSerial),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TimeSerial, "TIMESERIAL"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "hour"), new IntegerType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "minute"), new IntegerType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "second"), new IntegerType(), noFlags, null, false)
            ], new TimeType(), TokenKind.TimeSerial),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Cdow, "CDOW"),
                [new Parameter(-1, T(TokenKind.Identifier, "date"), new DoubleType(), noFlags, null, false)], new StringType(), TokenKind.Cdow),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Cmonth, "CMONTH"),
                [new Parameter(-1, T(TokenKind.Identifier, "date"), new DoubleType(), noFlags, null, false)], new StringType(), TokenKind.Cmonth),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Ctod, "CTOD"),
                [new Parameter(-1, T(TokenKind.Identifier, "dateString"), new StringType(), noFlags, null, false)], new DoubleType(), TokenKind.Ctod),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Dtoc, "DTOC"),
                [new Parameter(-1, T(TokenKind.Identifier, "date"), new DoubleType(), noFlags, null, false)], new StringType(), TokenKind.Dtoc),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Dow, "DOW"),
                [new Parameter(-1, T(TokenKind.Identifier, "date"), new DoubleType(), noFlags, null, false)], new IntegerType(), TokenKind.Dow),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Seconds, "SECONDS"),
                [], new DoubleType(), TokenKind.Seconds),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Dtos, "DTOS"),

                [new Parameter(-1, T(TokenKind.Identifier, "dateValue"), new DateType(), noFlags, null, false)],
                new StringType(),
                TokenKind.Dtos),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Stod, "STOD"),
                [new Parameter(-1, T(TokenKind.Identifier, "dateString"), new StringType(), noFlags, null, false)],
                new DateType(),
                TokenKind.Stod),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.DateAdd, "DATEADD"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "interval"), new StringType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "number"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "dateValue"), new DateTimeType(), noFlags, null, false)
            ], new DateTimeType(), TokenKind.DateAdd),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.DateDiff, "DATEDIFF"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "interval"), new StringType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "date1"), new DateTimeType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "date2"), new DateTimeType(), noFlags, null, false)
            ], new DoubleType(), TokenKind.DateDiff),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Eomonth, "EOMONTH"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "startDate"), new DateType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "monthsToAdd"), new IntegerType(),
                    SymbolFlags.IsOptional, null, false)
            ], new DateType(), TokenKind.Eomonth),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Weekday, "WEEKDAY"),
                [new Parameter(-1, T(TokenKind.Identifier, "dateValue"), new DateType(), SymbolFlags.IsOptional, nowDefault(), false)],
                new LongType(),
                TokenKind.Weekday),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Year, "YEAR"),
                [new Parameter(-1, T(TokenKind.Identifier, "dateValue"), new DateTimeType(), SymbolFlags.IsOptional, nowDefault(), false)],
                new LongType(),
                TokenKind.Year),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Month, "MONTH"),
                [new Parameter(-1, T(TokenKind.Identifier, "dateValue"), new DateTimeType(), SymbolFlags.IsOptional, nowDefault(), false)],
                new LongType(),
                TokenKind.Month),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Day, "DAY"),
                [new Parameter(-1, T(TokenKind.Identifier, "dateValue"), new DateTimeType(), SymbolFlags.IsOptional, nowDefault(), false)],
                new LongType(),
                TokenKind.Day),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Hour, "HOUR"),
                [new Parameter(-1, T(TokenKind.Identifier, "timeValue"), new DateTimeType(), SymbolFlags.IsOptional, nowDefault(), false)],
                new LongType(),
                TokenKind.Hour),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Minute, "MINUTE"),
                [new Parameter(-1, T(TokenKind.Identifier, "timeValue"), new DateTimeType(), SymbolFlags.IsOptional, nowDefault(), false)],
                new LongType(),
                TokenKind.Minute),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Second, "SECOND"),
                [new Parameter(-1, T(TokenKind.Identifier, "timeValue"), new DateTimeType(), SymbolFlags.IsOptional, nowDefault(), false)],
                new LongType(),
                TokenKind.Second),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.IsDate, "ISDATE"),
                [new Parameter(-1, T(TokenKind.Identifier, "expression"), new InferredType(), noFlags, null, false)],
                new BitType(),
                TokenKind.IsDate),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Ti, "TI"), [], new LongType(), TokenKind.Ti),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.StrTi, "STRTI"), [], new StringType(),
                TokenKind.StrTi),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.DaysInMonth, "DAYSINMONTH"),
                [new Parameter(-1, T(TokenKind.Identifier, "dateValue"), new DateTimeType(), noFlags, null, false)],
                new IntegerType(),
                TokenKind.DaysInMonth),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.IsLeapYear, "ISLEAPYEAR"),
                [new Parameter(-1, T(TokenKind.Identifier, "dateValue"), new DateTimeType(), noFlags, null, false)],
                new BitType(),
                TokenKind.IsLeapYear),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.FirstDayOfMonth, "FIRSTDAYOFMONTH"),
                [new Parameter(-1, T(TokenKind.Identifier, "dateValue"), new DateTimeType(), noFlags, null, false)],
                new DateType(),
                TokenKind.FirstDayOfMonth),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.IsoWeekOfYear, "ISOWEEKOFYEAR"),
                [new Parameter(-1, T(TokenKind.Identifier, "dateValue"), new DateTimeType(), noFlags, null, false)],
                new IntegerType(),
                TokenKind.IsoWeekOfYear),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.DateFromUnix, "DATEFROMUNIX"),
                [new Parameter(-1, T(TokenKind.Identifier, "unixTimestamp"), new LongType(), noFlags, null, false)],
                new DateType(),
                TokenKind.DateFromUnix),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.UnixTimestamp, "UNIXTIMESTAMP"),
                [new Parameter(-1, T(TokenKind.Identifier, "dateValue"), new DateTimeType(), noFlags, null, false)],
                new LongType(),
                TokenKind.UnixTimestamp)
        };

        RegisterBuiltinNamespace(namespaceName, procedures, ImmutableDictionary<string, DataType>.Empty);
    }
}