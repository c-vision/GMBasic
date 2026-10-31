// FILE: CommonLib.FileSystemUtils.cs

using System.Runtime.CompilerServices;

/* 
 * FileSystemUtils.cs: utilità per operazioni su filesystem (percorsi, directory, file, metadati)
 * 
 * FUNZIONI:
 * - CoreNormalizePath(path): normalizza un percorso
 * - CoreResolveFilePath(fileName, defaultDirectory, searchPaths): risolve un percorso file
 * - CoreCwd(): restituisce la directory corrente
 * - CoreSetCurrentDirectory(path): imposta la directory corrente
 * - CoreAppPath(): restituisce la directory base dell'applicazione
 * - CoreBaseDirectory(): restituisce AppContext.BaseDirectory
 * - CoreProjectPath(): restituisce la directory radice del progetto
 * - CoreProjectFolder(folderName): restituisce il percorso di una cartella di progetto
 * - CoreUserDataPath(appName): restituisce il percorso dati utente
 * - CoreGetParentDirectoryFromFilePath(filePath): directory padre del file
 * - CoreGetFolderPath(folderName): percorso cartella speciale di sistema
 * - CoreGetTempDirectory(): directory temporanea
 * - CoreGetTempFileName(): crea file temporaneo
 * - CoreCompilationFilePath/CoreCompilationFilePathNormalized: percorso file sorgente compile-time
 * - CoreCompilationProjectPathNormalized/CoreSourceCodeDirectory/CoreSourceCodeProjectRoot: percorsi compile-time
 * - CoreCreateDirectoryFromFileInfo(fileInfo): crea directory da FileInfo
 * - CoreDeleteDirectory(path, recursive): elimina directory
 * - CoreMoveDirectory(src, dest): sposta directory
 * - CoreDirectoryCopy(sourceDirName, destDirName, copySubDirs, overwrite): copia directory
 * - CoreDirectorySize(directory, includeSubdirectories): calcola dimensione directory
 * - CoreGetFilesWhere(directory, ...): ricerca file con predicato
 * - CoreDeleteFile(path): elimina file
 * - CoreMoveFile(src, dest): sposta file
 * - CoreCopyFile(src, dest, overwrite): copia file
 * - CoreCountLines(fileName/file): conta righe di un file
 * - CoreGetFileSize(filePath): dimensione file
 * - CoreFileExists/CoreDirectoryExists: verifica esistenza
 * - CoreGetLastWriteTime/CoreGetCreationTime/CoreGetLastAccessTime: metadati file
 * - CoreGetExtension/CoreGetFileName/CoreGetFileNameWithoutExtension/CoreGetDirectoryName/CoreChangeExtension: estensioni e nomi
 * - CoreCombinePaths/CoreIsPathRooted/CoreGetRelativePath: combinazione percorsi
 * - CoreGetFiles/CoreGetDirectories/CoreEnumerateFileSystemEntries: enumerazione
 * - CoreCreateDirectory/CoreCreateEmptyFile: creazione
 */

namespace CommonLib;

/// <summary>
/// Pure file-system utility methods extracted from the BASIC interpreter.
/// All methods are stateless and side-effect-free (except CoreSetCurrentDirectory).
/// </summary>
public static class FileSystemUtils
{
    // ========================================================================
    // SEZIONE 1: Normalizzazione e Risoluzione Percorsi
    // ========================================================================

    /// <summary>
    /// Normalizes a path by converting it to an absolute, canonical form
    /// and replacing alternate directory separators with the system default.
    /// </summary>
    /// <param name="path">The path to normalize.</param>
    /// <returns>The normalized absolute path, or <see cref="string.Empty"/> if the input is null or whitespace.</returns>
    public static string CoreNormalizePath(string path)
    {
        if (string.IsNullOrWhiteSpace(path))
            return string.Empty;
        var fullPath = Path.GetFullPath(path);
        return fullPath.Replace(Path.AltDirectorySeparatorChar, Path.DirectorySeparatorChar);
    }

