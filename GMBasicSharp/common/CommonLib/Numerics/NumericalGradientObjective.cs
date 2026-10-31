using MathNet.Numerics.LinearAlgebra;
using MathNet.Numerics.Optimization;

namespace CommonLib;

/// <summary>
/// Wrapper che fornisce gradienti calcolati numericamente (differenze finite)
/// per algoritmi di ottimizzazione che li richiedono (BFGS, L-BFGS, CG).
/// </summary>
internal sealed class NumericalGradientObjective : IObjectiveFunction
{
        private readonly Func<Vector<double>, double> _f;
        private const double H = 1e-8;

        public NumericalGradientObjective(Func<Vector<double>, double> f) => _f = f;

        public void EvaluateAt(Vector<double> point)
        {
            Point = point.Clone();
            Value = _f(point);
            var gradient = Vector<double>.Build.Dense(point.Count);
            for (var i = 0; i < point.Count; i++)
            {
                var vPlus = point.Clone();
                vPlus[i] += H;
                gradient[i] = (_f(vPlus) - Value) / H;
            }
            Gradient = gradient;
        }

        public IObjectiveFunction Fork() => new NumericalGradientObjective(_f);

        public Vector<double> Point { get; private set; } = null!;
        public double Value { get; private set; }
        public bool IsGradientSupported => true;
        public Vector<double> Gradient { get; private set; } = null!;
        public bool IsHessianSupported => false;
        public Matrix<double> Hessian => throw new NotSupportedException();

        public IObjectiveFunction CreateNew() => new NumericalGradientObjective(_f);
}
