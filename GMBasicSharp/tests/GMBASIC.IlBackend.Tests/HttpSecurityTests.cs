// HTTP CONFIG security (2026-09-02): helper condiviso IP allow/block (CIDR IPv4) + header
// di sicurezza. Unit test puri di CommonLib.Http.HttpSecurity.
using Xunit;
using CommonLib.Http;

namespace Basic.IlBackend.Tests;

public class HttpSecurityTests
{
    [Fact]
    public void DefaultHeaders_Contain_Security_Set()
    {
        var h = HttpSecurity.DefaultSecurityHeaders();
        Assert.Contains("Strict-Transport-Security", h.Keys);
        Assert.Contains("Content-Security-Policy", h.Keys);
        Assert.Contains("X-Frame-Options", h.Keys);
        Assert.Contains("X-Content-Type-Options", h.Keys);
        Assert.Contains("Referrer-Policy", h.Keys);
        Assert.Contains("X-Permitted-Cross-Domain-Policies", h.Keys);
    }

    [Fact]
    public void NoLists_Allow_All()
    {
        Assert.True(HttpSecurity.IpAllowed("192.168.1.50", "", ""));
    }

    [Fact]
    public void BlockList_SingleIp_Forbids()
    {
        Assert.False(HttpSecurity.IpAllowed("10.0.0.7", "", "10.0.0.7"));
        Assert.True(HttpSecurity.IpAllowed("10.0.0.8", "", "10.0.0.7"));
    }

    [Fact]
    public void BlockList_Cidr_Forbids()
    {
        Assert.False(HttpSecurity.IpAllowed("10.0.0.200", "", "10.0.0.0/24"));
        Assert.True(HttpSecurity.IpAllowed("10.0.1.5", "", "10.0.0.0/24"));
    }

    [Fact]
    public void AllowList_Restricts()
    {
        Assert.True(HttpSecurity.IpAllowed("127.0.0.1", "127.0.0.1, 10.0.0.0/8", ""));
        Assert.False(HttpSecurity.IpAllowed("192.168.0.10", "127.0.0.1, 10.0.0.0/8", ""));
    }

    [Fact]
    public void BlockOverrides_Allow()
    {
        Assert.False(HttpSecurity.IpAllowed("10.1.2.3", "10.0.0.0/8", "10.1.2.3"));
    }
}
