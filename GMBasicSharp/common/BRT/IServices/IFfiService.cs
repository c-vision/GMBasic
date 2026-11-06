// FILE: Basic.Runtime.IServices.IFfiService.cs

namespace Basic.Runtime;

/// <summary>Struct/UDT per valore, incluse STRUTTURE ANNIDATE (2026-09-28, esteso al nesting
/// 2026-09-29): la "forma" di un UDT semplice usato in una firma FFI, dopo il FLATTENING
/// ricorsivo delle sue foglie scalari — vedi AiWiki Bugs/(solved) - FFI-Struct-Annidate-
/// Bloccato-Da-Bug-UDT-Nested.md. <paramref name="FieldNames"/> = path punteggiato per FOGLIA
/// in ordine di dichiarazione ("X" per un campo diretto, "I.X" per un campo annidato — per
/// leggere un'istanza PCodeVM, un <c>Dictionary&lt;string,object?&gt;</c> chiavato per nome,
/// navigato ricorsivamente splittando il path su '.').
/// <paramref name="FieldSymbolIdPaths"/> = SymbolId compile-time di OGNI SEGMENTO del path
/// (non solo della foglia), stesso ordine di <paramref name="FieldNames"/> — per costruire
/// un'istanza in USCITA leggibile da OP_LOAD_MEMBER: un livello intermedio ricostruito dal
/// marshalling non ha un vero __class__/FieldNames, quindi ogni livello usa il fallback
/// "__field_&lt;SymbolId&gt;" (già usato altrove nel motore — vedi
/// Compiler.UserTypes.cs/PCodeProcessor.Helpers.cs PCodeFieldNameFromOffset).</summary>
/// <summary><paramref name="FieldIsInt"/> (2026-09-29, kind misto): kind PER FOGLIA (true =
/// intero, false = DOUBLE) — per un parametro è sempre uniforme (kind unico dell'intera
/// dichiarazione, garantito dal guard di compilazione), per un RITORNO può variare campo per
/// campo (un UDT-ritorno a kind misto è ammesso, vedi Compiler.Expressions.cs
/// ValidateUdtForFfi).</summary>
public sealed record FfiUdtShape(
    string[] FieldNames,
    int[][] FieldSymbolIdPaths,
    bool[] FieldIsInt,
    byte[] FieldTypes = null!,
    int[] FieldOffsets = null!,
    int Size = 0);

