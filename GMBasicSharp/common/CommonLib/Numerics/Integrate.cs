// FILE: StdLib.Numerics.Integrate.cs
// ReSharper disable CheckNamespace

namespace CommonLib;

/// <summary>
/// Quadratura numerica e solutore di ODE con parità ai nuclei scipy.integrate
/// (quad, trapezoid, cumulative_trapezoid) e scipy.integrate.solve_ivp.
/// </summary>
public static partial class Numerics
{
    // --- Nodi e pesi Gauss-Kronrod 15/7 (costanti standard QUADPACK dqk15). ---
    private static readonly double[] K15Nodes =
    {
        0.9914553711208126, 0.9491079123427585, 0.8648644233597691,
        0.7415311855993944, 0.5860872354676911, 0.4058451513773972, 0.2077849550078985
    };
    private static readonly double[] K15Weights =
    {
        0.0229353220105292, 0.0630920926299786, 0.1047900103222501,
        0.1406532597155259, 0.1690047266392679, 0.1903505780647854, 0.2044329400752989, 0.2094821410847278
    };
    private static readonly double[] K7Weights =
    {
        0.1294849661688697, 0.2797053914892767, 0.3818300505051189, 0.4179591836734694
    };

    /// <summary>
    /// Integrale definito adattivo con regola Gauss-Kronrod 15/7 (come scipy.quad):
    /// stima e stima dell'errore, suddivisione ricorsiva fino a <paramref name="tol"/>
    /// o profondità massima. Adatto a integrandi lisci su intervalli finiti.
    /// </summary>
    public static double IntegrateQuad(Func<double, double> f, double a, double b, double tol = 1e-12)
    {
        if (f == null) throw new ArgumentNullException(nameof(f));
        if (double.IsNaN(a) || double.IsNaN(b)) return double.NaN;
        if (a == b) return 0.0;
        var sum = GaussKronrod15(f, a, b, out _);
        var scale = 1.0 + Math.Abs(sum);
        var threshold = tol * scale;
        var err = AdaptiveGaussKronrod(f, a, b, threshold, 0, out var total);
        return err <= threshold ? total : total;
    }

    /// <summary>Stima dell'errore (per diagnostica).</summary>
    public static double IntegrateQuadError(Func<double, double> f, double a, double b, double tol = 1e-12)
    {
        if (f == null || a == b) return 0.0;
        var threshold = tol * (1.0 + Math.Abs(GaussKronrod15(f, a, b, out _)));
        return AdaptiveGaussKronrod(f, a, b, threshold, 0, out _);
    }

    private static double GaussKronrod15(Func<double, double> f, double a, double b, out double err)
    {
        var mid = 0.5 * (a + b);
        var half = 0.5 * (b - a);
        double sum15 = K15Weights[7] * f(mid);
        double sum7 = K7Weights[3] * f(mid);
        // I nodi della Gauss a 7 punti sono i sotto-nodi di Kronrod agli indici 1, 3, 5.
        int[] gaussIdx = { 1, 3, 5 };
        var g = 0;
        for (var i = 0; i < 7; i++)
        {
            var dx = half * K15Nodes[i];
            var f1 = f(mid - dx);
            var f2 = f(mid + dx);
            sum15 += K15Weights[i] * (f1 + f2);
            if (g < 3 && i == gaussIdx[g])
            {
                sum7 += K7Weights[g] * (f1 + f2);
                g++;
            }
        }
        var r15 = sum15 * half;
        err = Math.Abs(r15 - sum7 * half);
        return r15;
    }

    private static double AdaptiveGaussKronrod(Func<double, double> f, double a, double b, double threshold, int depth,
        out double total)
    {
        var mid = 0.5 * (a + b);
        var left = GaussKronrod15(f, a, mid, out var eL);
        var right = GaussKronrod15(f, mid, b, out var eR);
        var e = eL + eR;
        if (depth < 24 && e > threshold)
        {
            var e2 = AdaptiveGaussKronrod(f, a, mid, threshold * 0.5, depth + 1, out var tL)
                     + AdaptiveGaussKronrod(f, mid, b, threshold * 0.5, depth + 1, out var tR);
            total = tL + tR;
            return e2;
        }
        total = left + right;
        return e;
    }

