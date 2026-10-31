// FILE: StdLib.Numerics.SignalFilters.cs
// ReSharper disable CheckNamespace

using System.Numerics;

namespace CommonLib;

/// <summary>Filtro IIR in forma (b, a) — risultato del design scipy.signal.</summary>
public sealed class SignalFilter
{
    public double[] B { get; init; } = [1.0];
    public double[] A { get; init; } = [1.0];
}

/// <summary>
/// Design e applicazione di filtri IIR con parità a scipy.signal (iirfilter):
/// Butterworth, Chebyshev I/II e ellittico (prototipo analogico + trasformazioni di
/// banda + bilineare), più lfilter (direct form II transposed), filtfilt (zero-phase
/// con padding odd di scipy) e freqz (risposta in ampiezza).
/// </summary>
public static partial class Numerics
{
    // ============================ design (parity scipy) ============================

    public static SignalFilter Butter(int n, double[] wn, string btype)
        => IirFilterBa(n, wn, btype, ButterworthZpk);

    public static SignalFilter Cheby1(int n, double rp, double[] wn, string btype)
        => IirFilterBa(n, wn, btype, nn => Cheby1Zpk(nn, rp));

    public static SignalFilter Cheby2(int n, double rs, double[] wn, string btype)
        => IirFilterBa(n, wn, btype, nn => Cheby2Zpk(nn, rs));

    public static SignalFilter Ellip(int n, double rp, double rs, double[] wn, string btype)
        => IirFilterBa(n, wn, btype, nn => EllipZpk(nn, rp, rs));

    private static SignalFilter IirFilterBa(int n, double[] wn, string btype,
        Func<int, (Complex[] z, Complex[] p, double k)> prototype)
    {
        var (z, p, k) = prototype(n);
        const double fs = 2.0;
        var warped = new double[wn.Length];
        for (var i = 0; i < wn.Length; i++) warped[i] = 2 * fs * Math.Tan(Math.PI * wn[i] / fs);

        var b = btype.ToLowerInvariant();
        switch (b)
        {
            case "low" or "lowpass":
                (z, p, k) = Lp2Lp(z, p, k, warped[0]);
                break;
            case "high" or "highpass":
                (z, p, k) = Lp2Hp(z, p, k, warped[0]);
                break;
            case "band" or "bandpass":
            case "bandstop" or "stop":
                var bw = warped[1] - warped[0];
                var wo = Math.Sqrt(warped[0] * warped[1]);
                (z, p, k) = b is "band" or "bandpass" ? Lp2Bp(z, p, k, wo, bw) : Lp2Bs(z, p, k, wo, bw);
                break;
            default:
                throw new ArgumentException($"btype non supportato: {btype}");
        }
        (z, p, k) = BilinearZpk(z, p, k, fs);
        var (bb, aa) = Zpk2Ba(z, p, k);
        return new SignalFilter { B = bb, A = aa };
    }

    private static int RelativeDegree(Complex[] z, Complex[] p) => p.Length - z.Length;

    private static Complex ProductNeg(Complex[] v)
    {
        var r = Complex.One;
        foreach (var x in v) r *= -x;
        return r;
    }

    // --- prototipi analogici (scipy buttap/cheb1ap/cheb2ap/ellipap) ---

    private static (Complex[] z, Complex[] p, double k) ButterworthZpk(int n)
    {
        var p = new Complex[n];
        for (var i = 0; i < n; i++)
        {
            var m = -n + 1 + 2 * i;
            p[i] = -Complex.Exp(new Complex(0, Math.PI * m / (2.0 * n)));
        }
        return (Array.Empty<Complex>(), p, 1.0);
    }

