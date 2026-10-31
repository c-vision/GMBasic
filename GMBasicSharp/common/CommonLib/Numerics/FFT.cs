// FILE: StdLib.Numerics.FFT.cs
// ReSharper disable CheckNamespace

using MathNet.Numerics.IntegralTransforms;
using System.Numerics;

namespace CommonLib;

/// <summary>
/// Fornisce funzioni per la Trasformata Veloce di Fourier (FFT) e la sua inversa (IFFT).
/// Contiene i seguenti metodi:
/// <list type="bullet">
///   <item><description><c>Fft(double[] data)</c> — FFT di un array di valori reali.</description></item>
///   <item><description><c>Fft(double[] real, double[]? imag)</c> — FFT di array reali/immaginari separati.</description></item>
///   <item><description><c>Ifft(double[] data)</c> — FFT inversa di un array di valori reali.</description></item>
///   <item><description><c>Ifft(double[] real, double[]? imag)</c> — FFT inversa di array reali/immaginari separati.</description></item>
///   <item><description><c>Fft(Complex[] complexData, bool inverse)</c> — FFT diretta o inversa su array di numeri complessi.</description></item>
///   <item><description><c>ComplexArrayToObjectMatrix</c> — Converte un array Complex in una matrice object[,].</description></item>
/// </list>
/// </summary>
public static partial class Numerics
{
    /// <summary>
    /// Esegue la Trasformata Veloce di Fourier (FFT) su un array di valori reali.
    /// Restituisce una matrice di dimensioni n×2 dove [i,0] = parte reale, [i,1] = parte immaginaria.
    /// </summary>
    /// <param name="data">Array di valori reali da trasformare.</param>
    /// <returns>Matrice object[n,2] con parte reale e immaginaria per ogni componente frequenziale.</returns>
    /// <example>var result = Numerics.Fft(new double[] {1.0, 2.0, 1.0, 0.0});</example>
    public static object[,] Fft(double[] data)
    {
        if (data.Length == 0) return new object[0, 2];
        var complexData = data.Select(d => new Complex(d, 0)).ToArray();
        Fourier.Forward(complexData, FourierOptions.Default);
        return ComplexArrayToObjectMatrix(complexData);
    }

    /// <summary>
    /// Esegue la Trasformata Veloce di Fourier (FFT) su array separati di parte reale e immaginaria.
    /// </summary>
    /// <param name="real">Array della parte reale.</param>
    /// <param name="imag">Array della parte immaginaria (opzionale; se null, assume zero).</param>
    /// <returns>Matrice object[n,2] con parte reale e immaginaria per ogni componente frequenziale.</returns>
    /// <exception cref="ArgumentNullException">Se real è null.</exception>
    /// <exception cref="ArgumentException">Se gli array hanno lunghezza diversa, o contengono valori NaN/Infinity.</exception>
    /// <exception cref="InvalidOperationException">Se la FFT fallisce.</exception>
    /// <example>var result = Numerics.Fft(new double[] {1.0, 0.0, -1.0, 0.0}, null);</example>
    public static object[,] Fft(double[] real, double[]? imag)
    {
        if (real == null)
            throw new ArgumentNullException(nameof(real));
        if (real.Length == 0)
            return new object[0, 2];
        if (imag != null && imag.Length != real.Length)
            throw new ArgumentException("Gli array real e imag devono avere la stessa lunghezza.");
        var complexData = new Complex[real.Length];
        for (var i = 0; i < real.Length; i++)
        {
            var r = real[i];
            var im = imag != null ? imag[i] : 0.0;
            if (double.IsNaN(r) || double.IsInfinity(r) || double.IsNaN(im) || double.IsInfinity(im))
                throw new ArgumentException($"Valore non valido in posizione {i}: ({r}, {im})");
            complexData[i] = new Complex(r, im);
        }
        try
        {
            Fourier.Forward(complexData, FourierOptions.Default);
        }
        catch (Exception ex)
        {
            throw new InvalidOperationException("Errore durante l'esecuzione della FFT.", ex);
        }
        return ComplexArrayToObjectMatrix(complexData);
    }

