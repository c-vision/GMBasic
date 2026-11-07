using static CommonLib.Numerics;

namespace CommonLib.Trading;

/// <summary>
/// Funzioni per la valutazione di opzioni finanziarie e il calcolo delle greche (sensibilità).
/// Supporta i modelli Black-Scholes, Merton (con dividend yield), albero binomiale CRR,
/// e tutte le principali greche di primo e secondo ordine.
///
/// Metodi:
/// - <see cref="BsCall"/>: prezzo call Black-Scholes.
/// - <see cref="BsPut"/>: prezzo put Black-Scholes.
/// - <see cref="MertonCall"/>: prezzo call Merton (con dividend yield).
/// - <see cref="MertonPut"/>: prezzo put Merton (con dividend yield).
/// - <see cref="BinomialOption"/>: prezzo con albero binomiale CRR.
/// - <see cref="Delta"/>: delta (sensibilità al sottostante).
/// - <see cref="Gamma"/>: gamma (sensibilità del delta).
/// - <see cref="Vega"/>: vega (sensibilità alla volatilità).
/// - <see cref="Theta"/>: theta (decadimento temporale).
/// - <see cref="Rho"/>: rho (sensibilità al tasso d'interesse).
/// - <see cref="Vanna"/>: vanna (cross gamma volatilità-sottostante).
/// - <see cref="Volga"/>: volga (gamma della volatilità).
/// - <see cref="Charm"/>: charm (delta decay).
/// - <see cref="Speed"/>: speed (gamma del gamma).
/// - <see cref="Zomma"/>: zomma (gamma-volga cross).
/// - <see cref="Color"/>: color (gamma decay).
/// </summary>
public static class Options
{
    /// <summary>
    /// Calcola il prezzo di un'opzione call europea secondo il modello di Black-Scholes.
    /// </summary>
    /// <param name="s">Prezzo corrente del sottostante (spot).</param>
    /// <param name="k">Prezzo di esercizio (strike).</param>
    /// <param name="t">Tempo residuo alla scadenza in anni.</param>
    /// <param name="r">Tasso d'interesse privo di rischio annuale.</param>
    /// <param name="v">Volatilità annualizzata del sottostante.</param>
    /// <returns>Premio teorico dell'opzione call.</returns>
    /// <example>
    /// <code>var call = Options.BsCall(100, 100, 1.0, 0.05, 0.20);</code>
    /// </example>
    public static double BsCall(double s, double k, double t, double r, double v)
    {
        if (t <= 0 || v <= 0) return Math.Max(0.0, s - k);

        var d1 = (Math.Log(s / k) + (r + 0.5 * v * v) * t) / (v * Math.Sqrt(t));
        var d2 = d1 - v * Math.Sqrt(t);

        return s * Cndf(d1) - k * Math.Exp(-r * t) * Cndf(d2);
    }

    /// <summary>
    /// Calcola il prezzo di un'opzione put europea secondo il modello di Black-Scholes.
    /// </summary>
    /// <param name="s">Prezzo corrente del sottostante (spot).</param>
    /// <param name="k">Prezzo di esercizio (strike).</param>
    /// <param name="t">Tempo residuo alla scadenza in anni.</param>
    /// <param name="r">Tasso d'interesse privo di rischio annuale.</param>
    /// <param name="v">Volatilità annualizzata del sottostante.</param>
    /// <returns>Premio teorico dell'opzione put.</returns>
    /// <example>
    /// <code>var put = Options.BsPut(100, 100, 1.0, 0.05, 0.20);</code>
    /// </example>
    public static double BsPut(double s, double k, double t, double r, double v)
    {
        if (t <= 0 || v <= 0) return Math.Max(0.0, k - s);

        var d1 = (Math.Log(s / k) + (r + 0.5 * v * v) * t) / (v * Math.Sqrt(t));
        var d2 = d1 - v * Math.Sqrt(t);

        return k * Math.Exp(-r * t) * Cndf(-d2) - s * Cndf(-d1);
    }

