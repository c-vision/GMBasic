/* FILE: native/gmb_db_c/src/Ledger/LedgerManager.c */
/* Port 1:1 di modules/Database/Ledger/LedgerManager.cs */
/* Ogni riga del C# è sotto come // — nessun commento, attributo o riga persa. */
#include "../../include/gmb_db_port.h"

/* --- C# originale --- */
//using System.Collections.Concurrent;
//using System.Globalization;
//using System.Text;
//using System.Text.RegularExpressions;
//
//// ReSharper disable CheckNamespace
//
//namespace Basic.Ledger;
//
///// <summary>
///// Nuova classe di utilità statica che incapsula tutta la logica di basso livello,
///// pura e senza stato (stateless) per la manipolazione dei file Ledger.
///// Questi metodi vengono chiamati dall'Executor all'interno delle operazioni PerformSyscall
///// per eseguire il lavoro di I/O e di parsing in un thread in background.
///// Questa classe sostituisce la logica precedentemente contenuta nel LedgerManager.
///// </summary>
//public sealed partial class LedgerManager
//{
//    /// <summary>
//    /// Rappresenta il risultato aggregato di una query di bilancio.
//    /// Viene restituito dalla logica di BALANCE e poi mappato su un array UDT dall'Executor.
//    /// </summary>
//    /// <param name="Account">Il nome del conto aggregato.</param>
//    /// <param name="TotalAmount">La somma totale per quel conto.</param>
//    // ReSharper disable once NotAccessedPositionalProperty.Global
//    public sealed record LedgerBalanceEntry(string Account, decimal TotalAmount);
//
//    #region Metodi di I/O e Indicizzazione
//
//    /// <summary>
//    /// Salva l'indice in memoria su un file binario (.idx) in modo sicuro.
//    /// Utilizza il pattern "write-to-temp-then-rename" per garantire l'atomicità.
//    /// </summary>
//    public static void SaveIndexToFile(string indexPath, ConcurrentDictionary<long, LedgerIndexRecord> index)
//    {
//        var tempFile = indexPath + ".tmp";
//        try
//        {
//            using (var stream = new FileStream(tempFile, FileMode.Create, FileAccess.Write, FileShare.None))
//            using (var writer = new BinaryWriter(stream, Encoding.UTF8, false))
//            {
//                writer.Write("LDGIDX"); // Magic string
//                writer.Write(1); // Version
//                writer.Write(index.Count);
//                foreach (var (transactionId, record) in index)
//                {
//                    writer.Write(transactionId);
//                    writer.Write(record.FileOffset);
//                    writer.Write(record.Date);
//                }
//            }
//
//            File.Move(tempFile, indexPath, true);
//        }
//        catch
//        {
//            if (File.Exists(tempFile)) File.Delete(tempFile);
//            throw;
//        }
//    }
//
//    /// <summary>
//    /// Legge il file indice (.idx) binario e popola la struttura dati dell'indice in memoria.
//    /// </summary>
//    public static void ReadIndexFromFile(string indexPath, ConcurrentDictionary<long, LedgerIndexRecord> index)
//    {
//        using var stream = new FileStream(indexPath, FileMode.Open, FileAccess.Read, FileShare.Read);
//        using var reader = new BinaryReader(stream, Encoding.UTF8, false);
//        if (stream.Length < 14) throw new InvalidDataException("Ledger index file is corrupted.");
//        if (reader.ReadString() != "LDGIDX") throw new InvalidDataException("Invalid Ledger index file format.");
//        if (reader.ReadInt32() != 1) throw new NotSupportedException("Unsupported Ledger index file version.");
//        var count = reader.ReadInt32();
//        for (var i = 0; i < count; i++)
//        {
//            var transactionId = reader.ReadInt64();
//            var fileOffset = reader.ReadInt64();
//            var date = reader.ReadInt32();
//            index[transactionId] = new LedgerIndexRecord(fileOffset, date);
//        }
//    }
//
//    /// <summary>
//    /// Esegue una scansione completa del file di giornale (.dat) e costruisce un nuovo indice.
//    /// </summary>
//    public static void RebuildIndexFromFile(string journalPath, ConcurrentDictionary<long, LedgerIndexRecord> index)
//    {
//        index.Clear();
//        if (!File.Exists(journalPath)) return;
//
//        long currentOffset = 0;
//        long transactionIdCounter = 0;
//        using var stream = new FileStream(journalPath, FileMode.Open, FileAccess.Read, FileShare.ReadWrite);
//        using var reader = new StreamReader(stream, Encoding.UTF8);
//        while (reader.ReadLine() is { } line)
//        {
//            if (!string.IsNullOrWhiteSpace(line) && !";#%*".Contains(line[0]) && char.IsDigit(line[0]))
//            {
//                var dateAsInt = ParseDateFromTransactionText(line);
//                index[++transactionIdCounter] = new LedgerIndexRecord(currentOffset, dateAsInt);
//            }
//
//            currentOffset = reader.BaseStream.Position;
//        }
//    }
//
//    /// <summary>
//    /// Legge un file Piano dei Conti (.coa) e popola la struttura dati corrispondente nel contesto.
//    /// </summary>
//    public static void LoadChartOfAccounts(string coaPath, LedgerContext context)
//    {
//        foreach (var line in File.ReadLines(coaPath))
//        {
//            if (string.IsNullOrWhiteSpace(line) || ";#%*".Contains(line[0])) continue;
//            var descriptionSeparatorIndex = line.IndexOf("  ", StringComparison.Ordinal);
//            if (descriptionSeparatorIndex < 0) descriptionSeparatorIndex = line.IndexOf('\t');
//            var accountFullName =
//                descriptionSeparatorIndex > 0 ? line[..descriptionSeparatorIndex].Trim() : line.Trim();
//            var description = descriptionSeparatorIndex > 0 ? line[descriptionSeparatorIndex..].Trim() : string.Empty;
//            var typeSeparatorIndex = accountFullName.IndexOf(':');
//            var accountType = typeSeparatorIndex > 0 ? accountFullName[..typeSeparatorIndex] : "Senza categoria";
//            if (!string.IsNullOrEmpty(accountFullName))
//                context.ChartOfAccounts[accountFullName] = new AccountDetails(accountType, description);
//        }
//    }
//
//    #endregion
//
//    #region Metodi di Parsing e Query
//
//    /// <summary>
//    /// Recupera e parsifica una singola transazione dato il suo ID.
//    /// Utilizza una cache per evitare I/O su disco ripetuti.
//    /// </summary>
//    /// <param name="context">Il contesto del libro mastro aperto.</param>
//    /// <param name="transactionId">L'ID della transazione da recuperare.</param>
//    /// <returns>Un array di <see cref="LedgerEntry"/> che rappresenta la transazione completa.</returns>
//    /// <exception cref="KeyNotFoundException">Lanciata se l'ID della transazione non viene trovato o è stato cancellato.</exception>
//    public static LedgerEntry[] GetTransaction(LedgerContext context, long transactionId)
//    {
//        // Controlla prima la cache per evitare I/O su disco non necessario.
//        if (context.TransactionCache.TryGetValue(transactionId, out var cachedTransaction))
//        {
//            return cachedTransaction;
//        }
//
//        // Cerca la transazione nell'indice.
//        if (!context.TransactionIndex.TryGetValue(transactionId, out var record) || record.FileOffset == -1)
//        {
//            throw new KeyNotFoundException($"Transazione con ID {transactionId} non trovata o cancellata.");
//        }
//
//        // Leggi il blocco di transazione dal file di giornale.
//        using var stream = new FileStream(context.JournalFilePath, FileMode.Open, FileAccess.Read, FileShare.ReadWrite);
//        using var reader = new StreamReader(stream, Encoding.UTF8);
//
//        stream.Seek(record.FileOffset, SeekOrigin.Begin);
//        var transactionEntries = ParseTransactionBlock(reader, transactionId);
//
//        // Aggiungi la transazione appena letta alla cache per accessi futuri.
//        context.TransactionCache.TryAdd(transactionId, transactionEntries);
//
//        return transactionEntries;
//    }
//
//    /// <summary>
//    /// Esegue il parsing di un singolo blocco di transazione da un TextReader e lo valida.
//    /// Questa versione è stata corretta per utilizzare una regex robusta per l'estrazione degli importi
//    /// e per gestire correttamente i fine riga e gli spazi bianchi.
//    /// </summary>
//    public static LedgerEntry[] ParseTransactionBlock(TextReader reader, long transactionId)
//    {
//        var entries = new List<LedgerEntry>();
//        var headerLine = reader.ReadLine()?.Trim() ?? string.Empty;
//        if (string.IsNullOrEmpty(headerLine)) throw new InvalidDataException("Transaction block cannot be empty.");
//
//        var headerParts = headerLine.Split([' ', '\t'], StringSplitOptions.RemoveEmptyEntries);
//        if (!DateOnly.TryParse(headerParts[0], out var date))
//            throw new InvalidDataException($"Invalid date format in header: '{headerLine}'");
//
//        var status = ' ';
//        var descriptionStart = 1;
//        if (headerParts.Length > 1 && headerParts[1].Length == 1 && "*!".Contains(headerParts[1][0]))
//        {
//            status = headerParts[1][0];
//            descriptionStart = 2;
//        }
//
//        var description = string.Join(" ", headerParts.Skip(descriptionStart));
//
//        // Legge le righe successive (i "postings") che appartengono alla stessa transazione.
//        // Un posting deve iniziare con uno spazio o un tab.
//        while (reader.Peek() != -1 && " \t".Contains((char)reader.Peek()))
//        {
//            var line = reader.ReadLine();
//            if (line == null) break;
//
//            var trimmedLine = line.Trim();
//            if (string.IsNullOrEmpty(trimmedLine)) continue;
//
//            string account;
//            var amountStr = "";
//            var comment = "";
//
//            var commentIndex = trimmedLine.IndexOf(';');
//            if (commentIndex >= 0)
//            {
//                comment = trimmedLine[(commentIndex + 1)..].Trim();
//                trimmedLine = trimmedLine[..commentIndex].Trim();
//            }
//
//            var amountMatch = MyAmountRegex().Match(trimmedLine);
//            if (amountMatch.Success)
//            {
//                account = trimmedLine[..amountMatch.Index].TrimEnd();
//                // La regex ha due gruppi di cattura: uno per [simbolo valuta facoltativo] + numero, 
//                // e uno per solo il numero. Prende il primo gruppo che ha catturato un valore.
//                amountStr = amountMatch.Groups[1].Success ? amountMatch.Groups[1].Value : amountMatch.Groups[2].Value;
//            }
//            else
//            {
//                account = trimmedLine;
//            }
//
//            decimal amount = 0;
//            decimal exchangeRate = 1m;
//            string? currency = null;
//            var amountIsExplicit = false;
//
//            if (!string.IsNullOrWhiteSpace(amountStr))
//            {
//                var parsed = ParseAmountWithCurrency(amountStr);
//                amount = parsed.Amount;
//                currency = parsed.Currency;
//                exchangeRate = parsed.Rate;
//                amountIsExplicit = true;
//            }
//
//            entries.Add(new LedgerEntry
//            {
//                TransactionId = transactionId, Date = date, Status = status, Description = description,
//                Account = account, Amount = amount, Comment = string.IsNullOrWhiteSpace(comment) ? null : comment,
//                AmountIsExplicit = amountIsExplicit,
//                Currency = currency,
//                ExchangeRate = exchangeRate
//            });
//        }
//
//        if (!IsTransactionBalanced(entries))
//            throw new InvalidDataException(
//                $"Transaction ID {transactionId} at {date} ('{description}') is not balanced.");
//
//        // Aggiorna l'ID della transazione per tutte le voci dopo il bilanciamento.
//        return entries.Select(e => e with { TransactionId = transactionId }).ToArray();
//    }
//
//    /// <summary>
//    /// Verifica se una transazione è bilanciata, calcolando l'importo mancante se necessario.
//    /// </summary>
//    public static bool IsTransactionBalanced(List<LedgerEntry> entries)
//    {
//        if (entries.Count == 0) return true;
//
//        var entriesWithNoAmount = entries.Where(e => !e.AmountIsExplicit).ToList();
//        if (entriesWithNoAmount.Count > 1) return false;
//
//        var sum = entries.Sum(e => e.BaseAmount);
//
//        if (entriesWithNoAmount.Count != 1) return Math.Abs(sum) < 0.0001m;
//        var entryToFix = entriesWithNoAmount.Single();
//        var index = entries.IndexOf(entryToFix);
//        entries[index] = entryToFix with { Amount = -sum, ExchangeRate = 1m };
//        return true;
//    }
//
//    /// <summary>Serializza un importo per il journal (es. <c>100.00 USD @ 0.95</c>).</summary>
//    public static string FormatAmountForJournal(decimal amount, string? currency, decimal exchangeRate)
//    {
//        var amountStr = amount.ToString(CultureInfo.InvariantCulture);
//        if (string.IsNullOrEmpty(currency))
//            return amountStr;
//
//        return exchangeRate == 1m
//            ? $"{amountStr} {currency}"
//            : $"{amountStr} {currency} @ {exchangeRate.ToString(CultureInfo.InvariantCulture)}";
//    }
//
//    /// <summary>
//    /// Metodo helper che incapsula la logica di filtraggio per ottenere le voci di giornale.
//    /// Questa versione è stata ottimizzata per:
//    /// 1. Aprire il file di giornale una sola volta, evitando I/O ripetuti.
//    /// 2. Utilizzare la cache delle transazioni per evitare di ri-parsificare dati già letti.
//    /// </summary>
//    public static List<LedgerEntry> GetFilteredEntries(LedgerContext context, string query)
//    {
//        var filters = ParseLedgerQuery(query);
//        var relevantRecords = context.TransactionIndex
//            .Where(kvp =>
//            {
//                var record = kvp.Value;
//                if (record.FileOffset == -1) return false; // Ignora cancellati
//                return filters.DateFilter == null || filters.DateFilter(record.Date);
//            })
//            .OrderBy(kvp => kvp.Value.Date)
//            .ThenBy(kvp => kvp.Key) // Aggiungi ordinamento secondario per stabilità
//            .ToList();
//
//        var results = new List<LedgerEntry>();
//
//        foreach (var (transactionId, _) in relevantRecords)
//        {
//            var transactionEntries = GetTransaction(context, transactionId);
//
//            // Applica il filtro per conto in memoria dopo aver ottenuto la transazione.
//            results.AddRange(transactionEntries.Where(entry =>
//                filters.AccountFilter == null || filters.AccountFilter(entry.Account)));
//        }
//
//        return results;
//    }
//
//    /// <summary>
//    /// Elenca i nomi conto dal piano dei conti o dal giornale, con filtro radice opzionale.
//    /// </summary>
//    /// <param name="context">Il contesto del libro mastro aperto.</param>
//    /// <param name="rootAccount">Se non vuoto, restituisce il conto radice e i discendenti diretti/indiretti (<c>root:child</c>).</param>
//    public static string[] GetAccounts(LedgerContext context, string? rootAccount = null)
//    {
//        string[] accounts;
//        if (context.ChartOfAccounts.Count > 0)
//        {
//            accounts = [.. context.ChartOfAccounts.Keys.OrderBy(static k => k)];
//        }
//        else
//        {
//            accounts = GetFilteredEntries(context, string.Empty)
//                .Select(static e => e.Account)
//                .Distinct(StringComparer.OrdinalIgnoreCase)
//                .OrderBy(static a => a)
//                .ToArray();
//        }
//
//        if (string.IsNullOrWhiteSpace(rootAccount))
//        {
//            return accounts;
//        }
//
//        var root = rootAccount.Trim();
//        return accounts.Where(a => IsUnderRootAccount(a, root)).ToArray();
//    }
//
//    // ponytail: colon-separated prefix only — "Expenses" matches "Expenses:Office", not wildcard or type segments before ':'
//    private static bool IsUnderRootAccount(string account, string root) =>
//        account.Equals(root, StringComparison.OrdinalIgnoreCase)
//        || account.StartsWith(root + ":", StringComparison.OrdinalIgnoreCase);
//
//    /// <summary>
//    /// Parsifica una stringa di query in un oggetto contenente delegati di filtro.
//    /// </summary>
//    public static (Func<int, bool>? DateFilter, Func<string, bool>? AccountFilter) ParseLedgerQuery(string query)
//    {
//        Func<int, bool>? dateFilter = null;
//        Func<string, bool>? accountFilter = null;
//        if (string.IsNullOrWhiteSpace(query)) return (dateFilter, accountFilter);
//
//        var accountMatch = MyAccountRegex().Match(query);
//        if (accountMatch.Success)
//        {
//            var patternValue = accountMatch.Groups[1].Success
//                ? accountMatch.Groups[1].Value
//                : accountMatch.Groups[2].Value;
//            var pattern = "^" + Regex.Escape(patternValue).Replace(@"\*", ".*") + "$";
//            accountFilter = acc => Regex.IsMatch(acc, pattern, RegexOptions.IgnoreCase);
//        }
//
//        var dateMatch = MyDateRegex().Match(query);
//        if (!dateMatch.Success || !DateOnly.TryParse(dateMatch.Groups[2].Value, out var dateValParsed))
//            return (dateFilter, accountFilter);
//        var op = dateMatch.Groups[1].Value;
//        var dateVal = dateValParsed.Year * 10000 + dateValParsed.Month * 100 + dateValParsed.Day;
//        dateFilter = op switch
//        {
//            ">" => d => d > dateVal, ">=" => d => d >= dateVal,
//            "<" => d => d < dateVal, "<=" => d => d <= dateVal,
//            "=" => d => d == dateVal, _ => null
//        };
//
//        return (dateFilter, accountFilter);
//    }
//
//    /// <summary>
//    /// Esegue il parsing della prima riga di un blocco di testo per estrarre la data.
//    /// </summary>
//    private static int ParseDateFromTransactionText(string transactionHeader)
//    {
//        var parts = transactionHeader.Split(' ', StringSplitOptions.RemoveEmptyEntries);
//        if (parts.Length > 0 && DateOnly.TryParse(parts[0], out var date))
//            return date.Year * 10000 + date.Month * 100 + date.Day;
//        var today = DateTime.Today;
//        return today.Year * 10000 + today.Month * 100 + today.Day;
//    }
//
//    /// <summary>
//    /// Metodo helper per leggere un intero blocco di transazione come testo grezzo.
//    /// </summary>
//    public static string? ReadTransactionBlockAsText(StreamReader reader)
//    {
//        var sb = new StringBuilder();
//        string? line;
//
//        if ((line = reader.ReadLine()) == null)
//            return null; // EOF
//        
//        sb.AppendLine(line);
//
//        while ((line = reader.ReadLine()) != null)
//        {
//            var trimmedLine = line.Trim();
//            if (string.IsNullOrEmpty(trimmedLine)) break;
//            if (char.IsDigit(trimmedLine[0]) && DateOnly.TryParse(trimmedLine.Split(' ')[0], out _))
//            {
//                reader.BaseStream.Seek(-Encoding.UTF8.GetByteCount(line) - Environment.NewLine.Length,
//                    SeekOrigin.Current);
//                break;
//            }
//
//            sb.AppendLine(line);
//        }
//
//        var result = sb.ToString().Trim();
//        return string.IsNullOrEmpty(result) ? null : result;
//    }
//
//    [GeneratedRegex(@"date:([><=]+)(\d{4}-\d{2}-\d{2})")]
//    private static partial Regex MyDateRegex();
//
//    [GeneratedRegex(@"\s{2,}(.*)")]
//    private static partial Regex MyAmountRegex();
//
//    [GeneratedRegex("""
//                    account:(?:"([^"]+)"|(\S+(?::\S+)*))
//                    """)]
//    private static partial Regex MyAccountRegex();
//
//    #endregion
//
//    #region Session Handles
//
//    private static int _nextSessionHandle;
//
//    /// <summary>
//    /// Allocates a monotonic LEDGER OPEN session handle. Thread-safe; avoids <see cref="object.GetHashCode"/> collisions.
//    /// </summary>
//    public static int AllocateSessionHandle() => Interlocked.Increment(ref _nextSessionHandle);
//
//    #endregion
//
//    #region Public API per P-CODE VM
//
//    /// <summary>
//    /// Aggiunge una transazione al journal e restituisce il nuovo transaction ID.
//    /// </summary>
//    public static long AddTransaction(LedgerContext context, DateOnly date, string description,
//        List<LedgerEntry> entries)
//    {
//        if (!IsTransactionBalanced(entries))
//            throw new InvalidOperationException($"La transazione del {date:yyyy-MM-dd} ('{description}') non è bilanciata.");
//
//        context.Lock.Wait();
//        try
//        {
//            var sb = new StringBuilder();
//            sb.Append(date.ToString("yyyy-MM-dd", CultureInfo.InvariantCulture)).Append(' ').Append(description);
//            foreach (var entry in entries)
//            {
//                sb.AppendLine().Append("    ").Append(entry.Account);
//                sb.Append("      ").Append(FormatAmountForJournal(entry.Amount, entry.Currency, entry.ExchangeRate));
//            }
//
//            var transactionText = sb.ToString();
//            long newTransactionId;
//            using (var fs = new FileStream(context.JournalFilePath, FileMode.OpenOrCreate, FileAccess.ReadWrite, FileShare.ReadWrite))
//            using (var writer = new StreamWriter(fs, Encoding.UTF8))
//            {
//                var newOffset = fs.Length;
//                if (newOffset > 0)
//                {
//                    fs.Seek(-1, SeekOrigin.End);
//                    if (fs.ReadByte() != '\n') writer.WriteLine();
//                    newOffset = fs.Position;
//                }
//                writer.WriteLine(transactionText);
//                writer.Flush();
//                newTransactionId = (context.TransactionIndex.Keys.Count != 0 ? context.TransactionIndex.Keys.Max() : 0) + 1;
//                var dateAsInt = date.Year * 10000 + date.Month * 100 + date.Day;
//                context.TransactionIndex[newTransactionId] = new LedgerIndexRecord(newOffset, dateAsInt);
//            }
//            context.IsDirty = true;
//            return newTransactionId;
//        }
//        finally
//        {
//            context.Lock.Release();
//        }
//    }
//
//    /// <summary>
//    /// Elimina una transazione dal journal (la marca come cancellata).
//    /// </summary>
//    public static bool DeleteTransaction(LedgerContext context, long transactionId)
//    {
//        context.Lock.Wait();
//        try
//        {
//            if (!context.TransactionIndex.TryGetValue(transactionId, out var record)) return false;
//            var entries = GetTransaction(context, transactionId);
//            if (entries.Length == 0) return false;
//
//            var dateStr = entries[0].Date.ToString("yyyy-MM-dd", CultureInfo.InvariantCulture);
//            var desc = entries[0].Description ?? "";
//            var sb = new StringBuilder();
//            sb.Append(dateStr).Append(" * ").Append(desc);
//            foreach (var entry in entries)
//            {
//                sb.AppendLine().Append("    ").Append(entry.Account);
//                sb.Append("      ").Append(FormatAmountForJournal(-entry.Amount, entry.Currency, entry.ExchangeRate));
//            }
//
//            var voidText = sb.ToString();
//            using (var fs = new FileStream(context.JournalFilePath, FileMode.OpenOrCreate, FileAccess.ReadWrite, FileShare.ReadWrite))
//            using (var writer = new StreamWriter(fs, Encoding.UTF8))
//            {
//                var newOffset = fs.Length;
//                if (newOffset > 0)
//                {
//                    fs.Seek(-1, SeekOrigin.End);
//                    if (fs.ReadByte() != '\n') writer.WriteLine();
//                    newOffset = fs.Position;
//                }
//                writer.WriteLine(voidText);
//                writer.Flush();
//            }
//            context.IsDirty = true;
//            return true;
//        }
//        finally
//        {
//            context.Lock.Release();
//        }
//    }
//
//    /// <summary>
//    /// Compatta il journal rimuovendo le transazioni cancellate.
//    /// </summary>
//    public static void PackJournal(LedgerContext context)
//    {
//        context.Lock.Wait();
//        try
//        {
//            var tempPath = context.JournalFilePath + ".packed";
//            var index = new ConcurrentDictionary<long, LedgerIndexRecord>();
//            long nextId = 1;
//            long offset = 0;
//
//            using (var reader = new StreamReader(context.JournalFilePath, Encoding.UTF8))
//            using (var writer = new StreamWriter(tempPath, false, Encoding.UTF8))
//            {
//                string? block;
//                while ((block = ReadTransactionBlockAsText(reader)) != null)
//                {
//                    if (string.IsNullOrWhiteSpace(block)) continue;
//                    writer.Write(block);
//                    if (!block.EndsWith('\n')) writer.WriteLine();
//                    index[nextId] = new LedgerIndexRecord(offset, 0);
//                    offset = writer.BaseStream.Position;
//                    nextId++;
//                }
//                writer.Flush();
//            }
//
//            File.Move(tempPath, context.JournalFilePath, true);
//            context.TransactionIndex.Clear();
//            foreach (var kv in index) context.TransactionIndex[kv.Key] = kv.Value;
//
//            var indexFile = context.IndexFilePath;
//            if (File.Exists(indexFile)) File.Delete(indexFile);
//            SaveIndexToFile(indexFile, context.TransactionIndex);
//            context.IsDirty = false;
//        }
//        finally
//        {
//            context.Lock.Release();
//        }
//    }
//
//    #endregion
//}