    /// <summary>
    /// Resolves a file path by searching a default directory and a list of fallback search paths.
    /// If the file is rooted, it is returned as-is.
    /// </summary>
    /// <param name="fileName">The file name or relative path to resolve.</param>
    /// <param name="defaultDirectory">The default directory to combine with the file name.</param>
    /// <param name="searchPaths">Additional directories to search in order of priority.</param>
    /// <returns>The first existing full path found, or the combined default directory path if none exist.</returns>
    public static string CoreResolveFilePath(string fileName, string defaultDirectory, IReadOnlyList<string> searchPaths)
    {
        if (string.IsNullOrWhiteSpace(fileName))
            return string.Empty;
        if (Path.IsPathRooted(fileName))
            return fileName;
        var fullPath = Path.Combine(defaultDirectory, fileName);
        if (File.Exists(fullPath))
            return fullPath;
        foreach (var searchPath in searchPaths)
        {
            fullPath = Path.Combine(searchPath, fileName);
            if (File.Exists(fullPath))
                return fullPath;
        }
        return Path.Combine(defaultDirectory, fileName);
    }

    // ========================================================================
    // SEZIONE 2: Percorsi dell'Applicazione e del Progetto
    // ========================================================================

    /// <summary>
    /// Returns the current working directory.
    /// </summary>
    /// <returns>The absolute path of the current working directory.</returns>
    public static string CoreCwd() => Directory.GetCurrentDirectory();

    /// <summary>
    /// Sets the current working directory to the specified path.
    /// </summary>
    /// <param name="path">The directory path to set as current.</param>
    /// <returns><c>true</c> if the directory was set successfully; <c>false</c> if the directory does not exist or an error occurred.</returns>
    public static bool CoreSetCurrentDirectory(string path)
    {
        try
        {
            if (!Directory.Exists(path))
                return false;
            
            Directory.SetCurrentDirectory(path);
            return true;
        }
        catch
        {
            return false;
        }
    }
    
