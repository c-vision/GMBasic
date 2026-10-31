// FILE: CommonLib.Numerics.HypothesisTests.cs

using MathNet.Numerics;
using MathNet.Numerics.Distributions;

namespace CommonLib;

/// <summary>
/// Rappresenta il risultato numerico di un test d'ipotesi compatibile con SciPy.
/// </summary>
/// <param name="Statistic">Statistica calcolata dal test.</param>
/// <param name="PValue">Probabilità a due code associata alla statistica.</param>
/// <param name="DegreesOfFreedom">Gradi di libertà, oppure <see cref="double.NaN"/> quando non applicabili.</param>
public readonly record struct StatisticalTestResult(
    double Statistic,
    double PValue,
    double DegreesOfFreedom);

public static partial class Numerics
{
    /// <summary>
    /// Esegue il t-test indipendente a varianza combinata usato da scipy.stats.ttest_ind con equal_var=True.
    /// </summary>
    /// <param name="first">Primo campione, con almeno due osservazioni finite.</param>
    /// <param name="second">Secondo campione, con almeno due osservazioni finite.</param>
    /// <returns>Statistica t, p-value a due code e gradi di libertà combinati.</returns>
    /// <exception cref="ArgumentNullException">Sollevata se un campione è null.</exception>
    /// <exception cref="ArgumentException">Sollevata se un campione ha meno di due osservazioni.</exception>
    public static StatisticalTestResult TTestIndependent(double[] first, double[] second)
    {
        ValidateSample(first, nameof(first), 2);
        ValidateSample(second, nameof(second), 2);

        var firstMean = first.Average();
        var secondMean = second.Average();
        var firstVariance = SampleVariance(first, firstMean);
        var secondVariance = SampleVariance(second, secondMean);
        var degreesOfFreedom = first.Length + second.Length - 2d;
        var pooledVariance = ((first.Length - 1d) * firstVariance + (second.Length - 1d) * secondVariance) /
                             degreesOfFreedom;
        var standardError = Math.Sqrt(pooledVariance * (1d / first.Length + 1d / second.Length));
        return StudentResult(firstMean - secondMean, standardError, degreesOfFreedom);
    }

    /// <summary>
    /// Esegue il t-test appaiato usato da scipy.stats.ttest_rel.
    /// </summary>
    /// <param name="first">Primo campione appaiato, con almeno due osservazioni finite.</param>
    /// <param name="second">Secondo campione appaiato della stessa lunghezza.</param>
    /// <returns>Statistica t, p-value a due code e gradi di libertà delle differenze.</returns>
    /// <exception cref="ArgumentNullException">Sollevata se un campione è null.</exception>
    /// <exception cref="ArgumentException">Sollevata se i campioni sono troppo corti o hanno lunghezze diverse.</exception>
    public static StatisticalTestResult TTestPaired(double[] first, double[] second)
    {
        ValidateSample(first, nameof(first), 2);
        ValidateSample(second, nameof(second), 2);
        if (first.Length != second.Length)
            throw new ArgumentException("Il t-test appaiato richiede campioni della stessa lunghezza.", nameof(second));

        var differences = new double[first.Length];
        for (var index = 0; index < differences.Length; index++)
            differences[index] = first[index] - second[index];
        return TTestOneSample(differences, 0d);
    }

    /// <summary>
    /// Esegue il t-test a un campione usato da scipy.stats.ttest_1samp.
    /// </summary>
    /// <param name="sample">Campione con almeno due osservazioni finite.</param>
    /// <param name="populationMean">Media della popolazione sotto l'ipotesi nulla.</param>
    /// <returns>Statistica t, p-value a due code e gradi di libertà del campione.</returns>
    /// <exception cref="ArgumentNullException">Sollevata se il campione è null.</exception>
    /// <exception cref="ArgumentException">Sollevata se il campione ha meno di due osservazioni.</exception>
    public static StatisticalTestResult TTestOneSample(double[] sample, double populationMean)
    {
        ValidateSample(sample, nameof(sample), 2);
        var mean = sample.Average();
        var standardError = Math.Sqrt(SampleVariance(sample, mean) / sample.Length);
        return StudentResult(mean - populationMean, standardError, sample.Length - 1d);
    }

