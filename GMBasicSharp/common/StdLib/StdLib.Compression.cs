// FILE: StdLib.Compression.cs
// ReSharper disable InconsistentNaming

/* ==== GMB-STDLIB-AUDIT-BEGIN ====
 * FILE: Compression — namespace 'COMPRESSION'
 * 21 funzioni | 2 comandi | 0 tipi | 0 costanti
 *
 * FUNZIONI (21):
 *   ALGORITHMS
 *   BROTLI
 *   BZIP2
 *   COMPRESS
 *   DECOMPRESS
 *   DEFLATE
 *   GZIP
 *   LZ4
 *   UNBROTLI
 *   UNBZIP2
 *   UNDEFLATE
 *   UNGZIP
 *   UNLZ4
 *   UNXZ
 *   UNZIP
 *   UNZLIB
 *   UNZSTD
 *   XZ
 *   ZIP
 *   ZLIB
 *   ZSTD
 * COMANDI (2):
 *   UNZIP
 *   ZIP
    ==== GMB-STDLIB-AUDIT-END ==== */


using System.Collections.Immutable;

namespace Basic;

public sealed partial class StdLib
{
    // ========================================================================
    // --- COMPRESSION NAMESPACE ---
    // ========================================================================

    private void CreateCompressionNamespace()
    {
        const string namespaceName = "COMPRESSION";
        const SymbolFlags publicFuncFlag =
            SymbolFlags.IsPublic | SymbolFlags.IsFunction;
        const SymbolFlags noFlags = SymbolFlags.None;

        // --- Comandi legacy (statement) ---
        RegisterCommand(TokenKind.Zip, "ZIP", namespaceName,
            static (parser, ns) => Parser.ParseZipStatement(parser, ns));
        RegisterCommand(TokenKind.Unzip, "UNZIP", namespaceName,
            static (parser, ns) => Parser.ParseUnzipStatement(parser, ns));

        var procedures = new ProcedureDefinition[]
        {
            // COMPRESSION.COMPRESS(data$, algo$) -> STRING (Base64)
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.Compress, "COMPRESS"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "data"), new StringType(), noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "algorithm"), new StringType(), noFlags, null, false)
                ],
                new StringType(), TokenKind.Compress),

            // COMPRESSION.DECOMPRESS(data$, algo$) -> STRING
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.Decompress, "DECOMPRESS"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "data"), new StringType(), noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "algorithm"), new StringType(), noFlags, null, false)
                ],
                new StringType(), TokenKind.Decompress),

            // COMPRESSION.GZIP(data$) / UNGZIP(data$) / BROTLI / UNBROTLI / ZLIB / UNZLIB / BZIP2 / UNBZIP2 / DEFLATE / UNDEFLATE
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.Gzip, "GZIP"),
                [new Parameter(-1, T(TokenKind.Identifier, "data"), new StringType(), noFlags, null, false)],
                new StringType(), TokenKind.Gzip),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.Ungzip, "UNGZIP"),
                [new Parameter(-1, T(TokenKind.Identifier, "data"), new StringType(), noFlags, null, false)],
                new StringType(), TokenKind.Ungzip),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.Brotli, "BROTLI"),
                [new Parameter(-1, T(TokenKind.Identifier, "data"), new StringType(), noFlags, null, false)],
                new StringType(), TokenKind.Brotli),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.Zlib, "ZLIB"),
                [new Parameter(-1, T(TokenKind.Identifier, "data"), new StringType(), noFlags, null, false)],
                new StringType(), TokenKind.Zlib),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.Bzip2, "BZIP2"),
                [new Parameter(-1, T(TokenKind.Identifier, "data"), new StringType(), noFlags, null, false)],
                new StringType(), TokenKind.Bzip2),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.Deflate, "DEFLATE"),
                [new Parameter(-1, T(TokenKind.Identifier, "data"), new StringType(), noFlags, null, false)],
                new StringType(), TokenKind.Deflate),

            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.Unbrotli, "UNBROTLI"),
                [new Parameter(-1, T(TokenKind.Identifier, "data"), new StringType(), noFlags, null, false)],
                new StringType(), TokenKind.Unbrotli),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.Unzlib, "UNZLIB"),
                [new Parameter(-1, T(TokenKind.Identifier, "data"), new StringType(), noFlags, null, false)],
                new StringType(), TokenKind.Unzlib),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.Unbzip2, "UNBZIP2"),
                [new Parameter(-1, T(TokenKind.Identifier, "data"), new StringType(), noFlags, null, false)],
                new StringType(), TokenKind.Unbzip2),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.Undeflate, "UNDEFLATE"),
                [new Parameter(-1, T(TokenKind.Identifier, "data"), new StringType(), noFlags, null, false)],
                new StringType(), TokenKind.Undeflate),

            // Nuovi algoritmi (2026-10-10): ZSTD/LZ4/XZ (+ inversi).
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.Zstd, "ZSTD"),
                [new Parameter(-1, T(TokenKind.Identifier, "data"), new StringType(), noFlags, null, false)],
                new StringType(), TokenKind.Zstd),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.Unzstd, "UNZSTD"),
                [new Parameter(-1, T(TokenKind.Identifier, "data"), new StringType(), noFlags, null, false)],
                new StringType(), TokenKind.Unzstd),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.Lz4, "LZ4"),
                [new Parameter(-1, T(TokenKind.Identifier, "data"), new StringType(), noFlags, null, false)],
                new StringType(), TokenKind.Lz4),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.Unlz4, "UNLZ4"),
                [new Parameter(-1, T(TokenKind.Identifier, "data"), new StringType(), noFlags, null, false)],
                new StringType(), TokenKind.Unlz4),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.Xz, "XZ"),
                [new Parameter(-1, T(TokenKind.Identifier, "data"), new StringType(), noFlags, null, false)],
                new StringType(), TokenKind.Xz),
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.Unxz, "UNXZ"),
                [new Parameter(-1, T(TokenKind.Identifier, "data"), new StringType(), noFlags, null, false)],
                new StringType(), TokenKind.Unxz),

            // COMPRESSION.ALGORITHMS() -> STRING
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.Algorithms, "ALGORITHMS"),
                [], new StringType(), TokenKind.Algorithms),

            // COMPRESSION.ZIP(sources, dest) -> BIT — comprime file/cartelle (sources: stringa con ';' o array)
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.Zip, "ZIP"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "sources"), new StringType(), noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "dest"), new StringType(), noFlags, null, false)
                ],
                new BitType(), TokenKind.Zip),

            // COMPRESSION.UNZIP(archive, dir) -> BIT
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.Unzip, "UNZIP"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "archive"), new StringType(), noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "dir"), new StringType(), noFlags, null, false)
                ],
                new BitType(), TokenKind.Unzip),
        };

        RegisterBuiltinNamespace(namespaceName, procedures, ImmutableDictionary<string, DataType>.Empty);
    }
}
