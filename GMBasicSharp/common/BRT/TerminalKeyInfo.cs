// FILE: Basic.Runtime.TerminalKeyInfo.cs

namespace Basic.Runtime;

/// <summary>
/// Rappresenta le informazioni su un tasto premuto.
/// Equivalente backend-agnostic di System.ConsoleKeyInfo.
/// </summary>
public readonly record struct TerminalKeyInfo(char KeyChar, int KeyCode, bool Shift, bool Alt, bool Ctrl);
