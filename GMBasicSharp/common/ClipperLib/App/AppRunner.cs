using CommonLib.Consoles.Tui;
using CommonLib.Screens;
using ClipperLib.Clipper;

namespace ClipperLib.App;

public sealed class AppRunner
{
    private readonly Dictionary<int, Action<IApp>> _keyHandlers = new();
    private Action<MouseAction, IApp>? _mouseHandler;
    private Action<UiEvent, IApp>? _globalHandler;
    private Action<int, int, IApp>? _resizeHandler;

    public AppRunner OnKey(int key, Action<IApp> handler)
    {
        _keyHandlers[key] = handler;
        return this;
    }

    public AppRunner OnMouse(Action<MouseAction, IApp> handler)
    {
        _mouseHandler = handler;
        return this;
    }

    public AppRunner OnResize(Action<int, int, IApp> handler)
    {
        _resizeHandler = handler;
        return this;
    }

    public AppRunner OnEvent(Action<UiEvent, IApp> handler)
    {
        _globalHandler = handler;
        return this;
    }

    public void Run(IApp app, IInputSource input, Action<ScreenBuffer> render, int width = 80, int height = 24)
    {
        var sb = new ScreenBuffer(width, height);
        app.Resize(width, height);
        app.Draw(sb);
        render(sb);
        ConsoleRenderer.PositionCursor(app.Caret.x, app.Caret.y, app.CursorVisible);

        var loop = ModalHost.CurrentLoopOrDefault(input);
        loop.Clock.Reset();
        while (!app.CloseRequested)
        {
            var ev = loop.Next(50);
            if (ev is UiNone)
            {
                if (loop.BlinkTick())
                {
                    app.Draw(sb);
                    render(sb);
                    ConsoleRenderer.PositionCursor(app.Caret.x, app.Caret.y, app.CursorVisible);
                }
                continue;
            }

            switch (ev)
            {
                case UiResize r:
                    sb.Resize(r.Width, r.Height);
                    app.Resize(r.Width, r.Height);
                    _resizeHandler?.Invoke(r.Width, r.Height, app);
                    break;

                case UiKey k:
                    if (_keyHandlers.TryGetValue(k.Key, out var h))
                        h(app);
                    break;

                case UiMouse m:
                    _mouseHandler?.Invoke(m.Action, app);
                    break;
            }

            _globalHandler?.Invoke(ev, app);
            app.HandleEvent(ev);
            app.Draw(sb);
            render(sb);
            ConsoleRenderer.PositionCursor(app.Caret.x, app.Caret.y, app.CursorVisible);
        }
    }
}
