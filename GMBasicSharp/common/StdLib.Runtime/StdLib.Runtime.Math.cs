// FILE: StdLib.Runtime.Math.cs
// Dominio Math della StdLib.Runtime: funzioni pure di statistica/probabilita'.
// PURO per costruzione (vedi Ast.NativeExecutionKind): nessun metodo qui accede a stato che
// sopravvive oltre la singola chiamata. Rispecchia esattamente le firme di CommonLib.Numerics
// (nessun riordino argomenti — la convenzione di chiamata BASIC, es. "x per primo", e' un
// dettaglio dell'adapter del motore che chiama questi metodi, non di questa libreria).
//
// Nome MathFunctions, non Math: un consumer con ImplicitUsings (quindi "global using System;")
// avrebbe un riferimento ambiguo fra questa classe e System.Math ad ogni chiamata non qualificata
// — scoperto scrivendo il primo programma di verifica reale, non a tavolino (vedi
// GMBasic-PCode-Testing-Bug-Hunting-Methodology per il principio "scrivi il primo sample
// end-to-end", qui applicato al codice invece che a un test .bas).
//
// Namespace Basic.StdLibRuntime (NON Basic.StdLib.Runtime): "Basic.StdLib" è già il nome
// completo della classe StdLib esistente (namespace Basic, classe StdLib — dichiarazioni
// NativeProcedureDefinition). Un namespace annidato "Basic.StdLib.Runtime" confliggerebbe con
// quel TIPO appena qualunque altro progetto (qui: Interpreter) referenzia entrambi gli assembly
// insieme — scoperto alla Fase 2 (wiring nella VM), quando PCodeVm ha iniziato a referenziare
// StdLib.Runtime accanto a StdLib: 25 errori CS0434 "il namespace confligge con il tipo" in
// tutto Interpreter/, ovunque il codice scrivesse "StdLib.Qualcosa" aspettandosi la classe.

using System.Runtime.CompilerServices;

namespace Basic.StdLibRuntime;

// Fase 9 del piano (docs/StdLib-Runtime-Plan.md): IlBackend emette una `Call` IL diretta verso
// questi metodi invece di duplicarli in IlRuntime.cs. Ognuno è un forward di una riga a
// CommonLib — AggressiveInlining garantisce che il JIT lo elimini, quindi la Call generata da
// IL costa esattamente quanto una chiamata diretta a CommonLib, zero overhead misurabile.
public static class MathFunctions
{
    /// <summary>
    /// Media aritmetica. <c>params</c> gestisce nativamente sia un array pre-costruito
    /// (<c>Avg(myArray)</c>) sia argomenti individuali (<c>Avg(1, 2, 3)</c>) — la stessa
    /// ambiguita' che la sintassi BASIC <c>AVG(...)</c> deve risolvere, qui espressa con un
    /// costrutto C# standard, senza bisogno di ispezionare il tipo runtime degli argomenti.
    /// </summary>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static double Avg(params double[] values) => values.Length == 0 ? 0.0 : System.Linq.Enumerable.Average(values);

