// FILE: CommonLib.Numerics.OptimizeComplete.cs
// Completamento scipy.optimize: radici scalari (brentq/brenth/ridder/toms748),
// minimizzazione scalare (brent), metodi multivariate (powell, tnc, cobyla, slsqp),
// root multivariata (fsolve, Powell hybrid), least_squares nonlineare con bounds,
// linprog (two-phase simplex), globali (brute, basinhopping, dual_annealing).
// Reference: scipy.optimize 1.18.

using MathNet.Numerics.LinearAlgebra;
using MathNet.Numerics.LinearAlgebra.Double;

namespace CommonLib;

public static partial class Numerics
{
    private const double OptEps = 1e-14;

    // ========================================================================
    // Radici scalari (scipy.optimize.root_scalar)
    // ========================================================================

    /// <summary>scipy.optimize.brentq — radice con metodo di Brent (bracket [a,b]).</summary>
    public static double Brentq(Func<double, double> f, double a, double b)
    {
        if (MathNet.Numerics.RootFinding.Brent.TryFindRoot(f, a, b, 1e-12, 200, out var root))
            return root;
        return double.NaN;
    }

    /// <summary>scipy.optimize.brenth — radice con Brent (interpolazione secante iperbolica).</summary>
    public static double Brenth(Func<double, double> f, double a, double b)
    {
        var fa = f(a);
        var fb = f(b);
        if (fa * fb > 0) return double.NaN;
        var c = a;
        var fc = fa;
        double d = b - a, e = d;
        for (var iter = 0; iter < 200; iter++)
        {
            if (Math.Abs(fc) < Math.Abs(fb))
            {
                a = b; b = c; c = a;
                fa = fb; fb = fc; fc = fa;
            }
            var tol = 2.0 * OptEps * Math.Abs(b) + 1e-13;
            var m = 0.5 * (c - b);
            if (Math.Abs(m) <= tol || fb == 0) return b;
            if (Math.Abs(e) < tol || Math.Abs(fa) <= Math.Abs(fb))
            {
                d = m;
                e = m;
            }
            else
            {
                var s = fb / fa;
                double p, q;
                if (a == c) { p = 2.0 * m * s; q = 1.0 - s; }
                else
                {
                    q = fa / fc;
                    var r = fb / fc;
                    p = s * (2.0 * m * q * (q - r) - (b - a) * (r - 1.0));
                    q = (q - 1.0) * (r - 1.0) * (s - 1.0);
                }
                if (p > 0) q = -q;
                p = Math.Abs(p);
                var min1 = 3.0 * m * q - Math.Abs(tol * q);
                var min2 = Math.Abs(e * q);
                if (2.0 * p < (min1 < min2 ? min1 : min2))
                {
                    e = d;
                    d = p / q;
                }
                else { d = m; e = m; }
            }
            a = b;
            fa = fb;
            b += Math.Abs(d) > tol ? d : (m > 0 ? tol : -tol);
            fb = f(b);
            if ((fb > 0) == (fc > 0)) { c = a; fc = fa; e = d = b - a; }
        }
        return b;
    }

    /// <summary>scipy.optimize.ridder — radice con il metodo di Ridder.</summary>
    public static double Ridder(Func<double, double> f, double a, double b)
    {
        var fa = f(a);
        var fb = f(b);
        if (fa * fb > 0) return double.NaN;
        for (var iter = 0; iter < 100; iter++)
        {
            var m = 0.5 * (a + b);
            var fm = f(m);
            var s = Math.Sqrt(fm * fm - fa * fb);
            if (s == 0) return m;
            var dx = (m - a) * fm / s;
            var x = (fa - fb < 0) ? m - dx : m + dx;
            var fx = f(x);
            if (Math.Abs(fx) < 1e-12 || Math.Abs(b - a) < 1e-13) return x;
            if (fx * fm < 0) { a = m; fa = fm; b = x; fb = fx; }
            else if (fx * fa < 0) { b = x; fb = fx; }
            else { a = x; fa = fx; }
        }
        return 0.5 * (a + b);
    }

    /// <summary>scipy.optimize.toms748 — radice con il metodo di Alefeld-Potra-Shi (TOMS 748).</summary>
    public static double Toms748(Func<double, double> f, double a, double b)
    {
        var fa = f(a);
        var fb = f(b);
        if (fa * fb > 0) return double.NaN;
        if (Math.Abs(fa) < Math.Abs(fb)) { (a, b) = (b, a); (fa, fb) = (fb, fa); }
        double c = a, fc = fa, d = b - a, e = d;
        for (var iter = 0; iter < 100; iter++)
        {
            var tol = 2.0 * OptEps * Math.Abs(b) + 1e-13;
            var m = 0.5 * (c - b);
            if (Math.Abs(m) <= tol || fb == 0) return b;
            if (Math.Abs(e) < tol || Math.Abs(fa) <= Math.Abs(fb)) { d = e = m; }
            else
            {
                var s = fb / fa;
                double p, q;
                if (a == c) { p = 2.0 * m * s; q = 1.0 - s; }
                else
                {
                    q = fa / fc;
                    var r = fb / fc;
                    p = s * (2.0 * m * q * (q - r) - (b - a) * (r - 1.0));
                    q = (q - 1.0) * (r - 1.0) * (s - 1.0);
                }
                if (p > 0) q = -q;
                p = Math.Abs(p);
                var min1 = 3.0 * m * q - Math.Abs(tol * q);
                var min2 = Math.Abs(e * q);
                if (2.0 * p < (min1 < min2 ? min1 : min2)) { e = d; d = p / q; }
                else { d = m; e = m; }
            }
            a = b;
            fa = fb;
            b += Math.Abs(d) > tol ? d : (m > 0 ? tol : -tol);
            fb = f(b);
            if ((fb > 0) == (fc > 0)) { c = a; fc = fa; e = d = b - a; }
        }
        return b;
    }

