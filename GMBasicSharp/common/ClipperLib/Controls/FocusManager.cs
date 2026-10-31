using CommonLib.Consoles.Tui;
using CommonLib.Screens;
using ClipperLib.Clipper;

namespace ClipperLib.Controls;

/// <summary>Entry di focus: controlla se è abilitata e la sua area per click.</summary>
public class FocusEntry
{
    public string Id { get; set; } = "";
    public ScreenRect Area { get; set; }
    public bool Enabled { get; set; } = true;
    public bool TabStop { get; set; } = true;
    public int TabIndex { get; set; }
    public Func<ScreenRect>? LiveRect { get; set; }
}

/// <summary>Gestisce l'ordine di focus Tab/Shift+Tab e il focus da click.</summary>
public class FocusManager
{
    private readonly List<FocusEntry> entries = [];
    private int? current;
    private readonly Dictionary<string, string> keyBindings = []; // chord → action name

    /// <summary>Registra un keybinding (es. "ctrl+s" → "save").</summary>
    public void BindKey(string chord, string action) =>
        keyBindings[chord.ToLowerInvariant()] = action;

    /// <summary>Risolve un chord in un'azione globale, se registrata.</summary>
    public string? ResolveKey(string chord) =>
        keyBindings.TryGetValue(chord.ToLowerInvariant(), out var a) ? a : null;

    /// <summary>Aggiunge una voce di focus con area statica.</summary>
    public void Register(string id, ScreenRect area)
    {
        entries.Add(new FocusEntry { Id = id, Area = area, Enabled = true });
        if (current == null && entries.Count > 0)
            current = 0;
    }

    /// <summary>Aggiunge una voce di focus con area live (Rect aggiornata a ogni hit test dal controllo).</summary>
    public void Register(string id, Control ctrl)
    {
        var entry = new FocusEntry
        {
            Id = id,
            Area = ctrl.Rect,
            Enabled = ctrl.Enabled,
            TabStop = ctrl.TabStop,
            TabIndex = ctrl.TabIndex,
            LiveRect = () => ctrl.Rect,
        };
        var insertAt = entries.FindIndex(e => e.TabIndex > entry.TabIndex);
        if (insertAt < 0)
            entries.Add(entry);
        else
            entries.Insert(insertAt, entry);
        if (current == null && entries.Count > 0)
            current = 0;
    }

    /// <summary>Abilita o disabilita una voce.</summary>
    public void SetEnabled(string id, bool enabled)
    {
        var e = entries.Find(x => x.Id == id);
        if (e != null) e.Enabled = enabled;
    }

    /// <summary>Aggiorna l'area di una voce (utile per popup dinamici).</summary>
    public void SetArea(string id, ScreenRect area)
    {
        var e = entries.Find(x => x.Id == id);
        if (e != null) e.Area = area;
    }

    /// <summary>ID della voce correntemente focalizzata, o null.</summary>
    public string? CurrentId =>
        current.HasValue ? entries[current.Value]?.Id : null;

    /// <summary>Imposta il focus sull'ID indicato.</summary>
    public bool SetFocusById(string id)
    {
        var idx = entries.FindIndex(e => e.Id == id && e.Enabled);
        if (idx >= 0)
        {
            current = idx;
            return true;
        }
        return false;
    }

    /// <summary>Imposta il focus sul controllo che contiene il punto (x,y), preferendo l'ultimo registrato (sopra in ordine Z).</summary>
    public bool SetFocusByPoint(int x, int y)
    {
        var idx = entries.FindLastIndex(e =>
        {
            if (!e.Enabled) return false;
            var rect = e.LiveRect?.Invoke() ?? e.Area;
            return rect.Contains(x, y);
        });
        if (idx >= 0)
        {
            current = idx;
            return true;
        }
        return false;
    }

    /// <summary>Sposta il focus all'elemento successivo abilitato e con TabStop.</summary>
    public string? Next()
    {
        if (entries.Count == 0) return null;
        var idx = current ?? 0;
        for (var i = 0; i < entries.Count; i++)
        {
            idx = (idx + 1) % entries.Count;
            if (entries[idx].Enabled && entries[idx].TabStop)
            {
                current = idx;
                return entries[idx].Id;
            }
        }
        return null;
    }

    /// <summary>Sposta il focus all'elemento precedente abilitato e con TabStop.</summary>
    public string? Previous()
    {
        if (entries.Count == 0) return null;
        var idx = current ?? 0;
        for (var i = 0; i < entries.Count; i++)
        {
            idx = (idx - 1 + entries.Count) % entries.Count;
            if (entries[idx].Enabled && entries[idx].TabStop)
            {
                current = idx;
                return entries[idx].Id;
            }
        }
        return null;
    }

    /// <summary>Gestisce un evento chiave. Ritorna true se gestito (Tab/BackTab navigazione).</summary>
    public bool HandleKey(int key)
    {
        if (key == 9) // Tab
        {
            Next();
            return true;
        }
        if (key == 15) // BackTab (Shift+Tab)
        {
            Previous();
            return true;
        }
        return false;
    }

    /// <summary>
    /// Gestisce un UiEvent unificato (tastiera, mouse o resize).
    /// Ritorna l'id del controllo che ha ricevuto il focus, oppure null.
    /// Il mouse cambia focus ma non "consuma" l'evento — il widget cliccato
    /// deve ricevere lo stesso click/double-click.
    /// </summary>
    public string? HandleEvent(UiEvent evt)
    {
        switch (evt)
        {
            case UiKey(var key, var mods):
                if (key == KeyConstants.K_BTAB || (key == KeyConstants.K_TAB && (mods & KeyModifiers.Shift) != 0))
                {
                    Previous();
                    return CurrentId;
                }
                if (key == KeyConstants.K_TAB)
                {
                    Next();
                    return CurrentId;
                }
                if (key == KeyConstants.K_BTAB)
                {
                    Previous();
                    return CurrentId;
                }
                return null;

            case UiMouse(var action):
                if (action.Kind is MouseActionKind.Click or MouseActionKind.DoubleClick)
                {
                    if (SetFocusByPoint(action.X, action.Y))
                        return CurrentId;
                    return null;
                }
                return null;

            default:
                return null;
        }
    }

    /// <summary>Lista completa delle entry.</summary>
    public IReadOnlyList<FocusEntry> Entries => entries;

    /// <summary>Rimuove tutte le entry.</summary>
    public void Clear()
    {
        entries.Clear();
        current = null;
    }
}