/* --- C (corrispondenza funzione-per-funzione: riempito file per file) --- */

#include <pthread.h>
#include <unistd.h>
#include <limits.h>
#include <time.h>
#ifndef _WIN32
#include <strings.h>
#endif

#define GMB_LEDGER_TX_TEXT 8192
#define GMB_LEDGER_MAX_POSTINGS 32
#define GMB_LEDGER_MAX_COA 256
#define GMB_LEDGER_TYPE 64
#define GMB_LEDGER_DESC 128
#define GMB_LEDGER_CSV_FIELD 256
#define GMB_LEDGER_MAX_CSV_COLS 32
#define GMB_LEDGER_INDEX_INIT 16
#define GMB_LEDGER_CACHE_INIT 8

typedef struct {
    int64_t txId;
    int n;
    LedgerEntry e[GMB_LEDGER_MAX_POSTINGS];
} LedgerTxCache;

struct LedgerContext {
    char* JournalFilePath;
    char* IndexFilePath;
    char* BudgetFilePath;
    int IsDirty;
    int BudgetsDirty;
    int nIndex;
    int capIndex;
    int64_t* indexIds;
    LedgerIndexRecord* indexRecs;
    int nCache;
    int capCache;
    LedgerTxCache* cache;
    int nCoa;
    char coaName[GMB_LEDGER_MAX_COA][GMB_LEDGER_NAME];
    char coaType[GMB_LEDGER_MAX_COA][GMB_LEDGER_TYPE];
    char coaDesc[GMB_LEDGER_MAX_COA][GMB_LEDGER_DESC];
    int nBudget;
    char budgetAccount[GMB_LEDGER_MAX_COA][GMB_LEDGER_NAME];
    char budgetPeriod[GMB_LEDGER_MAX_COA][16];
    GmbDecimal budgetAmount[GMB_LEDGER_MAX_COA];
    pthread_mutex_t Lock;
};

/* .NET Path.ChangeExtension: replace last '.' in the filename, else append. */
static char* ledger_change_ext(const char* path, const char* ext)
{
    const char* slash;
    const char* base;
    const char* dot;
    size_t stem, elen;
    char* out;
    if (!path || !ext) return NULL;
    slash = strrchr(path, '/');
    base = slash ? slash + 1 : path;
    dot = strrchr(base, '.');
    stem = dot ? (size_t)(dot - path) : strlen(path);
    elen = strlen(ext);
    out = (char*)malloc(stem + elen + 1);
    if (!out) return NULL;
    memcpy(out, path, stem);
    memcpy(out + stem, ext, elen + 1);
    return out;
}

static void ledger_write7(FILE* f, unsigned v)
{
    while (v >= 0x80) {
        fputc((int)((v & 0x7Fu) | 0x80u), f);
        v >>= 7;
    }
    fputc((int)v, f);
}

static void ledger_write_bw_string(FILE* f, const char* s)
{
    unsigned n = (unsigned)strlen(s);
    ledger_write7(f, n);
    fwrite(s, 1, n, f);
}

static void ledger_write_le32(FILE* f, int32_t v)
{
    uint8_t b[4];
    b[0] = (uint8_t)v;
    b[1] = (uint8_t)(v >> 8);
    b[2] = (uint8_t)(v >> 16);
    b[3] = (uint8_t)(v >> 24);
    fwrite(b, 1, 4, f);
}

static void ledger_write_le64(FILE* f, int64_t v)
{
    int i;
    for (i = 0; i < 8; i++)
        fputc((int)((v >> (8 * i)) & 0xFF), f);
}

static int ledger_read7(FILE* f, unsigned* v)
{
    unsigned n = 0, shift = 0;
    for (;;) {
        int c = fgetc(f);
        if (c < 0) return -1;
        n |= ((unsigned)c & 0x7Fu) << shift;
        if ((c & 0x80) == 0) break;
        shift += 7;
        if (shift > 28) return -1;
    }
    *v = n;
    return 0;
}

static int ledger_read_bw_string(FILE* f, char* buf, int cap)
{
    unsigned n;
    if (ledger_read7(f, &n) != 0) return -1;
    if ((int)n >= cap) return -1;
    if (n > 0 && fread(buf, 1, n, f) != n) return -1;
    buf[n] = 0;
    return 0;
}

static int ledger_read_le32(FILE* f, int32_t* v)
{
    uint8_t b[4];
    if (fread(b, 1, 4, f) != 4) return -1;
    *v = (int32_t)((uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24));
    return 0;
}

static int ledger_read_le64(FILE* f, int64_t* v)
{
    uint8_t b[8];
    int i;
    uint64_t u = 0;
    if (fread(b, 1, 8, f) != 8) return -1;
    for (i = 7; i >= 0; i--)
        u = (u << 8) | b[i];
    *v = (int64_t)u;
    return 0;
}

static long ledger_file_len(FILE* f)
{
    long cur = ftell(f), end;
    if (cur < 0) return -1;
    if (fseek(f, 0, SEEK_END) != 0) return -1;
    end = ftell(f);
    fseek(f, cur, SEEK_SET);
    return end;
}

static void ledger_fmt_num(GmbDecimal v, char* out, int cap)
{
    (void)GmbDecimal_ToString(v, out, cap);
}

static void ledger_fmt_double(double v, char* out, int cap)
{
    GmbDecimal d = GmbDecimal_FromDouble(v);
    (void)GmbDecimal_ToString(d, out, cap);
}

static int ledger_index_grow(LedgerContext* ctx)
{
    int cap;
    int64_t* ids;
    LedgerIndexRecord* recs;
    if (!ctx) return GMB_DB_ARG;
    cap = ctx->capIndex < GMB_LEDGER_INDEX_INIT ? GMB_LEDGER_INDEX_INIT : ctx->capIndex * 2;
    ids = (int64_t*)realloc(ctx->indexIds, (size_t)cap * sizeof(int64_t));
    recs = (LedgerIndexRecord*)realloc(ctx->indexRecs, (size_t)cap * sizeof(LedgerIndexRecord));
    if (!ids || !recs) {
        if (ids && ids != ctx->indexIds) ctx->indexIds = ids;
        if (recs && recs != ctx->indexRecs) ctx->indexRecs = recs;
        return GMB_DB_ERR;
    }
    ctx->indexIds = ids;
    ctx->indexRecs = recs;
    ctx->capIndex = cap;
    return GMB_DB_OK;
}

static int ledger_cache_grow(LedgerContext* ctx)
{
    int cap, old;
    LedgerTxCache* p;
    if (!ctx) return GMB_DB_ARG;
    old = ctx->capCache;
    cap = old < GMB_LEDGER_CACHE_INIT ? GMB_LEDGER_CACHE_INIT : old * 2;
    p = (LedgerTxCache*)realloc(ctx->cache, (size_t)cap * sizeof(LedgerTxCache));
    if (!p) return GMB_DB_ERR;
    if (cap > old)
        memset(p + old, 0, (size_t)(cap - old) * sizeof(LedgerTxCache));
    ctx->cache = p;
    ctx->capCache = cap;
    return GMB_DB_OK;
}

static int ledger_filtered_cap(const LedgerContext* ctx)
{
    int cap = ctx && ctx->nIndex > 0 ? ctx->nIndex * GMB_LEDGER_MAX_POSTINGS : 16;
    return cap < 16 ? 16 : cap;
}

static int ledger_index_find(const LedgerContext* ctx, int64_t txId)
{
    int i;
    for (i = 0; i < ctx->nIndex; i++) {
        if (ctx->indexIds[i] == txId) return i;
    }
    return -1;
}

static int64_t ledger_index_max_id(const LedgerContext* ctx)
{
    int64_t m = 0;
    int i;
    for (i = 0; i < ctx->nIndex; i++) {
        if (ctx->indexIds[i] > m) m = ctx->indexIds[i];
    }
    return m;
}

static int ledger_index_put(LedgerContext* ctx, int64_t txId, int64_t offset, int32_t date)
{
    int i = ledger_index_find(ctx, txId);
    if (i >= 0) {
        ctx->indexRecs[i].FileOffset = offset;
        ctx->indexRecs[i].Date = date;
        return GMB_DB_OK;
    }
    if (ctx->nIndex >= ctx->capIndex && ledger_index_grow(ctx) != GMB_DB_OK) return GMB_DB_ERR;
    ctx->indexIds[ctx->nIndex] = txId;
    ctx->indexRecs[ctx->nIndex].FileOffset = offset;
    ctx->indexRecs[ctx->nIndex].Date = date;
    ctx->nIndex++;
    return GMB_DB_OK;
}

static void ledger_index_clear(LedgerContext* ctx)
{
    if (ctx) ctx->nIndex = 0;
}

static void ledger_entry_clear_owned(LedgerEntry* e)
{
    if (!e) return;
    free((void*)e->Description);
    free((void*)e->Account);
    free((void*)e->Comment);
    free((void*)e->Currency);
    memset(e, 0, sizeof(*e));
}

static void ledger_cache_slot_free(LedgerTxCache* slot)
{
    int i;
    if (!slot) return;
    for (i = 0; i < slot->n; i++)
        ledger_entry_clear_owned(&slot->e[i]);
    slot->n = 0;
    slot->txId = 0;
}

static void ledger_cache_clear(LedgerContext* ctx)
{
    int i;
    if (!ctx) return;
    for (i = 0; i < ctx->nCache; i++)
        ledger_cache_slot_free(&ctx->cache[i]);
    ctx->nCache = 0;
}

