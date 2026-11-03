// FILE: StdLib.Numerics.Finance.cs
// ReSharper disable CheckNamespace
// ReSharper disable InconsistentNaming

namespace CommonLib;

/// <summary>
/// Fornisce funzioni finanziarie per il runtime BASIC (mutui, prestiti, obbligazioni, ammortamento, VAN, TIR).
/// Contiene i seguenti metodi:
/// <list type="bullet">
///   <item><description><c>Pmt</c> — Calcolo della rata periodica (pagamento) di un prestito.</description></item>
///   <item><description><c>PmtExt</c> — Rata periodica con valore futuro e tipo opzionali.</description></item>
///   <item><description><c>Fv</c> — Valore futuro di una rendita.</description></item>
///   <item><description><c>FvExt</c> — Valore futuro con tipo di pagamento.</description></item>
///   <item><description><c>Pv</c> — Valore attuale di una rendita.</description></item>
///   <item><description><c>PvExt</c> — Valore attuale con valore futuro e tipo.</description></item>
///   <item><description><c>Nper</c> — Numero di periodi di un prestito.</description></item>
///   <item><description><c>NperExt</c> — Numero di periodi con valore futuro e tipo.</description></item>
///   <item><description><c>Cumipmt</c> — Interessi cumulativi pagati tra due periodi.</description></item>
///   <item><description><c>Cumprinc</c> — Capitale cumulativo rimborsato tra due periodi.</description></item>
///   <item><description><c>Sln</c> — Ammortamento a quote costanti (lineare).</description></item>
///   <item><description><c>Syd</c> — Ammortamento a quote decrescenti (somma anni).</description></item>
///   <item><description><c>Db</c> — Ammortamento a tasso fisso decrescente.</description></item>
///   <item><description><c>Ddb</c> — Ammortamento a doppio tasso decrescente.</description></item>
///   <item><description><c>Npv</c> — Valore attuale netto (VAN).</description></item>
///   <item><description><c>Irr</c> — Tasso interno di rendimento (TIR).</description></item>
///   <item><description><c>Xnpv</c> — VAN con date non periodiche.</description></item>
///   <item><description><c>Xirr</c> — TIR con date non periodiche.</description></item>
///   <item><description><c>Mirr</c> — TIR modificato (con tasso di reinvestimento).</description></item>
///   <item><description><c>Mnpv</c> — VAN modificato (con reinvestimento).</description></item>
///   <item><description><c>PriceSimple</c> — Prezzo di un'obbligazione con cedole periodiche semplici.</description></item>
///   <item><description><c>Price</c> — Prezzo di un'obbligazione con convenzioni calendariali.</description></item>
///   <item><description><c>Yield</c> — Rendimento effettivo di un'obbligazione.</description></item>
///   <item><description><c>Duration</c> — Duration Macaulay o modificata di un'obbligazione.</description></item>
///   <item><description><c>Rate</c> — Tasso di interesse periodico di una rendita.</description></item>
///   <item><description><c>Days360</c> — Conteggio giorni in base 360 (NASD).</description></item>
///   <item><description><c>YearFrac</c> — Frazione di anno tra due date secondo diverse basi.</description></item>
///   <item><description><c>GetNextCoupon</c> — Data della prossima cedola dopo il regolamento.</description></item>
///   <item><description><c>PrevCoupon</c> — Data della cedola precedente.</description></item>
///   <item><description><c>AddMonths</c> — Aggiunge mesi a una data rispettando fine mese.</description></item>
///   <item><description><c>DayCount</c> — Conteggio giorni tra due date secondo la convenzione scelta.</description></item>
/// </list>
/// </summary>
public static partial class Numerics
{
    /// <summary>
    /// Calcola la rata periodica (pagamento) di un prestito, dato il tasso, il numero di periodi e il valore attuale.
    /// </summary>
    /// <param name="rate">Tasso di interesse periodico.</param>
    /// <param name="nper">Numero totale di periodi.</param>
    /// <param name="pv">Valore attuale (capitale del prestito).</param>
    /// <returns>L'importo della rata periodica (negativo indica un pagamento in uscita).</returns>
    /// <exception cref="ArgumentOutOfRangeException">Se nper è zero.</exception>
    /// <example>decimal pmt = Numerics.Pmt(0.05m, 12, 10000m); // restituisce circa -1128.25m</example>
    public static decimal Pmt(decimal rate, decimal nper, decimal pv)
    {
        if (nper == 0) throw new ArgumentOutOfRangeException(nameof(nper), "Il numero di periodi non può essere zero.");
        if (rate == 0) return -pv / nper;
        var rateD = (double)rate;
        var nperD = (double)nper;
        var pow = (decimal)Math.Pow(1 + rateD, nperD);
        var pmt = -pv * rate * pow / (pow - 1);
        return pmt;
    }

    /// <summary>
    /// Calcola la rata periodica con parametri estesi: valore futuro e tipo di pagamento (inizio/fine periodo).
    /// </summary>
    /// <param name="rate">Tasso di interesse periodico.</param>
    /// <param name="nper">Numero totale di periodi.</param>
    /// <param name="pv">Valore attuale (capitale del prestito).</param>
    /// <param name="fv">Valore futuro desiderato dopo l'ultimo pagamento (default 0).</param>
    /// <param name="type">Tipo di pagamento: 0 = fine periodo, 1 = inizio periodo (default 0).</param>
    /// <returns>L'importo della rata periodica.</returns>
    /// <exception cref="ArgumentOutOfRangeException">Se nper è zero.</exception>
    /// <example>decimal pmt = Numerics.PmtExt(0.05m, 12, 10000m, 0, 0); // rata fine periodo</example>
    public static decimal PmtExt(decimal rate, decimal nper, decimal pv, decimal fv = 0, int type = 0)
    {
        if (nper == 0) throw new ArgumentOutOfRangeException(nameof(nper));
        if (rate == 0) return -(pv + fv) / nper;
        var rateD = (double)rate;
        var nperD = (double)nper;
        var pow = (decimal)Math.Pow(1 + rateD, nperD);
        var pmt = (-pv * pow - fv) / ((1 + rate * type) * (pow - 1) / rate);
        return pmt;
    }

