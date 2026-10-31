// FILE: Basic.Providers.ProviderStatus
namespace Basic.Providers;

/// <summary>Operational status of a registered provider.</summary>
/// <remarks>
/// ponytail: Available/Unavailable are toggled automatically (background health check,
/// live HTTP call outcome). Blocked/Damaged are manual overrides set by the BASIC
/// programmer and are never touched by automatic health reporting — only an explicit
/// PROVIDER.SETSTATUS call can change them.
/// </remarks>
public enum ProviderStatus
{
    Available,
    Unavailable,
    Blocked,
    Damaged
}

public static class ProviderStatusExtensions
{
    public static ProviderStatus ParseStatus(string value)
    {
        if (Enum.TryParse<ProviderStatus>(value, ignoreCase: true, out var status))
            return status;
        throw new ArgumentException(
            $"Unknown provider status '{value}'. Expected AVAILABLE, UNAVAILABLE, BLOCKED or DAMAGED.",
            nameof(value));
    }
}