static int ledger_cache_find(const LedgerContext* ctx, int64_t txId)
{
    int i;
    for (i = 0; i < ctx->nCache; i++) {
        if (ctx->cache[i].txId == txId) return i;
    }
    return -1;
}

static char* ledger_dup(const char* s)
{
    return s ? strdup(s) : NULL;
}

static int ledger_is_blank(const char* s)
{
    if (!s) return 1;
    while (*s) {
        if (!isspace((unsigned char)*s)) return 0;
        s++;
    }
    return 1;
}

static void ledger_rtrim(char* s)
{
    size_t n;
    if (!s) return;
    n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1]))
        s[--n] = 0;
}

static int ledger_glob_ci(const char* s, const char* pat)
{
    if (!s || !pat) return 0;
    while (*pat) {
        if (*pat == '*') {
            pat++;
            if (!*pat) return 1;
            while (*s) {
                if (ledger_glob_ci(s, pat)) return 1;
                s++;
            }
            return 0;
        }
        if (!*s || tolower((unsigned char)*s) != tolower((unsigned char)*pat))
            return 0;
        s++;
        pat++;
    }
    return *s == 0;
}

static int ledger_parse_ymd(const char* s, int32_t* out)
{
    int y, m, d;
    if (!s || !out) return 0;
    if (sscanf(s, "%4d-%2d-%2d", &y, &m, &d) != 3) return 0;
    if (y < 1 || m < 1 || m > 12 || d < 1 || d > 31) return 0;
    *out = y * 10000 + m * 100 + d;
    return 1;
}

static int ledger_date_match(const LedgerQueryFilter* f, int32_t d)
{
    if (!f || !f->hasDate) return 1;
    if (strcmp(f->dateOp, ">") == 0) return d > f->dateVal;
    if (strcmp(f->dateOp, ">=") == 0) return d >= f->dateVal;
    if (strcmp(f->dateOp, "<") == 0) return d < f->dateVal;
    if (strcmp(f->dateOp, "<=") == 0) return d <= f->dateVal;
    if (strcmp(f->dateOp, "=") == 0) return d == f->dateVal;
    return 1;
}

typedef struct {
    int64_t id;
    int32_t date;
} LedgerQueryRec;

static int ledger_query_rec_cmp(const void* a, const void* b)
{
    const LedgerQueryRec* x = (const LedgerQueryRec*)a;
    const LedgerQueryRec* y = (const LedgerQueryRec*)b;
    if (x->date != y->date) return x->date < y->date ? -1 : 1;
    if (x->id != y->id) return x->id < y->id ? -1 : 1;
    return 0;
}

static int ledger_name_cmp(const void* a, const void* b)
{
    return strcmp((const char*)a, (const char*)b);
}

static int ledger_coa_find(const LedgerContext* ctx, const char* name)
{
    int i;
    if (!ctx || !name) return -1;
    for (i = 0; i < ctx->nCoa; i++) {
        if (gmb_strcasecmp(ctx->coaName[i], name) == 0) return i;
    }
    return -1;
}

static int ledger_is_under_root(const char* account, const char* root)
{
    size_t n;
    if (!account || !root) return 0;
    if (gmb_strcasecmp(account, root) == 0) return 1;
    n = strlen(root);
    return gmb_strncasecmp(account, root, n) == 0 && account[n] == ':';
}

static void ledger_copy_trim(char* dst, size_t cap, const char* src)
{
    if (!dst || cap == 0) return;
    while (src && *src && isspace((unsigned char)*src)) src++;
    snprintf(dst, cap, "%s", src ? src : "");
    ledger_rtrim(dst);
}

static int ledger_budget_find(const LedgerContext* ctx, const char* account, const char* period)
{
    char a[GMB_LEDGER_NAME], p[16], key[GMB_LEDGER_NAME + 20], have[GMB_LEDGER_NAME + 20];
    int i;
    if (!ctx) return -1;
    ledger_copy_trim(a, sizeof(a), account);
    ledger_copy_trim(p, sizeof(p), period);
    snprintf(key, sizeof(key), "%s|%s", a, p);
    for (i = 0; i < ctx->nBudget; i++) {
        snprintf(have, sizeof(have), "%s|%s", ctx->budgetAccount[i], ctx->budgetPeriod[i]);
        if (gmb_strcasecmp(have, key) == 0) return i;
    }
    return -1;
}

static int ledger_try_parse_budget_period(const char* period, int* year, int* month)
{
    int y = 0, m = 0;
    if (!period || !year || !month) return 0;
    if (sscanf(period, "%d-%d", &y, &m) != 2) return 0;
    if (m < 1 || m > 12) return 0;
    *year = y;
    *month = m;
    return 1;
}

static int ledger_days_in_month(int y, int m)
{
    static const int d[] = { 0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    if (m < 1 || m > 12) return 0;
    if (m == 2 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0)) return 29;
    return d[m];
}

typedef struct {
    char period[16];
    char account[GMB_LEDGER_NAME];
    GmbDecimal amount;
} LedgerBudgetRow;

static int ledger_budget_row_cmp(const void* a, const void* b)
{
    const LedgerBudgetRow* x = (const LedgerBudgetRow*)a;
    const LedgerBudgetRow* y = (const LedgerBudgetRow*)b;
    int c = strcmp(x->period, y->period);
    if (c) return c;
    return strcmp(x->account, y->account);
}

LedgerContext* LedgerContext_New(const char* journalPath)
{
    LedgerContext* ctx;
    if (!journalPath) return NULL;
    ctx = (LedgerContext*)calloc(1, sizeof(*ctx));
    if (!ctx) return NULL;
    pthread_mutex_init(&ctx->Lock, NULL);
    ctx->JournalFilePath = strdup(journalPath);
    ctx->IndexFilePath = ledger_change_ext(journalPath, ".idx");
    ctx->BudgetFilePath = ledger_change_ext(journalPath, ".budget");
    if (!ctx->JournalFilePath || !ctx->IndexFilePath || !ctx->BudgetFilePath) {
        LedgerContext_Dispose(ctx);
        return NULL;
    }
    return ctx;
}

void LedgerContext_Dispose(LedgerContext* self)
{
    int i;
    if (!self) return;
    for (i = 0; i < self->nCache; i++)
        ledger_cache_slot_free(&self->cache[i]);
    pthread_mutex_destroy(&self->Lock);
    free(self->indexIds);
    free(self->indexRecs);
    free(self->cache);
    free(self->JournalFilePath);
    free(self->IndexFilePath);
    free(self->BudgetFilePath);
    free(self);
}

const char* LedgerContext_JournalFilePath(const LedgerContext* self)
{
    return self ? self->JournalFilePath : NULL;
}

const char* LedgerContext_IndexFilePath(const LedgerContext* self)
{
    return self ? self->IndexFilePath : NULL;
}

const char* LedgerContext_BudgetFilePath(const LedgerContext* self)
{
    return self ? self->BudgetFilePath : NULL;
}

int LedgerContext_IsDirty(const LedgerContext* self)
{
    return self ? self->IsDirty : 0;
}

int LedgerContext_IsBudgetsDirty(const LedgerContext* self)
{
    return self ? self->BudgetsDirty : 0;
}

int LedgerManager_IndexCount(const LedgerContext* ctx)
{
    return ctx ? ctx->nIndex : 0;
}

static int ledger_id_cmp(const void* a, const void* b)
{
    int64_t x = *(const int64_t*)a, y = *(const int64_t*)b;
    return (x > y) - (x < y);
}

/* Cursore LEDGER: id transazioni ORDINATI per ID (pattern DBF/ISAM, parità C#
 * LedgerSortedIds). out deve avere capacità >= LedgerManager_IndexCount(ctx). */
int LedgerManager_GetSortedIndexIds(const LedgerContext* ctx, int64_t* out, int max, int* nOut)
{
    if (!ctx || !out || !nOut || max < ctx->nIndex) return GMB_DB_ARG;
    memcpy(out, ctx->indexIds, (size_t)ctx->nIndex * sizeof(int64_t));
    qsort(out, (size_t)ctx->nIndex, sizeof(int64_t), ledger_id_cmp);
    *nOut = ctx->nIndex;
    return GMB_DB_OK;
}

int LedgerManager_TryGetIndex(const LedgerContext* ctx, int64_t txId, LedgerIndexRecord* out)
{
    int i;
    if (!ctx || !out) return 0;
    i = ledger_index_find(ctx, txId);
    if (i < 0) return 0;
    *out = ctx->indexRecs[i];
    return 1;
}

int LedgerManager_SaveIndexToFile(const char* indexPath, const LedgerContext* ctx)
{
    char* tmpPath;
    FILE* f;
    int i, n;
    size_t plen;
    if (!indexPath || !ctx) return GMB_DB_ARG;
    plen = strlen(indexPath);
    tmpPath = (char*)malloc(plen + 5);
    if (!tmpPath) return GMB_DB_ERR;
    memcpy(tmpPath, indexPath, plen);
    memcpy(tmpPath + plen, ".tmp", 5);
    f = fopen(tmpPath, "wb");
    if (!f) {
        free(tmpPath);
        return GMB_DB_ERR;
    }
    ledger_write_bw_string(f, "LDGIDX");
    ledger_write_le32(f, 1);
    ledger_write_le32(f, ctx->nIndex);
    for (i = 0; i < ctx->nIndex; i++) {
        ledger_write_le64(f, ctx->indexIds[i]);
        ledger_write_le64(f, ctx->indexRecs[i].FileOffset);
        ledger_write_le32(f, ctx->indexRecs[i].Date);
    }
    n = ferror(f) ? -1 : 0;
    fclose(f);
    if (n != 0) {
        unlink(tmpPath);
        free(tmpPath);
        return GMB_DB_ERR;
    }
    if (rename(tmpPath, indexPath) != 0) {
        unlink(tmpPath);
        free(tmpPath);
        return GMB_DB_ERR;
    }
    free(tmpPath);
    return GMB_DB_OK;
}

int LedgerManager_ReadIndexFromFile(const char* indexPath, LedgerContext* ctx)
{
    FILE* f;
    long len;
    char magic[16];
    int32_t version, count, date, i;
    int64_t txId, offset;
    if (!indexPath || !ctx) return GMB_DB_ARG;
    f = fopen(indexPath, "rb");
    if (!f) return GMB_DB_ERR;
    len = ledger_file_len(f);
    if (len < 14) {
        fclose(f);
        return GMB_DB_ERR;
    }
    if (ledger_read_bw_string(f, magic, (int)sizeof(magic)) != 0 || strcmp(magic, "LDGIDX") != 0) {
        fclose(f);
        return GMB_DB_ERR;
    }
    if (ledger_read_le32(f, &version) != 0 || version != 1) {
        fclose(f);
        return GMB_DB_ERR;
    }
    if (ledger_read_le32(f, &count) != 0 || count < 0) {
        fclose(f);
        return GMB_DB_ERR;
    }
    for (i = 0; i < count; i++) {
        if (ledger_read_le64(f, &txId) != 0 || ledger_read_le64(f, &offset) != 0 || ledger_read_le32(f, &date) != 0) {
            fclose(f);
            return GMB_DB_ERR;
        }
        if (ledger_index_put(ctx, txId, offset, date) != GMB_DB_OK) {
            fclose(f);
            return GMB_DB_ERR;
        }
    }
    fclose(f);
    return GMB_DB_OK;
}

int LedgerManager_IsTransactionBalanced(LedgerEntry* entries, int n)
{
    int i, implicitCount = 0, implicitIdx = -1;
    GmbDecimal sum = GmbDecimal_Zero();
    GmbDecimal eps = GmbDecimal_FromString("0.0001");
    if (n == 0) return 1;
    if (!entries) return 0;
    for (i = 0; i < n; i++) {
        double rateD = entries[i].ExchangeRate == 0.0 ? 1.0 : entries[i].ExchangeRate;
        GmbDecimal rate = GmbDecimal_FromDouble(rateD);
        if (!entries[i].AmountIsExplicit) {
            implicitCount++;
            implicitIdx = i;
        }
        sum = GmbDecimal_Add(sum, GmbDecimal_Mul(entries[i].Amount, rate));
    }
    if (implicitCount > 1) return 0;
    if (implicitCount != 1)
        return GmbDecimal_Cmp(GmbDecimal_Abs(sum), eps) < 0;
    entries[implicitIdx].Amount = GmbDecimal_Neg(sum);
    entries[implicitIdx].ExchangeRate = 1.0;
    return 1;
}

int LedgerManager_FormatAmountForJournal(GmbDecimal amount, const char* currency, double exchangeRate,
                                         char* out, int cap)
{
    char amt[64], rate[64];
    if (!out || cap < 8) return GMB_DB_ARG;
    ledger_fmt_num(amount, amt, (int)sizeof(amt));
    if (!currency || currency[0] == 0) {
        snprintf(out, (size_t)cap, "%s", amt);
        return GMB_DB_OK;
    }
    if (exchangeRate == 1.0) {
        snprintf(out, (size_t)cap, "%s %s", amt, currency);
        return GMB_DB_OK;
    }
    ledger_fmt_double(exchangeRate, rate, (int)sizeof(rate));
    snprintf(out, (size_t)cap, "%s %s @ %s", amt, currency, rate);
    return GMB_DB_OK;
}

/* Encoding.UTF8 StreamWriter constructed at Position 0: first Flush writes BOM at the
 * current stream position (after the optional last-byte seek). */
static int ledger_append_journal_text(const char* path, const char* text, long* outOffset)
{
    FILE* f;
    int extraNl;
    long last, newOffset;
    size_t tlen;
    static const unsigned char kUtf8Bom[3] = { 0xEF, 0xBB, 0xBF };
    if (!path || !text || !outOffset) return GMB_DB_ARG;
    tlen = strlen(text);
    f = fopen(path, "r+b");
    if (!f) f = fopen(path, "w+b");
    if (!f) return GMB_DB_ERR;
    extraNl = 0;
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return GMB_DB_ERR;
    }
    newOffset = ftell(f);
    if (newOffset > 0) {
        if (fseek(f, -1, SEEK_END) != 0) {
            fclose(f);
            return GMB_DB_ERR;
        }
        last = fgetc(f);
        extraNl = (last != '\n');
        newOffset = ftell(f);
    }
    if (fwrite(kUtf8Bom, 1, 3, f) != 3) {
        fclose(f);
        return GMB_DB_ERR;
    }
    if (extraNl && fputc('\n', f) == EOF) {
        fclose(f);
        return GMB_DB_ERR;
    }
    if (fwrite(text, 1, tlen, f) != tlen || fputc('\n', f) == EOF || fflush(f) != 0) {
        fclose(f);
        return GMB_DB_ERR;
    }
    fclose(f);
    *outOffset = newOffset;
    return GMB_DB_OK;
}

static int ledger_readline(FILE* f, char* buf, int cap)
{
    int n = 0, c;
    if (!f || cap < 2) return -1;
    c = fgetc(f);
    if (c == EOF) return -1;
    while (c != EOF && c != '\n') {
        if (c != '\r' && n < cap - 1)
            buf[n++] = (char)c;
        c = fgetc(f);
    }
    buf[n] = 0;
    return n;
}

static char* ledger_trim_inplace(char* s)
{
    char* e;
    unsigned char* u;
    if (!s) return s;
    u = (unsigned char*)s;
    /* .NET string.Trim() also strips U+FEFF (UTF-8 BOM decoded as a char). */
    while (*u && (isspace(*u) || (u[0] == 0xEF && u[1] == 0xBB && u[2] == 0xBF))) {
        if (u[0] == 0xEF && u[1] == 0xBB && u[2] == 0xBF) u += 3;
        else u++;
    }
    s = (char*)u;
    e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1]))
        *--e = 0;
    return s;
}

static int ledger_parse_date(const char* s, int* y, int* m, int* d)
{
    int Y = 0, M = 0, D = 0;
    if (!s || sscanf(s, "%d-%d-%d", &Y, &M, &D) != 3)
        return 0;
    if (Y < 1 || M < 1 || M > 12 || D < 1 || D > 31)
        return 0;
    *y = Y;
    *m = M;
    *d = D;
    return 1;
}

static int ledger_split_ws(char* line, char** parts, int max)
{
    int n = 0;
    char* p = line;
    while (*p && n < max) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;
        parts[n++] = p;
        while (*p && *p != ' ' && *p != '\t') p++;
        if (*p) *p++ = 0;
    }
    return n;
}

static void ledger_join_parts(char** parts, int start, int n, char* out, int cap)
{
    int i, pos = 0;
    out[0] = 0;
    for (i = start; i < n; i++) {
        int used = snprintf(out + pos, (size_t)(cap - pos), "%s%s", i == start ? "" : " ", parts[i]);
        if (used < 0 || used >= cap - pos) break;
        pos += used;
    }
}

/* C# MyAmountRegex = \s{2,}(.*) */
static int ledger_split_amount(const char* line, char* account, int acap, char* amount, int ncap)
{
    const char* p = line;
    int alen;
    account[0] = 0;
    amount[0] = 0;
    while (*p) {
        if (isspace((unsigned char)*p) && p[1] && isspace((unsigned char)p[1])) {
            alen = (int)(p - line);
            while (alen > 0 && isspace((unsigned char)line[alen - 1])) alen--;
            if (alen >= acap) alen = acap - 1;
            memcpy(account, line, (size_t)alen);
            account[alen] = 0;
            while (*p && isspace((unsigned char)*p)) p++;
            snprintf(amount, (size_t)ncap, "%s", p);
            return 1;
        }
        p++;
    }
    snprintf(account, (size_t)acap, "%s", line);
    return 0;
}

