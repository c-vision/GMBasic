namespace CommonLib.Trading;

/// <summary>
/// Funzioni per la valutazione di obbligazioni e il calcolo dei rendimenti obbligazionari.
/// Supporta obbligazioni con cedola, zero-coupon, yield to maturity e metriche derivate.
///
/// Metodi:
/// - <see cref="BondPrice"/>: prezzo teorico di un'obbligazione con cedola.
/// - <see cref="BondYield"/>: rendimento a scadenza (YTM) mediante Newton-Raphson.
/// - <see cref="BondEquivalentYield"/>: rendimento equivalente su base obbligazionaria.
/// - <see cref="CurrentYield"/>: rendimento corrente (cedola / prezzo).
/// - <see cref="YieldToMaturity"/>: YTM approssimato (formula standard).
/// - <see cref="ZeroCouponBondValue"/>: valore attuale di uno zero-coupon bond.
/// - <see cref="ZeroCouponBondYield"/>: rendimento implicito di uno zero-coupon bond.
/// </summary>
public static class Bonds
{
    /// <summary>
    /// Calcola il prezzo teorico di un'obbligazione con cedola utilizzando il rendimento (YTM).
    /// Il prezzo è pari al valore attuale dei flussi di cassa futuri (cedole + rimborso) scontati al rendimento.
    /// </summary>
    /// <param name="settlement">Data di liquidazione (acquisto).</param>
    /// <param name="maturity">Data di scadenza dell'obbligazione.</param>
    /// <param name="rate">Tasso cedolare annuale (es. 0.05 per 5%).</param>
    /// <param name="yld">Rendimento annuo a scadenza (YTM).</param>
    /// <param name="redemption">Valore di rimborso (tipicamente 100).</param>
    /// <param name="frequency">Frequenza cedolare: 1 (annuale), 2 (semestrale), 4 (trimestrale).</param>
    /// <returns>Prezzo teorico dell'obbligazione (dirty price).</returns>
    /// <exception cref="ArgumentException">
    /// Sollevata se settlement &gt;= maturity, frequency non valida, o rate/yld negativi.
    /// </exception>
    /// <example>
    /// <code>
    /// var prezzo = Bonds.BondPrice(new DateTime(2024, 1, 1), new DateTime(2034, 1, 1), 0.05, 0.04, 100, 2);
    /// </code>
    /// </example>
    public static double BondPrice(DateTime settlement, DateTime maturity, double rate, double yld,
        double redemption, int frequency)
    {
        if (settlement >= maturity)
            throw new ArgumentException("La data di liquidazione deve essere precedente alla data di scadenza.");
        if (frequency is not 1 and not 2 and not 4)
            throw new ArgumentException("La frequenza deve essere 1, 2, o 4.", nameof(frequency));
        if (rate < 0 || yld < 0)
            throw new ArgumentException("Il tasso cedolare e il rendimento devono essere >= 0.");
        var (dsc, e, n, a) = GetCouponDateInfo(settlement, maturity, frequency);
        var couponAmount = 100.0 * rate / frequency;
        var discount = 1 + yld / frequency;

        var price = redemption / Math.Pow(discount, n - 1 + dsc / e);
        for (var i = 1; i <= n; i++)
        {
            price += couponAmount / Math.Pow(discount, i - 1 + dsc / e);
        }
        price -= couponAmount * a / e;
        return price;
    }

    /// <summary>
    /// Calcola il rendimento a scadenza (YTM) di un'obbligazione dato il prezzo di mercato.
    /// Utilizza il metodo di Newton-Raphson per risolvere numericamente l'equazione del prezzo.
    /// </summary>
    /// <param name="settlement">Data di liquidazione.</param>
    /// <param name="maturity">Data di scadenza.</param>
    /// <param name="rate">Tasso cedolare annuale.</param>
    /// <param name="pr">Prezzo di mercato dell'obbligazione (dirty price).</param>
    /// <param name="redemption">Valore di rimborso.</param>
    /// <param name="frequency">Frequenza cedolare.</param>
    /// <param name="guess">Stima iniziale per il rendimento (default 0.1 = 10%).</param>
    /// <returns>Rendimento annuo a scadenza (YTM).</returns>
    /// <exception cref="ArgumentException">Sollevata se settlement &gt;= maturity.</exception>
    /// <exception cref="InvalidOperationException">Sollevata se il metodo non converge entro 100 iterazioni.</exception>
    /// <example>
    /// <code>
    /// var ytm = Bonds.BondYield(new DateTime(2024,1,1), new DateTime(2034,1,1), 0.05, 105.0, 100, 2);
    /// </code>
    /// </example>
    public static double BondYield(DateTime settlement, DateTime maturity, double rate, double pr,
        double redemption, int frequency, double guess = 0.1)
    {
        if (settlement >= maturity)
            throw new ArgumentException("La data di liquidazione deve essere precedente alla data di scadenza.");
        var yld = guess;
        const int maxIterations = 100;
        const double tolerance = 1.0e-8;
        for (var i = 0; i < maxIterations; i++)
        {
            var priceAtYield = BondPrice(settlement, maturity, rate, yld, redemption, frequency);
            var duration = CalculateDuration(settlement, maturity, rate, yld, redemption, frequency);
            var derivative =
                -duration * priceAtYield /
                (1 + yld / frequency);
            if (Math.Abs(derivative) < tolerance) break;
            var newYld = yld - (priceAtYield - pr) / derivative;
            if (Math.Abs(newYld - yld) < tolerance)
            {
                return newYld;
            }
            yld = newYld;
        }
        throw new InvalidOperationException(
            "Il calcolo del rendimento non converge. Provare con una stima (guess) differente.");
    }

