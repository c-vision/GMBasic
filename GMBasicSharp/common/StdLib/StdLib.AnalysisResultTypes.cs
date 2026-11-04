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
    }
}
