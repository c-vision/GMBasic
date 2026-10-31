using CommonLib.Consoles.Tui;
using CommonLib.Screens;
using ClipperLib.Clipper;

namespace ClipperLib.Controls;

public static class FileTheme
{
    public readonly struct IconInfo
    {
        public string Icon { get; }
        public CellColor Color { get; }

        public IconInfo(string icon, CellColor color)
        {
            Icon = icon;
            Color = color;
        }
    }

    public static string IconSeparator { get; set; } = " ";

    public static IconInfo IconForPath(string path)
    {
        var name = Path.GetFileName(path) ?? "";
        var isSymlink = false;
        var isDir = false;
        var isExecutable = false;

        try
        {
            var attr = File.GetAttributes(path);
            isDir = attr.HasFlag(FileAttributes.Directory);
            var info = new FileInfo(path);
            isExecutable = info.Extension.Equals(".exe", StringComparison.OrdinalIgnoreCase)
                || info.Extension.Equals(".com", StringComparison.OrdinalIgnoreCase)
                || info.Extension.Equals(".bat", StringComparison.OrdinalIgnoreCase)
                || info.Extension.Equals(".cmd", StringComparison.OrdinalIgnoreCase);
        }
        catch { }

        return IconFor(name, isDir, isSymlink, isExecutable);
    }

    public static CellColor ColorForPath(string path)
    {
        var isSymlink = false;
        var isDir = false;
        var isExecutable = false;

        try
        {
            var attr = File.GetAttributes(path);
            isDir = attr.HasFlag(FileAttributes.Directory);
            var info = new FileInfo(path);
            isExecutable = info.Extension.Equals(".exe", StringComparison.OrdinalIgnoreCase)
                || info.Extension.Equals(".com", StringComparison.OrdinalIgnoreCase)
                || info.Extension.Equals(".bat", StringComparison.OrdinalIgnoreCase)
                || info.Extension.Equals(".cmd", StringComparison.OrdinalIgnoreCase);
        }
        catch { }

        var name = Path.GetFileName(path) ?? "";

        if (isSymlink)
            return ColorSymlink;
        if (isDir)
            return ColorDir;

        var lowerName = name.ToLowerInvariant();
        var ext = Path.GetExtension(lowerName);
        if (!string.IsNullOrEmpty(ext))
            ext = ext.TrimStart('.');

        if (CategoryImages.Contains(ext))
            return ColorImage;
        if (CategoryAudio.Contains(ext))
            return ColorAudio;
        if (CategoryVideo.Contains(ext))
            return ColorVideo;
        if (CategoryArchive.Contains(ext))
            return ColorArchive;
        if (CategorySource.Contains(ext))
            return ColorSource;

        if (isExecutable)
            return ColorExecutable;

        return ColorFile;
    }

    public static IconInfo IconFor(string name, bool isDir, bool isSymlink, bool isExecutable)
    {
        CellColor color;

        if (isSymlink)
        {
            color = ColorSymlink;
            return new IconInfo(isDir ? "\U0001F4C1" : "\U0001F4C4", color);
        }

        if (isDir)
        {
            color = ColorDir;
            return new IconInfo("\U0001F4C1", color);
        }

        var lower = name.ToLowerInvariant();

        if (IconsByName.TryGetValue(lower, out var icon))
        {
            color = ColorForName(lower);
            return new IconInfo(icon, color);
        }

        var ext = Path.GetExtension(lower);
        if (!string.IsNullOrEmpty(ext))
            ext = ext.TrimStart('.');

        if (!string.IsNullOrEmpty(ext) && IconsByExtension.TryGetValue(ext, out var extIcon))
        {
            color = ColorForExt(ext);
            return new IconInfo(extIcon, color);
        }

        if (isExecutable)
        {
            color = ColorExecutable;
            return new IconInfo("\u26A1", color);
        }

        color = ColorFile;
        return new IconInfo("\U0001F4C4", color);
    }

