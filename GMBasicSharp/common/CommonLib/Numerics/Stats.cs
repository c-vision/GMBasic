// FILE: StdLib.Numerics.Stats.cs
// ReSharper disable CheckNamespace

using MathNet.Numerics.Distributions;
using MathNet.Numerics.Statistics;
// ReSharper disable InconsistentNaming

namespace CommonLib;

/// <summary>
/// Fornisce funzioni statistiche e di distribuzione di probabilità per il runtime BASIC.
/// Contiene i seguenti metodi:
/// <list type="bullet">
///   <item><description><c>Mean</c> — Media aritmetica.</description></item>
///   <item><description><c>Median</c> — Mediana.</description></item>
///   <item><description><c>Variance</c> — Varianza campionaria.</description></item>
///   <item><description><c>StdDev</c> — Deviazione standard campionaria.</description></item>
///   <item><description><c>Skewness</c> — Asimmetria (skewness).</description></item>
///   <item><description><c>Kurtosis</c> — Curtosi.</description></item>
///   <item><description><c>Correlation</c> — Correlazione di Pearson.</description></item>
///   <item><description><c>Min</c> — Valore minimo.</description></item>
///   <item><description><c>Max</c> — Valore massimo.</description></item>
///   <item><description><c>Percentile</c> — Percentile.</description></item>
///   <item><description><c>Mad</c> — Deviazione assoluta mediana (MAD).</description></item>
///   <item><description><c>Cndf</c> — Funzione di ripartizione normale standard (CDF).</description></item>
///   <item><description><c>Ndf</c> — Funzione di densità normale standard (PDF).</description></item>
///   <item><description><c>NormalPdf</c> — PDF della distribuzione normale.</description></item>
///   <item><description><c>NormalCdf</c> — CDF della distribuzione normale.</description></item>
///   <item><description><c>LognormalPdf</c> — PDF della distribuzione lognormale.</description></item>
///   <item><description><c>LognormalCdf</c> — CDF della distribuzione lognormale.</description></item>
///   <item><description><c>ChiPdf</c> — PDF della distribuzione chi.</description></item>
///   <item><description><c>ChiCdf</c> — CDF della distribuzione chi.</description></item>
///   <item><description><c>ChiSquaredPdf</c> — PDF della distribuzione chi-quadro.</description></item>
///   <item><description><c>ChiSquaredCdf</c> — CDF della distribuzione chi-quadro.</description></item>
///   <item><description><c>BetaPdf</c> — PDF della distribuzione Beta.</description></item>
///   <item><description><c>BetaCdf</c> — CDF della distribuzione Beta.</description></item>
///   <item><description><c>StudentTPdf</c> — PDF della distribuzione t di Student.</description></item>
///   <item><description><c>StudentTCdf</c> — CDF della distribuzione t di Student.</description></item>
///   <item><description><c>WeibullPdf</c> — PDF della distribuzione di Weibull.</description></item>
///   <item><description><c>WeibullCdf</c> — CDF della distribuzione di Weibull.</description></item>
///   <item><description><c>FPdf</c> — PDF della distribuzione F di Fisher-Snedecor.</description></item>
///   <item><description><c>FCdf</c> — CDF della distribuzione F di Fisher-Snedecor.</description></item>
///   <item><description><c>ExponentialPdf</c> — PDF della distribuzione esponenziale.</description></item>
///   <item><description><c>ExponentialCdf</c> — CDF della distribuzione esponenziale.</description></item>
///   <item><description><c>ParetoPdf</c> — PDF della distribuzione di Pareto.</description></item>
///   <item><description><c>LogisticPdf</c> — PDF della distribuzione logistica.</description></item>
///   <item><description><c>TriangularPdf</c> — PDF della distribuzione triangolare.</description></item>
///   <item><description><c>UniformPdf</c> — PDF della distribuzione uniforme continua.</description></item>
///   <item><description><c>SkewedNormalPdf</c> — PDF della distribuzione normale asimmetrica.</description></item>
///   <item><description><c>BernoulliPmf</c> — PMF della distribuzione di Bernoulli.</description></item>
///   <item><description><c>BinomialPmf</c> — PMF della distribuzione binomiale.</description></item>
///   <item><description><c>PoissonPmf</c> — PMF della distribuzione di Poisson.</description></item>
///   <item><description><c>GeometricPmf</c> — PMF della distribuzione geometrica.</description></item>
///   <item><description><c>NegativeBinomialPmf</c> — PMF della distribuzione binomiale negativa.</description></item>
///   <item><description><c>HypergeometricPmf</c> — PMF della distribuzione ipergeometrica.</description></item>
/// </list>
/// </summary>
public static partial class Numerics
{
    // =====================================================================
    // Descriptive Statistics
    // =====================================================================

    /// <summary>
    /// Calcola la media aritmetica di un array di dati.
    /// </summary>
    /// <param name="data">Array di valori double.</param>
    /// <returns>La media aritmetica.</returns>
    /// <exception cref="ArgumentException">Se l'array è null o vuoto.</exception>
    /// <example>double mean = Numerics.Mean(new double[] {1.0, 2.0, 3.0}); // restituisce 2.0</example>
    public static double Mean(double[] data)
    {
        if (data == null || data.Length == 0)
            throw new ArgumentException("L'array di dati per MEAN non può essere vuoto.");
        return data.Mean();
    }

    /// <summary>
    /// Calcola la mediana di un array di dati.
    /// </summary>
    /// <param name="data">Array di valori double.</param>
    /// <returns>La mediana (50° percentile).</returns>
    /// <exception cref="ArgumentException">Se l'array è null o vuoto.</exception>
    /// <example>double median = Numerics.Median(new double[] {1.0, 3.0, 2.0}); // restituisce 2.0</example>
    public static double Median(double[] data)
    {
        if (data == null || data.Length == 0)
            throw new ArgumentException("L'array di dati per MEDIAN non può essere vuoto.");
        return data.Median();
    }

    /// <summary>
    /// Calcola la varianza campionaria di un array di dati.
    /// </summary>
    /// <param name="data">Array di valori double (almeno due elementi).</param>
    /// <returns>La varianza campionaria.</returns>
    /// <exception cref="ArgumentException">Se l'array è null o ha meno di due elementi.</exception>
    /// <example>double variance = Numerics.Variance(new double[] {1.0, 2.0, 3.0}); // restituisce 1.0</example>
    public static double Variance(double[] data)
    {
        if (data == null || data.Length <= 1)
            throw new ArgumentException("L'array di dati per VARIANCE deve contenere almeno due elementi.");
        return data.Variance();
    }

    /// <summary>
    /// Calcola la deviazione standard campionaria di un array di dati.
    /// </summary>
    /// <param name="data">Array di valori double (almeno due elementi).</param>
    /// <returns>La deviazione standard campionaria.</returns>
    /// <exception cref="ArgumentException">Se l'array è null o ha meno di due elementi.</exception>
    /// <example>double std = Numerics.StdDev(new double[] {1.0, 2.0, 3.0}); // restituisce 1.0</example>
    public static double StdDev(double[] data)
    {
        if (data == null || data.Length <= 1)
            throw new ArgumentException("L'array di dati per STDDEV deve contenere almeno due elementi.");
        return data.StandardDeviation();
    }

