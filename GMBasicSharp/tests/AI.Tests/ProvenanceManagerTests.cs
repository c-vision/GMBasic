// FILE: AI.Tests.ProvenanceManagerTests
// §2 Second Brain — ProvenanceCrypto: firma HMAC-SHA256 con chiave condivisa e verifica.
// Parità di output con il runtime C (gmb_provenance_sign/verify): la firma hex di
// "nota di test per provenance" con chiave "chiave-segreta-42" è nota e verificata
// contro il valore prodotto da gmb_hmac_sha256 C (f649104f...).
using Basic.AI.Knowledge;
using FluentAssertions;
using Xunit;

namespace AI.Tests;

[Trait("Category", "Fast")]
public sealed class ProvenanceManagerTests
{
    private const string Content = "nota di test per provenance";
    private const string Key = "chiave-segreta-42";
    // Valore di parità: calcolato dal test C (gmb_provenance_sign) e verificato contro
    // C# HMACSHA256 (stesso output). Se cambia per errore di implementazione, il test fallisce.
    private const string ExpectedSignature = "f649104f87bf898efce8eb903d6d27b207db27ac1d432393fda7d21af2f9c7e0";

    [Fact]
    public void Sign_ProduceKnownHex_MatchesCRuntime()
    {
        var sig = ProvenanceCrypto.Sign(Content, Key);
        sig.Should().Be(ExpectedSignature);
    }

    [Fact]
    public void Verify_CorrectSignature_ReturnsTrue()
    {
        ProvenanceCrypto.Verify(Content, Key, ExpectedSignature).Should().BeTrue();
    }

    [Fact]
    public void Verify_WrongContent_ReturnsFalse()
    {
        ProvenanceCrypto.Verify("altro contenuto", Key, ExpectedSignature).Should().BeFalse();
    }

    [Fact]
    public void Verify_WrongKey_ReturnsFalse()
    {
        ProvenanceCrypto.Verify(Content, "altra-chiave", ExpectedSignature).Should().BeFalse();
    }

    [Fact]
    public void Verify_EmptySignature_ReturnsFalse()
    {
        ProvenanceCrypto.Verify(Content, Key, "").Should().BeFalse();
    }

    [Fact]
    public void Verify_TamperedSignature_ReturnsFalse()
    {
        var sig = ProvenanceCrypto.Sign(Content, Key);
        // cambia un carattere al centro
        var tampered = sig[..10] + '0' + sig[11..];
        ProvenanceCrypto.Verify(Content, Key, tampered).Should().BeFalse();
    }

    [Fact]
    public void Sign_EmptyContent_Throws()
    {
        Assert.Throws<ArgumentException>(() => ProvenanceCrypto.Sign("", Key));
    }

    [Fact]
    public void Sign_EmptyKey_Throws()
    {
        Assert.Throws<ArgumentException>(() => ProvenanceCrypto.Sign(Content, ""));
    }

    // ── Share (F7 P2): AES-256-GCM round-trip ──

    [Fact]
    public void ShareEncryptDecrypt_RoundTripsContent()
    {
        var cipher = ProvenanceManager.ShareEncrypt(Content, Key);
        cipher.Should().NotBeNullOrWhiteSpace();
        // base64 contiene il prefisso salt|nonce|tag → almeno ~64 caratteri
        cipher.Length.Should().BeGreaterThan(60);
        ProvenanceManager.ShareDecrypt(cipher, Key).Should().Be(Content);
    }

    [Fact]
    public void ShareDecrypt_WrongKey_Throws()
    {
        var cipher = ProvenanceManager.ShareEncrypt(Content, Key);
        Assert.Throws<System.Security.Cryptography.AuthenticationTagMismatchException>(
            () => ProvenanceManager.ShareDecrypt(cipher, "altra-chiave"));
    }

    [Fact]
    public void ShareDecrypt_InvalidBase64_Throws()
    {
        Assert.Throws<FormatException>(() => ProvenanceManager.ShareDecrypt("@@@non-base64@@@", Key));
    }

    [Fact]
    public void ShareEncrypt_EmptyContent_Throws()
    {
        Assert.Throws<ArgumentException>(() => ProvenanceManager.ShareEncrypt("", Key));
    }
}
