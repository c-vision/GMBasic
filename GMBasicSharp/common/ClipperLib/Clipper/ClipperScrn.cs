using CommonLib.Consoles.Tui;
using CommonLib.Screens;
using System.Text;

namespace ClipperLib.Clipper;

/// <summary>
/// Funzioni di schermo di Clipper 5.3 non ancora coperte da <see cref="ClipperOutput"/>:
/// MAXCOL/MAXROW, SCROLL, SAVESCREEN/RESTSCREEN, TCOLOR, SETBLINK, @...PROMPT e MESSAGE.
/// </summary>
public static class ClipperScrn
{
    private static ClipperState S() => ClipperGlobal.State;

    /// <summary>MAXCOL() — ultima colonna (larghezza buffer - 1).</summary>
    public static int MaxCol() => S().Buffer.Width - 1;

    /// <summary>MAXROW() — ultima riga (altezza buffer - 1).</summary>
    public static int MaxRow() => S().Buffer.Height - 1;

    /// <summary>
    /// SCROLL(top, left, bottom, right, rows) — scrolla la regione verso l'alto
    /// (rows &gt; 0) o verso il basso (rows &lt; 0); le righe lasciate libere sono riempite di spazi.
    /// </summary>
    public static void Scroll(int top, int left, int bottom, int right, int rows)
    {
        if (rows == 0) return;
        var st = S();
        top = Math.Max(0, top);
        left = Math.Max(0, left);
        bottom = Math.Min(st.Buffer.Height - 1, bottom);
        right = Math.Min(st.Buffer.Width - 1, right);
        if (top > bottom || left > right) return;

        var height = bottom - top + 1;
        var shift = rows;
        if (shift > height) shift = height;
        if (shift < -height) shift = -height;

        var w = right - left + 1;
        // Con shift negativo (scroll verso il basso) occorre iterare dal fondo,
        // altrimenti le righe sorgente verrebbero sovrascritte prima di essere lette.
        for (var y = 0; y < height; y++)
        {
            var i = shift < 0 ? height - 1 - y : y;
            var srcY = i + shift;
            if (srcY >= 0 && srcY < height)
            {
                for (var x = 0; x < w; x++)
                    st.Buffer.SetChar(left + x, top + i,
                        st.Buffer.Cells[top + srcY, left + x].Character,
                        st.Buffer.Cells[top + srcY, left + x].ForegroundRgb ?? CellColor.White,
                        st.Buffer.Cells[top + srcY, left + x].BackgroundRgb ?? CellColor.Black);
            }
            else
            {
                for (var x = 0; x < w; x++)
                    st.Buffer.SetChar(left + x, top + i, ' ', st.Fg, st.Bg);
            }
        }
    }

    /// <summary>SAVESCREEN() — serializza l'intero schermo in una stringa.</summary>
    public static string SaveScreen() =>
        SaveScreen(0, 0, S().Buffer.Height - 1, S().Buffer.Width - 1);

    /// <summary>SAVESCREEN(top, left, bottom, right) — serializza una regione dello schermo.</summary>
    public static string SaveScreen(int top, int left, int bottom, int right)
    {
        var st = S();
        top = Math.Max(0, top);
        left = Math.Max(0, left);
        bottom = Math.Min(st.Buffer.Height - 1, bottom);
        right = Math.Min(st.Buffer.Width - 1, right);
        if (top > bottom || left > right) return $"S0;0|";

        var w = right - left + 1;
        var h = bottom - top + 1;
        var sb = new StringBuilder($"S{w};{h}|");
        for (var y = top; y <= bottom; y++)
            for (var x = left; x <= right; x++)
            {
                var c = st.Buffer.Cells[y, x];
                sb.Append(c.Character);
                sb.Append((char)((c.ForegroundRgb ?? CellColor.Default).R + 32));
                sb.Append((char)((c.ForegroundRgb ?? CellColor.Default).G + 32));
                sb.Append((char)((c.ForegroundRgb ?? CellColor.Default).B + 32));
                sb.Append((c.ForegroundRgb ?? CellColor.Default).IsDefault ? '1' : '0');
                sb.Append((char)((c.BackgroundRgb ?? CellColor.Default).R + 32));
                sb.Append((char)((c.BackgroundRgb ?? CellColor.Default).G + 32));
                sb.Append((char)((c.BackgroundRgb ?? CellColor.Default).B + 32));
                sb.Append((c.BackgroundRgb ?? CellColor.Default).IsDefault ? '1' : '0');
            }
        return sb.ToString();
    }

    /// <summary>RESTSCREEN(s) — ripristina l'intero schermo dalla stringa di <see cref="SaveScreen"/>.</summary>
    public static void RestScreen(string s) => RestScreen(s, 0, 0);

    /// <summary>RESTSCREEN(s, top, left) — ripristina la regione serializzata alla posizione data.</summary>
    public static void RestScreen(string s, int top, int left)
    {
        if (string.IsNullOrEmpty(s) || !s.StartsWith('S')) return;
        var semi = s.IndexOf('|');
        if (semi < 0) return;
        var dims = s[1..semi].Split(';');
        if (dims.Length != 2) return;
        if (!int.TryParse(dims[0], out var w) || !int.TryParse(dims[1], out var h)) return;
        if (w <= 0 || h <= 0) return;

        var st = S();
        var idx = semi + 1;
        for (var dy = 0; dy < h; dy++)
            for (var dx = 0; dx < w; dx++)
            {
                if (idx + 8 > s.Length) return;
                var ch = s[idx];
                var fg = new CellColor
                {
                    R = (byte)(s[idx + 1] - 32),
                    G = (byte)(s[idx + 2] - 32),
                    B = (byte)(s[idx + 3] - 32),
                    IsDefault = s[idx + 4] == '1',
                };
                var bg = new CellColor
                {
                    R = (byte)(s[idx + 5] - 32),
                    G = (byte)(s[idx + 6] - 32),
                    B = (byte)(s[idx + 7] - 32),
                    IsDefault = s[idx + 8] == '1',
                };
                st.Buffer.SetChar(left + dx, top + dy, ch, fg, bg);
                idx += 9;
            }
    }

    /// <summary>TCOLOR() — specifica colore corrente (es. "W/N").</summary>
    public static string TColor() => S().ColorSpec;

    /// <summary>SETBLINK(on) — abilita/disabilita il lampeggio del cursore.</summary>
    public static bool SetBlink(bool? on = null)
    {
        var st = S();
        if (on.HasValue)
        {
            st.BlinkEnabled = on.Value;
            if (!on.Value) st.BlinkOn = false;
        }
        return st.BlinkEnabled;
    }

    /// <summary>
    /// @...PROMPT(row, col, text) — imposta il testo del messaggio; se la riga coincide
    /// con la riga di SET MESSAGE, lo mostra in quella riga alla colonna data.
    /// </summary>
    public static void Prompt(int row, int col, string text)
    {
        var st = S();
        if (st.MessageRow.HasValue && st.MessageRow.Value == row)
        {
            var fg = st.Fg;
            var bg = st.Bg;
            if (st.MessageText.Length > 0)
            {
                var blank = new string(' ', st.MessageText.Length);
                st.Buffer.SetString(st.MessageCol, st.MessageRow.Value, blank, fg, bg);
            }
            st.Buffer.SetString(col, st.MessageRow.Value, text, fg, bg);
            st.MessageCol = col;
        }
        st.MessageText = text;
    }

    /// <summary>MESSAGE() — testo corrente del messaggio.</summary>
    public static string Message() => ClipperSet.Message();
}
