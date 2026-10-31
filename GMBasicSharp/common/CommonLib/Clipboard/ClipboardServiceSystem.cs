// FILE: Interpreter.Clipboard.ClipboardServiceSystem.cs

using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Text;

namespace CommonLib.Clipboard;

/// <summary>
/// Implementazione di <see cref="IClipboardService"/> che utilizza le API native
/// del sistema operativo o utility a riga di comando per interagire con gli appunti.
/// Questa implementazione è attiva quando l'interprete è in modalità console standard.
/// </summary>
public class ClipboardServiceSystem : IClipboardService
{
#if WINDOWS
    // P/Invoke per le funzioni della clipboard di Windows per GET
    [DllImport("user32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool OpenClipboard(IntPtr hWndNewOwner);

    [DllImport("user32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool CloseClipboard();

    [DllImport("user32.dll", SetLastError = true)]
    private static extern IntPtr GetClipboardData(uint uFormat);

    [DllImport("kernel32.dll")]
    private static extern IntPtr GlobalLock(IntPtr hMem);

    [DllImport("kernel32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool GlobalUnlock(IntPtr hMem);

    private const uint CF_UNICODETEXT = 13;
#endif

    /// <summary>
    /// Recupera il testo attualmente presente negli appunti di sistema.
    /// Utilizza P/Invoke su Windows e processi esterni (`pbpaste`, `xclip`) su macOS e Linux.
    /// </summary>
    /// <returns>Il testo degli appunti, o una stringa vuota se l'operazione fallisce.</returns>
    public string GetText()
    {
        try
        {
            if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
            {
                return GetTextWindows();
            }
            if (RuntimeInformation.IsOSPlatform(OSPlatform.OSX))
            {
                return RunProcess("pbpaste");
            }

            if (!RuntimeInformation.IsOSPlatform(OSPlatform.Linux)) return string.Empty;
            try
            {
                return RunProcess("xclip", "-selection clipboard -o");
            }
            catch
            {
                return RunProcess("xsel", "--clipboard --output");
            }
        }
        catch
        {
            return string.Empty;
        }
    }
    
    /// <summary>
    /// Imposta il testo specificato negli appunti di sistema.
    /// </summary>
    /// <param name="text">Il testo da inserire negli appunti.</param>
    public void SetText(string? text)
    {
        // La logica per questa funzione non è stata fornita.
        // Verrà implementata una volta ricevuto il codice per le API di sistema (es. SetClipboardData, pbcopy).
    }

    /// <summary>
    /// Cancella il contenuto corrente degli appunti di sistema.
    /// </summary>
    public void Clear()
    {
        // La logica per questa funzione non è stata fornita.
        // Verrà implementata una volta ricevuto il codice per le API di sistema (es. EmptyClipboard).
    }

    /// <summary>
    /// Esegue un processo esterno e cattura il suo output standard.
    /// </summary>
    /// <param name="fileName">Il nome dell'eseguibile da avviare.</param>
    /// <param name="arguments">Gli argomenti da passare al processo.</param>
    /// <returns>L'output standard del processo come stringa.</returns>
    private static string RunProcess(string fileName, string? arguments = null)
    {
        using var process = new Process();
        process.StartInfo = new ProcessStartInfo
        {
            FileName = fileName,
            Arguments = arguments,
            RedirectStandardOutput = true,
            UseShellExecute = false,
            CreateNoWindow = true,
            StandardOutputEncoding = Encoding.UTF8
        };
        process.Start();
        var result = process.StandardOutput.ReadToEnd();
        process.WaitForExit();
        return result;
    }

    /// <summary>
    /// Implementazione specifica per Windows per recuperare il testo dagli appunti.
    /// Richiede l'esecuzione su un thread con ApartmentState.STA.
    /// </summary>
    /// <returns>Il testo dagli appunti o stringa vuota in caso di fallimento.</returns>
    private static string GetTextWindows()
    {
#if !WINDOWS
        return string.Empty;
#else
        string? clipboardText = null;
        var staThread = new Thread(() =>
        {
            try
            {
                if (!OpenClipboard(IntPtr.Zero)) 
                {
                    clipboardText = string.Empty;
                    return;
                }

                var hGlobal = GetClipboardData(CF_UNICODETEXT);
                if (hGlobal == IntPtr.Zero)
                {
                    clipboardText = string.Empty;
                    CloseClipboard();
                    return;
                }

                var lpwcstr = GlobalLock(hGlobal);
                if (lpwcstr == IntPtr.Zero)
                {
                    clipboardText = string.Empty;
                    CloseClipboard();
                    return;
                }

                clipboardText = Marshal.PtrToStringUni(lpwcstr);
                GlobalUnlock(hGlobal);
            }
            catch
            {
                clipboardText = string.Empty;
            }
            finally
            {
                // Assicura che la clipboard venga sempre chiusa.
                CloseClipboard();
            }
        });
        staThread.SetApartmentState(ApartmentState.STA);
        staThread.Start();
        staThread.Join();
        return clipboardText ?? string.Empty;
#endif
    }
}