    /// <summary>
    /// Calcola il valore futuro di una rendita basata su pagamenti periodici costanti e un tasso di interesse costante.
    /// </summary>
    /// <param name="rate">Tasso di interesse periodico.</param>
    /// <param name="nper">Numero totale di periodi.</param>
    /// <param name="pmt">Importo del pagamento periodico.</param>
    /// <param name="pv">Valore attuale (capitale iniziale).</param>
    /// <returns>Il valore futuro della rendita.</returns>
    /// <example>decimal fv = Numerics.Fv(0.05m, 12, 1000m, 0m); // valore futuro di 12 rate</example>
    public static decimal Fv(decimal rate, decimal nper, decimal pmt, decimal pv)
    {
        if (rate == 0) return -(pv + pmt * nper);
        var rateD = (double)rate;
        var nperD = (double)nper;
        var pow = (decimal)Math.Pow(1 + rateD, nperD);
        var fv = -pv * pow - pmt * (pow - 1) / rate;
        return fv;
    }

    /// <summary>
    /// Calcola il valore futuro di una rendita con tipo di pagamento specificato (inizio/fine periodo).
    /// </summary>
    /// <param name="rate">Tasso di interesse periodico.</param>
    /// <param name="nper">Numero totale di periodi.</param>
    /// <param name="pmt">Importo del pagamento periodico.</param>
    /// <param name="pv">Valore attuale (capitale iniziale).</param>
    /// <param name="type">Tipo di pagamento: 0 = fine periodo, 1 = inizio periodo (default 0).</param>
    /// <returns>Il valore futuro della rendita.</returns>
    /// <example>decimal fv = Numerics.FvExt(0.05m, 12, 1000m, 0m, 0);</example>
    public static decimal FvExt(decimal rate, decimal nper, decimal pmt, decimal pv, int type = 0)
    {
        if (rate == 0) return -(pv + pmt * nper);
        var rateD = (double)rate;
        var nperD = (double)nper;
        var pow = (decimal)Math.Pow(1 + rateD, nperD);
        var fv = -pv * pow - pmt * (1 + rate * type) * (pow - 1) / rate;
        return fv;
    }

    /// <summary>
    /// Calcola il valore attuale di una rendita: l'importo che una serie di pagamenti futuri vale oggi.
    /// </summary>
    /// <param name="rate">Tasso di interesse periodico.</param>
    /// <param name="nper">Numero totale di periodi.</param>
    /// <param name="pmt">Importo del pagamento periodico.</param>
    /// <returns>Il valore attuale della rendita.</returns>
    /// <example>decimal pv = Numerics.Pv(0.05m, 12, 1000m); // restituisce circa -8863.25m</example>
    public static decimal Pv(decimal rate, decimal nper, decimal pmt)
    {
        if (rate == 0) return -pmt * nper;
        var rateD = (double)rate;
        var nperD = (double)nper;
        var pow = (decimal)Math.Pow(1 + rateD, nperD);
        var pv = -pmt * (1 - 1 / pow) / rate;
        return pv;
    }

    /// <summary>
    /// Calcola il valore attuale di una rendita con valore futuro e tipo di pagamento opzionali.
    /// </summary>
    /// <param name="rate">Tasso di interesse periodico.</param>
    /// <param name="nper">Numero totale di periodi.</param>
    /// <param name="pmt">Importo del pagamento periodico.</param>
    /// <param name="fv">Valore futuro desiderato (default 0).</param>
    /// <param name="type">Tipo di pagamento: 0 = fine periodo, 1 = inizio periodo (default 0).</param>
    /// <returns>Il valore attuale della rendita.</returns>
    /// <example>decimal pv = Numerics.PvExt(0.05m, 12, 1000m, 0m, 0);</example>
    public static decimal PvExt(decimal rate, decimal nper, decimal pmt, decimal fv = 0, int type = 0)
    {
        if (rate == 0) return -(pmt * nper + fv);
        var rateD = (double)rate;
        var nperD = (double)nper;
        var pow = (decimal)Math.Pow(1 + rateD, nperD);
        var pv = (-pmt * (1 + rate * type) * (1 - 1 / pow) / rate) - fv / pow;
        return pv;
    }

    /// <summary>
    /// Calcola il numero di periodi necessari per estinguere un prestito o raggiungere un obiettivo finanziario.
    /// </summary>
    /// <param name="rate">Tasso di interesse periodico.</param>
    /// <param name="pmt">Importo del pagamento periodico.</param>
    /// <param name="pv">Valore attuale (capitale del prestito).</param>
    /// <returns>Il numero di periodi.</returns>
    /// <exception cref="ArgumentException">Se il tasso è zero e pmt è zero, o se il logaritmo non è calcolabile.</exception>
    /// <example>double nper = Numerics.Nper(0.05, 1000, 10000); // numero rate per estinguere 10000</example>
    public static double Nper(double rate, double pmt, double pv)
    {
        if (rate == 0)
        {
            if (pmt == 0)
                throw new ArgumentException("Il pagamento (pmt) non può essere zero se il tasso è zero.", nameof(pmt));
            return -pv / pmt;
        }
        var val = pmt / (pmt + pv * rate);
        if (val <= 0)
            throw new ArgumentException("Impossibile calcolare NPER (l'argomento del logaritmo non è positivo).");
        return Math.Log(val) / Math.Log(1 + rate);
    }

