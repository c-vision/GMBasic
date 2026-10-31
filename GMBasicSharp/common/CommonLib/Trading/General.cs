namespace CommonLib.Trading;

/// <summary>
/// Funzioni finanziarie generali per la valutazione di titoli azionari, indici di bilancio,
/// modelli di sconto dei dividendi e metriche di performance di base.
///
/// Metodi:
/// - <see cref="BidAskSpread"/>: differenza denaro-lettera.
/// - <see cref="BookValuePerShare"/>: valore contabile per azione.
/// - <see cref="Capm"/>: rendimento atteso con CAPM.
/// - <see cref="CapitalGainsYield"/>: rendimento da capital gain.
/// - <see cref="DilutedEps"/>: utile per azione diluito.
/// - <see cref="DividendPayoutRatio"/>: payout ratio.
/// - <see cref="DividendYield"/>: dividend yield.
/// - <see cref="Dps"/>: dividendo per azione.
/// - <see cref="Eps"/>: utile per azione.
/// - <see cref="EquityMultiplier"/>: moltiplicatore del patrimonio netto.
/// - <see cref="GeometricMeanReturn"/>: media geometrica dei rendimenti.
/// - <see cref="HoldingPeriodReturn"/>: rendimento totale nel periodo di detenzione.
/// - <see cref="Nav"/>: valore patrimoniale netto (NAV).
/// - <see cref="PreferredStockValue"/>: valore di un'azione privilegiata.
/// - <see cref="PeRatio"/>: rapporto prezzo/utili (P/E).
/// - <see cref="PsRatio"/>: rapporto prezzo/vendite (P/S).
/// - <see cref="PbRatio"/>: rapporto prezzo/valore contabile (P/B).
/// - <see cref="RiskPremium"/>: premio per il rischio.
/// - <see cref="StockValueConstGrowth"/>: valore azione con crescita costante (Gordon).
/// - <see cref="GrowthRate"/>: tasso di crescita sostenibile.
/// - <see cref="StockValueZeroGrowth"/>: valore azione a crescita zero.
/// - <see cref="TaxEquivalentYield"/>: rendimento equivalente al netto delle imposte.
/// - <see cref="TotalStockReturn"/>: rendimento totale azionario.
/// </summary>
public static class General
{
    /// <summary>
    /// Calcola lo spread denaro-lettera (bid-ask spread).
    /// </summary>
    /// <param name="bid">Prezzo denaro (bid).</param>
    /// <param name="ask">Prezzo lettera (ask).</param>
    /// <returns>Differenza tra ask e bid.</returns>
    /// <example>
    /// <code>var spread = General.BidAskSpread(10.0, 10.05);</code>
    /// </example>
    public static double BidAskSpread(double bid, double ask) => ask - bid;

    /// <summary>
    /// Calcola il valore contabile per azione (book value per share).
    /// </summary>
    /// <param name="totalEquity">Patrimonio netto totale.</param>
    /// <param name="commonShares">Numero di azioni ordinarie in circolazione.</param>
    /// <returns>Valore contabile per azione.</returns>
    /// <exception cref="DivideByZeroException">Sollevata se commonShares è zero.</exception>
    /// <example>
    /// <code>var bvps = General.BookValuePerShare(1_000_000, 100_000);</code>
    /// </example>
    public static double BookValuePerShare(double totalEquity, double commonShares)
    {
        if (commonShares == 0) throw new DivideByZeroException("Il numero di azioni non può essere zero.");
        return totalEquity / commonShares;
    }

    /// <summary>
    /// Calcola il rendimento atteso di un'attività finanziaria secondo il Capital Asset Pricing Model (CAPM).
    /// Formula: riskFreeRate + beta × (marketReturn - riskFreeRate).
    /// </summary>
    /// <param name="riskFreeRate">Tasso privo di rischio.</param>
    /// <param name="beta">Beta dell'attività.</param>
    /// <param name="marketReturn">Rendimento atteso del mercato.</param>
    /// <returns>Rendimento atteso secondo CAPM.</returns>
    /// <example>
    /// <code>var capm = General.Capm(0.02, 1.2, 0.08);</code>
    /// </example>
    public static double Capm(double riskFreeRate, double beta, double marketReturn) =>
        riskFreeRate + beta * (marketReturn - riskFreeRate);

