// FILE: Interpreter.Gpm.Plotting.Chart.Core.cs

using System.Collections.Concurrent;
using System.Globalization;
using OxyPlot;
using OxyPlot.Axes;
using OxyPlot.Series;

namespace CommonGraphics.Graphics;

/// <summary>
/// Contiene la logica di business pura e il gestore "headless" per il sottosistema di plotting.
/// Questa classe è responsabile della creazione e manipolazione di oggetti grafici in memoria
/// utilizzando la libreria OxyPlot.Core, e del loro rendering in formato PNG.
/// </summary>
/// <summary>
/// Motore di plotting headless (OxyPlot) — portato da Interpreter deprecato (2026-08-07).
/// Crea grafici in memoria (handle LONG) e li rende in PNG.
/// </summary>
internal sealed class ChartPlotManager
{
    /// <summary>
    /// UDT interno che rappresenta i dati per un grafico OHLC.
    /// </summary>
    internal sealed record OhlcData(double Open, double High, double Low, double Close, DateTime Timestamp);

    /// <summary>
    /// Gestore centralizzato per tutti gli oggetti grafici creati durante l'esecuzione.
    /// Questa classe è thread-safe.
    /// </summary>
    internal sealed class PlotManager
    {
        private readonly ConcurrentDictionary<long, PlotModel> _plots = new();
        private long _nextHandle;

        /// <summary>
        /// Crea un nuovo oggetto PlotModel in memoria e restituisce il suo handle.
        /// </summary>
        public long CreatePlot(string title, string xLabel, string yLabel)
        {
            var handle = Interlocked.Increment(ref _nextHandle);
            var model = new PlotModel { Title = title };

            model.Axes.Add(new LinearAxis { Position = AxisPosition.Bottom, Title = xLabel });
            model.Axes.Add(new LinearAxis { Position = AxisPosition.Left, Title = yLabel });

            _plots[handle] = model;
            return handle;
        }

        /// <summary>
        /// Recupera un oggetto PlotModel in base al suo handle.
        /// </summary>
        private PlotModel GetPlot(long handle)
        {
            if (_plots.TryGetValue(handle, out var plot))
            {
                return plot;
            }

            throw new ArgumentException(
                $"Nessun grafico trovato con handle {handle}. Potrebbe essere stato già chiuso.");
        }

        /// <summary>
        /// Aggiunge una serie di dati generici a un grafico.
        /// </summary>
        public void PlotData(long handle, double[] xData, double[] yData, string plotType, string legend)
        {
            var model = GetPlot(handle);

            Series series = plotType.ToUpperInvariant() switch
            {
                "SCATTER" => new ScatterSeries { Title = legend, MarkerType = MarkerType.Circle },
                "BAR" => new BarSeries { Title = legend },
                "PIE" => new PieSeries { Title = legend },
                "LINE" => new LineSeries { Title = legend },
                _ => new LineSeries { Title = legend } // Default
            };

            // Popola la serie con i dati
            switch (series)
            {
                case LineSeries s:
                    for (var i = 0; i < xData.Length; i++) s.Points.Add(new DataPoint(xData[i], yData[i]));
                    break;
                case ScatterSeries s:
                    for (var i = 0; i < xData.Length; i++) s.Points.Add(new ScatterPoint(xData[i], yData[i]));
                    break;
                case BarSeries s:
                    for (var i = 0; i < yData.Length; i++) s.Items.Add(new BarItem(yData[i], i));
                    if (s.Items.Count > 0)
                        model.Axes.Add(new CategoryAxis
                        {
                            Position = AxisPosition.Bottom,
                            ItemsSource = xData.Select(x => x.ToString(CultureInfo.InvariantCulture))
                        });
                    break;
                case PieSeries s:
                    for (var i = 0; i < yData.Length; i++)
                        s.Slices.Add(new PieSlice(
                            xData.Length > i ? xData[i].ToString(CultureInfo.InvariantCulture) : "", yData[i]));
                    break;
            }

            model.Series.Add(series);
            model.IsLegendVisible = true;
            model.InvalidatePlot(true);
        }

        /// <summary>Bar chart with string category labels (used by LEDGER.PLOT).</summary>
        public void PlotCategoryBar(long handle, string[] categories, double[] values, string legend)
        {
            var model = GetPlot(handle);
            var series = new BarSeries { Title = legend };
            for (var i = 0; i < values.Length; i++)
            {
                series.Items.Add(new BarItem(values[i], i));
            }

            if (series.Items.Count > 0)
            {
                model.Axes.Add(new CategoryAxis
                {
                    Position = AxisPosition.Bottom,
                    ItemsSource = categories,
                });
            }

            model.Series.Add(series);
            model.IsLegendVisible = true;
            model.InvalidatePlot(true);
        }