    /// <summary>
    /// Esegue il test chi-quadrato di bontà dell'adattamento usato da scipy.stats.chisquare.
    /// </summary>
    /// <param name="observed">Frequenze osservate non negative.</param>
    /// <param name="expected">Frequenze attese positive, con la stessa somma delle osservate.</param>
    /// <returns>Statistica chi-quadrato, p-value e gradi di libertà.</returns>
    /// <exception cref="ArgumentNullException">Sollevata se un vettore è null.</exception>
    /// <exception cref="ArgumentException">Sollevata per dimensioni, frequenze o somme non valide.</exception>
    public static StatisticalTestResult ChiSquareGoodnessOfFit(double[] observed, double[] expected)
    {
        ValidateSample(observed, nameof(observed), 2);
        ValidateSample(expected, nameof(expected), 2);
        if (observed.Length != expected.Length)
            throw new ArgumentException("CHI2 richiede vettori della stessa lunghezza.", nameof(expected));
        if (Array.Exists(observed, static value => value < 0d) ||
            Array.Exists(expected, static value => value <= 0d))
            throw new ArgumentException("CHI2 richiede osservazioni non negative e attese positive.");

        var observedSum = observed.Sum();
        var expectedSum = expected.Sum();
        var scale = Math.Max(Math.Abs(observedSum), Math.Abs(expectedSum));
        if (Math.Abs(observedSum - expectedSum) > 1e-12 * Math.Max(1d, scale))
            throw new ArgumentException("Le frequenze osservate e attese devono avere la stessa somma.");

        var statistic = 0d;
        for (var index = 0; index < observed.Length; index++)
        {
            var difference = observed[index] - expected[index];
            statistic += difference * difference / expected[index];
        }

        var degreesOfFreedom = observed.Length - 1d;
        return new StatisticalTestResult(
            statistic,
            1d - ChiSquared.CDF(degreesOfFreedom, statistic),
            degreesOfFreedom);
    }

    /// <summary>
    /// Esegue il test esatto di Fisher a due code usato da scipy.stats.fisher_exact per una tabella 2x2.
    /// </summary>
    /// <param name="a">Conteggio nella prima riga e prima colonna.</param>
    /// <param name="b">Conteggio nella prima riga e seconda colonna.</param>
    /// <param name="c">Conteggio nella seconda riga e prima colonna.</param>
    /// <param name="d">Conteggio nella seconda riga e seconda colonna.</param>
    /// <returns>Odds ratio e p-value esatto a due code; i gradi di libertà non sono applicabili.</returns>
    /// <exception cref="ArgumentOutOfRangeException">Sollevata se un conteggio è negativo.</exception>
    public static StatisticalTestResult FisherExact(int a, int b, int c, int d)
    {
        if (a < 0 || b < 0 || c < 0 || d < 0)
            throw new ArgumentOutOfRangeException(nameof(a), "FISHER richiede conteggi non negativi.");

        var firstRow = a + b;
        var secondRow = c + d;
        var firstColumn = a + c;
        var total = firstRow + secondRow;
        if (total == 0)
            return new StatisticalTestResult(double.NaN, 1d, double.NaN);

        var lower = Math.Max(0, firstColumn - secondRow);
        var upper = Math.Min(firstColumn, firstRow);
        var observedProbability = HypergeometricProbability(a, firstRow, firstColumn, total);
        var pValue = 0d;
        for (var value = lower; value <= upper; value++)
        {
            var probability = HypergeometricProbability(value, firstRow, firstColumn, total);
            if (probability <= observedProbability * (1d + 1e-12))
                pValue += probability;
        }

        var oddsRatio = b == 0 || c == 0 ? double.PositiveInfinity : (double)a * d / ((double)b * c);
        return new StatisticalTestResult(oddsRatio, Math.Min(1d, pValue), double.NaN);
    }

    /// <summary>
    /// Esegue uno z-test a un campione con deviazione standard della popolazione nota.
    /// </summary>
    /// <param name="sample">Campione non vuoto con osservazioni finite.</param>
    /// <param name="populationMean">Media della popolazione sotto l'ipotesi nulla.</param>
    /// <param name="populationStandardDeviation">Deviazione standard nota, strettamente positiva.</param>
    /// <returns>Statistica z e p-value normale a due code; i gradi di libertà non sono applicabili.</returns>
    /// <exception cref="ArgumentNullException">Sollevata se il campione è null.</exception>
    /// <exception cref="ArgumentException">Sollevata se il campione è vuoto.</exception>
    /// <exception cref="ArgumentOutOfRangeException">Sollevata se la deviazione standard non è positiva.</exception>
    public static StatisticalTestResult ZTestOneSample(
        double[] sample,
        double populationMean,
        double populationStandardDeviation)
    {
        ValidateSample(sample, nameof(sample), 1);
        if (populationStandardDeviation <= 0d)
            throw new ArgumentOutOfRangeException(
                nameof(populationStandardDeviation),
                "La deviazione standard della popolazione deve essere positiva.");

        var statistic = (sample.Average() - populationMean) /
                        (populationStandardDeviation / Math.Sqrt(sample.Length));
        var pValue = 2d * (1d - Normal.CDF(0d, 1d, Math.Abs(statistic)));
        return new StatisticalTestResult(statistic, pValue, double.NaN);
    }