    /// <summary>
    /// Esegue la Trasformata Veloce di Fourier inversa (IFFT) su un array di valori reali.
    /// </summary>
    /// <param name="data">Array di valori reali nel dominio della frequenza.</param>
    /// <returns>Matrice object[n,2] con parte reale e immaginaria nel dominio del tempo.</returns>
    /// <exception cref="ArgumentNullException">Se data è null.</exception>
    /// <exception cref="ArgumentException">Se l'array contiene valori NaN/Infinity.</exception>
    /// <exception cref="InvalidOperationException">Se la FFT inversa fallisce.</exception>
    /// <example>var result = Numerics.Ifft(new double[] {4.0, 0.0, 0.0, 0.0});</example>
    public static object[,] Ifft(double[] data)
    {
        if (data == null)
            throw new ArgumentNullException(nameof(data));
        if (data.Length == 0)
            return new object[0, 2];
        var complexData = new Complex[data.Length];
        for (var i = 0; i < data.Length; i++)
        {
            var v = data[i];
            if (double.IsNaN(v) || double.IsInfinity(v))
                throw new ArgumentException($"Valore non valido in posizione {i}: {v}", nameof(data));
            complexData[i] = new Complex(v, 0.0);
        }
        try
        {
            Fourier.Inverse(complexData, FourierOptions.Default);
        }
        catch (Exception ex)
        {
            throw new InvalidOperationException("Errore durante l'esecuzione della FFT inversa.", ex);
        }
        return ComplexArrayToObjectMatrix(complexData);
    }

    /// <summary>
    /// Esegue la Trasformata Veloce di Fourier inversa (IFFT) su array separati di parte reale e immaginaria.
    /// </summary>
    /// <param name="real">Array della parte reale (dominio della frequenza).</param>
    /// <param name="imag">Array della parte immaginaria (opzionale; se null, assume zero).</param>
    /// <returns>Matrice object[n,2] con parte reale e immaginaria nel dominio del tempo.</returns>
    /// <exception cref="ArgumentNullException">Se real è null.</exception>
    /// <exception cref="ArgumentException">Se gli array hanno lunghezza diversa, o contengono valori NaN/Infinity.</exception>
    /// <exception cref="InvalidOperationException">Se la FFT inversa fallisce.</exception>
    /// <example>var result = Numerics.Ifft(new double[] {4.0, 0.0, 0.0, 0.0}, null);</example>
    public static object[,] Ifft(double[] real, double[]? imag)
    {
        if (real == null)
            throw new ArgumentNullException(nameof(real));
        if (real.Length == 0)
            return new object[0, 2];
        if (imag != null && imag.Length != real.Length)
            throw new ArgumentException("Gli array real e imag devono avere la stessa lunghezza.");
        var complexData = new Complex[real.Length];
        for (var i = 0; i < real.Length; i++)
        {
            var r = real[i];
            var im = imag != null ? imag[i] : 0.0;
            if (double.IsNaN(r) || double.IsInfinity(r) || double.IsNaN(im) || double.IsInfinity(im))
                throw new ArgumentException($"Valore non valido in posizione {i}: ({r}, {im})");
            complexData[i] = new Complex(r, im);
        }
        try
        {
            Fourier.Inverse(complexData, FourierOptions.Default);
        }
        catch (Exception ex)
        {
            throw new InvalidOperationException("Errore durante l'esecuzione della FFT inversa.", ex);
        }
        return ComplexArrayToObjectMatrix(complexData);
    }

    /// <summary>
    /// Frequenze discrete della FFT (scipy.fft.fftfreq): per n pari
    /// [0,1,...,n/2-1,-n/2,...,-1]/(n*d); per n dispari [0,1,...,(n-1)/2,-(n-1)/2,...,-1]/(n*d).
    /// </summary>
    public static double[] FftFreq(int n, double d = 1.0)
    {
        if (n <= 0) return Array.Empty<double>();
        if (d == 0) throw new ArgumentException("d non può essere 0.");
        var res = new double[n];
        var pos = n - n / 2; // frequenze non negative incl. 0: pari n/2, dispari (n+1)/2
        for (var i = 0; i < pos; i++) res[i] = i / (n * d);
        for (var i = pos; i < n; i++) res[i] = (i - n) / (n * d);
        return res;
    }

