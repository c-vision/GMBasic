// FILE: AI.Tests.AiConfigTests
using Basic.AI;
using FluentAssertions;

namespace AI.Tests;

[Trait("Category", "Fast")]
public sealed class AiConfigTests : IDisposable
{
    // Bug 2026-08-28: questi test mutavano la config utente REALE (~/.gmbasic/ai.json) e non
    // erano crash-safe — un run ucciso lasciava il file inquinato e faceva fallire
    // SystemPromptDefaultAndOverride + AgentOperatingCenterTests. Ora ogni istanza usa un path
    // temporaneo univoco e non tocca MAI la config reale.
    private readonly string _configPath =
        Path.Combine(Path.GetTempPath(), "gmbasic-aiconfig-" + Guid.NewGuid().ToString("N") + ".json");

    public AiConfigTests()
    {
        AiConfig.OverridePathForTests(_configPath);
    }

    public void Dispose()
    {
        AiConfig.ResetForTests();
        if (File.Exists(_configPath)) File.Delete(_configPath);
    }

    [Fact(DisplayName = "AiConfig: add/remove model path")]
    public void AddRemoveModelPath()
    {
        var before = AiConfig.Load();
        try
        {
            AiConfig.AddModelPath("/tmp/gmb-test-models");
            AiConfig.ModelPaths.Should().Contain("/tmp/gmb-test-models");
            AiConfig.RemoveModelPath("/tmp/gmb-test-models");
            AiConfig.ModelPaths.Should().NotContain("/tmp/gmb-test-models");
        }
        finally
        {
            AiConfig.Save(before);
        }
    }

    [Fact(DisplayName = "AiConfig: add/remove skill path")]
    public void AddRemoveSkillPath()
    {
        var before = AiConfig.Load();
        try
        {
            AiConfig.AddSkillPath("/tmp/gmb-test-skills");
            AiConfig.SkillPaths.Should().Contain("/tmp/gmb-test-skills");
            AiConfig.RemoveSkillPath("/tmp/gmb-test-skills");
            AiConfig.SkillPaths.Should().NotContain("/tmp/gmb-test-skills");
        }
        finally
        {
            AiConfig.Save(before);
        }
    }

    [Fact(DisplayName = "AiConfig: default skill/index paths senza override")]
    public void DefaultPaths()
    {
        var before = AiConfig.Load();
        try
        {
            var oldDefault = AiConfig.SkillsDefaultPath;
            var oldIndex = AiConfig.SkillsIndexPath;
            AiConfig.SkillsDefaultPath.Should().NotBeNullOrWhiteSpace();
            AiConfig.SkillsIndexPath.Should().NotBeNullOrWhiteSpace();
        }
        finally
        {
            AiConfig.Save(before);
        }
    }
    [Fact(DisplayName = "AiConfig: system prompt default e override")]
    public void SystemPromptDefaultAndOverride()
    {
        var before = AiConfig.Load();
        try
        {
            AiConfig.SystemPrompt.Should().Contain("autonomous AI agent");
            AiConfig.SystemPrompt = "PROMPT TEST";
            AiConfig.SystemPrompt.Should().Be("PROMPT TEST");
            AiConfig.SystemPrompt = "";
            AiConfig.SystemPrompt.Should().Contain("autonomous AI agent");
        }
        finally
        {
            AiConfig.Save(before);
        }
    }
}
