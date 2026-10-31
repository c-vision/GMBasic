using System.Collections.Concurrent;
using System.Globalization;
using System.Text;
using System.Text.RegularExpressions;

// ReSharper disable CheckNamespace

namespace Basic.Ledger;

/// <summary>
/// Nuova classe di utilità statica che incapsula tutta la logica di basso livello,
/// pura e senza stato (stateless) per la manipolazione dei file Ledger.
/// Questi metodi vengono chiamati dall'Executor all'interno delle operazioni PerformSyscall
/// per eseguire il lavoro di I/O e di parsing in un thread in background.
/// Questa classe sostituisce la logica precedentemente contenuta nel LedgerManager.
/// </summary>
public sealed partial class LedgerManager
{
    /// <summary>
    /// Rappresenta il risultato aggregato di una query di bilancio.
    /// Viene restituito dalla logica di BALANCE e poi mappato su un array UDT dall'Executor.
    /// </summary>
    /// <param name="Account">Il nome del conto aggregato.</param>
    /// <param name="TotalAmount">La somma totale per quel conto.</param>
    // ReSharper disable once NotAccessedPositionalProperty.Global
    public sealed record LedgerBalanceEntry(string Account, decimal TotalAmount);

    #region Metodi di I/O e Indicizzazione

    /// <summary>
    /// Salva l'indice in memoria su un file binario (.idx) in modo sicuro.
    /// Utilizza il pattern "write-to-temp-then-rename" per garantire l'atomicità.
    /// </summary>
    public static void SaveIndexToFile(string indexPath, ConcurrentDictionary<long, LedgerIndexRecord> index)
    {
        var tempFile = indexPath + ".tmp";
        try
        {
            using (var stream = new FileStream(tempFile, FileMode.Create, FileAccess.Write, FileShare.None))
            using (var writer = new BinaryWriter(stream, Encoding.UTF8, false))
            {
                writer.Write("LDGIDX"); // Magic string
                writer.Write(1); // Version
                writer.Write(index.Count);
                foreach (var (transactionId, record) in index)
                {
                    writer.Write(transactionId);
                    writer.Write(record.FileOffset);
                    writer.Write(record.Date);
                }
            }

            File.Move(tempFile, indexPath, true);
        }
        catch
        {
            if (File.Exists(tempFile)) File.Delete(tempFile);
            throw;
        }
    }

    /// <summary>
    /// Legge il file indice (.idx) binario e popola la struttura dati dell'indice in memoria.
    /// </summary>
    public static void ReadIndexFromFile(string indexPath, ConcurrentDictionary<long, LedgerIndexRecord> index)
    {
        using var stream = new FileStream(indexPath, FileMode.Open, FileAccess.Read, FileShare.Read);
        using var reader = new BinaryReader(stream, Encoding.UTF8, false);
        if (stream.Length < 14) throw new InvalidDataException("Ledger index file is corrupted.");
        if (reader.ReadString() != "LDGIDX") throw new InvalidDataException("Invalid Ledger index file format.");
        if (reader.ReadInt32() != 1) throw new NotSupportedException("Unsupported Ledger index file version.");
        var count = reader.ReadInt32();
        for (var i = 0; i < count; i++)
        {
            var transactionId = reader.ReadInt64();
            var fileOffset = reader.ReadInt64();
            var date = reader.ReadInt32();
            index[transactionId] = new LedgerIndexRecord(fileOffset, date);
        }
    }

    /// <summary>
    /// Esegue una scansione completa del file di giornale (.dat) e costruisce un nuovo indice.
    /// </summary>
    public static void RebuildIndexFromFile(string journalPath, ConcurrentDictionary<long, LedgerIndexRecord> index)
    {
        index.Clear();
        if (!File.Exists(journalPath)) return;

        long currentOffset = 0;
        long transactionIdCounter = 0;
        using var stream = new FileStream(journalPath, FileMode.Open, FileAccess.Read, FileShare.ReadWrite);
        using var reader = new StreamReader(stream, Encoding.UTF8);
        while (reader.ReadLine() is { } line)
        {
            if (!string.IsNullOrWhiteSpace(line) && !";#%*".Contains(line[0]) && char.IsDigit(line[0]))
            {
                var dateAsInt = ParseDateFromTransactionText(line);
                index[++transactionIdCounter] = new LedgerIndexRecord(currentOffset, dateAsInt);
            }

            currentOffset = reader.BaseStream.Position;
        }
    }