    /// <summary>
    /// Calcola il numero di periodi con parametri estesi: valore futuro e tipo di pagamento.
    /// </summary>
    /// <param name="rate">Tasso di interesse periodico.</param>
    /// <param name="pmt">Importo del pagamento periodico.</param>
    /// <param name="pv">Valore attuale (capitale del prestito).</param>
    /// <param name="fv">Valore futuro desiderato (default 0).</param>
    /// <param name="type">Tipo di pagamento: 0 = fine periodo, 1 = inizio periodo (default 0).</param>
    /// <returns>Il numero di periodi.</returns>
    /// <exception cref="ArgumentException">Se il calcolo non è possibile (divisione per zero o logaritmo non positivo).</exception>
    /// <example>double nper = Numerics.NperExt(0.05, 1000, 10000);</example>
    public static double NperExt(double rate, double pmt, double pv, double fv = 0, int type = 0)
    {
        if (rate == 0)
        {
            if (pmt == 0)
                throw new ArgumentException("Il pagamento (pmt) non può essere zero se il tasso è zero.", nameof(pmt));
            return -(fv + pv) / pmt;
        }
        var numerator = pmt * (1 + rate * type) - fv * rate;
        var denominator = pmt * (1 + rate * type) + pv * rate;
        if (denominator == 0 || numerator / denominator <= 0)
            throw new ArgumentException("Impossibile calcolare NPER (divisione per zero o argomento log non positivo).");
        return Math.Log(numerator / denominator) / Math.Log(1 + rate);
    }

    /// <summary>
    /// Calcola gli interessi cumulativi pagati su un prestito tra due periodi specificati.
    /// </summary>
    /// <param name="rate">Tasso di interesse periodico.</param>
    /// <param name="nper">Numero totale di periodi.</param>
    /// <param name="pv">Valore attuale (capitale del prestito).</param>
    /// <param name="startPeriod">Periodo di inizio del calcolo (1-based).</param>
    /// <param name="endPeriod">Periodo di fine del calcolo.</param>
    /// <param name="type">Tipo di pagamento: 0 = fine periodo, 1 = inizio periodo.</param>
    /// <returns>L'interesse cumulativo pagato nel periodo specificato.</returns>
    /// <exception cref="ArgumentException">Se i periodi non sono validi.</exception>
    /// <example>decimal interest = Numerics.Cumipmt(0.05m, 12, 10000m, 1, 6, 0);</example>
    public static decimal Cumipmt(decimal rate, decimal nper, decimal pv, int startPeriod, int endPeriod, int type)
    {
        if (nper <= 0 || startPeriod < 1 || endPeriod > nper || startPeriod > endPeriod)
            throw new ArgumentException("Periodi non validi.");
        decimal total = 0;
        var pmt = PmtExt(rate, nper, pv, 0, type);
        var balance = pv;
        for (var per = 1; per <= endPeriod; per++)
        {
            var interest = type == 1 ? balance * rate : (balance - pmt) * rate;
            if (per >= startPeriod) total += interest;
            balance = type == 1 ? balance + interest - pmt : balance - pmt + interest;
        }
        return total;
    }

    /// <summary>
    /// Calcola il capitale cumulativo rimborsato su un prestito tra due periodi specificati.
    /// </summary>
    /// <param name="rate">Tasso di interesse periodico.</param>
    /// <param name="nper">Numero totale di periodi.</param>
    /// <param name="pv">Valore attuale (capitale del prestito).</param>
    /// <param name="startPeriod">Periodo di inizio del calcolo (1-based).</param>
    /// <param name="endPeriod">Periodo di fine del calcolo.</param>
    /// <param name="type">Tipo di pagamento: 0 = fine periodo, 1 = inizio periodo.</param>
    /// <returns>Il capitale cumulativo rimborsato nel periodo specificato.</returns>
    /// <exception cref="ArgumentException">Se i periodi non sono validi.</exception>
    /// <example>decimal principal = Numerics.Cumprinc(0.05m, 12, 10000m, 1, 6, 0);</example>
    public static decimal Cumprinc(decimal rate, decimal nper, decimal pv, int startPeriod, int endPeriod, int type)
    {
        if (nper <= 0 || startPeriod < 1 || endPeriod > nper || startPeriod > endPeriod)
            throw new ArgumentException("Periodi non validi.");
        decimal total = 0;
        var pmt = PmtExt(rate, nper, pv, 0, type);
        var balance = pv;
        for (var per = 1; per <= endPeriod; per++)
        {
            var interest = type == 1 ? balance * rate : (balance - pmt) * rate;
            var principal = pmt - interest;
            if (per >= startPeriod) total += principal;
            balance = type == 1 ? balance + interest - pmt : balance - pmt + interest;
        }
        return total;
    }

    /// <summary>
    /// Calcola l'ammortamento lineare (a quote costanti) di un cespite.
    /// </summary>
    /// <param name="cost">Costo iniziale del cespite.</param>
    /// <param name="salvage">Valore residuo alla fine della vita utile.</param>
    /// <param name="life">Numero di periodi della vita utile.</param>
    /// <returns>La quota di ammortamento periodica costante.</returns>
    /// <exception cref="ArgumentOutOfRangeException">Se life è zero.</exception>
    /// <example>decimal sln = Numerics.Sln(10000m, 2000m, 5); // restituisce 1600m</example>
    public static decimal Sln(decimal cost, decimal salvage, decimal life)
    {
        if (life == 0) throw new ArgumentOutOfRangeException(nameof(life), "La vita utile non può essere zero.");
        return (cost - salvage) / life;
    }

    /// <summary>
    /// Calcola l'ammortamento a quote decrescenti secondo il metodo della somma degli anni (SYD).
    /// </summary>
    /// <param name="cost">Costo iniziale del cespite.</param>
    /// <param name="salvage">Valore residuo alla fine della vita utile.</param>
    /// <param name="life">Numero di periodi della vita utile.</param>
    /// <param name="per">Periodo specifico per cui calcolare la quota.</param>
    /// <returns>La quota di ammortamento per il periodo specificato.</returns>
    /// <exception cref="ArgumentException">Se life o per non sono validi.</exception>
    /// <exception cref="ArgumentOutOfRangeException">Se la somma degli anni è zero.</exception>
    /// <example>decimal syd = Numerics.Syd(10000m, 2000m, 5, 1); // restituisce circa 2666.67m</example>
    public static decimal Syd(decimal cost, decimal salvage, decimal life, decimal per)
    {
        if (life <= 0 || per <= 0 || per > life)
            throw new ArgumentException(
                $"Argomenti non validi per SYD: life ({life}) e per ({per}) devono essere positivi e 'per' non deve superare 'life'.");
        var sumOfYears = life * (life + 1) / 2;
        if (sumOfYears == 0) throw new ArgumentOutOfRangeException(nameof(life), "La vita utile non può essere zero.");
        return (cost - salvage) * (life - per + 1) / sumOfYears;
    }