    /// <summary>
    /// Integrale definito con la regola di Simpson composita su n pannelli pari.
    /// (scipy: quad/romberg come riferimento; Simpson è la regola classica a pannelli pari.)
    /// </summary>
    public static double IntegrateSimpson(Func<double, double> f, double a, double b, int n = 256)
    {
        if (f == null) throw new ArgumentNullException(nameof(f));
        if (double.IsNaN(a) || double.IsNaN(b)) return double.NaN;
        if (a == b) return 0.0;
        if (n < 2) n = 2;
        if (n % 2 != 0) n++;
        var h = (b - a) / n;
        double sum = f(a) + f(b);
        for (var i = 1; i < n; i++)
        {
            var w = i % 2 == 0 ? 2.0 : 4.0;
            sum += w * f(a + i * h);
        }
        return sum * h / 3.0;
    }

    /// <summary>
    /// Integrale trapezoidale di una funzione campionata (scipy.integrate.trapezoid):
    /// y campioni e relative ascisse x (stessa lunghezza).
    /// </summary>
    public static double IntegrateTrapezoid(double[] y, double[] x)
    {
        if (y == null || x == null) throw new ArgumentNullException();
        if (y.Length != x.Length) throw new ArgumentException("y e x devono avere la stessa lunghezza.");
        if (y.Length < 2) return 0.0;
        double sum = 0;
        for (var i = 0; i < y.Length - 1; i++)
            sum += 0.5 * (y[i] + y[i + 1]) * (x[i + 1] - x[i]);
        return sum;
    }

    /// <summary>
    /// Integrale trapezoidale cumulativo (scipy.integrate.cumulative_trapezoid):
    /// restituisce n-1 valori (default scipy initial=None).
    /// </summary>
    public static double[] IntegrateCumulativeTrapezoid(double[] y, double[] x)
    {
        if (y == null || x == null) throw new ArgumentNullException();
        if (y.Length != x.Length) throw new ArgumentException("y e x devono avere la stessa lunghezza.");
        if (y.Length < 2) return Array.Empty<double>();
        var res = new double[y.Length - 1];
        double acc = 0;
        for (var i = 0; i < y.Length - 1; i++)
        {
            acc += 0.5 * (y[i] + y[i + 1]) * (x[i + 1] - x[i]);
            res[i] = acc;
        }
        return res;
    }

    /// <summary>
    /// Solutore ODE del primo ordine dy/dt = f(t, y) con Runge-Kutta a 4 stadi a passo
    /// fisso (scipy.integrate.solve_ivp per problemi non stiff). Restituisce y(t) sugli
    /// n+1 punti uniformi da t0 a t1.
    /// </summary>
    public static double[] SolveOde(Func<double, double, double> f, double y0, double t0, double t1, int steps)
    {
        if (f == null) throw new ArgumentNullException(nameof(f));
        if (steps < 1) steps = 1;
        if (t0 == t1) return new[] { y0 };
        var h = (t1 - t0) / steps;
        var ys = new double[steps + 1];
        var t = t0;
        var y = y0;
        ys[0] = y0;
        for (var i = 0; i < steps; i++)
        {
            var k1 = f(t, y);
            var k2 = f(t + 0.5 * h, y + 0.5 * h * k1);
            var k3 = f(t + 0.5 * h, y + 0.5 * h * k2);
            var k4 = f(t + h, y + h * k3);
            y += (h / 6.0) * (k1 + 2.0 * k2 + 2.0 * k3 + k4);
            t += h;
            ys[i + 1] = y;
        }
        return ys;
    }
    /// <summary>scipy.integrate.solve_ivp (method RK45) — ODE adattiva Dormand-Prince.
    /// Ritorna la traiettoria come matrice [n+1, ydim+1]: colonna 0 = t, colonne 1..ydim = y.</summary>
    public static double[,] SolveIvpk45(Func<double, double[], double[]> f, double[] y0, double t0, double t1,
        double rtol = 1e-6, double atol = 1e-9)
    {
        var yd = y0.Length;
        var y = (double[])y0.Clone();
        var t = t0;
        var rows = new System.Collections.Generic.List<double[]>();
        rows.Add(ConcatT(t, y));
        double h = Math.Sqrt(Math.Max(1e-12, Math.Abs(t1 - t0)));
        var direction = t1 >= t0 ? 1.0 : -1.0;
        var maxSteps = 10000;
        while (Math.Abs(t - t1) > 1e-12 && rows.Count < maxSteps)
        {
            if (Math.Abs(h) > Math.Abs(t1 - t)) h = t1 - t;
            var (yNew, err) = Rk45Step(f, t, y, h, yd);
            // errore relativo normalizzato
            double maxErr = 0;
            for (var i = 0; i < yd; i++)
            {
                var sc = atol + rtol * Math.Max(Math.Abs(y[i]), Math.Abs(yNew[i]));
                var e = err[i] / sc;
                maxErr = Math.Max(maxErr, e * e);
            }
            maxErr = Math.Sqrt(maxErr / yd);
            if (maxErr <= 1.0)
            {
                t += h;
                y = yNew;
                rows.Add(ConcatT(t, y));
            }
            // step adattivo (norma 5, cap fattore 0.1..5)
            var factor = maxErr > 0 ? Math.Min(5.0, Math.Max(0.1, 0.9 * Math.Pow(maxErr, -0.2))) : 5.0;
            h *= factor * direction;
            if (Math.Abs(h) < 1e-12) break;
        }
        var result = new double[rows.Count, yd + 1];
        for (var r = 0; r < rows.Count; r++)
            for (var c = 0; c <= yd; c++)
                result[r, c] = rows[r][c];
        return result;
    }