    /// <summary>
    /// Legge un file Piano dei Conti (.coa) e popola la struttura dati corrispondente nel contesto.
    /// </summary>
    public static void LoadChartOfAccounts(string coaPath, LedgerContext context)
    {
        foreach (var line in File.ReadLines(coaPath))
        {
            if (string.IsNullOrWhiteSpace(line) || ";#%*".Contains(line[0])) continue;
            var descriptionSeparatorIndex = line.IndexOf("  ", StringComparison.Ordinal);
            if (descriptionSeparatorIndex < 0) descriptionSeparatorIndex = line.IndexOf('\t');
            var accountFullName =
                descriptionSeparatorIndex > 0 ? line[..descriptionSeparatorIndex].Trim() : line.Trim();
            var description = descriptionSeparatorIndex > 0 ? line[descriptionSeparatorIndex..].Trim() : string.Empty;
            var typeSeparatorIndex = accountFullName.IndexOf(':');
            var accountType = typeSeparatorIndex > 0 ? accountFullName[..typeSeparatorIndex] : "Senza categoria";
            if (!string.IsNullOrEmpty(accountFullName))
                context.ChartOfAccounts[accountFullName] = new AccountDetails(accountType, description);
        }
    }

    #endregion

    #region Metodi di Parsing e Query

    /// <summary>
    /// Recupera e parsifica una singola transazione dato il suo ID.
    /// Utilizza una cache per evitare I/O su disco ripetuti.
    /// </summary>
    /// <param name="context">Il contesto del libro mastro aperto.</param>
    /// <param name="transactionId">L'ID della transazione da recuperare.</param>
    /// <returns>Un array di <see cref="LedgerEntry"/> che rappresenta la transazione completa.</returns>
    /// <exception cref="KeyNotFoundException">Lanciata se l'ID della transazione non viene trovato o è stato cancellato.</exception>
    public static LedgerEntry[] GetTransaction(LedgerContext context, long transactionId)
    {
        // Controlla prima la cache per evitare I/O su disco non necessario.
        if (context.TransactionCache.TryGetValue(transactionId, out var cachedTransaction))
        {
            return cachedTransaction;
        }

        // Cerca la transazione nell'indice.
        if (!context.TransactionIndex.TryGetValue(transactionId, out var record) || record.FileOffset == -1)
        {
            throw new KeyNotFoundException($"Transazione con ID {transactionId} non trovata o cancellata.");
        }

        // Leggi il blocco di transazione dal file di giornale.
        using var stream = new FileStream(context.JournalFilePath, FileMode.Open, FileAccess.Read, FileShare.ReadWrite);
        using var reader = new StreamReader(stream, Encoding.UTF8);

        stream.Seek(record.FileOffset, SeekOrigin.Begin);
        var transactionEntries = ParseTransactionBlock(reader, transactionId);

        // Aggiungi la transazione appena letta alla cache per accessi futuri.
        context.TransactionCache.TryAdd(transactionId, transactionEntries);

        return transactionEntries;
    }