    /// <summary>
    /// Calcola l'ammortamento a tasso fisso decrescente (DB) di un cespite per un periodo specificato.
    /// </summary>
    /// <param name="cost">Costo iniziale del cespite.</param>
    /// <param name="salvage">Valore residuo alla fine della vita utile.</param>
    /// <param name="life">Numero di periodi della vita utile.</param>
    /// <param name="period">Periodo specifico per cui calcolare la quota.</param>
    /// <param name="month">Numero di mesi nel primo anno (1-12).</param>
    /// <returns>La quota di ammortamento per il periodo specificato.</returns>
    /// <exception cref="ArgumentException">Se gli argomenti non sono validi.</exception>
    /// <example>decimal db = Numerics.Db(10000m, 2000m, 5, 1, 12);</example>
    public static decimal Db(decimal cost, decimal salvage, decimal life, decimal period, int month)
    {
        if (life <= 0 || period <= 0 || period > life || month < 1 || month > 12 || cost <= salvage)
            throw new ArgumentException("Argomenti non validi per la funzione DB.");
        var rate = 1 - (decimal)Math.Pow((double)(salvage / cost), 1.0 / (double)life);
        rate = Math.Round(rate, 3);
        decimal totalDepreciation = 0;
        var currentDepreciation = cost * rate * month / 12;
        if (period == 1) return Math.Min(currentDepreciation, cost - salvage);
        totalDepreciation += currentDepreciation;
        for (var i = 2; i < period; i++)
        {
            currentDepreciation = (cost - totalDepreciation) * rate;
            totalDepreciation += currentDepreciation;
        }
        currentDepreciation = (cost - totalDepreciation) * rate;
        return Math.Min(currentDepreciation, cost - salvage - totalDepreciation);
    }

    /// <summary>
    /// Calcola l'ammortamento a doppio tasso decrescente (DDB) per un periodo specificato.
    /// </summary>
    /// <param name="cost">Costo iniziale del cespite.</param>
    /// <param name="salvage">Valore residuo alla fine della vita utile.</param>
    /// <param name="life">Numero di periodi della vita utile.</param>
    /// <param name="period">Periodo specifico per cui calcolare la quota.</param>
    /// <param name="factor">Fattore di accelerazione (tipicamente 2.0 per il doppio tasso).</param>
    /// <returns>La quota di ammortamento per il periodo specificato.</returns>
    /// <exception cref="ArgumentException">Se gli argomenti non sono validi.</exception>
    /// <example>decimal ddb = Numerics.Ddb(10000m, 2000m, 5, 1, 2.0);</example>
    public static decimal Ddb(decimal cost, decimal salvage, decimal life, decimal period, double factor)
    {
        if (life <= 0 || period <= 0 || period > life || factor <= 0)
            throw new ArgumentException("Argomenti non validi per la funzione DDB.");
        decimal totalDepreciation = 0;
        for (var i = 1; i <= period; i++)
        {
            var depreciation = Math.Min(
                (cost - totalDepreciation) * (decimal)factor / life,
                cost - salvage - totalDepreciation);
            if (depreciation <= 0) return 0m;
            if (i == period) return depreciation;
            totalDepreciation += depreciation;
        }
        return 0m;
    }

    /// <summary>
    /// Calcola il valore attuale netto (VAN / NPV) di una serie di flussi di cassa a un tasso di sconto dato.
    /// Il primo flusso si considera all'anno 1 (non all'anno 0).
    /// </summary>
    /// <param name="rate">Tasso di sconto periodico. Deve essere maggiore di -1.</param>
    /// <param name="cashFlows">Array di flussi di cassa (positivi = entrate, negativi = uscite).</param>
    /// <returns>Il valore attuale netto.</returns>
    /// <exception cref="ArgumentOutOfRangeException">Se rate ≤ -1.</exception>
    /// <example>double npv = Numerics.Npv(0.1, new double[] {100, 200, 300});</example>
    public static double Npv(double rate, double[] cashFlows)
    {
        if (rate <= -1.0)
            throw new ArgumentOutOfRangeException(nameof(rate), "Il tasso per NPV deve essere maggiore di -1.");
        var npv = 0.0;
        for (var i = 0; i < cashFlows.Length; i++)
            npv += cashFlows[i] / Math.Pow(1 + rate, i + 1);
        return npv;
    }

    /// <summary>
    /// Calcola il tasso interno di rendimento (TIR / IRR) di una serie di flussi di cassa.
    /// Utilizza il metodo di Newton-Raphson con un valore di partenza (guess).
    /// </summary>
    /// <param name="flows">Array di flussi di cassa (deve contenere almeno un valore positivo e uno negativo).</param>
    /// <param name="guess">Stima iniziale del tasso (tipicamente 0.1).</param>
    /// <returns>Il tasso interno di rendimento come valore double.</returns>
    /// <exception cref="ArgumentException">Se l'array ha meno di 2 elementi o non contiene sia valori positivi che negativi.</exception>
    /// <exception cref="InvalidOperationException">Se la soluzione non converge entro 100 iterazioni.</exception>
    /// <example>double irr = Numerics.Irr(new double[] {-1000, 300, 400, 500}, 0.1);</example>
    public static double Irr(double[] flows, double guess)
    {
        if (flows == null || flows.Length < 2)
            throw new ArgumentException("L'array di flussi di cassa per IRR deve contenere almeno due valori.");
        if (!flows.Any(f => f > 0) || !flows.Any(f => f < 0))
            throw new ArgumentException(
                "L'array di flussi di cassa per IRR deve contenere almeno un valore positivo e uno negativo.");
        const int maxIterations = 100;
        const double tolerance = 1.0e-7;
        var rate = guess;
        for (var i = 0; i < maxIterations; i++)
        {
            var npv = flows[0] + Npv(rate, flows.Skip(1).ToArray());
            var derivative = 0.0;
            for (var j = 1; j < flows.Length; j++)
                derivative -= j * flows[j] / Math.Pow(1 + rate, j + 1);
            if (Math.Abs(derivative) < tolerance) break;
            var newRate = rate - npv / derivative;
            if (Math.Abs(newRate - rate) < tolerance) return newRate;
            rate = newRate;
        }
        throw new InvalidOperationException("Impossibile calcolare IRR: la soluzione non converge.");
    }