    /// <summary>
    /// Esegue il test Kolmogorov-Smirnov a due campioni con p-value esatto per campioni piccoli.
    /// </summary>
    /// <param name="first">Primo campione non vuoto.</param>
    /// <param name="second">Secondo campione non vuoto.</param>
    /// <returns>Distanza KS e p-value esatto a due code; i gradi di libertà non sono applicabili.</returns>
    /// <exception cref="ArgumentNullException">Sollevata se un campione è null.</exception>
    /// <exception cref="ArgumentException">Sollevata se un campione è vuoto.</exception>
    public static StatisticalTestResult KolmogorovSmirnovTwoSample(double[] first, double[] second)
    {
        ValidateSample(first, nameof(first), 1);
        ValidateSample(second, nameof(second), 1);
        var sortedFirst = (double[])first.Clone();
        var sortedSecond = (double[])second.Clone();
        Array.Sort(sortedFirst);
        Array.Sort(sortedSecond);
        var firstIndex = 0;
        var secondIndex = 0;
        var statistic = 0d;

        while (firstIndex < sortedFirst.Length || secondIndex < sortedSecond.Length)
        {
            var value = secondIndex >= sortedSecond.Length ||
                        firstIndex < sortedFirst.Length && sortedFirst[firstIndex] <= sortedSecond[secondIndex]
                ? sortedFirst[firstIndex]
                : sortedSecond[secondIndex];
            while (firstIndex < sortedFirst.Length && sortedFirst[firstIndex] <= value) firstIndex++;
            while (secondIndex < sortedSecond.Length && sortedSecond[secondIndex] <= value) secondIndex++;
            statistic = Math.Max(
                statistic,
                Math.Abs((double)firstIndex / sortedFirst.Length - (double)secondIndex / sortedSecond.Length));
        }

        var insideProbability = KolmogorovSmirnovInsideProbability(
            sortedFirst.Length,
            sortedSecond.Length,
            statistic);
        return new StatisticalTestResult(statistic, Math.Clamp(1d - insideProbability, 0d, 1d), double.NaN);
    }