/* C#: LedgerManager.Extended.ParseAmountWithCurrency — sequential, no Regex. */
static void ledger_parse_amount_with_currency(const char* amountStr, GmbDecimal* amount, char* currency, int ccap,
                                              double* rate)
{
    char num[128];
    const char* s;
    const char* endNum;
    int nlen = 0;
    currency[0] = 0;
    *amount = GmbDecimal_Zero();
    *rate = 1.0;
    if (!amountStr) return;
    s = amountStr;
    while (*s && isspace((unsigned char)*s)) s++;
    if (!*s) return;
    if (*s == '+' || *s == '-') {
        if (nlen < (int)sizeof(num) - 1) num[nlen++] = *s;
        s++;
    }
    while (*s && (isdigit((unsigned char)*s) || *s == '.')) {
        if (nlen < (int)sizeof(num) - 1) num[nlen++] = *s;
        s++;
    }
    num[nlen] = 0;
    endNum = s;
    while (*s && isspace((unsigned char)*s)) s++;
    if (isalpha((unsigned char)s[0]) && isalpha((unsigned char)s[1]) && isalpha((unsigned char)s[2])
        && (s[3] == 0 || isspace((unsigned char)s[3]))) {
        if (ccap > 3) {
            currency[0] = s[0];
            currency[1] = s[1];
            currency[2] = s[2];
            currency[3] = 0;
        }
        s += 3;
        while (*s && isspace((unsigned char)*s)) s++;
        if (*s == '@') {
            char* end = NULL;
            s++;
            while (*s && isspace((unsigned char)*s)) s++;
            *rate = strtod(s, &end);
            if (end == s) *rate = 1.0;
        }
        *amount = GmbDecimal_FromString(num);
        (void)endNum;
        return;
    }
    *amount = GmbDecimal_FromString(amountStr);
}

static int ledger_skip_bom(FILE* f)
{
    long pos = ftell(f);
    unsigned char b[3];
    if (pos < 0) return 0;
    if (fread(b, 1, 3, f) != 3) {
        fseek(f, pos, SEEK_SET);
        return 0;
    }
    if (b[0] == 0xEF && b[1] == 0xBB && b[2] == 0xBF) return 1;
    fseek(f, pos, SEEK_SET);
    return 0;
}

static int ledger_parse_transaction_block(FILE* f, int64_t txId, LedgerEntry* out, int cap, int* nOut)
{
    char raw[1024], headerCopy[1024];
    char* header;
    char* parts[32];
    int nparts, y = 0, m = 0, d = 0, i, n = 0, peek;
    char status = ' ';
    char description[512];
    if (!f || !out || !nOut || cap < 1) return GMB_DB_ARG;
    *nOut = 0;
    if (ledger_readline(f, raw, (int)sizeof(raw)) < 0) return GMB_DB_ERR;
    header = ledger_trim_inplace(raw);
    if (!header[0]) return GMB_DB_ERR;
    snprintf(headerCopy, sizeof(headerCopy), "%s", header);
    nparts = ledger_split_ws(headerCopy, parts, 32);
    if (nparts < 1 || !ledger_parse_date(parts[0], &y, &m, &d)) return GMB_DB_ERR;
    i = 1;
    if (nparts > 1 && parts[1][0] && parts[1][1] == 0 && (parts[1][0] == '*' || parts[1][0] == '!')) {
        status = parts[1][0];
        i = 2;
    }
    ledger_join_parts(parts, i, nparts, description, (int)sizeof(description));

    for (;;) {
        char line[1024], work[1024], *trimmed, account[256], amountStr[256], comment[256];
        char cur[8];
        GmbDecimal amt = GmbDecimal_Zero();
        double rate = 1.0;
        int explicitAmt, ci;
        peek = fgetc(f);
        if (peek == EOF) break;
        ungetc(peek, f);
        if (peek != ' ' && peek != '\t') break;
        if (ledger_readline(f, line, (int)sizeof(line)) < 0) break;
        snprintf(work, sizeof(work), "%s", line);
        trimmed = ledger_trim_inplace(work);
        if (!trimmed[0]) continue;
        comment[0] = 0;
        ci = 0;
        while (trimmed[ci] && trimmed[ci] != ';') ci++;
        if (trimmed[ci] == ';') {
            snprintf(comment, sizeof(comment), "%s", trimmed + ci + 1);
            ledger_trim_inplace(comment);
            trimmed[ci] = 0;
            ledger_trim_inplace(trimmed);
        }
        if (ledger_split_amount(trimmed, account, (int)sizeof(account), amountStr, (int)sizeof(amountStr))
            && amountStr[0]) {
            ledger_parse_amount_with_currency(amountStr, &amt, cur, (int)sizeof(cur), &rate);
            explicitAmt = 1;
        } else {
            snprintf(account, sizeof(account), "%s", trimmed);
            cur[0] = 0;
            explicitAmt = 0;
            amt = GmbDecimal_Zero();
            rate = 1.0;
        }
        if (n >= cap) return GMB_DB_ARG;
        memset(&out[n], 0, sizeof(out[n]));
        out[n].TransactionId = txId;
        out[n].DateY = y;
        out[n].DateM = m;
        out[n].DateD = d;
        out[n].Status = status;
        out[n].Description = ledger_dup(description);
        out[n].Account = ledger_dup(account);
        out[n].Amount = amt;
        out[n].Comment = comment[0] ? ledger_dup(comment) : NULL;
        out[n].AmountIsExplicit = explicitAmt;
        out[n].Currency = cur[0] ? ledger_dup(cur) : NULL;
        out[n].ExchangeRate = rate;
        n++;
    }
    if (!LedgerManager_IsTransactionBalanced(out, n)) {
        for (i = 0; i < n; i++) ledger_entry_clear_owned(&out[i]);
        return GMB_DB_ERR;
    }
    for (i = 0; i < n; i++) out[i].TransactionId = txId;
    *nOut = n;
    return GMB_DB_OK;
}

int64_t LedgerManager_AddTransaction(LedgerContext* ctx, int year, int month, int day,
                                     const char* description, LedgerEntry* entries, int n)
{
    char text[GMB_LEDGER_TX_TEXT];
    char amt[128];
    int pos, i;
    long newOffset;
    int64_t txId;
    int32_t dateAsInt;

    if (!ctx || !description || !entries || n < 1) return GMB_DB_ARG;
    if (!LedgerManager_IsTransactionBalanced(entries, n)) return GMB_DB_ERR;

    pos = snprintf(text, sizeof(text), "%04d-%02d-%02d %s", year, month, day, description);
    if (pos < 0 || pos >= (int)sizeof(text)) return GMB_DB_ERR;
    for (i = 0; i < n; i++) {
        int used;
        if (!entries[i].Account) return GMB_DB_ARG;
        if (LedgerManager_FormatAmountForJournal(entries[i].Amount, entries[i].Currency,
                                                 entries[i].ExchangeRate == 0.0 ? 1.0 : entries[i].ExchangeRate,
                                                 amt, (int)sizeof(amt)) != GMB_DB_OK)
            return GMB_DB_ERR;
        used = snprintf(text + pos, sizeof(text) - (size_t)pos, "\n    %s      %s", entries[i].Account, amt);
        if (used < 0 || used >= (int)sizeof(text) - pos) return GMB_DB_ERR;
        pos += used;
    }

    pthread_mutex_lock(&ctx->Lock);
    if (ledger_append_journal_text(ctx->JournalFilePath, text, &newOffset) != GMB_DB_OK) {
        pthread_mutex_unlock(&ctx->Lock);
        return GMB_DB_ERR;
    }
    txId = ledger_index_max_id(ctx) + 1;
    dateAsInt = year * 10000 + month * 100 + day;
    if (ledger_index_put(ctx, txId, newOffset, dateAsInt) != GMB_DB_OK) {
        pthread_mutex_unlock(&ctx->Lock);
        return GMB_DB_ERR;
    }
    ctx->IsDirty = 1;
    pthread_mutex_unlock(&ctx->Lock);
    return txId;
}

int LedgerManager_GetTransaction(LedgerContext* ctx, int64_t txId, LedgerEntry* out, int cap, int* nOut)
{
    int slot;
    LedgerIndexRecord rec;
    FILE* f;
    LedgerTxCache tmp;
    if (!ctx || !out || !nOut || cap < 1) return GMB_DB_ARG;
    *nOut = 0;
    slot = ledger_cache_find(ctx, txId);
    if (slot >= 0) {
        if (ctx->cache[slot].n > cap) return GMB_DB_ARG;
        memcpy(out, ctx->cache[slot].e, (size_t)ctx->cache[slot].n * sizeof(LedgerEntry));
        *nOut = ctx->cache[slot].n;
        return GMB_DB_OK;
    }
    if (!LedgerManager_TryGetIndex(ctx, txId, &rec) || rec.FileOffset == -1)
        return GMB_DB_ERR;
    f = fopen(ctx->JournalFilePath, "rb");
    if (!f) return GMB_DB_ERR;
    if (fseek(f, (long)rec.FileOffset, SEEK_SET) != 0) {
        fclose(f);
        return GMB_DB_ERR;
    }
    ledger_skip_bom(f);
    memset(&tmp, 0, sizeof(tmp));
    if (ledger_parse_transaction_block(f, txId, tmp.e, GMB_LEDGER_MAX_POSTINGS, &tmp.n) != GMB_DB_OK) {
        fclose(f);
        ledger_cache_slot_free(&tmp);
        return GMB_DB_ERR;
    }
    fclose(f);
    tmp.txId = txId;
    if (ctx->nCache >= ctx->capCache && ledger_cache_grow(ctx) != GMB_DB_OK) {
        ledger_cache_slot_free(&tmp);
        return GMB_DB_ERR;
    }
    ctx->cache[ctx->nCache] = tmp;
    slot = ctx->nCache++;
    if (ctx->cache[slot].n > cap) return GMB_DB_ARG;
    memcpy(out, ctx->cache[slot].e, (size_t)ctx->cache[slot].n * sizeof(LedgerEntry));
    *nOut = ctx->cache[slot].n;
    return GMB_DB_OK;
}

/* ── Editing del journal (DELETE/EDIT in place) ─────────────────────────────────
 * Il journal è una sequenza di blocchi a offset noti nell'indice. DELETE marca il
 * blocco come storno (`YYYY-MM-DD * desc`, convenzione Ledger standard) e lo toglie
 * dall'indice attivo; EDIT sostituisce il blocco mantenendo lo STESSO id. In entrambi
 * i casi gli offset dei blocchi successivi vengono riallineati. Lo storno resta nel
 * journal (storia) ma non è mai una transazione: Rebuild/Pack lo ignorano. */

static long ledger_path_len(const char* path)
{
    FILE* f;
    long n;
    if (!path) return -1;
    f = fopen(path, "rb");
    if (!f) return -1;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return -1; }
    n = ftell(f);
    fclose(f);
    return n;
}

static int ledger_read_range(const char* path, long off, char* buf, long len)
{
    FILE* f;
    if (!path || !buf || len < 0) return GMB_DB_ARG;
    f = fopen(path, "rb");
    if (!f) return GMB_DB_ERR;
    if (fseek(f, off, SEEK_SET) != 0) { fclose(f); return GMB_DB_ERR; }
    if (len > 0 && (long)fread(buf, 1, (size_t)len, f) != len) { fclose(f); return GMB_DB_ERR; }
    fclose(f);
    return GMB_DB_OK;
}

/* 1 se l'header del blocco è uno STORNO: `YYYY-MM-DD * ...`. */
static int ledger_header_is_void(const char* line)
{
    char tmp[512];
    char* t;
    char* p;
    int y = 0, m = 0, d = 0;
    if (!line) return 0;
    snprintf(tmp, sizeof(tmp), "%s", line);
    t = ledger_trim_inplace(tmp);
    if (!t[0]) return 0;
    p = t;
    while (*p && !isspace((unsigned char)*p)) p++;
    if (*p) { *p = 0; p++; }
    if (!ledger_parse_date(t, &y, &m, &d)) return 0;
    while (*p && isspace((unsigned char)*p)) p++;
    return *p == '*' && (p[1] == 0 || isspace((unsigned char)p[1]));
}

/* Fine del blocco che inizia a `start`: inizio del blocco indicizzato successivo o EOF. */
static long ledger_block_end(const LedgerContext* ctx, long start, long fileLen)
{
    long end = fileLen;
    int i;
    for (i = 0; i < ctx->nIndex; i++) {
        long off = ctx->indexRecs[i].FileOffset;
        if (off > start && off < end) end = off;
    }
    return end;
}

/* Testo del blocco (header + postings), senza BOM né newline finale. */
static int ledger_build_tx_text(int year, int month, int day, const char* description,
                                LedgerEntry* entries, int n, char* text, int cap)
{
    char amt[128];
    int pos, i;
    if (!text || cap < 32 || !description || !entries || n < 1) return GMB_DB_ARG;
    pos = snprintf(text, (size_t)cap, "%04d-%02d-%02d %s", year, month, day, description);
    if (pos < 0 || pos >= cap) return GMB_DB_ERR;
    for (i = 0; i < n; i++) {
        int used;
        if (!entries[i].Account) return GMB_DB_ARG;
        if (LedgerManager_FormatAmountForJournal(entries[i].Amount, entries[i].Currency,
                                                 entries[i].ExchangeRate == 0.0 ? 1.0 : entries[i].ExchangeRate,
                                                 amt, (int)sizeof(amt)) != GMB_DB_OK)
            return GMB_DB_ERR;
        used = snprintf(text + pos, (size_t)(cap - pos), "\n    %s      %s", entries[i].Account, amt);
        if (used < 0 || used >= cap - pos) return GMB_DB_ERR;
        pos += used;
    }
    return GMB_DB_OK;
}

/* Riscrive il journal sostituendo i byte [start,end) con `repl` (replLen; 0 = rimozione).
 * Gli offset > end sono spostati di delta; il record `fixedId` (il blocco riscritto)
 * resta a `start`. La cache viene svuotata (cambiano gli offset). */
static int ledger_rewrite_journal(LedgerContext* ctx, long start, long end,
                                  const char* repl, long replLen, int64_t fixedId)
{
    FILE* f;
    char* in;
    char* out;
    long len, outLen, delta, i, nTail;
    char tmpPath[PATH_MAX];
    int rc = GMB_DB_OK;
    if (!ctx || start < 0 || end < start || replLen < 0) return GMB_DB_ARG;
    len = ledger_path_len(ctx->JournalFilePath);
    if (len < 0 || end > len) return GMB_DB_ERR;
    in = (char*)malloc((size_t)len + 1);
    if (!in) return GMB_DB_ERR;
    if (ledger_read_range(ctx->JournalFilePath, 0, in, len) != GMB_DB_OK) { free(in); return GMB_DB_ERR; }
    nTail = len - end;
    outLen = start + replLen + nTail;
    out = (char*)malloc((size_t)outLen + 1);
    if (!out) { free(in); return GMB_DB_ERR; }
    memcpy(out, in, (size_t)start);
    if (replLen > 0 && repl) memcpy(out + start, repl, (size_t)replLen);
    if (nTail > 0) memcpy(out + start + replLen, in + end, (size_t)nTail);
    free(in);
    snprintf(tmpPath, sizeof(tmpPath), "%s.tmp", ctx->JournalFilePath);
    f = fopen(tmpPath, "wb");
    if (!f) { free(out); return GMB_DB_ERR; }
    if (outLen > 0 && fwrite(out, 1, (size_t)outLen, f) != (size_t)outLen) rc = GMB_DB_ERR;
    if (rc == GMB_DB_OK && fflush(f) != 0) rc = GMB_DB_ERR;
    fclose(f);
    free(out);
    if (rc != GMB_DB_OK || rename(tmpPath, ctx->JournalFilePath) != 0) {
        unlink(tmpPath);
        return GMB_DB_ERR;
    }
    delta = replLen - (end - start);
    for (i = 0; i < ctx->nIndex; i++) {
        if (ctx->indexIds[i] == fixedId) continue;
        if (ctx->indexRecs[i].FileOffset >= end) ctx->indexRecs[i].FileOffset += delta;
    }
    ledger_cache_clear(ctx);
    return GMB_DB_OK;
}