    private static (Complex[] z, Complex[] p, double k) Cheby1Zpk(int n, double rp)
    {
        if (n == 0) return (Array.Empty<Complex>(), Array.Empty<Complex>(), Math.Pow(10, -rp / 20.0));
        var eps = Math.Sqrt(Math.Pow(10, 0.1 * rp) - 1.0);
        var mu = (1.0 / n) * Asinh(1.0 / eps);
        var p = new Complex[n];
        for (var i = 0; i < n; i++)
        {
            var m = -n + 1 + 2 * i;
            var theta = Math.PI * m / (2.0 * n);
            p[i] = -Complex.Sinh(new Complex(mu, theta));
        }
        var k = ProductNeg(p).Real;
        if (n % 2 == 0) k = k / Math.Sqrt(1.0 + eps * eps);
        return (Array.Empty<Complex>(), p, k);
    }

    private static (Complex[] z, Complex[] p, double k) Cheby2Zpk(int n, double rs)
    {
        if (n == 0) return (Array.Empty<Complex>(), Array.Empty<Complex>(), 1.0);
        var de = 1.0 / Math.Sqrt(Math.Pow(10, 0.1 * rs) - 1.0);
        var mu = Asinh(1.0 / de) / n;
        var m = new double[n];
        int cnt = 0;
        if (n % 2 == 1)
        {
            for (var v = -n + 1; v < 0; v += 2) m[cnt++] = v;
            for (var v = 2; v < n; v += 2) m[cnt++] = v;
        }
        else
        {
            for (var i = 0; i < n; i++) m[i] = -n + 1 + 2 * i;
            cnt = n;
        }
        Array.Resize(ref m, cnt);
        var z = new Complex[m.Length];
        for (var i = 0; i < m.Length; i++) z[i] = Complex.ImaginaryOne / Complex.Sin(m[i] * Math.PI / (2.0 * n));
        var p = new Complex[n];
        for (var i = 0; i < n; i++)
        {
            var m1 = -n + 1 + 2 * i;
            var th = Math.PI * m1 / (2.0 * n);
            p[i] = -1.0 / Complex.Sinh(new Complex(mu, th));
        }
        var k = (ProductNeg(p) / ProductNeg(z)).Real;
        return (z, p, k);
    }

    private static (Complex[] z, Complex[] p, double k) EllipZpk(int n, double rp, double rs)
    {
        if (n == 0) return (Array.Empty<Complex>(), Array.Empty<Complex>(), Math.Pow(10, -rp / 20.0));
        if (n == 1)
        {
            var p1 = -Math.Sqrt(1.0 / (Math.Pow(10, 0.1 * rp) - 1.0));
            return (Array.Empty<Complex>(), new[] { new Complex(p1, 0) }, -p1);
        }
        var epsSq = Math.Pow(10, 0.1 * rp) - 1.0;
        var eps = Math.Sqrt(epsSq);
        var ck1Sq = epsSq / (Math.Pow(10, 0.1 * rs) - 1.0);
        if (ck1Sq == 0) throw new ArgumentException("Impossibile progettare il filtro con rp/rs dati.");

        var val0 = EllipticK(ck1Sq);
        var m = EllipDeg(n, ck1Sq);
        var capk = EllipticK(m);

        var jc = new double[n - (n % 2 == 1 ? 1 : 0)];
        for (var i = 0; i < jc.Length; i++) jc[i] = 1 - (n % 2) + 2 * i;

        var snew = new List<double>();
        foreach (var j in jc)
        {
            var sn = EllipSn(j * capk / n, m);
            if (Math.Abs(sn) > 2.2e-16) snew.Add(sn);
        }
        var zList = new List<Complex>();
        foreach (var s in snew)
        {
            var zv = Complex.ImaginaryOne / (Math.Sqrt(m) * s);
            zList.Add(zv);
            zList.Add(Complex.Conjugate(zv));
        }

        var r = ArcJacSc1(1.0 / eps, ck1Sq);
        var v0 = capk * r / (n * val0);

        EllipSnCnDn(v0, 1.0 - m, out var sv, out var cv, out var dv, out _);
        var pList = new List<Complex>();
        foreach (var j in jc)
        {
            EllipSnCnDn(j * capk / n, m, out var sj, out var cj, out var dj, out _);
            var denom = 1.0 - Math.Pow(dj * sv, 2.0);
            var num = -(cj * dj * sv * cv + Complex.ImaginaryOne * sj * dv);
            pList.Add(num / denom);
        }
        var pA = pList.ToArray();
        if (n % 2 == 1)
        {
            var norm = Math.Sqrt(pA.Sum(x => x.Real * x.Real + x.Imaginary * x.Imaginary));
            foreach (var pv in pA)
                if (Math.Abs(pv.Imaginary) > 2.2e-16 * norm)
                    pList.Add(Complex.Conjugate(pv));
        }
        else
        {
            foreach (var pv in pA) pList.Add(Complex.Conjugate(pv));
        }

        var k = (ProductNeg(pList.ToArray()) / ProductNeg(zList.ToArray())).Real;
        if (n % 2 == 0) k = k / Math.Sqrt(1.0 + epsSq);
        return (zList.ToArray(), pList.ToArray(), k);
    }

