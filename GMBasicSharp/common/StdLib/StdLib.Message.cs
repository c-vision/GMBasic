// FILE: Interpreter.StdLib.Message.cs
// Namespace MESSAGE — messaging generico (bus) + evento ON MESSAGE.
// Sintassi a spazio (come WINDOW): MESSAGE CONNECT|SUBSCRIBE|UNSUBSCRIBE|PUBLISH|PING|CLOSE.

/* ==== GMB-STDLIB-AUDIT-BEGIN ====
 * FILE: Message — namespace 'MESSAGE'
 * 1 funzioni | 1 comandi | 0 tipi | 0 costanti
 *
 * FUNZIONI (1):
 *   DELIVERY
 * COMANDI (1):
 *   MESSAGE
    ==== GMB-STDLIB-AUDIT-END ==== */

using System.Collections.Immutable;

namespace Basic;

public sealed partial class StdLib
{
    private void CreateMessageNamespace()
    {
        const string namespaceName = "MESSAGE";

        RegisterCommand(TokenKind.Message, "MESSAGE", namespaceName,
            (p, ns) => Parser.ParseMessageStatement(p, ns).First());

        // MESSAGE.DELIVERY(bus) — contatori di consegna del bus (sent/recv/ack/nack/dup/last).
        // È una FUNZIONE (legge, non scrive): ritorna "sent=S recv=R ack=A nack=N dup=D last=L"
        // (vedi gmb_msg.h / IlRuntime.MessageDelivery).
        // L'argomento è il bus, dichiarato a tipo inferito perché l'handle è NUMERICO su IL/C99Native
        // (MESSAGE CONNECT lo assegna) e un NOME su PCodeVM/VM-C (EmitMessageOp/HandleNameExpr), come
        // per PUBLISH/PING/CLOSE. Il backend risolve il proprio handle: parità di programma e di output.
        // Registrazione nella forma CANONICA (array di parametri inline `[ ... ]`, come le altre
        // procedure native): la forma con `ImmutableArray.Create(...)` faceva perdere la
        // SINTASSI al catalogo `command_desc.tsv` (il parser dei metadati riconosce `[ ... ]`),
        // quindi MESSAGE.DELIVERY finiva nel manuale senza parametri.
        var procedures = ImmutableArray.Create<ProcedureDefinition>(
            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.MessageDelivery, "DELIVERY"), // Contatori di consegna del bus (sent/recv/ack/nack/dup).
                [
                    new Parameter(-1, T(TokenKind.Identifier, "bus"), new InferredType(),
                        SymbolFlags.None, null, false)
                ],
                new StringType(), TokenKind.MessageDelivery, false));

        RegisterBuiltinNamespace(namespaceName, procedures,
            ImmutableDictionary<string, DataType>.Empty);
    }
}