int LedgerManager_DeleteTransaction(LedgerContext* ctx, int64_t txId)
{
    LedgerIndexRecord rec;
    char* block = NULL;
    char* marked = NULL;
    long fileLen, end, blen, i, nl, insPos = -1, h;
    int rc = 0, alreadyVoid = 0;
    if (!ctx) return 0;
    pthread_mutex_lock(&ctx->Lock);
    if (!LedgerManager_TryGetIndex(ctx, txId, &rec) || rec.FileOffset < 0) goto done;
    fileLen = ledger_path_len(ctx->JournalFilePath);
    if (fileLen < 0) goto done;
    end = ledger_block_end(ctx, rec.FileOffset, fileLen);
    blen = end - rec.FileOffset;
    if (blen <= 0) goto done;
    block = (char*)malloc((size_t)blen + 1);
    if (!block || ledger_read_range(ctx->JournalFilePath, rec.FileOffset, block, blen) != GMB_DB_OK) goto done;
    block[blen] = 0;
    for (nl = 0; nl < blen && block[nl] != '\n'; nl++) { }
    /* posizione subito dopo la DATA (il primo spazio dell'header) */
    h = 0;
    if (blen > 2 && (unsigned char)block[0] == 0xEF && (unsigned char)block[1] == 0xBB
        && (unsigned char)block[2] == 0xBF) h = 3;
    for (i = h; i < nl; i++) {
        if (block[i] == ' ' || block[i] == '\t') { insPos = i + 1; break; }
    }
    if (insPos < 0) goto done;
    for (i = insPos; i < nl && (block[i] == ' ' || block[i] == '\t'); i++) { }
    if (i < nl && block[i] == '*') alreadyVoid = 1;
    if (!alreadyVoid) {
        marked = (char*)malloc((size_t)blen + 3);
        if (!marked) goto done;
        memcpy(marked, block, (size_t)insPos);
        marked[insPos] = '*';
        marked[insPos + 1] = ' ';
        memcpy(marked + insPos + 2, block + insPos, (size_t)(blen - insPos));
        if (ledger_rewrite_journal(ctx, rec.FileOffset, end, marked, blen + 2, txId) != GMB_DB_OK) goto done;
    }
    /* rimuove l'id dall'indice ATTIVO (lo storno resta nel journal come storia ma non
     * è una transazione: COUNT/VARIANCE/BALANCE/REGISTER non lo vedono). */
    for (i = 0; i < ctx->nIndex; i++) {
        if (ctx->indexIds[i] == txId) {
            memmove(&ctx->indexIds[i], &ctx->indexIds[i + 1],
                    (size_t)(ctx->nIndex - i - 1) * sizeof(int64_t));
            memmove(&ctx->indexRecs[i], &ctx->indexRecs[i + 1],
                    (size_t)(ctx->nIndex - i - 1) * sizeof(LedgerIndexRecord));
            ctx->nIndex--;
            break;
        }
    }
    ctx->IsDirty = 1;
    rc = 1;
done:
    free(block);
    free(marked);
    pthread_mutex_unlock(&ctx->Lock);
    return rc;
}

/* EDIT: sostituisce il blocco del journal MANTENENDO lo stesso id (guida §LEDGER EDIT
 * "preserva id e categoria"). Prima era delete+add: nuovo id e COUNT incrementato. */
int64_t LedgerManager_UpdateTransaction(LedgerContext* ctx, int64_t txId, int year, int month, int day,
                                        const char* description, LedgerEntry* entries, int n)
{
    LedgerIndexRecord rec;
    char text[GMB_LEDGER_TX_TEXT];
    char* repl = NULL;
    long fileLen, end, tlen, rlen;
    int64_t rc = -1;
    static const char kBom[3] = { (char)0xEF, (char)0xBB, (char)0xBF };
    if (!ctx || !entries || n < 1) return GMB_DB_ARG;
    if (!LedgerManager_IsTransactionBalanced(entries, n)) return GMB_DB_ERR;
    pthread_mutex_lock(&ctx->Lock);
    if (!LedgerManager_TryGetIndex(ctx, txId, &rec) || rec.FileOffset < 0) goto done;
    fileLen = ledger_path_len(ctx->JournalFilePath);
    if (fileLen < 0) goto done;
    end = ledger_block_end(ctx, rec.FileOffset, fileLen);
    if (end <= rec.FileOffset) goto done;
    if (ledger_build_tx_text(year, month, day, description, entries, n, text, (int)sizeof(text)) != GMB_DB_OK)
        goto done;
    tlen = (long)strlen(text);
    rlen = 3 + tlen + 1;
    repl = (char*)malloc((size_t)rlen + 1);
    if (!repl) goto done;
    memcpy(repl, kBom, 3);
    memcpy(repl + 3, text, (size_t)tlen);
    repl[3 + tlen] = '\n';
    if (ledger_rewrite_journal(ctx, rec.FileOffset, end, repl, rlen, txId) != GMB_DB_OK) goto done;
    if (ledger_index_put(ctx, txId, rec.FileOffset, year * 10000 + month * 100 + day) != GMB_DB_OK)
        goto done;
    ctx->IsDirty = 1;
    rc = txId;
done:
    free(repl);
    pthread_mutex_unlock(&ctx->Lock);
    return rc;
}

static int ledger_line_has_date(const char* line)
{
    char tmp[1024];
    char* t;
    char* sp;
    int y = 0, m = 0, d = 0;
    snprintf(tmp, sizeof(tmp), "%s", line ? line : "");
    t = ledger_trim_inplace(tmp);    if (!t[0] || !isdigit((unsigned char)t[0])) return 0;
    sp = t;
    while (*sp && *sp != ' ') sp++;
    if (*sp) *sp = 0;
    return ledger_parse_date(t, &y, &m, &d);
}

/* C#: ReadTransactionBlockAsText — unread next header via Seek; we use ftell/fseek
 * instead of UTF8.GetByteCount+NewLine (StreamReader buffer desync). */
static int ledger_read_block_as_text(FILE* f, char* out, int cap)
{
    char line[2048];
    int n, pos = 0;
    long unread;
    if (!f || !out || cap < 2) return -1;
    out[0] = 0;
    if (ledger_readline(f, line, (int)sizeof(line)) < 0) return 0;
    n = snprintf(out, (size_t)cap, "%s\n", line);
    if (n < 0 || n >= cap) return -1;
    pos = n;
    for (;;) {
        char tmp[2048];
        char* t;
        unread = ftell(f);
        if (unread < 0) return -1;
        if (ledger_readline(f, line, (int)sizeof(line)) < 0) break;
        snprintf(tmp, sizeof(tmp), "%s", line);
        t = ledger_trim_inplace(tmp);
        if (!t[0]) break;
        if (isdigit((unsigned char)t[0]) && ledger_line_has_date(t)) {
            fseek(f, unread, SEEK_SET);
            break;
        }
        n = snprintf(out + pos, (size_t)cap - (size_t)pos, "%s\n", line);
        if (n < 0 || n >= cap - pos) return -1;
        pos += n;
    }
    {
        char* s = ledger_trim_inplace(out);
        if (s != out) memmove(out, s, strlen(s) + 1);
    }
    return 1;
}

int LedgerManager_RebuildIndexFromFile(const char* journalPath, LedgerContext* ctx)
{
    FILE* f;
    char line[2048];
    long currentOffset = 0;
    int64_t txId = 0;
    if (!journalPath || !ctx) return GMB_DB_ARG;
    ledger_index_clear(ctx);
    f = fopen(journalPath, "rb");
    if (!f) return GMB_DB_OK;
    ledger_skip_bom(f);
    while (ledger_readline(f, line, (int)sizeof(line)) >= 0) {
        char tmp[2048];
        char* t;
        int y = 0, m = 0, d = 0;
        snprintf(tmp, sizeof(tmp), "%s", line);
        t = ledger_trim_inplace(tmp);
        /* C# checks line[0] on the raw StreamReader string; after a leading file BOM
         * that is already consumed. Mid-file AddTransaction BOMs decode as U+FEFF —
         * we skip BOM before the digit check so every header is found. */
        if (t[0] && strchr(";#%*", t[0]) == NULL && isdigit((unsigned char)t[0])
            && ledger_parse_date(t, &y, &m, &d)) {
            /* Blocco di STORNO (`date * …`): non è una transazione attiva. */
            if (ledger_header_is_void(t)) { currentOffset = ftell(f); continue; }
            if (ledger_index_put(ctx, ++txId, currentOffset, y * 10000 + m * 100 + d) != GMB_DB_OK) {
                fclose(f);
                return GMB_DB_ERR;
            }
        }
        currentOffset = ftell(f);
    }
    fclose(f);
    return GMB_DB_OK;
}

int LedgerManager_PackJournal(LedgerContext* ctx)
{
    char packed[PATH_MAX];
    char block[GMB_LEDGER_TX_TEXT];
    FILE *in, *out;
    long offset;
    int64_t nextId;
    static const unsigned char kUtf8Bom[3] = { 0xEF, 0xBB, 0xBF };
    if (!ctx) return GMB_DB_ARG;
    pthread_mutex_lock(&ctx->Lock);
    snprintf(packed, sizeof(packed), "%s.packed", ctx->JournalFilePath);
    in = fopen(ctx->JournalFilePath, "rb");
    if (!in) {
        pthread_mutex_unlock(&ctx->Lock);
        return GMB_DB_ERR;
    }
    out = fopen(packed, "wb");
    if (!out) {
        fclose(in);
        pthread_mutex_unlock(&ctx->Lock);
        return GMB_DB_ERR;
    }
    if (fwrite(kUtf8Bom, 1, 3, out) != 3) {
        fclose(in);
        fclose(out);
        unlink(packed);
        pthread_mutex_unlock(&ctx->Lock);
        return GMB_DB_ERR;
    }
    ledger_skip_bom(in);
    ledger_index_clear(ctx);
    ledger_cache_clear(ctx);
    offset = 0;
    nextId = 1;
    for (;;) {
        int rc = ledger_read_block_as_text(in, block, (int)sizeof(block));
        size_t blen;
        if (rc < 0) {
            fclose(in);
            fclose(out);
            unlink(packed);
            pthread_mutex_unlock(&ctx->Lock);
            return GMB_DB_ERR;
        }
        if (rc == 0) break;
        {
            char* t = block;
            while (*t && isspace((unsigned char)*t)) t++;
            if (!*t) continue;
            /* Compaction: gli storni non sono transazioni → non copiati né indicizzati. */
            if (ledger_header_is_void(t)) continue;
        }
        /* ponytail: Flush per blocco così FileOffset è reale. C# legge BaseStream.Position
         * senza Flush (buffer StreamWriter → offset spesso 0). */
        if (ledger_index_put(ctx, nextId, offset, 0) != GMB_DB_OK) {
            fclose(in);
            fclose(out);
            unlink(packed);
            pthread_mutex_unlock(&ctx->Lock);
            return GMB_DB_ERR;
        }
        blen = strlen(block);
        if (fwrite(block, 1, blen, out) != blen) {
            fclose(in);
            fclose(out);
            unlink(packed);
            pthread_mutex_unlock(&ctx->Lock);
            return GMB_DB_ERR;
        }
        if (blen == 0 || block[blen - 1] != '\n') {
            if (fputc('\n', out) == EOF) {
                fclose(in);
                fclose(out);
                unlink(packed);
                pthread_mutex_unlock(&ctx->Lock);
                return GMB_DB_ERR;
            }
        }
        fflush(out);
        offset = ftell(out);
        nextId++;
    }
    fclose(in);
    fclose(out);
    if (rename(packed, ctx->JournalFilePath) != 0) {
        unlink(packed);
        pthread_mutex_unlock(&ctx->Lock);
        return GMB_DB_ERR;
    }
    if (ctx->IndexFilePath) unlink(ctx->IndexFilePath);
    if (LedgerManager_SaveIndexToFile(ctx->IndexFilePath, ctx) != GMB_DB_OK) {
        pthread_mutex_unlock(&ctx->Lock);
        return GMB_DB_ERR;
    }
    ctx->IsDirty = 0;
    pthread_mutex_unlock(&ctx->Lock);
    return GMB_DB_OK;
}

int LedgerManager_ParseLedgerQuery(const char* query, LedgerQueryFilter* out)
{
    const char *accAt, *dateAt, *p, *end;
    size_t n;
    if (!out) return GMB_DB_ARG;
    memset(out, 0, sizeof(*out));
    if (ledger_is_blank(query)) return GMB_DB_OK;

    accAt = strstr(query, "account:");
    if (accAt) {
        p = accAt + 8;
        if (*p == '"') {
            p++;
            end = strchr(p, '"');
            n = end ? (size_t)(end - p) : strlen(p);
        } else {
            end = p;
            while (*end && !isspace((unsigned char)*end)) end++;
            n = (size_t)(end - p);
        }
        if (n >= GMB_LEDGER_NAME) n = GMB_LEDGER_NAME - 1;
        if (n > 0) {
            memcpy(out->accountPat, p, n);
            out->accountPat[n] = 0;
            out->hasAccount = 1;
        }
    }

    dateAt = strstr(query, "date:");
    if (dateAt) {
        p = dateAt + 5;
        if (p[0] == '>' && p[1] == '=') {
            memcpy(out->dateOp, ">=", 3);
            p += 2;
        } else if (p[0] == '<' && p[1] == '=') {
            memcpy(out->dateOp, "<=", 3);
            p += 2;
        } else if (p[0] == '>' || p[0] == '<' || p[0] == '=') {
            out->dateOp[0] = p[0];
            out->dateOp[1] = 0;
            p++;
        }
        if (out->dateOp[0] && ledger_parse_ymd(p, &out->dateVal))
            out->hasDate = 1;
        else
            out->dateOp[0] = 0;
    }
    return GMB_DB_OK;
}

int LedgerManager_LoadChartOfAccounts(const char* coaPath, LedgerContext* ctx)
{
    FILE* f;
    char line[512];
    char *sep, *tab, *account, *desc, *colon;
    int slot;
    if (!coaPath || !ctx) return GMB_DB_ARG;
    f = fopen(coaPath, "r");
    if (!f) return GMB_DB_ERR;
    while (fgets(line, (int)sizeof(line), f)) {
        ledger_rtrim(line);
        if (ledger_is_blank(line)) continue;
        if (strchr(";#%*", line[0])) continue;
        sep = strstr(line, "  ");
        tab = strchr(line, '\t');
        if (tab && (!sep || tab < sep)) sep = tab;
        desc = "";
        if (sep) {
            if (*sep == '\t') {
                *sep = 0;
                desc = sep + 1;
            } else {
                *sep = 0;
                desc = sep + 2;
            }
            while (*desc == ' ' || *desc == '\t') desc++;
            ledger_rtrim(desc);
        }
        account = line;
        while (isspace((unsigned char)*account)) account++;
        ledger_rtrim(account);
        if (!*account) continue;
        colon = strchr(account, ':');
        slot = ledger_coa_find(ctx, account);
        if (slot < 0) {
            if (ctx->nCoa >= GMB_LEDGER_MAX_COA) {
                fclose(f);
                return GMB_DB_ERR;
            }
            slot = ctx->nCoa++;
        }
        snprintf(ctx->coaName[slot], GMB_LEDGER_NAME, "%s", account);
        snprintf(ctx->coaType[slot], GMB_LEDGER_TYPE, "%s",
                 colon ? "" : "Senza categoria");
        if (colon) {
            size_t tlen = (size_t)(colon - account);
            if (tlen >= GMB_LEDGER_TYPE) tlen = GMB_LEDGER_TYPE - 1;
            memcpy(ctx->coaType[slot], account, tlen);
            ctx->coaType[slot][tlen] = 0;
        }
        snprintf(ctx->coaDesc[slot], GMB_LEDGER_DESC, "%s", desc);
    }
    fclose(f);
    return GMB_DB_OK;
}

int LedgerManager_GetFilteredEntries(LedgerContext* ctx, const char* query, LedgerEntry* out, int cap, int* nOut)
{
    LedgerQueryFilter filters;
    LedgerQueryRec* recs = NULL;
    LedgerEntry tx[GMB_LEDGER_MAX_POSTINGS];
    int nRec = 0, i, t, tn, n = 0;
    if (!ctx || !out || !nOut || cap < 1) return GMB_DB_ARG;
    *nOut = 0;
    if (LedgerManager_ParseLedgerQuery(query, &filters) != GMB_DB_OK) return GMB_DB_ARG;
    if (ctx->nIndex > 0) {
        recs = (LedgerQueryRec*)malloc((size_t)ctx->nIndex * sizeof(LedgerQueryRec));
        if (!recs) return GMB_DB_ERR;
    }
    for (i = 0; i < ctx->nIndex; i++) {
        if (ctx->indexRecs[i].FileOffset == -1) continue;
        if (!ledger_date_match(&filters, ctx->indexRecs[i].Date)) continue;
        recs[nRec].id = ctx->indexIds[i];
        recs[nRec].date = ctx->indexRecs[i].Date;
        nRec++;
    }
    if (nRec > 1) qsort(recs, (size_t)nRec, sizeof(recs[0]), ledger_query_rec_cmp);
    for (i = 0; i < nRec; i++) {
        tn = 0;
        if (LedgerManager_GetTransaction(ctx, recs[i].id, tx, GMB_LEDGER_MAX_POSTINGS, &tn) != GMB_DB_OK) {
            free(recs);
            return GMB_DB_ERR;
        }
        for (t = 0; t < tn; t++) {
            if (filters.hasAccount && !ledger_glob_ci(tx[t].Account ? tx[t].Account : "", filters.accountPat))
                continue;
            if (n >= cap) {
                free(recs);
                return GMB_DB_ARG;
            }
            out[n++] = tx[t];
        }
    }
    free(recs);
    *nOut = n;
    return GMB_DB_OK;
}

