// FILE: StdLib.Runtime.TimeSeries.cs
// Dominio TimeSeries della StdLib.Runtime: interpolazione + analisi di serie temporali.
// PURO per costruzione (vedi Ast.NativeExecutionKind): nessun metodo qui accede a stato che
// sopravvive oltre la singola chiamata. Rispecchia esattamente le firme di CommonLib.Numerics
// (nessun riordino argomenti). Ordine argomenti replicato dal vecchio dead file
// PCodeVm/PCodeProcessor.StdLib.TimeSeries.cs (mai dispatchato, ma con arg-mapping verificato
// dal porting storico da IlBackend/IlRuntime.cs) — Returns/Volatility/RollingDelta usano solo
// gli argomenti impliciti (senza il parametro string/int opzionale) per restare fedeli a quel
// comportamento consolidato.

namespace Basic.StdLibRuntime;

public static class TimeSeriesFunctions
{
    public static double InterpolateLinear(double[] xPoints, double[] yPoints, double targetX) => CommonLib.Numerics.InterpolateLinear(xPoints, yPoints, targetX);
    public static double InterpolateSpline(double[] xPoints, double[] yPoints, double targetX) => CommonLib.Numerics.InterpolateSpline(xPoints, yPoints, targetX);
    public static double InterpolatePolynomial(double[] xPoints, double[] yPoints, double targetX) => CommonLib.Numerics.InterpolatePolynomial(xPoints, yPoints, targetX);
    public static double InterpolatePchip(double[] xPoints, double[] yPoints, double targetX) => CommonLib.Numerics.InterpolatePchip(xPoints, yPoints, targetX);
    public static double InterpolateStep(double[] xPoints, double[] yPoints, double targetX) => CommonLib.Numerics.InterpolateStep(xPoints, yPoints, targetX);

    public static double[] MovingAverage(double[] data, int window) => CommonLib.Numerics.MovingAverage(data, window);

    public static double[] Ema(double[] data, double period) => CommonLib.Numerics.Ema(data, 2.0 / (period + 1.0));

    public static double[] RollingStdDev(double[] data, int window) => CommonLib.Numerics.RollingStdDev(data, window);
    public static double[] RollingMad(double[] data, int window) => CommonLib.Numerics.RollingMad(data, window);
    public static double[] RollingZScore(double[] data, int window) => CommonLib.Numerics.RollingZScore(data, window);
    public static double[] RollingAutocorrelation(double[] data, int window, int lag) => CommonLib.Numerics.RollingAutocorrelation(data, window, lag);
    public static double[] RollingCovariance(double[] x, double[] y, int window) => CommonLib.Numerics.RollingCovariance(x, y, window);
    public static double[] RollingCorrelation(double[] x, double[] y, int window) => CommonLib.Numerics.RollingCorrelation(x, y, window);
    public static double[] RollingDelta(double[] data, int window) => CommonLib.Numerics.RollingDelta(data, window);

    public static double[] Difference(double[] data, int order) => CommonLib.Numerics.Difference(data, order);
    public static double[] Returns(double[] data) => CommonLib.Numerics.Returns(data);
    public static double Volatility(double[] data) => CommonLib.Numerics.Volatility(data);
    public static double Autocorrelation(double[] data, int lag) => CommonLib.Numerics.Autocorrelation(data, lag);
    public static double[] Deseasonalize(double[] data, int seasonLength) => CommonLib.Numerics.Deseasonalize(data, seasonLength);

    /// <summary>Restituisce la decomposizione (trend/stagionale/residuo) come array jagged —
    /// CommonLib.Numerics.Decompose ritorna un double[,] rettangolare, convertito qui perché il
    /// resto della VM (e IL) lavora con array jagged per i risultati multi-riga.</summary>
    public static double[][] Decompose(double[] data, int seasonLength)
    {
        var matrix = CommonLib.Numerics.Decompose(data, seasonLength);
        var rows = matrix.GetLength(0);
        var cols = matrix.GetLength(1);
        var result = new double[rows][];
        for (var r = 0; r < rows; r++)
        {
            result[r] = new double[cols];
            for (var c = 0; c < cols; c++) result[r][c] = matrix[r, c];
        }
        return result;
    }
}