    /// <summary>
    /// Esegue il test di normalità Shapiro-Wilk con l'approssimazione AS R94 usata da SciPy.
    /// </summary>
    /// <param name="sample">Campione con almeno tre e non più di cinquemila osservazioni finite.</param>
    /// <returns>Statistica W e p-value; i gradi di libertà non sono applicabili.</returns>
    /// <exception cref="ArgumentException">Sollevata se la dimensione del campione non è supportata.</exception>
    public static StatisticalTestResult ShapiroWilk(double[] sample)
    {
        ValidateSample(sample, nameof(sample), 3);
        if (sample.Length > 5000)
            throw new ArgumentException("SHAPIRO supporta al massimo 5000 osservazioni.", nameof(sample));

        var sorted = (double[])sample.Clone();
        Array.Sort(sorted);
        if (sorted[0] == sorted[^1])
            return new StatisticalTestResult(1d, 1d, double.NaN);

        var count = sorted.Length;
        var half = count / 2;
        var normalScores = new double[half];
        var scoreSquares = 0d;
        for (var index = 0; index < half; index++)
        {
            var probability = (index + 1d - 0.375d) / (count + 0.25d);
            normalScores[index] = Normal.InvCDF(0d, 1d, probability);
            scoreSquares += normalScores[index] * normalScores[index];
        }
        scoreSquares *= 2d;

        var reciprocalRoot = 1d / Math.Sqrt(count);
        var firstWeight = EvaluatePolynomial(
            [0d, 0.221157d, -0.147981d, -2.07119d, 4.434685d, -2.706056d], reciprocalRoot) -
                          normalScores[0] / Math.Sqrt(scoreSquares);
        var secondWeight = count > 5
            ? EvaluatePolynomial(
                [0d, 0.042981d, -0.293762d, -1.752461d, 5.682633d, -3.582633d], reciprocalRoot) -
              normalScores[1] / Math.Sqrt(scoreSquares)
            : 0d;
        var removedScoreSquares = count > 5
            ? 2d * (normalScores[0] * normalScores[0] + normalScores[1] * normalScores[1])
            : 2d * normalScores[0] * normalScores[0];
        var removedWeightSquares = count > 5
            ? 2d * (firstWeight * firstWeight + secondWeight * secondWeight)
            : 2d * firstWeight * firstWeight;
        var scale = Math.Sqrt((scoreSquares - removedScoreSquares) / (1d - removedWeightSquares));

        var weightedRange = 0d;
        for (var index = 0; index < half; index++)
        {
            var weight = index switch
            {
                0 => firstWeight,
                1 when count > 5 => secondWeight,
                _ => -normalScores[index] / scale
            };
            weightedRange += weight * (sorted[^(index + 1)] - sorted[index]);
        }

        var mean = sorted.Average();
        var totalSquares = sorted.Sum(value => (value - mean) * (value - mean));
        var statistic = Math.Clamp(weightedRange * weightedRange / totalSquares, 0d, 1d);
        if (count == 3)
        {
            var pValue = 6d / Math.PI *
                         (Math.Asin(Math.Sqrt(statistic)) - Math.Asin(Math.Sqrt(0.75d)));
            return new StatisticalTestResult(statistic, Math.Clamp(pValue, 0d, 1d), double.NaN);
        }

        var transformed = Math.Log(1d - statistic);
        double location;
        double scaleParameter;
        if (count <= 11)
        {
            var gamma = EvaluatePolynomial([-2.273d, 0.459d], count);
            if (transformed >= gamma)
                return new StatisticalTestResult(statistic, double.Epsilon, double.NaN);
            transformed = -Math.Log(gamma - transformed);
            location = EvaluatePolynomial([0.544d, -0.39978d, 0.025054d, -0.0006714d], count);
            scaleParameter = Math.Exp(
                EvaluatePolynomial([1.3822d, -0.77857d, 0.062767d, -0.0020322d], count));
        }
        else
        {
            var logarithm = Math.Log(count);
            location = EvaluatePolynomial([-1.5861d, -0.31082d, -0.083751d, 0.0038915d], logarithm);
            scaleParameter = Math.Exp(EvaluatePolynomial([-0.4803d, -0.082676d, 0.0030302d], logarithm));
        }

        var zScore = (transformed - location) / scaleParameter;
        return new StatisticalTestResult(statistic, 1d - Normal.CDF(0d, 1d, zScore), double.NaN);
    }

    /// <summary>
    /// Esegue il test omnibus di normalità D'Agostino-Pearson usato da SciPy.
    /// </summary>
    /// <param name="sample">Campione con almeno venti osservazioni finite.</param>
    /// <returns>Statistica K quadrato e p-value chi-quadrato; i gradi di libertà valgono due.</returns>
    /// <exception cref="ArgumentException">Sollevata se il campione contiene meno di venti osservazioni.</exception>
    public static StatisticalTestResult NormalityTest(double[] sample)
    {
        ValidateSample(sample, nameof(sample), 20);
        var skewZ = SkewTestZ(sample);
        var kurtosisZ = KurtosisTestZ(sample);
        var statistic = skewZ * skewZ + kurtosisZ * kurtosisZ;
        return new StatisticalTestResult(statistic, 1d - ChiSquared.CDF(2d, statistic), 2d);
    }

