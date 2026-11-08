// FILE: StdLib.Compression.cs
// ReSharper disable InconsistentNaming

/* ==== GMB-STDLIB-AUDIT-BEGIN ====
 * FILE: Archive — namespace 'ARCHIVE'
 * 2 funzioni | 2 comandi | 0 tipi | 0 costanti
 *
 * FUNZIONI (2):
 *   ZIP
 *   UNZIP
 * COMANDI (2):
 *   UNZIP
 *   ZIP
 *
 * NOTA: le 19 funzioni di compressione stream (COMPRESS/DECOMPRESS, GZIP/UNGZIP,
 * ZLIB/UNZLIB, DEFLATE/UNDEFLATE, BZIP2/UNBZIP2, BROTLI/UNBROTLI, ZSTD/UNZSTD,
 * LZ4/UNLZ4, XZ/UNXZ, ALGORITHMS) NON sono più superficie di linguaggio: vivono nel
 * pacchetto FFI `compression` (namespace COMPRESSION). ZIP/UNZIP restano builtin
 * (runtime) nel namespace ARCHIVE.
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
        // ZIP/UNZIP restano builtin (archivi STORE del runtime), ma in un namespace
        // PROPRIO (ARCHIVE): il nome COMPRESSION è del pacchetto FFI `compression`.
        // Comandi legacy invariati: `ZIP src TO dest`, `UNZIP arc TO dir`.
        const string namespaceName = "ARCHIVE";
        const SymbolFlags publicFuncFlag =
            SymbolFlags.IsPublic | SymbolFlags.IsFunction;
        const SymbolFlags noFlags = SymbolFlags.None;

        RegisterCommand(TokenKind.Zip, "ZIP", namespaceName,
            static (parser, ns) => Parser.ParseZipStatement(parser, ns));
        RegisterCommand(TokenKind.Unzip, "UNZIP", namespaceName,
            static (parser, ns) => Parser.ParseUnzipStatement(parser, ns));

        var procedures = new ProcedureDefinition[]
        {
            // ARCHIVE.ZIP(sources, dest) -> BIT — file/cartelle (sources: ';' o array)
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.Zip, "ZIP"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "sources"), new StringType(), noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "dest"), new StringType(), noFlags, null, false)
                ],
                new BitType(), TokenKind.Zip),

            // ARCHIVE.UNZIP(archive, dir) -> BIT
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
