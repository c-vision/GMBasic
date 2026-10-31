// FILE: CommonLib.EntrySource.cs — cartella entry-point del programma BASIC (AGENTS.md §9, §11.1)
// La cartella iniziale è SEMPRE quella del programma .bas con entrypoint, MAI quella del bin del compilatore.
// Usata da Connector.Filesystem, GMBasicInputSource, ecc. per risolvere "work/" accanto al .bas.

namespace CommonLib;

public static class EntrySource
{
    private static string _dir = "";

    public static void Set(string? basPath)
    {
        if (string.IsNullOrWhiteSpace(basPath)) return;
        try
        {
            var full = Path.GetFullPath(basPath);
            var dir = Path.GetDirectoryName(full);
            if (!string.IsNullOrWhiteSpace(dir)) _dir = dir;
        }
        catch { }
    }

    public static string Dir => string.IsNullOrWhiteSpace(_dir) ? Directory.GetCurrentDirectory() : _dir;
}
