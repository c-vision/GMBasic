// FILE: Basic.IlBackend.ExecutableBuilder.cs

namespace Basic.IlBackend;

/// <summary>Builds a native executable from a generated IL DLL using the .NET AppHost.</summary>
/// <remarks>
/// On macOS/Linux, .NET executables require a native AppHost wrapper that loads the managed DLL.
/// The AppHost looks for {name}.dll next to {name}. This is a 2-file output by design.
/// Single-file embedding (NativeAOT) is not possible because NativeAOT on macOS does not support 
/// Assembly.Load(byte[]). A custom C host using hostfxr would be required for true single-file.
/// </remarks>
public static class ExecutableBuilder
{
    public static string Build(string appHostPath, string dllPath, string outputExe)
    {
        // Copy AppHost as the executable
        File.Copy(appHostPath, outputExe, true);

        // Copy generated DLL alongside (AppHost loads {name}.dll by convention)
        var dir = Path.GetDirectoryName(Path.GetFullPath(outputExe))!;
        var baseName = Path.GetFileNameWithoutExtension(outputExe);
        var destDll = Path.Combine(dir, baseName + ".dll");
        File.Copy(dllPath, destDll, true);

        if (!OperatingSystem.IsWindows())
            new FileInfo(outputExe).UnixFileMode =
                UnixFileMode.UserRead | UnixFileMode.UserWrite | UnixFileMode.UserExecute |
                UnixFileMode.GroupRead | UnixFileMode.GroupExecute |
                UnixFileMode.OtherRead | UnixFileMode.OtherExecute;
        return outputExe;
    }
}