    /// <summary>
    /// Calcola il valore attuale netto per flussi di cassa con date non periodiche (XNPV).
    /// Ogni flusso viene scontato in base ai giorni effettivi dalla data base.
    /// </summary>
    /// <param name="rate">Tasso di sconto annuale.</param>
    /// <param name="cashFlows">Array di flussi di cassa.</param>
    /// <param name="dates">Array di date corrispondenti a ciascun flusso di cassa.</param>
    /// <returns>Il valore attuale netto con date non periodiche.</returns>
    /// <exception cref="ArgumentOutOfRangeException">Se rate ≤ -1.</exception>
    /// <exception cref="ArgumentException">Se gli array hanno lunghezza diversa o sono vuoti.</exception>
    /// <example>double xnpv = Numerics.Xnpv(0.1, new double[] {100, 200}, new DateTime[] {DateTime.Today, DateTime.Today.AddDays(365)});</example>
    public static double Xnpv(double rate, double[] cashFlows, DateTime[] dates)
    {
        if (rate <= -1.0)
            throw new ArgumentOutOfRangeException(nameof(rate), "Il tasso per XNPV deve essere maggiore di -1.");
        if (cashFlows.Length != dates.Length || cashFlows.Length == 0)
            throw new ArgumentException(
                "Gli array di valori e date devono avere la stessa lunghezza e non essere vuoti.");
        var baseDate = dates[0];
        var xnpv = 0.0;
        for (var i = 0; i < cashFlows.Length; i++)
        {
            var daysDiff = (dates[i] - baseDate).TotalDays;
            xnpv += cashFlows[i] / Math.Pow(1 + rate, daysDiff / 365.0);
        }
        return xnpv;
    }

    /// <summary>
    /// Calcola il tasso interno di rendimento per flussi di cassa con date non periodiche (XIRR).
    /// Utilizza il metodo di Newton-Raphson.
    /// </summary>
    /// <param name="cashFlows">Array di flussi di cassa.</param>
    /// <param name="dates">Array di date corrispondenti a ciascun flusso.</param>
    /// <param name="guess">Stima iniziale del tasso.</param>
    /// <returns>Il tasso interno di rendimento annuale.</returns>
    /// <exception cref="ArgumentException">Se gli array hanno lunghezza diversa o meno di 2 elementi, o non contengono sia valori positivi che negativi.</exception>
    /// <exception cref="InvalidOperationException">Se la soluzione non converge entro 100 iterazioni.</exception>
    /// <example>double xirr = Numerics.Xirr(new double[] {-1000, 500, 600}, new DateTime[] {DateTime.Today, DateTime.Today.AddDays(365), DateTime.Today.AddDays(730)}, 0.1);</example>
    public static double Xirr(double[] cashFlows, DateTime[] dates, double guess)
    {
        if (cashFlows.Length != dates.Length || cashFlows.Length < 2)
            throw new ArgumentException("Gli array per XIRR devono avere la stessa lunghezza e almeno due valori.");
        if (!cashFlows.Any(f => f > 0) || !cashFlows.Any(f => f < 0))
            throw new ArgumentException(
                "Gli array per XIRR devono contenere almeno un valore positivo e uno negativo.");
        const int maxIterations = 100;
        const double tolerance = 1.0e-7;
        var rate = guess;
        for (var i = 0; i < maxIterations; i++)
        {
            var xnpv = Xnpv(rate, cashFlows, dates);
            if (Math.Abs(xnpv) < tolerance) return rate;
            var derivative = 0.0;
            var baseDate = dates[0];
            for (var j = 0; j < cashFlows.Length; j++)
            {
                var daysDiff = (dates[j] - baseDate).TotalDays;
                if (daysDiff != 0)
                    derivative -= (daysDiff / 365.0) * cashFlows[j] / Math.Pow(1 + rate, (daysDiff / 365.0) + 1);
            }
            if (Math.Abs(derivative) < tolerance) break;
            var newRate = rate - xnpv / derivative;
            if (Math.Abs(newRate - rate) < tolerance) return newRate;
            rate = newRate;
        }
        throw new InvalidOperationException("Impossibile calcolare XIRR: la soluzione non converge.");
    }

