using System;
using System.Collections.Generic;
using System.Linq;
using Basic.Runtime;
using CommonLib.Consoles;

namespace Basic.Executor;

/// <summary>
/// Lista paginata / selettore — NUOVO MODELLO (GPM-compatibile): controlli C cell
/// (UiAddTbrowse/UiAddStatusbar) guidati da UiFrame/UiPollEvent (niente loop immediate-mode).
/// La navigazione la gestisce il controllo C; l'host reagisce a SELCHANGE e al quit (ESC).
/// </summary>
public static class PaginatedMenu
{
    // Codici evento C (gmb_form.h GMB_EV_*).
    private const int EvSelChange = 22;

    public static void Show(string content, string? title = null)
    {
        if (string.IsNullOrEmpty(content)) return;
        var lines = content.Split(['\r', '\n'], StringSplitOptions.RemoveEmptyEntries);
        Render(lines, title ?? "Items", false);
    }

    public static void Show(IReadOnlyList<string> lines, string? title = null)
    {
        if (lines.Count == 0) return;
        Render(lines, title ?? "Items", false);
    }

    public static string? Select(IReadOnlyList<string> lines, string? title = null)
    {
        if (lines.Count == 0) return null;
        return Render(lines, title ?? "Select", true);
    }

    private static string? Render(IReadOnlyList<string> lines, string title, bool selectable)
    {
        var g = Basic.IlBackend.IlRuntime.Current.Graphics;
        int W = ActiveConsoleIO.WindowWidth;
        int H = Math.Max(ActiveConsoleIO.WindowHeight, 25);

        g.UiControlsReset();
        g.UiAddStatusbar(0, 0, W, $" {title}  |  {lines.Count} items  |  ESC=Quit");
        var colsCsv = ":" + Math.Max(10, W - 4) + ":T:L";
        var dataCsv = string.Join(";", lines.Select(l => l.Replace('|', '/').Replace(';', ',')));
        g.UiAddTbrowse(1, 1, Math.Max(20, W - 2), Math.Max(4, H - 3), colsCsv, dataCsv);
        g.UiAddStatusbar(0, H - 1, W, selectable ? "  ↑↓/Mouse  Enter=Select  ESC=Quit" : "  ↑↓/Mouse  ESC=Quit");

        ActiveConsoleIO.CursorVisible = false;
        string? result = null;
        while (!g.UiClosed() && !ActiveConsoleIO.QuitRequested)
        {
            g.UiFrame(0); // drain tasti + disegno controlli C (render cell)

            while (g.UiPollEvent(out int ev, out int widx))
            {
                if (ev == EvSelChange && selectable)
                {
                    int row = g.UiTbrowseSelRow(widx);
                    if (row >= 0 && row < lines.Count) { result = lines[row]; break; }
                }
            }
            if (result != null) break;
        }

        ActiveConsoleIO.CursorVisible = true;
        if (ActiveConsoleIO.HasConsole) ActiveConsoleIO.Clear();
        return result;
    }
}