int LedgerManager_GetAccounts(LedgerContext* ctx, const char* rootAccount,
                              char names[][GMB_LEDGER_NAME], int maxNames, int* nOut)
{
    char tmp[GMB_LEDGER_MAX_COA][GMB_LEDGER_NAME];
    int nTmp = 0, i, j, n = 0;
    int hasRoot;
    if (!ctx || !names || !nOut || maxNames < 1) return GMB_DB_ARG;
    *nOut = 0;
    hasRoot = !ledger_is_blank(rootAccount);
    if (ctx->nCoa > 0) {
        for (i = 0; i < ctx->nCoa; i++)
            snprintf(tmp[nTmp++], GMB_LEDGER_NAME, "%s", ctx->coaName[i]);
    } else {
        LedgerEntry* entries;
        int ne = 0;
        int capF = ledger_filtered_cap(ctx);
        entries = (LedgerEntry*)malloc(sizeof(LedgerEntry) * (size_t)capF);
        if (!entries) return GMB_DB_ERR;
        if (LedgerManager_GetFilteredEntries(ctx, "", entries, capF, &ne) != GMB_DB_OK) {
            free(entries);
            return GMB_DB_ERR;
        }
        for (i = 0; i < ne && nTmp < GMB_LEDGER_MAX_COA; i++) {
            const char* acc = entries[i].Account ? entries[i].Account : "";
            int seen = 0;
            for (j = 0; j < nTmp; j++) {
                if (gmb_strcasecmp(tmp[j], acc) == 0) {
                    seen = 1;
                    break;
                }
            }
            if (!seen)
                snprintf(tmp[nTmp++], GMB_LEDGER_NAME, "%s", acc);
        }
        free(entries);
    }
    qsort(tmp, (size_t)nTmp, sizeof(tmp[0]), ledger_name_cmp);
    for (i = 0; i < nTmp; i++) {
        char* rootTrim;
        char rootBuf[GMB_LEDGER_NAME];
        if (hasRoot) {
            snprintf(rootBuf, sizeof(rootBuf), "%s", rootAccount);
            rootTrim = rootBuf;
            while (isspace((unsigned char)*rootTrim)) rootTrim++;
            ledger_rtrim(rootTrim);
            if (!ledger_is_under_root(tmp[i], rootTrim)) continue;
        }
        if (n >= maxNames) return GMB_DB_ARG;
        snprintf(names[n++], GMB_LEDGER_NAME, "%s", tmp[i]);
    }
    *nOut = n;
    return GMB_DB_OK;
}

int LedgerManager_SetBudget(LedgerContext* ctx, const char* account, GmbDecimal amount, const char* period)
{
    char a[GMB_LEDGER_NAME], p[16];
    int slot;
    if (!ctx) return GMB_DB_ARG;
    ledger_copy_trim(a, sizeof(a), account);
    ledger_copy_trim(p, sizeof(p), period);
    if (!a[0] || !p[0]) return GMB_DB_ARG;
    slot = ledger_budget_find(ctx, a, p);
    if (slot < 0) {
        if (ctx->nBudget >= GMB_LEDGER_MAX_COA) return GMB_DB_ERR;
        slot = ctx->nBudget++;
    }
    snprintf(ctx->budgetAccount[slot], GMB_LEDGER_NAME, "%s", a);
    snprintf(ctx->budgetPeriod[slot], sizeof(ctx->budgetPeriod[slot]), "%s", p);
    ctx->budgetAmount[slot] = amount;
    ctx->BudgetsDirty = 1;
    return GMB_DB_OK;
}

int LedgerManager_LoadBudgets(const char* budgetPath, LedgerContext* ctx)
{
    FILE* f;
    char line[512];
    char *acc, *amtStr, *per, *rest;
    if (!budgetPath || !ctx) return GMB_DB_ARG;
    ctx->nBudget = 0;
    ctx->BudgetsDirty = 0;
    f = fopen(budgetPath, "r");
    if (!f) return GMB_DB_OK;
    while (fgets(line, (int)sizeof(line), f)) {
        ledger_rtrim(line);
        if (ledger_is_blank(line)) continue;
        if (strchr(";#%*", line[0])) continue;
        acc = line;
        while (*acc && isspace((unsigned char)*acc)) acc++;
        amtStr = acc;
        while (*amtStr && !isspace((unsigned char)*amtStr)) amtStr++;
        if (!*amtStr) continue;
        *amtStr++ = 0;
        while (*amtStr && isspace((unsigned char)*amtStr)) amtStr++;
        per = amtStr;
        while (*per && !isspace((unsigned char)*per)) per++;
        if (!*per) continue;
        *per++ = 0;
        while (*per && isspace((unsigned char)*per)) per++;
        rest = per;
        while (*rest && !isspace((unsigned char)*rest)) rest++;
        *rest = 0;
        if (!*acc || !*amtStr || !*per) continue;
        {
            GmbDecimal amt = GmbDecimal_FromString(amtStr);
            if (GmbDecimal_IsZero(amt) && amtStr[0] != '0') continue;
            if (LedgerManager_SetBudget(ctx, acc, amt, per) != GMB_DB_OK) {
                fclose(f);
                return GMB_DB_ERR;
            }
        }
    }
    fclose(f);
    ctx->BudgetsDirty = 0;
    return GMB_DB_OK;
}

int LedgerManager_SaveBudgets(const char* budgetPath, LedgerContext* ctx)
{
    char tmpPath[PATH_MAX];
    FILE* f;
    LedgerBudgetRow rows[GMB_LEDGER_MAX_COA];
    int i, n;
    size_t plen;
    if (!budgetPath || !ctx) return GMB_DB_ARG;
    plen = strlen(budgetPath);
    if (plen + 5 >= sizeof(tmpPath)) return GMB_DB_ARG;
    snprintf(tmpPath, sizeof(tmpPath), "%s.tmp", budgetPath);
    n = ctx->nBudget;
    for (i = 0; i < n; i++) {
        snprintf(rows[i].account, GMB_LEDGER_NAME, "%s", ctx->budgetAccount[i]);
        snprintf(rows[i].period, sizeof(rows[i].period), "%s", ctx->budgetPeriod[i]);
        rows[i].amount = ctx->budgetAmount[i];
    }
    qsort(rows, (size_t)n, sizeof(rows[0]), ledger_budget_row_cmp);
    f = fopen(tmpPath, "w");
    if (!f) return GMB_DB_ERR;
    for (i = 0; i < n; i++) {
        char abuf[64];
        if (GmbDecimal_ToString(rows[i].amount, abuf, (int)sizeof(abuf)) != 0) {
            fclose(f);
            unlink(tmpPath);
            return GMB_DB_ERR;
        }
        if (fprintf(f, "%s  %s  %s\n", rows[i].account, abuf, rows[i].period) < 0) {
            fclose(f);
            unlink(tmpPath);
            return GMB_DB_ERR;
        }
    }
    if (fclose(f) != 0) {
        unlink(tmpPath);
        return GMB_DB_ERR;
    }
    unlink(budgetPath);
    if (rename(tmpPath, budgetPath) != 0) {
        unlink(tmpPath);
        return GMB_DB_ERR;
    }
    ctx->BudgetsDirty = 0;
    return GMB_DB_OK;
}

int LedgerManager_GetVariance(LedgerContext* ctx, const char* account, const char* period,
                              LedgerBudgetVariance* out)
{
    char a[GMB_LEDGER_NAME], p[16], query[GMB_LEDGER_NAME + 16];
    LedgerEntry* got;
    int year, month, startInt, endInt, n = 0, i, slot, rc;
    GmbDecimal actual = GmbDecimal_Zero(), budget = GmbDecimal_Zero();
    if (!ctx || !out) return GMB_DB_ARG;
    ledger_copy_trim(a, sizeof(a), account);
    ledger_copy_trim(p, sizeof(p), period);
    if (!a[0] || !p[0]) return GMB_DB_ARG;
    if (!ledger_try_parse_budget_period(p, &year, &month)) return GMB_DB_ARG;
    slot = ledger_budget_find(ctx, a, p);
    if (slot >= 0) budget = ctx->budgetAmount[slot];
    startInt = year * 10000 + month * 100 + 1;
    endInt = year * 10000 + month * 100 + ledger_days_in_month(year, month);
    snprintf(query, sizeof(query), "account:%s", a);
    got = (LedgerEntry*)malloc(sizeof(LedgerEntry) * ledger_filtered_cap(ctx));
    if (!got) return GMB_DB_ERR;
    rc = LedgerManager_GetFilteredEntries(ctx, query, got, ledger_filtered_cap(ctx), &n);
    if (rc != GMB_DB_OK) {
        free(got);
        return rc;
    }
    for (i = 0; i < n; i++) {
        int d = got[i].DateY * 10000 + got[i].DateM * 100 + got[i].DateD;
        if (d < startInt || d > endInt) continue;
        actual = GmbDecimal_Add(actual, GmbDecimal_Mul(got[i].Amount, GmbDecimal_FromDouble(got[i].ExchangeRate)));
    }
    free(got);
    memset(out, 0, sizeof(*out));
    snprintf(out->Account, GMB_LEDGER_NAME, "%s", a);
    snprintf(out->Period, sizeof(out->Period), "%s", p);
    out->BudgetAmount = budget;
    out->ActualAmount = actual;
    out->Variance = GmbDecimal_Sub(budget, actual);
    return GMB_DB_OK;
}

static int ledger_contains_ci(const char* s, const char* needle)
{
    size_t n;
    if (!s || !needle || !*needle) return 0;
    n = strlen(needle);
    for (; *s; s++) {
        if (gmb_strncasecmp(s, needle, n) == 0) return 1;
    }
    return 0;
}

static int ledger_split_csv_line(const char* line, char fields[][GMB_LEDGER_CSV_FIELD], int maxFields)
{
    int n = 0, inQuotes = 0, cl = 0;
    char cur[GMB_LEDGER_CSV_FIELD];
    const char* p;
    if (!line || !fields || maxFields < 1) return 0;
    cur[0] = 0;
    for (p = line; *p && *p != '\n' && *p != '\r'; p++) {
        if (*p == '"') {
            inQuotes = !inQuotes;
            continue;
        }
        if (*p == ',' && !inQuotes) {
            if (n >= maxFields) return n;
            cur[cl] = 0;
            snprintf(fields[n++], GMB_LEDGER_CSV_FIELD, "%s", cur);
            cl = 0;
            cur[0] = 0;
            continue;
        }
        if (cl < GMB_LEDGER_CSV_FIELD - 1) cur[cl++] = (char)*p;
    }
    if (n < maxFields) {
        cur[cl] = 0;
        snprintf(fields[n++], GMB_LEDGER_CSV_FIELD, "%s", cur);
    }
    return n;
}

static int ledger_parse_import_mapping(const char* rules, int* dateCol, int* accountCol, int* amountCol,
                                       int* descCol, char* contra, int contraCap)
{
    char buf[512];
    char* tok;
    char* save = NULL;
    if (!rules || !dateCol || !accountCol || !amountCol || !descCol || !contra || contraCap < 2)
        return GMB_DB_ARG;
    *dateCol = *accountCol = *amountCol = -1;
    *descCol = 0; /* C#: TryGetValue miss → default 0 */
    snprintf(contra, (size_t)contraCap, "Equity:Import");
    snprintf(buf, sizeof(buf), "%s", rules);
    for (tok = strtok_r(buf, ",", &save); tok; tok = strtok_r(NULL, ",", &save)) {
        char key[64], val[GMB_LEDGER_NAME];
        char* eq;
        char* end = NULL;
        long col;
        while (*tok && isspace((unsigned char)*tok)) tok++;
        ledger_rtrim(tok);
        eq = strchr(tok, '=');
        if (!eq || eq == tok) continue;
        *eq = 0;
        ledger_copy_trim(key, sizeof(key), tok);
        ledger_copy_trim(val, sizeof(val), eq + 1);
        if (!key[0]) continue;
        if (gmb_strcasecmp(key, "contra") == 0) {
            snprintf(contra, (size_t)contraCap, "%s", val);
            continue;
        }
        col = strtol(val, &end, 10);
        if (end == val) continue;
        if (gmb_strcasecmp(key, "date") == 0) *dateCol = (int)col;
        else if (gmb_strcasecmp(key, "account") == 0) *accountCol = (int)col;
        else if (gmb_strcasecmp(key, "amount") == 0) *amountCol = (int)col;
        else if (gmb_strcasecmp(key, "desc") == 0) *descCol = (int)col;
    }
    if (*dateCol < 0 || *accountCol < 0 || *amountCol < 0) return GMB_DB_ARG;
    return GMB_DB_OK;
}

typedef struct {
    char name[GMB_LEDGER_NAME];
    double amount;
} LedgerPlotRow;

static int ledger_plot_row_cmp(const void* a, const void* b)
{
    return strcmp(((const LedgerPlotRow*)a)->name, ((const LedgerPlotRow*)b)->name);
}

int LedgerManager_ImportCsv(LedgerContext* ctx, const char* csvPath, const char* mappingRules, int* importedOut)
{
    FILE* f;
    char line[4096];
    char fields[GMB_LEDGER_MAX_CSV_COLS][GMB_LEDGER_CSV_FIELD];
    char contra[GMB_LEDGER_NAME];
    int dateCol, accountCol, amountCol, descCol, imported = 0, first = 1, nCols, maxCol, y, m, d;
    if (importedOut) *importedOut = 0;
    if (!ctx || ledger_is_blank(csvPath) || ledger_is_blank(mappingRules)) return GMB_DB_ARG;
    if (ledger_parse_import_mapping(mappingRules, &dateCol, &accountCol, &amountCol, &descCol, contra,
                                    (int)sizeof(contra)) != GMB_DB_OK)
        return GMB_DB_ARG;
    maxCol = dateCol;
    if (accountCol > maxCol) maxCol = accountCol;
    if (amountCol > maxCol) maxCol = amountCol;
    f = fopen(csvPath, "r");
    if (!f) return GMB_DB_ERR;
    while (fgets(line, (int)sizeof(line), f)) {
        LedgerEntry entries[2];
        char desc[GMB_LEDGER_NAME], acc[GMB_LEDGER_NAME];
        GmbDecimal amt;
        int64_t id;
        ledger_rtrim(line);
        if (ledger_is_blank(line)) continue;
        if (first && ledger_contains_ci(line, "date")) {
            first = 0;
            continue;
        }
        first = 0;
        nCols = ledger_split_csv_line(line, fields, GMB_LEDGER_MAX_CSV_COLS);
        if (nCols <= maxCol) continue;
        ledger_copy_trim(acc, sizeof(acc), fields[dateCol]);
        if (sscanf(acc, "%d-%d-%d", &y, &m, &d) != 3 || m < 1 || m > 12 || d < 1 || d > 31) continue;
        ledger_copy_trim(acc, sizeof(acc), fields[amountCol]);
        amt = GmbDecimal_FromString(acc);
        if (GmbDecimal_IsZero(amt) && acc[0] != '0') continue;
        ledger_copy_trim(acc, sizeof(acc), fields[accountCol]);
        if (!acc[0]) continue;
        if (descCol >= 0 && descCol < nCols)
            ledger_copy_trim(desc, sizeof(desc), fields[descCol]);
        else
            snprintf(desc, sizeof(desc), "CSV import");
        memset(entries, 0, sizeof(entries));
        entries[0].Account = acc;
        entries[0].Amount = amt;
        entries[0].AmountIsExplicit = 1;
        entries[0].ExchangeRate = 1.0;
        entries[1].Account = contra;
        entries[1].Amount = GmbDecimal_Neg(amt);
        entries[1].AmountIsExplicit = 1;
        entries[1].ExchangeRate = 1.0;
        /* ponytail: C# holds Lock for the whole import; AddTransaction already locks per row */
        id = LedgerManager_AddTransaction(ctx, y, m, d, desc, entries, 2);
        if (id < 1) {
            fclose(f);
            return GMB_DB_ERR;
        }
        imported++;
    }
    fclose(f);
    if (importedOut) *importedOut = imported;
    return GMB_DB_OK;
}

int LedgerManager_GetBalancePlotData(LedgerContext* ctx, const char* query,
                                     char labels[][GMB_LEDGER_NAME], double* amounts, int max, int* nOut)
{
    LedgerEntry* got;
    LedgerPlotRow rows[GMB_LEDGER_MAX_COA];
    int n = 0, nRows = 0, i, j;
    if (!ctx || !labels || !amounts || !nOut || max < 1) return GMB_DB_ARG;
    *nOut = 0;
    got = (LedgerEntry*)malloc(sizeof(LedgerEntry) * ledger_filtered_cap(ctx));
    if (!got) return GMB_DB_ERR;
    if (LedgerManager_GetFilteredEntries(ctx, query, got, ledger_filtered_cap(ctx), &n) != GMB_DB_OK) {
        free(got);
        return GMB_DB_ERR;
    }
    for (i = 0; i < n && nRows < GMB_LEDGER_MAX_COA; i++) {
        const char* acc = got[i].Account ? got[i].Account : "";
        double base = GmbDecimal_ToDouble(got[i].Amount) * got[i].ExchangeRate;
        int found = 0;
        for (j = 0; j < nRows; j++) {
            if (gmb_strcasecmp(rows[j].name, acc) == 0) {
                rows[j].amount += base;
                found = 1;
                break;
            }
        }
        if (!found) {
            snprintf(rows[nRows].name, GMB_LEDGER_NAME, "%s", acc);
            rows[nRows].amount = base;
            nRows++;
        }
    }
    free(got);
    qsort(rows, (size_t)nRows, sizeof(rows[0]), ledger_plot_row_cmp);
    if (nRows > max) return GMB_DB_ARG;
    for (i = 0; i < nRows; i++) {
        snprintf(labels[i], GMB_LEDGER_NAME, "%s", rows[i].name);
        amounts[i] = rows[i].amount;
    }
    *nOut = nRows;
    return GMB_DB_OK;
}