    // --- trasformazioni di banda (scipy lp2lp/lp2hp/lp2bp/lp2bs_zpk) ---

    private static (Complex[] z, Complex[] p, double k) Lp2Lp(Complex[] z, Complex[] p, double k, double wo)
    {
        var zl = new Complex[z.Length];
        var pl = new Complex[p.Length];
        for (var i = 0; i < z.Length; i++) zl[i] = wo * z[i];
        for (var i = 0; i < p.Length; i++) pl[i] = wo * p[i];
        return (zl, pl, k * Math.Pow(wo, RelativeDegree(z, p)));
    }

    private static (Complex[] z, Complex[] p, double k) Lp2Hp(Complex[] z, Complex[] p, double k, double wo)
    {
        var zh = new Complex[z.Length];
        var ph = new Complex[p.Length];
        for (var i = 0; i < z.Length; i++) zh[i] = wo / z[i];
        for (var i = 0; i < p.Length; i++) ph[i] = wo / p[i];
        var deg = RelativeDegree(z, p);
        var zNew = new Complex[z.Length + deg];
        Array.Copy(zh, zNew, z.Length);
        for (var i = 0; i < deg; i++) zNew[z.Length + i] = Complex.Zero;
        var kNew = k * (ProductNeg(z) / ProductNeg(p)).Real;
        return (zNew, ph, kNew);
    }

    private static (Complex[] z, Complex[] p, double k) Lp2Bp(Complex[] z, Complex[] p, double k, double wo, double bw)
    {
        var zBp = new List<Complex>();
        foreach (var zv in z)
        {
            var zl = zv * bw / 2.0;
            var s = Complex.Sqrt(zl * zl - wo * wo);
            zBp.Add(zl + s);
            zBp.Add(zl - s);
        }
        var pBp = new List<Complex>();
        foreach (var pv in p)
        {
            var pl = pv * bw / 2.0;
            var s = Complex.Sqrt(pl * pl - wo * wo);
            pBp.Add(pl + s);
            pBp.Add(pl - s);
        }
        var deg = RelativeDegree(z, p);
        for (var i = 0; i < deg; i++) zBp.Add(Complex.Zero);
        return (zBp.ToArray(), pBp.ToArray(), k * Math.Pow(bw, deg));
    }

    private static (Complex[] z, Complex[] p, double k) Lp2Bs(Complex[] z, Complex[] p, double k, double wo, double bw)
    {
        var zBs = new List<Complex>();
        foreach (var zv in z)
        {
            var zh = (bw / 2.0) / zv;
            var s = Complex.Sqrt(zh * zh - wo * wo);
            zBs.Add(zh + s);
            zBs.Add(zh - s);
        }
        var pBs = new List<Complex>();
        foreach (var pv in p)
        {
            var ph = (bw / 2.0) / pv;
            var s = Complex.Sqrt(ph * ph - wo * wo);
            pBs.Add(ph + s);
            pBs.Add(ph - s);
        }
        var deg = RelativeDegree(z, p);
        for (var i = 0; i < deg; i++)
        {
            zBs.Add(new Complex(0, wo));
            zBs.Add(new Complex(0, -wo));
        }
        var kNew = k * (ProductNeg(z) / ProductNeg(p)).Real;
        return (zBs.ToArray(), pBs.ToArray(), kNew);
    }