    /// <summary>
    /// Esegue il parsing di un singolo blocco di transazione da un TextReader e lo valida.
    /// Questa versione è stata corretta per utilizzare una regex robusta per l'estrazione degli importi
    /// e per gestire correttamente i fine riga e gli spazi bianchi.
    /// </summary>
    public static LedgerEntry[] ParseTransactionBlock(TextReader reader, long transactionId)
    {
        var entries = new List<LedgerEntry>();
        var headerLine = reader.ReadLine()?.Trim() ?? string.Empty;
        if (string.IsNullOrEmpty(headerLine)) throw new InvalidDataException("Transaction block cannot be empty.");

        var headerParts = headerLine.Split([' ', '\t'], StringSplitOptions.RemoveEmptyEntries);
        if (!DateOnly.TryParse(headerParts[0], out var date))
            throw new InvalidDataException($"Invalid date format in header: '{headerLine}'");

        var status = ' ';
        var descriptionStart = 1;
        if (headerParts.Length > 1 && headerParts[1].Length == 1 && "*!".Contains(headerParts[1][0]))
        {
            status = headerParts[1][0];
            descriptionStart = 2;
        }

        var description = string.Join(" ", headerParts.Skip(descriptionStart));

        // Legge le righe successive (i "postings") che appartengono alla stessa transazione.
        // Un posting deve iniziare con uno spazio o un tab.
        while (reader.Peek() != -1 && " \t".Contains((char)reader.Peek()))
        {
            var line = reader.ReadLine();
            if (line == null) break;

            var trimmedLine = line.Trim();
            if (string.IsNullOrEmpty(trimmedLine)) continue;

            string account;
            var amountStr = "";
            var comment = "";

            var commentIndex = trimmedLine.IndexOf(';');
            if (commentIndex >= 0)
            {
                comment = trimmedLine[(commentIndex + 1)..].Trim();
                trimmedLine = trimmedLine[..commentIndex].Trim();
            }

            var amountMatch = MyAmountRegex().Match(trimmedLine);
            if (amountMatch.Success)
            {
                account = trimmedLine[..amountMatch.Index].TrimEnd();
                // La regex ha due gruppi di cattura: uno per [simbolo valuta facoltativo] + numero, 
                // e uno per solo il numero. Prende il primo gruppo che ha catturato un valore.
                amountStr = amountMatch.Groups[1].Success ? amountMatch.Groups[1].Value : amountMatch.Groups[2].Value;
            }
            else
            {
                account = trimmedLine;
            }

            decimal amount = 0;
            decimal exchangeRate = 1m;
            string? currency = null;
            var amountIsExplicit = false;

            if (!string.IsNullOrWhiteSpace(amountStr))
            {
                var parsed = ParseAmountWithCurrency(amountStr);
                amount = parsed.Amount;
                currency = parsed.Currency;
                exchangeRate = parsed.Rate;
                amountIsExplicit = true;
            }

            entries.Add(new LedgerEntry
            {
                TransactionId = transactionId, Date = date, Status = status, Description = description,
                Account = account, Amount = amount, Comment = string.IsNullOrWhiteSpace(comment) ? null : comment,
                AmountIsExplicit = amountIsExplicit,
                Currency = currency,
                ExchangeRate = exchangeRate
            });
        }

        if (!IsTransactionBalanced(entries))
            throw new InvalidDataException(
                $"Transaction ID {transactionId} at {date} ('{description}') is not balanced.");

        // Aggiorna l'ID della transazione per tutte le voci dopo il bilanciamento.
        return entries.Select(e => e with { TransactionId = transactionId }).ToArray();
    }

    /// <summary>
    /// Verifica se una transazione è bilanciata, calcolando l'importo mancante se necessario.
    /// </summary>
    public static bool IsTransactionBalanced(List<LedgerEntry> entries)
    {
        if (entries.Count == 0) return true;

        var entriesWithNoAmount = entries.Where(e => !e.AmountIsExplicit).ToList();
        if (entriesWithNoAmount.Count > 1) return false;

        var sum = entries.Sum(e => e.BaseAmount);

        if (entriesWithNoAmount.Count != 1) return Math.Abs(sum) < 0.0001m;
        var entryToFix = entriesWithNoAmount.Single();
        var index = entries.IndexOf(entryToFix);
        entries[index] = entryToFix with { Amount = -sum, ExchangeRate = 1m };
        return true;
    }

    /// <summary>Serializza un importo per il journal (es. <c>100.00 USD @ 0.95</c>).</summary>
    public static string FormatAmountForJournal(decimal amount, string? currency, decimal exchangeRate)
    {
        var amountStr = amount.ToString(CultureInfo.InvariantCulture);
        if (string.IsNullOrEmpty(currency))
            return amountStr;

        return exchangeRate == 1m
            ? $"{amountStr} {currency}"
            : $"{amountStr} {currency} @ {exchangeRate.ToString(CultureInfo.InvariantCulture)}";
    }

