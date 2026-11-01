// FILE: Compiler.SizeOf.cs
// Fold a compile-time di SYSTEM.SIZEOF: la chiamata alla builtin viene sostituita dalla
// costante della dimensione in byte, risolta dall'unica sorgente del canone (TypeLayout).

namespace Compiler;

public sealed partial class Compiler
{
    /// <summary>
    /// Dimensione in byte del tipo STATICO dell'argomento di SIZEOF (canone in
    /// <see cref="TypeLayout"/>): il fold è TOTALE — ogni argomento produce la costante, così
    /// PCodeVM e VM/C leggono lo stesso numero di IL/C99Native e il runtime non può
    /// divergere (vedrebbe solo valori: ogni numerico è un double, quindi INTEGER sarebbe 8).
    /// Eccezione: il NOME di un TYPE, che resta al runtime perché è il descrittore della classe
    /// (chiave "Size") a portare il layout, non l'inferimento del compilatore.
    /// </summary>
    private bool TryStaticSizeOf(Expr arg, out long bytes)
    {
        // `SIZEOF((x))`: le parentesi sono un GroupingExpr ma la FORMA resta una variabile →
        // vale il suo tipo statico (canone TypeLayout; C99Native rispondeva già così).
        while (arg is GroupingExpr grp) arg = grp.Expression;
        // Letterale numerico = il double di default del linguaggio; ogni altro letterale
        // (stringa, booleano, nil) non ha tipo statico dichiarato → -1.
        if (arg is not VariableExpr ve)
        {
            bytes = arg is NumberExpr ? TypeLayout.ScalarByteSize(new DoubleType()) : TypeLayout.NoStaticSize;
            return true;
        }
        var name = ve.NameCleaned.Lexeme;
        // KEYWORD DI TIPO SCALARE prima di tutto (SIZEOF(INTEGER) = 2, SIZEOF(STRING) = -1): un
        // nome che è keyword non può essere una variabile dichiarata, e l'inferimento gli
        // attribuirebbe il double di default (8), divergente dalle gambe C.
        if (TypeLayout.TrySizeOfTypeName(name, out bytes)) return true;
        // Nome di TYPE: resta al runtime, che spinge il descrittore con la chiave "Size".
        if (IsUserTypeName(name)) return false;
        // KEYWORD CHE NON È NOME DI TIPO NÉ TYPE UTENTE = FUNZIONE/COSTANTE built-in a zero
        // argomenti (DATE, TIME, NOW, PI, LEN, FREEFILE, …): il simbolo del linker porta il suo
        // tipo di RITORNO (DateType, DoubleType, …), che NON è il tipo di una variabile e la cui
        // larghezza NON è misurabile — le gambe C non hanno nessuna tabella dei ritorni dei
        // builtin, quindi risponderebbero -1 (canone regola 5; in C99Native lo stesso test è sul
        // TokenKind in gmb_sizeof_arg). Un TYPE utente che collide con una keyword (es.
        // <c>TYPE Empty</c>) è già escluso sopra; una variabile DICHIARATA non può avere nome di
        // keyword, quindi qui il test non può mai nascondere una DIM reale.
        if (TypeLayout.IsBuiltinFunctionKeyword(ve.NameCleaned.Kind))
        {
            bytes = TypeLayout.NoStaticSize;
            return true;
        }
        // Variabile (DIM esplicita, parametro, campo, implicita auto-dichiarata): la sorgente è
        // il SIMBOLO del linker — la stessa che legge IL (InferVariableType) — perché il bytecode
        // locale registra ogni slot come double e risponderebbe 8 a una stringa (divergenza
        // misurata 2026-09-29 su SIZEOF di una variabile a cui è assegnata una stringa). Un
        // simbolo senza tipo (Any/Inferred, o non risolto) è l'implicita numerica: il DOUBLE di
        // default del linguaggio.
        var sym = SymbolDataType(ve);
        bytes = sym is null or AnyType or InferredType
            ? TypeLayout.ScalarByteSize(new DoubleType())
            : TypeLayout.ByteSize(sym);
        return true;
    }

    /// <summary>Tipo del simbolo del linker per un identificatore, o null se non risolto.</summary>
    private DataType? SymbolDataType(VariableExpr ve)
    {
        if (ve.SymbolId < 0 || _ctx is not BasicProgram bp) return null;
        return bp.Symbols.GetSymbol(ve.SymbolId).DataType;
    }

    private bool IsUserTypeName(string name)
        => !string.IsNullOrEmpty(name) && _ctx.GetAllUserDefinedTypes().Any(u =>
            string.Equals(u.OriginalName.Lexeme, name, StringComparison.OrdinalIgnoreCase));
}