    /// <summary>
    /// Calcola il rendimento da capital gain (plusvalenza).
    /// </summary>
    /// <param name="initialPrice">Prezzo iniziale.</param>
    /// <param name="endPrice">Prezzo finale.</param>
    /// <returns>Rendimento percentuale della plusvalenza.</returns>
    /// <exception cref="DivideByZeroException">Sollevata se initialPrice è zero.</exception>
    /// <example>
    /// <code>var cgy = General.CapitalGainsYield(100, 110);</code>
    /// </example>
    public static double CapitalGainsYield(double initialPrice, double endPrice)
    {
        if (initialPrice == 0) throw new DivideByZeroException("Il prezzo iniziale non può essere zero.");
        return (endPrice - initialPrice) / initialPrice;
    }

    /// <summary>
    /// Calcola l'utile per azione diluito (Diluted EPS), considerando strumenti convertibili.
    /// </summary>
    /// <param name="netIncome">Reddito netto.</param>
    /// <param name="avgShares">Numero medio di azioni ordinarie.</param>
    /// <param name="otherInstruments">Strumenti convertibili (opzioni, warrant, obbligazioni convertibili).</param>
    /// <returns>Utile per azione diluito.</returns>
    /// <exception cref="DivideByZeroException">Sollevata se avgShares + otherInstruments è zero.</exception>
    /// <example>
    /// <code>var diluted = General.DilutedEps(500_000, 100_000, 10_000);</code>
    /// </example>
    public static double DilutedEps(double netIncome, double avgShares, double otherInstruments)
    {
        var denominator = avgShares + otherInstruments;
        if (denominator == 0)
            throw new DivideByZeroException(
                "La somma delle azioni medie e degli strumenti convertibili non può essere zero.");
        return netIncome / denominator;
    }

    /// <summary>
    /// Calcola il rapporto di distribuzione dei dividendi (dividend payout ratio).
    /// </summary>
    /// <param name="dividends">Dividendi totali distribuiti.</param>
    /// <param name="netIncome">Reddito netto.</param>
    /// <returns>Payout ratio (0.0 – 1.0).</returns>
    /// <exception cref="DivideByZeroException">Sollevata se netIncome è zero.</exception>
    /// <example>
    /// <code>var dpr = General.DividendPayoutRatio(200_000, 500_000);</code>
    /// </example>
    public static double DividendPayoutRatio(double dividends, double netIncome)
    {
        if (netIncome == 0) throw new DivideByZeroException("Il reddito netto non può essere zero.");
        return dividends / netIncome;
    }

    /// <summary>
    /// Calcola il dividend yield: rapporto tra dividendo annuale e prezzo dell'azione.
    /// </summary>
    /// <param name="dividends">Dividendo annuale per azione.</param>
    /// <param name="price">Prezzo corrente dell'azione.</param>
    /// <returns>Dividend yield.</returns>
    /// <exception cref="DivideByZeroException">Sollevata se price è zero.</exception>
    /// <example>
    /// <code>var dy = General.DividendYield(2.5, 50.0);</code>
    /// </example>
    public static double DividendYield(double dividends, double price)
    {
        if (price == 0) throw new DivideByZeroException("Il prezzo non può essere zero.");
        return dividends / price;
    }

    /// <summary>
    /// Calcola il dividendo per azione (Dividends Per Share).
    /// </summary>
    /// <param name="dividends">Dividendi totali distribuiti.</param>
    /// <param name="numShares">Numero di azioni in circolazione.</param>
    /// <returns>Dividendo per azione.</returns>
    /// <exception cref="DivideByZeroException">Sollevata se numShares è zero.</exception>
    /// <example>
    /// <code>var dps = General.Dps(200_000, 100_000);</code>
    /// </example>
    public static double Dps(double dividends, double numShares)
    {
        if (numShares == 0) throw new DivideByZeroException("Il numero di azioni non può essere zero.");
        return dividends / numShares;
    }