    /// <summary>
    /// Calcola l'asimmetria (skewness) campionaria di un array di dati.
    /// </summary>
    /// <param name="data">Array di valori double (almeno tre elementi).</param>
    /// <returns>Il coefficiente di asimmetria.</returns>
    /// <exception cref="ArgumentException">Se l'array è null o ha meno di tre elementi.</exception>
    /// <example>double skew = Numerics.Skewness(new double[] {1.0, 2.0, 3.0, 10.0});</example>
    public static double Skewness(double[] data)
    {
        if (data == null || data.Length < 3)
            throw new ArgumentException("L'array di dati per SKEWNESS deve contenere almeno tre elementi.");
        return data.Skewness();
    }

    /// <summary>
    /// Calcola la curtosi campionaria di un array di dati.
    /// </summary>
    /// <param name="data">Array di valori double (almeno quattro elementi).</param>
    /// <returns>La curtosi (eccesso di curtosi).</returns>
    /// <exception cref="ArgumentException">Se l'array è null o ha meno di quattro elementi.</exception>
    /// <example>double kurt = Numerics.Kurtosis(new double[] {1.0, 2.0, 3.0, 4.0, 5.0});</example>
    public static double Kurtosis(double[] data)
    {
        if (data == null || data.Length < 4)
            throw new ArgumentException("L'array di dati per KURTOSIS deve contenere almeno quattro elementi.");
        return data.Kurtosis();
    }

    /// <summary>
    /// Calcola il coefficiente di correlazione lineare di Pearson tra due array di dati.
    /// </summary>
    /// <param name="dataA">Primo array di valori.</param>
    /// <param name="dataB">Secondo array di valori (stessa lunghezza di dataA).</param>
    /// <returns>Il coefficiente di correlazione di Pearson (valore tra -1 e 1).</returns>
    /// <exception cref="ArgumentException">Se gli array sono null, vuoti o di lunghezza diversa.</exception>
    /// <example>double r = Numerics.Correlation(new double[] {1, 2, 3}, new double[] {2, 4, 6}); // restituisce 1.0</example>
    public static double Correlation(double[] dataA, double[] dataB)
    {
        if (dataA == null || dataB == null || dataA.Length == 0 || dataB.Length == 0)
            throw new ArgumentException("Gli array di dati per CORRELATION non possono essere vuoti.");
        if (dataA.Length != dataB.Length)
            throw new ArgumentException("Gli array di dati per CORRELATION devono avere la stessa lunghezza.");
        return MathNet.Numerics.Statistics.Correlation.Pearson(dataA, dataB);
    }

    /// <summary>
    /// Restituisce il valore minimo in un array di dati.
    /// </summary>
    /// <param name="data">Array di valori double.</param>
    /// <returns>Il valore minimo.</returns>
    /// <exception cref="ArgumentException">Se l'array è null o vuoto.</exception>
    /// <example>double min = Numerics.Min(new double[] {3.0, 1.0, 2.0}); // restituisce 1.0</example>
    public static double Min(double[] data)
    {
        if (data == null || data.Length == 0)
            throw new ArgumentException("L'array di dati per MIN non può essere vuoto.");
        return data.Minimum();
    }

    /// <summary>
    /// Restituisce il valore massimo in un array di dati.
    /// </summary>
    /// <param name="data">Array di valori double.</param>
    /// <returns>Il valore massimo.</returns>
    /// <exception cref="ArgumentException">Se l'array è null o vuoto.</exception>
    /// <example>double max = Numerics.Max(new double[] {3.0, 1.0, 2.0}); // restituisce 3.0</example>
    public static double Max(double[] data)
    {
        if (data == null || data.Length == 0)
            throw new ArgumentException("L'array di dati per MAX non può essere vuoto.");
        return data.Maximum();
    }

    /// <summary>
    /// Calcola il percentile specificato di un array di dati.
    /// </summary>
    /// <param name="data">Array di valori double.</param>
    /// <param name="fraction">Frazione percentile compresa tra 0.0 e 1.0 (es. 0.5 = mediana).</param>
    /// <returns>Il valore al percentile specificato.</returns>
    /// <exception cref="ArgumentException">Se l'array è null o vuoto.</exception>
    /// <exception cref="ArgumentOutOfRangeException">Se fraction è fuori dall'intervallo [0, 1].</exception>
    /// <example>double p50 = Numerics.Percentile(new double[] {1.0, 2.0, 3.0, 4.0, 5.0}, 0.5); // restituisce 3.0</example>
    public static double Percentile(double[] data, double fraction)
    {
        if (data == null || data.Length == 0)
            throw new ArgumentException("Array vuoto");
        if (fraction is < 0.0 or > 1.0)
            throw new ArgumentOutOfRangeException(nameof(fraction), "Il percentile deve essere tra 0 e 1");
        var perc = (int)Math.Round(fraction * 100);
        return data.Percentile(perc);
    }

    /// <summary>
    /// Calcola la deviazione assoluta mediana (MAD), una misura robusta della dispersione statistica.
    /// </summary>
    /// <param name="data">Array di valori double.</param>
    /// <returns>La mediana delle deviazioni assolute dalla mediana.</returns>
    /// <exception cref="ArgumentException">Se l'array è null o vuoto.</exception>
    /// <example>double mad = Numerics.Mad(new double[] {1.0, 2.0, 3.0, 4.0, 5.0});</example>
    public static double Mad(double[] data)
    {
        if (data == null || data.Length == 0)
            throw new ArgumentException("L'array di dati per MAD non può essere vuoto.");
        var median = data.Median();
        var absDev = new double[data.Length];
        for (var i = 0; i < data.Length; i++)
            absDev[i] = Math.Abs(data[i] - median);
        return absDev.Median();
    }

    // =====================================================================
    // Distribution Functions
    // =====================================================================

    /// <summary>
    /// Calcola la funzione di ripartizione (CDF) della distribuzione normale standard N(0,1).
    /// </summary>
    /// <param name="z">Punteggio z (numero di deviazioni standard).</param>
    /// <returns>La probabilità cumulativa P(Z ≤ z).</returns>
    /// <example>double prob = Numerics.Cndf(1.96); // restituisce circa 0.975</example>
    public static double Cndf(double z) => Normal.CDF(0, 1, z);

    /// <summary>
    /// Calcola la funzione di densità di probabilità (PDF) della distribuzione normale standard N(0,1).
    /// </summary>
    /// <param name="z">Punteggio z.</param>
    /// <returns>Il valore della densità di probabilità in z.</returns>
    /// <example>double pdf = Numerics.Ndf(0); // restituisce circa 0.3989</example>
    public static double Ndf(double z) => Normal.PDF(0, 1, z);

    /// <summary>
    /// Calcola la funzione di densità di probabilità (PDF) della distribuzione normale.
    /// </summary>
    /// <param name="mean">Media della distribuzione.</param>
    /// <param name="stdDev">Deviazione standard. Deve essere positiva.</param>
    /// <param name="x">Punto in cui valutare la PDF.</param>
    /// <returns>Il valore della densità di probabilità in x.</returns>
    /// <exception cref="ArgumentOutOfRangeException">Se stdDev ≤ 0.</exception>
    /// <example>double pdf = Numerics.NormalPdf(0, 1, 0); // circa 0.3989</example>
    public static double NormalPdf(double mean, double stdDev, double x)
    {
        if (stdDev <= 0) throw new ArgumentOutOfRangeException(nameof(stdDev), "La deviazione standard deve essere positiva.");
        return Normal.PDF(mean, stdDev, x);
    }

