using System.Globalization;
using System.Text;
using System.Text.RegularExpressions;

// ReSharper disable CheckNamespace

namespace Basic.Ledger;

public sealed partial class LedgerManager
{
    private static string BudgetKey(string account, string period) =>
        $"{account.Trim()}|{period.Trim()}";

    /// <summary>Loads budget lines from the collateral <c>.budget</c> file.</summary>
    public static void LoadBudgets(string budgetPath, LedgerContext context)
    {
        context.Budgets.Clear();
        if (!File.Exists(budgetPath))
        {
            return;
        }

        foreach (var line in File.ReadLines(budgetPath))
        {
            if (string.IsNullOrWhiteSpace(line) || ";#%*".Contains(line[0]))
            {
                continue;
            }

            var parts = line.Split([' ', '\t'], StringSplitOptions.RemoveEmptyEntries);
            if (parts.Length < 3)
            {
                continue;
            }

            var account = parts[0];
            if (!decimal.TryParse(parts[1], NumberStyles.Any, CultureInfo.InvariantCulture, out var amount))
            {
                continue;
            }

            var period = parts[2];
            context.Budgets[BudgetKey(account, period)] = new BudgetEntry(account, amount, period);
        }
    }

    /// <summary>Persists in-memory budgets to the <c>.budget</c> sidecar file.</summary>
    public static void SaveBudgets(string budgetPath, LedgerContext context)
    {
        var tempFile = budgetPath + ".tmp";
        try
        {
            using (var writer = new StreamWriter(tempFile, false, Encoding.UTF8))
            {
                foreach (var entry in context.Budgets.Values.OrderBy(e => e.Period).ThenBy(e => e.Account))
                {
                    writer.Write(entry.Account);
                    writer.Write("  ");
                    writer.Write(entry.Amount.ToString(CultureInfo.InvariantCulture));
                    writer.Write("  ");
                    writer.WriteLine(entry.Period);
                }
            }

            File.Move(tempFile, budgetPath, true);
            context.BudgetsDirty = false;
        }
        catch
        {
            if (File.Exists(tempFile))
            {
                File.Delete(tempFile);
            }

            throw;
        }
    }

