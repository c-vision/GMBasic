// FILE: Basic.Agents.Recursive.RecursiveLink
// Modulo "RecursiveLink" (piano RecursiveMAS) — proiezione residuale a 2 layer nello spazio
// latente, come definito dal paper (arXiv:2604.25917 §3.1):
//   R_in(h)  = h  + W2·GELU(W1·h)                  (dense->shallow, stesso agente)
//   R_out(h) = W3·h + W2·GELU(W1·h)               (cross-agent, W3 bridge su dim diverse)
// Il residual preserva la semantica: il link impara solo lo shift distribuzionale.
// Implementazione C# pura (nessuna dipendenza): TORCH (gmb_ml.h) può diventare backend in R5.
// Solo FORWARD + coseno per ora; il training (inner cos-regression / outer CE) è R4.

using System.Collections.Concurrent;

namespace Basic.Agents.Recursive;

/// <summary>Proiezione residuale 2-layer (W1·hidden + GELU, W2·out; W3 per l'outer).
/// Handle opachi int; registry statico in-process.</summary>
public sealed class RecursiveLink
{
    private sealed class Link
    {
        public int Dim;         // dimensione input h
        public int Hidden;      // hidden layer
        public int OutDim;      // dimensione output (Dim per inner, dstDim per outer)
        public bool IsOuter;
        public double[] W1 = [];     // [Hidden, Dim]
        public double[] B1 = [];     // [Hidden]
        public double[] W2 = [];     // [OutDim, Hidden]
        public double[] B2 = [];     // [OutDim]
        public double[] W3 = [];     // [OutDim, Dim] (outer: bridge; inner: assente)
        public long CreatedAt;
    }

    private static readonly ConcurrentDictionary<int, Link> _links = new();
    private static int _nextHandle = 1;
    private static readonly object _lock = new();

    private static double Gelu(double x) => 0.5 * x * (1.0 + Math.Tanh(Math.Sqrt(2.0 / Math.PI) * (x + 0.044715 * x * x * x)));

    // Init deterministico PORTABILE (splitmix64 su (i, seed)) — identico in C# e in C
    // (gmb_team.c): garantisce che i pesi dei link siano BIT-IDENTICI tra i backend
    // (IL/PCodeVM e VM/C/C99), quindi anche la loss del training e le fingerprint.
    private static ulong SplitMix64(ulong x)
    {
        x += 0x9E3779B97F4A7C15UL;
        x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9UL;
        x = (x ^ (x >> 27)) * 0x94D049BB133111EBUL;
        return x ^ (x >> 31);
    }

    private static double Unit01(int index, int seed)
        => (SplitMix64((ulong)(uint)seed + (ulong)(uint)index) & 0xFFFFFFFFUL) / 4294967296.0;

    private static void InitW(double[] w, int fanIn, int seed)
    {
        for (var i = 0; i < w.Length; i++)
            w[i] = (Unit01(i, seed) * 2.0 - 1.0) * Math.Sqrt(2.0 / fanIn);
    }

    private static int Create(int dim, int outDim, int hidden, bool isOuter)
    {
        ArgumentOutOfRangeException.ThrowIfNegativeOrZero(dim);
        ArgumentOutOfRangeException.ThrowIfNegativeOrZero(outDim);
        ArgumentOutOfRangeException.ThrowIfNegativeOrZero(hidden);
        var seed = 42 + Interlocked.Increment(ref _nextHandle); // deterministico, portabile (splitmix64)
        var link = new Link
        {
            Dim = dim, Hidden = hidden, OutDim = outDim, IsOuter = isOuter,
            W1 = new double[hidden * dim], B1 = new double[hidden],
            W2 = new double[outDim * hidden], B2 = new double[outDim],
            W3 = new double[outDim * dim],
            CreatedAt = DateTimeOffset.UtcNow.ToUnixTimeMilliseconds(),
        };
        InitW(link.W1, dim, seed); InitW(link.W2, hidden, seed);
        if (isOuter) InitW(link.W3, dim, seed);
        lock (_lock)
        {
            var handle = _nextHandle;
            _links[handle] = link;
            return handle;
        }
    }

    /// <summary>Inner link: proietta h nello stesso spazio (residual h + W2·GELU(W1·h)).</summary>
    public static int CreateInner(int dim, int hidden) => Create(dim, dim, hidden, isOuter: false);

    /// <summary>Outer link: proietta h da srcDim a dstDim (W3·h + W2·GELU(W1·h)).</summary>
    public static int CreateOuter(int srcDim, int dstDim, int hidden) => Create(srcDim, dstDim, hidden, isOuter: true);