    // --- bilineare + zpk→(b,a) (scipy bilinear_zpk / zpk2tf) ---

    private static (Complex[] z, Complex[] p, double k) BilinearZpk(Complex[] z, Complex[] p, double k, double fs)
    {
        var fs2 = 2.0 * fs;
        var zz = new Complex[z.Length];
        var pz = new Complex[p.Length];
        for (var i = 0; i < z.Length; i++) zz[i] = (fs2 + z[i]) / (fs2 - z[i]);
        for (var i = 0; i < p.Length; i++) pz[i] = (fs2 + p[i]) / (fs2 - p[i]);
        var deg = RelativeDegree(z, p);
        var zNew = new Complex[z.Length + deg];
        Array.Copy(zz, zNew, z.Length);
        for (var i = 0; i < deg; i++) zNew[z.Length + i] = -1.0;
        var num = Complex.One;
        foreach (var zv in z) num *= fs2 - zv;
        var den = Complex.One;
        foreach (var pv in p) den *= fs2 - pv;
        var kNew = k * (num / den).Real;
        return (zNew, pz, kNew);
    }

    private static (double[] b, double[] a) Zpk2Ba(Complex[] z, Complex[] p, double k)
    {
        var b = PolyFromRoots(z);
        var a = PolyFromRoots(p);
        for (var i = 0; i < b.Length; i++) b[i] *= k;
        return (b, a);
    }

    /// <summary>Coefficienti del polinomio monico con le radici date (np.poly), parte reale.</summary>
    private static double[] PolyFromRoots(Complex[] roots)
    {
        var coeff = new Complex[] { Complex.One };
        foreach (var r in roots)
        {
            var next = new Complex[coeff.Length + 1];
            for (var i = 0; i < coeff.Length; i++)
            {
                next[i] += coeff[i];
                next[i + 1] -= r * coeff[i];
            }
            coeff = next;
        }
        var res = new double[coeff.Length];
        for (var i = 0; i < coeff.Length; i++) res[i] = coeff[i].Real;
        return res;
    }

    // ============================ elliptic functions ============================

    private static double Asinh(double x) => Math.Log(x + Math.Sqrt(x * x + 1.0));

    /// <summary>Integrale ellittico completo K(m) (scipy special.ellipk) via AGM.</summary>
    public static double EllipticK(double m)
    {
        if (m >= 1) return double.PositiveInfinity;
        if (m <= 0) return Math.PI / 2.0;
        var a = 1.0;
        var b = Math.Sqrt(1.0 - m);
        for (var i = 0; i < 40; i++)
        {
            var an = (a + b) / 2.0;
            b = Math.Sqrt(a * b);
            a = an;
            if (Math.Abs(a - b) < 1e-15 * a) break;
        }
        return Math.PI / (2.0 * a);
    }

    public static double EllipSn(double u, double m)
    {
        EllipSnCnDn(u, m, out var sn, out _, out _, out _);
        return sn;
    }