    /// <summary>
    /// Calcola l'utile per azione (Earnings Per Share).
    /// </summary>
    /// <param name="netIncome">Reddito netto.</param>
    /// <param name="avgShares">Numero medio ponderato di azioni in circolazione.</param>
    /// <returns>Utile per azione.</returns>
    /// <exception cref="DivideByZeroException">Sollevata se avgShares è zero.</exception>
    /// <example>
    /// <code>var eps = General.Eps(500_000, 100_000);</code>
    /// </example>
    public static double Eps(double netIncome, double avgShares)
    {
        if (avgShares == 0) throw new DivideByZeroException("Il numero di azioni medie non può essere zero.");
        return netIncome / avgShares;
    }

    /// <summary>
    /// Calcola il moltiplicatore del patrimonio netto (equity multiplier):
    /// totale attivo / patrimonio netto. Misura la leva finanziaria.
    /// </summary>
    /// <param name="totalAssets">Totale attivo.</param>
    /// <param name="equity">Patrimonio netto.</param>
    /// <returns>Moltiplicatore del patrimonio netto.</returns>
    /// <exception cref="DivideByZeroException">Sollevata se equity è zero.</exception>
    /// <example>
    /// <code>var em = General.EquityMultiplier(2_000_000, 1_000_000);</code>
    /// </example>
    public static double EquityMultiplier(double totalAssets, double equity)
    {
        if (equity == 0) throw new DivideByZeroException("Il patrimonio netto non può essere zero.");
        return totalAssets / equity;
    }

    /// <summary>
    /// Calcola la media geometrica di un insieme di rendimenti periodici.
    /// Utile per determinare il tasso di crescita medio composto.
    /// </summary>
    /// <param name="ratesOfReturn">Array dei rendimenti periodici (es. 0.05 per +5%).</param>
    /// <returns>Media geometrica dei rendimenti.</returns>
    /// <example>
    /// <code>var gmr = General.GeometricMeanReturn(new[] { 0.05, -0.02, 0.03 });</code>
    /// </example>
    public static double GeometricMeanReturn(double[] ratesOfReturn)
    {
        if (ratesOfReturn.Length == 0) return 0.0;

        var product = 1.0;
        foreach (var rate in ratesOfReturn)
        {
            product *= (1.0 + rate);
        }

        return Math.Pow(product, 1.0 / ratesOfReturn.Length) - 1.0;
    }

    /// <summary>
    /// Calcola il rendimento totale nel periodo di detenzione (Holding Period Return),
    /// composto su più periodi.
    /// </summary>
    /// <param name="percentageReturns">Array dei rendimenti percentuali per ciascun sottoperiodo.</param>
    /// <returns>Rendimento totale composto del periodo.</returns>
    /// <example>
    /// <code>var hpr = General.HoldingPeriodReturn(new[] { 0.05, -0.02, 0.03 });</code>
    /// </example>
    public static double HoldingPeriodReturn(double[] percentageReturns)
    {
        if (percentageReturns.Length == 0) return 0.0;

        var totalReturn = 1.0;
        foreach (var periodReturn in percentageReturns)
        {
            totalReturn *= (1.0 + periodReturn);
        }

        return totalReturn - 1.0;
    }

    /// <summary>
    /// Calcola il valore patrimoniale netto (Net Asset Value) per un fondo di investimento.
    /// </summary>
    /// <param name="fundAssets">Attività totali del fondo.</param>
    /// <param name="fundLiabilities">Passività totali del fondo.</param>
    /// <param name="outstandingShares">Numero di quote/azioni in circolazione.</param>
    /// <returns>NAV per quota/azione.</returns>
    /// <exception cref="DivideByZeroException">Sollevata se outstandingShares è zero.</exception>
    /// <example>
    /// <code>var nav = General.Nav(10_000_000, 1_000_000, 500_000);</code>
    /// </example>
    public static double Nav(double fundAssets, double fundLiabilities, double outstandingShares)
    {
        if (outstandingShares == 0)
            throw new DivideByZeroException("Il numero di azioni in circolazione non può essere zero.");
        return (fundAssets - fundLiabilities) / outstandingShares;
    }