    /// <summary>
    /// Calcola la funzione di ripartizione (CDF) della distribuzione normale.
    /// </summary>
    /// <param name="mean">Media della distribuzione.</param>
    /// <param name="stdDev">Deviazione standard. Deve essere positiva.</param>
    /// <param name="x">Punto in cui valutare la CDF.</param>
    /// <returns>La probabilità cumulativa P(X ≤ x).</returns>
    /// <exception cref="ArgumentOutOfRangeException">Se stdDev ≤ 0.</exception>
    /// <example>double cdf = Numerics.NormalCdf(0, 1, 0); // restituisce 0.5</example>
    public static double NormalCdf(double mean, double stdDev, double x)
    {
        if (stdDev <= 0) throw new ArgumentOutOfRangeException(nameof(stdDev), "La deviazione standard deve essere positiva.");
        return Normal.CDF(mean, stdDev, x);
    }

    /// <summary>
    /// Calcola la funzione di densità di probabilità (PDF) della distribuzione lognormale.
    /// </summary>
    /// <param name="mu">Parametro di scala (media del log).</param>
    /// <param name="sigma">Parametro di forma (deviazione standard del log).</param>
    /// <param name="x">Punto in cui valutare la PDF.</param>
    /// <returns>Il valore della densità, oppure 0 se sigma ≤ 0 o x ≤ 0.</returns>
    /// <example>double pdf = Numerics.LognormalPdf(0, 1, 1.0);</example>
    public static double LognormalPdf(double mu, double sigma, double x)
    {
        if (sigma <= 0 || x <= 0) return 0.0;
        return LogNormal.PDF(mu, sigma, x);
    }

    /// <summary>
    /// Calcola la funzione di ripartizione (CDF) della distribuzione lognormale.
    /// </summary>
    /// <param name="mu">Parametro di scala (media del log).</param>
    /// <param name="sigma">Parametro di forma (deviazione standard del log).</param>
    /// <param name="x">Punto in cui valutare la CDF.</param>
    /// <returns>La probabilità cumulativa P(X ≤ x), oppure 0 se x ≤ 0.</returns>
    /// <example>double cdf = Numerics.LognormalCdf(0, 1, 1.0);</example>
    public static double LognormalCdf(double mu, double sigma, double x)
    {
        if (x <= 0) return 0.0;
        return LogNormal.CDF(mu, sigma, x);
    }

    /// <summary>
    /// Calcola la funzione di densità di probabilità (PDF) della distribuzione chi.
    /// </summary>
    /// <param name="freedom">Gradi di libertà.</param>
    /// <param name="x">Punto in cui valutare la PDF.</param>
    /// <returns>Il valore della densità, oppure 0 se freedom ≤ 0 o x &lt; 0.</returns>
    /// <example>double pdf = Numerics.ChiPdf(3, 1.0);</example>
    public static double ChiPdf(double freedom, double x)
    {
        if (freedom <= 0 || x < 0) return 0.0;
        return Chi.PDF(freedom, x);
    }

    /// <summary>
    /// Calcola la funzione di ripartizione (CDF) della distribuzione chi.
    /// </summary>
    /// <param name="freedom">Gradi di libertà.</param>
    /// <param name="x">Punto in cui valutare la CDF.</param>
    /// <returns>La probabilità cumulativa P(X ≤ x), oppure 0 se x &lt; 0.</returns>
    /// <exception cref="ArgumentOutOfRangeException">Se freedom ≤ 0.</exception>
    /// <example>double cdf = Numerics.ChiCdf(3, 1.0);</example>
    public static double ChiCdf(double freedom, double x)
    {
        if (freedom <= 0) throw new ArgumentOutOfRangeException(nameof(freedom));
        return x < 0 ? 0.0 : Chi.CDF(freedom, x);
    }
    /// <summary>Funzione di sopravvivenza Chi (1 - CDF).</summary>
    public static double ChiSf(double k, double x) => 1.0 - ChiCdf(k, x);


    /// <summary>
    /// Calcola la funzione di densità di probabilità (PDF) della distribuzione chi-quadro (χ²).
    /// </summary>
    /// <param name="freedom">Gradi di libertà.</param>
    /// <param name="x">Punto in cui valutare la PDF.</param>
    /// <returns>Il valore della densità, oppure 0 se freedom ≤ 0 o x &lt; 0.</returns>
    /// <example>double pdf = Numerics.ChiSquaredPdf(3, 1.0);</example>
    public static double ChiSquaredPdf(double freedom, double x)
    {
        if (freedom <= 0 || x < 0) return 0.0;
        return ChiSquared.PDF(freedom, x);
    }

    /// <summary>
    /// Calcola la funzione di ripartizione (CDF) della distribuzione chi-quadro (χ²).
    /// </summary>
    /// <param name="freedom">Gradi di libertà.</param>
    /// <param name="x">Punto in cui valutare la CDF.</param>
    /// <returns>La probabilità cumulativa P(X ≤ x), oppure 0 se x &lt; 0.</returns>
    /// <exception cref="ArgumentOutOfRangeException">Se freedom ≤ 0.</exception>
    /// <example>double cdf = Numerics.ChiSquaredCdf(3, 1.0);</example>
    public static double ChiSquaredCdf(double freedom, double x)
    {
        if (freedom <= 0) throw new ArgumentOutOfRangeException(nameof(freedom));
        return x < 0 ? 0.0 : ChiSquared.CDF(freedom, x);
    }


    public static double NcBetaPdf(double a, double b, double nc, double x)
    {
        if (a <= 0 || b <= 0 || nc < 0) return double.NaN;
        double half = nc * 0.5, w = Math.Exp(-half), sum = 0;
        for (var j = 0; j < 300; j++) { sum += w * BetaPdf(a + j, b, x); w *= half / (j + 1.0); }
        return sum;
    }

    public static double NcBetaCdf(double a, double b, double nc, double x)
    {
        if (a <= 0 || b <= 0 || nc < 0) return double.NaN;
        if (x <= 0) return 0;
        if (x >= 1) return 1;
        double half = nc * 0.5, w = Math.Exp(-half), sum = 0;
        for (var j = 0; j < 300; j++) { sum += w * BetaCdf(a + j, b, x); w *= half / (j + 1.0); }
        return Math.Min(1.0, sum);
    }

    public static double NcBetaSf(double a, double b, double nc, double x) => 1.0 - NcBetaCdf(a, b, nc, x);

    public static double NcGammaPdf(double shape, double nc, double x)
    {
        if (shape <= 0 || nc < 0) return double.NaN;
        double half = nc * 0.5, w = Math.Exp(-half), sum = 0;
        for (var j = 0; j < 300; j++) { sum += w * GammaPdf(shape + j, 1.0, x); w *= half / (j + 1.0); }
        return sum;
    }

    public static double NcGammaCdf(double shape, double nc, double x)
    {
        if (shape <= 0 || nc < 0) return double.NaN;
        if (x <= 0) return 0;
        double half = nc * 0.5, w = Math.Exp(-half), sum = 0;
        for (var j = 0; j < 300; j++) { sum += w * GammaCdf(shape + j, 1.0, x); w *= half / (j + 1.0); }
        return Math.Min(1.0, sum);
    }

    public static double NcGammaSf(double shape, double nc, double x) => 1.0 - NcGammaCdf(shape, nc, x);
    public static double NcChi2Pdf(double df, double nc, double x)
    {
        if (df <= 0 || nc < 0 || x < 0) return double.NaN;
        double half = nc * 0.5, w = Math.Exp(-half), sum = 0;
        for (var j = 0; j < 300; j++)
        {
            sum += w * ChiSquaredPdf(df + 2.0 * j, x);
            w *= half / (j + 1.0);
        }
        return sum;
    }