/// <summary>Servizio FFI (Foreign Function Interface) per il BASIC Runtime.</summary>
public interface IFfiService
{
    IntPtr LoadLibrary(string path);
    void FreeLibrary(IntPtr handle);
    IntPtr GetFunction(IntPtr library, string name);
    object? CallFunction(IntPtr function, object?[] args);
    /// <summary>Chiamata tipizzata (OP_CALL_FFI_T): kind omogeneo — 1 = int64 (LONG), 0 = double
    /// — per tutti i parametri e il ritorno, così l'ABI corrisponde al simbolo C exportato.</summary>
    object? CallFunctionTyped(IntPtr function, object?[] args, byte kind);
    /// <summary>Come <see cref="CallFunctionTyped"/>, ma per firme con almeno un parametro
    /// array BYREF (2026-09-28): <paramref name="arrayMask"/> ha il bit i settato se
    /// <c>args[i]</c> è un array (CLR <c>double[]</c>/<c>long[]</c>, oppure <c>object[]</c>
    /// boxato per PCodeVM) da marshallare come puntatore nativo, invece di uno scalare
    /// <c>kind</c>-omogeneo. <paramref name="arrayElemIntMask"/> ha il bit i settato se
    /// l'array al parametro i ha elementi INTERI (AS LONG/...) invece di DOUBLE — necessario
    /// per il caso <c>object[]</c> (i cui elementi non preservano il tipo dichiarato a
    /// runtime in modo affidabile); ignorato per array CLR già tipizzati (double[]/long[]).
    /// Gli argomenti con bit a 0 in arrayMask restano scalari <c>kind</c>-omogenei come in
    /// <see cref="CallFunctionTyped"/>. <paramref name="stringMask"/> (2026-09-28, supporto
    /// STRING) ha il bit i settato se <c>args[i]</c> è una <c>string</c> CLR da marshallare
    /// come puntatore nativo (UTF-8, valido solo durante la chiamata). <paramref
    /// name="retIsString"/> indica che il ritorno è STRING: il valore int64 della chiamata
    /// nativa è in realtà un puntatore, copiato (copy-out UTF-8) in una nuova <c>string</c>
    /// CLR prima di tornare — il metodo ritorna quella <c>string</c> invece di un numero.
    /// <para><paramref name="byrefScalarMask"/> (2026-09-28, BYREF scalare) ha il bit i settato
    /// se il parametro i è BYREF scalare: <c>args[i]</c> contiene il valore IN, il delegate
    /// dinamico usa un parametro CLR <c>ref</c> (marshalling nativo di .NET per byref
    /// blittable), e DOPO la chiamata il chiamante DEVE rileggere <c>args[i]</c> — questo
    /// metodo lo sovrascrive IN PLACE nell'array passato con il valore OUT. <paramref
    /// name="byrefScalarElemIntMask"/> indica se quel valore è intero (AS LONG/QUAD/...)
    /// invece di DOUBLE, stesso ruolo di <paramref name="arrayElemIntMask"/> per gli array.
    /// Un parametro BYREF scalare è ammesso solo per tipi a 8 byte (stessa restrizione degli
    /// elementi-array, §4bis della guida FFI) — nessun rischio di mismatch di larghezza.</para>
    /// <para>Struct/UDT per valore (2026-09-28): <paramref name="udtParams"/>[i] non-null
    /// indica che il parametro logico i è un UDT semplice passato per VALORE — <c>args[i]</c>
    /// è l'istanza PCodeVM (<c>Dictionary&lt;string,object?&gt;</c>). Il delegate dinamico usa
    /// un vero VALUE TYPE CLR costruito a runtime (campi <c>kind</c>-omogenei, layout
    /// sequenziale) — .NET marshalla il passaggio per valore da solo (stessa ABI di un
    /// parametro struct P/Invoke nativo). <paramref name="udtReturn"/> non-null indica che il
    /// RITORNO è un UDT: il risultato è una NUOVA istanza PCodeVM (Dictionary con __class__/
    /// FieldNames costruito da <see cref="FfiUdtShape.FieldSymbolIds"/>, leggibile da un
    /// successivo accesso a campo).</para>
    /// <para>IL backend (2026-09-29): a differenza di PCodeVM (istanza UDT = Dictionary
    /// dinamico, serve costruire un VALUE TYPE ombra a runtime via <paramref name="udtParams"/>/
    /// <paramref name="udtReturn"/>), il backend IL compila già ogni UDT semplice in un VERO
    /// value type CLR a COMPILE TIME (<c>TypeBuilder</c>, vedi IlProgramCompiler.Host.Compile.cs)
    /// — <c>args[i]</c> è già un'istanza BOXED di quel tipo esatto (stesso layout sequenziale
    /// kind-omogeneo richiesto dall'ABI nativa). <paramref name="udtParamClrTypes"/>[i] non-null
    /// comunica quel tipo REALE direttamente: nessuna copia campo-per-campo serve, l'istanza
    /// boxed passa inalterata come argomento del delegate dinamico. <paramref
    /// name="udtReturnClrType"/> non-null: il RITORNO è un UDT IL — il risultato torna COSÌ
    /// COM'È (l'istanza boxed del value type reale), non una Dictionary — il chiamante (IL
    /// generato da EmitFfiCall) la unboxa con <c>Unbox_Any</c> sul tipo noto a compile-time.</para></summary>
    /// <param name="scalarKindMask">Firma mista LONG/DOUBLE fra scalari semplici (2026-10,
    /// v1): bit i settato = il parametro SCALARE SEMPLICE i (non array/string/byref/udt, già
    /// classificato dalle altre maschere) è Int64, 0 = Double — invece del <paramref
    /// name="kind"/> uniforme usato finora per quegli slot. <paramref name="kind"/> resta il
    /// kind del RITORNO (quando non STRING/UDT). Ammesso solo quando la firma non ha nessun
    /// array/STRING/BYREF/UDT (garantito dal guard di compilazione, Compiler.Expressions.cs/
    /// IlProgramCompiler.Expressions.cs) — per quelle posizioni il bit corrispondente non è
    /// significativo. Default 0 = comportamento legacy (tutti gli scalari semplici seguono
    /// <paramref name="kind"/>, nessun cambio per le dichiarazioni esistenti).</param>
    object? CallFunctionTypedArr(IntPtr function, object?[] args, int kind, long arrayMask, long arrayElemIntMask,
        long stringMask, bool retIsString, long byrefScalarMask = 0, long byrefScalarElemIntMask = 0,
        FfiUdtShape?[]? udtParams = null, FfiUdtShape? udtReturn = null,
        Type?[]? udtParamClrTypes = null, Type? udtReturnClrType = null, long scalarKindMask = 0,
        string[]? udtShapeStrs = null, string? udtReturnShapeStr = null);
}