    /// <summary>
    /// Metodo helper che incapsula la logica di filtraggio per ottenere le voci di giornale.
    /// Questa versione è stata ottimizzata per:
    /// 1. Aprire il file di giornale una sola volta, evitando I/O ripetuti.
    /// 2. Utilizzare la cache delle transazioni per evitare di ri-parsificare dati già letti.
    /// </summary>
    public static List<LedgerEntry> GetFilteredEntries(LedgerContext context, string query)
    {
        var filters = ParseLedgerQuery(query);
        var relevantRecords = context.TransactionIndex
            .Where(kvp =>
            {
                var record = kvp.Value;
                if (record.FileOffset == -1) return false; // Ignora cancellati
                return filters.DateFilter == null || filters.DateFilter(record.Date);
            })
            .OrderBy(kvp => kvp.Value.Date)
            .ThenBy(kvp => kvp.Key) // Aggiungi ordinamento secondario per stabilità
            .ToList();

        var results = new List<LedgerEntry>();

        foreach (var (transactionId, _) in relevantRecords)
        {
            var transactionEntries = GetTransaction(context, transactionId);

            // Applica il filtro per conto in memoria dopo aver ottenuto la transazione.
            results.AddRange(transactionEntries.Where(entry =>
                filters.AccountFilter == null || filters.AccountFilter(entry.Account)));
        }

        return results;
    }

    /// <summary>
    /// Elenca i nomi conto dal piano dei conti o dal giornale, con filtro radice opzionale.
    /// </summary>
    /// <param name="context">Il contesto del libro mastro aperto.</param>
    /// <param name="rootAccount">Se non vuoto, restituisce il conto radice e i discendenti diretti/indiretti (<c>root:child</c>).</param>
    public static string[] GetAccounts(LedgerContext context, string? rootAccount = null)
    {
        string[] accounts;
        if (context.ChartOfAccounts.Count > 0)
        {
            accounts = [.. context.ChartOfAccounts.Keys.OrderBy(static k => k)];
        }
        else
        {
            accounts = GetFilteredEntries(context, string.Empty)
                .Select(static e => e.Account)
                .Distinct(StringComparer.OrdinalIgnoreCase)
                .OrderBy(static a => a)
                .ToArray();
        }

        if (string.IsNullOrWhiteSpace(rootAccount))
        {
            return accounts;
        }

        var root = rootAccount.Trim();
        return accounts.Where(a => IsUnderRootAccount(a, root)).ToArray();
    }

    // ponytail: colon-separated prefix only — "Expenses" matches "Expenses:Office", not wildcard or type segments before ':'
    private static bool IsUnderRootAccount(string account, string root) =>
        account.Equals(root, StringComparison.OrdinalIgnoreCase)
        || account.StartsWith(root + ":", StringComparison.OrdinalIgnoreCase);

    /// <summary>
    /// Parsifica una stringa di query in un oggetto contenente delegati di filtro.
    /// </summary>
    public static (Func<int, bool>? DateFilter, Func<string, bool>? AccountFilter) ParseLedgerQuery(string query)
    {
        Func<int, bool>? dateFilter = null;
        Func<string, bool>? accountFilter = null;
        if (string.IsNullOrWhiteSpace(query)) return (dateFilter, accountFilter);

        var accountMatch = MyAccountRegex().Match(query);
        if (accountMatch.Success)
        {
            var patternValue = accountMatch.Groups[1].Success
                ? accountMatch.Groups[1].Value
                : accountMatch.Groups[2].Value;
            var pattern = "^" + Regex.Escape(patternValue).Replace(@"\*", ".*") + "$";
            accountFilter = acc => Regex.IsMatch(acc, pattern, RegexOptions.IgnoreCase);
        }

        var dateMatch = MyDateRegex().Match(query);
        if (!dateMatch.Success || !DateOnly.TryParse(dateMatch.Groups[2].Value, out var dateValParsed))
            return (dateFilter, accountFilter);
        var op = dateMatch.Groups[1].Value;
        var dateVal = dateValParsed.Year * 10000 + dateValParsed.Month * 100 + dateValParsed.Day;
        dateFilter = op switch
        {
            ">" => d => d > dateVal, ">=" => d => d >= dateVal,
            "<" => d => d < dateVal, "<=" => d => d <= dateVal,
            "=" => d => d == dateVal, _ => null
        };

        return (dateFilter, accountFilter);
    }

