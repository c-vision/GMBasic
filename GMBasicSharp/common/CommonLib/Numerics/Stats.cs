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