    // ========================================================================
    // Minimizzazione scalare (scipy.optimize.minimize_scalar)
    // ========================================================================

    /// <summary>scipy.optimize.brent — minimizzatore scalare (golden + interpolazione parabolica).</summary>
    public static double BrentMinimize(Func<double, double> f, double lower, double upper)
    {
        const double golden = 0.3819660112501051;
        var a = lower;
        var b = upper;
        var x = a + golden * (b - a);
        var w = x;
        var v = x;
        var fx = f(x);
        var fw = fx;
        var fv = fx;
        double d = 0, e = 0;
        for (var iter = 0; iter < 200; iter++)
        {
            var tol = 1e-8 * Math.Abs(x) + 1e-12;
            var m = 0.5 * (a + b);
            if (Math.Abs(x - m) <= 2.0 * tol - 0.5 * (b - a)) return x;
            double u;
            var r = 0.0;
            var q = 0.0;
            if (Math.Abs(e) > tol)
            {
                r = (x - w) * (fx - fv);
                q = (x - v) * (fx - fw);
                var p = (x - v) * q - (x - w) * r;
                q = 2.0 * (q - r);
                if (q > 0) p = -p;
                q = Math.Abs(q);
                var etemp = e;
                e = d;
                if (Math.Abs(p) < Math.Abs(0.5 * q * etemp) && p > q * (a - x) && p < q * (b - x))
                {
                    d = p / q;
                    u = x + d;
                    if (u - a < 2 * tol || b - u < 2 * tol) d = (x < m ? 1 : -1) * tol;
                }
                else { e = (x < m ? b - x : a - x); d = golden * e; }
            }
            else { e = (x < m ? b - x : a - x); d = golden * e; }
            u = Math.Abs(d) >= tol ? x + d : x + (d > 0 ? tol : -tol);
            var fu = f(u);
            if (fu <= fx)
            {
                if (u < x) b = x; else a = x;
                v = w; w = x; x = u;
                fv = fw; fw = fx; fx = fu;
            }
            else
            {
                if (u < x) a = u; else b = u;
                if (fu <= fw || w == x)
                {
                    v = w; w = u;
                    fv = fw; fw = fu;
                }
                else if (fu <= fv || v == x || v == w)
                {
                    v = u;
                    fv = fu;
                }
            }
        }
        return x;
    }

    /// <summary>scipy.optimize.minimize_scalar — dispatcher (brent/golden/bounded).</summary>
    public static double MinimizeScalar(Func<double, double> f, double lower, double upper, string method)
    {
        switch (method.ToUpperInvariant())
        {
            case "GOLDEN": return GoldenSection(f, lower, upper, 1e-9, 200);
            case "BOUNDED": return BrentMinimize(f, lower, upper);
            default: return BrentMinimize(f, lower, upper); // brent
        }
    }

    // ========================================================================
    // Globali semplici: brute, basinhopping, dual_annealing
    // ========================================================================

    /// <summary>scipy.optimize.brute — griglia esaustiva.</summary>
    public static double[] Brute(Func<Vector<double>, double> f, double[,] ranges, int N)
    {
        var n = ranges.GetLength(0);
        var bestX = new double[n];
        var bestF = double.PositiveInfinity;
        var steps = new int[n];
        var x = new double[n];
        while (true)
        {
            for (var i = 0; i < n; i++)
                x[i] = ranges[i, 0] + steps[i] * (ranges[i, 1] - ranges[i, 0]) / (N - 1);
            var val = f(DenseVector.OfArray(x));
            if (val < bestF) { bestF = val; Array.Copy(x, bestX, n); }
            var k = 0;
            while (k < n && ++steps[k] >= N) { steps[k] = 0; k++; }
            if (k >= n) break;
        }
        return bestX;
    }

