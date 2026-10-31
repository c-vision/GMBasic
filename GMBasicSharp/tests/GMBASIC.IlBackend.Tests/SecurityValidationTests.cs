// Validation core (Security D, 2026-09-02): parità C#/C. Vettori condivisi con l'harness C.
using Xunit;
using CommonLib;

namespace Basic.IlBackend.Tests;

public class SecurityValidationTests
{
    [Theory]
    [InlineData(null, false)]
    [InlineData("", false)]
    [InlineData("   ", false)]
    [InlineData("abc", true)]
    public void Required(string? v, bool expected) => Assert.Equal(expected, SecurityValidation.IsRequired(v));

    [Theory]
    [InlineData("a@b.co", true)]
    [InlineData("a.b@c.d.e", true)]
    [InlineData("a@b", false)]
    [InlineData("a b@c.co", false)]
    [InlineData("@x.co", false)]
    [InlineData("a@.co", false)]
    public void Email(string? v, bool expected) => Assert.Equal(expected, SecurityValidation.IsEmail(v));

    [Theory]
    [InlineData("https://example.com", true)]
    [InlineData("http://x", false)]
    [InlineData("ftp://a.b", false)]
    [InlineData("https://a b.com", false)]
    public void Url(string? v, bool expected) => Assert.Equal(expected, SecurityValidation.IsUrl(v));

    [Theory]
    [InlineData("4111111111111111", true)]
    [InlineData("1234567890123456", false)]
    [InlineData("4111 1111 1111 1111", true)]
    [InlineData("abc", false)]
    public void CreditCard(string? v, bool expected) => Assert.Equal(expected, SecurityValidation.IsCreditCard(v));

    [Theory]
    [InlineData("abc", 2, 5, true)]
    [InlineData("abc", 4, -1, false)]
    [InlineData("abcdef", 1, 3, false)]
    public void Length(string? v, int min, int max, bool expected)
        => Assert.Equal(expected, SecurityValidation.IsLengthBetween(v, min, max));
}