int LedgerManager_AppendAuditLogEntry(LedgerContext* ctx, const char* description)
{
    time_t now;
    struct tm utc;
    LedgerEntry entries[2];
    int64_t id;
    if (!ctx || ledger_is_blank(description)) return GMB_DB_ARG;
    now = time(NULL);
    if (now == (time_t)-1 || !gmtime_r(&now, &utc)) return GMB_DB_ERR;
    memset(entries, 0, sizeof(entries));
    entries[0].Account = "Audit:MCP";
    entries[0].Amount = GmbDecimal_Zero();
    entries[0].AmountIsExplicit = 1;
    entries[0].ExchangeRate = 1.0;
    entries[1].Account = "Audit:MCP:Clearing";
    entries[1].Amount = GmbDecimal_Zero();
    entries[1].AmountIsExplicit = 1;
    entries[1].ExchangeRate = 1.0;
    id = LedgerManager_AddTransaction(ctx, utc.tm_year + 1900, utc.tm_mon + 1, utc.tm_mday,
                                      description, entries, 2);
    return id < 1 ? GMB_DB_ERR : GMB_DB_OK;
}

static void ledger_selfcheck_rm(const char* path)
{
    if (path) unlink(path);
}

int LedgerManager_SelfCheck(void)
{
    char dir[] = "native/gmb_db_c/build/ledgersc_XXXXXX";
    char journal[PATH_MAX];
    char buf[128];
    LedgerContext *ctx, *reopen;
    LedgerEntry entries[2];
    LedgerEntry got[4];
    LedgerIndexRecord rec;
    FILE* f;
    unsigned char head[32];
    size_t nread;
    int64_t id;
    char* text;
    long jlen;
    int gn, bomN, bi;

    if (GmbDecimal_SelfCheck() != 0) return -1;
    if (!mkdtemp(dir)) return -2;
    snprintf(journal, sizeof(journal), "%s/book.dat", dir);

    ctx = LedgerContext_New(journal);
    if (!ctx) return -3;
    if (!ctx->IndexFilePath || !strstr(ctx->IndexFilePath, ".idx")) {
        LedgerContext_Dispose(ctx);
        return -4;
    }
    if (!ctx->BudgetFilePath || !strstr(ctx->BudgetFilePath, ".budget")) {
        LedgerContext_Dispose(ctx);
        return -5;
    }

    if (LedgerManager_FormatAmountForJournal(GmbDecimal_FromI64(100), NULL, 1.0, buf, (int)sizeof(buf)) != GMB_DB_OK
        || strcmp(buf, "100") != 0) {
        LedgerContext_Dispose(ctx);
        return -6;
    }
    if (LedgerManager_FormatAmountForJournal(GmbDecimal_FromI64(100), "USD", 1.0, buf, (int)sizeof(buf)) != GMB_DB_OK
        || strcmp(buf, "100 USD") != 0) {
        LedgerContext_Dispose(ctx);
        return -7;
    }
    if (LedgerManager_FormatAmountForJournal(GmbDecimal_FromI64(100), "USD", 2.0, buf, (int)sizeof(buf)) != GMB_DB_OK
        || strcmp(buf, "100 USD @ 2") != 0) {
        LedgerContext_Dispose(ctx);
        return -8;
    }

    memset(entries, 0, sizeof(entries));
    entries[0].Account = "Cash";
    entries[0].Amount = GmbDecimal_FromI64(100);
    entries[0].AmountIsExplicit = 1;
    entries[0].ExchangeRate = 1.0;
    entries[1].Account = "Equity";
    entries[1].Amount = GmbDecimal_FromI64(-99);
    entries[1].AmountIsExplicit = 1;
    entries[1].ExchangeRate = 1.0;
    if (LedgerManager_IsTransactionBalanced(entries, 2)) {
        LedgerContext_Dispose(ctx);
        return -9;
    }
    entries[1].Amount = GmbDecimal_FromI64(-100);
    if (!LedgerManager_IsTransactionBalanced(entries, 2)) {
        LedgerContext_Dispose(ctx);
        return -10;
    }
    entries[1].Amount = GmbDecimal_Zero();
    entries[1].AmountIsExplicit = 0;
    if (!LedgerManager_IsTransactionBalanced(entries, 2) || GmbDecimal_Cmp(entries[1].Amount, GmbDecimal_FromI64(-100)) != 0) {
        LedgerContext_Dispose(ctx);
        return -11;
    }

    entries[0].Account = "Cash";
    entries[0].Amount = GmbDecimal_FromI64(100);
    entries[0].AmountIsExplicit = 1;
    entries[0].ExchangeRate = 1.0;
    entries[0].Currency = NULL;
    entries[1].Account = "Equity";
    entries[1].Amount = GmbDecimal_FromI64(-100);
    entries[1].AmountIsExplicit = 1;
    entries[1].ExchangeRate = 1.0;
    entries[1].Currency = NULL;
    id = LedgerManager_AddTransaction(ctx, 2026, 8, 20, "seed", entries, 2);
    if (id != 1) {
        LedgerContext_Dispose(ctx);
        return -12;
    }
    if (!LedgerContext_IsDirty(ctx)) {
        LedgerContext_Dispose(ctx);
        return -13;
    }
    if (!LedgerManager_TryGetIndex(ctx, 1, &rec) || rec.FileOffset != 0 || rec.Date != 20260820) {
        LedgerContext_Dispose(ctx);
        return -14;
    }
    gn = 0;
    if (LedgerManager_GetTransaction(ctx, 1, got, 4, &gn) != GMB_DB_OK || gn != 2) {
        LedgerContext_Dispose(ctx);
        return -30;
    }
    if (!got[0].Account || strcmp(got[0].Account, "Cash") != 0 || fabs(GmbDecimal_ToDouble(got[0].Amount) - 100.0) >= 0.0001
        || !got[1].Account || strcmp(got[1].Account, "Equity") != 0 || fabs(GmbDecimal_ToDouble(got[1].Amount) + 100.0) >= 0.0001) {
        LedgerContext_Dispose(ctx);
        return -31;
    }
    if (got[0].DateY != 2026 || got[0].DateM != 8 || got[0].DateD != 20
        || !got[0].Description || strcmp(got[0].Description, "seed") != 0
        || got[0].TransactionId != 1) {
        LedgerContext_Dispose(ctx);
        return -32;
    }
    if (LedgerManager_GetTransaction(ctx, 1, got, 4, &gn) != GMB_DB_OK || gn != 2) {
        LedgerContext_Dispose(ctx);
        return -33;
    }
    if (LedgerManager_GetTransaction(ctx, 99, got, 4, &gn) != GMB_DB_ERR) {
        LedgerContext_Dispose(ctx);
        return -34;
    }

    f = fopen(journal, "rb");
    if (!f) {
        LedgerContext_Dispose(ctx);
        return -15;
    }
    nread = fread(head, 1, sizeof(head), f);
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        LedgerContext_Dispose(ctx);
        return -16;
    }
    jlen = ftell(f);
    fclose(f);
    if (nread < 18 || head[0] != 0xEF || head[1] != 0xBB || head[2] != 0xBF) {
        LedgerContext_Dispose(ctx);
        return -17;
    }
    if (memcmp(head + 3, "2026-08-20 seed", 15) != 0) {
        LedgerContext_Dispose(ctx);
        return -18;
    }

    text = (char*)malloc((size_t)jlen + 1);
    if (!text) {
        LedgerContext_Dispose(ctx);
        return -19;
    }
    f = fopen(journal, "rb");
    if (!f || (long)fread(text, 1, (size_t)jlen, f) != jlen) {
        if (f) fclose(f);
        free(text);
        LedgerContext_Dispose(ctx);
        return -20;
    }
    fclose(f);
    text[jlen] = 0;
    if (!strstr(text, "    Cash      100") || !strstr(text, "    Equity      -100")) {
        free(text);
        LedgerContext_Dispose(ctx);
        return -21;
    }
    free(text);

    if (LedgerManager_SaveIndexToFile(ctx->IndexFilePath, ctx) != GMB_DB_OK) {
        LedgerContext_Dispose(ctx);
        return -22;
    }
    f = fopen(ctx->IndexFilePath, "rb");
    if (!f) {
        LedgerContext_Dispose(ctx);
        return -23;
    }
    nread = fread(head, 1, 11, f);
    fclose(f);
    if (nread != 11 || head[0] != 0x06 || memcmp(head + 1, "LDGIDX", 6) != 0
        || head[7] != 1 || head[8] != 0 || head[9] != 0 || head[10] != 0) {
        LedgerContext_Dispose(ctx);
        return -24;
    }

    reopen = LedgerContext_New(journal);
    if (!reopen) {
        LedgerContext_Dispose(ctx);
        return -25;
    }
    if (LedgerManager_ReadIndexFromFile(ctx->IndexFilePath, reopen) != GMB_DB_OK) {
        LedgerContext_Dispose(reopen);
        LedgerContext_Dispose(ctx);
        return -26;
    }
    if (LedgerManager_IndexCount(reopen) != 1
        || !LedgerManager_TryGetIndex(reopen, 1, &rec)
        || rec.FileOffset != 0 || rec.Date != 20260820) {
        LedgerContext_Dispose(reopen);
        LedgerContext_Dispose(ctx);
        return -27;
    }
    gn = 0;
    if (LedgerManager_GetTransaction(reopen, 1, got, 4, &gn) != GMB_DB_OK || gn != 2
        || !got[0].Account || strcmp(got[0].Account, "Cash") != 0) {
        LedgerContext_Dispose(reopen);
        LedgerContext_Dispose(ctx);
        return -35;
    }
    LedgerContext_Dispose(reopen);

    entries[0].Amount = GmbDecimal_FromI64(50);
    entries[1].Amount = GmbDecimal_FromI64(-50);
    id = LedgerManager_AddTransaction(ctx, 2026, 8, 21, "follow", entries, 2);
    if (id != 2) {
        LedgerContext_Dispose(ctx);
        return -28;
    }
    if (!LedgerManager_TryGetIndex(ctx, 2, &rec) || rec.Date != 20260821 || rec.FileOffset <= 0) {
        LedgerContext_Dispose(ctx);
        return -29;
    }
    gn = 0;
    if (LedgerManager_GetTransaction(ctx, 2, got, 4, &gn) != GMB_DB_OK || gn != 2
        || !got[0].Description || strcmp(got[0].Description, "follow") != 0
        || fabs(GmbDecimal_ToDouble(got[0].Amount) - 50.0) >= 0.0001) {
        LedgerContext_Dispose(ctx);
        return -36;
    }
    if (!LedgerManager_DeleteTransaction(ctx, 1)) {
        LedgerContext_Dispose(ctx);
        return -37;
    }
    f = fopen(journal, "rb");
    if (!f) {
        LedgerContext_Dispose(ctx);
        return -38;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        LedgerContext_Dispose(ctx);
        return -39;
    }
    jlen = ftell(f);
    rewind(f);
    text = (char*)malloc((size_t)jlen + 1);
    if (!text || (long)fread(text, 1, (size_t)jlen, f) != jlen) {
        fclose(f);
        free(text);
        LedgerContext_Dispose(ctx);
        return -40;
    }
    fclose(f);
    text[jlen] = 0;
    /* DELETE = storno IN PLACE (`date * …`) + rimozione dall'indice attivo: il blocco
     * originale resta nel journal con gli importi NON negati, e lo storno non è una
     * transazione. */
    if (!strstr(text, "2026-08-20 * seed") || !strstr(text, "    Cash      100")) {
        free(text);
        LedgerContext_Dispose(ctx);
        return -41;
    }
    free(text);

    if (LedgerManager_IndexCount(ctx) != 1
        || LedgerManager_GetTransaction(ctx, 1, got, 4, &gn) != GMB_DB_ERR) {
        LedgerContext_Dispose(ctx);
        return -41;
    }

    if (LedgerManager_PackJournal(ctx) != GMB_DB_OK) {
        LedgerContext_Dispose(ctx);
        return -42;
    }
    /* PACK scarta gli storni: resta solo "follow" (rinumerato 1). */
    if (LedgerContext_IsDirty(ctx) || LedgerManager_IndexCount(ctx) != 1) {
        LedgerContext_Dispose(ctx);
        return -43;
    }
    if (!LedgerManager_TryGetIndex(ctx, 1, &rec) || rec.Date != 0) {
        LedgerContext_Dispose(ctx);
        return -44;
    }
    gn = 0;
    if (LedgerManager_GetTransaction(ctx, 1, got, 4, &gn) != GMB_DB_OK || gn != 2
        || !got[0].Description || strcmp(got[0].Description, "follow") != 0) {
        LedgerContext_Dispose(ctx);
        return -45;
    }
    gn = 0;
    if (LedgerManager_GetTransaction(ctx, 2, got, 4, &gn) != GMB_DB_ERR) {
        LedgerContext_Dispose(ctx);
        return -46;
    }
    f = fopen(journal, "rb");
    if (!f) {
        LedgerContext_Dispose(ctx);
        return -48;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        LedgerContext_Dispose(ctx);
        return -49;
    }
    jlen = ftell(f);
    rewind(f);
    text = (char*)malloc((size_t)jlen + 1);
    if (!text || (long)fread(text, 1, (size_t)jlen, f) != jlen) {
        fclose(f);
        free(text);
        LedgerContext_Dispose(ctx);
        return -50;
    }
    fclose(f);
    bomN = 0;
    for (bi = 0; bi < (int)jlen - 2; bi++) {
        if ((unsigned char)text[bi] == 0xEF && (unsigned char)text[bi + 1] == 0xBB
            && (unsigned char)text[bi + 2] == 0xBF)
            bomN++;
    }
    if (bomN != 1 || text[0] != (char)0xEF) {
        free(text);
        LedgerContext_Dispose(ctx);
        return -51;
    }
    free(text);

    reopen = LedgerContext_New(journal);
    if (!reopen) {
        LedgerContext_Dispose(ctx);
        return -52;
    }
    if (LedgerManager_RebuildIndexFromFile(journal, reopen) != GMB_DB_OK
        || LedgerManager_IndexCount(reopen) != 1
        || !LedgerManager_TryGetIndex(reopen, 1, &rec) || rec.Date != 20260821) {
        LedgerContext_Dispose(reopen);
        LedgerContext_Dispose(ctx);
        return -53;
    }
    LedgerContext_Dispose(reopen);

    ledger_selfcheck_rm(ctx->IndexFilePath);
    LedgerContext_Dispose(ctx);
    ledger_selfcheck_rm(journal);
    rmdir(dir);
    return GMB_DB_OK;
}