    private static double[] ConcatT(double t, double[] y)
    {
        var row = new double[y.Length + 1];
        row[0] = t;
        for (var i = 0; i < y.Length; i++) row[i + 1] = y[i];
        return row;
    }

    // Tabella Butcher Dormand-Prince 5(4)
    private static (double[], double[]) Rk45Step(Func<double, double[], double[]> f, double t, double[] y, double h, int n)
    {
        var k1 = f(t, y);
        var yk = new double[n];
        for (var i = 0; i < n; i++) yk[i] = y[i] + h * (1.0 / 5.0) * k1[i];
        var k2 = f(t + h / 5.0, yk);
        for (var i = 0; i < n; i++) yk[i] = y[i] + h * ((3.0 / 40.0) * k1[i] + (9.0 / 40.0) * k2[i]);
        var k3 = f(t + 3.0 * h / 10.0, yk);
        for (var i = 0; i < n; i++) yk[i] = y[i] + h * ((44.0 / 45.0) * k1[i] - (56.0 / 15.0) * k2[i] + (32.0 / 9.0) * k3[i]);
        var k4 = f(t + 4.0 * h / 5.0, yk);
        for (var i = 0; i < n; i++) yk[i] = y[i] + h * ((19372.0 / 6561.0) * k1[i] - (25360.0 / 2187.0) * k2[i] + (64448.0 / 6561.0) * k3[i] - (212.0 / 729.0) * k4[i]);
        var k5 = f(t + h, yk);
        for (var i = 0; i < n; i++) yk[i] = y[i] + h * ((9017.0 / 3168.0) * k1[i] - (355.0 / 33.0) * k2[i] + (46732.0 / 5247.0) * k3[i] + (49.0 / 176.0) * k4[i] - (5103.0 / 18656.0) * k5[i]);
        var k6 = f(t + h, yk);
        var y5 = new double[n];
        for (var i = 0; i < n; i++)
            y5[i] = y[i] + h * ((35.0 / 384.0) * k1[i] + (500.0 / 1113.0) * k3[i] + (125.0 / 192.0) * k4[i] - (2187.0 / 6784.0) * k5[i] + (11.0 / 84.0) * k6[i]);
        var k7 = f(t + h, y5);
        var y4 = new double[n];
        for (var i = 0; i < n; i++)
            y4[i] = y[i] + h * ((5179.0 / 57600.0) * k1[i] + (7571.0 / 16695.0) * k3[i] + (393.0 / 640.0) * k4[i] - (92097.0 / 339200.0) * k5[i] + (187.0 / 2100.0) * k6[i] + (1.0 / 40.0) * k7[i]);
        var err = new double[n];
        for (var i = 0; i < n; i++) err[i] = y5[i] - y4[i];
        return (y5, err);
    }
}