    /// <summary>
    /// Statistica Z del test D'Agostino sull'asimmetria (scipy.stats.skewtest). Condivisa da
    /// <see cref="NormalityTest"/> e dal test SKEWTEST standalone: STESSA formula, MAI due
    /// implementazioni divergenti (una precisa qui, una approssimata altrove — bug reale:
    /// STATS.SKEWTEST usava `skew/sqrt(6/n)`, un'approssimazione grezza che non replicava
    /// scipy).
    /// </summary>
    private static double SkewTestZ(double[] sample)
    {
        var count = (double)sample.Length;
        var mean = sample.Average();
        var secondMoment = sample.Average(value => Math.Pow(value - mean, 2));
        if (secondMoment == 0d)
        {
            // Campione costante (varianza nulla): skewness non definita, ma scipy applica
            // comunque la sostituzione y=1 sotto (skewY sarebbe 0/0 qui) — replica diretta
            // del ramo "campione perfettamente simmetrico" per coerenza, nessun campione
            // reale con varianza nulla arriva qui in pratica (STATS.NORMALTEST già
            // intercetta questo caso separatamente).
            secondMoment = 1d;
        }
        var thirdMoment = sample.Average(value => Math.Pow(value - mean, 3));
        var skewness = thirdMoment / Math.Pow(secondMoment, 1.5d);
        var skewY = skewness * Math.Sqrt((count + 1d) * (count + 3d) / (6d * (count - 2d)));
        // scipy.stats.skewtest: un campione perfettamente simmetrico (skewY == 0) NON dà
        // Z=0 — la trasformata sostituisce y=1 prima dell'asinh (`np.where(y == 0, 1, y)`
        // nel sorgente scipy), verificato dal vivo contro scipy.stats.skewtest/normaltest
        // (z=1..20: scipy dà Z≈1.0403, non 0). Senza questa sostituzione un campione
        // simmetrico risultava "più normale del reale".
        if (skewY == 0d) skewY = 1d;
        var beta2 = 3d * (count * count + 27d * count - 70d) * (count + 1d) * (count + 3d) /
                    ((count - 2d) * (count + 5d) * (count + 7d) * (count + 9d));
        var wSquared = -1d + Math.Sqrt(2d * (beta2 - 1d));
        var delta = 1d / Math.Sqrt(0.5d * Math.Log(wSquared));
        var alpha = Math.Sqrt(2d / (wSquared - 1d));
        return delta * Math.Asinh(skewY / alpha);
    }

    /// <summary>
    /// Statistica Z del test sulla curtosi (scipy.stats.kurtosistest). Condivisa da
    /// <see cref="NormalityTest"/> e dal test KURTOSISTEST standalone (stesso motivo di
    /// <see cref="SkewTestZ"/>: un'unica implementazione precisa).
    /// </summary>
    private static double KurtosisTestZ(double[] sample)
    {
        var count = (double)sample.Length;
        var mean = sample.Average();
        var secondMoment = sample.Average(value => Math.Pow(value - mean, 2));
        if (secondMoment == 0d) return double.NaN;
        var fourthMoment = sample.Average(value => Math.Pow(value - mean, 4));
        var kurtosis = fourthMoment / (secondMoment * secondMoment);
        var expected = 3d * (count - 1d) / (count + 1d);
        var variance = 24d * count * (count - 2d) * (count - 3d) /
                       (Math.Pow(count + 1d, 2d) * (count + 3d) * (count + 5d));
        var standardized = (kurtosis - expected) / Math.Sqrt(variance);
        var rootBeta = 6d * (count * count - 5d * count + 2d) / ((count + 7d) * (count + 9d)) *
                       Math.Sqrt(6d * (count + 3d) * (count + 5d) /
                                 (count * (count - 2d) * (count - 3d)));
        var a = 6d + 8d / rootBeta * (2d / rootBeta + Math.Sqrt(1d + 4d / (rootBeta * rootBeta)));
        var denominator = 1d + standardized * Math.Sqrt(2d / (a - 4d));
        var cubeRoot = Math.CopySign(
            Math.Pow((1d - 2d / a) / Math.Abs(denominator), 1d / 3d), denominator);
        return (1d - 2d / (9d * a) - cubeRoot) / Math.Sqrt(2d / (9d * a));
    }

    /// <summary>
    /// Valuta un polinomio con coefficienti ordinati dal termine costante al grado massimo.
    /// </summary>
    /// <param name="coefficients">Coefficienti del polinomio in ordine crescente.</param>
    /// <param name="value">Valore nel quale valutare il polinomio.</param>
    /// <returns>Valore del polinomio calcolato con lo schema di Horner.</returns>
    private static double EvaluatePolynomial(ReadOnlySpan<double> coefficients, double value)
    {
        var result = 0d;
        for (var index = coefficients.Length - 1; index >= 0; index--)
            result = result * value + coefficients[index];
        return result;
    }