int LedgerManager_Query_SelfCheck(void)
{
    char dir[] = "native/gmb_db_c/build/ledgerq_XXXXXX";
    char journal[PATH_MAX];
    char coaPath[PATH_MAX];
    LedgerContext* ctx;
    LedgerEntry entries[2];
    LedgerEntry got[16];
    LedgerQueryFilter q;
    char names[8][GMB_LEDGER_NAME];
    FILE* f;
    int n, nn;
    int64_t id;

    if (!mkdtemp(dir)) return -60;
    snprintf(journal, sizeof(journal), "%s/book.dat", dir);
    snprintf(coaPath, sizeof(coaPath), "%s/chart.coa", dir);

    ctx = LedgerContext_New(journal);
    if (!ctx) return -61;

    if (LedgerManager_ParseLedgerQuery("  ", &q) != GMB_DB_OK || q.hasDate || q.hasAccount) {
        LedgerContext_Dispose(ctx);
        rmdir(dir);
        return -62;
    }
    if (LedgerManager_ParseLedgerQuery("account:\"Expenses:Office\" date:>=2026-08-21", &q) != GMB_DB_OK
        || !q.hasAccount || strcmp(q.accountPat, "Expenses:Office") != 0
        || !q.hasDate || strcmp(q.dateOp, ">=") != 0 || q.dateVal != 20260821) {
        LedgerContext_Dispose(ctx);
        rmdir(dir);
        return -63;
    }
    if (LedgerManager_ParseLedgerQuery("account:Expenses:*", &q) != GMB_DB_OK
        || !q.hasAccount || strcmp(q.accountPat, "Expenses:*") != 0 || q.hasDate) {
        LedgerContext_Dispose(ctx);
        rmdir(dir);
        return -64;
    }

    memset(entries, 0, sizeof(entries));
    entries[0].Account = "Assets:Cash";
    entries[0].Amount = GmbDecimal_FromI64(100);
    entries[0].AmountIsExplicit = 1;
    entries[0].ExchangeRate = 1.0;
    entries[1].Account = "Equity";
    entries[1].Amount = GmbDecimal_FromI64(-100);
    entries[1].AmountIsExplicit = 1;
    entries[1].ExchangeRate = 1.0;
    id = LedgerManager_AddTransaction(ctx, 2026, 8, 20, "seed", entries, 2);
    if (id < 1) {
        LedgerContext_Dispose(ctx);
        ledger_selfcheck_rm(journal);
        rmdir(dir);
        return -65;
    }

    entries[0].Account = "Expenses:Office";
    entries[0].Amount = GmbDecimal_FromI64(40);
    entries[1].Account = "Assets:Cash";
    entries[1].Amount = GmbDecimal_FromI64(-40);
    id = LedgerManager_AddTransaction(ctx, 2026, 8, 21, "office", entries, 2);
    if (id < 1) {
        ledger_selfcheck_rm(ctx->IndexFilePath);
        LedgerContext_Dispose(ctx);
        ledger_selfcheck_rm(journal);
        rmdir(dir);
        return -66;
    }

    n = 0;
    if (LedgerManager_GetFilteredEntries(ctx, "", got, 16, &n) != GMB_DB_OK || n != 4) {
        LedgerContext_Dispose(ctx);
        ledger_selfcheck_rm(journal);
        rmdir(dir);
        return -67;
    }
    n = 0;
    if (LedgerManager_GetFilteredEntries(ctx, "account:Assets:Cash", got, 16, &n) != GMB_DB_OK || n != 2
        || !got[0].Account || strcmp(got[0].Account, "Assets:Cash") != 0
        || !got[1].Account || strcmp(got[1].Account, "Assets:Cash") != 0) {
        LedgerContext_Dispose(ctx);
        ledger_selfcheck_rm(journal);
        rmdir(dir);
        return -68;
    }
    n = 0;
    if (LedgerManager_GetFilteredEntries(ctx, "date:>=2026-08-21", got, 16, &n) != GMB_DB_OK || n != 2
        || !got[0].Description || strcmp(got[0].Description, "office") != 0) {
        LedgerContext_Dispose(ctx);
        ledger_selfcheck_rm(journal);
        rmdir(dir);
        return -69;
    }
    n = 0;
    if (LedgerManager_GetFilteredEntries(ctx, "account:Expenses:*", got, 16, &n) != GMB_DB_OK || n != 1
        || !got[0].Account || strcmp(got[0].Account, "Expenses:Office") != 0) {
        LedgerContext_Dispose(ctx);
        ledger_selfcheck_rm(journal);
        rmdir(dir);
        return -70;
    }

    nn = 0;
    if (LedgerManager_GetAccounts(ctx, NULL, names, 8, &nn) != GMB_DB_OK || nn != 3
        || strcmp(names[0], "Assets:Cash") != 0
        || strcmp(names[1], "Equity") != 0
        || strcmp(names[2], "Expenses:Office") != 0) {
        LedgerContext_Dispose(ctx);
        ledger_selfcheck_rm(journal);
        rmdir(dir);
        return -71;
    }
    nn = 0;
    if (LedgerManager_GetAccounts(ctx, "assets", names, 8, &nn) != GMB_DB_OK || nn != 1
        || strcmp(names[0], "Assets:Cash") != 0) {
        LedgerContext_Dispose(ctx);
        ledger_selfcheck_rm(journal);
        rmdir(dir);
        return -72;
    }

    f = fopen(coaPath, "w");
    if (!f) {
        LedgerContext_Dispose(ctx);
        ledger_selfcheck_rm(journal);
        rmdir(dir);
        return -73;
    }
    fputs("; comment\n", f);
    fputs("Assets:Cash  Liquid\n", f);
    fputs("Expenses:Office\tOffice costs\n", f);
    fclose(f);
    if (LedgerManager_LoadChartOfAccounts(coaPath, ctx) != GMB_DB_OK || ctx->nCoa != 2) {
        LedgerContext_Dispose(ctx);
        ledger_selfcheck_rm(coaPath);
        ledger_selfcheck_rm(journal);
        rmdir(dir);
        return -74;
    }
    nn = 0;
    if (LedgerManager_GetAccounts(ctx, NULL, names, 8, &nn) != GMB_DB_OK || nn != 2
        || strcmp(names[0], "Assets:Cash") != 0
        || strcmp(names[1], "Expenses:Office") != 0) {
        LedgerContext_Dispose(ctx);
        ledger_selfcheck_rm(coaPath);
        ledger_selfcheck_rm(journal);
        rmdir(dir);
        return -75;
    }
    nn = 0;
    if (LedgerManager_GetAccounts(ctx, "Expenses", names, 8, &nn) != GMB_DB_OK || nn != 1
        || strcmp(names[0], "Expenses:Office") != 0) {
        LedgerContext_Dispose(ctx);
        ledger_selfcheck_rm(coaPath);
        ledger_selfcheck_rm(journal);
        rmdir(dir);
        return -76;
    }

    ledger_selfcheck_rm(ctx->IndexFilePath);
    ledger_selfcheck_rm(ctx->BudgetFilePath);
    LedgerContext_Dispose(ctx);
    ledger_selfcheck_rm(journal);
    ledger_selfcheck_rm(coaPath);
    rmdir(dir);
    return GMB_DB_OK;
}

int LedgerManager_Budget_SelfCheck(void)
{
    char dir[] = "native/gmb_db_c/build/ledgerb_XXXXXX";
    char journal[PATH_MAX];
    LedgerContext *ctx, *reopen;
    LedgerEntry entries[2];
    LedgerBudgetVariance v;
    FILE* f;
    char line[128];
    int64_t id;

    if (!mkdtemp(dir)) return -80;
    snprintf(journal, sizeof(journal), "%s/book.dat", dir);
    ctx = LedgerContext_New(journal);
    if (!ctx) return -81;

    if (LedgerManager_LoadBudgets(ctx->BudgetFilePath, ctx) != GMB_DB_OK || ctx->nBudget != 0) {
        LedgerContext_Dispose(ctx);
        rmdir(dir);
        return -82;
    }
    if (LedgerManager_SetBudget(ctx, "  ", GmbDecimal_FromI64(100), "2026-08") != GMB_DB_ARG
        || LedgerManager_SetBudget(ctx, "Expenses:Office", GmbDecimal_FromI64(100), "  ") != GMB_DB_ARG) {
        LedgerContext_Dispose(ctx);
        rmdir(dir);
        return -83;
    }
    if (LedgerManager_SetBudget(ctx, "Expenses:Office", GmbDecimal_FromI64(100), "2026-08") != GMB_DB_OK
        || !ctx->BudgetsDirty || ctx->nBudget != 1) {
        LedgerContext_Dispose(ctx);
        rmdir(dir);
        return -84;
    }
    if (LedgerManager_GetVariance(ctx, "Expenses:Office", "2026-13", &v) != GMB_DB_ARG) {
        LedgerContext_Dispose(ctx);
        rmdir(dir);
        return -85;
    }

    memset(entries, 0, sizeof(entries));
    entries[0].Account = "Expenses:Office";
    entries[0].Amount = GmbDecimal_FromI64(40);
    entries[0].AmountIsExplicit = 1;
    entries[0].ExchangeRate = 1.0;
    entries[1].Account = "Assets:Cash";
    entries[1].Amount = GmbDecimal_FromI64(-40);
    entries[1].AmountIsExplicit = 1;
    entries[1].ExchangeRate = 1.0;
    id = LedgerManager_AddTransaction(ctx, 2026, 8, 21, "office", entries, 2);
    if (id < 1) {
        LedgerContext_Dispose(ctx);
        ledger_selfcheck_rm(journal);
        rmdir(dir);
        return -86;
    }
    memset(&v, 0, sizeof(v));
    if (LedgerManager_GetVariance(ctx, "Expenses:Office", "2026-08", &v) != GMB_DB_OK
        || fabs(GmbDecimal_ToDouble(v.BudgetAmount) - 100.0) >= 0.0001
        || fabs(GmbDecimal_ToDouble(v.ActualAmount) - 40.0) >= 0.0001
        || fabs(GmbDecimal_ToDouble(v.Variance) - 60.0) >= 0.0001) {
        LedgerContext_Dispose(ctx);
        ledger_selfcheck_rm(journal);
        rmdir(dir);
        return -87;
    }
    if (LedgerManager_SaveBudgets(ctx->BudgetFilePath, ctx) != GMB_DB_OK || ctx->BudgetsDirty) {
        LedgerContext_Dispose(ctx);
        ledger_selfcheck_rm(journal);
        rmdir(dir);
        return -88;
    }
    f = fopen(ctx->BudgetFilePath, "r");
    if (!f || !fgets(line, (int)sizeof(line), f)) {
        if (f) fclose(f);
        LedgerContext_Dispose(ctx);
        ledger_selfcheck_rm(journal);
        rmdir(dir);
        return -89;
    }
    fclose(f);
    if (!strstr(line, "Expenses:Office") || !strstr(line, "2026-08")) {
        LedgerContext_Dispose(ctx);
        ledger_selfcheck_rm(journal);
        rmdir(dir);
        return -90;
    }

    reopen = LedgerContext_New(journal);
    if (!reopen) {
        LedgerContext_Dispose(ctx);
        ledger_selfcheck_rm(journal);
        rmdir(dir);
        return -91;
    }
    if (LedgerManager_LoadBudgets(ctx->BudgetFilePath, reopen) != GMB_DB_OK
        || reopen->nBudget != 1 || reopen->BudgetsDirty) {
        LedgerContext_Dispose(reopen);
        LedgerContext_Dispose(ctx);
        ledger_selfcheck_rm(journal);
        rmdir(dir);
        return -92;
    }
    if (LedgerManager_SetBudget(reopen, "expenses:office", GmbDecimal_FromI64(100), "2026-08") != GMB_DB_OK
        || reopen->nBudget != 1) {
        LedgerContext_Dispose(reopen);
        LedgerContext_Dispose(ctx);
        ledger_selfcheck_rm(journal);
        rmdir(dir);
        return -93;
    }

    ledger_selfcheck_rm(ctx->IndexFilePath);
    ledger_selfcheck_rm(ctx->BudgetFilePath);
    LedgerContext_Dispose(reopen);
    LedgerContext_Dispose(ctx);
    ledger_selfcheck_rm(journal);
    rmdir(dir);
    return GMB_DB_OK;
}

int LedgerManager_Csv_SelfCheck(void)
{
    char dir[] = "native/gmb_db_c/build/ledgerc_XXXXXX";
    char journal[PATH_MAX];
    char csvPath[PATH_MAX];
    LedgerContext* ctx;
    LedgerEntry got[8];
    char labels[8][GMB_LEDGER_NAME];
    double amounts[8];
    FILE* f;
    int imported = 0, n = 0, gn = 0;

    if (!mkdtemp(dir)) return -100;
    snprintf(journal, sizeof(journal), "%s/book.dat", dir);
    snprintf(csvPath, sizeof(csvPath), "%s/bank.csv", dir);
    ctx = LedgerContext_New(journal);
    if (!ctx) return -101;

    if (LedgerManager_ImportCsv(ctx, "", "date=0,account=2,amount=3", &imported) != GMB_DB_ARG
        || LedgerManager_ImportCsv(ctx, csvPath, "account=2,amount=3", &imported) != GMB_DB_ARG) {
        LedgerContext_Dispose(ctx);
        rmdir(dir);
        return -102;
    }

    f = fopen(csvPath, "w");
    if (!f) {
        LedgerContext_Dispose(ctx);
        rmdir(dir);
        return -103;
    }
    fputs("Date,Desc,Account,Amount\n", f);
    fputs("2026-08-21,Coffee,Expenses:Office,12.5\n", f);
    fputs("2026-08-21,Rent,Expenses:Rent,800\n", f);
    fputs("2026-08-22,\"Lunch, team\",Expenses:Food,20\n", f);
    fputs("\n", f);
    fputs("not-a-date,Skip,Expenses:X,10\n", f);
    fputs("2026-08-23,Bad,Expenses:X,notnum\n", f);
    fclose(f);

    imported = -1;
    if (LedgerManager_ImportCsv(ctx, csvPath,
                                "date=0,desc=1,account=2,amount=3,contra=Assets:Bank",
                                &imported) != GMB_DB_OK || imported != 3) {
        LedgerContext_Dispose(ctx);
        ledger_selfcheck_rm(csvPath);
        ledger_selfcheck_rm(journal);
        rmdir(dir);
        return -104;
    }
    gn = 0;
    if (LedgerManager_GetFilteredEntries(ctx, "", got, 8, &gn) != GMB_DB_OK || gn != 6) {
        LedgerContext_Dispose(ctx);
        ledger_selfcheck_rm(csvPath);
        ledger_selfcheck_rm(journal);
        rmdir(dir);
        return -105;
    }
    gn = 0;
    if (LedgerManager_GetTransaction(ctx, 3, got, 4, &gn) != GMB_DB_OK || gn != 2
        || !got[0].Description || strcmp(got[0].Description, "Lunch, team") != 0
        || !got[0].Account || strcmp(got[0].Account, "Expenses:Food") != 0
        || fabs(GmbDecimal_ToDouble(got[0].Amount) - 20.0) >= 0.0001) {
        LedgerContext_Dispose(ctx);
        ledger_selfcheck_rm(csvPath);
        ledger_selfcheck_rm(journal);
        rmdir(dir);
        return -106;
    }

    n = 0;
    if (LedgerManager_GetBalancePlotData(ctx, "", labels, amounts, 8, &n) != GMB_DB_OK || n != 4
        || strcmp(labels[0], "Assets:Bank") != 0 || fabs(amounts[0] + 832.5) >= 0.0001
        || strcmp(labels[1], "Expenses:Food") != 0 || fabs(amounts[1] - 20.0) >= 0.0001
        || strcmp(labels[2], "Expenses:Office") != 0 || fabs(amounts[2] - 12.5) >= 0.0001
        || strcmp(labels[3], "Expenses:Rent") != 0 || fabs(amounts[3] - 800.0) >= 0.0001) {
        LedgerContext_Dispose(ctx);
        ledger_selfcheck_rm(csvPath);
        ledger_selfcheck_rm(journal);
        rmdir(dir);
        return -107;
    }
    n = 0;
    if (LedgerManager_GetBalancePlotData(ctx, "account:Expenses:*", labels, amounts, 8, &n) != GMB_DB_OK
        || n != 3
        || strcmp(labels[0], "Expenses:Food") != 0
        || strcmp(labels[1], "Expenses:Office") != 0
        || strcmp(labels[2], "Expenses:Rent") != 0) {
        LedgerContext_Dispose(ctx);
        ledger_selfcheck_rm(csvPath);
        ledger_selfcheck_rm(journal);
        rmdir(dir);
        return -108;
    }

    ledger_selfcheck_rm(ctx->IndexFilePath);
    LedgerContext_Dispose(ctx);
    ledger_selfcheck_rm(journal);
    ledger_selfcheck_rm(csvPath);
    rmdir(dir);
    return GMB_DB_OK;
}

int LedgerManager_Audit_SelfCheck(void)
{
    char dir[] = "native/gmb_db_c/build/ledgera_XXXXXX";
    char journal[PATH_MAX];
    LedgerContext* ctx;
    LedgerEntry got[4];
    int gn = 0;

    if (!mkdtemp(dir)) return -110;
    snprintf(journal, sizeof(journal), "%s/book.dat", dir);
    ctx = LedgerContext_New(journal);
    if (!ctx) return -111;

    if (LedgerManager_AppendAuditLogEntry(ctx, "  ") != GMB_DB_ARG
        || LedgerManager_AppendAuditLogEntry(NULL, "mcp ping") != GMB_DB_ARG) {
        LedgerContext_Dispose(ctx);
        rmdir(dir);
        return -112;
    }
    if (LedgerManager_AppendAuditLogEntry(ctx, "mcp ping") != GMB_DB_OK) {
        LedgerContext_Dispose(ctx);
        ledger_selfcheck_rm(journal);
        rmdir(dir);
        return -113;
    }
    gn = 0;
    if (LedgerManager_GetTransaction(ctx, 1, got, 4, &gn) != GMB_DB_OK || gn != 2
        || !got[0].Description || strcmp(got[0].Description, "mcp ping") != 0
        || !got[0].Account || strcmp(got[0].Account, "Audit:MCP") != 0
        || !got[1].Account || strcmp(got[1].Account, "Audit:MCP:Clearing") != 0
        || fabs(GmbDecimal_ToDouble(got[0].Amount)) >= 0.0001 || fabs(GmbDecimal_ToDouble(got[1].Amount)) >= 0.0001
        || got[0].DateY < 2026) {
        LedgerContext_Dispose(ctx);
        ledger_selfcheck_rm(journal);
        rmdir(dir);
        return -114;
    }

    {
        int t, gn2;
        LedgerEntry pair[2];
        LedgerEntry one[4];
        for (t = 0; t < 80; t++) {
            memset(pair, 0, sizeof(pair));
            pair[0].Account = "Cash";
            pair[0].Amount = GmbDecimal_FromI64(1);
            pair[0].AmountIsExplicit = 1;
            pair[0].ExchangeRate = 1.0;
            pair[1].Account = "Equity";
            pair[1].Amount = GmbDecimal_FromI64(-1);
            pair[1].AmountIsExplicit = 1;
            pair[1].ExchangeRate = 1.0;
            if (LedgerManager_AddTransaction(ctx, 2026, 3, 1, "cache grow", pair, 2) < 1) {
                LedgerContext_Dispose(ctx);
                ledger_selfcheck_rm(journal);
                rmdir(dir);
                return -115;
            }
        }
        for (t = 0; t < ctx->nIndex; t++) {
            gn2 = 0;
            if (LedgerManager_GetTransaction(ctx, ctx->indexIds[t], one, 4, &gn2) != GMB_DB_OK || gn2 < 1) {
                LedgerContext_Dispose(ctx);
                ledger_selfcheck_rm(journal);
                rmdir(dir);
                return -116;
            }
        }
        if (ctx->nCache < 70 || ctx->capCache < 70) {
            LedgerContext_Dispose(ctx);
            ledger_selfcheck_rm(journal);
            rmdir(dir);
            return -117;
        }
    }

    ledger_selfcheck_rm(ctx->IndexFilePath);
    LedgerContext_Dispose(ctx);
    ledger_selfcheck_rm(journal);
    rmdir(dir);
    return GMB_DB_OK;
}