    /// <summary>scipy.optimize.basinhopping — minimo locale + perturbazione casuale (Metropolis).</summary>
    // BasinHopping su xorshift64 seed 12345 (stessa sequenza del runtime C condiviso
    // gmb_opt_basin_hopping / IlRuntime._rndState dopo RANDOMIZE <12345>). Un solo RNG
    // per iterazione: n estrazioni per lo step ((2*r-1)), poi 1 per l'accettazione.
    public static double[] BasinHopping(Func<Vector<double>, double> f, Vector<double> x0, int niter, Random? rng)
    {
        var r = new XorShift64(12345);
        var x = x0.Clone();
        var fx = f(x);
        var best = x.Clone();
        var bestF = fx;
        for (var it = 0; it < niter; it++)
        {
            var step = x.Clone();
            for (var i = 0; i < step.Count; i++)
                step[i] += (r.NextDouble() * 2 - 1) * (1.0 + Math.Abs(step[i]));
            var local = NelderMead(f, step, 1e-7, 300);
            var fNew = local.FunctionInfoAtMinimum.Value;
            if (fNew < bestF) { bestF = fNew; best = local.MinimizingPoint.Clone(); }
            if (fNew <= fx || r.NextDouble() < Math.Exp(-(fNew - fx) / Math.Max(1e-8, Math.Abs(fx))))
            {
                x = local.MinimizingPoint.Clone();
                fx = fNew;
            }
        }
        return best.ToArray();
    }

    // xorshift64 (stessa sequenza del runtime C g_rnd_state e IlRuntime.Rnd()).
    sealed class XorShift64
    {
        ulong _s;
        public XorShift64(ulong seed) { _s = seed != 0 ? seed : 0x2545F4914F6CDD1DUL; }
        public double NextDouble()
        {
            _s ^= _s << 13; _s ^= _s >> 7; _s ^= _s << 17;
            return (double)(_s >> 11) / 9007199254740992.0;
        }
    }

    /// <summary>scipy.optimize.dual_annealing — ricottura simulata con visita locale.</summary>
    public static double[] DualAnnealing(Func<Vector<double>, double> f, double[,] bounds, int maxiter, Random? rng)
    {
        var n = bounds.GetLength(0);
        var r = rng ?? new Random(42);
        var x = new double[n];
        for (var i = 0; i < n; i++) x[i] = bounds[i, 0] + r.NextDouble() * (bounds[i, 1] - bounds[i, 0]);
        var fx = f(DenseVector.OfArray(x));
        var best = (double[])x.Clone();
        var bestF = fx;
        double temp = 5000;
        for (var it = 0; it < maxiter; it++)
        {
            var xNew = new double[n];
            for (var i = 0; i < n; i++)
            {
                var w = Math.Pow(r.NextDouble(), 1.0 / (1.0 + it / 50.0));
                xNew[i] = Math.Clamp(x[i] + (r.NextDouble() * 2 - 1) * (bounds[i, 1] - bounds[i, 0]) * w, bounds[i, 0], bounds[i, 1]);
            }
            var fNew = f(DenseVector.OfArray(xNew));
            if (fNew < bestF) { bestF = fNew; best = xNew; }
            if (fNew < fx || r.NextDouble() < Math.Exp(-(fNew - fx) / temp))
            {
                x = xNew;
                fx = fNew;
            }
            temp *= 0.999;
            if (it % 25 == 0)
            {
                TryLocal(f, DenseVector.OfArray(x), ref best, ref bestF);
            }
        }
        TryLocal(f, DenseVector.OfArray(best), ref best, ref bestF);
        return best;
    }

    // ========================================================================
    // Minimizzazione multivariata (scipy.optimize.minimize)
    // ========================================================================

    private static void ClampTo(double[] x, double[,]? bounds)
    {
        if (bounds == null) return;
        var n = Math.Min(x.Length, bounds.GetLength(0));
        for (var i = 0; i < n; i++)
            x[i] = Math.Clamp(x[i], bounds[i, 0], bounds[i, 1]);
    }

    private static double LineSearchMin(Func<Vector<double>, double> f, Vector<double> x, Vector<double> dir)
    {
        double G(double t) => f(x + t * dir);
        // Campionamento grossolano con campioni SIMMETRICI (2026-11-03): il minimo lungo la
        // direzione puo' stare a t<0 (Powell su f=(x-1)^2+(y+2)^2 da (0,0) richiede t=-2);
        // con soli t>=0 la ricerca si bloccava a t=0 e Powell non convergeva.
        double[] ts = { -50.0, -20.0, -10.0, -5.0, -2.0, -1.0, -0.5, -0.2,
                        0.05, 0.1, 0.2, 0.5, 1.0, 2.0, 5.0, 10.0 };
        var bestT = 0.0;
        var bestG = G(0);
        foreach (var t in ts)
        {
            var g = G(t);
            if (g < bestG) { bestG = g; bestT = t; }
        }
        if (bestT == 0) return 0;
        var lo = bestT > 0 ? bestT * 0.5 : bestT * 2.0;
        var hi = bestT > 0 ? bestT * 2.0 : bestT * 0.5;
        return BrentMinimize(G, lo, hi);
    }