    /// <summary>sn/cn/dn di Jacobi via trasformazione di Landen discendente (scipy
    /// special.ellipj per u reale in [0,K]). cn/dn dalle relazioni cn²=1-sn², dn²=1-m·sn².</summary>
    public static void EllipSnCnDn(double u, double m, out double sn, out double cn, out double dn, out double ph)
    {
        if (m <= 0) { ph = u; sn = Math.Sin(u); cn = Math.Cos(u); dn = 1.0; return; }
        if (m >= 1) { ph = u; sn = Math.Tanh(u); cn = 1.0 / Math.Cosh(u); dn = 1.0 / Math.Cosh(u); return; }
        var kk = Math.Sqrt(m);
        var uu = u;
        var ks = new List<double>();
        while (kk > 1e-14)
        {
            var kp = Math.Sqrt((1.0 - kk) * (1.0 + kk));
            var kk2 = (1.0 - kp) / (1.0 + kp);
            uu = uu / (1.0 + kk2);
            ks.Add(kk2);
            kk = kk2;
        }
        var s = Math.Sin(uu);
        for (var i = ks.Count - 1; i >= 0; i--)
        {
            var k1 = ks[i];
            s = (1.0 + k1) * s / (1.0 + k1 * s * s);
        }
        ph = Math.Asin(s);
        sn = s;
        cn = Math.Sqrt(Math.Max(0, 1.0 - s * s));
        dn = Math.Sqrt(Math.Max(0, 1.0 - m * s * s));
    }

    // --- inversa sn via Landen (scipy _arc_jac_sn, argomento complesso) ---

    private static Complex ArcJacSn(Complex w, double m)
    {
        var k = Math.Sqrt(m);
        if (k > 1) return Complex.NaN;
        if (k == 1) return 0.5 * Complex.Log((Complex.One + w) / (Complex.One - w));
        var ks = new List<double> { k };
        var niter = 0;
        while (ks[^1] != 0)
        {
            var kk = ks[^1];
            var kp = Math.Sqrt((1 - kk) * (1 + kk));
            ks.Add((1 - kp) / (1 + kp));
            niter++;
            if (niter > 1000) throw new InvalidOperationException("Landen non converge");
        }
        var K = Math.PI / 2.0;
        for (var i = 1; i < ks.Count; i++) K *= 1.0 + ks[i];
        var wns = new List<Complex> { w };
        for (var i = 0; i < ks.Count - 1; i++)
        {
            var kn = ks[i];
            var knext = ks[i + 1];
            var wn0 = wns[^1];
            var wnext = 2.0 * wn0 / ((1.0 + knext) * (1.0 + Complex.Sqrt((1.0 - kn * wn0) * (1.0 + kn * wn0))));
            wns.Add(wnext);
        }
        var u = 2.0 / Math.PI * (-Complex.ImaginaryOne * Complex.Log(Complex.ImaginaryOne * wns[^1] + Complex.Sqrt(Complex.One - wns[^1] * wns[^1])));
        return K * u;
    }

    /// <summary>scipy _arc_jac_sc1(w, m): z reale tale che w = sc(z, 1-m).</summary>
    private static double ArcJacSc1(double w, double m)
        => ArcJacSn(Complex.ImaginaryOne * w, m).Imaginary;

    private static double EllipDeg(int n, double m1)
    {
        var K1 = EllipticK(m1);
        var K1p = EllipticK(1.0 - m1);
        var q1 = Math.Exp(-Math.PI * K1p / K1);
        var q = Math.Pow(q1, 1.0 / n);
        double num = 0;
        for (var mn = 0; mn <= 7; mn++) num += Math.Pow(q, mn * (mn + 1));
        double den = 1;
        for (var md = 1; md <= 8; md++) den += 2 * Math.Pow(q, md * md);
        return 16.0 * q * Math.Pow(num / den, 4);
    }

    // ============================ applicazione ============================

    /// <summary>Filtro causale direct form II transposed (scipy lfilter, ba).</summary>
    public static double[] LFilter(SignalFilter f, double[] x)
        => LFilterCore(f.B, f.A, x, null);

