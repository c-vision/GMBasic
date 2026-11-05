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

        /* Scansione BYTE-oriented: gli offset devono essere posizioni reali nel file.
         * Con StreamReader + reader.BaseStream.Position gli offset erano sfalsati dal
         * buffer di lettura (le transazioni dalla seconda in poi sparivano/duplicavano). */
        var bytes = File.ReadAllBytes(journalPath);
        long transactionIdCounter = 0;
        var pos = 0;
        while (pos < bytes.Length)
        {
            var nl = Array.IndexOf(bytes, (byte)'\n', pos);
            var lineEnd = nl < 0 ? bytes.Length : nl;
            var line = Encoding.UTF8.GetString(bytes, pos, lineEnd - pos);
            var clean = line.TrimStart().TrimStart('\uFEFF');
            if (IsJournalHeaderLine(clean) && !HeaderIsVoid(clean))
            {
                var dateAsInt = ParseDateFromTransactionText(clean);
                index[++transactionIdCounter] = new LedgerIndexRecord(pos, dateAsInt);
            }
            pos = nl < 0 ? bytes.Length : nl + 1;
        }
    }

    /// <summary>True se la riga è l'header di un blocco (`YYYY-MM-DD …`).</summary>
    private static bool IsJournalHeaderLine(string line)
    {
        if (string.IsNullOrEmpty(line) || !char.IsDigit(line[0])) return false;
        var sp = line.IndexOf(' ');
        var token = sp < 0 ? line : line[..sp];
        return DateOnly.TryParseExact(token, "yyyy-MM-dd", CultureInfo.InvariantCulture,
            DateTimeStyles.None, out _);
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
            /* Il journal ha un BOM per OGNI blocco (StreamWriter creato per ogni append):
             * senza TrimStart('\uFEFF') l'header successivo non era riconosciuto come
             * inizio-blocco e veniva accorpato al precedente (PACK perdeva le transazioni
             * dalla seconda in poi). */
            var trimmedLine = line.Trim().TrimStart('\uFEFF');
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

    /// <summary>True se la riga di header è uno STORNO (`YYYY-MM-DD * …`).</summary>
    private static bool HeaderIsVoid(string line)
    {
        var t = line.Trim().TrimStart('\uFEFF');
        var parts = t.Split((char[]?)null, StringSplitOptions.RemoveEmptyEntries);
        return parts.Length >= 2 && parts[1] == "*";
    }

    /// <summary>Inizio del blocco indicizzato successivo (o fine file).</summary>
    private static long BlockEnd(LedgerContext context, long start, long fileLength)
    {
        var end = fileLength;
        foreach (var kv in context.TransactionIndex)
            if (kv.Value.FileOffset > start && kv.Value.FileOffset < end)
                end = kv.Value.FileOffset;
        return end;
    }

    /// <summary>Riscrive il journal sostituendo [start,end) con <paramref name="replacement"/>
    /// (vuoto = rimozione) e riallinea gli offset dei blocchi successivi. Il blocco
    /// <paramref name="fixedId"/> resta a <paramref name="start"/>. Svuota la cache.</summary>
    private static void RewriteJournalRange(LedgerContext context, long start, long end,
        byte[]? replacement, long fixedId)
    {
        var bytes = File.ReadAllBytes(context.JournalFilePath);
        var repl = replacement ?? [];
        var outBytes = new byte[start + repl.Length + (bytes.Length - end)];
        Buffer.BlockCopy(bytes, 0, outBytes, 0, (int)start);
        if (repl.Length > 0) Buffer.BlockCopy(repl, 0, outBytes, (int)start, repl.Length);
        Buffer.BlockCopy(bytes, (int)end, outBytes, (int)(start + repl.Length), (int)(bytes.Length - end));
        var tmp = context.JournalFilePath + ".tmp";
        File.WriteAllBytes(tmp, outBytes);
        File.Move(tmp, context.JournalFilePath, true);
        var delta = repl.Length - (end - start);
        foreach (var key in context.TransactionIndex.Keys)
        {
            var rec = context.TransactionIndex[key];
            if (key != fixedId && rec.FileOffset >= end)
                context.TransactionIndex[key] = rec with { FileOffset = rec.FileOffset + delta };
        }
        context.TransactionCache.Clear();
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
            var fileLen = new FileInfo(context.JournalFilePath).Length;
            var end = BlockEnd(context, record.FileOffset, fileLen);
            if (end <= record.FileOffset) return false;
            var bytes = File.ReadAllBytes(context.JournalFilePath);
            var startOffset = (int)record.FileOffset;
            var headerEnd = Array.IndexOf(bytes, (byte)'\n', startOffset);
            if (headerEnd < 0) headerEnd = bytes.Length;
            var p = startOffset;
            if (bytes.Length >= p + 3 && bytes[p] == 0xEF && bytes[p + 1] == 0xBB && bytes[p + 2] == 0xBF) p += 3;
            var insertAt = -1;
            for (var i = p; i < headerEnd; i++)
                if (bytes[i] == (byte)' ' || bytes[i] == (byte)'\t') { insertAt = i + 1; break; }
            if (insertAt < 0) return false;
            var q = insertAt;
            while (q < headerEnd && (bytes[q] == (byte)' ' || bytes[q] == (byte)'\t')) q++;
            /* Già storno: nulla da fare nel journal (caso difensivo). */
            if (!(q < headerEnd && bytes[q] == (byte)'*'))
            {
                var repl = new byte[end - startOffset + 2];
                Buffer.BlockCopy(bytes, startOffset, repl, 0, insertAt - startOffset);
                repl[insertAt - startOffset] = (byte)'*';
                repl[insertAt - startOffset + 1] = (byte)' ';
                Buffer.BlockCopy(bytes, insertAt, repl, insertAt - startOffset + 2, (int)end - insertAt);
                RewriteJournalRange(context, record.FileOffset, end, repl, transactionId);
            }
            /* Lo storno resta nel journal (storia) ma NON è una transazione attiva:
             * COUNT/VARIANCE/BALANCE/REGISTER non lo vedono, Rebuild/Pack lo ignorano. */
            context.TransactionIndex.TryRemove(transactionId, out _);
            context.IsDirty = true;
            return true;
        }
        finally
        {
            context.Lock.Release();
        }
    }

    /// <summary>
    /// Modifica una transazione esistente SOSTITUENDOLA nello stesso id (guida §LEDGER EDIT:
    /// "preserva id e categoria"). Prima era delete+add con un nuovo id e COUNT incrementato.
    /// </summary>
    public static long UpdateTransaction(LedgerContext context, long transactionId, DateOnly date,
        string description, List<LedgerEntry> entries)
    {
        if (!IsTransactionBalanced(entries))
            throw new InvalidOperationException($"La transazione del {date:yyyy-MM-dd} ('{description}') non è bilanciata.");
        context.Lock.Wait();
        try
        {
            if (!context.TransactionIndex.TryGetValue(transactionId, out var record)) return -1;
            var fileLen = new FileInfo(context.JournalFilePath).Length;
            var end = BlockEnd(context, record.FileOffset, fileLen);
            if (end <= record.FileOffset) return -1;
            var sb = new StringBuilder();
            sb.Append(date.ToString("yyyy-MM-dd", CultureInfo.InvariantCulture)).Append(' ').Append(description);
            foreach (var entry in entries)
            {
                sb.AppendLine().Append("    ").Append(entry.Account);
                sb.Append("      ").Append(FormatAmountForJournal(entry.Amount, entry.Currency, entry.ExchangeRate));
            }
            sb.AppendLine();
            var bytes = File.ReadAllBytes(context.JournalFilePath);
            var p = (int)record.FileOffset;
            var hasBom = bytes.Length >= p + 3 && bytes[p] == 0xEF && bytes[p + 1] == 0xBB && bytes[p + 2] == 0xBF;
            var text = (hasBom ? "\uFEFF" : "") + sb;
            RewriteJournalRange(context, record.FileOffset, end, Encoding.UTF8.GetBytes(text), transactionId);
            var dateAsInt = date.Year * 10000 + date.Month * 100 + date.Day;
            context.TransactionIndex[transactionId] = new LedgerIndexRecord(record.FileOffset, dateAsInt);
            context.IsDirty = true;
            return transactionId;
        }
        finally
        {
            context.Lock.Release();
        }
    }

    /// <summary>
    /// Compatta il journal rimuovendo le transazioni cancellate.
    /// </summary>
    public static void PackJournal(LedgerContext context)
    {
        context.Lock.Wait();
        try
        {
            var bytes = File.ReadAllBytes(context.JournalFilePath);
            var blocks = SplitJournalBlocks(bytes);
            var tempPath = context.JournalFilePath + ".packed";
            var index = new ConcurrentDictionary<long, LedgerIndexRecord>();
            long nextId = 1;
            long offset = 0;
            using (var outStream = new FileStream(tempPath, FileMode.Create, FileAccess.Write, FileShare.None))
            {
                var first = true;
                foreach (var block in blocks)
                {
                    /* Compaction: gli storni non sono transazioni → non copiati né indicizzati. */
                    if (block.IsVoid) continue;
                    var start = block.Start;
                    /* BOM solo all'inizio del file compattato. */
                    if (!first && bytes.Length >= start + 3
                        && bytes[start] == 0xEF && bytes[start + 1] == 0xBB && bytes[start + 2] == 0xBF)
                        start += 3;
                    var len = block.End - start;
                    if (len <= 0) continue;
                    outStream.Write(bytes, start, len);
                    if (bytes[block.End - 1] != (byte)'\n') outStream.WriteByte((byte)'\n');
                    outStream.Flush();
                    index[nextId] = new LedgerIndexRecord(offset, 0);
                    offset = outStream.Position;
                    nextId++;
                    first = false;
                }
                outStream.Flush();
            }

            File.Move(tempPath, context.JournalFilePath, true);
            context.TransactionIndex.Clear();
            context.TransactionCache.Clear();
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

    /// <summary>Blocchi del journal come intervalli di byte [Start,End) + flag storno.</summary>
    private static List<(int Start, int End, bool IsVoid)> SplitJournalBlocks(byte[] bytes)
    {
        var starts = new List<int>();
        var voids = new List<bool>();
        var pos = 0;
        while (pos < bytes.Length)
        {
            var nl = Array.IndexOf(bytes, (byte)'\n', pos);
            var lineEnd = nl < 0 ? bytes.Length : nl;
            var line = Encoding.UTF8.GetString(bytes, pos, lineEnd - pos);
            var clean = line.TrimStart().TrimStart('\uFEFF');
            if (IsJournalHeaderLine(clean))
            {
                starts.Add(pos);
                voids.Add(HeaderIsVoid(clean));
            }
            pos = nl < 0 ? bytes.Length : nl + 1;
        }
        var result = new List<(int, int, bool)>(starts.Count);
        for (var i = 0; i < starts.Count; i++)
            result.Add((starts[i], i + 1 < starts.Count ? starts[i + 1] : bytes.Length, voids[i]));
        return result;
    }

    #endregion
}