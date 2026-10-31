// FILE: Mcp.Tests.McpHttpSecurityTests
using Basic.Mcp;
using FluentAssertions;
using Xunit;

namespace AI.Tests;

[Trait("Category", "Fast")]
public sealed class McpHttpSecurityTests
{
    [Fact]
    public void TryValidateApiKeyHeaders_AcceptsBearerAndXApiKey()
    {
        var security = new McpHttpSecurity(new McpHttpOptions { ApiKey = "secret-key" });

        security.TryValidateApiKeyHeaders("Bearer secret-key", null, out var bearerFailure).Should().BeTrue();
        bearerFailure.Should().BeNull();

        security.TryValidateApiKeyHeaders(null, "secret-key", out var headerFailure).Should().BeTrue();
        headerFailure.Should().BeNull();
    }

    [Fact]
    public void TryValidateApiKeyHeaders_RejectsMissingOrInvalidKey()
    {
        var security = new McpHttpSecurity(new McpHttpOptions { ApiKey = "secret-key" });

        security.TryValidateApiKeyHeaders(null, null, out var missingReason).Should().BeFalse();
        missingReason.Should().Be("missing_api_key");

        security.TryValidateApiKeyHeaders("Bearer wrong", null, out var invalidReason).Should().BeFalse();
        invalidReason.Should().Be("invalid_api_key");
    }

    [Fact]
    public void TryConsumeRateLimit_BlocksAfterThreshold()
    {
        var security = new McpHttpSecurity(new McpHttpOptions { RequestsPerMinutePerIp = 2 });
        security.TryConsumeRateLimit("127.0.0.1", out _).Should().BeTrue();
        security.TryConsumeRateLimit("127.0.0.1", out _).Should().BeTrue();
        security.TryConsumeRateLimit("127.0.0.1", out var reason).Should().BeFalse();
        reason.Should().Be("rate_limit_exceeded");
    }
}