    /// <summary>Forward del link. Ritorna h_out di lunghezza OutDim, o null se handle invalido.</summary>
    public static double[]? Forward(int handle, double[] h)
    {
        if (h is null || !_links.TryGetValue(handle, out var link)) return null;
        if (h.Length != link.Dim) return null;
        var outArr = new double[link.OutDim];

        // attivazione: a_j = GELU(W1[j,:]·h + b1[j])
        var activations = new double[link.Hidden];
        for (var j = 0; j < link.Hidden; j++)
        {
            var acc = link.B1[j];
            var w1row = j * link.Dim;
            for (var i = 0; i < link.Dim; i++) acc += link.W1[w1row + i] * h[i];
            activations[j] = Gelu(acc);
        }

        // W2·a
        for (var o = 0; o < link.OutDim; o++)
        {
            var acc = link.B2[o];
            var w2row = o * link.Hidden;
            for (var j = 0; j < link.Hidden; j++) acc += link.W2[w2row + j] * activations[j];
            outArr[o] = acc;
        }

        if (link.IsOuter)
        {
            // + W3·h
            for (var o = 0; o < link.OutDim; o++)
            {
                var acc = 0.0;
                var w3row = o * link.Dim;
                for (var i = 0; i < link.Dim; i++) acc += link.W3[w3row + i] * h[i];
                outArr[o] += acc;
            }
        }
        else
        {
            // residual interno: + h (stessa dim)
            for (var i = 0; i < link.OutDim; i++) outArr[i] += h[i];
        }
        return outArr;
    }

    /// <summary>Loss di regressione coseno (paper §4 inner loop): L = 1 − cos(R(h), target).</summary>
    public static double CosineLoss(int handle, double[] h, double[] target)
    {
        var r = Forward(handle, h);
        if (r is null || target is null || r.Length != target.Length) return double.NaN;
        double dot = 0, nr = 0, nt = 0;
        for (var i = 0; i < r.Length; i++)
        {
            dot += r[i] * target[i];
            nr += r[i] * r[i];
            nt += target[i] * target[i];
        }
        if (nr <= 0 || nt <= 0) return 1.0;
        return 1.0 - dot / (Math.Sqrt(nr) * Math.Sqrt(nt));
    }

    /// <summary>GELU derivative (per il gradiente analitico del training).</summary>
    private static double GeluPrime(double x)
    {
        var t = Math.Tanh(Math.Sqrt(2.0 / Math.PI) * (x + 0.044715 * x * x * x));
        var inner = Math.Sqrt(2.0 / Math.PI) * (1.0 + 3.0 * 0.044715 * x * x);
        return 0.5 * (1.0 + t) + 0.5 * x * (1.0 - t * t) * inner;
    }

    /// <summary>Un passo SGD sul gradiente analitico della loss coseno (paper §4 inner):
    /// aggiorna W1/B1/W2/B2 (e W3 se outer). Ritorna la loss prima dell'update. Lr tipico 1e-2..1e-1.</summary>
    public static double TrainStep(int handle, double[] h, double[] target, double lr)
    {
        if (h is null || target is null || !_links.TryGetValue(handle, out var link)) return double.NaN;
        if (h.Length != link.Dim || target.Length != link.OutDim) return double.NaN;
        var (dim, hidden, outDim, isOuter) = (link.Dim, link.Hidden, link.OutDim, link.IsOuter);

        // forward
        var z = new double[hidden]; var a = new double[hidden]; var aG = new double[hidden];
        for (var j = 0; j < hidden; j++)
        {
            var acc = link.B1[j];
            var w1row = j * dim;
            for (var i = 0; i < dim; i++) acc += link.W1[w1row + i] * h[i];
            z[j] = acc; a[j] = Gelu(acc); aG[j] = GeluPrime(acc);
        }
        var r = new double[outDim];
        for (var o = 0; o < outDim; o++)
        {
            var acc = link.B2[o];
            var w2row = o * hidden;
            for (var j = 0; j < hidden; j++) acc += link.W2[w2row + j] * a[j];
            if (isOuter)
            {
                var w3row = o * dim;
                for (var i = 0; i < dim; i++) acc += link.W3[w3row + i] * h[i];
            }
            else acc += h[o];
            r[o] = acc;
        }

        // coseno e gradiente dL/dr
        double c = 0, nr = 0, nt = 0;
        for (var i = 0; i < outDim; i++) { c += r[i] * target[i]; nr += r[i] * r[i]; nt += target[i] * target[i]; }
        double loss = 1.0;
        var dL = new double[outDim];
        if (nr > 1e-12 && nt > 1e-12)
        {
            var n = Math.Sqrt(nr); var m = Math.Sqrt(nt);
            loss = 1.0 - c / (n * m);
            var c_n3_m = c / (nr * n * m);
            for (var i = 0; i < outDim; i++) dL[i] = -target[i] / (n * m) + c_n3_m * r[i];
        }
        else return loss;

        // backprop
        var gradA = new double[hidden];
        for (var j = 0; j < hidden; j++)
        {
            double ga = 0;
            for (var o = 0; o < outDim; o++) ga += dL[o] * link.W2[o * hidden + j];
            gradA[j] = ga * aG[j];
        }

        // update W2, B2
        for (var o = 0; o < outDim; o++)
        {
            var w2row = o * hidden;
            for (var j = 0; j < hidden; j++) link.W2[w2row + j] -= lr * dL[o] * a[j];
            link.B2[o] -= lr * dL[o];
        }
        // update W1, B1
        for (var j = 0; j < hidden; j++)
        {
            var w1row = j * dim;
            for (var i = 0; i < dim; i++) link.W1[w1row + i] -= lr * gradA[j] * h[i];
            link.B1[j] -= lr * gradA[j];
        }
        // update W3 (outer)
        if (isOuter)
            for (var o = 0; o < outDim; o++)
            {
                var w3row = o * dim;
                for (var i = 0; i < dim; i++) link.W3[w3row + i] -= lr * dL[o] * h[i];
            }
        return loss;
    }