    /// <summary>Stato stazionario per passo unitario (scipy lfilter_zi).</summary>
    private static double[] LFilterZi(double[] b, double[] a)
    {
        var n = Math.Max(a.Length, b.Length);
        var bp = new double[n];
        var ap = new double[n];
        Array.Copy(b, bp, b.Length);
        Array.Copy(a, ap, a.Length);
        var yInf = bp.Sum() / ap.Sum();
        var t = new double[n];
        for (var i = 0; i < n; i++) t[i] = bp[i] - yInf * ap[i];
        Array.Reverse(t);
        var cs = new double[n];
        cs[0] = t[0];
        for (var i = 1; i < n; i++) cs[i] = cs[i - 1] + t[i];
        Array.Reverse(cs);
        var zi = new double[n - 1];
        for (var i = 0; i < n - 1; i++) zi[i] = cs[i + 1];
        return zi;
    }

    private static double[] LFilterCore(double[] b, double[] a, double[] x, double[]? zi)
    {
        var M = Math.Max(b.Length, a.Length);
        var bp = new double[M];
        var ap = new double[M];
        Array.Copy(b, bp, b.Length);
        Array.Copy(a, ap, a.Length);
        var z = new double[M - 1];
        if (zi != null) for (var i = 0; i < M - 1 && i < zi.Length; i++) z[i] = zi[i];
        var y = new double[x.Length];
        for (var t = 0; t < x.Length; t++)
        {
            var xv = x[t];
            var yv = bp[0] * xv + z[0];
            for (var i = 1; i < M; i++)
            {
                var zi2 = i < M - 1 ? z[i] : 0.0;
                z[i - 1] = bp[i] * xv - ap[i] * yv + zi2;
            }
            y[t] = yv;
        }
        return y;
    }

    /// <summary>Filtraggio zero-phase (scipy filtfilt, method='pad'): estende ai bordi con
    /// padding odd, filtra avanti/indietro con lo stato stazionario lfilter_zi, taglia il
    /// padding.</summary>
    public static double[] FiltFilt(SignalFilter f, double[] x)
    {
        if (x.Length == 0) return Array.Empty<double>();
        var ntaps = Math.Max(f.B.Length, f.A.Length);
        var padlen = 3 * ntaps;
        if (padlen >= x.Length) padlen = Math.Max(0, x.Length - 1);
        var n = x.Length + 2 * padlen;
        var ext = new double[n];
        for (var i = 0; i < padlen; i++) ext[i] = 2 * x[0] - x[padlen - i];
        Array.Copy(x, 0, ext, padlen, x.Length);
        for (var i = 0; i < padlen; i++) ext[padlen + x.Length + i] = 2 * x[x.Length - 1] - x[x.Length - 2 - i];

        var zi = LFilterZi(f.B, f.A);
        var x0 = ext[0];
        var zix0 = new double[zi.Length];
        for (var i = 0; i < zi.Length; i++) zix0[i] = zi[i] * x0;
        var y1 = LFilterCore(f.B, f.A, ext, zix0);
        var y0 = y1[y1.Length - 1];
        var ziy0 = new double[zi.Length];
        for (var i = 0; i < zi.Length; i++) ziy0[i] = zi[i] * y0;
        var y2 = LFilterCore(f.B, f.A, ReverseCopy(y1), ziy0);
        Array.Reverse(y2);
        var res = new double[x.Length];
        Array.Copy(y2, padlen, res, 0, x.Length);
        return res;
    }

    private static double[] ReverseCopy(double[] v)
    {
        var r = (double[])v.Clone();
        Array.Reverse(r);
        return r;
    }

    /// <summary>Risposta in ampiezza |H(e^{jw})| a n punti w=0..π (endpoint escluso), come freqz.</summary>
    public static double[] Freqz(SignalFilter f, int n)
    {
        var res = new double[n];
        for (var i = 0; i < n; i++)
        {
            var w = Math.PI * i / n;
            var num = Complex.Zero;
            for (var k = 0; k < f.B.Length; k++) num += f.B[k] * Complex.Exp(new Complex(0, -w * k));
            var den = Complex.Zero;
            for (var k = 0; k < f.A.Length; k++) den += f.A[k] * Complex.Exp(new Complex(0, -w * k));
            res[i] = (num / den).Magnitude;
        }
        return res;
    }
}
