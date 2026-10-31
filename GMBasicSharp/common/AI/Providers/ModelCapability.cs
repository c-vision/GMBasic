// AI/Providers/ModelCapability.cs

namespace Basic.Providers;

[Flags]
public enum ModelCapability
{
    Text = 1 << 0,
    Vision = 1 << 1,
    Audio = 1 << 2,
    FunctionCalling = 1 << 3,
}
