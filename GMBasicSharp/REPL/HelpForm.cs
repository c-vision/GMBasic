using System;
using System.Collections.Generic;
using System.Linq;
using Basic.Runtime;
using CommonLib.Consoles;

namespace Basic.Executor;

/// <summary>
/// Help browser — NUOVO MODELLO (GPM-compatibile): la UI è costruita con i controlli C cell
/// (UiAddTbrowse/UiAddStatusbar) e guidata da <see cref="IGraphicsService.UiFrame"/> +
/// <see cref="IGraphicsService.UiPollEvent"/> (niente loop immediate-mode con ClipperInput.InKey).
/// Navegazione/tasti li gestisce il controllo C; l'host reagisce agli eventi (select/quit).
/// </summary>
public class HelpForm
{
    private readonly HelpIndex _index;
    private List<HelpEntry> _allEntries = new();
    private List<HelpEntry> _filtered = new();

    // Codici evento C (gmb_form.h GMB_EV_*).
    private const int EvChange = 3;
    private const int EvSelChange = 22;

    public HelpForm(HelpIndex index)
    {
        _index = index;
        foreach (var ns in index.GetNamespaces())
        {
            _allEntries.Add(new HelpEntry
            {
                Name = $"<{ns}>",
                Namespace = ns,
                Description = $"Namespace {ns} — {index.GetByNamespace(ns).Count} items",
                IsCommand = false
            });
            _allEntries.AddRange(index.GetByNamespace(ns));
        }
        _filtered = new List<HelpEntry>(_allEntries);
    }

    public void Show()
    {
        var g = Basic.IlBackend.IlRuntime.Current.Graphics;
        int W = ActiveConsoleIO.WindowWidth;
        int H = Math.Max(ActiveConsoleIO.WindowHeight, 25);

        ActiveConsoleIO.CursorVisible = false;
        RegisterAll(g, W, H);

        while (!g.UiClosed() && !ActiveConsoleIO.QuitRequested)
        {
            g.UiFrame(0); // drain tasti + disegno controlli C (render cell)

            while (g.UiPollEvent(out int ev, out int widx))
            {
                if (ev == EvSelChange)
                {
                    int row = g.UiTbrowseSelRow(widx);
                    if (row >= 0 && row < _filtered.Count)
                    {
                        if (Open(_filtered[row]))
                            RegisterAll(g, W, H); // i dati sono cambiati: ri-registra
                    }
                }
                else if (ev == EvChange)
                {
                    // (futuro: filtro dal TextInput C)
                }
            }
        }

        ActiveConsoleIO.CursorVisible = true;
        if (ActiveConsoleIO.HasConsole) ActiveConsoleIO.Clear();
    }

    private void RegisterAll(IGraphicsService g, int W, int H)
    {
        g.UiControlsReset();
        g.UiAddStatusbar(0, 0, W, "  GMBASIC Help  |  ↑↓/Mouse  Enter=Open  ESC=Quit");
        var colsCsv = "Name:24:T:L|Namespace:25:T:L|Description:" + Math.Max(10, W - 56) + ":T:L";
        var dataCsv = string.Join(";", _filtered.Select(e => $"{e.Name}|{e.Namespace ?? ""}|{e.Description}"));
        g.UiAddTbrowse(1, 3, Math.Max(20, W - 2), Math.Max(4, H - 5), colsCsv, dataCsv);
        g.UiAddStatusbar(0, H - 1, W, $"  {_filtered.Count} items");
    }

    /// <summary>Apre l'entry selezionata. Ritorna true se i dati sono cambiati (ri-registrare).</summary>
    private bool Open(HelpEntry entry)
    {
        if (entry.IsCommand == false && entry.Name.StartsWith("<") && entry.Name.EndsWith(">"))
        {
            _filtered = _index.GetByNamespace(entry.Namespace).ToList();
            return true;
        }
        PaginatedMenu.Show(BuildDetail(entry), entry.FullName);
        return false;
    }

    private static string BuildDetail(HelpEntry entry)
    {
        var sb = new System.Text.StringBuilder();
        sb.AppendLine($"{entry.FullName}");
        sb.AppendLine(new string('=', entry.FullName.Length));
        sb.AppendLine();
        if (!string.IsNullOrEmpty(entry.Description)) sb.AppendLine(entry.Description);
        if (entry.Syntax.Count > 0)
        {
            sb.AppendLine(); sb.AppendLine("Syntax:");
            foreach (var s in entry.Syntax) sb.AppendLine($"  {s}");
        }
        if (!string.IsNullOrEmpty(entry.ReturnType)) { sb.AppendLine(); sb.AppendLine($"Returns: {entry.ReturnType}"); }
        if (entry.Parameters.Count > 0)
        {
            sb.AppendLine(); sb.AppendLine("Parameters:");
            foreach (var p in entry.Parameters) sb.AppendLine($"  {p.Name} ({p.Type}) — {p.Description}");
        }
        if (entry.Examples.Count > 0)
        {
            sb.AppendLine(); sb.AppendLine("Examples:");
            foreach (var ex in entry.Examples)
            {
                if (!string.IsNullOrEmpty(ex.Description)) sb.AppendLine($"  // {ex.Description}");
                if (!string.IsNullOrEmpty(ex.Code)) sb.AppendLine($"  {ex.Code}");
                if (!string.IsNullOrEmpty(ex.ExpectedOutput)) sb.AppendLine($"  => {ex.ExpectedOutput}");
                sb.AppendLine();
            }
        }
        if (entry.SeeAlso.Count > 0)
        {
            sb.AppendLine(); sb.AppendLine("See also:");
            foreach (var sa in entry.SeeAlso) sb.AppendLine($"  {sa}");
        }
        return sb.ToString();
    }
}