    /// <summary>
    /// Esegue il parsing della prima riga di un blocco di testo per estrarre la data.
    /// </summary>
    private static int ParseDateFromTransactionText(string transactionHeader)
    {
        var parts = transactionHeader.Split(' ', StringSplitOptions.RemoveEmptyEntries);
        if (parts.Length > 0 && DateOnly.TryParse(parts[0], out var date))
            return date.Year * 10000 + date.Month * 100 + date.Day;
        var today = DateTime.Today;
        return today.Year * 10000 + today.Month * 100 + today.Day;
    }

    /// <summary>
    /// Metodo helper per leggere un intero blocco di transazione come testo grezzo.
    /// </summary>
    public static string? ReadTransactionBlockAsText(StreamReader reader)
    {
        var sb = new StringBuilder();
        string? line;

        if ((line = reader.ReadLine()) == null)
            return null; // EOF
        
        sb.AppendLine(line);

        while ((line = reader.ReadLine()) != null)
        {
            var trimmedLine = line.Trim();
            if (string.IsNullOrEmpty(trimmedLine)) break;
            if (char.IsDigit(trimmedLine[0]) && DateOnly.TryParse(trimmedLine.Split(' ')[0], out _))
            {
                reader.BaseStream.Seek(-Encoding.UTF8.GetByteCount(line) - Environment.NewLine.Length,
                    SeekOrigin.Current);
                break;
            }

            sb.AppendLine(line);
        }

        var result = sb.ToString().Trim();
        return string.IsNullOrEmpty(result) ? null : result;
    }

    [GeneratedRegex(@"date:([><=]+)(\d{4}-\d{2}-\d{2})")]
    private static partial Regex MyDateRegex();

    [GeneratedRegex(@"\s{2,}(.*)")]
    private static partial Regex MyAmountRegex();

    [GeneratedRegex("""
                    account:(?:"([^"]+)"|(\S+(?::\S+)*))
                    """)]
    private static partial Regex MyAccountRegex();

    #endregion

    #region Session Handles

    private static int _nextSessionHandle;

    /// <summary>
    /// Allocates a monotonic LEDGER OPEN session handle. Thread-safe; avoids <see cref="object.GetHashCode"/> collisions.
    /// </summary>
    public static int AllocateSessionHandle() => Interlocked.Increment(ref _nextSessionHandle);

    #endregion

    #region Public API per P-CODE VM

    /// <summary>
    /// Aggiunge una transazione al journal e restituisce il nuovo transaction ID.
    /// </summary>
    public static long AddTransaction(LedgerContext context, DateOnly date, string description,
        List<LedgerEntry> entries)
    {
        if (!IsTransactionBalanced(entries))
            throw new InvalidOperationException($"La transazione del {date:yyyy-MM-dd} ('{description}') non è bilanciata.");

        context.Lock.Wait();
        try
        {
            var sb = new StringBuilder();
            sb.Append(date.ToString("yyyy-MM-dd", CultureInfo.InvariantCulture)).Append(' ').Append(description);
            foreach (var entry in entries)
            {
                sb.AppendLine().Append("    ").Append(entry.Account);
                sb.Append("      ").Append(FormatAmountForJournal(entry.Amount, entry.Currency, entry.ExchangeRate));
            }

            var transactionText = sb.ToString();
            long newTransactionId;
            using (var fs = new FileStream(context.JournalFilePath, FileMode.OpenOrCreate, FileAccess.ReadWrite, FileShare.ReadWrite))
            using (var writer = new StreamWriter(fs, Encoding.UTF8))
            {
                var newOffset = fs.Length;
                if (newOffset > 0)
                {
                    fs.Seek(-1, SeekOrigin.End);
                    if (fs.ReadByte() != '\n') writer.WriteLine();
                    newOffset = fs.Position;
                }
                writer.WriteLine(transactionText);
                writer.Flush();
                newTransactionId = (context.TransactionIndex.Keys.Count != 0 ? context.TransactionIndex.Keys.Max() : 0) + 1;
                var dateAsInt = date.Year * 10000 + date.Month * 100 + date.Day;
                context.TransactionIndex[newTransactionId] = new LedgerIndexRecord(newOffset, dateAsInt);
            }
            context.IsDirty = true;
            return newTransactionId;
        }
        finally
        {
            context.Lock.Release();
        }
    }

