// ReSharper disable InconsistentNaming
namespace CommonLib.Trading;

/// <summary>
/// Risultato dell'ottimizzazione MAD (Mean Absolute Deviation) o Semi-MAD.
/// Contiene i pesi ottimali, il valore della deviazione assoluta e lo stato del solutore.
///
/// Proprietà:
/// - <see cref="Weights"/>: pesi ottimali del portafoglio.
/// - <see cref="Mad"/>: valore della Mean Absolute Deviation minimizzata.
/// - <see cref="Ut"/>: variabili ausiliarie per ciascun periodo (scarti assoluti).
/// - <see cref="SolverStatus"/>: stato del solutore ("OK" in caso di successo).
/// </summary>
public class MadLpResult
{
    /// <summary>Pesi ottimali del portafoglio.</summary>
    public double[] Weights { get; set; } = [];

    /// <summary>Valore della Mean Absolute Deviation (o Semi-MAD) minimizzata.</summary>
    public double Mad { get; set; }

    /// <summary>Variabili ausiliarie u_t per ciascun periodo T (scarti assoluti).</summary>
    public double[] Ut { get; set; } = [];

    /// <summary>Stato del solutore lineare.</summary>
    public string SolverStatus { get; set; } = "";
}

/// <summary>
/// Risolve problemi di ottimizzazione di portafoglio utilizzando la minimizzazione
/// della deviazione assoluta media (MAD) e della semi-deviazione assoluta (Semi-MAD).
/// I problemi sono formulati come programmi lineari (LP) e risolti tramite il metodo del simplesso.
///
/// Metodi:
/// - <see cref="SolveMadPortfolio"/>: ottimizzazione MAD completa.
/// - <see cref="SolveSemiMadPortfolio"/>: ottimizzazione Semi-MAD (solo scarti negativi).
/// </summary>
public static class MadSolver
{
    /// <summary>
    /// Risolve il problema di ottimizzazione Mean Absolute Deviation (MAD) per un portafoglio.
    /// Formula il problema come programma lineare con variabili w_i (pesi) e u_t (scarti),
    /// vincolo di somma pesi = 1, e non negatività. Utilizza il simplesso per la soluzione.
    /// </summary>
    /// <param name="returns">Matrice [periodi × asset] dei rendimenti storici.</param>
    /// <returns>Risultato MAD con pesi ottimali, valore della deviazione e variabili ausiliarie.</returns>
    /// <example>
    /// <code>
    /// var result = MadSolver.SolveMadPortfolio(rendimentiStorici);
    /// Console.WriteLine($"MAD: {result.Mad}");
    /// </code>
    /// </example>
    public static MadLpResult SolveMadPortfolio(double[,] returns)
    {
        var T = returns.GetLength(0);
        var N = returns.GetLength(1);
        var numVars = N + T;

        var mean = new double[N];
        for (var i = 0; i < N; i++)
        {
            double s = 0;
            for (var t = 0; t < T; t++) s += returns[t, i];
            mean[i] = s / T;
        }

        var c = new double[numVars];
        for (var t = 0; t < T; t++) c[N + t] = 1.0 / T;

        var A = new List<double[]>();
        var b = new List<double>();
        var sense = new List<char>();

        for (var t = 0; t < T; t++)
        {
            var row1 = new double[numVars];
            for (var i = 0; i < N; i++) row1[i] = -(returns[t, i] - mean[i]);
            row1[N + t] = 1.0;
            A.Add(row1);
            b.Add(0.0);
            sense.Add('G');

            var row2 = new double[numVars];
            for (var i = 0; i < N; i++) row2[i] = (returns[t, i] - mean[i]);
            row2[N + t] = 1.0;
            A.Add(row2);
            b.Add(0.0);
            sense.Add('G');
        }

        var sumRow = new double[numVars];
        for (var i = 0; i < N; i++) sumRow[i] = 1.0;
        A.Add(sumRow);
        b.Add(1.0);
        sense.Add('E');

        for (var i = 0; i < N; i++)
        {
            var row = new double[numVars];
            row[i] = 1.0;
            A.Add(row);
            b.Add(0.0);
            sense.Add('G');
        }

        for (var t = 0; t < T; t++)
        {
            var row = new double[numVars];
            row[N + t] = 1.0;
            A.Add(row);
            b.Add(0.0);
            sense.Add('G');
        }

        var Aeq = new List<double[]>();
        var beq = new List<double>();
        var Aineq = new List<double[]>();
        var bineq = new List<double>();
        for (var k = 0; k < A.Count; k++)
        {
            if (sense[k] == 'E')
            {
                Aeq.Add(A[k]);
                beq.Add(b[k]);
            }
            else
            {
                Aineq.Add(A[k]);
                bineq.Add(b[k]);
            }
        }

        var tableau = BuildLpTableau(c, Aineq, bineq, Aeq, beq);

        var solution = Numerics.Simplex(tableau, maximize: false);

        var w = new double[N];
        var ut = new double[T];
        for (var i = 0; i < N; i++) w[i] = solution[i];
        for (var t = 0; t < T; t++) ut[t] = solution[N + t];
        var mad = 0.0;
        for (var t = 0; t < T; t++) mad += ut[t];
        mad /= T;

        return new MadLpResult
        {
            Weights = w,
            Mad = mad,
            Ut = ut,
            SolverStatus = "OK"
        };
    }

