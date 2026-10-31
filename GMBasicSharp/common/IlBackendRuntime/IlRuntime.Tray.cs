// FILE: IlRuntime.Tray.cs — namespace TRAY (icona nella barra del SO).
// Controparte gestita (§0bis): P/Invoke alla libreria C del runtime condiviso
// (native/gmb_runtime_c/src/gmb_tray*.c) via CommonLib/TrayUtils.cs — la stessa
// usata dai backend C. Nessuna logica di piattaforma qui.
// Main thread; il callback degli item va dispatchato come task GPM.

namespace Basic.IlBackend;

public static partial class IlRuntime
{
    public static long TrayCreate(string icon, string tooltip) => TrayUtils.Create(icon, tooltip);
    public static long TrayItem(long id, string label) => TrayUtils.Item((int)id, label);
    public static long TraySep() => TrayUtils.Sep();
    public static long TrayTooltip(string text) => TrayUtils.Tooltip(text);
    public static long TrayPoll() => TrayUtils.Poll();
    public static long TrayRemove() { TrayUtils.Remove(); return 0; }
    public static string TrayBackend() => TrayUtils.Backend();
    // Convenzione GMBasic per i predicati: -1 vero / 0 falso.
    public static long TraySelfTest() => TrayUtils.SelfTest() ? -1 : 0;
    public static long TrayAvailable() => TrayUtils.Available() ? -1 : 0;
    public static long TrayLast() => TrayUtils.Last();
}