    public static double NcChi2Cdf(double df, double nc, double x)
    {
        if (df <= 0 || nc < 0) return double.NaN;
        if (x <= 0) return 0;
        double half = nc * 0.5, w = Math.Exp(-half), sum = 0;
        for (var j = 0; j < 300; j++)
        {
            sum += w * ChiSquaredCdf(df + 2.0 * j, x);
            w *= half / (j + 1.0);
        }
        return Math.Min(1.0, sum);
    }

    public static double NcChi2Sf(double df, double nc, double x) => 1.0 - NcChi2Cdf(df, nc, x);

    public static double NcFPdf(double d1, double d2, double nc, double x)
    {
        if (d1 <= 0 || d2 <= 0 || nc < 0) return double.NaN;
        if (x <= 0) return 0;
        var y = d1 * x / (d1 * x + d2);
        var dydx = d1 * d2 / ((d1 * x + d2) * (d1 * x + d2));
        double half = nc * 0.5, w = Math.Exp(-half), sum = 0;
        for (var j = 0; j < 400; j++)
        {
            sum += w * BetaPdf(d1 * 0.5 + j, d2 * 0.5, y);
            if (j > half && w < 1e-18) break;
            w *= half / (j + 1.0);
        }
        return sum * dydx;
    }

    public static double NcFCdf(double d1, double d2, double nc, double x)
    {
        if (d1 <= 0 || d2 <= 0 || nc < 0) return double.NaN;
        if (x <= 0) return 0;
        var y = d1 * x / (d1 * x + d2);
        return NcBetaCdf(d1 * 0.5, d2 * 0.5, nc, y);
    }

    public static double NcFSf(double d1, double d2, double nc, double x) => 1.0 - NcFCdf(d1, d2, nc, x);

    private static double NcTTilde(double df, double mu, double x)
    {
        var phi = 0.5 * SpecialErfc(mu / Math.Sqrt(2.0));
        var half = 0.5 * mu * mu;
        var y = (x * x) / (x * x + df);
        var powr = Math.Exp(-half);
        double sum = 0.0;
        for (var j = 0; j < 400; j++)
        {
            var pj = powr / Math.Exp(SpecialLGamma(j + 1.0));
            var qj = mu / (Math.Sqrt(2.0) * Math.Exp(SpecialLGamma(j + 1.5))) * powr;
            sum += pj * SpecialBetaInc(j + 0.5, df / 2.0, y)
                 + qj * SpecialBetaInc(j + 1.0, df / 2.0, y);
            if (j > half && Math.Abs(pj) + Math.Abs(qj) < 1e-18) break;
            powr *= half;
        }
        return phi + 0.5 * sum;
    }

    public static double NcTCdf(double df, double nc, double x)
    {
        if (df <= 0) return double.NaN;
        if (nc == 0) return StudentTCdf(df, x);
        if (x < 0) return 1.0 - NcTTilde(df, -nc, -x);
        return NcTTilde(df, nc, x);
    }

    public static double NcTPdf(double df, double nc, double x)
    {
        if (df <= 0) return double.NaN;
        if (nc == 0) return StudentTPdf(df, x);
        if (x == 0)
            return Math.Exp(SpecialLGamma(0.5 * (df + 1.0)) - SpecialLGamma(0.5 * df) - 0.5 * nc * nc)
                   / Math.Sqrt(Math.PI * df);
        return (df / x) * (NcTCdf(df + 2.0, nc, x * Math.Sqrt(1.0 + 2.0 / df)) - NcTCdf(df, nc, x));
    }

    public static double NcTSf(double df, double nc, double x) => 1.0 - NcTCdf(df, nc, x);


    private static double NcBisect(Func<double, double> cdf, double p, double lo, double hi)
    {
        if (!(p > 0.0 && p < 1.0)) return double.NaN;
        for (var i = 0; i < 200; i++)
        {
            var mid = 0.5 * (lo + hi);
            if (cdf(mid) < p) lo = mid; else hi = mid;
        }
        return 0.5 * (lo + hi);
    }

    private static double NcPpfPos(Func<double, double> cdf, double p)
    {
        double lo = 0.0, hi = 1.0;
        if (!(p > 0.0 && p < 1.0)) return double.NaN;
        while (cdf(hi) < p && hi < 1e12) hi *= 2.0;
        return NcBisect(cdf, p, lo, hi);
    }

    private static double NcPpfT(Func<double, double> cdf, double p)
    {
        double lo = -1.0, hi = 1.0;
        if (!(p > 0.0 && p < 1.0)) return double.NaN;
        while (cdf(lo) > p && lo > -1e9) lo *= 2.0;
        while (cdf(hi) < p && hi < 1e9) hi *= 2.0;
        return NcBisect(cdf, p, lo, hi);
    }

    public static double NcChi2Ppf(double df, double nc, double p) => NcPpfPos(x => NcChi2Cdf(df, nc, x), p);
    public static double NcChi2Isf(double df, double nc, double p) => NcChi2Ppf(df, nc, 1.0 - p);
    public static double NcBetaPpf(double a, double b, double nc, double p) => NcBisect(x => NcBetaCdf(a, b, nc, x), p, 0.0, 1.0);
    public static double NcBetaIsf(double a, double b, double nc, double p) => NcBetaPpf(a, b, nc, 1.0 - p);
    public static double NcGammaPpf(double shape, double nc, double p) => NcPpfPos(x => NcGammaCdf(shape, nc, x), p);
    public static double NcGammaIsf(double shape, double nc, double p) => NcGammaPpf(shape, nc, 1.0 - p);
    public static double NcFPpf(double d1, double d2, double nc, double p) => NcPpfPos(x => NcFCdf(d1, d2, nc, x), p);
    public static double NcFIsf(double d1, double d2, double nc, double p) => NcFPpf(d1, d2, nc, 1.0 - p);
    public static double NcTPpf(double df, double nc, double p) => NcPpfT(x => NcTCdf(df, nc, x), p);
    public static double NcTIsf(double df, double nc, double p) => NcTPpf(df, nc, 1.0 - p);

    public static double ZTPoissonPmf(double lambda, int k)
    {
        if (lambda <= 0) return double.NaN;
        if (k < 1) return 0.0;
        var denom = lambda < 1e-6 ? lambda * (1.0 - lambda * 0.5) : 1.0 - Math.Exp(-lambda);
        return Math.Exp(k * Math.Log(lambda) - SpecialLGamma(k + 1.0) - lambda - Math.Log(denom));
    }

    public static double ZTPoissonCdf(double lambda, int k)
    {
        if (lambda <= 0) return double.NaN;
        if (k < 1) return 0.0;
        var denom = lambda < 1e-6 ? lambda * (1.0 - lambda * 0.5) : 1.0 - Math.Exp(-lambda);
        var p = lambda * Math.Exp(-lambda) / denom;
        var s = p;
        for (var j = 2; j <= k; j++)
        {
            p *= lambda / j;
            s += p;
            if (p < 1e-15 * s) break;
        }
        return Math.Min(1.0, s);
    }


    // ── Gaussian Process regression (parità gs_gp_regression) ──────────────────────

