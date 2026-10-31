// AI/Providers/KnownProviders.cs

namespace Basic.Providers;

public static class KnownProviders
{
    public static readonly Dictionary<string, (string BaseUrl, Dictionary<string, string> Headers)> Map = new()
    {
        ["openrouter"] = ("https://openrouter.ai/api/v1", new() { ["httpReferer"] = "https://gmbasic.local", ["xTitle"] = "GMBasic" }),
        ["deepseek"] = ("https://api.deepseek.com/v1", []),
        ["groq"] = ("https://api.groq.com/openai/v1", []),
        ["mistral"] = ("https://api.mistral.ai/v1", []),
        ["qwen"] = ("https://dashscope-intl.aliyuncs.com/compatible-mode/v1", []),
        ["cerebras"] = ("https://api.cerebras.ai/v1", []),
        ["nvidia"] = ("https://integrate.api.nvidia.com/v1", []),
        ["together"] = ("https://api.together.xyz/v1", []),
        ["fireworks"] = ("https://api.fireworks.ai/inference/v1", []),
        ["mimo"] = ("https://api.xiaomimimo.com/v1", []),
        ["metis"] = ("https://api.metisrouter.com/v1", []),
        ["ollama-cloud"] = ("https://ollama.com/v1", []),
        ["perplexity"] = ("https://api.perplexity.ai", []),
        ["xai"] = ("https://api.x.ai/v1", []),
    };
}
