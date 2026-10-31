// FILE: AI.Tests.PiiRedactorTests
using Basic.AI;
using FluentAssertions;

namespace AI.Tests;

[Trait("Category", "Fast")]
public sealed class PiiRedactorTests
{
    [Fact(DisplayName = "PII redaction: email, telefono, CF, P.IVA, carta, IBAN")]
    public void Redact_MasksCommonPii()
    {
        var text = "Contatto mario.rossi@example.com tel +39 345 1234567 "
                 + "CF RSSMRA80A01H501U P.IVA IT12345678901 carta 4111 1111 1111 1111 "
                 + "IBAN IT60X0542811101000000123456 data 12/05/1990";
        var redacted = PiiRedactor.Redact(text);

        redacted.Should().Contain("[EMAIL]");
        redacted.Should().Contain("[PHONE]");
        redacted.Should().Contain("[TAX_ID]");
        redacted.Should().Contain("[VAT_ID]");
        redacted.Should().Contain("[CARD]");
        redacted.Should().Contain("[IBAN]");
        redacted.Should().Contain("[DATE]");
        redacted.Should().NotContain("mario.rossi");
        redacted.Should().NotContain("345 1234567");
        redacted.Should().NotContain("RSSMRA80A01H501U");
        redacted.Should().NotContain("4111 1111 1111 1111");
    }

    [Fact(DisplayName = "PII redaction: testo senza PII resta invariato")]
    public void Redact_LeavesCleanTextUntouched()
    {
        var text = "Il contratto scade il 31 dicembre 2026 e prevede un indennizzo.";
        PiiRedactor.Redact(text).Should().Be(text);
    }

    [Fact(DisplayName = "PII redaction: input null o vuoto")]
    public void Redact_HandlesNullAndEmpty()
    {
        PiiRedactor.Redact(null).Should().Be("");
        PiiRedactor.Redact("").Should().Be("");
        PiiRedactor.Redact("   ").Should().Be("   ");
    }
}
