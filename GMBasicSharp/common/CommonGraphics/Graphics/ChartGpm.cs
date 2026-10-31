using SDL2;

namespace CommonGraphics.Graphics;

// Metodi GPM del sotto-sistema CHART (plotting headless con OxyPlot).
// Portati dall'Interpreter deprecato (Gpm.Plotting.*) — 2026-08-07.
// I grafici sono costruiti in memoria (handle LONG) e resi in PNG.

public sealed partial class GraphicsManager
{
    private ChartPlotManager.PlotManager? _chartManager;

    /// <summary>CHART.CREATE(title, [xLabel], [yLabel]) → handle LONG.</summary>
    public void ChartCreate_GPM(string title, string xLabel, string yLabel,
        Action<object?, Exception?> completionCallback) =>
        EnqueueGpmOperation(() =>
        {
            _chartManager ??= new ChartPlotManager.PlotManager();
            return _chartManager.CreatePlot(title, xLabel, yLabel);
        }, completionCallback);

    /// <summary>CHART.PLOT(handle, x[], y[], plotType, legend) — serie di dati XY.</summary>
    public void ChartPlot_GPM(long handle, double[] xData, double[] yData, string plotType, string legend,
        Action<object?, Exception?> completionCallback) =>
        EnqueueGpmOperation(() =>
        {
            _chartManager?.PlotData(handle, xData, yData, plotType, legend);
            return null;
        }, completionCallback);

    /// <summary>CHART.FINANCIAL(handle, ohlc, plotType, legend) — serie OHLC/Candlestick.</summary>
    public void ChartFinancial_GPM(long handle, object[] ohlc, string plotType, string legend,
        Action<object?, Exception?> completionCallback) =>
        EnqueueGpmOperation(() =>
        {
            if (_chartManager is null) return null;
            var data = ohlc.Select(v =>
            {
                // v può essere un array [open, high, low, close, date] o [date, open, high, low, close]
                if (v is System.Collections.IList arr && arr.Count >= 4)
                {
                    double d0 = Convert.ToDouble(arr[0] ?? 0);
                    double d1 = Convert.ToDouble(arr[1] ?? 0);
                    double d2 = Convert.ToDouble(arr[2] ?? 0);
                    double d3 = Convert.ToDouble(arr[3] ?? 0);
                    var ts = arr.Count >= 5 && arr[4] is DateTime dt ? dt : DateTime.FromOADate(d0);
                    return new ChartPlotManager.OhlcData(d0, d1, d2, d3, ts);
                }
                return new ChartPlotManager.OhlcData(0, 0, 0, 0, DateTime.MinValue);
            }).ToArray();
            _chartManager.PlotFinancial(handle, data, plotType, legend);
            return null;
        }, completionCallback);

    /// <summary>CHART.CLEAR(handle) — svuota le serie del grafico.</summary>
    public void ChartClear_GPM(long handle, Action<object?, Exception?> completionCallback) =>
        EnqueueGpmOperation(() =>
        {
            _chartManager?.ClearPlot(handle);
            return null;
        }, completionCallback);

    /// <summary>CHART.CLOSE(handle) — rilascia il grafico.</summary>
    public void ChartClose_GPM(long handle, Action<object?, Exception?> completionCallback) =>
        EnqueueGpmOperation(() =>
        {
            _chartManager?.ClosePlot(handle);
            return null;
        }, completionCallback);

    /// <summary>CHART.RENDER(handle, width, height) → byte[] PNG.</summary>
    public void ChartRender_GPM(long handle, int width, int height,
        Action<object?, Exception?> completionCallback) =>
        EnqueueGpmOperation(() =>
        {
            if (_chartManager is null) return null;
            return _chartManager.GetPlotBytes(handle, width, height);
        }, completionCallback);
}