    /// <summary>
    /// Valida la dimensione minima e la finitezza di un campione numerico.
    /// </summary>
    /// <param name="sample">Campione da validare.</param>
    /// <param name="parameterName">Nome del parametro usato nelle eccezioni.</param>
    /// <param name="minimumLength">Numero minimo di osservazioni richiesto.</param>
    /// <exception cref="ArgumentNullException">Sollevata se il campione è null.</exception>
    /// <exception cref="ArgumentException">Sollevata se dimensione o valori non sono validi.</exception>
    private static void ValidateSample(double[] sample, string parameterName, int minimumLength)
    {
        ArgumentNullException.ThrowIfNull(sample, parameterName);
        if (sample.Length < minimumLength)
            throw new ArgumentException($"Il campione richiede almeno {minimumLength} osservazioni.", parameterName);
        if (Array.Exists(sample, static value => !double.IsFinite(value)))
            throw new ArgumentException("Il campione contiene valori non finiti.", parameterName);
    }

    /// <summary>
    /// Calcola la varianza campionaria corretta rispetto a una media già nota.
    /// </summary>
    /// <param name="sample">Campione con almeno due osservazioni.</param>
    /// <param name="mean">Media aritmetica del campione.</param>
    /// <returns>Varianza con correzione di Bessel.</returns>
    private static double SampleVariance(double[] sample, double mean)
    {
        var sum = 0d;
        foreach (var value in sample)
        {
            var difference = value - mean;
            sum += difference * difference;
        }

        return sum / (sample.Length - 1d);
    }

    /// <summary>
    /// Costruisce un risultato t di Student a due code da differenza e errore standard.
    /// </summary>
    /// <param name="difference">Differenza osservata rispetto all'ipotesi nulla.</param>
    /// <param name="standardError">Errore standard della differenza.</param>
    /// <param name="degreesOfFreedom">Gradi di libertà positivi.</param>
    /// <returns>Risultato con statistica t e p-value a due code.</returns>
    private static StatisticalTestResult StudentResult(
        double difference,
        double standardError,
        double degreesOfFreedom)
    {
        var statistic = standardError == 0d
            ? difference == 0d ? double.NaN : Math.CopySign(double.PositiveInfinity, difference)
            : difference / standardError;
        var pValue = double.IsNaN(statistic)
            ? double.NaN
            : 2d * (1d - StudentT.CDF(0d, 1d, degreesOfFreedom, Math.Abs(statistic)));
        return new StatisticalTestResult(statistic, pValue, degreesOfFreedom);
    }

    /// <summary>
    /// Calcola la probabilità ipergeometrica di una cella con margini fissi.
    /// </summary>
    /// <param name="value">Valore della cella superiore sinistra.</param>
    /// <param name="firstRow">Totale della prima riga.</param>
    /// <param name="firstColumn">Totale della prima colonna.</param>
    /// <param name="total">Totale complessivo della tabella.</param>
    /// <returns>Probabilità esatta della tabella.</returns>
    private static double HypergeometricProbability(int value, int firstRow, int firstColumn, int total)
    {
        var logarithm = SpecialFunctions.BinomialLn(firstColumn, value) +
                        SpecialFunctions.BinomialLn(total - firstColumn, firstRow - value) -
                        SpecialFunctions.BinomialLn(total, firstRow);
        return Math.Exp(logarithm);
    }

    /// <summary>
    /// Calcola la probabilità che la distanza empirica KS resti strettamente sotto la soglia osservata.
    /// </summary>
    /// <param name="firstCount">Dimensione del primo campione.</param>
    /// <param name="secondCount">Dimensione del secondo campione.</param>
    /// <param name="statistic">Distanza KS osservata.</param>
    /// <returns>Probabilità cumulativa interna alla banda KS.</returns>
    private static double KolmogorovSmirnovInsideProbability(
        int firstCount,
        int secondCount,
        double statistic)
    {
        var probabilities = new double[firstCount + 1, secondCount + 1];
        probabilities[0, 0] = 1d;
        for (var firstIndex = 0; firstIndex <= firstCount; firstIndex++)
        {
            for (var secondIndex = 0; secondIndex <= secondCount; secondIndex++)
            {
                if (firstIndex == 0 && secondIndex == 0) continue;
                var distance = Math.Abs(
                    (double)firstIndex / firstCount - (double)secondIndex / secondCount);
                if (distance >= statistic - 1e-15) continue;
                var visited = firstIndex + secondIndex;
                var probability = 0d;
                if (firstIndex > 0)
                    probability += probabilities[firstIndex - 1, secondIndex] * firstIndex / visited;
                if (secondIndex > 0)
                    probability += probabilities[firstIndex, secondIndex - 1] * secondIndex / visited;
                probabilities[firstIndex, secondIndex] = probability;
            }
        }

        return probabilities[firstCount, secondCount];
    }
}