    /// <summary>
    /// Risolve il problema di ottimizzazione Semi-MAD (Mean Absolute Deviation unilaterale),
    /// considerando solo gli scarti negativi rispetto alla media (deviazione downside).
    /// Formula il problema come programma lineare e lo risolve con il simplesso.
    /// </summary>
    /// <param name="returns">Matrice [periodi × asset] dei rendimenti storici.</param>
    /// <returns>Risultato Semi-MAD con pesi ottimali e deviazione downside.</returns>
    /// <example>
    /// <code>
    /// var result = MadSolver.SolveSemiMadPortfolio(rendimentiStorici);
    /// Console.WriteLine($"Semi-MAD: {result.Mad}");
    /// </code>
    /// </example>
    public static MadLpResult SolveSemiMadPortfolio(double[,] returns)
    {
        var T = returns.GetLength(0);
        var N = returns.GetLength(1);
        var numVars = N + T;

        var mean = new double[N];
        for (var i = 0; i < N; i++)
        {
            double s = 0;
            for (var t = 0; t < T; t++) s += returns[t, i];
            mean[i] = s / T;
        }

        var c = new double[numVars];
        for (var t = 0; t < T; t++) c[N + t] = 1.0 / T;

        var A = new List<double[]>();
        var b = new List<double>();
        var sense = new List<char>();

        for (var t = 0; t < T; t++)
        {
            var row = new double[numVars];
            for (var i = 0; i < N; i++)
                row[i] = mean[i] - returns[t, i];
            row[N + t] = 1.0;
            A.Add(row);
            b.Add(0.0);
            sense.Add('G');
        }

        for (var t = 0; t < T; t++)
        {
            var row = new double[numVars];
            row[N + t] = 1.0;
            A.Add(row);
            b.Add(0.0);
            sense.Add('G');
        }

        var sumRow = new double[numVars];
        for (var i = 0; i < N; i++) sumRow[i] = 1.0;
        A.Add(sumRow);
        b.Add(1.0);
        sense.Add('E');

        for (var i = 0; i < N; i++)
        {
            var row = new double[numVars];
            row[i] = 1.0;
            A.Add(row);
            b.Add(0.0);
            sense.Add('G');
        }

        var Aeq = new List<double[]>();
        var beq = new List<double>();
        var Aineq = new List<double[]>();
        var bineq = new List<double>();
        for (var k = 0; k < A.Count; k++)
        {
            if (sense[k] == 'E')
            {
                Aeq.Add(A[k]);
                beq.Add(b[k]);
            }
            else
            {
                Aineq.Add(A[k]);
                bineq.Add(b[k]);
            }
        }

        var tableau = BuildLpTableau(c, Aineq, bineq, Aeq, beq);
        var solution = Numerics.Simplex(tableau, maximize: false);

        var w = new double[N];
        var dt = new double[T];
        for (var i = 0; i < N; i++) w[i] = solution[i];
        for (var t = 0; t < T; t++) dt[t] = solution[N + t];
        var semimad = 0.0;
        for (var t = 0; t < T; t++) semimad += dt[t];
        semimad /= T;

        return new MadLpResult
        {
            Weights = w,
            Mad = semimad,
            Ut = dt,
            SolverStatus = "OK"
        };
    }

    /// <summary>
    /// Costruisce il tableau iniziale per il metodo del simplesso a partire dai coefficienti
    /// della funzione obiettivo e dai vincoli di disuguaglianza/uguaglianza.
    /// Le variabili di slack sono aggiunte automaticamente per i vincoli di disuguaglianza.
    /// </summary>
    /// <param name="c">Coefficienti della funzione obiettivo.</param>
    /// <param name="Aineq">Matrice dei vincoli di disuguaglianza (righe × variabili).</param>
    /// <param name="bineq">Termini noti dei vincoli di disuguaglianza.</param>
    /// <param name="Aeq">Matrice dei vincoli di uguaglianza.</param>
    /// <param name="beq">Termini noti dei vincoli di uguaglianza.</param>
    /// <returns>Tableau [righe × colonne] per il simplesso.</returns>
    private static double[,] BuildLpTableau(
        double[] c,
        List<double[]> Aineq,
        List<double> bineq,
        List<double[]> Aeq,
        List<double> beq)
    {
        var nVars = c.Length;
        var nIneq = Aineq.Count;
        var nEq = Aeq.Count;
        var nRows = nIneq + nEq + 1;
        var nCols = nVars + nIneq + 1;

        var tableau = new double[nRows, nCols];

        for (var i = 0; i < nIneq; i++)
        {
            for (var j = 0; j < nVars; j++)
                tableau[i, j] = Aineq[i][j];
            tableau[i, nVars + i] = 1.0;
            tableau[i, nCols - 1] = bineq[i];
        }

        for (var i = 0; i < nEq; i++)
        {
            for (var j = 0; j < nVars; j++)
                tableau[nIneq + i, j] = Aeq[i][j];
            tableau[nIneq + i, nCols - 1] = beq[i];
        }

        for (var j = 0; j < nVars; j++)
            tableau[nRows - 1, j] = c[j];

        return tableau;
    }
}