    /// <summary>
    /// Calcola il tasso interno di rendimento modificato (MIRR), che considera separatamente
    /// il tasso di finanziamento (costo del capitale) e il tasso di reinvestimento.
    /// </summary>
    /// <param name="cashFlows">Array di flussi di cassa (positivi = ricavi, negativi = costi).</param>
    /// <param name="financeRate">Tasso di finanziamento (costo del capitale per flussi negativi).</param>
    /// <param name="reinvestRate">Tasso di reinvestimento per i flussi positivi.</param>
    /// <returns>Il tasso interno di rendimento modificato.</returns>
    /// <exception cref="ArgumentException">Se l'array ha meno di 2 elementi o mancano flussi positivi/negativi.</exception>
    /// <exception cref="InvalidOperationException">Se il valore attuale dei costi è zero.</exception>
    /// <example>double mirr = Numerics.Mirr(new double[] {-1000, 300, 400, 500}, 0.08, 0.12);</example>
    public static double Mirr(double[] cashFlows, double financeRate, double reinvestRate)
    {
        if (cashFlows.Length < 2)
            throw new ArgumentException("MIRR richiede almeno un flusso di cassa positivo e uno negativo.");
        var n = cashFlows.Length;
        var positiveFlows = cashFlows.Where(cf => cf > 0).ToArray();
        var negativeFlows = cashFlows.Where(cf => cf < 0).ToArray();
        if (positiveFlows.Length == 0 || negativeFlows.Length == 0)
            throw new ArgumentException("MIRR richiede sia flussi di cassa positivi (ricavi) che negativi (costi).");
        var fvPositive = 0.0;
        for (var i = 0; i < n; i++)
        {
            if (cashFlows[i] > 0)
                fvPositive += cashFlows[i] * Math.Pow(1 + reinvestRate, n - 1 - i);
        }
        var pvNegative = 0.0;
        for (var i = 0; i < n; i++)
        {
            if (cashFlows[i] < 0)
                pvNegative += cashFlows[i] / Math.Pow(1 + financeRate, i);
        }
        if (pvNegative == 0)
            throw new InvalidOperationException("La somma dei valori attuali dei costi non può essere zero.");
        var mirr = Math.Pow(-fvPositive / pvNegative, 1.0 / (n - 1)) - 1;
        return mirr;
    }

    /// <summary>
    /// Calcola il valore attuale netto modificato (MNPV), che reinveste i flussi intermedi
    /// al tasso di reinvestimento prima di scontarli al tasso richiesto.
    /// </summary>
    /// <param name="rate">Tasso di sconto richiesto.</param>
    /// <param name="reinvestRate">Tasso di reinvestimento per i flussi intermedi.</param>
    /// <param name="cashFlows">Array di flussi di cassa.</param>
    /// <returns>Il valore attuale netto modificato.</returns>
    /// <exception cref="ArgumentException">Se l'array ha meno di 2 elementi.</exception>
    /// <example>double mnpv = Numerics.Mnpv(0.1, 0.12, new double[] {-1000, 300, 400, 500});</example>
    public static double Mnpv(double rate, double reinvestRate, double[] cashFlows)
    {
        if (cashFlows.Length < 2) throw new ArgumentException("MNPV richiede almeno due flussi.");
        var n = cashFlows.Length;
        double npv = 0;
        for (var i = 0; i < n; i++)
        {
            var reinvested = cashFlows[i] * Math.Pow(1 + reinvestRate, n - i - 1);
            npv += reinvested / Math.Pow(1 + rate, n - 1);
        }
        return npv;
    }

    /// <summary>
    /// Calcola il prezzo semplice di un'obbligazione con cedole periodiche.
    /// </summary>
    /// <param name="settlement">Data di regolamento (acquisto).</param>
    /// <param name="maturity">Data di scadenza.</param>
    /// <param name="rate">Tasso cedolare annuale.</param>
    /// <param name="yld">Rendimento annuale (YTM).</param>
    /// <param name="redemption">Valore di rimborso (tipicamente 100).</param>
    /// <param name="frequency">Numero di cedole per anno (1, 2, 4).</param>
    /// <returns>Il prezzo dell'obbligazione.</returns>
    /// <exception cref="ArgumentException">Se la scadenza precede il regolamento.</exception>
    /// <example>double price = Numerics.PriceSimple(DateTime.Today, DateTime.Today.AddYears(5), 0.05, 0.04, 100, 2);</example>
    public static double PriceSimple(DateTime settlement, DateTime maturity, double rate, double yld,
        double redemption, int frequency)
    {
        var n = (int)Math.Round((maturity - settlement).TotalDays / 365.0 * frequency);
        if (n <= 0) throw new ArgumentException("La scadenza deve essere dopo la data di regolamento.");
        double price = 0;
        for (var k = 1; k <= n; k++)
            price += (rate * redemption / frequency) / Math.Pow(1 + yld / frequency, k);
        price += redemption / Math.Pow(1 + yld / frequency, n);
        return price;
    }

    /// <summary>
    /// Calcola la data della prossima cedola dopo la data di regolamento.
    /// </summary>
    /// <param name="settlement">Data di regolamento.</param>
    /// <param name="maturity">Data di scadenza.</param>
    /// <param name="frequency">Frequenza cedolare (1, 2, 4).</param>
    /// <returns>La data della prossima cedola.</returns>
    private static DateTime GetNextCoupon(DateTime settlement, DateTime maturity, int frequency)
    {
        var coupon = maturity;
        var months = 12 / frequency;
        while (coupon > settlement)
            coupon = coupon.AddMonths(-months);
        return coupon.AddMonths(months);
    }

    /// <summary>
    /// Calcola la data della cedola precedente rispetto a una data di cedola.
    /// </summary>
    /// <param name="nextCoupon">Data di una cedola futura.</param>
    /// <param name="frequency">Frequenza cedolare.</param>
    /// <returns>La data della cedola precedente.</returns>
    private static DateTime PrevCoupon(DateTime nextCoupon, int frequency)
    {
        return nextCoupon.AddMonths(-(12 / frequency));
    }

    /// <summary>
    /// Aggiunge un numero di mesi a una data, preservando il giorno di fine mese se applicabile.
    /// </summary>
    /// <param name="date">Data di partenza.</param>
    /// <param name="months">Numero di mesi da aggiungere.</param>
    /// <returns>La nuova data con i mesi aggiunti.</returns>
    private static DateTime AddMonths(DateTime date, int months)
    {
        var d = date.AddMonths(months);
        if (date.Day == DateTime.DaysInMonth(date.Year, date.Month))
            d = new DateTime(d.Year, d.Month, DateTime.DaysInMonth(d.Year, d.Month));
        return d;
    }

