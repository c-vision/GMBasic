// FILE: Interpreter.Consoles.JoyManager.cs

using SDL2;

namespace CommonGraphics.Consoles;

public sealed class JoyManager : IDisposable
{
    private static readonly Lazy<JoyManager> _lazy = new(() => new JoyManager());
    public static JoyManager Instance => _lazy.Value;

    private readonly List<IntPtr> _joysticks = new();
    private int _lastRefreshCount;

    private JoyManager() { }

    public int JoystickCount
    {
        get
        {
            RefreshDeviceList();
            return _joysticks.Count;
        }
    }

    public void RefreshDeviceList()
    {
        try
        {
            SdlManager.Instance.Initialize();
        }
        catch
        {
            // SDL non disponibile (headless senza librerie native): nessun joystick.
            _joysticks.Clear();
            _lastRefreshCount = 0;
            return;
        }
        var count = SDL.SDL_NumJoysticks();
        if (count == _lastRefreshCount) return;
        _lastRefreshCount = count;

        foreach (var j in _joysticks)
            if (j != IntPtr.Zero)
                SDL.SDL_JoystickClose(j);
        _joysticks.Clear();

        for (int i = 0; i < count; i++)
        {
            var joy = SDL.SDL_JoystickOpen(i);
            _joysticks.Add(joy);
        }
    }

    public bool IsPresent(int id)
    {
        if (id < 0) return false;
        RefreshDeviceList();
        return id < _joysticks.Count && _joysticks[id] != IntPtr.Zero;
    }

    public long GetState(int id)
    {
        if (!IsPresent(id)) return 0L;

        var joy = _joysticks[id];
        long result = 0;

        if (SDL.SDL_JoystickGetAxis(joy, 0) < -16384) result |= 4;   // LEFT
        if (SDL.SDL_JoystickGetAxis(joy, 0) > 16384)  result |= 8;   // RIGHT
        if (SDL.SDL_JoystickGetAxis(joy, 1) < -16384) result |= 1;   // UP
        if (SDL.SDL_JoystickGetAxis(joy, 1) > 16384)  result |= 2;   // DOWN

        if (SDL.SDL_JoystickGetButton(joy, 0) != 0) result |= 16;    // FIRE (A)
        if (SDL.SDL_JoystickGetButton(joy, 1) != 0) result |= 32;    // BUTTON2 (B)

        var hatCount = SDL.SDL_JoystickNumHats(joy);
        if (hatCount > 0)
        {
            var hat = SDL.SDL_JoystickGetHat(joy, 0);
            if ((hat & SDL.SDL_HAT_UP) != 0)    result |= 1;
            if ((hat & SDL.SDL_HAT_DOWN) != 0)  result |= 2;
            if ((hat & SDL.SDL_HAT_LEFT) != 0)  result |= 4;
            if ((hat & SDL.SDL_HAT_RIGHT) != 0) result |= 8;
        }

        return result;
    }

    public string GetName(int id)
    {
        if (!IsPresent(id)) return "";
        return SDL.SDL_JoystickName(_joysticks[id]) ?? "";
    }

    public void Dispose()
    {
        foreach (var j in _joysticks)
            if (j != IntPtr.Zero)
                SDL.SDL_JoystickClose(j);
        _joysticks.Clear();
    }
}
