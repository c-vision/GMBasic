namespace Basic;

/// <summary>Modalità di selezione del percorso GPM vs lineare nel backend C99.</summary>
public enum C99GpmMode
{
    /// <summary>Rilevamento automatico: GPM ⇔ il programma usa costrutti di concorrenza.</summary>
    Auto = 0,

    /// <summary>GPM sempre (default): rete di sicurezza contro i bug di detection —
    /// anche se la detection manca un GO SUB annidato, gira comunque sullo scheduler.</summary>
    Always = 1,

    /// <summary>Preferisce il percorso lineare (ottimizzazione) ma SOLO se il programma è
    /// genuinamente lineare; se contiene concorrenza ripiega su Auto (mai crash).</summary>
    Linear = 2,
}
