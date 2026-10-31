// Browser/ToolSearchNative.cs — ToolRegistry GESTITO (C# puro) per IL/PCodeVM.
// Parità con il ToolRegistry C (native/gmb_agent_runtime/tool_registry.c + builtin_seed.c):
// ricerca case-insensitive su name+description, ritorna JSON {name,description,provider,risk}.
// Backend gestiti = implementazione C# (mai bridge verso il runtime C).
using System.Text.Json;

namespace Basic.AI.Browser;

public static class ToolSearchNative
{
    public sealed record ToolEntry(string Name, string Description, string Provider, string Risk);

    private static readonly List<ToolEntry> Registry = new();
    private static readonly object Gate = new();

    static ToolSearchNative()
    {
        // Seed canonico (parità builtin_seed.c del runtime C)
        Register("browser.navigate", "Navigate the browser to a URL", "BrowserProvider", "low");
        Register("browser.click", "Click an element identified by selector/text", "BrowserProvider", "low");
        Register("browser.fill", "Fill a form field with text", "BrowserProvider", "low");
        Register("browser.snapshot", "Take an a11y snapshot of the current page", "BrowserProvider", "low");
        Register("browser.evaluate", "Evaluate JavaScript in page context", "BrowserProvider", "low");
        Register("browser.login", "Login using credentials from the vault (model never sees password)", "BrowserProvider", "low");
        Register("computer.screenshot", "Take a screenshot of the screen", "ComputerProvider", "low");
        Register("computer.click", "Click at absolute coordinates x,y", "ComputerProvider", "low");
        Register("computer.type", "Type text at the current focus", "ComputerProvider", "low");
        Register("computer.key", "Press a key (e.g. Return, Tab)", "ComputerProvider", "low");
        Register("files.read", "Read a text file (sandboxed)", "FilesProvider", "low");
        Register("files.write", "Write text to a file (sandboxed)", "FilesProvider", "medium");
        Register("files.list", "List files in a directory", "FilesProvider", "low");
        Register("ui.find", "Find a UI element by text/selector (API→AX→DOM→CUA)", "ActionResolver", "low");
        Register("ui.click", "Click a UI element by text/selector", "ActionResolver", "medium");
        Register("ui.type", "Type text into a UI element by text/selector", "ActionResolver", "medium");
    }

    public static void Register(string name, string description, string provider, string risk = "low")
    {
        lock (Gate)
        {
            var i = Registry.FindIndex(t => string.Equals(t.Name, name, StringComparison.OrdinalIgnoreCase));
            var e = new ToolEntry(name, description, provider, risk);
            if (i >= 0) Registry[i] = e; else Registry.Add(e);
        }
    }

    public static string Search(string query)
    {
        ToolEntry[] matched;
        lock (Gate)
        {
            matched = string.IsNullOrWhiteSpace(query)
                ? Registry.ToArray()
                : Registry.Where(t =>
                        t.Name.Contains(query, StringComparison.OrdinalIgnoreCase) ||
                        t.Description.Contains(query, StringComparison.OrdinalIgnoreCase))
                    .ToArray();
        }
        var arr = matched.Select(t => new
        {
            name = t.Name,
            description = t.Description,
            provider = t.Provider,
            risk = t.Risk
        });
        return JsonSerializer.Serialize(arr);
    }
}