    /// <summary>
    /// Calcola il rendimento equivalente su base obbligazionaria (BEY) per strumenti a breve termine.
    /// Formula standard: ((valore nominale - prezzo) / prezzo) × (365 / giorni a scadenza).
    /// </summary>
    /// <param name="faceValue">Valore nominale dell'obbligazione.</param>
    /// <param name="bondPrice">Prezzo corrente dell'obbligazione.</param>
    /// <param name="daysToMaturity">Numero di giorni alla scadenza.</param>
    /// <returns>Rendimento equivalente annualizzato.</returns>
    /// <exception cref="DivideByZeroException">Sollevata se bondPrice o daysToMaturity sono zero.</exception>
    /// <example>
    /// <code>
    /// var bey = Bonds.BondEquivalentYield(100, 98.5, 180);
    /// </code>
    /// </example>
    public static double BondEquivalentYield(double faceValue, double bondPrice, double daysToMaturity)
    {
        if (bondPrice == 0) throw new DivideByZeroException("Il prezzo dell'obbligazione non può essere zero.");
        if (daysToMaturity == 0) throw new DivideByZeroException("I giorni a scadenza non possono essere zero.");
        return ((faceValue - bondPrice) / bondPrice) * (365.0 / daysToMaturity);
    }

    /// <summary>
    /// Calcola il rendimento corrente (current yield) di un'obbligazione:
    /// rapporto tra cedola annuale e prezzo di mercato.
    /// </summary>
    /// <param name="annualCoupons">Importo totale delle cedole annuali.</param>
    /// <param name="bondPrice">Prezzo corrente dell'obbligazione.</param>
    /// <returns>Rendimento corrente (current yield).</returns>
    /// <exception cref="DivideByZeroException">Sollevata se bondPrice è zero.</exception>
    /// <example>
    /// <code>
    /// var cy = Bonds.CurrentYield(5.0, 105.0);
    /// </code>
    /// </example>
    public static double CurrentYield(double annualCoupons, double bondPrice)
    {
        if (bondPrice == 0) throw new DivideByZeroException("Il prezzo dell'obbligazione non può essere zero.");
        return annualCoupons / bondPrice;
    }

    /// <summary>
    /// Calcola il rendimento a scadenza (YTM) approssimato utilizzando la formula standard:
    /// (cedola + (valore nominale - prezzo) / anni) / ((valore nominale + prezzo) / 2).
    /// Utile per stime rapide senza iterazione numerica.
    /// </summary>
    /// <param name="couponPayment">Importo della cedola annuale.</param>
    /// <param name="faceValue">Valore nominale dell'obbligazione.</param>
    /// <param name="price">Prezzo corrente di mercato.</param>
    /// <param name="years">Anni residui alla scadenza.</param>
    /// <returns>Rendimento annuo approssimato.</returns>
    /// <exception cref="DivideByZeroException">Sollevata se years o (faceValue + price) / 2 sono zero.</exception>
    /// <example>
    /// <code>
    /// var ytm = Bonds.YieldToMaturity(5, 100, 105, 10);
    /// </code>
    /// </example>
    public static double YieldToMaturity(double couponPayment, double faceValue, double price, double years)
    {
        if (years == 0) throw new DivideByZeroException("Gli anni a scadenza non possono essere zero.");
        var numerator = couponPayment + ((faceValue - price) / years);
        var denominator = (faceValue + price) / 2.0;
        if (denominator == 0)
            throw new DivideByZeroException("La media tra valore nominale e prezzo non può essere zero.");
        return numerator / denominator;
    }

