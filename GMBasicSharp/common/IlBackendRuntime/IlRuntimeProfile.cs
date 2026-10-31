// FILE: Basic.IlBackend.IlRuntimeProfile.cs
// Profilo di utilizzo del runtime: analizza la DLL generata da IlProgramCompiler e raccoglie
// i metodi IlRuntime chiamati dal programma BASIC. Scopo: alimentare il root descriptor di
// NativeAOT (vedi Todo/NativeAOT-Link-Solo-Necessario) per compilare nativamente solo ciò
// che il programma usa davvero.

namespace Basic.IlBackend;

/// <summary>Risultato dell'analisi della DLL generata: metodi IlRuntime usati.</summary>
public sealed class IlRuntimeProfile
{
    /// <summary>Metodi IlRuntime chiamati, formato "Nome".</summary>
    public HashSet<string> UsedMethods { get; } = new(StringComparer.Ordinal);
    public int CallCount { get; set; }
    /// <summary>True se il programma usa &amp;/EVAL/BSP (codice che compila a runtime).</summary>
    public bool UsesDynamicCode { get; set; }

    public bool IsEmpty => UsedMethods.Count == 0;

    public override string ToString() =>
        $"{UsedMethods.Count} metodi IlRuntime usati, {CallCount} call, dynamic={(UsesDynamicCode ? "si" : "no")}";
}

/// <summary>Estrae dal codice IL della DLL generata i call verso IlRuntime.</summary>
public static class IlRuntimeProfiler
{
    /// <summary>Analizza la DLL generata: metodi IlRuntime usati + rileva codice dinamico.</summary>
    public static IlRuntimeProfile Analyze(string dllPath)
    {
        var profile = new IlRuntimeProfile();
        var targets = IlDisassembler.CollectCallTargets(dllPath);
        foreach (var t in targets)
        {
            profile.CallCount++;
            if (t.IsIlRuntime)
                profile.UsedMethods.Add(t.MethodName);
            else if (t.MethodName.Contains("Dynamic", StringComparison.Ordinal)
                     && (t.ParentNamespace?.StartsWith("Basic.IlBackend", StringComparison.Ordinal) == true
                         || t.ParentNamespace?.StartsWith("Basic", StringComparison.Ordinal) == true))
                profile.UsesDynamicCode = true;
        }
        return profile;
    }

    /// <summary>Genera un root descriptor illink che radica solo i metodi IlRuntime usati
    /// dal programma (più l'entry point GeneratedProgram). Se il programma usa &amp;/EVAL/BSP
    /// (compilazione a runtime) restituisce null: in quel caso serve il grafo completo.</summary>
    public static string? GenerateDescriptor(IlRuntimeProfile profile, string programAssemblyName)
    {
        if (profile.UsesDynamicCode || profile.IsEmpty) return null;

        var sb = new System.Text.StringBuilder();
        sb.AppendLine("<linker>");
        sb.AppendLine($"  <assembly fullname=\"{programAssemblyName}\">");
        sb.AppendLine("    <type fullname=\"GeneratedProgram\" />");
        sb.AppendLine("  </assembly>");
        sb.AppendLine("  <assembly fullname=\"GMBasic.IlBackend\">");
        sb.AppendLine("    <type fullname=\"Basic.IlBackend.IlRuntime\">");
        foreach (var m in profile.UsedMethods.OrderBy(x => x))
            sb.AppendLine($"      <method name=\"{System.Security.SecurityElement.Escape(m)}\" />");
        sb.AppendLine("    </type>");
        sb.AppendLine("  </assembly>");
        sb.AppendLine("</linker>");
        return sb.ToString();
    }

    /// <summary>Scrive il descriptor in <paramref name="dir"/>/<paramref name="baseName"/>.linker.xml
    /// e restituisce il path, o null se il programma non ne ha bisogno (dinamico/vuoto).</summary>
    public static string? WriteDescriptor(IlRuntimeProfile profile, string programAssemblyName, string dir, string baseName)
    {
        var xml = GenerateDescriptor(profile, programAssemblyName);
        if (xml == null) return null;
        Directory.CreateDirectory(dir);
        var path = Path.Combine(dir, baseName + ".linker.xml");
        File.WriteAllText(path, xml);
        return path;
    }
}
