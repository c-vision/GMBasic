using CommonLib.Screens;
namespace ClipperLib.Clipper;

/// <summary>Unified UI event: keyboard, mouse, resize or paste.</summary>
public abstract record UiEvent;

public sealed record UiKey(int Key, KeyModifiers Modifiers) : UiEvent;
public sealed record UiMouse(MouseAction Action) : UiEvent;
public sealed record UiResize(int Width, int Height) : UiEvent;
public sealed record UiPaste(string Text) : UiEvent;
public sealed record UiNone : UiEvent;
