// FILE: Basic.Runtime.IServices.IConsoleService.cs

namespace Basic.Runtime;

using CommonLib.Screens;
using CommonLib.Consoles;

/// <summary>
/// Servizio console astratto per il BASIC Runtime.
/// Backend-agnostic: può essere implementato da SystemConsole,
/// StandardConsole, HeadlessConsole, o un wrapper JS per Wasm.
/// </summary>
public interface IConsoleService
{
    // === Output ===
    void Clear();
    void Print(string? text);
    void PrintLine(string? text);
    void PrintNewLine();
    void SetForegroundColor(int colorIndex);
    void SetBackgroundColor(int colorIndex);
    void ResetColor();
    void SetCursorPosition(int left, int top);
    void Beep();

    // === Proprietà ===
    int CursorLeft { get; set; }
    int CursorTop { get; set; }
    int WindowWidth { get; }
    int WindowHeight { get; }
    string Title { get; set; }

    // === Palette ===
    void SetPaletteColor(int index, int r, int g, int b);
    void SetActivePalette(byte[][] palette);
    byte[][] GetActivePalette();

    // === Scroll ===
    void SetScrollRegion(int top, int bottom);

    // === Buffer schermo (SAVE/RESTORE SCREEN in modalità testo) ===
    // Stessa firma di IConsole.Get/Put (ScreenRect, ScreenCell[,]) — buffer a griglia 2D.
    // Default: buffer non disponibile (le console native non espongono la griglia).
    ScreenCell[,]? GetScreen(ScreenRect rect) => null;
    void PutScreen(int x, int y, ScreenCell[,] cells) { }

    // === TUI ===
    void Alert(string message, string title);
    string? GetInput(string prompt, string? mask);
    string? GetPassword(string prompt);
    bool Confirm(string message, string title);
    int? Menu(string title, string[] items);
    string? FileDialog(string title, string pattern, bool save);
    string? ColorPicker(string title, int defaultColor);
    string? DatePicker(string title, string defaultDate);
    int? Spinner(string title, string[] items);
    bool Toggle(string title, string prompt, bool initial);
    double? Slider(string title, double min, double max, double initial);
    void MessageBox(string title, string message);
}
