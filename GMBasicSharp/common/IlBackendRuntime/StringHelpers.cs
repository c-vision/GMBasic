// FILE: Basic.IlBackend.StringHelpers.cs

using System.Globalization;

namespace Basic.IlBackend;

/// <summary>
/// Wrapper senza <c>Nullable&lt;int&gt;</c> per <see cref="CommonLib.StringUtils.Mid"/>,
/// usati dal codice IL generato per MID$.
///
/// Un parametro <c>Nullable&lt;T&gt;</c> nella firma di un metodo esterno richiederebbe un
/// TypeSpec a <c>System.Nullable`1</c> nell'assembly corelib effettivo — a differenza dei
/// tipi scalari puri (int/string/double/ecc.), che usano la codifica CLI ELEMENT_TYPE
/// compatta e non soffrono del problema di identità di assembly documentato in
/// <see cref="IlTypeContext"/>, un generico istanziato come <c>Nullable&lt;Int32&gt;</c> non
/// ha questa scorciatoia: serve comunque un TypeRef risolvibile dal consumer. Evitarlo del
/// tutto con due overload separati (uno "a fine stringa", uno "con lunghezza esplicita") è
/// più semplice che risolvere <c>Nullable&lt;int&gt;</c> per ogni "mondo" di tipi come fatto
/// per gli scalari in <see cref="IlTypeContext"/>.
/// </summary>
public static class StringHelpers
{
    /// <summary>MID$(source, start) — estrae fino alla fine della stringa.</summary>
    public static string MidToEnd(string? source, int start) => CommonLib.StringUtils.Mid(source, start, null);

    /// <summary>MID$(source, start, length) — estrae length caratteri a partire da start.</summary>
    public static string MidWithLength(string? source, int start, int length) => CommonLib.StringUtils.Mid(source, start, length);

    /// <summary>
    /// PRINT di un DOUBLE — <c>Convert.ToString(double)</c> usa la culture CORRENTE del thread,
    /// quindi il separatore decimale cambia con la locale del sistema ospite (es. virgola in
    /// it-IT invece del punto). Un linguaggio BASIC deve stampare numeri in modo identico a
    /// prescindere dalla locale dell'host (stesso comportamento dell'Executor/PCodeVm
    /// esistenti) — da qui l'InvariantCulture esplicita invece del metodo Convert diretto.
    /// </summary>
    public static string DoubleToInvariantString(double value) => value.ToString("G15", CultureInfo.InvariantCulture);

    /// <summary>Stessa ragione di <see cref="DoubleToInvariantString"/>, per SINGLE.</summary>
    public static string SingleToInvariantString(float value) => value.ToString("G15", CultureInfo.InvariantCulture);

    /// <summary>Stessa ragione di <see cref="DoubleToInvariantString"/>, per CURRENCY.</summary>
    public static string DecimalToInvariantString(decimal value) => value.ToString(CultureInfo.InvariantCulture);

    public static long Asc(string? value) => string.IsNullOrEmpty(value) ? 0L : value[0];
    public static string Chr(long value)
    {
        if (value is < 0 or > 255) throw new ArgumentOutOfRangeException(nameof(value));
        return ((char)value).ToString();
    }
    public static string Str(double value) => CommonLib.StringUtils.Str(value);
    public static double Val(string? value) => CommonLib.ConversionUtils.Val(value);
    public static string CStr(object? value) => CommonLib.ConversionUtils.CStr(value);
    public static string Hex(long value) => CommonLib.ConversionUtils.Hex(value);
    public static string Oct(long value) => CommonLib.ConversionUtils.Oct(value);
    public static string Bin(long value) => CommonLib.ConversionUtils.Bin(value);
    public static bool IsNumeric(string? value) => CommonLib.Numerics.IsNumeric(value);
    public static string Space(long count) => new string(' ', Math.Max(0, checked((int)count)));
    public static string Dup(long count, string? value) => CommonLib.StringUtils.DuplicateFirst(checked((int)count), value);
    public static string Reverse(string? value) => new string((value ?? string.Empty).Reverse().ToArray());
    public static string Pascal(string? value) => CommonLib.StringUtils.Pascal(value);
    public static string Camel(string? value) => CommonLib.StringUtils.Camel(value);
    public static string Title(string? value) => CommonLib.StringUtils.Title(value);
    public static string Sentence(string? value) => CommonLib.StringUtils.Sentence(value);
    public static string Replace(string? value, string? oldValue, string? newValue) => (value ?? string.Empty).Replace(oldValue ?? string.Empty, newValue ?? string.Empty, StringComparison.Ordinal);
    public static string Remove(string? value, string? substring) => (value ?? string.Empty).Replace(substring ?? string.Empty, string.Empty, StringComparison.Ordinal);
    public static bool Contains(string? value, string? search) => (value ?? string.Empty).Contains(search ?? string.Empty, StringComparison.Ordinal);
    public static bool StartsWith(string? value, string? prefix) => (value ?? string.Empty).StartsWith(prefix ?? string.Empty, StringComparison.OrdinalIgnoreCase);
    public static bool EndsWith(string? value, string? suffix) => (value ?? string.Empty).EndsWith(suffix ?? string.Empty, StringComparison.OrdinalIgnoreCase);
    public static string Snake(string? value) => ToSeparated(value, '_');
    public static string Kebab(string? value) => ToSeparated(value, '-');
    public static string Repeat(string? value, long count, string? separator) => CommonLib.StringUtils.Repeat(value, checked((int)count), separator);
    public static string PadL(string? value, long length, string? pad) => (value ?? string.Empty).PadLeft(checked((int)length), FirstPad(pad));
    public static string PadR(string? value, long length, string? pad) => (value ?? string.Empty).PadRight(checked((int)length), FirstPad(pad));
    public static string PadC(string? value, long length, string? pad) => CommonLib.StringUtils.PadCenter(value, checked((int)length), FirstPad(pad));
    public static string Wrap(string? value, long width, long alignment) => CommonLib.StringUtils.Wrap(value, checked((int)width), checked((int)alignment));
    public static bool Search(string? value, string? pattern) => (value ?? string.Empty).Contains(pattern ?? string.Empty, StringComparison.OrdinalIgnoreCase);
    public static string[] Split(string? value, string? delimiter) => (value ?? string.Empty).Split([delimiter ?? string.Empty], StringSplitOptions.None);
    public static string Join(object? values, string? delimiter) => values is System.Collections.IEnumerable enumerable and not string
        ? string.Join(delimiter ?? string.Empty, enumerable.Cast<object?>().Select(value => value?.ToString() ?? string.Empty))
        : throw new ArgumentException("JOIN richiede un array o una collezione.", nameof(values));
    public static string Format(object? value, string? format) => CommonLib.FormatUtils.FormatValue(value, format ?? string.Empty);
    public static long Count(string? value, string? substring) => CommonLib.StringUtils.Count(value, substring);
    private static char FirstPad(string? value) => string.IsNullOrEmpty(value) ? ' ' : value[0];
    private static string ToSeparated(string? value, char separator)
    {
        var source = value ?? string.Empty;
        return string.Concat(source.Select((c, i) => i > 0 && char.IsUpper(c) && source[i - 1] != separator ? $"{separator}{char.ToLowerInvariant(c)}" : char.ToLowerInvariant(c).ToString()));
    }
}