    /// <summary>Embedding proxy deterministico 768-dim (hashing trigram) — condiviso tra
    /// TeamOrchestrator (embed) e RecursiveTrainer (R4). R5 lo sostituisce con AI.EMBED/last_hidden.</summary>
    public static double[] SimpleEmbed(string text)
    {
        const int dim = 768;
        var v = new double[dim];
        if (string.IsNullOrEmpty(text)) return v;
        var bytes = System.Text.Encoding.UTF8.GetBytes(text);
        for (var i = 0; i + 2 < bytes.Length; i++)
        {
            var t = (bytes[i] << 16) | (bytes[i + 1] << 8) | bytes[i + 2];
            var hsh = unchecked((int)((uint)t * 2654435761u)) >> 8;
            var idx = Math.Abs(hsh) % dim;
            v[idx] += (hsh & 1) == 0 ? 1.0 : -1.0;
        }
        double norm = 0;
        foreach (var x in v) norm += x * x;
        if (norm > 0)
        {
            var s = 1.0 / Math.Sqrt(norm);
            for (var i = 0; i < dim; i++) v[i] *= s;
        }
        return v;
    }

    /// <summary>Fingerprint testuale deterministica del latente (top-k dims) — per osservabilità.
    /// Formato INVARIANT (punto decimale) così è identico su tutti i backend (IL/PCodeVM/VM/C/C99).</summary>
    public static string Fingerprint(double[] h, int k = 3)
    {
        if (h is null || h.Length == 0) return "";
        var idx = Enumerable.Range(0, h.Length).OrderByDescending(i => Math.Abs(h[i])).Take(k);
        return string.Join(",", idx.Select(i => $"{i}:{h[i].ToString("0.000", System.Globalization.CultureInfo.InvariantCulture)}"));
    }

    // ── Persistenza (R6/A.4): export/import dei pesi per TEAM SAVE/LOAD ──────
    /// <summary>Snapshot serializzabile dei pesi di un link (R_in/R_out) — checkpoint su disco.</summary>
    public sealed record LinkSnapshot(int Dim, int Hidden, int OutDim, bool IsOuter,
        double[] W1, double[] B1, double[] W2, double[] B2, double[] W3);

    /// <summary>Esporta i pesi di un handle, o null se handle invalido.</summary>
    public static LinkSnapshot? Export(int handle)
    {
        if (!_links.TryGetValue(handle, out var link)) return null;
        return new LinkSnapshot(link.Dim, link.Hidden, link.OutDim, link.IsOuter,
            (double[])link.W1.Clone(), (double[])link.B1.Clone(), (double[])link.W2.Clone(),
            (double[])link.B2.Clone(), (double[])link.W3.Clone());
    }

    /// <summary>Ricrea un link dai pesi esportati (deterministico, nessuna inizializzazione random).</summary>
    public static int Import(LinkSnapshot snap)
    {
        ArgumentNullException.ThrowIfNull(snap);
        if (snap.W1.Length != snap.Hidden * snap.Dim || snap.W2.Length != snap.OutDim * snap.Hidden ||
            snap.W3.Length != snap.OutDim * snap.Dim) return -1;
        var link = new Link
        {
            Dim = snap.Dim, Hidden = snap.Hidden, OutDim = snap.OutDim, IsOuter = snap.IsOuter,
            W1 = (double[])snap.W1.Clone(), B1 = (double[])snap.B1.Clone(),
            W2 = (double[])snap.W2.Clone(), B2 = (double[])snap.B2.Clone(),
            W3 = (double[])snap.W3.Clone(),
            CreatedAt = DateTimeOffset.UtcNow.ToUnixTimeMilliseconds(),
        };
        lock (_lock)
        {
            var handle = _nextHandle;
            _nextHandle++;
            _links[handle] = link;
            return handle;
        }
    }

    public static void ResetForTests()
    {
        lock (_lock) _links.Clear();
        _nextHandle = 1;
    }
}