    /// <summary>Sposta la frequenza zero al centro dello spettro (scipy.fft.fftshift =
    /// np.roll(x, n//2)).</summary>
    public static double[] FftShift(double[] arr)
    {
        if (arr == null) throw new ArgumentNullException(nameof(arr));
        if (arr.Length < 2) return (double[])arr.Clone();
        var shift = arr.Length - arr.Length / 2;
        var res = new double[arr.Length];
        for (var i = 0; i < arr.Length; i++) res[i] = arr[(i + shift) % arr.Length];
        return res;
    }

    /// <summary>Inverso di FftShift (scipy.fft.ifftshift = np.roll(x, -(n//2))).</summary>
    public static double[] IfftShift(double[] arr)
    {
        if (arr == null) throw new ArgumentNullException(nameof(arr));
        if (arr.Length < 2) return (double[])arr.Clone();
        var shift = arr.Length / 2;
        var res = new double[arr.Length];
        for (var i = 0; i < arr.Length; i++) res[i] = arr[(i + shift) % arr.Length];
        return res;
    }

    /// <summary>
    /// Converte un array di numeri complessi in una matrice bidimensionale object[,].
    /// Ogni riga contiene [parte reale, parte immaginaria].
    /// </summary>
    /// <param name="complexData">Array di numeri complessi da convertire.</param>
    /// <returns>Matrice object[n,2] dove [i,0] = Real, [i,1] = Imaginary.</returns>
    private static object[,] ComplexArrayToObjectMatrix(Complex[] complexData)
    {
        var result = new object[complexData.Length, 2];
        for (var i = 0; i < complexData.Length; i++)
        {
            result[i, 0] = complexData[i].Real;
            result[i, 1] = complexData[i].Imaginary;
        }
        return result;
    }

    /// <summary>
    /// Esegue la Trasformata Veloce di Fourier (FFT) diretta o inversa su un array di numeri complessi.
    /// </summary>
    /// <param name="complexData">Array di numeri complessi da trasformare.</param>
    /// <param name="inverse">Se true, esegue la trasformata inversa (IFFT); altrimenti esegue la FFT diretta.</param>
    /// <returns>Matrice object[n,2] con parte reale e immaginaria del risultato.</returns>
    /// <exception cref="ArgumentNullException">Se complexData è null.</exception>
    /// <exception cref="ArgumentException">Se l'array contiene valori NaN/Infinity.</exception>
    /// <exception cref="InvalidOperationException">Se la trasformata fallisce.</exception>
    /// <example>var data = new[] { new Complex(1,0), new Complex(0,0), new Complex(-1,0), new Complex(0,0) };
    /// var result = Numerics.Fft(data, inverse: false);</example>
    public static object[,] Fft(Complex[] complexData, bool inverse = false)
    {
        if (complexData == null)
            throw new ArgumentNullException(nameof(complexData));
        if (complexData.Length == 0)
            return new object[0, 2];
        var dataCopy = new Complex[complexData.Length];
        for (var i = 0; i < complexData.Length; i++)
        {
            var z = complexData[i];
            if (double.IsNaN(z.Real) || double.IsInfinity(z.Real) || double.IsNaN(z.Imaginary) ||
                double.IsInfinity(z.Imaginary))
                throw new ArgumentException($"Valore non valido in posizione {i}: ({z.Real}, {z.Imaginary})");
            dataCopy[i] = z;
        }
        try
        {
            if (!inverse)
                Fourier.Forward(dataCopy, FourierOptions.Default);
            else
                Fourier.Inverse(dataCopy, FourierOptions.Default);
        }
        catch (Exception ex)
        {
            throw new InvalidOperationException("Errore durante l'esecuzione della FFT.", ex);
        }
        return ComplexArrayToObjectMatrix(dataCopy);
    }
}