    /// <summary>
    /// Calcola il valore di un'azione privilegiata utilizzando il modello della rendita perpetua.
    /// </summary>
    /// <param name="dividend">Dividendo annuale dell'azione privilegiata.</param>
    /// <param name="discountRate">Tasso di sconto richiesto.</param>
    /// <returns>Valore teorico dell'azione privilegiata.</returns>
    /// <exception cref="DivideByZeroException">Sollevata se discountRate è zero.</exception>
    /// <example>
    /// <code>var psv = General.PreferredStockValue(5.0, 0.08);</code>
    /// </example>
    public static double PreferredStockValue(double dividend, double discountRate)
    {
        if (discountRate == 0) throw new DivideByZeroException("Il tasso di sconto non può essere zero.");
        return dividend / discountRate;
    }

    /// <summary>
    /// Calcola il rapporto prezzo/utili (Price-to-Earnings ratio).
    /// Restituisce Infinity se l'utile per azione è zero.
    /// </summary>
    /// <param name="price">Prezzo corrente dell'azione.</param>
    /// <param name="eps">Utile per azione (EPS).</param>
    /// <returns>Rapporto P/E, o PositiveInfinity se EPS = 0.</returns>
    /// <example>
    /// <code>var pe = General.PeRatio(50.0, 2.5);</code>
    /// </example>
    public static double PeRatio(double price, double eps)
    {
        if (eps == 0) return double.PositiveInfinity;
        return price / eps;
    }

    /// <summary>
    /// Calcola il rapporto prezzo/vendite (Price-to-Sales ratio).
    /// </summary>
    /// <param name="price">Prezzo corrente dell'azione.</param>
    /// <param name="salesPerShare">Vendite per azione.</param>
    /// <returns>Rapporto P/S.</returns>
    /// <exception cref="DivideByZeroException">Sollevata se salesPerShare è zero.</exception>
    /// <example>
    /// <code>var ps = General.PsRatio(50.0, 10.0);</code>
    /// </example>
    public static double PsRatio(double price, double salesPerShare)
    {
        if (salesPerShare == 0) throw new DivideByZeroException("Le vendite per azione non possono essere zero.");
        return price / salesPerShare;
    }

    /// <summary>
    /// Calcola il rapporto prezzo/valore contabile (Price-to-Book ratio).
    /// </summary>
    /// <param name="price">Prezzo corrente dell'azione.</param>
    /// <param name="bookValue">Valore contabile per azione.</param>
    /// <returns>Rapporto P/B.</returns>
    /// <exception cref="DivideByZeroException">Sollevata se bookValue è zero.</exception>
    /// <example>
    /// <code>var pb = General.PbRatio(50.0, 25.0);</code>
    /// </example>
    public static double PbRatio(double price, double bookValue)
    {
        if (bookValue == 0) throw new DivideByZeroException("Il valore contabile per azione non può essere zero.");
        return price / bookValue;
    }

    /// <summary>
    /// Calcola il premio per il rischio: differenza tra rendimento dell'attività e tasso privo di rischio.
    /// </summary>
    /// <param name="assetReturn">Rendimento dell'attività.</param>
    /// <param name="riskFreeReturn">Rendimento privo di rischio.</param>
    /// <returns>Premio per il rischio.</returns>
    /// <example>
    /// <code>var rp = General.RiskPremium(0.08, 0.02);</code>
    /// </example>
    public static double RiskPremium(double assetReturn, double riskFreeReturn) => assetReturn - riskFreeReturn;

