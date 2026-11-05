using System.Collections.Immutable;

namespace Basic;

public sealed partial class StdLib
{
    /// <summary>
    /// Schemi dei tipi risultato gia' esposti da ANALYSIS; restano globali dopo lo split
    /// nei namespace NUMERICAL/TIMESERIES/MULTIVARIATE/REGRESSION/STATS.
    /// </summary>
    private void RegisterAnalysisResultTypeSchemas()
    {
        RegisterTypeSchema(typeof(AnovaResultType), new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase)
        {
            { "FValue", new DoubleType() },
            { "PValue", new DoubleType() },
            { "DFBetween", new IntegerType() },
            { "DFWithin", new IntegerType() },
            { "SSBetween", new DoubleType() },
            { "SSWithin", new DoubleType() },
            { "MSBetween", new DoubleType() },
            { "MSWithin", new DoubleType() }
        });

        RegisterTypeSchema(typeof(ArimaResultType), new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase)
        {
            { "ArCoeffs", new ArrayType(new DoubleType()) },
            { "MaCoeffs", new ArrayType(new DoubleType()) },
            { "AIC", new DoubleType() },
            { "Fitted", new ArrayType(new DoubleType()) }
        });

        RegisterTypeSchema(typeof(RegressionResultType),
            new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase)
            {
                { "Slope", new DoubleType() },
                { "Intercept", new DoubleType() },
                { "R2", new DoubleType() },
                { "StandardError", new DoubleType() }
            });

        RegisterTypeSchema(typeof(KMeansResultType), new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase)
        {
            { "Centroids", new ArrayType(new DoubleType()) },
            { "Assignments", new ArrayType(new IntegerType()) },
            { "Iterations", new IntegerType() }
        });

        RegisterTypeSchema(typeof(HoltWintersResultType),
            new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase)
            {
                { "Forecast", new ArrayType(new DoubleType()) },
                { "Mse", new DoubleType() }
            });

        RegisterTypeSchema(typeof(AdfTestResultType), new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase)
        {
            { "Statistic", new DoubleType() },
            { "PValue", new DoubleType() }
        });

        RegisterTypeSchema(typeof(PcaResultType), new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase)
        {
            { "EigenValues", new ArrayType(new DoubleType()) },
            { "EigenVectors", new ArrayType(new ArrayType(new DoubleType())) }
        });

        RegisterTypeSchema(typeof(GmmResultType), new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase)
        {
            { "Mu", new ArrayType(new DoubleType()) },
            { "Variance", new ArrayType(new DoubleType()) },
            { "Weight", new ArrayType(new DoubleType()) }
        });

        RegisterTypeSchema(typeof(SvmResultType), new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase)
        {
            { "W", new ArrayType(new DoubleType()) },
            { "Classes", new DoubleType() },
            { "Features", new DoubleType() }
        });

        RegisterTypeSchema(typeof(GpResultType), new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase)
        {
            { "Mean", new ArrayType(new DoubleType()) },
            { "Variance", new ArrayType(new DoubleType()) }
        });

        RegisterTypeSchema(typeof(JohansenResultType), new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase)
        {
            { "EigenValues", new ArrayType(new DoubleType()) },
            { "TraceStatistic", new ArrayType(new DoubleType()) },
            { "MaxStatistic", new ArrayType(new DoubleType()) },
            { "Observations", new DoubleType() },
            { "Lags", new DoubleType() }
        });

        RegisterTypeSchema(typeof(HmmResultType), new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase)
        {
            { "Pi", new ArrayType(new DoubleType()) },
            { "A", new ArrayType(new DoubleType()) },
            { "Means", new ArrayType(new DoubleType()) },
            { "Covs", new ArrayType(new DoubleType()) },
            { "LogLikelihood", new DoubleType() }
        });
    }
}