    private static CellColor ColorForName(string lower)
    {
        return lower switch
        {
            ".bashrc" => CellColor.DarkGreen,
            ".gitignore" => CellColor.DarkYellow,
            ".vimrc" => CellColor.Green,
            "dockerfile" => CellColor.Cyan,
            "makefile" => CellColor.Magenta,
            "node_modules" => CellColor.Grey,
            _ => ColorFile,
        };
    }

    private static CellColor ColorForExt(string ext)
    {
        if (CategoryImages.Contains(ext))
            return ColorImage;
        if (CategoryAudio.Contains(ext))
            return ColorAudio;
        if (CategoryVideo.Contains(ext))
            return ColorVideo;
        if (CategoryArchive.Contains(ext))
            return ColorArchive;
        if (CategorySource.Contains(ext))
            return ColorSource;
        return ColorFile;
    }

    // Color definitions matching ANSI 256 values from lsd
    private static readonly CellColor ColorDir = new() { R = 0, G = 135, B = 255 };
    private static readonly CellColor ColorSymlink = new() { R = 0, G = 215, B = 215 };
    private static readonly CellColor ColorExecutable = new() { R = 0, G = 215, B = 0 };
    private static readonly CellColor ColorFile = new() { R = 215, G = 215, B = 0 };
    private static readonly CellColor ColorImage = new() { R = 215, G = 135, B = 0 };
    private static readonly CellColor ColorAudio = new() { R = 215, G = 135, B = 135 };
    private static readonly CellColor ColorVideo = new() { R = 215, G = 0, B = 255 };
    private static readonly CellColor ColorArchive = new() { R = 0, G = 175, B = 135 };
    private static readonly CellColor ColorSource = new() { R = 175, G = 215, B = 0 };

    private static readonly Dictionary<string, string> IconsByName = new(StringComparer.OrdinalIgnoreCase)
    {
        [".bashrc"] = "\u2699",
        [".git"] = "\U0001F5C3",
        [".gitignore"] = "\U0001F5C3",
        [".vimrc"] = "\u2699",
        ["cargo.lock"] = "\uD83D\uDCE6",
        ["cargo.toml"] = "\uD83D\uDCE6",
        ["dockerfile"] = "\U0001F433",
        ["makefile"] = "\u2699",
        ["node_modules"] = "\U0001F4E6",
        ["package.json"] = "\uD83D\uDCE6",
        ["pyproject.toml"] = "\U0001F40D",
        ["readme"] = "\U0001F4D6",
        ["readme.md"] = "\U0001F4D6",
        ["rustfmt.toml"] = "\uD83E\uDE80",
        ["src"] = "\U0001F4C1",
        ["vendor"] = "\U0001F4E6",
        ["yarn.lock"] = "\U0001F3AA",
    };