    /// <summary>
    /// Calcola il prezzo di un'opzione call europea con dividend yield (modello di Merton).
    /// Estensione del modello Black-Scholes per attività che pagano dividendi continui.
    /// </summary>
    /// <param name="s">Prezzo corrente del sottostante (spot).</param>
    /// <param name="k">Prezzo di esercizio (strike).</param>
    /// <param name="t">Tempo residuo alla scadenza in anni.</param>
    /// <param name="r">Tasso d'interesse privo di rischio annuale.</param>
    /// <param name="q">Dividend yield continuo annuale.</param>
    /// <param name="v">Volatilità annualizzata del sottostante.</param>
    /// <returns>Premio teorico dell'opzione call con dividend yield.</returns>
    /// <example>
    /// <code>var call = Options.MertonCall(100, 100, 1.0, 0.05, 0.02, 0.20);</code>
    /// </example>
    public static double MertonCall(double s, double k, double t, double r, double q, double v)
    {
        if (t <= 0 || v <= 0) return Math.Max(0.0, s - k);

        var d1 = (Math.Log(s / k) + (r - q + 0.5 * v * v) * t) / (v * Math.Sqrt(t));
        var d2 = d1 - v * Math.Sqrt(t);

        return s * Math.Exp(-q * t) * Cndf(d1) - k * Math.Exp(-r * t) * Cndf(d2);
    }

    /// <summary>
    /// Calcola il prezzo di un'opzione put europea con dividend yield (modello di Merton).
    /// </summary>
    /// <param name="s">Prezzo corrente del sottostante (spot).</param>
    /// <param name="k">Prezzo di esercizio (strike).</param>
    /// <param name="t">Tempo residuo alla scadenza in anni.</param>
    /// <param name="r">Tasso d'interesse privo di rischio annuale.</param>
    /// <param name="q">Dividend yield continuo annuale.</param>
    /// <param name="v">Volatilità annualizzata del sottostante.</param>
    /// <returns>Premio teorico dell'opzione put con dividend yield.</returns>
    /// <example>
    /// <code>var put = Options.MertonPut(100, 100, 1.0, 0.05, 0.02, 0.20);</code>
    /// </example>
    public static double MertonPut(double s, double k, double t, double r, double q, double v)
    {
        if (t <= 0 || v <= 0) return Math.Max(0.0, k - s);

        var d1 = (Math.Log(s / k) + (r - q + 0.5 * v * v) * t) / (v * Math.Sqrt(t));
        var d2 = d1 - v * Math.Sqrt(t);

        return k * Math.Exp(-r * t) * Cndf(-d2) - s * Math.Exp(-q * t) * Cndf(-d1);
    }

    /// <summary>
    /// Calcola il prezzo di un'opzione (europea o americana, call o put) utilizzando
    /// l'albero binomiale di Cox-Ross-Rubinstein (CRR).
    /// </summary>
    /// <param name="s">Prezzo corrente del sottostante (spot).</param>
    /// <param name="k">Prezzo di esercizio (strike).</param>
    /// <param name="t">Tempo residuo alla scadenza in anni.</param>
    /// <param name="r">Tasso d'interesse privo di rischio annuale.</param>
    /// <param name="v">Volatilità annualizzata del sottostante.</param>
    /// <param name="steps">Numero di passi nell'albero binomiale.</param>
    /// <param name="isPut">True per opzione put, false per call.</param>
    /// <param name="isAmerican">True per opzione americana (esercizio anticipato), false per europea.</param>
    /// <returns>Premio teorico dell'opzione.</returns>
    /// <exception cref="ArgumentOutOfRangeException">Sollevata se steps &lt;= 0.</exception>
    /// <exception cref="InvalidOperationException">Sollevata se le condizioni di non arbitraggio non sono soddisfatte.</exception>
    /// <example>
    /// <code>
    /// var prezzo = Options.BinomialOption(100, 100, 1.0, 0.05, 0.20, 100, false, false);
    /// </code>
    /// </example>
    public static double BinomialOption(double s, double k, double t, double r, double v, int steps, bool isPut,
        bool isAmerican)
    {
        if (steps <= 0)
            throw new ArgumentOutOfRangeException(nameof(steps), "Il numero di passi deve essere positivo.");

        var dt = t / steps;
        var u = Math.Exp(v * Math.Sqrt(dt));
        var d = 1.0 / u;
        var p = (Math.Exp(r * dt) - d) / (u - d);

        if (p is < 0 or > 1)
            throw new InvalidOperationException(
                "Condizioni di non arbitraggio non soddisfatte. Controllare i parametri.");

        var optionValues = new double[steps + 1];

        for (var i = 0; i <= steps; i++)
        {
            var assetPrice = s * Math.Pow(u, i) * Math.Pow(d, steps - i);
            optionValues[i] = Math.Max(0.0, isPut ? k - assetPrice : assetPrice - k);
        }

        for (var j = steps - 1; j >= 0; j--)
        {
            for (var i = 0; i <= j; i++)
            {
                var continuationValue = Math.Exp(-r * dt) * (p * optionValues[i + 1] + (1 - p) * optionValues[i]);

                if (isAmerican)
                {
                    var assetPrice = s * Math.Pow(u, i) * Math.Pow(d, j - i);
                    var exerciseValue = Math.Max(0.0, isPut ? k - assetPrice : assetPrice - k);
                    optionValues[i] = Math.Max(continuationValue, exerciseValue);
                }
                else
                {
                    optionValues[i] = continuationValue;
                }
            }
        }

        return optionValues[0];
    }