        /// <summary>
        /// Aggiunge una serie di dati finanziari OHLC a un grafico.
        /// </summary>
        public void PlotFinancial(long handle, OhlcData[] ohlcData, string plotType, string legend)
        {
            var model = GetPlot(handle);

            // OxyPlot per OHLC usa il tipo HighLowItem
            var items = ohlcData.Select(d => new HighLowItem(
                DateTimeAxis.ToDouble(d.Timestamp),
                d.High,
                d.Low,
                d.Open,
                d.Close)).ToList();

            XYAxisSeries series = plotType.ToUpperInvariant() switch
            {
                "CANDLESTICK" => new CandleStickSeries { Title = legend, ItemsSource = items },
                "OHLC" => new HighLowSeries { Title = legend, ItemsSource = items },
                _ => new CandleStickSeries { Title = legend, ItemsSource = items }
            };

            // Assicura che l'asse X sia di tipo DateTime
            var xAxis = model.Axes.FirstOrDefault(a => a.Position == AxisPosition.Bottom);
            if (xAxis is not DateTimeAxis)
            {
                model.Axes.Remove(xAxis);
                model.Axes.Add(new DateTimeAxis { Position = AxisPosition.Bottom, Title = "Date" });
            }

            model.Series.Add(series);
            model.IsLegendVisible = true;
            model.InvalidatePlot(true);
        }

        /// <summary>
        /// Rimuove tutte le serie di dati da un grafico.
        /// </summary>
        public void ClearPlot(long handle)
        {
            var model = GetPlot(handle);
            model.Series.Clear();
            model.InvalidatePlot(true);
        }

        /// <summary>
        /// Rilascia un oggetto grafico dalla memoria.
        /// </summary>
        public void ClosePlot(long handle)
        {
            _plots.TryRemove(handle, out _);
        }

        /// <summary>
        /// Renderizza un grafico in un array di byte in formato BMP, utilizzando un renderer custom
        /// per eliminare dipendenze esterne.
        /// </summary>
        public byte[] GetPlotBytes(long handle, int width, int height)
        {
            var model = GetPlot(handle);
            var renderContext = new BmpRenderContext(width, height);
            // Esegue il cast a IPlotModel per accedere al metodo Render esplicito.
            ((IPlotModel)model).Render(renderContext, new OxyRect(0, 0, width, height));
            // Passa l'intero contesto all'encoder.
            return BmpEncoder.Encode(renderContext);
        }
    }
    
    // ========================================================================
    // --- Renderer e Encoder BMP Custom per Plotting Headless ---
    // ========================================================================

    /// <summary>
    /// Un RenderContext custom che disegna su un buffer di byte in memoria (pixel data grezzi).
    /// Questa implementazione è minimale e supporta solo le operazioni di base necessarie
    /// per renderizzare i grafici.
    /// </summary>
    private class BmpRenderContext : IRenderContext
    {
        public readonly byte[] PixelData;
        public readonly int Width;
        public readonly int Height;

        public bool RendersToScreen => false; // Disegniamo su un buffer in memoria
        public int ClipCount => 0;

        public BmpRenderContext(int width, int height)
        {
            Width = width;
            Height = height;
            PixelData = new byte[width * height * 3];
            for (var i = 0; i < PixelData.Length; i++) PixelData[i] = 255; // Sfondo bianco
        }

        private void SetPixel(int x, int y, OxyColor color)
        {
            if (x < 0 || x >= Width || y < 0 || y >= Height) return;
            var index = ((Height - 1 - y) * Width + x) * 3;
            PixelData[index] = color.B;
            PixelData[index + 1] = color.G;
            PixelData[index + 2] = color.R;
        }

        private void DrawSimpleLine(ScreenPoint p1, ScreenPoint p2, OxyColor color)
        {
            int x0 = (int)p1.X, y0 = (int)p1.Y;
            int x1 = (int)p2.X, y1 = (int)p2.Y;
            int dx = Math.Abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
            int dy = -Math.Abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
            var err = dx + dy;

            while (true)
            {
                SetPixel(x0, y0, color);
                if (x0 == x1 && y0 == y1) break;
                var e2 = 2 * err;
                if (e2 >= dy)
                {
                    err += dy;
                    x0 += sx;
                }

                if (e2 <= dx)
                {
                    err += dx;
                    y0 += sy;
                }
            }
        }

        public void DrawLine(IList<ScreenPoint> points, OxyColor stroke, double thickness,
            EdgeRenderingMode edgeRenderingMode, double[]? dashArray, LineJoin lineJoin)
        {
            for (var i = 0; i < points.Count - 1; i++)
            {
                DrawSimpleLine(points[i], points[i + 1], stroke);
            }
        }

        public void DrawPolygon(IList<ScreenPoint> points, OxyColor fill, OxyColor stroke, double thickness,
            EdgeRenderingMode edgeRenderingMode, double[]? dashArray, LineJoin lineJoin)
        {
            if (stroke.IsVisible())
            {
                for (var i = 0; i < points.Count; i++)
                {
                    DrawSimpleLine(points[i], points[(i + 1) % points.Count], stroke);
                }
            }
            // Il riempimento (fill) non è implementato in questa versione minimale.
        }

