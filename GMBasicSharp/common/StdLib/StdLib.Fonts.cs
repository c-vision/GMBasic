using System.Collections.Immutable;

namespace Basic;

public sealed partial class StdLib
{
    // ========================================================================
    // --- FONTS NAMESPACE: TrueType/OpenType (rasterizzazione e metriche) ---
    // ========================================================================

    /// <summary>
    /// Registra il namespace 'FONTS'. Sono <b>funzioni</b> (§1/§3): parentesi
    /// obbligatorie, non scrivono nulla, ritornano un valore.
    ///
    /// L'algoritmo esiste <b>una sola volta</b>, nel runtime C condiviso
    /// (native/gmb_runtime_c/src/gmb_fonts.c sopra stb_truetype): i backend delegano.
    /// Convertire un font outline in bitmap <b>non</b> è una responsabilità di BASIC.
    /// </summary>
    private void CreateFontsNamespace()
    {
        const string namespaceName = "FONTS";
        const SymbolFlags publicFuncFlag = SymbolFlags.IsPublic | SymbolFlags.IsFunction;
        const SymbolFlags noFlags = SymbolFlags.None;

        NativeProcedureDefinition[] procedures =
        [
            // TTRENDER(path, size, codepoint) -> array di BYTE: copertura alpha 0..255,
            // riga per riga dall'alto, larghezza*altezza elementi (vedi TTMETRICS).
            // Codepoint non coperto dal font -> array vuoto (non è un errore).
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.TtRenderFunc, "TTRENDER"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "path"), new StringType(), noFlags, null, false),
                    // size/codepoint: INFERITI. Con DoubleType/LongType un letterale intero non
                    // veniva accettato dalla risoluzione dell'overload ("nessuna funzione ... che
                    // accetti 3 argomenti"); il valore è numerico per contratto e il backend
                    // applica la conversione (double/int) prima della chiamata.
                    new Parameter(-1, T(TokenKind.Identifier, "size"), new InferredType(), noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "codepoint"), new InferredType(), noFlags, null, false)
                ], new ArrayType(new ByteType()), TokenKind.TtRenderFunc),

            // TTMETRICS(path, size, codepoint) -> 8 numeri, nell'ordine:
            // [0] larghezza bitmap px   [1] altezza bitmap px
            // [2] xoff dal pen          [3] yoff dal baseline (verso l'alto)
            // [4] advance px            [5] ascent px
            // [6] descent px (negativo) [7] line gap px
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.TtMetricsFunc, "TTMETRICS"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "path"), new StringType(), noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "size"), new InferredType(), noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "codepoint"), new InferredType(), noFlags, null, false)
                ], new ArrayType(new DoubleType()), TokenKind.TtMetricsFunc),

            // TTGLYPHS(path) -> codepoint coperti dal cmap, in ordine crescente
            // (0..0xFFFF, surrogati esclusi). Serve a decidere QUALI glifi importare.
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.TtGlyphsFunc, "TTGLYPHS"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "path"), new StringType(), noFlags, null, false)
                ], new ArrayType(new DoubleType()), TokenKind.TtGlyphsFunc)
        ];

        RegisterBuiltinNamespace(namespaceName, procedures, ImmutableDictionary<string, DataType>.Empty);
    }
}
