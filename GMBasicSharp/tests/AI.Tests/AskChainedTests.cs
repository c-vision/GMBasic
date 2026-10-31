using Basic.Agents;
using Basic.AI.Rag;
using FluentAssertions;
using Xunit;

namespace AI.Tests;

[Trait("Category", "Fast")]
public sealed class AskChainedTests : IDisposable
{
    public AskChainedTests()
    {
        AgentServer.ResetForTests();
    }

    public void Dispose()
    {
        AgentServer.ResetForTests();
    }

    [Fact]
    public void AskChained_StepsCarryArtifactsViaBlackboard()
    {
        AgentServer.Instance.Create("Chain");
        var agent = AgentServer.Instance.List().First(a => a.Name == "Chain");
        var prefix = "chain_" + Guid.NewGuid().ToString("N")[..6];
        var stub = new SeqCompleter("fase-1", "fase-2", "fase-3");

        var answer = AgentServer.Instance.AskChainedCore(agent, stub,
            ["Analizza il problema", "Sintetizza la soluzione", "Concludi"], prefix);

        answer.Should().Be("fase-3");
        // Ogni artefatto di step è stato scritto sul Blackboard.
        Basic.Agents.Blackboard.Read(prefix + "_s1").Should().Be("fase-1");
        Basic.Agents.Blackboard.Read(prefix + "_s2").Should().Be("fase-2");
        Basic.Agents.Blackboard.Read(prefix + "_s3").Should().Be("fase-3");
        // Lo stato cumulativo a fine catena contiene tutti gli artefatti.
        var state = Basic.Agents.Blackboard.Read(prefix + "_state");
        state.Should().Contain("fase-1").And.Contain("fase-2");
        // Il contesto dello step successivo "riassume" lo stato dalla blackboard (niente
        // transcript pieno): prompt[1] contiene l'artefatto dello step 1, prompt[0] no.
        stub.Prompts[1].Should().Contain("fase-1");
        stub.Prompts[2].Should().Contain("fase-1\nfase-2");
        stub.Prompts[0].Should().NotContain("fase-");
    }

    [Fact]
    public void AskChainedStep_SequentialCallsAccumulateState()
    {
        AgentServer.Instance.Create("ChainStep");
        var agent = AgentServer.Instance.List().First(a => a.Name == "ChainStep");
        var prefix = "chainstep_" + Guid.NewGuid().ToString("N")[..6];
        var stub = new SeqCompleter("passo-1", "passo-2");

        AgentServer.Instance.AskChainedStepCore(agent, stub, "Passo uno", prefix);
        var last = AgentServer.Instance.AskChainedStepCore(agent, stub, "Passo due", prefix);

        last.Should().Be("passo-2");
        Basic.Agents.Blackboard.Read(prefix + "_s1").Should().Be("passo-1");
        Basic.Agents.Blackboard.Read(prefix + "_s2").Should().Be("passo-2");
        // Il prompt del secondo anello riassume lo stato del primo (reset di contesto implicito).
        stub.Prompts[1].Should().Contain("passo-1");
        stub.Prompts[1].Should().Contain("Passo due");
    }

    private sealed class SeqCompleter : ILlmCompleter
    {
        private readonly string[] _answers;
        private int _i;

        public SeqCompleter(params string[] answers) => _answers = answers;
        public string ModelName => "stub-chain";
        public List<string> Prompts { get; } = [];

        public string Complete(string systemPrompt, string userPrompt)
        {
            Prompts.Add(userPrompt);
            return _answers[Math.Min(_i++, _answers.Length - 1)];
        }
    }
}