    /// <summary>Sets or replaces a budget line for an account and period (YYYY-MM).</summary>
    public static void SetBudget(LedgerContext context, string account, decimal amount, string period)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(account);
        ArgumentException.ThrowIfNullOrWhiteSpace(period);
        context.Budgets[BudgetKey(account, period)] = new BudgetEntry(account.Trim(), amount, period.Trim());
        context.BudgetsDirty = true;
    }

    /// <summary>Computes budget vs actual variance for one account/period.</summary>
    public static BudgetVarianceEntry GetVariance(LedgerContext context, string account, string period)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(account);
        ArgumentException.ThrowIfNullOrWhiteSpace(period);

        var key = BudgetKey(account, period);
        var budgetAmount = context.Budgets.TryGetValue(key, out var budget)
            ? budget.Amount
            : 0m;

        if (!TryParseBudgetPeriod(period, out var year, out var month))
        {
            throw new ArgumentException($"Invalid budget period '{period}'. Expected YYYY-MM.", nameof(period));
        }

        var periodStart = new DateOnly(year, month, 1);
        var periodEnd = periodStart.AddMonths(1).AddDays(-1);
        var startInt = periodStart.Year * 10000 + periodStart.Month * 100 + periodStart.Day;
        var endInt = periodEnd.Year * 10000 + periodEnd.Month * 100 + periodEnd.Day;

        var actual = GetFilteredEntries(context, $"account:{account}")
            .Where(e => e.Date >= periodStart && e.Date <= periodEnd)
            .Sum(e => e.BaseAmount);

        return new BudgetVarianceEntry(account.Trim(), period.Trim(), budgetAmount, actual, budgetAmount - actual);
    }

    /// <summary>
    /// Imports CSV bank rows into balanced transactions.
    /// Mapping: <c>date=0,desc=1,account=2,amount=3,contra=Assets:Bank</c> (header row skipped when present).
    /// </summary>
    public static int ImportCsv(LedgerContext context, string csvPath, string mappingRules)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(csvPath);
        ArgumentException.ThrowIfNullOrWhiteSpace(mappingRules);

        var (mapping, contraAccount) = ParseImportMapping(mappingRules);
        if (!mapping.TryGetValue("date", out var dateCol) ||
            !mapping.TryGetValue("account", out var accountCol) ||
            !mapping.TryGetValue("amount", out var amountCol))
        {
            throw new ArgumentException(
                "Mapping must include date, account, and amount columns (e.g. date=0,account=2,amount=3).",
                nameof(mappingRules));
        }

        mapping.TryGetValue("desc", out var descCol);
        contraAccount ??= "Equity:Import";

        var imported = 0;
        var lines = File.ReadAllLines(csvPath);
        var startRow = 0;
        if (lines.Length > 0 && lines[0].Contains("date", StringComparison.OrdinalIgnoreCase))
        {
            startRow = 1;
        }

        context.Lock.Wait();
        try
        {
            for (var row = startRow; row < lines.Length; row++)
            {
                var line = lines[row];
                if (string.IsNullOrWhiteSpace(line))
                {
                    continue;
                }

                var cols = SplitCsvLine(line);
                if (cols.Length <= Math.Max(dateCol, Math.Max(accountCol, amountCol)))
                {
                    continue;
                }

                if (!DateOnly.TryParse(cols[dateCol].Trim(), out var date))
                {
                    continue;
                }

                if (!decimal.TryParse(cols[amountCol].Trim(), NumberStyles.Any, CultureInfo.InvariantCulture,
                        out var amount))
                {
                    continue;
                }

                var account = cols[accountCol].Trim();
                var description = descCol >= 0 && descCol < cols.Length ? cols[descCol].Trim() : "CSV import";

                AppendImportedTransaction(context, date, description, account, amount, contraAccount);
                imported++;
            }

            if (imported > 0)
            {
                context.IsDirty = true;
            }
        }
        finally
        {
            context.Lock.Release();
        }

        return imported;
    }

    /// <summary>Balance rows suitable for charting (account label + base-currency amount).</summary>
    public static (string[] Labels, decimal[] Amounts) GetBalancePlotData(LedgerContext context, string query)
    {
        var balances = GetFilteredEntries(context, query)
            .GroupBy(e => e.Account, StringComparer.OrdinalIgnoreCase)
            .Select(g => new LedgerBalanceEntry(g.Key, g.Sum(e => e.BaseAmount)))
            .OrderBy(b => b.Account)
            .ToArray();

        return (
            balances.Select(b => b.Account).ToArray(),
            balances.Select(b => b.TotalAmount).ToArray());
    }

    internal static void AppendImportedTransaction(
        LedgerContext context,
        DateOnly date,
        string description,
        string account,
        decimal amount,
        string contraAccount)
    {
        var entries = new List<LedgerEntry>
        {
            new()
            {
                TransactionId = 0,
                Date = date,
                Status = ' ',
                Description = description,
                Account = account,
                Amount = amount,
                AmountIsExplicit = true,
            },
            new()
            {
                TransactionId = 0,
                Date = date,
                Status = ' ',
                Description = description,
                Account = contraAccount,
                Amount = -amount,
                AmountIsExplicit = true,
            },
        };

        if (!IsTransactionBalanced(entries))
        {
            throw new InvalidDataException($"Imported row '{description}' is not balanced.");
        }

        var sb = new StringBuilder();
        sb.Append(date.ToString("yyyy-MM-dd", CultureInfo.InvariantCulture)).Append(' ').Append(description);
        foreach (var entry in entries)
        {
            sb.AppendLine().Append("    ").Append(entry.Account);
            sb.Append("      ").Append(entry.Amount.ToString(CultureInfo.InvariantCulture));
        }

        using var stream = new FileStream(context.JournalFilePath, FileMode.OpenOrCreate, FileAccess.ReadWrite,
            FileShare.ReadWrite);
        using var writer = new StreamWriter(stream, Encoding.UTF8);
        var newOffset = stream.Length;
        if (newOffset > 0)
        {
            stream.Seek(-1, SeekOrigin.End);
            if (stream.ReadByte() != '\n')
            {
                writer.WriteLine();
            }

            newOffset = stream.Position;
        }

        writer.WriteLine(sb.ToString());
        writer.Flush();

        var newTransactionId = (context.TransactionIndex.Keys.Count != 0 ? context.TransactionIndex.Keys.Max() : 0) + 1;
        var dateAsInt = date.Year * 10000 + date.Month * 100 + date.Day;
        context.TransactionIndex[newTransactionId] = new LedgerIndexRecord(newOffset, dateAsInt);
    }

    private static (Dictionary<string, int> Columns, string? ContraAccount) ParseImportMapping(string rules)
    {
        var map = new Dictionary<string, int>(StringComparer.OrdinalIgnoreCase);
        string? contra = null;
        foreach (var part in rules.Split(',', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries))
        {
            var eq = part.IndexOf('=');
            if (eq <= 0)
            {
                continue;
            }

            var key = part[..eq].Trim();
            var value = part[(eq + 1)..].Trim();
            if (key.Equals("contra", StringComparison.OrdinalIgnoreCase))
            {
                contra = value;
                continue;
            }

            if (int.TryParse(value, NumberStyles.Integer, CultureInfo.InvariantCulture, out var col))
            {
                map[key] = col;
            }
        }

        return (map, contra);
    }

    private static bool TryParseBudgetPeriod(string period, out int year, out int month)
    {
        year = 0;
        month = 0;
        var parts = period.Split('-');
        return parts.Length == 2
               && int.TryParse(parts[0], NumberStyles.Integer, CultureInfo.InvariantCulture, out year)
               && int.TryParse(parts[1], NumberStyles.Integer, CultureInfo.InvariantCulture, out month)
               && month is >= 1 and <= 12;
    }

    private static string[] SplitCsvLine(string line)
    {
        var fields = new List<string>();
        var current = new StringBuilder();
        var inQuotes = false;
        foreach (var c in line)
        {
            switch (c)
            {
                case '"':
                    inQuotes = !inQuotes;
                    continue;
                case ',' when !inQuotes:
                    fields.Add(current.ToString());
                    current.Clear();
                    continue;
                default:
                    current.Append(c);
                    break;
            }
        }

        fields.Add(current.ToString());
        return fields.ToArray();
    }

    private static (decimal Amount, string? Currency, decimal Rate) ParseAmountWithCurrency(string amountStr)
    {
        var trimmed = amountStr.Trim();
        if (string.IsNullOrEmpty(trimmed))
        {
            return (0, null, 1m);
        }

        var match = CurrencyAmountRegex.Match(trimmed);
        if (match.Success)
        {
            var amountPart = match.Groups[1].Value.Replace(",", "");
            if (!decimal.TryParse(amountPart, NumberStyles.Any, CultureInfo.InvariantCulture, out var amount))
            {
                return (0, null, 1m);
            }

            var currency = match.Groups[2].Success ? match.Groups[2].Value : null;
            var rate = match.Groups[3].Success &&
                       decimal.TryParse(match.Groups[3].Value, NumberStyles.Any, CultureInfo.InvariantCulture,
                           out var parsedRate)
                ? parsedRate
                : 1m;

            return (amount, currency, rate);
        }

        var legacy = LegacySymbolAmountRegex.Match(trimmed);
        if (legacy.Success &&
            decimal.TryParse(legacy.Groups[2].Value.Replace(",", ""), NumberStyles.Any, CultureInfo.InvariantCulture,
                out var legacyAmount))
        {
            var sign = legacy.Groups[1].Value == "-" ? -1m : 1m;
            return (legacyAmount * sign, null, 1m);
        }

        var cleaned = trimmed.Replace(",", "").TrimStart('$', '€', '£', '¥', ' ').Trim();
        return decimal.TryParse(cleaned, NumberStyles.Any, CultureInfo.InvariantCulture, out var plain)
            ? (plain, null, 1m)
            : (0, null, 1m);
    }

    // ponytail: static Regex — [GeneratedRegex] partial in this file did not emit implementations in IDE build
    private static readonly Regex LegacySymbolAmountRegex = new(
        @"^([+-]?)[\$€£¥]?\s*(\d+(?:\.\d+)?)",
        RegexOptions.CultureInvariant | RegexOptions.Compiled);

    private static readonly Regex CurrencyAmountRegex = new(
        @"^([+-]?\d+(?:\.\d+)?)\s+([A-Z]{3})(?:\s+@\s+([+-]?\d+(?:\.\d+)?))?",
        RegexOptions.IgnoreCase | RegexOptions.CultureInvariant | RegexOptions.Compiled);
}