    public static (double[] Mean, double[] Variance) GpRegress(double[] x, double[] y, double[] xs,
        double l2, double noise)
    {
        var n = x.Length;
        var m = xs.Length;
        if (n < 1 || m < 1 || l2 <= 0.0 || y.Length < n)
            return (new[] { double.NaN }, new[] { double.NaN });
        var K = new double[n * n];
        var alpha = new double[n];
        var kx = new double[n];
        var v = new double[n];
        var sol = new double[n];
        var A = new double[n * n];
        var mean = new double[m];
        var varArr = new double[m];
        int i, j, q;
        for (i = 0; i < n; i++)
            for (j = 0; j < n; j++)
            {
                var dd = x[i] - x[j];
                K[i * n + j] = Math.Exp(-0.5 * dd * dd / l2) + (i == j ? noise : 0.0);
            }
        for (i = 0; i < n; i++) alpha[i] = y[i];
        Array.Copy(K, A, n * n);
        if (!InferSolve(n, A, alpha, v))
            return (new[] { double.NaN }, new[] { double.NaN });
        Array.Copy(v, alpha, n);
        for (q = 0; q < m; q++)
        {
            double mu = 0.0, kv;
            for (i = 0; i < n; i++) { var dd = xs[q] - x[i]; kx[i] = Math.Exp(-0.5 * dd * dd / l2); mu += kx[i] * alpha[i]; }
            for (i = 0; i < n; i++) v[i] = kx[i];
            Array.Copy(K, A, n * n);
            if (!InferSolve(n, A, v, sol)) { mean[q] = mu; varArr[q] = noise; continue; }
            kv = 1.0 + noise;
            for (i = 0; i < n; i++) kv -= kx[i] * sol[i];
            mean[q] = mu;
            varArr[q] = kv > 0.0 ? kv : 0.0;
        }
        return (mean, varArr);
    }

    // ── SVM multiclasse (parità gs_svm_multiclass) ──────────────────────────────────

    public static (double[] W, int Classes, int Features) SvmFit(double[][] X, double[] y,
        int k, double C, int maxIter)
    {
        var n = X.Length;
        var d = n > 0 ? X[0].Length : 0;
        if (n < 1 || d < 1 || k < 2 || C <= 0.0 || maxIter < 1)
            return (Array.Empty<double>(), k, d);
        var W = new double[k * d];
        var eta = C / n;
        var changed = true;
        for (var iter = 0; iter < maxIter && changed; iter++)
        {
            changed = false;
            for (var i = 0; i < n; i++)
            {
                var yi = (int)y[i];
                for (var c = 0; c < k; c++)
                {
                    var lbl = c == yi ? 1 : -1;
                    double margin = 0.0;
                    for (var j = 0; j < d; j++) margin += W[c * d + j] * X[i][j];
                    if (lbl * margin < 1.0)
                    {
                        for (var j = 0; j < d; j++) W[c * d + j] += eta * lbl * X[i][j];
                        changed = true;
                    }
                }
            }
        }
        return (W, k, d);
    }

    public static int SvmPredict(double[] W, double[] x, int d, int k)
    {
        var best = 0;
        double bs = -1e300;
        for (var c = 0; c < k; c++)
        {
            double s = 0.0;
            for (var j = 0; j < d; j++) s += W[c * d + j] * x[j];
            if (s > bs) { bs = s; best = c; }
        }
        return best;
    }

    // ── Random Matrix Theory (parità gs_spectral.c) ─────────────────────────────────

    private static void RmtJacobi(int n, double[] A, double[] eval, double[] evec)
    {
        int i, j, k, sweep;
        Array.Clear(evec);
        for (i = 0; i < n; i++) evec[i * n + i] = 1.0;
        for (sweep = 0; sweep < 100; sweep++)
        {
            double off = 0.0, diag = 0.0;
            for (i = 0; i < n; i++)
            {
                diag += A[i * n + i] * A[i * n + i];
                for (j = i + 1; j < n; j++) off += A[i * n + j] * A[i * n + j];
            }
            if (off <= 1e-30 * (diag + 1e-300)) break;
            for (i = 0; i < n; i++)
                for (j = i + 1; j < n; j++)
                {
                    var apq = A[i * n + j];
                    if (Math.Abs(apq) < 1e-300) continue;
                    var app = A[i * n + i];
                    var aqq = A[j * n + j];
                    var tau = (aqq - app) / (2.0 * apq);
                    var t = (tau >= 0 ? 1.0 : -1.0) / (Math.Abs(tau) + Math.Sqrt(1.0 + tau * tau));
                    var c = 1.0 / Math.Sqrt(1.0 + t * t);
                    var sn = t * c;
                    for (k = 0; k < n; k++)
                    {
                        var aik = A[i * n + k];
                        var ajk = A[j * n + k];
                        A[i * n + k] = c * aik - sn * ajk;
                        A[j * n + k] = sn * aik + c * ajk;
                    }
                    for (k = 0; k < n; k++)
                    {
                        var aki = A[k * n + i];
                        var akj = A[k * n + j];
                        A[k * n + i] = c * aki - sn * akj;
                        A[k * n + j] = sn * aki + c * akj;
                    }
                    for (k = 0; k < n; k++)
                    {
                        var vik = evec[k * n + i];
                        var vjk = evec[k * n + j];
                        evec[k * n + i] = c * vik - sn * vjk;
                        evec[k * n + j] = sn * vik + c * vjk;
                    }
                    A[i * n + j] = 0.0; A[j * n + i] = 0.0;
                }
        }
        for (i = 0; i < n; i++) eval[i] = A[i * n + i];
    }

    public static double RmtLambdaPlus(int n, int T, double sigma2)
    {
        if (n < 1 || T < 1 || sigma2 < 0.0) return double.NaN;
        var r = Math.Sqrt((double)n / T);
        return sigma2 * (1.0 + r) * (1.0 + r);
    }

    public static double RmtLambdaMinus(int n, int T, double sigma2)
    {
        if (n < 1 || T < 1 || sigma2 < 0.0) return double.NaN;
        var r = Math.Sqrt((double)n / T);
        return sigma2 * (1.0 - r) * (1.0 - r);
    }

    public static double RmtCoherence(double[] v)
    {
        var n = v.Length;
        if (n < 1) return double.NaN;
        double mx = 0.0;
        for (var i = 0; i < n; i++) { var a = Math.Abs(v[i]); if (a > mx) mx = a; }
        return n * mx * mx;
    }

    public static double RmtSpectralNorm(double[] A, int n)
    {
        if (n < 1 || A.Length < n * n) return double.NaN;
        var W = (double[])A.Clone();
        var eval = new double[n];
        var evec = new double[n * n];
        RmtJacobi(n, W, eval, evec);
        double mx = 0.0;
        for (var i = 0; i < n; i++) { var a = Math.Abs(eval[i]); if (a > mx) mx = a; }
        return mx;
    }

    public static double RmtDavisKahan(double eNorm, double gap)
        => gap <= 0.0 ? double.NaN : Math.Abs(eNorm) / gap;

    public static double RmtPrincipalAngle(double[] u, double[] v)
    {
        var n = Math.Min(u.Length, v.Length);
        if (n < 1) return double.NaN;
        double nu = 0.0, nv = 0.0, dot = 0.0;
        for (var i = 0; i < n; i++) { nu += u[i] * u[i]; nv += v[i] * v[i]; dot += u[i] * v[i]; }
        if (nu <= 0.0 || nv <= 0.0) return double.NaN;
        var c = Math.Abs(dot) / Math.Sqrt(nu * nv);
        if (c > 1.0) c = 1.0;
        return Math.Acos(c);
    }

    public static double RmtTightness(double observed, double bound)
        => bound <= 0.0 ? 0.0 : Math.Abs(observed) / bound;