    private static readonly Dictionary<string, string> IconsByExtension = new(StringComparer.OrdinalIgnoreCase)
    {
        ["7z"] = "\U0001F4E6",
        ["ai"] = "\uD83C\uDFA8",
        ["apk"] = "\uD83D\uDCF1",
        ["avi"] = "\U0001F3AC",
        ["bmp"] = "\U0001F5BC",
        ["bz2"] = "\U0001F4E6",
        ["c"] = "\uD83D\uDD0C",
        ["cpp"] = "\uD83D\uDD0C",
        ["cs"] = "\u2665",
        ["css"] = "\uD83C\uDFA8",
        ["csv"] = "\U0001F4CA",
        ["db"] = "\U0001F5C3",
        ["deb"] = "\U0001F4E6",
        ["doc"] = "\U0001F4C4",
        ["docx"] = "\U0001F4C4",
        ["exe"] = "\u2699",
        ["flac"] = "\U0001F3B5",
        ["flv"] = "\U0001F3AC",
        ["gif"] = "\U0001F5BC",
        ["go"] = "\U0001F426",
        ["gz"] = "\U0001F4E6",
        ["h"] = "\uD83D\uDD0C",
        ["hpp"] = "\uD83D\uDD0C",
        ["html"] = "\U0001F310",
        ["ico"] = "\U0001F5BC",
        ["jar"] = "\u2615",
        ["java"] = "\u2615",
        ["jpeg"] = "\U0001F5BC",
        ["jpg"] = "\U0001F5BC",
        ["js"] = "\uD83D\uDCDD",
        ["json"] = "\uD83D\uDCDD",
        ["jsx"] = "\u269B",
        ["lua"] = "\U0001F4D6",
        ["m4a"] = "\U0001F3B5",
        ["m4v"] = "\U0001F3AC",
        ["md"] = "\U0001F4D6",
        ["mkd"] = "\U0001F4D6",
        ["mkv"] = "\U0001F3AC",
        ["mov"] = "\U0001F3AC",
        ["mp3"] = "\U0001F3B5",
        ["mp4"] = "\U0001F3AC",
        ["ogg"] = "\U0001F3B5",
        ["ogv"] = "\U0001F3AC",
        ["pdf"] = "\U0001F4D5",
        ["php"] = "\U0001F431",
        ["png"] = "\U0001F5BC",
        ["ppt"] = "\U0001F4CA",
        ["pptx"] = "\U0001F4CA",
        ["py"] = "\U0001F40D",
        ["pyc"] = "\U0001F40D",
        ["rar"] = "\U0001F4E6",
        ["rb"] = "\u2665",
        ["rs"] = "\uD83E\uDE80",
        ["rpm"] = "\U0001F4E6",
        ["sh"] = "\u26A1",
        ["svg"] = "\U0001F5BC",
        ["tar"] = "\U0001F4E6",
        ["tgz"] = "\U0001F4E6",
        ["ts"] = "\uD83D\uDCDD",
        ["tsx"] = "\u269B",
        ["txt"] = "\U0001F4C4",
        ["wav"] = "\U0001F3B5",
        ["webm"] = "\U0001F3AC",
        ["webp"] = "\U0001F5BC",
        ["xls"] = "\U0001F4CA",
        ["xlsx"] = "\U0001F4CA",
        ["xml"] = "\u2699",
        ["yaml"] = "\u2699",
        ["yml"] = "\u2699",
        ["zip"] = "\U0001F4E6",
    };

    private static readonly HashSet<string> CategoryImages = new(StringComparer.OrdinalIgnoreCase)
    {
        "png", "jpg", "jpeg", "gif", "bmp", "ico", "svg", "webp", "heic", "heif", "heix", "tiff",
        "pxm", "xcf", "image",
    };

    private static readonly HashSet<string> CategoryAudio = new(StringComparer.OrdinalIgnoreCase)
    {
        "mp3", "flac", "wav", "ogg", "opus", "m4a", "ape", "cue", "wma",
    };

    private static readonly HashSet<string> CategoryVideo = new(StringComparer.OrdinalIgnoreCase)
    {
        "mp4", "mkv", "avi", "mov", "flv", "webm", "ogv", "m4v", "wmv", "video",
    };

    private static readonly HashSet<string> CategoryArchive = new(StringComparer.OrdinalIgnoreCase)
    {
        "zip", "tar", "gz", "tgz", "bz2", "xz", "7z", "rar", "ar", "taz", "tbz", "tbz2", "zst", "lz",
    };

    private static readonly HashSet<string> CategorySource = new(StringComparer.OrdinalIgnoreCase)
    {
        "rs", "c", "cpp", "cc", "cp", "cxx", "h", "hpp", "hxx", "hh", "go", "py", "js", "ts", "jsx",
        "tsx", "java", "class", "jar", "rb", "php", "pl", "pm", "lua", "sh", "bash", "zsh", "fish",
        "ksh", "csh", "css", "scss", "sass", "html", "htm", "xml", "json", "yaml", "yml", "toml",
        "ini", "cfg", "conf", "md", "mkd", "markdown", "txt", "sql", "swift", "kt", "kts", "dart",
        "elm", "erl", "ex", "exs", "hs", "lhs", "scala", "clj", "cljs", "vim", "asm", "s", "nim",
        "zig", "pp", "ml", "mli", "proto", "glsl", "vert", "frag", "cs",
    };
}
