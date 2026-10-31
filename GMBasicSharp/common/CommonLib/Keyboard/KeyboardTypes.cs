// FILE: CommonLib.Keyboard.KeyboardTypes.cs

using System.Text;
using CommonLib.Consoles;

namespace CommonLib.Keyboard;

/// <summary>Interfaccia marcatore per le richieste di input bloccanti.</summary>
public interface IBlockingRequest { }

/// <summary>Interfaccia marcatore per le richieste di input sincrono (polling).</summary>
public interface ISyncBlockingRequest : IBlockingRequest { }

/// <summary>Rappresenta una richiesta di polling per un tasto (INKEY$).</summary>
public sealed record PollKeyRequest(bool ConvertToUppercase) : ISyncBlockingRequest;

/// <summary>Rappresenta una richiesta per un singolo tasto (GET, WAITKEY).</summary>
public sealed record ReadKeyRequest(TaskCompletionSource<ConsoleKeyInfo> Tcs, bool ConvertToUppercase) : IBlockingRequest;

/// <summary>Rappresenta una richiesta per una riga di testo (INPUT, LINE INPUT).</summary>
public sealed class ReadLineRequest : IBlockingRequest
{
    public IConsole Console { get; }
    public StringBuilder InputBuffer { get; }
    public string Prompt { get; }
    public TaskCompletionSource<string?> Tcs { get; }
    public Func<char, string, (bool process, char convertedChar)>? CharConversionRule { get; }
    public int CursorPosition { get; set; }

    public ReadLineRequest(
        IConsole console,
        StringBuilder inputBuffer,
        string prompt,
        TaskCompletionSource<string?> tcs,
        Func<char, string, (bool process, char convertedChar)>? charConversionRule = null)
    {
        Console = console;
        InputBuffer = inputBuffer;
        Prompt = prompt;
        Tcs = tcs;
        CharConversionRule = charConversionRule;
        CursorPosition = inputBuffer.Length;
    }
}