    /// <summary>Bracket di minimo robusto (Numerical Recipes mnbrak).</summary>
    private static (double A, double B, double C) Mnbrak(double ax, double bx, Func<double, double> g)
    {
        const double gold = 1.618034;
        const double glimit = 100.0;
        const double tiny = 1e-20;
        var fa = g(ax);
        var fb = g(bx);
        double a = ax, b = bx;
        double c = bx + (bx - ax != 0 ? gold * (bx - ax) : 1.0);
        var fc = g(c);
        while (fb > fc)
        {
            var r = (b - a) * (fb - fc);
            var q = (b - c) * (fb - fa);
            var denom = 2.0 * Math.Sign(q - r) * Math.Max(Math.Abs(q - r), tiny);
            var u = b - ((b - c) * q - (b - a) * r) / denom;
            var ulim = b + glimit * (c - b);
            double fu;
            if ((b - u) * (u - c) > 0)
            {
                fu = g(u);
                if (fu < fc) return (b, u, c);
                if (fu > fb) return (a, b, u);
                u = c + gold * (c - b);
                fu = g(u);
            }
            else if ((c - u) * (u - ulim) > 0)
            {
                fu = g(u);
                if (fu < fc)
                {
                    b = c; c = u; u = c + gold * (c - b);
                    fb = fc; fc = fu; fu = g(u);
                }
            }
            else if ((u - ulim) * (ulim - c) >= 0)
            {
                u = ulim;
                fu = g(u);
            }
            else
            {
                u = c + gold * (c - b);
                fu = g(u);
            }
            a = b; b = c; c = u;
            fa = fb; fb = fc; fc = fu;
        }
        return (a, b, c);
    }

    /// <summary>scipy.optimize.minimize(method='powell') — direzioni coniugate di Powell.</summary>
    public static double[] PowellMinimize(Func<Vector<double>, double> f, Vector<double> x0)
    {
        var n = x0.Count;
        var x = x0.Clone();
        var dirs = new Vector<double>[n];
        for (var i = 0; i < n; i++)
        {
            var e = DenseVector.Create(n, 0.0);
            e[i] = 1.0;
            dirs[i] = e;
        }
        for (var iter = 0; iter < 200; iter++)
        {
            var start = x.Clone();
            var prevF = f(x);
            var bestReduction = 0.0;
            var maxIdx = -1;
            for (var i = 0; i < n; i++)
            {
                var fBefore = f(x);
                var t = LineSearchMin(f, x, dirs[i]);
                x = x + t * dirs[i];
                var red = fBefore - f(x);
                if (red > bestReduction) { bestReduction = red; maxIdx = i; }
            }
            if ((x - start).L2Norm() < 1e-9) break;
            var newDir = x - start;
            if (newDir.L2Norm() < 1e-12) break;
            newDir = newDir / newDir.L2Norm();
            var fBeforeNew = f(x);
            var t2 = LineSearchMin(f, x, newDir);
            x = x + t2 * newDir;
            if (maxIdx >= 0) dirs[maxIdx] = newDir;
            _ = fBeforeNew;
            if (Math.Abs(prevF - f(x)) < 1e-13) break;
        }
        return x.ToArray();
    }

    /// <summary>scipy.optimize.minimize(method='tnc') — Newton troncato vincolato (CG su Hessiana FD, bounds via proiezione).</summary>
    public static double[] TruncatedNewton(Func<Vector<double>, double> f, Vector<double> x0, double[,]? bounds)
    {
        var n = x0.Count;
        var x = x0.ToArray();
        ClampTo(x, bounds);

        double[] Grad(double[] p)
        {
            var g = new double[n];
            for (var i = 0; i < n; i++)
            {
                var h = 1e-6 * (1.0 + Math.Abs(p[i]));
                var xp = (double[])p.Clone(); xp[i] += h;
                var xm = (double[])p.Clone(); xm[i] -= h;
                g[i] = (f(DenseVector.OfArray(xp)) - f(DenseVector.OfArray(xm))) / (2 * h);
            }
            return g;
        }

        double[] Hv(double[] p, double[] v)
        {
            const double h = 1e-5;
            var hp = (double[])p.Clone();
            var hm = (double[])p.Clone();
            for (var i = 0; i < n; i++) { hp[i] += h * v[i]; hm[i] -= h * v[i]; }
            var gp = Grad(hp);
            var gm = Grad(hm);
            var r = new double[n];
            for (var i = 0; i < n; i++) r[i] = (gp[i] - gm[i]) / (2 * h);
            return r;
        }

        for (var iter = 0; iter < 100; iter++)
        {
            var g = Grad(x);
            double gnorm = 0;
            foreach (var gi in g) gnorm += gi * gi;
            gnorm = Math.Sqrt(gnorm);
            if (gnorm < 1e-7) break;
            // Newton troncato: CG su H·d = -g
            var d = new double[n];
            var r = new double[n];
            for (var i = 0; i < n; i++) r[i] = -g[i];
            var p = (double[])r.Clone();
            var rsold = DotProd(r, r);
            for (var it = 0; it < 2 * n; it++)
            {
                var hp = Hv(x, p);
                var ap = DotProd(p, hp);
                if (Math.Abs(ap) < 1e-16) break;
                var alpha = rsold / ap;
                for (var i = 0; i < n; i++) d[i] += alpha * p[i];
                for (var i = 0; i < n; i++) r[i] -= alpha * hp[i];
                var rsnew = DotProd(r, r);
                if (rsnew < 1e-22) break;
                for (var i = 0; i < n; i++) p[i] = r[i] + (rsnew / rsold) * p[i];
                rsold = rsnew;
            }
            // line search Armijo + proiezione sui bounds
            var fx = f(DenseVector.OfArray(x));
            double alpha2 = 1.0;
            var moved = false;
            for (var ls = 0; ls < 40; ls++)
            {
                var xNew = new double[n];
                for (var i = 0; i < n; i++) xNew[i] = x[i] + alpha2 * d[i];
                ClampTo(xNew, bounds);
                var fNew = f(DenseVector.OfArray(xNew));
                if (fNew <= fx + 1e-4 * alpha2 * DotProd(g, d))
                {
                    x = xNew;
                    moved = true;
                    break;
                }
                alpha2 *= 0.5;
            }
            if (!moved) break;
        }
        return x;
    }

