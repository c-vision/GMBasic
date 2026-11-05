// FILE: Interpreter.StdLib.Special.cs
// ReSharper disable InconsistentNaming
// ReSharper disable CheckNamespace

/* ==== GMB-STDLIB-AUDIT-BEGIN ====
 * FILE: Special — namespace 'SPECIAL'
 * 19 funzioni | 0 comandi | 0 tipi | 0 costanti
 *
 * FUNZIONI (19):
 *   BESSELI
 *   BESSELJ
 *   BESSELK
 *   BESSELY
 *   BETA
 *   BETAINC
 *   BETAINCINV
 *   BETALN
 *   DIGAMMA
 *   ERF
 *   ERFC
 *   ERFCINV
 *   ERFINV
 *   GAMMA
 *   GAMMAINC
 *   GAMMAINCC
 *   GAMMAINCCINV
 *   GAMMAINCINV
 *   LGAMMA
    ==== GMB-STDLIB-AUDIT-END ==== */


using System.Collections.Immutable;

namespace Basic;

public sealed partial class StdLib
{
    // ========================================================================
    // --- SPECIAL NAMESPACE (nucleo scipy.special) ---
    // ========================================================================

    /// <summary>
    /// Crea e registra tutte le definizioni per il namespace 'SPECIAL'.
    /// Implementazione C#: CommonLib.Numerics.Special (MathNet SpecialFunctions).
    /// Porting C previsto: gmb_gamma_p/gmb_lgamma/gmb_beta_fn/gmb_erf_c in gmb_math.c.
    /// </summary>
    private void CreateSpecialNamespace()
    {
        const string namespaceName = "SPECIAL";
        const SymbolFlags publicFlag = SymbolFlags.IsPublic;
        const SymbolFlags noFlags = SymbolFlags.None;

        var procedures = new ProcedureDefinition[]
        {
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SpecialGamma, "GAMMA"),
                [new Parameter(-1, T(TokenKind.Identifier, "x"), new DoubleType(), noFlags, null, false)],
                new DoubleType(), TokenKind.SpecialGamma),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SpecialLGamma, "LGAMMA"),
                [new Parameter(-1, T(TokenKind.Identifier, "x"), new DoubleType(), noFlags, null, false)],
                new DoubleType(), TokenKind.SpecialLGamma),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SpecialBeta, "BETA"),
                [new Parameter(-1, T(TokenKind.Identifier, "a"), new DoubleType(), noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "b"), new DoubleType(), noFlags, null, false)],
                new DoubleType(), TokenKind.SpecialBeta),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SpecialErf, "ERF"),
                [new Parameter(-1, T(TokenKind.Identifier, "x"), new DoubleType(), noFlags, null, false)],
                new DoubleType(), TokenKind.SpecialErf),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SpecialErfc, "ERFC"),
                [new Parameter(-1, T(TokenKind.Identifier, "x"), new DoubleType(), noFlags, null, false)],
                new DoubleType(), TokenKind.SpecialErfc),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SpecialGammaInc, "GAMMAINC"),
                [new Parameter(-1, T(TokenKind.Identifier, "a"), new DoubleType(), noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "x"), new DoubleType(), noFlags, null, false)],
                new DoubleType(), TokenKind.SpecialGammaInc),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SpecialGammaIncc, "GAMMAINCC"),
                [new Parameter(-1, T(TokenKind.Identifier, "a"), new DoubleType(), noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "x"), new DoubleType(), noFlags, null, false)],
                new DoubleType(), TokenKind.SpecialGammaIncc),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SpecialBetaInc, "BETAINC"),
                [new Parameter(-1, T(TokenKind.Identifier, "a"), new DoubleType(), noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "b"), new DoubleType(), noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "x"), new DoubleType(), noFlags, null, false)],
                new DoubleType(), TokenKind.SpecialBetaInc),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SpecialDigamma, "DIGAMMA"),
                [new Parameter(-1, T(TokenKind.Identifier, "x"), new DoubleType(), noFlags, null, false)],
                new DoubleType(), TokenKind.SpecialDigamma),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SpecialZeta, "ZETA"),
                [new Parameter(-1, T(TokenKind.Identifier, "s"), new DoubleType(), noFlags, null, false)],
                new DoubleType(), TokenKind.SpecialZeta),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SpecialZetaHurwitz, "ZETAHURWITZ"),
                [new Parameter(-1, T(TokenKind.Identifier, "s"), new DoubleType(), noFlags, null, false), new Parameter(-1, T(TokenKind.Identifier, "a"), new DoubleType(), noFlags, null, false)],
                new DoubleType(), TokenKind.SpecialZetaHurwitz),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SpecialPolygamma, "POLYGAMMA"),
                [new Parameter(-1, T(TokenKind.Identifier, "n"), new DoubleType(), noFlags, null, false), new Parameter(-1, T(TokenKind.Identifier, "x"), new DoubleType(), noFlags, null, false)],
                new DoubleType(), TokenKind.SpecialPolygamma),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SpecialHypergeom2F1, "HYPERGEOM2F1"),
                [new Parameter(-1, T(TokenKind.Identifier, "a"), new DoubleType(), noFlags, null, false), new Parameter(-1, T(TokenKind.Identifier, "b"), new DoubleType(), noFlags, null, false), new Parameter(-1, T(TokenKind.Identifier, "c"), new DoubleType(), noFlags, null, false), new Parameter(-1, T(TokenKind.Identifier, "x"), new DoubleType(), noFlags, null, false)],
                new DoubleType(), TokenKind.SpecialHypergeom2F1),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SpecialHypergeom1F1, "HYPERGEOM1F1"),
                [new Parameter(-1, T(TokenKind.Identifier, "a"), new DoubleType(), noFlags, null, false), new Parameter(-1, T(TokenKind.Identifier, "b"), new DoubleType(), noFlags, null, false), new Parameter(-1, T(TokenKind.Identifier, "x"), new DoubleType(), noFlags, null, false)],
                new DoubleType(), TokenKind.SpecialHypergeom1F1),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SpecialLambertW, "LAMBERTW"),
                [new Parameter(-1, T(TokenKind.Identifier, "x"), new DoubleType(), noFlags, null, false)],
                new DoubleType(), TokenKind.SpecialLambertW),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SpecialMittagLeffler, "MITTAGLEFFLER"),
                [new Parameter(-1, T(TokenKind.Identifier, "alpha"), new DoubleType(), noFlags, null, false), new Parameter(-1, T(TokenKind.Identifier, "beta"), new DoubleType(), noFlags, null, false), new Parameter(-1, T(TokenKind.Identifier, "x"), new DoubleType(), noFlags, null, false)],
                new DoubleType(), TokenKind.SpecialMittagLeffler),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SpecialStruveH, "STRUVEH"),
                [new Parameter(-1, T(TokenKind.Identifier, "v"), new DoubleType(), noFlags, null, false), new Parameter(-1, T(TokenKind.Identifier, "x"), new DoubleType(), noFlags, null, false)],
                new DoubleType(), TokenKind.SpecialStruveH),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SpecialKelvinBer, "KELVINBER"),
                [new Parameter(-1, T(TokenKind.Identifier, "x"), new DoubleType(), noFlags, null, false)],
                new DoubleType(), TokenKind.SpecialKelvinBer),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SpecialKelvinBei, "KELVINBEI"),
                [new Parameter(-1, T(TokenKind.Identifier, "x"), new DoubleType(), noFlags, null, false)],
                new DoubleType(), TokenKind.SpecialKelvinBei),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SpecialParabolicCylinder, "PARABOLICCYLINDER"),
                [new Parameter(-1, T(TokenKind.Identifier, "a"), new DoubleType(), noFlags, null, false), new Parameter(-1, T(TokenKind.Identifier, "x"), new DoubleType(), noFlags, null, false)],
                new DoubleType(), TokenKind.SpecialParabolicCylinder),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SpecialBesselJ, "BESSELJ"),
                [new Parameter(-1, T(TokenKind.Identifier, "v"), new DoubleType(), noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "x"), new DoubleType(), noFlags, null, false)],
                new DoubleType(), TokenKind.SpecialBesselJ),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SpecialBesselY, "BESSELY"),
                [new Parameter(-1, T(TokenKind.Identifier, "v"), new DoubleType(), noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "x"), new DoubleType(), noFlags, null, false)],
                new DoubleType(), TokenKind.SpecialBesselY),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SpecialBesselI, "BESSELI"),
                [new Parameter(-1, T(TokenKind.Identifier, "v"), new DoubleType(), noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "x"), new DoubleType(), noFlags, null, false)],
                new DoubleType(), TokenKind.SpecialBesselI),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SpecialBesselK, "BESSELK"),
                [new Parameter(-1, T(TokenKind.Identifier, "v"), new DoubleType(), noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "x"), new DoubleType(), noFlags, null, false)],
                new DoubleType(), TokenKind.SpecialBesselK),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SpecialBetaLn, "BETALN"),
                [new Parameter(-1, T(TokenKind.Identifier, "a"), new DoubleType(), noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "b"), new DoubleType(), noFlags, null, false)],
                new DoubleType(), TokenKind.SpecialBetaLn),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SpecialErfInv, "ERFINV"),
                [new Parameter(-1, T(TokenKind.Identifier, "x"), new DoubleType(), noFlags, null, false)],
                new DoubleType(), TokenKind.SpecialErfInv),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SpecialErfcInv, "ERFCINV"),
                [new Parameter(-1, T(TokenKind.Identifier, "x"), new DoubleType(), noFlags, null, false)],
                new DoubleType(), TokenKind.SpecialErfcInv),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SpecialGammaIncInv, "GAMMAINCINV"),
                [new Parameter(-1, T(TokenKind.Identifier, "a"), new DoubleType(), noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "y"), new DoubleType(), noFlags, null, false)],
                new DoubleType(), TokenKind.SpecialGammaIncInv),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SpecialGammaInccInv, "GAMMAINCCINV"),
                [new Parameter(-1, T(TokenKind.Identifier, "a"), new DoubleType(), noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "y"), new DoubleType(), noFlags, null, false)],
                new DoubleType(), TokenKind.SpecialGammaInccInv),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SpecialBetaIncInv, "BETAINCINV"),
                [new Parameter(-1, T(TokenKind.Identifier, "a"), new DoubleType(), noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "b"), new DoubleType(), noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "y"), new DoubleType(), noFlags, null, false)],
                new DoubleType(), TokenKind.SpecialBetaIncInv),
        };

        RegisterBuiltinNamespace(namespaceName, procedures, ImmutableDictionary<string, DataType>.Empty);
    }
}
