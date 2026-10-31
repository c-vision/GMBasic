// FILE: Basic.Runtime.CommercialEditionRequiredException.cs
// Errore chiaro quando un comando commerciale viene eseguito in Community Edition (D11).

namespace Basic.Runtime;

public sealed class CommercialEditionRequiredException(string command)
    : Exception(
        $"'{command}' richiede la GMBASIC Commercial Edition. " +
        "Questa funzionalità (driver database proprietari: ISAM/DBF/SPEED/LEDGER) non è " +
        "disponibile nella Community Edition.")
{
}