    [MethodImpl(MethodImplOptions.AggressiveInlining)] public static double NormalPdf(double mean, double stdDev, double x) => CommonLib.Numerics.NormalPdf(mean, stdDev, x);
    [MethodImpl(MethodImplOptions.AggressiveInlining)] public static double NormalCdf(double mean, double stdDev, double x) => CommonLib.Numerics.NormalCdf(mean, stdDev, x);
    [MethodImpl(MethodImplOptions.AggressiveInlining)] public static double LognormalPdf(double mu, double sigma, double x) => CommonLib.Numerics.LognormalPdf(mu, sigma, x);
    [MethodImpl(MethodImplOptions.AggressiveInlining)] public static double LognormalCdf(double mu, double sigma, double x) => CommonLib.Numerics.LognormalCdf(mu, sigma, x);
    [MethodImpl(MethodImplOptions.AggressiveInlining)] public static double ChiPdf(double freedom, double x) => CommonLib.Numerics.ChiPdf(freedom, x);
    [MethodImpl(MethodImplOptions.AggressiveInlining)] public static double ChiCdf(double freedom, double x) => CommonLib.Numerics.ChiCdf(freedom, x);
    [MethodImpl(MethodImplOptions.AggressiveInlining)] public static double ChiSquaredPdf(double freedom, double x) => CommonLib.Numerics.ChiSquaredPdf(freedom, x);
    [MethodImpl(MethodImplOptions.AggressiveInlining)] public static double ChiSquaredCdf(double freedom, double x) => CommonLib.Numerics.ChiSquaredCdf(freedom, x);
    [MethodImpl(MethodImplOptions.AggressiveInlining)] public static double BetaPdf(double a, double b, double x) => CommonLib.Numerics.BetaPdf(a, b, x);
    [MethodImpl(MethodImplOptions.AggressiveInlining)] public static double BetaCdf(double a, double b, double x) => CommonLib.Numerics.BetaCdf(a, b, x);
    [MethodImpl(MethodImplOptions.AggressiveInlining)] public static double StudentTPdf(double freedom, double x) => CommonLib.Numerics.StudentTPdf(freedom, x);
    [MethodImpl(MethodImplOptions.AggressiveInlining)] public static double StudentTCdf(double freedom, double x) => CommonLib.Numerics.StudentTCdf(freedom, x);
    [MethodImpl(MethodImplOptions.AggressiveInlining)] public static double WeibullPdf(double shape, double scale, double x) => CommonLib.Numerics.WeibullPdf(shape, scale, x);
    [MethodImpl(MethodImplOptions.AggressiveInlining)] public static double WeibullCdf(double shape, double scale, double x) => CommonLib.Numerics.WeibullCdf(shape, scale, x);
    [MethodImpl(MethodImplOptions.AggressiveInlining)] public static double FPdf(double d1, double d2, double x) => CommonLib.Numerics.FPdf(d1, d2, x);
    [MethodImpl(MethodImplOptions.AggressiveInlining)] public static double FCdf(double d1, double d2, double x) => CommonLib.Numerics.FCdf(d1, d2, x);
    [MethodImpl(MethodImplOptions.AggressiveInlining)] public static double ExponentialPdf(double rate, double x) => CommonLib.Numerics.ExponentialPdf(rate, x);
    [MethodImpl(MethodImplOptions.AggressiveInlining)] public static double ExponentialCdf(double rate, double x) => CommonLib.Numerics.ExponentialCdf(rate, x);
    // Inverse CDF (ppf) e ISF — oracoli scipy.stats 1.18.
    [MethodImpl(MethodImplOptions.AggressiveInlining)] public static double NormalPpf(double mean, double stdDev, double p) => CommonLib.Numerics.NormalPpf(p, mean, stdDev);
    [MethodImpl(MethodImplOptions.AggressiveInlining)] public static double NormalIsf(double mean, double stdDev, double p) => CommonLib.Numerics.NormalIsf(p, mean, stdDev);
    [MethodImpl(MethodImplOptions.AggressiveInlining)] public static double LognormalPpf(double mu, double sigma, double p) => CommonLib.Numerics.LognormalPpf(p, mu, sigma);
    [MethodImpl(MethodImplOptions.AggressiveInlining)] public static double ChiSquaredPpf(double freedom, double p) => CommonLib.Numerics.ChiSquaredPpf(p, freedom);
    [MethodImpl(MethodImplOptions.AggressiveInlining)] public static double ChiSquaredIsf(double freedom, double p) => CommonLib.Numerics.ChiSquaredIsf(p, freedom);
    [MethodImpl(MethodImplOptions.AggressiveInlining)] public static double ChiPpf(double freedom, double p) => CommonLib.Numerics.ChiPpf(p, freedom);
    [MethodImpl(MethodImplOptions.AggressiveInlining)] public static double BetaPpf(double a, double b, double p) => CommonLib.Numerics.BetaPpf(p, a, b);
    [MethodImpl(MethodImplOptions.AggressiveInlining)] public static double FPpf(double d1, double d2, double p) => CommonLib.Numerics.FPpf(p, d1, d2);
    [MethodImpl(MethodImplOptions.AggressiveInlining)] public static double FIsf(double d1, double d2, double p) => CommonLib.Numerics.FIsf(p, d1, d2);
    [MethodImpl(MethodImplOptions.AggressiveInlining)] public static double StudentTPpf(double freedom, double p) => CommonLib.Numerics.StudentTPpf(p, freedom);
    [MethodImpl(MethodImplOptions.AggressiveInlining)] public static double StudentTIsf(double freedom, double p) => CommonLib.Numerics.StudentTIsf(p, freedom);
    [MethodImpl(MethodImplOptions.AggressiveInlining)] public static double ExponentialPpf(double rate, double p) => CommonLib.Numerics.ExponentialPpf(p, rate);
    [MethodImpl(MethodImplOptions.AggressiveInlining)] public static double ExponentialIsf(double rate, double p) => CommonLib.Numerics.ExponentialIsf(p, rate);
    [MethodImpl(MethodImplOptions.AggressiveInlining)] public static double WeibullPpf(double shape, double scale, double p) => CommonLib.Numerics.WeibullPpf(p, shape, scale);
    [MethodImpl(MethodImplOptions.AggressiveInlining)] public static double LogisticPpf(double location, double scale, double p) => CommonLib.Numerics.LogisticPpf(p, location, scale);
    [MethodImpl(MethodImplOptions.AggressiveInlining)] public static double UniformPpf(double min, double max, double p) => CommonLib.Numerics.UniformPpf(p, min, max);
    [MethodImpl(MethodImplOptions.AggressiveInlining)] public static double ParetoPpf(double scale, double shape, double p) => CommonLib.Numerics.ParetoPpf(p, scale, shape);
    [MethodImpl(MethodImplOptions.AggressiveInlining)] public static double TriangularPpf(double min, double mode, double max, double p) => CommonLib.Numerics.TriangularPpf(p, min, mode, max);
    [MethodImpl(MethodImplOptions.AggressiveInlining)] public static double ParetoPdf(double scale, double shape, double x) => CommonLib.Numerics.ParetoPdf(scale, shape, x);
    [MethodImpl(MethodImplOptions.AggressiveInlining)] public static double LogisticPdf(double mean, double scale, double x) => CommonLib.Numerics.LogisticPdf(mean, scale, x);
    [MethodImpl(MethodImplOptions.AggressiveInlining)] public static double TriangularPdf(double min, double mode, double max, double x) => CommonLib.Numerics.TriangularPdf(min, mode, max, x);
    [MethodImpl(MethodImplOptions.AggressiveInlining)] public static double UniformPdf(double min, double max, double x) => CommonLib.Numerics.UniformPdf(min, max, x);
    [MethodImpl(MethodImplOptions.AggressiveInlining)] public static double SkewedNormalPdf(double mean, double stdDev, double skew, double x) => CommonLib.Numerics.SkewedNormalPdf(mean, stdDev, skew, x);
    [MethodImpl(MethodImplOptions.AggressiveInlining)] public static double BernoulliPmf(double p, int k) => CommonLib.Numerics.BernoulliPmf(p, k);
    [MethodImpl(MethodImplOptions.AggressiveInlining)] public static double BinomialPmf(double p, int n, int k) => CommonLib.Numerics.BinomialPmf(p, n, k);
    [MethodImpl(MethodImplOptions.AggressiveInlining)] public static double PoissonPmf(double lambda, int k) => CommonLib.Numerics.PoissonPmf(lambda, k);
    [MethodImpl(MethodImplOptions.AggressiveInlining)] public static double GeometricPmf(double p, int k) => CommonLib.Numerics.GeometricPmf(p, k);
    // ATTENZIONE ordine: CommonLib.Numerics.NegativeBinomialPmf è (r, p, k), non (p, r, k) come
    // nel vecchio dead file PCodeVm/PCodeProcessor.StdLib.Math.cs (che passava args[2], args[1] —
    // stesso ordine numerico ma nome invertito, mai stato un bug lì per pura coincidenza di
    // chiamata; qui la firma tipizzata rende l'ordine esplicito ed è stata verificata contro
    // CommonLib/Numerics/Stats.cs:646 prima di scriverla).
    [MethodImpl(MethodImplOptions.AggressiveInlining)] public static double NegativeBinomialPmf(double r, double p, int k) => CommonLib.Numerics.NegativeBinomialPmf(r, p, k);
    [MethodImpl(MethodImplOptions.AggressiveInlining)] public static double HypergeometricPmf(int populationSize, int successStates, int draws, int k) => CommonLib.Numerics.HypergeometricPmf(populationSize, successStates, draws, k);
}