    private static bool MvPrepare(double[] x, double[] loc, double[] cov, out double halfLogdet, out double q)
    {
        halfLogdet = 0; q = 0;
        var d = x.Length;
        if (d <= 0 || loc.Length < d || cov.Length < d * d) return false;
        var L = new double[d, d];
        var y = new double[d];
        for (var i = 0; i < d; i++)
            for (var j = 0; j <= i; j++)
            {
                var s = cov[i * d + j];
                for (var k = 0; k < j; k++) s -= L[i, k] * L[j, k];
                if (i == j) { if (s <= 0) return false; L[i, j] = Math.Sqrt(s); }
                else L[i, j] = s / L[j, j];
            }
        for (var i = 0; i < d; i++) halfLogdet += Math.Log(L[i, i]);
        for (var i = 0; i < d; i++)
        {
            var s = x[i] - loc[i];
            for (var j = 0; j < i; j++) s -= L[i, j] * y[j];
            y[i] = s / L[i, i];
        }
        for (var i = 0; i < d; i++) q += y[i] * y[i];
        return true;
    }

    public static double MvGaussLogPdf(double[] x, double[] loc, double[] cov)
    {
        if (!MvPrepare(x, loc, cov, out var hl, out var q)) return double.NaN;
        return -0.5 * q - hl - 0.5 * x.Length * Math.Log(2.0 * Math.PI);
    }

    public static double MvGaussPdf(double[] x, double[] loc, double[] cov) => Math.Exp(MvGaussLogPdf(x, loc, cov));

    public static double MvStudentPdf(double[] x, double[] loc, double[] cov, double nu)
    {
        if (nu <= 0) return double.NaN;
        if (double.IsInfinity(nu)) return MvGaussPdf(x, loc, cov);
        if (!MvPrepare(x, loc, cov, out var hl, out var q)) return double.NaN;
        var d = x.Length;
        var lognorm = SpecialLGamma(0.5 * (nu + d)) - SpecialLGamma(0.5 * nu) - 0.5 * d * Math.Log(nu * Math.PI);
        return Math.Exp(lognorm - hl - 0.5 * (nu + d) * Math.Log(1.0 + q / nu));
    }

    public static double CopulaClaytonCdf(double u, double v, double theta)
    {
        if (theta <= 0 || u <= 0 || u >= 1 || v <= 0 || v >= 1) return double.NaN;
        var x = Math.Pow(u, -theta) + Math.Pow(v, -theta) - 1.0;
        return x > 0 ? Math.Pow(x, -1.0 / theta) : 0.0;
    }

    public static double CopulaGumbelCdf(double u, double v, double theta)
    {
        if (theta < 1 || u <= 0 || u >= 1 || v <= 0 || v >= 1) return double.NaN;
        var a = Math.Pow(-Math.Log(u), theta);
        var b = Math.Pow(-Math.Log(v), theta);
        return Math.Exp(-Math.Pow(a + b, 1.0 / theta));
    }

    public static double CopulaFrankCdf(double u, double v, double theta)
    {
        if (theta <= 0 || u <= 0 || u >= 1 || v <= 0 || v >= 1) return double.NaN;
        var a = Math.Exp(-theta * u);
        var b = Math.Exp(-theta * v);
        var e = Math.Exp(-theta);
        return -Math.Log(1.0 + (a - 1.0) * (b - 1.0) / (e - 1.0)) / theta;
    }

    /// <summary>
    /// Calcola la funzione di densità di probabilità (PDF) della distribuzione Beta.
    /// </summary>
    /// <param name="a">Primo parametro di forma (α).</param>
    /// <param name="b">Secondo parametro di forma (β).</param>
    /// <param name="x">Punto in cui valutare la PDF (tra 0 e 1).</param>
    /// <returns>La densità di probabilità, NaN se a ≤ 0 o b ≤ 0, 0 se x fuori da [0,1].</returns>
    /// <example>double pdf = Numerics.BetaPdf(2, 5, 0.5);</example>
    public static double BetaPdf(double a, double b, double x)
    {
        if (a <= 0 || b <= 0) return double.NaN;
        if (x is < 0 or > 1) return 0.0;
        return Beta.PDF(a, b, x);
    }

    /// <summary>
    /// Calcola la funzione di ripartizione (CDF) della distribuzione Beta.
    /// </summary>
    /// <param name="a">Primo parametro di forma (α).</param>
    /// <param name="b">Secondo parametro di forma (β).</param>
    /// <param name="x">Punto in cui valutare la CDF (tra 0 e 1).</param>
    /// <returns>La probabilità cumulativa P(X ≤ x), NaN se a ≤ 0 o b ≤ 0.</returns>
    /// <example>double cdf = Numerics.BetaCdf(2, 5, 0.5);</example>
    public static double BetaCdf(double a, double b, double x)
    {
        if (a <= 0 || b <= 0) return double.NaN;
        if (x <= 0) return 0.0;
        if (x >= 1) return 1.0;
        return Beta.CDF(a, b, x);
    }
    /// <summary>Funzione di sopravvivenza Beta (1 - CDF).</summary>
    public static double BetaSf(double a, double b, double x) => 1.0 - BetaCdf(a, b, x);


    /// <summary>
    /// Calcola la funzione di densità di probabilità (PDF) della distribuzione t di Student.
    /// </summary>
    /// <param name="freedom">Gradi di libertà.</param>
    /// <param name="x">Punto in cui valutare la PDF.</param>
    /// <returns>Il valore della densità di probabilità.</returns>
    /// <exception cref="ArgumentOutOfRangeException">Se freedom ≤ 0.</exception>
    /// <example>double pdf = Numerics.StudentTPdf(5, 1.0);</example>
    public static double StudentTPdf(double freedom, double x)
    {
        if (freedom <= 0) throw new ArgumentOutOfRangeException(nameof(freedom));
        return StudentT.PDF(0, 1, freedom, x);
    }

    /// <summary>
    /// Calcola la funzione di ripartizione (CDF) della distribuzione t di Student.
    /// </summary>
    /// <param name="freedom">Gradi di libertà.</param>
    /// <param name="x">Punto in cui valutare la CDF.</param>
    /// <returns>La probabilità cumulativa P(X ≤ x).</returns>
    /// <exception cref="ArgumentOutOfRangeException">Se freedom ≤ 0.</exception>
    /// <example>double cdf = Numerics.StudentTCdf(5, 1.0);</example>
    public static double StudentTCdf(double freedom, double x)
    {
        if (freedom <= 0) throw new ArgumentOutOfRangeException(nameof(freedom));
        return StudentT.CDF(0, 1, freedom, x);
    }

    /// <summary>
    /// Calcola la funzione di densità di probabilità (PDF) della distribuzione di Weibull.
    /// </summary>
    /// <param name="shape">Parametro di forma.</param>
    /// <param name="scale">Parametro di scala.</param>
    /// <param name="x">Punto in cui valutare la PDF.</param>
    /// <returns>La densità di probabilità, NaN se shape ≤ 0 o scale ≤ 0, 0 se x &lt; 0.</returns>
    /// <example>double pdf = Numerics.WeibullPdf(1.5, 1.0, 0.5);</example>
    public static double WeibullPdf(double shape, double scale, double x)
    {
        if (shape <= 0 || scale <= 0) return double.NaN;
        return x < 0 ? 0.0 : Weibull.PDF(shape, scale, x);
    }