    /// <summary>
    /// Calcola il Delta di un'opzione: sensibilità del premio a variazioni del prezzo del sottostante.
    /// </summary>
    /// <param name="s">Prezzo corrente del sottostante.</param>
    /// <param name="k">Prezzo di esercizio.</param>
    /// <param name="t">Tempo residuo in anni.</param>
    /// <param name="r">Tasso privo di rischio.</param>
    /// <param name="v">Volatilità.</param>
    /// <param name="isPut">True per put, false per call.</param>
    /// <returns>Delta dell'opzione (da -1 a 1).</returns>
    /// <example>
    /// <code>var delta = Options.Delta(100, 100, 1.0, 0.05, 0.20, false);</code>
    /// </example>
    public static double Delta(double s, double k, double t, double r, double v, bool isPut)
    {
        if (t <= 0 || v <= 0) return isPut ? (s < k ? -1.0 : 0.0) : (s > k ? 1.0 : 0.0);
        var d1 = (Math.Log(s / k) + (r + 0.5 * v * v) * t) / (v * Math.Sqrt(t));
        return isPut ? Cndf(d1) - 1.0 : Cndf(d1);
    }

    /// <summary>
    /// Calcola il Gamma di un'opzione: sensibilità del Delta a variazioni del prezzo del sottostante.
    /// </summary>
    /// <param name="s">Prezzo corrente del sottostante.</param>
    /// <param name="k">Prezzo di esercizio.</param>
    /// <param name="t">Tempo residuo in anni.</param>
    /// <param name="r">Tasso privo di rischio.</param>
    /// <param name="v">Volatilità.</param>
    /// <returns>Gamma dell'opzione.</returns>
    /// <example>
    /// <code>var gamma = Options.Gamma(100, 100, 1.0, 0.05, 0.20);</code>
    /// </example>
    public static double Gamma(double s, double k, double t, double r, double v)
    {
        if (t <= 0 || v <= 0 || s <= 0) return 0.0;
        var d1 = (Math.Log(s / k) + (r + 0.5 * v * v) * t) / (v * Math.Sqrt(t));
        return Ndf(d1) / (s * v * Math.Sqrt(t));
    }

    /// <summary>
    /// Calcola il Vega di un'opzione: sensibilità del premio a variazioni della volatilità di 1 punto percentuale.
    /// </summary>
    /// <param name="s">Prezzo corrente del sottostante.</param>
    /// <param name="k">Prezzo di esercizio.</param>
    /// <param name="t">Tempo residuo in anni.</param>
    /// <param name="r">Tasso privo di rischio.</param>
    /// <param name="v">Volatilità.</param>
    /// <returns>Vega dell'opzione (variazione per +1% di volatilità).</returns>
    /// <example>
    /// <code>var vega = Options.Vega(100, 100, 1.0, 0.05, 0.20);</code>
    /// </example>
    public static double Vega(double s, double k, double t, double r, double v)
    {
        if (t <= 0 || v <= 0) return 0.0;
        var d1 = (Math.Log(s / k) + (r + 0.5 * v * v) * t) / (v * Math.Sqrt(t));
        return s * Ndf(d1) * Math.Sqrt(t) * 0.01;
    }