    /// <summary>
    /// Elimina una transazione dal journal (la marca come cancellata).
    /// </summary>
    public static bool DeleteTransaction(LedgerContext context, long transactionId)
    {
        context.Lock.Wait();
        try
        {
            if (!context.TransactionIndex.TryGetValue(transactionId, out var record)) return false;
            var entries = GetTransaction(context, transactionId);
            if (entries.Length == 0) return false;

            var dateStr = entries[0].Date.ToString("yyyy-MM-dd", CultureInfo.InvariantCulture);
            var desc = entries[0].Description ?? "";
            var sb = new StringBuilder();
            sb.Append(dateStr).Append(" * ").Append(desc);
            foreach (var entry in entries)
            {
                sb.AppendLine().Append("    ").Append(entry.Account);
                sb.Append("      ").Append(FormatAmountForJournal(-entry.Amount, entry.Currency, entry.ExchangeRate));
            }

            var voidText = sb.ToString();
            using (var fs = new FileStream(context.JournalFilePath, FileMode.OpenOrCreate, FileAccess.ReadWrite, FileShare.ReadWrite))
            using (var writer = new StreamWriter(fs, Encoding.UTF8))
            {
                var newOffset = fs.Length;
                if (newOffset > 0)
                {
                    fs.Seek(-1, SeekOrigin.End);
                    if (fs.ReadByte() != '\n') writer.WriteLine();
                    newOffset = fs.Position;
                }
                writer.WriteLine(voidText);
                writer.Flush();
            }
            context.IsDirty = true;
            return true;
        }
        finally
        {
            context.Lock.Release();
        }
    }

    /// <summary>
    /// Modifica una transazione esistente: scrive lo storno (void) della vecchia e aggiunge la
    /// nuova con la data/descrizione/postings forniti. Ritorna il nuovo id transazione, o -1 se
    /// la transazione non esiste o i nuovi postings non sono bilanciati.
    /// </summary>
    public static long UpdateTransaction(LedgerContext context, long transactionId, DateOnly date,
        string description, List<LedgerEntry> entries)
    {
        if (!IsTransactionBalanced(entries))
            throw new InvalidOperationException($"La transazione del {date:yyyy-MM-dd} ('{description}') non è bilanciata.");
        if (!DeleteTransaction(context, transactionId)) return -1;
        return AddTransaction(context, date, description, entries);
    }

    /// <summary>
    /// Compatta il journal rimuovendo le transazioni cancellate.
    /// </summary>
    public static void PackJournal(LedgerContext context)
    {
        context.Lock.Wait();
        try
        {
            var tempPath = context.JournalFilePath + ".packed";
            var index = new ConcurrentDictionary<long, LedgerIndexRecord>();
            long nextId = 1;
            long offset = 0;

            using (var reader = new StreamReader(context.JournalFilePath, Encoding.UTF8))
            using (var writer = new StreamWriter(tempPath, false, Encoding.UTF8))
            {
                string? block;
                while ((block = ReadTransactionBlockAsText(reader)) != null)
                {
                    if (string.IsNullOrWhiteSpace(block)) continue;
                    writer.Write(block);
                    if (!block.EndsWith('\n')) writer.WriteLine();
                    index[nextId] = new LedgerIndexRecord(offset, 0);
                    offset = writer.BaseStream.Position;
                    nextId++;
                }
                writer.Flush();
            }

            File.Move(tempPath, context.JournalFilePath, true);
            context.TransactionIndex.Clear();
            foreach (var kv in index) context.TransactionIndex[kv.Key] = kv.Value;

            var indexFile = context.IndexFilePath;
            if (File.Exists(indexFile)) File.Delete(indexFile);
            SaveIndexToFile(indexFile, context.TransactionIndex);
            context.IsDirty = false;
        }
        finally
        {
            context.Lock.Release();
        }
    }

    #endregion
}