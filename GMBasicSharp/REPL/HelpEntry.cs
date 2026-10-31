using System.Text.Json.Serialization;

namespace Basic.Executor;

public class HelpEntry
{
    [JsonPropertyName("name")]
    public string Name { get; set; } = "";

    [JsonPropertyName("namespace")]
    public string Namespace { get; set; } = "";

    [JsonPropertyName("category")]
    public string Category { get; set; } = "";

    [JsonPropertyName("description")]
    public string Description { get; set; } = "";

    [JsonPropertyName("syntax")]
    public List<string> Syntax { get; set; } = [];

    [JsonPropertyName("returnType")]
    public string ReturnType { get; set; } = "";

    [JsonPropertyName("parameters")]
    public List<HelpParam> Parameters { get; set; } = [];

    [JsonPropertyName("examples")]
    public List<HelpExample> Examples { get; set; } = [];

    [JsonPropertyName("seeAlso")]
    public List<string> SeeAlso { get; set; } = [];

    [JsonPropertyName("isCommand")]
    public bool IsCommand { get; set; }

    [JsonIgnore]
    public string FullName => string.IsNullOrEmpty(Namespace) ? Name : $"{Namespace}.{Name}";

    [JsonIgnore]
    public string SearchText => $"{Name} {Namespace} {Category} {Description}".ToLowerInvariant();
}

public class HelpParam
{
    [JsonPropertyName("name")]
    public string Name { get; set; } = "";

    [JsonPropertyName("type")]
    public string Type { get; set; } = "";

    [JsonPropertyName("description")]
    public string Description { get; set; } = "";
}

public class HelpExample
{
    [JsonPropertyName("description")]
    public string Description { get; set; } = "";

    [JsonPropertyName("code")]
    public string Code { get; set; } = "";

    [JsonPropertyName("expectedOutput")]
    public string ExpectedOutput { get; set; } = "";
}