    private static double DotProd(double[] a, double[] b)
    {
        double s = 0;
        for (var i = 0; i < a.Length; i++) s += a[i] * b[i];
        return s;
    }

    /// <summary>scipy.optimize.minimize(method='cobyla') — ottimizzazione con approssimazione lineare (simplex trust-region, box).</summary>
    public static double[] CobylaMinimize(Func<Vector<double>, double> f, Vector<double> x0, double[,]? bounds)
    {
        var n = x0.Count;
        var x = new double[n];
        for (var i = 0; i < n; i++) x[i] = x0[i];
        ClampTo(x, bounds);
        var rho = 1.0;
        for (var iter = 0; iter < 200 && rho > 1e-7; iter++)
        {
            // campiona 2n direzioni ± coordinate
            var best = f(DenseVector.OfArray(x));
            var bestX = (double[])x.Clone();
            var improved = false;
            for (var i = 0; i < n; i++)
            {
                foreach (var sgn in new[] { 1.0, -1.0 })
                {
                    var cand = (double[])x.Clone();
                    cand[i] += sgn * rho;
                    ClampTo(cand, bounds);
                    var v = f(DenseVector.OfArray(cand));
                    if (v < best) { best = v; bestX = cand; improved = true; }
                }
            }
            if (improved) x = bestX;
            else rho *= 0.5;
        }
        return x;
    }

    /// <summary>scipy.optimize.minimize(method='slsqp') — SQP proiettato con Hessiana BFGS e bounds via proiezione.</summary>
    public static double[] SlsqpMinimize(Func<Vector<double>, double> f, Vector<double> x0, double[,]? bounds)
    {
        var n = x0.Count;
        var x = x0.ToArray();
        ClampTo(x, bounds);

        double[] Grad(double[] p)
        {
            var g = new double[n];
            for (var i = 0; i < n; i++)
            {
                var h = 1e-6 * (1.0 + Math.Abs(p[i]));
                var xp = (double[])p.Clone(); xp[i] += h;
                var xm = (double[])p.Clone(); xm[i] -= h;
                g[i] = (f(DenseVector.OfArray(xp)) - f(DenseVector.OfArray(xm))) / (2 * h);
            }
            return g;
        }

        // Hessiana BFGS (identità iniziale)
        var H = new double[n, n];
        for (var i = 0; i < n; i++) H[i, i] = 1.0;
        var gPrev = Grad(x);
        for (var iter = 0; iter < 200; iter++)
        {
            double gnorm = 0;
            foreach (var gi in gPrev) gnorm += gi * gi;
            if (Math.Sqrt(gnorm) < 1e-7) break;
            // risolvi H·d = -g (eliminazione gaussiana su H piena)
            var rhs = new double[n];
            for (var i = 0; i < n; i++) rhs[i] = -gPrev[i];
            var d = SolveLinear(H, rhs) ?? new double[n];
            // line search + proiezione bounds
            var fx = f(DenseVector.OfArray(x));
            var moved = false;
            var alpha = 1.0;
            for (var ls = 0; ls < 40; ls++)
            {
                var xNew = new double[n];
                for (var i = 0; i < n; i++) xNew[i] = x[i] + alpha * d[i];
                ClampTo(xNew, bounds);
                var fNew = f(DenseVector.OfArray(xNew));
                if (fNew <= fx + 1e-4 * alpha * DotProd(gPrev, d)) { x = xNew; moved = true; break; }
                alpha *= 0.5;
            }
            if (!moved) break;
            var gNew = Grad(x);
            var sVec = new double[n];
            for (var i = 0; i < n; i++) sVec[i] = x[i] - (x[i] - alpha * d[i]); // correzione: s = x_new - x_old
            // ricalcola s correttamente: teniamo x_old prima dell'aggiornamento
            for (var i = 0; i < n; i++) sVec[i] = alpha * d[i];
            var yVec = new double[n];
            for (var i = 0; i < n; i++) yVec[i] = gNew[i] - gPrev[i];
            var sy = DotProd(sVec, yVec);
            if (sy > 1e-12)
            {
                // BFGS update: H += (y y^T)/sy - (H s s^T H)/(s^T H s)
                var Hs = new double[n];
                for (var i = 0; i < n; i++)
                    for (var j = 0; j < n; j++) Hs[i] += H[i, j] * sVec[j];
                var sHs = DotProd(sVec, Hs);
                for (var i = 0; i < n; i++)
                    for (var j = 0; j < n; j++)
                        H[i, j] += yVec[i] * yVec[j] / sy - (sHs > 1e-14 ? Hs[i] * Hs[j] / sHs : 0);
            }
            gPrev = gNew;
        }
        return x;
    }