    /// <summary>
    /// Calcola il Theta di un'opzione: decadimento temporale del premio per giorno.
    /// </summary>
    /// <param name="s">Prezzo corrente del sottostante.</param>
    /// <param name="k">Prezzo di esercizio.</param>
    /// <param name="t">Tempo residuo in anni.</param>
    /// <param name="r">Tasso privo di rischio.</param>
    /// <param name="v">Volatilità.</param>
    /// <param name="isPut">True per put, false per call.</param>
    /// <returns>Theta giornaliero (negativo per opzioni long).</returns>
    /// <example>
    /// <code>var theta = Options.Theta(100, 100, 1.0, 0.05, 0.20, false);</code>
    /// </example>
    public static double Theta(double s, double k, double t, double r, double v, bool isPut)
    {
        if (t <= 0 || v <= 0) return 0.0;
        var d1 = (Math.Log(s / k) + (r + 0.5 * v * v) * t) / (v * Math.Sqrt(t));
        var d2 = d1 - v * Math.Sqrt(t);
        var term1 = -(s * Ndf(d1) * v) / (2.0 * Math.Sqrt(t));

        double theta;
        if (isPut)
        {
            theta = term1 + r * k * Math.Exp(-r * t) * Cndf(-d2);
        }
        else
        {
            theta = term1 - r * k * Math.Exp(-r * t) * Cndf(d2);
        }

        return theta / 365.0;
    }

    /// <summary>
    /// Calcola il Rho di un'opzione: sensibilità del premio a variazioni del tasso d'interesse di 1 punto percentuale.
    /// </summary>
    /// <param name="s">Prezzo corrente del sottostante.</param>
    /// <param name="k">Prezzo di esercizio.</param>
    /// <param name="t">Tempo residuo in anni.</param>
    /// <param name="r">Tasso privo di rischio.</param>
    /// <param name="v">Volatilità.</param>
    /// <param name="isPut">True per put, false per call.</param>
    /// <returns>Rho dell'opzione (variazione per +1% del tasso).</returns>
    /// <example>
    /// <code>var rho = Options.Rho(100, 100, 1.0, 0.05, 0.20, false);</code>
    /// </example>
    public static double Rho(double s, double k, double t, double r, double v, bool isPut)
    {
        if (t <= 0) return 0.0;
        var d1 = (Math.Log(s / k) + (r + 0.5 * v * v) * t) / (v * Math.Sqrt(t));
        var d2 = d1 - v * Math.Sqrt(t);
        double rho;
        if (isPut)
        {
            rho = -k * t * Math.Exp(-r * t) * Cndf(-d2);
        }
        else
        {
            rho = k * t * Math.Exp(-r * t) * Cndf(d2);
        }

        return rho * 0.01;
    }

    /// <summary>
    /// Calcola il Vanna di un'opzione: sensibilità del Delta a variazioni della volatilità.
    /// </summary>
    /// <param name="s">Prezzo corrente del sottostante.</param>
    /// <param name="k">Prezzo di esercizio.</param>
    /// <param name="t">Tempo residuo in anni.</param>
    /// <param name="r">Tasso privo di rischio.</param>
    /// <param name="v">Volatilità.</param>
    /// <returns>Vanna dell'opzione.</returns>
    /// <example>
    /// <code>var vanna = Options.Vanna(100, 100, 1.0, 0.05, 0.20);</code>
    /// </example>
    public static double Vanna(double s, double k, double t, double r, double v)
    {
        if (t <= 0 || v <= 0) return 0.0;
        var d1 = (Math.Log(s / k) + (r + 0.5 * v * v) * t) / (v * Math.Sqrt(t));
        var d2 = d1 - v * Math.Sqrt(t);
        return -Ndf(d1) * d2 / v;
    }

    /// <summary>
    /// Calcola il Volga (Vomma) di un'opzione: sensibilità del Vega a variazioni della volatilità.
    /// </summary>
    /// <param name="s">Prezzo corrente del sottostante.</param>
    /// <param name="k">Prezzo di esercizio.</param>
    /// <param name="t">Tempo residuo in anni.</param>
    /// <param name="r">Tasso privo di rischio.</param>
    /// <param name="v">Volatilità.</param>
    /// <returns>Volga (Vomma) dell'opzione.</returns>
    /// <example>
    /// <code>var volga = Options.Volga(100, 100, 1.0, 0.05, 0.20);</code>
    /// </example>
    public static double Volga(double s, double k, double t, double r, double v)
    {
        if (t <= 0 || v <= 0) return 0.0;
        var d1 = (Math.Log(s / k) + (r + 0.5 * v * v) * t) / (v * Math.Sqrt(t));
        var d2 = d1 - v * Math.Sqrt(t);
        return s * Ndf(d1) * Math.Sqrt(t) * d1 * d2 / v;
    }

