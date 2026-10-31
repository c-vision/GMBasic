// FILE: Basic.Runtime.PrgFormat.cs
// Serializzazione/deserializzazione del formato .prg (P-CODE Program).

using System.Text;

namespace Basic.Runtime;

/// <summary>Versione serializzabile di CompiledChunk per il formato .prg.</summary>
public sealed class PrgChunk
{
    public byte[] Code { get; set; } = [];
    public object?[] Constants { get; set; } = [];
    public int[] Lines { get; set; } = [];
    public string? SourceName { get; set; }
}

/// <summary>Informazioni su un file .prg.</summary>
public readonly record struct PrgInfo(
    string SourceName,
    int OpcodeCount,
    int ConstantCount,
    long FileSize,
    uint Crc32
);

/// <summary>Formato file .prg: magic + version + CompiledChunk serializzato.</summary>
public static class PrgFormat
{
    // Magic number: "PCOD" in ASCII (little-endian: 0x444F4350)
    public const uint Magic = 0x50434F44;
    public const ushort Version = 1;

    // Tipi costanti serializzate
    private const byte ConstNull = 0;
    private const byte ConstDouble = 1;
    private const byte ConstString = 2;

    /// <summary>Scrive un PrgChunk in formato .prg.</summary>
    public static void Write(string path, PrgChunk chunk)
    {
        using var stream = File.Create(path);
        using var writer = new BinaryWriter(stream, Encoding.UTF8);

        // Magic + version
        writer.Write(Magic);
        writer.Write(Version);

        // Source name (UTF-8)
        var name = chunk.SourceName ?? Path.GetFileNameWithoutExtension(path) ?? "untitled";
        var nameBytes = Encoding.UTF8.GetBytes(name);
        writer.Write(nameBytes.Length);
        writer.Write(nameBytes);

        // Code
        var code = chunk.Code;
        writer.Write(code.Length);
        foreach (var b in code)
            writer.Write(b);

        // Constants
        var constants = chunk.Constants;
        writer.Write(constants.Length);
        foreach (var c in constants)
        {
            if (c == null)
            {
                writer.Write(ConstNull);
            }
            else if (c is double d)
            {
                writer.Write(ConstDouble);
                writer.Write(d);
            }
            else if (c is string s)
            {
                writer.Write(ConstString);
                var sBytes = Encoding.UTF8.GetBytes(s);
                writer.Write(sBytes.Length);
                writer.Write(sBytes);
            }
            else if (c is int i)
            {
                writer.Write(ConstDouble);
                writer.Write((double)i);
            }
            else if (c is long l)
            {
                writer.Write(ConstDouble);
                writer.Write((double)l);
            }
            else
            {
                // Fallback: serializza come stringa
                var str = c.ToString() ?? "";
                writer.Write(ConstString);
                var sBytes = Encoding.UTF8.GetBytes(str);
                writer.Write(sBytes.Length);
                writer.Write(sBytes);
            }
        }

        // Lines
        var lines = chunk.Lines;
        writer.Write(lines.Length);
        foreach (var line in lines)
            writer.Write(line);
    }

    /// <summary>Legge un file .prg e restituisce un PrgChunk.</summary>
    public static PrgChunk Read(string path)
    {
        using var stream = File.OpenRead(path);
        using var reader = new BinaryReader(stream, Encoding.UTF8);

        var magic = reader.ReadUInt32();
        if (magic != Magic)
            throw new InvalidDataException(
                $"Invalid .prg magic: 0x{magic:X8}. Expected 0x{Magic:X8}.");

        var version = reader.ReadUInt16();
        if (version != Version)
            throw new InvalidDataException(
                $"Unsupported .prg version: {version}. Expected {Version}.");

        // Source name
        var nameLen = reader.ReadInt32();
        if (nameLen > 0)
            reader.ReadBytes(nameLen);

        var codeList = new List<byte>();
        var constList = new List<object?>();
        var lineList = new List<int>();

        // Code
        var codeCount = reader.ReadInt32();
        for (var i = 0; i < codeCount; i++)
            codeList.Add(reader.ReadByte());

        // Constants
        var constCount = reader.ReadInt32();
        for (var i = 0; i < constCount; i++)
        {
            var type = reader.ReadByte();
            switch (type)
            {
                case ConstNull:
                    constList.Add(null);
                    break;
                case ConstDouble:
                    constList.Add(reader.ReadDouble());
                    break;
                case ConstString:
                    var sLen = reader.ReadInt32();
                    var sBytes = reader.ReadBytes(sLen);
                    constList.Add(Encoding.UTF8.GetString(sBytes));
                    break;
                default:
                    throw new InvalidDataException($"Unknown constant type: {type}");
            }
        }

        // Lines
        var lineCount = reader.ReadInt32();
        for (var i = 0; i < lineCount; i++)
            lineList.Add(reader.ReadInt32());

        return new PrgChunk
        {
            Code = [.. codeList],
            Constants = [.. constList],
            Lines = [.. lineList],
        };
    }

    /// <summary>Verifica se un file e' un .prg valido (legge solo il magic).</summary>
    public static bool IsValid(string path)
    {
        try
        {
            using var stream = File.OpenRead(path);
            if (stream.Length < 6) return false;
            using var reader = new BinaryReader(stream, Encoding.UTF8);
            return reader.ReadUInt32() == Magic;
        }
        catch { return false; }
    }

    /// <summary>Restituisce info su un file .prg senza caricarlo completamente.</summary>
    public static PrgInfo GetInfo(string path)
    {
        using var stream = File.OpenRead(path);
        using var reader = new BinaryReader(stream, Encoding.UTF8);

        var magic = reader.ReadUInt32();
        var version = reader.ReadUInt16();

        var nameLen = reader.ReadInt32();
        var name = nameLen > 0 ? Encoding.UTF8.GetString(reader.ReadBytes(nameLen)) : "";

        var opcodeCount = reader.ReadInt32();

        return new PrgInfo(name, opcodeCount, -1, new FileInfo(path).Length, magic);
    }
}
