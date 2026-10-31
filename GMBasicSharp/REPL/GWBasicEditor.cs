using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using Basic.Runtime;
using CommonLib.Consoles;

namespace Basic.Executor;

/// <summary>
/// Editor GWBASIC — NUOVO MODELLO (GPM-compatibile): i controlli sono quelli C cell
/// (UiAddCodeArea/UiAddStatusbar), il loop è UiFrame/UiPollEvent (niente immediate-mode).
/// Il modello IDE (documento/tab/analisi) vive in C (gmb_ide via CIdeNative).
/// </summary>
public class GWBasicEditor
{
    private readonly SortedDictionary<int, string> _programLines;
    private string _fileName;
    private bool _dirty;
    private int _tab;

    public string Name => "GMBASIC";
    public bool CloseRequested { get; private set; }
    public bool CursorVisible => false;

    public GWBasicEditor(SortedDictionary<int, string> programLines, string fileName)
    {
        _programLines = programLines;
        _fileName = fileName;
    }

    private string FileText() => string.Join('\n', _programLines.Select(kv => $"{kv.Key} {kv.Value}"));

    private void LoadProgramIntoEditor()
    {
        var ide = CIdeNative.Ide;
        _tab = CIdeNative.TabOpen(Path.GetFileName(_fileName), FileText());
    }

    private void SaveEditorIntoProgram()
    {
        _programLines.Clear();
        var text = CIdeNative.DocText(_tab);
        int anon = 10;
        foreach (var raw in text.Replace("\r", "").Split('\n'))
        {
            var sp = raw.IndexOf(' ');
            if (sp > 0 && int.TryParse(raw[..sp], out var num) && !string.IsNullOrWhiteSpace(raw[(sp + 1)..]))
                _programLines[num] = raw[(sp + 1)..];
            else if (!string.IsNullOrWhiteSpace(raw) && !int.TryParse(raw.Trim(), out _))
            { while (_programLines.ContainsKey(anon)) anon += 10; _programLines[anon] = raw.Trim(); }
        }
        _dirty = false;
    }

    private void SaveFileClean()
    {
        SaveEditorIntoProgram();
        try { File.WriteAllLines(_fileName, _programLines.Select(kv => $"{kv.Key} {kv.Value}")); } catch { }
    }

    public void Run(bool showWelcome = true)
    {
        var g = Basic.IlBackend.IlRuntime.Current.Graphics;
        int W = ActiveConsoleIO.WindowWidth;
        int H = Math.Max(ActiveConsoleIO.WindowHeight, 25);

        LoadProgramIntoEditor();

        int rows = Math.Max(4, H - 4);
        g.UiControlsReset();
        g.UiAddStatusbar(0, 0, W, $"  {Path.GetFileName(_fileName)}{(_dirty ? " [*]" : "")}   F1=Help F3=Save F5=Run F10=Menu ESC=Exit");
        g.UiAddCodeArea(0, 2, W, rows, CIdeNative.DocText(_tab));
        g.UiAddStatusbar(0, H - 1, W, "  GMBASIC Editor (nuovo modello)");

        ActiveConsoleIO.CursorVisible = false;
        while (!g.UiClosed() && !ActiveConsoleIO.QuitRequested)
        {
            g.UiFrame(0); // drain tasti (controllo C) + disegno + render cell
            while (g.UiPollEvent(out int ev, out int widx))
            {
                // SELCHANGE (22) = azione sul CodeArea (es. F5/Run). Estendibile.
                _ = ev; _ = widx;
            }
            // Shortcut host (tasti non consumati dal CodeArea C).
            int hk = g.UiPollKey();
            if (hk == (int)ConsoleKey.F3) { SaveFileClean(); _dirty = false; }
            else if (hk == (int)ConsoleKey.F5) { SaveFileClean(); _dirty = false; }
        }

        // Uscita (ESC dalla frame C o quit): salva il programma in memoria.
        SaveEditorIntoProgram();
        ActiveConsoleIO.CursorVisible = true;
        if (ActiveConsoleIO.HasConsole) ActiveConsoleIO.Clear();
    }
}