    /// <summary>
    /// Calcola il numero di giorni tra due date secondo la convenzione specificata (basis).
    /// </summary>
    /// <param name="start">Data di inizio.</param>
    /// <param name="end">Data di fine.</param>
    /// <param name="basis">Base di calcolo: 0=30/360 US, 1=effettivi/effettivi, 2=effettivi/360, 3=effettivi/365, 4=30/360 Europea.</param>
    /// <returns>Il numero di giorni secondo la convenzione.</returns>
    private static double DayCount(DateTime start, DateTime end, int basis)
    {
        switch (basis)
        {
            case 0:
                int d1 = start.Day, d2 = end.Day;
                int m1 = start.Month, m2 = end.Month;
                int y1 = start.Year, y2 = end.Year;
                if (d1 == 31) d1 = 30;
                if (d2 == 31 && d1 == 30) d2 = 30;
                return (y2 - y1) * 360 + (m2 - m1) * 30 + (d2 - d1);
            case 1:
                return (end - start).TotalDays;
            case 2:
                return (end - start).TotalDays;
            case 3:
                return (end - start).TotalDays;
            case 4:
                d1 = start.Day == 31 ? 30 : start.Day;
                d2 = end.Day == 31 ? 30 : end.Day;
                return (end.Year - start.Year) * 360 + (end.Month - start.Month) * 30 + (d2 - d1);
            default:
                return (end - start).TotalDays;
        }
    }

    /// <summary>
    /// Calcola il prezzo di un'obbligazione con convenzioni calendariali avanzate (basis, prima cedola, ecc.).
    /// </summary>
    /// <param name="settlement">Data di regolamento.</param>
    /// <param name="maturity">Data di scadenza.</param>
    /// <param name="rate">Tasso cedolare annuale.</param>
    /// <param name="yld">Rendimento annuale alla scadenza (YTM).</param>
    /// <param name="redemption">Valore di rimborso (tipicamente 100).</param>
    /// <param name="frequency">Frequenza cedolare (1, 2, 4).</param>
    /// <param name="basis">Base di calcolo giorni (0-4).</param>
    /// <param name="firstCoupon">Data della prima cedola (opzionale).</param>
    /// <param name="issue">Data di emissione (opzionale).</param>
    /// <param name="calcMethod">Metodo di calcolo (0=standard).</param>
    /// <returns>Il prezzo dell'obbligazione comprensivo di rateo cedolare.</returns>
    /// <example>double price = Numerics.Price(settlement, maturity, 0.05, 0.04, 100, 2, 0);</example>
    public static double Price(DateTime settlement, DateTime maturity, double rate, double yld, double redemption,
        int frequency, int basis, DateTime? firstCoupon = null, DateTime? issue = null, int calcMethod = 0)
    {
        var nextCoupon = GetNextCoupon(settlement, maturity, frequency);
        var N = 0;
        var coupon = nextCoupon;
        while (coupon <= maturity)
        {
            N++;
            coupon = AddMonths(coupon, 12 / frequency);
        }
        var DSC = DayCount(settlement, nextCoupon, basis);
        var E = DayCount(PrevCoupon(nextCoupon, frequency), nextCoupon, basis);
        var AI = (rate * redemption / frequency) * (1 - DSC / E);
        double price = 0;
        for (var k = 1; k <= N; k++)
        {
            var t = (DSC + E * (k - 1)) / E;
            price += (rate * redemption / frequency) / Math.Pow(1 + yld / frequency, t);
        }
        price += redemption / Math.Pow(1 + yld / frequency, (DSC + E * N) / E);
        return price - AI;
    }

    /// <summary>
    /// Calcola il rendimento effettivo alla scadenza (YIELD) di un'obbligazione.
    /// Utilizza il metodo di bisezione per risolvere l'equazione prezzo-rendimento.
    /// </summary>
    /// <param name="settlement">Data di regolamento.</param>
    /// <param name="maturity">Data di scadenza.</param>
    /// <param name="price">Prezzo di mercato dell'obbligazione.</param>
    /// <param name="redemption">Valore di rimborso (tipicamente 100).</param>
    /// <param name="rate">Tasso cedolare annuale.</param>
    /// <param name="frequency">Frequenza cedolare.</param>
    /// <param name="basis">Base di calcolo giorni.</param>
    /// <returns>Il rendimento annuale alla scadenza.</returns>
    /// <exception cref="InvalidOperationException">Se la soluzione non converge entro 100 iterazioni.</exception>
    /// <example>double yield = Numerics.Yield(settlement, maturity, 105.0, 100, 0.05, 2, 0);</example>
    public static double Yield(DateTime settlement, DateTime maturity, double price, double redemption,
        double rate, int frequency, int basis)
    {
        double lower = 0, upper = 1, yld = 0.05, tol = 1e-7;
        for (var i = 0; i < 100; i++)
        {
            var testPrice = Price(settlement, maturity, rate, yld, redemption, frequency, basis);
            if (Math.Abs(testPrice - price) < tol) return yld;
            if (testPrice > price) lower = yld;
            else upper = yld;
            yld = (lower + upper) / 2;
        }
        throw new InvalidOperationException("Impossibile calcolare YIELD: la soluzione non converge.");
    }

    /// <summary>
    /// Calcola la duration Macaulay o modificata di un'obbligazione.
    /// La duration misura la sensibilità del prezzo alle variazioni del tasso di interesse.
    /// </summary>
    /// <param name="settlement">Data di regolamento.</param>
    /// <param name="maturity">Data di scadenza.</param>
    /// <param name="coupon">Tasso cedolare annuale.</param>
    /// <param name="yld">Rendimento annuale alla scadenza.</param>
    /// <param name="frequency">Frequenza cedolare.</param>
    /// <param name="basis">Base di calcolo giorni.</param>
    /// <param name="modified">Se true, restituisce la duration modificata; altrimenti la duration Macaulay.</param>
    /// <returns>La duration (Macaulay o modificata) in anni.</returns>
    /// <exception cref="ArgumentException">Se la scadenza precede il regolamento.</exception>
    /// <example>double dur = Numerics.Duration(settlement, maturity, 0.05, 0.04, 2, 0, false);</example>
    public static double Duration(DateTime settlement, DateTime maturity, double coupon, double yld,
        int frequency, int basis, bool modified)
    {
        var n = (int)Math.Round((maturity - settlement).TotalDays / 365.0 * frequency);
        if (n <= 0) throw new ArgumentException("La scadenza deve essere dopo la data di regolamento.");
        double pv = 0, weighted = 0;
        for (var k = 1; k <= n; k++)
        {
            var cf = coupon / frequency;
            var t = (double)k / frequency;
            var df = Math.Pow(1 + yld / frequency, k);
            pv += cf / df;
            weighted += t * cf / df;
        }
        pv += 1 / Math.Pow(1 + yld / frequency, n);
        weighted += ((double)n / frequency) * (1 / Math.Pow(1 + yld / frequency, n));
        var duration = weighted / pv;
        return modified ? duration / (1 + yld / frequency) : duration;
    }