        // --- IMPLEMENTAZIONE DEI MEMBRI MANCANTI DELL'INTERFACCIA ---
        // La maggior parte di questi non sono necessari per il rendering di base e possono essere lasciati vuoti.
        public void CleanUp()
        {
        }

        public void DrawEllipse(OxyRect rect, OxyColor fill, OxyColor stroke, double thickness,
            EdgeRenderingMode edgeRenderingMode)
        {
        }

        public void DrawEllipses(IList<OxyRect> rectangles, OxyColor fill, OxyColor stroke, double thickness,
            EdgeRenderingMode edgeRenderingMode)
        {
        }

        public void DrawLineSegments(IList<ScreenPoint> points, OxyColor stroke, double thickness,
            EdgeRenderingMode edgeRenderingMode, double[]? dashArray, LineJoin lineJoin)
        {
            for (var i = 0; i < points.Count - 1; i += 2)
            {
                DrawSimpleLine(points[i], points[i + 1], stroke);
            }
        }

        public void DrawPolygons(IList<IList<ScreenPoint>> polygons, OxyColor fill, OxyColor stroke, double thickness,
            EdgeRenderingMode edgeRenderingMode, double[]? dashArray, LineJoin lineJoin)
        {
            foreach (var polygon in polygons)
            {
                DrawPolygon(polygon, fill, stroke, thickness, edgeRenderingMode, dashArray, lineJoin);
            }
        }

        public void DrawRectangle(OxyRect rect, OxyColor fill, OxyColor stroke, double thickness,
            EdgeRenderingMode edgeRenderingMode)
        {
            var p1 = new ScreenPoint(rect.Left, rect.Top);
            var p2 = new ScreenPoint(rect.Right, rect.Top);
            var p3 = new ScreenPoint(rect.Right, rect.Bottom);
            var p4 = new ScreenPoint(rect.Left, rect.Bottom);
            DrawPolygon([p1, p2, p3, p4], fill, stroke, thickness, edgeRenderingMode, [], LineJoin.Miter);
        }

        public void DrawRectangles(IList<OxyRect> rectangles, OxyColor fill, OxyColor stroke, double thickness,
            EdgeRenderingMode edgeRenderingMode)
        {
            foreach (var rect in rectangles)
            {
                DrawRectangle(rect, fill, stroke, thickness, edgeRenderingMode);
            }
        }

        public void DrawText(ScreenPoint p, string text, OxyColor fill, string? fontFamily, double fontSize,
            double fontWeight, double rotate, HorizontalAlignment halign, VerticalAlignment valign, OxySize? maxSize)
        {
        }

        public OxySize MeasureText(string text, string? fontFamily, double fontSize, double fontWeight) => OxySize.Empty;

        public void DrawImage(OxyImage source, double srcX, double srcY, double srcWidth, double srcHeight,
            double destX,
            double destY, double destWidth, double destHeight, double opacity, bool interpolate)
        {
        }

        public bool SetClip() => true;

        public void PushClip(OxyRect rect)
        {
        }

        public void PopClip()
        {
        }

        public void ResetClip()
        {
        }

        public void SetToolTip(string text)
        {
        }
    }
    
    /// <summary>
    /// Helper per codificare un buffer di pixel RGB in un formato file BMP a 24-bit.
    /// </summary>
    private static class BmpEncoder
    {
        public static byte[] Encode(BmpRenderContext context)
        {
            var pixelData = context.PixelData;
            var width = context.Width;
            var height = context.Height;

            var rowSize = (24 * width + 31) / 32 * 4;
            var imageSize = rowSize * height;
            var fileSize = 54 + imageSize;

            using var ms = new MemoryStream();
            using var writer = new BinaryWriter(ms);

            // File Header
            writer.Write((byte)'B'); writer.Write((byte)'M');
            writer.Write(fileSize);
            writer.Write((short)0); writer.Write((short)0);
            writer.Write(54); // Offset

            // Info Header
            writer.Write(40); // Header size
            writer.Write(width);
            writer.Write(height);
            writer.Write((short)1); // Planes
            writer.Write((short)24); // Bits per pixel
            writer.Write(0); // Compression
            writer.Write(imageSize);
            writer.Write(0); writer.Write(0);
            writer.Write(0); writer.Write(0);

            // Pixel Data
            var padding = rowSize - (width * 3);
            var paddingBytes = padding > 0 ? new byte[padding] : null;
            for (var y = 0; y < height; y++)
            {
                writer.Write(pixelData, y * width * 3, width * 3);
                if (paddingBytes != null)
                {
                    writer.Write(paddingBytes, 0, padding);
                }
            }
            return ms.ToArray();
        }
    }
}