// FILE: Interpreter.StdLib.Joy.cs

/* ==== GMB-STDLIB-AUDIT-BEGIN ====
 * FILE: Joy — namespace 'JOY'
 * 4 funzioni | 0 comandi | 0 tipi | 0 costanti
 *
 * FUNZIONI (4):
 *   AVAILABLE
 *   MASK
 *   NAME
 *   PORTS
    ==== GMB-STDLIB-AUDIT-END ==== */


using System.Collections.Immutable;

namespace Basic;

public sealed partial class StdLib
{
    private void CreateJoyNamespace()
    {
        const string namespaceName = "JOY";
        const SymbolFlags publicFuncFlag = SymbolFlags.IsPublic | SymbolFlags.IsFunction;

        var procedures = new ProcedureDefinition[]
        {
            // MASK(port) - Bitmask dello stato del joystick (UP=1, DOWN=2, LEFT=4, RIGHT=8, FIRE=16, BUTTON2=32)
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.Joy, "MASK"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "port"), new IntegerType(),
                        SymbolFlags.None, null, false)
                ],
                new LongType(), TokenKind.Joy),

            // PORTS() - Numero di porte joystick disponibili
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.JoyCount, "PORTS"),
                [], new IntegerType(), TokenKind.JoyCount),

            // NAME$(port) - Nome del joystick sulla porta
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.JoyName, "NAME"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "port"), new IntegerType(),
                        SymbolFlags.None, null, false)
                ],
                new StringType(), TokenKind.JoyName),

            // AVAILABLE(port) - TRUE se un joystick è presente sulla porta
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.JoyPresent, "AVAILABLE"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "port"), new IntegerType(),
                        SymbolFlags.None, null, false)
                ],
                new BitType(), TokenKind.JoyPresent),
        };

        // Le costanti JOYUP/JOYDOWN/JOYLEFT/JOYRIGHT/JOYFIRE/JOYBUTTON2 si sono spostate come
        // %JOYUP ecc. in CreateConstantsNamespace() (StdLib.Constants.cs) — sempre disponibili
        // senza IMPORT JOY, che ora serve solo per le funzioni JOY/JOYCOUNT/JOYNAME/JOYPRESENT.
        RegisterBuiltinNamespace(namespaceName, procedures,
            ImmutableDictionary<string, DataType>.Empty);
    }
}