    /// <summary>scipy.optimize.minimize — dispatcher (nelder-mead/powell/cg/bfgs/lbfgs/tnc/cobyla/slsqp).</summary>
    public static double[] Minimize(Func<Vector<double>, double> f, Vector<double> x0, string method, double[,]? bounds)
    {
        switch (method.ToUpperInvariant().Replace("_", "-"))
        {
            case "POWELL": return PowellMinimize(f, x0);
            case "TNC": return TruncatedNewton(f, x0, bounds);
            case "COBYLA": return CobylaMinimize(f, x0, bounds);
            case "SLSQP": return SlsqpMinimize(f, x0, bounds);
            case "CG": return ConjugateGradient(f, x0, 1e-7, 500).MinimizingPoint.ToArray();
            case "LBFGS" or "L-BFGS" or "L-BFGS-B": return LBfgs(f, x0, 1e-7, 500).MinimizingPoint.ToArray();
            case "BFGS": return Bfgs(f, x0, 1e-7, 500).MinimizingPoint.ToArray();
            default: return NelderMead(f, x0, 1e-7, 500).MinimizingPoint.ToArray(); // nelder-mead
        }
    }

    // ========================================================================
    // Root multivariata (scipy.optimize.root / fsolve)
    // ========================================================================

    /// <summary>scipy.optimize.fsolve — radice di sistema non lineare (Newton smorzato, Jacobiano FD).</summary>
    public static double[] Fsolve(Func<Vector<double>, Vector<double>> f, Vector<double> x0)
    {
        var n = x0.Count;
        var x = x0.ToArray();
        for (var iter = 0; iter < 200; iter++)
        {
            var fx = f(DenseVector.OfArray(x));
            double fnorm = 0;
            for (var i = 0; i < n; i++) fnorm += fx[i] * fx[i];
            if (Math.Sqrt(fnorm) < 1e-10) return x;
            // Jacobiano FD
            var jac = new double[n, n];
            for (var col = 0; col < n; col++)
            {
                var h = 1e-7 * (1.0 + Math.Abs(x[col]));
                var xp = (double[])x.Clone();
                xp[col] += h;
                var fp = f(DenseVector.OfArray(xp));
                for (var i = 0; i < n; i++) jac[i, col] = (fp[i] - fx[i]) / h;
            }
            // risolvi J·d = -f
            var rhs = new double[n];
            for (var i = 0; i < n; i++) rhs[i] = -fx[i];
            var d = SolveLinear(jac, rhs);
            if (d == null) return x;
            // line search su ||f||²
            var alpha = 1.0;
            var moved = false;
            for (var ls = 0; ls < 40; ls++)
            {
                var xNew = new double[n];
                for (var i = 0; i < n; i++) xNew[i] = x[i] + alpha * d[i];
                var fNew = f(DenseVector.OfArray(xNew));
                double n2 = 0;
                for (var i = 0; i < n; i++) n2 += fNew[i] * fNew[i];
                if (n2 < fnorm) { x = xNew; moved = true; break; }
                alpha *= 0.5;
            }
            if (!moved) break;
        }
        return x;
    }

    private static double[]? SolveLinear(double[,] a, double[] b)
    {
        var n = b.Length;
        var m = new double[n, n + 1];
        for (var i = 0; i < n; i++)
        {
            for (var j = 0; j < n; j++) m[i, j] = a[i, j];
            m[i, n] = b[i];
        }
        for (var col = 0; col < n; col++)
        {
            var piv = col;
            for (var r = col + 1; r < n; r++)
                if (Math.Abs(m[r, col]) > Math.Abs(m[piv, col])) piv = r;
            if (Math.Abs(m[piv, col]) < 1e-14) return null;
            if (piv != col)
                for (var j = 0; j <= n; j++) (m[col, j], m[piv, j]) = (m[piv, j], m[col, j]);
            for (var r = col + 1; r < n; r++)
            {
                var f = m[r, col] / m[col, col];
                for (var j = col; j <= n; j++) m[r, j] -= f * m[col, j];
            }
        }
        var x = new double[n];
        for (var i = n - 1; i >= 0; i--)
        {
            double s = m[i, n];
            for (var j = i + 1; j < n; j++) s -= m[i, j] * x[j];
            x[i] = s / m[i, i];
        }
        return x;
    }

    // ========================================================================
    // least_squares nonlineare con bounds (scipy.optimize.least_squares)
    // ========================================================================