    /// <summary>
    /// Calcola il Charm (delta decay): sensibilità del Delta al passaggio del tempo.
    /// </summary>
    /// <param name="s">Prezzo corrente del sottostante.</param>
    /// <param name="k">Prezzo di esercizio.</param>
    /// <param name="t">Tempo residuo in anni.</param>
    /// <param name="r">Tasso privo di rischio.</param>
    /// <param name="v">Volatilità.</param>
    /// <param name="isPut">True per put, false per call.</param>
    /// <returns>Charm giornaliero.</returns>
    /// <example>
    /// <code>var charm = Options.Charm(100, 100, 1.0, 0.05, 0.20, false);</code>
    /// </example>
    public static double Charm(double s, double k, double t, double r, double v, bool isPut)
    {
        if (t <= 0 || v <= 0) return 0.0;
        var d1 = (Math.Log(s / k) + (r + 0.5 * v * v) * t) / (v * Math.Sqrt(t));
        var d2 = d1 - v * Math.Sqrt(t);
        var charm = -Ndf(d1) * (2.0 * r * t - d2 * v * Math.Sqrt(t)) / (2.0 * t * v * Math.Sqrt(t));
        if (isPut)
            charm += r * Math.Exp(-r * t) * Cndf(-d2);
        else
            charm -= r * Math.Exp(-r * t) * Cndf(d2);
        return charm / 365.0;
    }

    /// <summary>
    /// Calcola lo Speed di un'opzione: sensibilità del Gamma a variazioni del prezzo del sottostante (terza derivata).
    /// </summary>
    /// <param name="s">Prezzo corrente del sottostante.</param>
    /// <param name="k">Prezzo di esercizio.</param>
    /// <param name="t">Tempo residuo in anni.</param>
    /// <param name="r">Tasso privo di rischio.</param>
    /// <param name="v">Volatilità.</param>
    /// <returns>Speed dell'opzione.</returns>
    /// <example>
    /// <code>var speed = Options.Speed(100, 100, 1.0, 0.05, 0.20);</code>
    /// </example>
    public static double Speed(double s, double k, double t, double r, double v)
    {
        if (t <= 0 || v <= 0) return 0.0;
        var d1 = (Math.Log(s / k) + (r + 0.5 * v * v) * t) / (v * Math.Sqrt(t));
        var gamma = Gamma(s, k, t, r, v);
        return -gamma / s * (d1 / (v * Math.Sqrt(t)) + 1.0);
    }

    /// <summary>
    /// Calcola lo Zomma di un'opzione: sensibilità del Gamma a variazioni della volatilità.
    /// </summary>
    /// <param name="s">Prezzo corrente del sottostante.</param>
    /// <param name="k">Prezzo di esercizio.</param>
    /// <param name="t">Tempo residuo in anni.</param>
    /// <param name="r">Tasso privo di rischio.</param>
    /// <param name="v">Volatilità.</param>
    /// <returns>Zomma dell'opzione.</returns>
    /// <example>
    /// <code>var zomma = Options.Zomma(100, 100, 1.0, 0.05, 0.20);</code>
    /// </example>
    public static double Zomma(double s, double k, double t, double r, double v)
    {
        if (t <= 0 || v <= 0) return 0.0;
        var d1 = (Math.Log(s / k) + (r + 0.5 * v * v) * t) / (v * Math.Sqrt(t));
        var d2 = d1 - v * Math.Sqrt(t);
        var gamma = Gamma(s, k, t, r, v);
        return gamma * ((d1 * d2 - 1.0) / v);
    }

    /// <summary>
    /// Calcola il Color (gamma decay): sensibilità del Gamma al passaggio del tempo (derivata temporale del Gamma).
    /// </summary>
    /// <param name="s">Prezzo corrente del sottostante.</param>
    /// <param name="k">Prezzo di esercizio.</param>
    /// <param name="t">Tempo residuo in anni.</param>
    /// <param name="r">Tasso privo di rischio.</param>
    /// <param name="v">Volatilità.</param>
    /// <returns>Color dell'opzione.</returns>
    /// <example>
    /// <code>var color = Options.Color(100, 100, 1.0, 0.05, 0.20);</code>
    /// </example>
    public static double Color(double s, double k, double t, double r, double v)
    {
        if (t <= 0 || v <= 0) return 0.0;
        var d1 = (Math.Log(s / k) + (r + 0.5 * v * v) * t) / (v * Math.Sqrt(t));
        var gamma = Gamma(s, k, t, r, v);
        return -gamma / (2.0 * t) * (1.0 + d1 / (v * Math.Sqrt(t)));
    }
}
