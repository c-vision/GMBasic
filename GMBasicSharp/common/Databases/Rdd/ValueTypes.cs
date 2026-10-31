// FILE: Basic.Databases.Rdd.ValueTypes.cs
// Value types per P-CODE: Date, Currency — evitano boxing.
// Usati da SchemaBuffer per storage nativo senza object?.

using System;
using System.Globalization;

namespace Basic.Databases.Rdd;

/// <summary>Tipo Data GM-Basic: giorno/mese/anno con aritmetica date.</summary>
public readonly struct GbDate : IComparable<GbDate>, IEquatable<GbDate>
{
    public readonly DateTime Value;

    public GbDate(DateTime dt) => Value = dt.Date;
    public GbDate(int year, int month, int day) => Value = new DateTime(year, month, day);
    public GbDate(double julianOrOa)
    {
        if (julianOrOa > 100000) // OLE Automation date
            Value = DateTime.FromOADate(julianOrOa);
        else if (julianOrOa > 1000) // Julian day number
            Value = new DateTime(1900, 1, 1).AddDays(julianOrOa - 2415019);
        else
            Value = DateTime.MinValue;
    }

    public int Year => Value.Year;
    public int Month => Value.Month;
    public int Day => Value.Day;
    public double ToOADate() => Value.ToOADate();
    public static GbDate Today => new(DateTime.Today);

    public static GbDate operator +(GbDate d, int days) => new(d.Value.AddDays(days));
    public static GbDate operator -(GbDate d, int days) => new(d.Value.AddDays(-days));
    public static int operator -(GbDate a, GbDate b) => (int)(a.Value - b.Value).TotalDays;

    public int CompareTo(GbDate other) => Value.CompareTo(other.Value);
    public bool Equals(GbDate other) => Value.Equals(other.Value);
    public override bool Equals(object? obj) => obj is GbDate d && Equals(d);
    public override int GetHashCode() => Value.GetHashCode();
    public override string ToString() => Value.ToString("yyyy-MM-dd");

    public static implicit operator DateTime(GbDate d) => d.Value;
    public static explicit operator GbDate(DateTime dt) => new(dt);
    public static GbDate FromLiteral(string s)
    {
        if (DateTime.TryParse(s, CultureInfo.InvariantCulture, DateTimeStyles.None, out var dt))
            return new GbDate(dt);
        return new GbDate(DateTime.MinValue);
    }
}

/// <summary>Tipo Currency GM-Basic: fixed-point 4 decimali, 8 byte.</summary>
public readonly struct GbCurrency : IComparable<GbCurrency>, IEquatable<GbCurrency>
{
    private readonly long _raw; // scaled by 10000

    public GbCurrency(decimal value) => _raw = (long)(value * 10000m);
    public GbCurrency(double value) => _raw = (long)(value * 10000);
    public GbCurrency(long raw) => _raw = raw;

    public decimal ToDecimal() => _raw / 10000m;
    public double ToDouble() => (double)_raw / 10000;
    public long RawValue => _raw;

    public static GbCurrency operator +(GbCurrency a, GbCurrency b) => new(a._raw + b._raw);
    public static GbCurrency operator -(GbCurrency a, GbCurrency b) => new(a._raw - b._raw);
    public static GbCurrency operator *(GbCurrency a, GbCurrency b) => new((long)((a._raw * (decimal)b._raw) / 10000m));
    public static GbCurrency operator /(GbCurrency a, GbCurrency b) => b._raw != 0 ? new((long)(a._raw * 10000m / b._raw)) : new(0L);
    public static GbCurrency operator -(GbCurrency a) => new(-a._raw);

    public int CompareTo(GbCurrency other) => _raw.CompareTo(other._raw);
    public bool Equals(GbCurrency other) => _raw == other._raw;
    public override bool Equals(object? obj) => obj is GbCurrency c && Equals(c);
    public override int GetHashCode() => _raw.GetHashCode();
    public override string ToString() => ToDecimal().ToString("F4");

    public static GbCurrency FromDouble(double d) => new(d);
    public static GbCurrency FromString(string s)
    {
        if (decimal.TryParse(s, NumberStyles.Currency, CultureInfo.InvariantCulture, out var d))
            return new GbCurrency(d);
        return new GbCurrency(0m);
    }
}