    /// <summary>
    /// Calcola la funzione di ripartizione (CDF) della distribuzione di Weibull.
    /// </summary>
    /// <param name="shape">Parametro di forma.</param>
    /// <param name="scale">Parametro di scala.</param>
    /// <param name="x">Punto in cui valutare la CDF.</param>
    /// <returns>La probabilità cumulativa, NaN se shape ≤ 0 o scale ≤ 0, 0 se x &lt; 0.</returns>
    /// <example>double cdf = Numerics.WeibullCdf(1.5, 1.0, 0.5);</example>
    public static double WeibullCdf(double shape, double scale, double x)
    {
        if (shape <= 0 || scale <= 0) return double.NaN;
        return x < 0 ? 0.0 : Weibull.CDF(shape, scale, x);
    }

    /// <summary>
    /// Calcola la funzione di densità di probabilità (PDF) della distribuzione F di Fisher-Snedecor.
    /// </summary>
    /// <param name="d1">Gradi di libertà del numeratore.</param>
    /// <param name="d2">Gradi di libertà del denominatore.</param>
    /// <param name="x">Punto in cui valutare la PDF.</param>
    /// <returns>La densità di probabilità, NaN se d1 ≤ 0 o d2 ≤ 0, 0 se x &lt; 0.</returns>
    /// <example>double pdf = Numerics.FPdf(5, 10, 2.0);</example>
    public static double FPdf(double d1, double d2, double x)
    {
        if (d1 <= 0 || d2 <= 0) return double.NaN;
        return x < 0 ? 0.0 : FisherSnedecor.PDF(d1, d2, x);
    }

    /// <summary>
    /// Calcola la funzione di ripartizione (CDF) della distribuzione F di Fisher-Snedecor.
    /// </summary>
    /// <param name="d1">Gradi di libertà del numeratore.</param>
    /// <param name="d2">Gradi di libertà del denominatore.</param>
    /// <param name="x">Punto in cui valutare la CDF.</param>
    /// <returns>La probabilità cumulativa, NaN se d1 ≤ 0 o d2 ≤ 0, 0 se x &lt; 0.</returns>
    /// <example>double cdf = Numerics.FCdf(5, 10, 2.0);</example>
    public static double FCdf(double d1, double d2, double x)
    {
        if (d1 <= 0 || d2 <= 0) return double.NaN;
        return x < 0 ? 0.0 : FisherSnedecor.CDF(d1, d2, x);
    }

    /// <summary>
    /// Calcola la funzione di densità di probabilità (PDF) della distribuzione esponenziale.
    /// </summary>
    /// <param name="rate">Tasso (λ).</param>
    /// <param name="x">Punto in cui valutare la PDF.</param>
    /// <returns>La densità di probabilità, oppure 0 se rate ≤ 0 o x &lt; 0.</returns>
    /// <example>double pdf = Numerics.ExponentialPdf(1.0, 0.5);</example>
    public static double ExponentialPdf(double rate, double x)
    {
        if (rate <= 0 || x < 0) return 0.0;
        return Exponential.PDF(rate, x);
    }

    /// <summary>
    /// Calcola la funzione di ripartizione (CDF) della distribuzione esponenziale.
    /// </summary>
    /// <param name="rate">Tasso (λ).</param>
    /// <param name="x">Punto in cui valutare la CDF.</param>
    /// <returns>La probabilità cumulativa P(X ≤ x), oppure 0 se x &lt; 0.</returns>
    /// <exception cref="ArgumentOutOfRangeException">Se rate ≤ 0.</exception>
    /// <example>double cdf = Numerics.ExponentialCdf(1.0, 0.5);</example>
    public static double ExponentialCdf(double rate, double x)
    {
        if (rate <= 0) throw new ArgumentOutOfRangeException(nameof(rate));
        return x < 0 ? 0.0 : Exponential.CDF(rate, x);
    }

    /// <summary>
    /// Calcola la funzione di densità di probabilità (PDF) della distribuzione di Pareto.
    /// </summary>
    /// <param name="scale">Parametro di scala (xm).</param>
    /// <param name="shape">Parametro di forma (α).</param>
    /// <param name="x">Punto in cui valutare la PDF.</param>
    /// <returns>La densità di probabilità, oppure 0 se parametri non validi o x &lt; scale.</returns>
    /// <example>double pdf = Numerics.ParetoPdf(1.0, 2.0, 2.0);</example>
    public static double ParetoCdf(double scale, double shape, double x)
    {
        if (scale <= 0 || shape <= 0) return double.NaN;
        if (x <= scale) return 0.0;
        return 1.0 - Math.Pow(scale / x, shape);
    }

    public static double ParetoPdf(double scale, double shape, double x)
    {
        if (scale <= 0 || shape <= 0) return 0.0;
        return x < scale ? 0.0 : Pareto.PDF(scale, shape, x);
    }

    /// <summary>
    /// Calcola la funzione di densità di probabilità (PDF) della distribuzione logistica.
    /// </summary>
    /// <param name="mean">Media della distribuzione.</param>
    /// <param name="scale">Parametro di scala.</param>
    /// <param name="x">Punto in cui valutare la PDF.</param>
    /// <returns>La densità di probabilità.</returns>
    /// <exception cref="ArgumentOutOfRangeException">Se scale ≤ 0.</exception>
    /// <example>double pdf = Numerics.LogisticPdf(0, 1, 0.5);</example>
    public static double LogisticCdf(double mu, double s, double x)
    {
        if (s <= 0) return double.NaN;
        return 1.0 / (1.0 + Math.Exp(-(x - mu) / s));
    }

    public static double LogisticSf(double mu, double s, double x) => 1.0 - LogisticCdf(mu, s, x);

    public static double LogisticPdf(double mean, double scale, double x)
    {
        if (scale <= 0) throw new ArgumentOutOfRangeException(nameof(scale));
        return Logistic.PDF(mean, scale, x);
    }

    /// <summary>
    /// Calcola la funzione di densità di probabilità (PDF) della distribuzione triangolare.
    /// </summary>
    /// <param name="min">Valore minimo.</param>
    /// <param name="mode">Valore modale (picco).</param>
    /// <param name="max">Valore massimo.</param>
    /// <param name="x">Punto in cui valutare la PDF.</param>
    /// <returns>La densità di probabilità.</returns>
    /// <example>double pdf = Numerics.TriangularPdf(0, 5, 10, 5);</example>
    public static double TriangularCdf(double min, double mode, double max, double x)
    {
        if (max <= min || mode < min || mode > max) return double.NaN;
        if (x <= min) return 0.0;
        if (x >= max) return 1.0;
        if (x <= mode) return (x - min) * (x - min) / ((max - min) * (mode - min));
        return 1.0 - (max - x) * (max - x) / ((max - min) * (max - mode));
    }

    public static double TriangularSf(double min, double mode, double max, double x) => 1.0 - TriangularCdf(min, mode, max, x);

    public static double TriangularPdf(double min, double mode, double max, double x)
    {
        // MathNet.Numerics.Distributions.Triangular.PDF ha firma (lower, upper, mode, x) — "mode"
        // è il TERZO parametro, non il secondo. Chiamarlo con (min, mode, max, x) come se
        // corrispondesse posizionalmente scambia mode/max, producendo quasi sempre una
        // parametrizzazione invalida (mode finisce nello slot "upper", spesso > del vero upper
        // nello slot "mode" — MathNet lancia ArgumentException "Invalid parametrization"). Bug
        // scoperto migrando IlBackend a StdLib.Runtime (Fase 9, docs/StdLib-Runtime-Plan.md):
        // anche l'esempio nel doc-comment sopra (TriangularPdf(0, 5, 10, 5)) lanciava con il
        // codice precedente — mai stato eseguito/verificato. Stesso fix in GMBasicAI (stesso
        // codice condiviso).
        return Triangular.PDF(min, max, mode, x);
    }