    /// <summary>
    /// Returns the application base directory, with any trailing separator trimmed.
    /// </summary>
    /// <returns>The application's base directory path.</returns>
    public static string CoreAppPath()
    {
        var path = AppContext.BaseDirectory;
        if (path.EndsWith(Path.DirectorySeparatorChar) || path.EndsWith(Path.AltDirectorySeparatorChar))
        {
            path = path.TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar);
        }
        return path;
    }
    
    /// <summary>
    /// Returns the application base directory (same as <see cref="AppContext.BaseDirectory"/>).
    /// </summary>
    /// <returns>The application's base directory path, with trailing separator.</returns>
    public static string CoreBaseDirectory() => AppContext.BaseDirectory;

    /// <summary>
    /// Returns the project root directory by walking up from the application base directory
    /// until a directory containing a .csproj or .sln file is found. Falls back to <see cref="CoreAppPath"/>.
    /// </summary>
    /// <returns>The project root directory path.</returns>
    public static string CoreProjectPath()
    {
        var appPath = CoreAppPath();
        if (!appPath.Contains($"{Path.DirectorySeparatorChar}bin{Path.DirectorySeparatorChar}")) return appPath;
        var parent = Directory.GetParent(appPath)?.Parent?.Parent?.FullName;
        if (parent != null && Directory.Exists(parent))
            return parent;
        return appPath;
    }
    
    /// <summary>
    /// Returns the full path to a folder inside the project root directory.
    /// </summary>
    /// <param name="folderName">The name of the folder relative to the project root.</param>
    /// <returns>The combined path: <c>&lt;ProjectPath&gt;/&lt;folderName&gt;</c>.</returns>
    public static string CoreProjectFolder(string folderName) => Path.Combine(CoreProjectPath(), folderName);

    /// <summary>
    /// Returns and ensures the existence of a user-specific application data directory.
    /// Uses <see cref="Environment.SpecialFolder.ApplicationData"/> as the base.
    /// </summary>
    /// <param name="appName">The application name used as a subfolder.</param>
    /// <returns>The full path to the user data directory, created if it did not exist.</returns>
    public static string CoreUserDataPath(string appName)
    {
        var basePath = Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData,
            Environment.SpecialFolderOption.Create);
        var appPath = Path.Combine(basePath, appName);
        if (!Directory.Exists(appPath))
            Directory.CreateDirectory(appPath);
        return appPath;
    }
    
    /// <summary>
    /// Gets the parent directory of the directory containing the specified file path.
    /// </summary>
    /// <param name="filePath">The file path to evaluate.</param>
    /// <returns>The parent directory of the file's container directory.</returns>
    /// <exception cref="ArgumentException">Thrown if <paramref name="filePath"/> is null or whitespace.</exception>
    /// <exception cref="InvalidOperationException">Thrown if the file's directory or its parent cannot be determined.</exception>
    public static string CoreGetParentDirectoryFromFilePath(string filePath)
    {
        if (string.IsNullOrWhiteSpace(filePath))
            throw new ArgumentException("Il percorso del file non può essere vuoto.", nameof(filePath));
        var directory = Path.GetDirectoryName(filePath);
        if (string.IsNullOrWhiteSpace(directory))
            throw new InvalidOperationException(
                "Impossibile determinare la directory del file (percorso radice?)");
        var parentDirectory = Path.GetDirectoryName(directory);
        if (string.IsNullOrWhiteSpace(parentDirectory))
            throw new InvalidOperationException(
                $"Impossibile determinare la directory padre di: {directory}");
        return parentDirectory;
    }

    // ========================================================================
    // SEZIONE 3: Percorsi Speciali del Sistema Operativo
    // ========================================================================

    /// <summary>
    /// Returns the path to a system special folder (e.g., Desktop, MyDocuments).
    /// </summary>
    /// <param name="folderName">The name of the special folder (case-insensitive match on <see cref="Environment.SpecialFolder"/>).</param>
    /// <returns>The path to the specified special folder.</returns>
    /// <exception cref="ArgumentException">Thrown if <paramref name="folderName"/> is null, empty, or not a valid <see cref="Environment.SpecialFolder"/> value.</exception>
    public static string CoreGetFolderPath(string folderName)
    {
        if (string.IsNullOrWhiteSpace(folderName))
            throw new ArgumentException("Il nome della cartella non può essere vuoto.", nameof(folderName));
        if (Enum.TryParse<Environment.SpecialFolder>(folderName, true, out var specialFolder))
            return Environment.GetFolderPath(specialFolder);
        throw new ArgumentException(
            $"Nome di cartella speciale non valido: '{folderName}'. " +
            $"Valori validi: {string.Join(", ", Enum.GetNames(typeof(Environment.SpecialFolder)))}",
            nameof(folderName));
    }

    /// <summary>
    /// Returns the system temporary directory path.
    /// </summary>
    /// <returns>The path to the temporary folder.</returns>
    public static string CoreGetTempDirectory() => Path.GetTempPath();

    /// <summary>
    /// Creates and returns the path to a uniquely named, zero-byte temporary file.
    /// </summary>
    /// <returns>The full path of the created temporary file.</returns>
    public static string CoreGetTempFileName() => Path.GetTempFileName();
    
    // ========================================================================
    // SEZIONE 4: Percorsi a Compile-Time (CallerFilePath)
    // ========================================================================

    /// <summary>
    /// Returns the source file path of the caller at compile time.
    /// </summary>
    /// <param name="filePath">Automatically injected by the compiler via <see cref="CallerFilePathAttribute"/>.</param>
    /// <returns>The source file path of the caller.</returns>
    public static string CoreCompilationFilePath([CallerFilePath] string filePath = "") => filePath;

    /// <summary>
    /// Returns the normalized source file path of the caller at compile time.
    /// </summary>
    /// <param name="filePath">Automatically injected by the compiler via <see cref="CallerFilePathAttribute"/>.</param>
    /// <returns>The normalized absolute path of the caller's source file.</returns>
    public static string CoreCompilationFilePathNormalized([CallerFilePath] string filePath = "") =>
        CoreNormalizePath(filePath);

    /// <summary>
    /// Returns the directory containing the caller's source file at compile time.
    /// </summary>
    /// <param name="filePath">Automatically injected by the compiler via <see cref="CallerFilePathAttribute"/>.</param>
    /// <returns>The directory path containing the caller's source file.</returns>
    /// <exception cref="ArgumentException">Thrown if the resolved path is invalid.</exception>
    /// <exception cref="InvalidOperationException">Thrown if the directory cannot be determined.</exception>
    public static string CoreCompilationProjectPathNormalized([CallerFilePath] string filePath = "")
    {
        var normalizedPath = CoreNormalizePath(filePath);
        if (string.IsNullOrWhiteSpace(normalizedPath))
            throw new ArgumentException("Il percorso filePath non è valido.", nameof(filePath));
        var directory = Path.GetDirectoryName(normalizedPath);
        if (string.IsNullOrWhiteSpace(directory))
            throw new InvalidOperationException(
                $"Impossibile determinare la directory per: '{normalizedPath}'");
        return directory;
    }

    /// <summary>
    /// Returns the directory of the caller's source file at compile time.
    /// </summary>
    /// <param name="filePath">Automatically injected by the compiler via <see cref="CallerFilePathAttribute"/>.</param>
    /// <returns>The directory of the caller's source file, or <see cref="string.Empty"/> if it cannot be determined.</returns>
    public static string CoreSourceCodeDirectory([CallerFilePath] string filePath = "")
    {
        var normalizedPath = CoreNormalizePath(filePath);
        if (string.IsNullOrWhiteSpace(normalizedPath))
            return string.Empty;
        return Path.GetDirectoryName(normalizedPath) ?? string.Empty;
    }
    
    /// <summary>
    /// Walks up from the caller's source file directory to find the nearest ancestor
    /// containing a .csproj or .sln file, indicating the project root.
    /// </summary>
    /// <param name="filePath">Automatically injected by the compiler via <see cref="CallerFilePathAttribute"/>.</param>
    /// <returns>The project root directory, or the source code directory if none is found.</returns>
    public static string CoreSourceCodeProjectRoot([CallerFilePath] string filePath = "")
    {
        var currentDir = CoreSourceCodeDirectory();
        if (string.IsNullOrWhiteSpace(currentDir))
            return string.Empty;
        var directory = new DirectoryInfo(currentDir);
        while (directory != null)
        {
            if (directory.GetFiles("*.csproj").Length > 0 || 
                directory.GetFiles("*.sln").Length > 0)
            {
                return directory.FullName;
            }
            directory = directory.Parent;
        }
        return currentDir;
    }
    
    // ========================================================================
    // SEZIONE 5: Operazioni su Directory
    // ========================================================================

    /// <summary>
    /// Creates the directory for the specified <see cref="FileInfo"/> if it does not exist.
    /// </summary>
    /// <param name="fileInfo">The file whose parent directory should be created.</param>
    /// <returns>A <see cref="DirectoryInfo"/> for the created or existing directory, or <c>null</c> if the file has no directory.</returns>
    public static DirectoryInfo? CoreCreateDirectoryFromFileInfo(FileInfo fileInfo)
    {
        return fileInfo.Directory == null ? null : Directory.CreateDirectory(fileInfo.Directory.FullName);
    }

    /// <summary>
    /// Deletes the specified directory.
    /// </summary>
    /// <param name="path">The path to the directory to delete.</param>
    /// <param name="recursive">If <c>true</c>, deletes all subdirectories and files; otherwise the directory must be empty.</param>
    /// <exception cref="DirectoryNotFoundException">Thrown if the directory does not exist.</exception>
    public static void CoreDeleteDirectory(string path, bool recursive = false)
    {
        if (!Directory.Exists(path))
            throw new DirectoryNotFoundException($"Directory non trovata: {path}");
        Directory.Delete(path, recursive);
    }

    /// <summary>
    /// Moves a directory from one location to another.
    /// </summary>
    /// <param name="src">The source directory path.</param>
    /// <param name="dest">The destination directory path.</param>
    /// <exception cref="DirectoryNotFoundException">Thrown if the source directory does not exist.</exception>
    public static void CoreMoveDirectory(string src, string dest)
    {
        if (!Directory.Exists(src))
            throw new DirectoryNotFoundException($"Directory non trovata: {src}");
        Directory.Move(src, dest);
    }

    /// <summary>
    /// Copies the contents of a directory to another location, optionally including subdirectories.
    /// </summary>
    /// <param name="sourceDirName">The source directory path.</param>
    /// <param name="destDirName">The destination directory path (created if it does not exist).</param>
    /// <param name="copySubDirs">If <c>true</c>, copies subdirectories recursively.</param>
    /// <param name="overwrite">If <c>true</c>, overwrites existing files at the destination.</param>
    /// <exception cref="DirectoryNotFoundException">Thrown if the source directory does not exist.</exception>
    public static void CoreDirectoryCopy(string sourceDirName, string destDirName, bool copySubDirs, bool overwrite)
    {
        var dir = new DirectoryInfo(sourceDirName);
        if (!dir.Exists)
            throw new DirectoryNotFoundException(
                $"La directory sorgente non esiste: {sourceDirName}");

        Directory.CreateDirectory(destDirName);
        foreach (var file in dir.GetFiles())
        {
            var tempPath = Path.Combine(destDirName, file.Name);
            file.CopyTo(tempPath, overwrite);
        }
        if (!copySubDirs) return;
        {
            foreach (var subdir in dir.GetDirectories())
            {
                var tempPath = Path.Combine(destDirName, subdir.Name);
                CoreDirectoryCopy(subdir.FullName, tempPath, copySubDirs, overwrite);
            }
        }
    }

    /// <summary>
    /// Calculates the total size (in bytes) of all files in a directory.
    /// </summary>
    /// <param name="directory">The directory to measure.</param>
    /// <param name="includeSubdirectories">If <c>true</c>, includes the size of all subdirectories recursively.</param>
    /// <returns>The total size in bytes. Returns 0 if the directory does not exist.</returns>
    public static long CoreDirectorySize(DirectoryInfo directory, bool includeSubdirectories)
    {
        if (!directory.Exists)
            return 0;
        var totalSize = directory.GetFiles().Sum(file => file.Length);
        if (includeSubdirectories)
            totalSize += directory.GetDirectories().Sum(dir => CoreDirectorySize(dir, true));
        return totalSize;
    }
    
    // ========================================================================
    // SEZIONE 6: Ricerca e Filtraggio File
    // ========================================================================

    /// <summary>
    /// Returns all files in a directory that match the given predicate.
    /// </summary>
    /// <param name="directory">The directory to search.</param>
    /// <param name="predicate">A function to test each file for inclusion.</param>
    /// <returns>An array of <see cref="FileInfo"/> objects matching the predicate.</returns>
    public static FileInfo[] CoreGetFilesWhere(DirectoryInfo directory, Func<FileInfo, bool> predicate)
    {
        return Directory
            .EnumerateFiles(directory.FullName)
            .Select(path => new FileInfo(path))
            .Where(predicate)
            .ToArray();
    }

    /// <summary>
    /// Returns all files matching a search pattern and predicate in a directory.
    /// </summary>
    /// <param name="directory">The directory to search.</param>
    /// <param name="searchPattern">The search string to match against file names (e.g., "*.txt").</param>
    /// <param name="predicate">A function to test each file for inclusion.</param>
    /// <returns>An array of <see cref="FileInfo"/> objects matching the pattern and predicate.</returns>
    public static FileInfo[] CoreGetFilesWhere(DirectoryInfo directory, string searchPattern,
        Func<FileInfo, bool> predicate)
    {
        return Directory
            .EnumerateFiles(directory.FullName, searchPattern)
            .Select(path => new FileInfo(path))
            .Where(predicate)
            .ToArray();
    }

    /// <summary>
    /// Returns all files matching a search pattern and predicate in a directory, with a search option.
    /// </summary>
    /// <param name="directory">The directory to search.</param>
    /// <param name="searchPattern">The search string to match against file names.</param>
    /// <param name="searchOption">Specifies whether to search only the top directory or all subdirectories.</param>
    /// <param name="predicate">A function to test each file for inclusion.</param>
    /// <returns>An array of <see cref="FileInfo"/> objects matching the criteria.</returns>
    public static FileInfo[] CoreGetFilesWhere(DirectoryInfo directory, string searchPattern, SearchOption searchOption,
        Func<FileInfo, bool> predicate)
    {
        return Directory
            .EnumerateFiles(directory.FullName, searchPattern, searchOption)
            .Select(path => new FileInfo(path))
            .Where(predicate)
            .ToArray();
    }

    /// <summary>
    /// Returns all files matching any of the given search patterns and predicate in a directory,
    /// with duplicates removed.
    /// </summary>
    /// <param name="directory">The directory to search.</param>
    /// <param name="searchPatterns">An array of search patterns to match against file names.</param>
    /// <param name="searchOption">Specifies whether to search only the top directory or all subdirectories.</param>
    /// <param name="predicate">A function to test each file for inclusion.</param>
    /// <returns>An array of distinct <see cref="FileInfo"/> objects matching any of the patterns and the predicate.</returns>
    public static FileInfo[] CoreGetFilesWhere(DirectoryInfo directory, string[] searchPatterns,
        SearchOption searchOption, Func<FileInfo, bool> predicate)
    {
        return searchPatterns
            .SelectMany(pattern => directory.GetFiles(pattern, searchOption))
            .Distinct()
            .Where(predicate)
            .ToArray();
    }

    // ========================================================================
    // SEZIONE 7: Operazioni su File
    // ========================================================================

    /// <summary>
    /// Deletes the specified file.
    /// </summary>
    /// <param name="path">The path to the file to delete.</param>
    /// <exception cref="FileNotFoundException">Thrown if the file does not exist.</exception>
    public static void CoreDeleteFile(string path)
    {
        if (!File.Exists(path))
            throw new FileNotFoundException($"File non trovato: {path}");
        File.Delete(path);
    }

    /// <summary>
    /// Moves a file from one location to another, overwriting the destination if it exists.
    /// </summary>
    /// <param name="src">The source file path.</param>
    /// <param name="dest">The destination file path.</param>
    /// <exception cref="FileNotFoundException">Thrown if the source file does not exist.</exception>
    public static void CoreMoveFile(string src, string dest)
    {
        if (!File.Exists(src))
            throw new FileNotFoundException($"File non trovato: {src}");
        File.Move(src, dest, overwrite: true);
    }

    /// <summary>
    /// Copies a file from the source path to the destination path.
    /// </summary>
    /// <param name="src">The source file path.</param>
    /// <param name="dest">The destination file path.</param>
    /// <param name="overwrite">If <c>true</c>, overwrites an existing destination file.</param>
    public static void CoreCopyFile(string src, string dest, bool overwrite = false) => 
        File.Copy(src, dest, overwrite);

    // ========================================================================
    // SEZIONE 8: Analisi Contenuto File
    // ========================================================================

    /// <summary>
    /// Counts the total number of lines in the specified file.
    /// </summary>
    /// <param name="fileName">The path to the file.</param>
    /// <returns>The number of lines, or 0 if the file does not exist.</returns>
    public static int CoreCountLines(string fileName) => CoreCountLines(new FileInfo(fileName));

    /// <summary>
    /// Counts the total number of lines in the specified file.
    /// </summary>
    /// <param name="file">The <see cref="FileInfo"/> representing the file.</param>
    /// <returns>The number of lines, or 0 if the file does not exist.</returns>
    public static int CoreCountLines(FileInfo file) => !file.Exists ? 0 : File.ReadAllLines(file.FullName).Length;

    /// <summary>
    /// Counts the lines in the specified file that match the given predicate.
    /// </summary>
    /// <param name="fileName">The path to the file.</param>
    /// <param name="predicate">A function to test each line for inclusion.</param>
    /// <returns>The number of matching lines, or 0 if the file does not exist.</returns>
    public static int CoreCountLines(string fileName, Func<string, bool> predicate) =>
        CoreCountLines(new FileInfo(fileName), predicate);

    /// <summary>
    /// Counts the lines in the specified file that match the given predicate.
    /// </summary>
    /// <param name="file">The <see cref="FileInfo"/> representing the file.</param>
    /// <param name="predicate">A function to test each line for inclusion.</param>
    /// <returns>The number of matching lines, or 0 if the file does not exist.</returns>
    public static int CoreCountLines(FileInfo file, Func<string, bool> predicate) => 
        !file.Exists ? 0 : File.ReadAllLines(file.FullName).Count(predicate);

    // ========================================================================
    // SEZIONE 9: Informazioni e Metadati File
    // ========================================================================

    /// <summary>
    /// Returns the size of the specified file in bytes.
    /// </summary>
    /// <param name="filePath">The path to the file.</param>
    /// <returns>The file size in bytes, or 0 if the file does not exist.</returns>
    public static long CoreGetFileSize(string filePath) => !File.Exists(filePath) ? 0 : new FileInfo(filePath).Length;

    /// <summary>
    /// Determines whether the specified file exists.
    /// </summary>
    /// <param name="filePath">The path to the file.</param>
    /// <returns><c>true</c> if the file exists; otherwise <c>false</c>.</returns>
    public static bool CoreFileExists(string filePath) => File.Exists(filePath);

    /// <summary>
    /// Determines whether the specified directory exists.
    /// </summary>
    /// <param name="directoryPath">The path to the directory.</param>
    /// <returns><c>true</c> if the directory exists; otherwise <c>false</c>.</returns>
    public static bool CoreDirectoryExists(string directoryPath) => Directory.Exists(directoryPath);

    /// <summary>
    /// Returns the last write time of the specified file.
    /// </summary>
    /// <param name="filePath">The path to the file.</param>
    /// <returns>The last write <see cref="DateTime"/>, or <see cref="DateTime.MinValue"/> if the file does not exist.</returns>
    public static DateTime CoreGetLastWriteTime(string filePath) => 
        !File.Exists(filePath) ? DateTime.MinValue : File.GetLastWriteTime(filePath);

    /// <summary>
    /// Returns the creation time of the specified file.
    /// </summary>
    /// <param name="filePath">The path to the file.</param>
    /// <returns>The creation <see cref="DateTime"/>, or <see cref="DateTime.MinValue"/> if the file does not exist.</returns>
    public static DateTime CoreGetCreationTime(string filePath) => 
        !File.Exists(filePath) ? DateTime.MinValue : File.GetCreationTime(filePath);

    /// <summary>
    /// Returns the last access time of the specified file.
    /// </summary>
    /// <param name="filePath">The path to the file.</param>
    /// <returns>The last access <see cref="DateTime"/>, or <see cref="DateTime.MinValue"/> if the file does not exist.</returns>
    public static DateTime CoreGetLastAccessTime(string filePath) => 
        !File.Exists(filePath) ? DateTime.MinValue : File.GetLastAccessTime(filePath);

    // ========================================================================
    // SEZIONE 10: Estensioni e Nomi File
    // ========================================================================

    /// <summary>
    /// Returns the extension (including the period) of the specified file path.
    /// </summary>
    /// <param name="filePath">The file path.</param>
    /// <returns>The file extension (e.g., ".txt"), or <see cref="string.Empty"/> if none.</returns>
    public static string CoreGetExtension(string filePath) => Path.GetExtension(filePath);

    /// <summary>
    /// Returns the file name and extension of the specified file path.
    /// </summary>
    /// <param name="filePath">The file path.</param>
    /// <returns>The file name with extension.</returns>
    public static string CoreGetFileName(string filePath) => Path.GetFileName(filePath);

    /// <summary>
    /// Returns the file name without the extension of the specified file path.
    /// </summary>
    /// <param name="filePath">The file path.</param>
    /// <returns>The file name without the extension.</returns>
    public static string CoreGetFileNameWithoutExtension(string filePath) => 
        Path.GetFileNameWithoutExtension(filePath);

    /// <summary>
    /// Returns the directory portion of the specified file path.
    /// </summary>
    /// <param name="filePath">The file path.</param>
    /// <returns>The directory path, or <see cref="string.Empty"/> if none.</returns>
    public static string CoreGetDirectoryName(string filePath) => 
        Path.GetDirectoryName(filePath) ?? string.Empty;

    /// <summary>
    /// Changes the extension of the specified file path.
    /// </summary>
    /// <param name="filePath">The original file path.</param>
    /// <param name="newExtension">The new extension (with or without the leading period), or <c>null</c> to remove the extension.</param>
    /// <returns>The file path with the new extension.</returns>
    public static string CoreChangeExtension(string filePath, string? newExtension) => 
        Path.ChangeExtension(filePath, newExtension);

    // ========================================================================
    // SEZIONE 11: Combinazione e Costruzione Percorsi
    // ========================================================================

    /// <summary>
    /// Combines multiple path segments into a single path.
    /// </summary>
    /// <param name="paths">The path segments to combine.</param>
    /// <returns>The combined path, or <see cref="string.Empty"/> if no segments are provided.</returns>
    public static string CoreCombinePaths(params string[] paths)
    {
        if (paths.Length == 0)
            return string.Empty;
        return Path.Combine(paths);
    }

    /// <summary>
    /// Determines whether the specified path contains a root (e.g., "C:\" or "/").
    /// </summary>
    /// <param name="path">The path to test.</param>
    /// <returns><c>true</c> if the path is rooted; otherwise <c>false</c>.</returns>
    public static bool CoreIsPathRooted(string path) => Path.IsPathRooted(path);

    /// <summary>
    /// Returns a relative path from one path to another.
    /// </summary>
    /// <param name="relativeTo">The base path.</param>
    /// <param name="path">The target path.</param>
    /// <returns>The relative path from <paramref name="relativeTo"/> to <paramref name="path"/>.</returns>
    public static string CoreGetRelativePath(string relativeTo, string path) => 
        Path.GetRelativePath(relativeTo, path);

    // ========================================================================
    // SEZIONE 12: Enumerazione Directory
    // ========================================================================

    /// <summary>
    /// Returns the names of all files in the specified directory.
    /// </summary>
    /// <param name="directoryPath">The directory to search.</param>
    /// <returns>A string array of file paths, or an empty array if the directory does not exist.</returns>
    public static string[] CoreGetFiles(string directoryPath) => 
        !Directory.Exists(directoryPath) ? [] : Directory.GetFiles(directoryPath);

    /// <summary>
    /// Returns the names of all files matching a search pattern in the specified directory.
    /// </summary>
    /// <param name="directoryPath">The directory to search.</param>
    /// <param name="searchPattern">The search string to match against file names (e.g., "*.txt").</param>
    /// <param name="searchOption">Specifies whether to search only the top directory or all subdirectories.</param>
    /// <returns>A string array of matching file paths, or an empty array if the directory does not exist.</returns>
    public static string[] CoreGetFiles(string directoryPath, string searchPattern, 
        SearchOption searchOption = SearchOption.TopDirectoryOnly) => 
        !Directory.Exists(directoryPath) ? [] : Directory.GetFiles(directoryPath, searchPattern, searchOption);

    /// <summary>
    /// Returns the names of all subdirectories in the specified directory.
    /// </summary>
    /// <param name="directoryPath">The directory to search.</param>
    /// <param name="searchOption">Specifies whether to search only the top directory or all subdirectories.</param>
    /// <returns>A string array of directory paths, or an empty array if the directory does not exist.</returns>
    public static string[] CoreGetDirectories(string directoryPath, SearchOption searchOption = SearchOption.TopDirectoryOnly) => 
        !Directory.Exists(directoryPath) ? [] : Directory.GetDirectories(directoryPath, "*", searchOption);

    /// <summary>
    /// Enumerates all file system entries (files and directories) in the specified directory.
    /// </summary>
    /// <param name="directoryPath">The directory to enumerate.</param>
    /// <returns>An <see cref="IEnumerable{T}"/> of entry paths, or an empty collection if the directory does not exist.</returns>
    public static IEnumerable<string> CoreEnumerateFileSystemEntries(string directoryPath) => 
        !Directory.Exists(directoryPath) ? [] : Directory.EnumerateFileSystemEntries(directoryPath);

    // ========================================================================
    // SEZIONE 13: Creazione Directory e File
    // ========================================================================

    /// <summary>
    /// Creates a directory at the specified path (including any missing parent directories).
    /// </summary>
    /// <param name="directoryPath">The path of the directory to create.</param>
    /// <returns>A <see cref="DirectoryInfo"/> representing the created or existing directory.</returns>
    public static DirectoryInfo CoreCreateDirectory(string directoryPath) => Directory.CreateDirectory(directoryPath);

    /// <summary>
    /// Creates an empty file at the specified path, creating parent directories as needed.
    /// </summary>
    /// <param name="filePath">The path of the file to create.</param>
    public static void CoreCreateEmptyFile(string filePath)
    {
        var directory = Path.GetDirectoryName(filePath);
        if (!string.IsNullOrEmpty(directory) && !Directory.Exists(directory))
            Directory.CreateDirectory(directory);
        File.Create(filePath).Dispose();
    }
}