    /// <summary>
    /// Calcola il tasso di interesse periodico di una rendita (funzione RATE).
    /// Utilizza il metodo di Newton-Raphson.
    /// </summary>
    /// <param name="nper">Numero totale di periodi.</param>
    /// <param name="pmt">Importo del pagamento periodico.</param>
    /// <param name="pv">Valore attuale (capitale del prestito).</param>
    /// <param name="fv">Valore futuro (default 0).</param>
    /// <param name="type">Tipo di pagamento: 0 = fine periodo, 1 = inizio periodo (default 0).</param>
    /// <param name="guess">Stima iniziale del tasso (default 0.1).</param>
    /// <returns>Il tasso di interesse periodico.</returns>
    /// <exception cref="InvalidOperationException">Se la soluzione non converge entro 100 iterazioni.</exception>
    /// <example>double rate = Numerics.Rate(12, 1000, -10000); // tasso mensile di un prestito</example>
    public static double Rate(double nper, double pmt, double pv, double fv = 0, int type = 0, double guess = 0.1)
    {
        /* guess == 0 arriva dal backend IL/PCode quando l'argomento è OMESSO
         * (`FINANCE.RATE(n, pmt, pv)` → il compilatore passa 0.0): senza questa
         * guardia la formula divide per rate=0 e il Newton parte da NaN →
         * "Impossibile calcolare RATE". Allineato a gmb_fin_rate del runtime C
         * (riferimento), che fa esattamente `if (guess == 0) guess = 0.1;`. */
        if (guess == 0) guess = 0.1;
        double rate = guess, tol = 1e-7;
        for (var i = 0; i < 100; i++)
        {
            var pow = Math.Pow(1 + rate, nper);
            var f = pv * pow + pmt * (1 + rate * type) * (pow - 1) / rate + fv;
            // Derivata analitica esatta di f rispetto a rate:
            //   q(r) = ((1+r)^n - 1)/r ;  q'(r) = [n*r*(1+r)^(n-1) - ((1+r)^n - 1)] / r^2
            var powNm1 = Math.Pow(1 + rate, nper - 1);
            var dq = (nper * rate * powNm1 - (pow - 1)) / (rate * rate);
            var df = nper * pv * powNm1 + pmt * (1 + rate * type) * dq + pmt * type * ((pow - 1) / rate);
            var newRate = rate - f / df;
            if (Math.Abs(newRate - rate) < tol) return newRate;
            rate = newRate;
        }
        throw new InvalidOperationException("Impossibile calcolare RATE: la soluzione non converge.");
    }

    /// <summary>
    /// Calcola il numero di giorni tra due date utilizzando la convenzione 30/360 (NASD o Europea).
    /// </summary>
    /// <param name="start">Data di inizio.</param>
    /// <param name="end">Data di fine.</param>
    /// <param name="european">Se true, usa la convenzione europea (30/360 EU); altrimenti la NASD US.</param>
    /// <returns>Il numero di giorni in base 360.</returns>
    /// <example>int days = Numerics.Days360(new DateTime(2024, 1, 31), new DateTime(2024, 2, 28), false);</example>
    public static int Days360(DateTime start, DateTime end, bool european)
    {
        int d1 = start.Day, d2 = end.Day;
        int m1 = start.Month, m2 = end.Month;
        int y1 = start.Year, y2 = end.Year;
        if (!european)
        {
            if (d1 == 31) d1 = 30;
            if (d2 == 31 && d1 == 30) d2 = 30;
        }
        else
        {
            if (d1 == 31) d1 = 30;
            if (d2 == 31) d2 = 30;
        }
        return (y2 - y1) * 360 + (m2 - m1) * 30 + (d2 - d1);
    }

    /// <summary>
    /// Calcola la frazione di anno tra due date secondo diverse basi di conteggio.
    /// </summary>
    /// <param name="start">Data di inizio.</param>
    /// <param name="end">Data di fine.</param>
    /// <param name="basis">Base di calcolo: 0=30/360 US, 1=effettivi/effettivi, 2=effettivi/360, 3=effettivi/365, 4=30/360 EU.</param>
    /// <returns>La frazione di anno come double.</returns>
    /// <exception cref="ArgumentException">Se la base non è supportata (valori fuori 0-4).</exception>
    /// <example>double frac = Numerics.YearFrac(DateTime.Today, DateTime.Today.AddDays(90), 1);</example>
    public static double YearFrac(DateTime start, DateTime end, int basis)
    {
        switch (basis)
        {
            case 0:
                return Days360(start, end, false) / 360.0;
            case 1:
                return (end - start).TotalDays / (DateTime.IsLeapYear(start.Year) ? 366 : 365);
            case 2:
                return (end - start).TotalDays / 360.0;
            case 3:
                return (end - start).TotalDays / 365.0;
            case 4:
                return Days360(start, end, true) / 360.0;
            default:
                throw new ArgumentException("Base non supportata.");
        }
    }
    public static bool IsLongOnly(double[] weights) { for (int i = 0; i < weights.Length; i++) if (weights[i] < 0) return false; return true; }
}