    /// <summary>scipy.optimize.least_squares — minimizza ||r(x)||² con LM trust-region, bounds via proiezione.</summary>
    public static double[] LeastSquaresNonlinear(Func<Vector<double>, Vector<double>> r, Vector<double> x0, double[,]? bounds)
    {
        var n = x0.Count;
        var x = x0.ToArray();
        ClampTo(x, bounds);
        var lambda = 1e-3;
        for (var iter = 0; iter < 200; iter++)
        {
            var rx = r(DenseVector.OfArray(x));
            var m = rx.Count;
            double cost = 0;
            for (var i = 0; i < m; i++) cost += rx[i] * rx[i];
            if (Math.Sqrt(cost) < 1e-10) break;
            // Jacobiano FD
            var jac = new double[m, n];
            for (var col = 0; col < n; col++)
            {
                var h = 1e-7 * (1.0 + Math.Abs(x[col]));
                var xp = (double[])x.Clone();
                xp[col] += h;
                var rp = r(DenseVector.OfArray(xp));
                for (var i = 0; i < m; i++) jac[i, col] = (rp[i] - rx[i]) / h;
            }
            // normale: (J^T J + λI) d = -J^T r
            var jtj = new double[n, n];
            var jtr = new double[n];
            for (var i = 0; i < n; i++)
            {
                for (var j = 0; j < n; j++)
                {
                    double s = 0;
                    for (var k = 0; k < m; k++) s += jac[k, i] * jac[k, j];
                    jtj[i, j] = s;
                }
                jtj[i, i] += lambda;
                for (var k = 0; k < m; k++) jtr[i] += jac[k, i] * rx[k];
            }
            var rhs = new double[n];
            for (var i = 0; i < n; i++) rhs[i] = -jtr[i];
            var d = SolveLinear(jtj, rhs);
            if (d == null) break;
            // step adattivo (accept se riduce costo)
            var xNew = new double[n];
            for (var i = 0; i < n; i++) xNew[i] = x[i] + d[i];
            ClampTo(xNew, bounds);
            var rxN = r(DenseVector.OfArray(xNew));
            double newCost = 0;
            for (var i = 0; i < m; i++) newCost += rxN[i] * rxN[i];
            if (newCost < cost) { x = xNew; lambda *= 0.3; }
            else lambda *= 2.0;
            if (lambda > 1e12) break;
        }
        return x;
    }

    private static void TryLocal(Func<Vector<double>, double> f, Vector<double> start, ref double[] best, ref double bestF)
    {
        try
        {
            var local = NelderMead(f, start, 1e-7, 200);
            var lv = local.FunctionInfoAtMinimum.Value;
            if (lv < bestF) { bestF = lv; best = local.MinimizingPoint.ToArray(); }
        }
        catch (Exception) { /* visita locale best-effort */ }
    }

    // ========================================================================
    // linprog (scipy.optimize.linprog) — two-phase simplex
    // ========================================================================