    /// <summary>
    /// Calcola il valore teorico di un'azione con crescita costante dei dividendi
    /// (modello di Gordon — Dividend Discount Model a crescita costante).
    /// </summary>
    /// <param name="nextDividend">Dividendo atteso per il prossimo periodo.</param>
    /// <param name="rate">Tasso di rendimento richiesto.</param>
    /// <param name="growth">Tasso di crescita perpetua dei dividendi.</param>
    /// <returns>Valore teorico dell'azione.</returns>
    /// <exception cref="ArgumentException">Sollevata se rate &lt;= growth.</exception>
    /// <example>
    /// <code>
    /// var valore = General.StockValueConstGrowth(2.0, 0.10, 0.03);
    /// </code>
    /// </example>
    public static double StockValueConstGrowth(double nextDividend, double rate, double growth)
    {
        var denominator = rate - growth;
        if (denominator <= 0)
            throw new ArgumentException("Il tasso di rendimento richiesto deve essere maggiore del tasso di crescita.");
        return nextDividend / denominator;
    }

    /// <summary>
    /// Calcola il tasso di crescita sostenibile (g = retention rate × ROE).
    /// </summary>
    /// <param name="retentionRate">Tasso di ritenzione degli utili (1 - payout ratio).</param>
    /// <param name="roe">Return on Equity (ROE).</param>
    /// <returns>Tasso di crescita sostenibile.</returns>
    /// <example>
    /// <code>var g = General.GrowthRate(0.6, 0.15);</code>
    /// </example>
    public static double GrowthRate(double retentionRate, double roe) => retentionRate * roe;

    /// <summary>
    /// Calcola il valore teorico di un'azione con dividendi a crescita zero (rendita perpetua).
    /// </summary>
    /// <param name="dividend">Dividendo annuale costante.</param>
    /// <param name="rate">Tasso di rendimento richiesto.</param>
    /// <returns>Valore teorico dell'azione.</returns>
    /// <exception cref="DivideByZeroException">Sollevata se rate è zero.</exception>
    /// <example>
    /// <code>var valore = General.StockValueZeroGrowth(2.0, 0.10);</code>
    /// </example>
    public static double StockValueZeroGrowth(double dividend, double rate)
    {
        if (rate == 0) throw new DivideByZeroException("Il tasso di rendimento richiesto non può essere zero.");
        return dividend / rate;
    }

    /// <summary>
    /// Calcola il rendimento equivalente al netto delle imposte per un'obbligazione
    /// o investimento tax-exempt. Utile per confrontare rendimenti al lordo e al netto delle imposte.
    /// </summary>
    /// <param name="taxFreeYield">Rendimento esentasse.</param>
    /// <param name="taxRate">Aliquota fiscale marginale (0.0 – 1.0).</param>
    /// <returns>Rendimento equivalente al lordo delle imposte.</returns>
    /// <exception cref="ArgumentOutOfRangeException">Sollevata se taxRate non è compreso tra 0 e 1.</exception>
    /// <example>
    /// <code>var tey = General.TaxEquivalentYield(0.03, 0.35);</code>
    /// </example>
    public static double TaxEquivalentYield(double taxFreeYield, double taxRate)
    {
        if (taxRate is >= 1.0 or < 0)
            throw new ArgumentOutOfRangeException(nameof(taxRate),
                "L'aliquota fiscale deve essere compresa tra 0 e 1.");
        var denominator = 1.0 - taxRate;
        return taxFreeYield / denominator;
    }

    /// <summary>
    /// Calcola il rendimento totale di un investimento azionario, inclusi i dividendi.
    /// Formula: ((prezzoFinale - prezzoIniziale) + dividendi) / prezzoIniziale.
    /// </summary>
    /// <param name="initialPrice">Prezzo iniziale dell'azione.</param>
    /// <param name="endPrice">Prezzo finale dell'azione.</param>
    /// <param name="dividends">Dividendi ricevuti nel periodo.</param>
    /// <returns>Rendimento totale percentuale.</returns>
    /// <exception cref="DivideByZeroException">Sollevata se initialPrice è zero.</exception>
    /// <example>
    /// <code>var tsr = General.TotalStockReturn(100, 110, 3);</code>
    /// </example>
    public static double TotalStockReturn(double initialPrice, double endPrice, double dividends)
    {
        if (initialPrice == 0) throw new DivideByZeroException("Il prezzo iniziale non può essere zero.");
        return ((endPrice - initialPrice) + dividends) / initialPrice;
    }
}
