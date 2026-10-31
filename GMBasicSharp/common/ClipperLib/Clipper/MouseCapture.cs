using CommonLib.Consoles.Tui;
using CommonLib.Screens;
namespace ClipperLib.Clipper;

internal static class MouseCapture
{
    private static bool _enabled;
    private static bool _motion;

    public static void Enable()
    {
        if (_enabled || Console.IsInputRedirected) return;
        _enabled = true;
        _motion = false;
        try
        {
            Console.Out.Write(Ansi.MouseTrackingOn + Ansi.MouseSgrOn);
            Console.Out.Flush();
            DrainResponses();
        }
        catch { }
    }

    /// <summary>
    /// Abilita anche il tracciamento del movimento del mouse (mode ?1003h) così
    /// che l'applicazione possa rispondere all'hover. Il terminale invia eventi
    /// di movimento ad ogni cella attraversata, quindi va usato solo quando il
    /// programma è in grado di gestire un flusso fitto di eventi.
    /// </summary>
    public static void EnableMotion()
    {
        if (_motion || Console.IsInputRedirected) return;
        _motion = true;
        _enabled = true;
        try
        {
            Console.Out.Write(Ansi.MouseMotionOn + Ansi.MouseSgrOn);
            Console.Out.Flush();
            DrainResponses();
        }
        catch { }
    }

    public static void Disable()
    {
        _enabled = false;
        _motion = false;
        if (Console.IsInputRedirected) return;
        try
        {
            Console.Out.Write(Ansi.MouseTrackingOff + Ansi.MouseMotionOff + Ansi.MouseSgrOff + Ansi.MousePassThroughOff);
            Console.Out.Flush();
        }
        catch { }
    }

    /// <summary>
    /// Consuma per pochi ms eventuali risposte del terminale (DECRPM) arrivate
    /// su stdin dopo l'abilitazione del mouse tracking. Se l'utente digita in
    /// questa finestra le sequenze vengono perse, ma 5 ms sono troppo brevi
    /// perché un essere umano le generi accidentalmente.
    /// </summary>
    private static void DrainResponses()
    {
        var sw = System.Diagnostics.Stopwatch.StartNew();
        while (sw.ElapsedMilliseconds < 5 && Console.KeyAvailable)
            Console.ReadKey(true);
    }
}