    /// <summary>
    /// Calcola il valore attuale di un'obbligazione zero-coupon (bullet bond).
    /// Formula: valore nominale / (1 + tasso)^tempo.
    /// </summary>
    /// <param name="faceValue">Valore nominale a scadenza.</param>
    /// <param name="rate">Tasso di attualizzazione annuo.</param>
    /// <param name="time">Tempo residuo in anni.</param>
    /// <returns>Valore attuale dell'obbligazione zero-coupon.</returns>
    /// <exception cref="ArgumentException">Sollevata se il tasso è -1 (denominatore nullo).</exception>
    /// <example>
    /// <code>
    /// var pv = Bonds.ZeroCouponBondValue(100, 0.04, 5);
    /// </code>
    /// </example>
    public static double ZeroCouponBondValue(double faceValue, double rate, double time)
    {
        if (1.0 + rate == 0) throw new ArgumentException("Il tasso non può essere -1.", nameof(rate));
        return faceValue / Math.Pow(1.0 + rate, time);
    }

    /// <summary>
    /// Calcola il rendimento implicito di un'obbligazione zero-coupon dato il prezzo corrente.
    /// Formula: (valore nominale / prezzo)^(1/tempo) - 1.
    /// </summary>
    /// <param name="faceValue">Valore nominale a scadenza.</param>
    /// <param name="price">Prezzo corrente di mercato.</param>
    /// <param name="time">Tempo residuo in anni.</param>
    /// <returns>Rendimento annuo implicito.</returns>
    /// <exception cref="DivideByZeroException">Sollevata se time o price sono zero.</exception>
    /// <example>
    /// <code>
    /// var ytm = Bonds.ZeroCouponBondYield(100, 82.0, 5);
    /// </code>
    /// </example>
    public static double ZeroCouponBondYield(double faceValue, double price, double time)
    {
        if (time == 0) throw new DivideByZeroException("Il tempo a scadenza non può essere zero.");
        if (price == 0) throw new DivideByZeroException("Il prezzo attuale non può essere zero.");
        return Math.Pow(faceValue / price, 1.0 / time) - 1.0;
    }

    /// <summary>
    /// Calcola la duration modificata di un'obbligazione, necessaria per il metodo Newton-Raphson
    /// nel calcolo dell'YTM. La duration è la media ponderata dei tempi dei flussi di cassa.
    /// </summary>
    private static double CalculateDuration(DateTime settlement, DateTime maturity, double rate, double yld,
        double redemption, int frequency)
    {
        var (dsc, e, n, a) = GetCouponDateInfo(settlement, maturity, frequency);
        var couponAmount = 100.0 * rate / frequency;
        double weightedCashFlowTimeSum = 0;
        var price = BondPrice(settlement, maturity, rate, yld, redemption, frequency) + (couponAmount * a / e);

        for (var i = 1; i <= n; i++)
        {
            var timeToFlow = (i - 1.0 + dsc / e) / frequency;
            var pvFactor = Math.Pow(1.0 + yld / frequency, i - 1.0 + dsc / e);
            var cashFlow = (i == n) ? couponAmount + redemption : couponAmount;

            weightedCashFlowTimeSum += timeToFlow * (cashFlow / pvFactor);
        }

        if (price == 0) return 0.0;
        return weightedCashFlowTimeSum / price;
    }

    /// <summary>
    /// Calcola le informazioni sulle date delle cedole: giorni tra cedola successiva e liquidazione (dsc),
    /// giorni nel periodo cedolare (e), numero di cedole residue (n), giorni dalla cedola precedente (a).
    /// </summary>
    private static (double dsc, double e, int n, double a) GetCouponDateInfo(DateTime settlement, DateTime maturity,
        int frequency)
    {
        var nextCouponDate = maturity;
        while (nextCouponDate > settlement)
        {
            nextCouponDate = GetPreviousCouponDate(nextCouponDate, frequency);
        }
        var prevCouponDate = nextCouponDate;
        nextCouponDate = GetNextCouponDate(nextCouponDate, frequency);
        var e = (nextCouponDate - prevCouponDate).TotalDays;
        var dsc = (nextCouponDate - settlement).TotalDays;
        var a = (settlement - prevCouponDate).TotalDays;
        var n = 1;
        var tempDate = nextCouponDate;
        while (tempDate < maturity)
        {
            tempDate = GetNextCouponDate(tempDate, frequency);
            n++;
        }
        return (dsc, e, n, a);
    }

    /// <summary>
    /// Calcola la data della cedola precedente rispetto a una data data.
    /// </summary>
    private static DateTime GetPreviousCouponDate(DateTime date, int frequency)
    {
        var months = 12 / frequency;
        return date.AddMonths(-months);
    }

    /// <summary>
    /// Calcola la data della cedola successiva rispetto a una data data.
    /// </summary>
    private static DateTime GetNextCouponDate(DateTime date, int frequency)
    {
        var months = 12 / frequency;
        return date.AddMonths(months);
    }
}