    /// <summary>scipy.optimize.linprog — min c·x s.t. A_ub x ≤ b_ub, A_eq x = b_eq, bounds.</summary>
    public static double[] LinProg(double[] c, double[][]? aUb, double[]? bUb, double[][]? aEq, double[]? bEq, double[,]? bounds)
    {
        var n = c.Length;
        var lo = new double[n];
        var hi = new double[n];
        for (var i = 0; i < n; i++)
        {
            if (bounds != null && i < bounds.GetLength(0)) { lo[i] = bounds[i, 0]; hi[i] = bounds[i, 1]; }
            else { lo[i] = 0; hi[i] = double.PositiveInfinity; }
        }
        // variabile shift: x = lo + y  (y ≥ 0)
        var cEff = new double[n];
        for (var i = 0; i < n; i++) cEff[i] = c[i];
        var aUbRows = aUb ?? Array.Empty<double[]>();
        var bUbEff = (bUb ?? Array.Empty<double>()).ToArray();
        var aEqRows = aEq ?? Array.Empty<double[]>();
        var bEqEff = (bEq ?? Array.Empty<double>()).ToArray();
        var ub = new double[bUbEff.Length][];
        var eq = new double[bEqEff.Length][];
        for (var i = 0; i < ub.Length; i++)
        {
            ub[i] = new double[n];
            for (var j = 0; j < n; j++) ub[i][j] = aUbRows[i][j];
        }
        for (var i = 0; i < eq.Length; i++)
        {
            eq[i] = new double[n];
            for (var j = 0; j < n; j++) eq[i][j] = aEqRows[i][j];
        }
        // tableau two-phase: [A_ub | I | A_eq] con variabili artificiali per A_eq e slack per A_ub
        var mUb = ub.Length;
        var mEq = eq.Length;
        var rows = mUb + mEq;
        var cols = n + mUb + mEq + 1; // variabili y, slack, artificiali, rhs
        var tab = new double[rows, cols];
        var basis = new int[rows];
        for (var i = 0; i < mUb; i++)
        {
            for (var j = 0; j < n; j++) tab[i, j] = ub[i][j];
            tab[i, n + i] = 1; // slack
            tab[i, cols - 1] = bUbEff[i];
            basis[i] = n + i;
        }
        for (var i = 0; i < mEq; i++)
        {
            var r = mUb + i;
            for (var j = 0; j < n; j++) tab[r, j] = eq[i][j];
            tab[r, n + mUb + i] = 1; // artificiale
            tab[r, cols - 1] = bEqEff[i];
            basis[r] = n + mUb + i;
        }
        // normalizza rhs ≥ 0
        for (var r = 0; r < rows; r++)
            if (tab[r, cols - 1] < 0)
            {
                for (var j = 0; j < cols - 1; j++) tab[r, j] = -tab[r, j];
                tab[r, cols - 1] = -tab[r, cols - 1];
            }
        // FASE 1: minimizza somma artificiali
        var artStart = n + mUb;
        for (var r = 0; r < rows; r++)
        {
            if (basis[r] >= artStart)
            {
                var row = r;
                for (var j = 0; j < cols - 1; j++) tab[row, j] = -tab[row, j];
            }
        }
        // costi fase 1: -Σ righe con artificiali in base
        var costRow = new double[cols - 1];
        for (var r = 0; r < rows; r++)
            if (basis[r] >= artStart)
                for (var j = 0; j < cols - 1; j++) costRow[j] -= tab[r, j];
        // Pivot finché tutti i costi ridotti ≥ 0
        for (var it = 0; it < 2000; it++)
        {
            var entering = -1;
            var minCost = -1e-9;
            for (var j = 0; j < cols - 1; j++)
                if (costRow[j] < minCost) { minCost = costRow[j]; entering = j; }
            if (entering < 0) break;
            // ratio test
            var leaving = -1;
            var bestRatio = double.PositiveInfinity;
            for (var r = 0; r < rows; r++)
            {
                if (tab[r, entering] > 1e-12)
                {
                    var ratio = tab[r, cols - 1] / tab[r, entering];
                    if (ratio < bestRatio) { bestRatio = ratio; leaving = r; }
                }
            }
            if (leaving < 0) return new double[n]; // illimitato
            Pivot(tab, costRow, rows, cols, leaving, entering);
            basis[leaving] = entering;
        }
        // rimuovi artificiali in base: se una artificiale è in base con rhs>0 → infeasible
        for (var r = 0; r < rows; r++)
            if (basis[r] >= artStart && Math.Abs(tab[r, cols - 1]) > 1e-8)
                return new double[n]; // infeasible
        // FASE 2: costi originali c
        var cost2 = new double[cols - 1];
        for (var j = 0; j < n; j++) cost2[j] = cEff[j];
        for (var j = n; j < n + mUb; j++) cost2[j] = 0; // slack
        for (var j = n + mUb; j < cols - 1; j++) cost2[j] = 1e9; // artificiali fuori base
        for (var r = 0; r < rows; r++)
            if (basis[r] < n)
            {
                var br = basis[r];
                var coef = cost2[br];
                for (var j = 0; j < cols - 1; j++) cost2[j] -= coef * tab[r, j];
                cost2[br] = 0;
            }
        for (var it = 0; it < 2000; it++)
        {
            var entering = -1;
            var minCost = -1e-9;
            for (var j = 0; j < cols - 1; j++)
                if (cost2[j] < minCost && j < n + mUb) { minCost = cost2[j]; entering = j; }
            if (entering < 0) break;
            var leaving = -1;
            var bestRatio = double.PositiveInfinity;
            for (var r = 0; r < rows; r++)
            {
                if (tab[r, entering] > 1e-12)
                {
                    var ratio = tab[r, cols - 1] / tab[r, entering];
                    if (ratio < bestRatio) { bestRatio = ratio; leaving = r; }
                }
            }
            if (leaving < 0) break; // illimitato (best-effort)
            Pivot(tab, cost2, rows, cols, leaving, entering);
            basis[leaving] = entering;
        }
        var y = new double[n];
        for (var r = 0; r < rows; r++)
            if (basis[r] < n) y[basis[r]] = tab[r, cols - 1];
        var x = new double[n];
        for (var i = 0; i < n; i++)
        {
            x[i] = lo[i] + Math.Clamp(y[i], 0, hi[i] == double.PositiveInfinity ? y[i] : hi[i] - lo[i]);
            if (hi[i] != double.PositiveInfinity) x[i] = Math.Min(x[i], hi[i]);
        }
        return x;
    }

    private static void Pivot(double[,] tab, double[] cost, int rows, int cols, int leaving, int entering)
    {
        var piv = tab[leaving, entering];
        for (var j = 0; j < cols; j++) tab[leaving, j] /= piv;
        for (var r = 0; r < rows; r++)
        {
            if (r == leaving) continue;
            var f = tab[r, entering];
            if (Math.Abs(f) < 1e-15) continue;
            for (var j = 0; j < cols; j++) tab[r, j] -= f * tab[leaving, j];
        }
        var cf = cost[entering];
        for (var j = 0; j < cols - 1; j++) cost[j] -= cf * tab[leaving, j];
        cost[entering] = 0;
    }
}
