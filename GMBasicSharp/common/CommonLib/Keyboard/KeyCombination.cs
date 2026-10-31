// FILE: CommonLib.Keyboard.KeyCombination.cs

namespace CommonLib.Keyboard;

/// <summary>Combinazione di tasti con modificatori (Ctrl, Alt, Shift).</summary>
public readonly record struct KeyCombination(ConsoleKey Key, ConsoleModifiers Modifiers = 0);