    /// <summary>
    /// Calcola la funzione di densità di probabilità (PDF) della distribuzione uniforme continua.
    /// </summary>
    /// <param name="min">Limite inferiore dell'intervallo.</param>
    /// <param name="max">Limite superiore dell'intervallo.</param>
    /// <param name="x">Punto in cui valutare la PDF.</param>
    /// <returns>La densità di probabilità.</returns>
    /// <exception cref="ArgumentException">Se max ≤ min.</exception>
    /// <example>double pdf = Numerics.UniformPdf(0, 1, 0.5); // restituisce 1.0</example>
    public static double UniformPdf(double min, double max, double x)
    {
        if (max <= min) throw new ArgumentException("Il valore massimo deve essere maggiore del minimo.");
        return ContinuousUniform.PDF(min, max, x);
    }

    /// <summary>
    /// Calcola la funzione di densità di probabilità (PDF) della distribuzione normale asimmetrica (skew-normal).
    /// </summary>
    /// <param name="mean">Media della distribuzione.</param>
    /// <param name="stdDev">Deviazione standard.</param>
    /// <param name="skew">Parametro di asimmetria (α).</param>
    /// <param name="x">Punto in cui valutare la PDF.</param>
    /// <returns>La densità di probabilità.</returns>
    /// <exception cref="ArgumentOutOfRangeException">Se stdDev ≤ 0.</exception>
    /// <example>double pdf = Numerics.SkewedNormalPdf(0, 1, 2, 0.5);</example>
    public static double SkewedNormalPdf(double mean, double stdDev, double skew, double x)
    {
        if (stdDev <= 0) throw new ArgumentOutOfRangeException(nameof(stdDev));
        var y = (x - mean) / stdDev;
        var pdfNormalStandard = Normal.PDF(0, 1, y);
        var cdfNormalStandard = Normal.CDF(0, 1, skew * y);
        return 2 * pdfNormalStandard * cdfNormalStandard;
    }

    /// <summary>
    /// Calcola la funzione di massa di probabilità (PMF) della distribuzione di Bernoulli.
    /// </summary>
    /// <param name="p">Probabilità di successo (tra 0 e 1).</param>
    /// <param name="k">Valore (0 o 1).</param>
    /// <returns>La probabilità P(X = k).</returns>
    /// <exception cref="ArgumentOutOfRangeException">Se p è fuori dall'intervallo [0, 1].</exception>
    /// <example>double pmf = Numerics.BernoulliPmf(0.5, 1); // restituisce 0.5</example>
    public static double BernoulliPmf(double p, int k)
    {
        if (p is < 0 or > 1) throw new ArgumentOutOfRangeException(nameof(p));
        return Bernoulli.PMF(p, k);
    }

    /// <summary>
    /// Calcola la funzione di massa di probabilità (PMF) della distribuzione binomiale.
    /// </summary>
    /// <param name="p">Probabilità di successo (tra 0 e 1).</param>
    /// <param name="n">Numero di prove.</param>
    /// <param name="k">Numero di successi.</param>
    /// <returns>La probabilità P(X = k).</returns>
    /// <exception cref="ArgumentException">Se i parametri non sono validi.</exception>
    /// <example>double pmf = Numerics.BinomialPmf(0.5, 10, 5); // probabilità di 5 successi su 10</example>
    public static double BinomialPmf(double p, int n, int k)
    {
        if (p < 0 || p > 1 || n < 0) throw new ArgumentException("Parametri non validi.");
        return Binomial.PMF(p, n, k);
    }

    /// <summary>
    /// Calcola la funzione di massa di probabilità (PMF) della distribuzione di Poisson.
    /// </summary>
    /// <param name="lambda">Tasso medio (λ).</param>
    /// <param name="k">Numero di eventi.</param>
    /// <returns>La probabilità P(X = k), oppure 0 se lambda ≤ 0 o k &lt; 0.</returns>
    /// <example>double pmf = Numerics.PoissonPmf(2.5, 3);</example>
    public static double PoissonPmf(double lambda, int k)
    {
        if (lambda <= 0 || k < 0) return 0.0;
        return Poisson.PMF(lambda, k);
    }

    /// <summary>
    /// Calcola la funzione di massa di probabilità (PMF) della distribuzione geometrica.
    /// </summary>
    /// <param name="p">Probabilità di successo per ogni prova (tra 0 e 1).</param>
    /// <param name="k">Numero di prove fino al primo successo (≥ 1).</param>
    /// <returns>La probabilità P(X = k), oppure 0 se k ≤ 0.</returns>
    /// <exception cref="ArgumentException">Se p è fuori dall'intervallo [0, 1].</exception>
    /// <example>double pmf = Numerics.GeometricPmf(0.5, 3); // probabilità di successo al 3° tentativo</example>
    public static double GeometricPmf(double p, int k)
    {
        if (p is < 0 or > 1) throw new ArgumentException("La probabilità p deve essere tra 0 e 1.");
        return k <= 0 ? 0.0 : Geometric.PMF(p, k);
    }

    /// <summary>
    /// Calcola la funzione di massa di probabilità (PMF) della distribuzione binomiale negativa.
    /// </summary>
    /// <param name="r">Numero di successi desiderati.</param>
    /// <param name="p">Probabilità di successo per ogni prova.</param>
    /// <param name="k">Numero di fallimenti prima di raggiungere r successi.</param>
    /// <returns>La probabilità P(X = k), oppure 0 se k &lt; 0.</returns>
    /// <exception cref="ArgumentException">Se i parametri non sono validi.</exception>
    /// <example>double pmf = Numerics.NegativeBinomialPmf(3, 0.5, 2);</example>
    public static double NegativeBinomialPmf(double r, double p, int k)
    {
        if (r <= 0 || p < 0 || p > 1) throw new ArgumentException("Parametri non validi.");
        // Bug reale pre-esistente: chiamava NegativeBinomial.PMF(p, r, k) — invertiti rispetto
        // alla firma di MathNet stessa (PMF(r, p, k), verificato dallo stack trace di
        // un'eccezione "Invalid parametrization" con p=5 fuori da [0,1] perche' in realta' era r
        // locale=5 passato al parametro p di MathNet). Con la validazione sopra (p in [0,1], r>0)
        // il caso r>1 e p valido falliva SEMPRE dentro MathNet con un messaggio fuorviante,
        // mascherando che i due argomenti erano scambiati. Scoperto scrivendo il primo vero
        // programma di verifica per StdLib.Runtime.MathFunctions.NegativeBinomialPmf.
        return k < 0 ? 0.0 : NegativeBinomial.PMF(r, p, k);
    }

    /// <summary>
    /// Calcola la funzione di massa di probabilità (PMF) della distribuzione ipergeometrica.
    /// </summary>
    /// <param name="N">Dimensione della popolazione.</param>
    /// <param name="K">Numero di successi nella popolazione.</param>
    /// <param name="n">Numero di estrazioni.</param>
    /// <param name="k">Numero di successi osservati.</param>
    /// <returns>La probabilità P(X = k).</returns>
    /// <example>double pmf = Numerics.HypergeometricPmf(50, 10, 5, 2);</example>
    public static double HypergeometricPmf(int N, int K, int n, int k)
    {
        return Hypergeometric.PMF(N, K, n, k);
    }
}
