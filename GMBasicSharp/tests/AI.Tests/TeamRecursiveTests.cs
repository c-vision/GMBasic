// FILE: AI.Tests.TeamRecursiveTests
// RecursiveMAS A.1/A.3/A.4/A.5 + B.17: loop ricorsivo, latent embed, checkpoint link,
// dataset JSON, funzioni read AI.TEAMRECURSION/TEAMLATENT/TEAMRUNTIMESTATS.
using System.Text.Json.Nodes;
using Basic.Agents;
using Basic.Agents.Recursive;
using Basic.Providers;
using FluentAssertions;
using Xunit;

namespace AI.Tests;

[Trait("Category", "Fast")]
public sealed class TeamRecursiveTests : IDisposable
{
    public TeamRecursiveTests()
    {
        AgentServer.ResetForTests();
        TeamServer.ResetForTests();
        ProviderServer.ResetForTests();
        RecursiveTrainer.ResetForTests();
    }

    public void Dispose()
    {
        AgentServer.ResetForTests();
        TeamServer.ResetForTests();
        ProviderServer.ResetForTests();
        RecursiveTrainer.ResetForTests();
    }

    private static void CreateTwoMemberTeam(string team, string strategy = "sequential")
    {
        AgentServer.Instance.Create(team + "-a");
        AgentServer.Instance.Create(team + "-b");
        TeamServer.Instance.Create(team);
        TeamServer.Instance.AddAgent(team, team + "-a");
        TeamServer.Instance.AddAgent(team, team + "-b");
        TeamServer.Instance.SetStrategy(team, strategy == "coordinator" ? TeamStrategy.Coordinator : TeamStrategy.Sequential);
    }

    [Fact(DisplayName = "A.1: r=3 sequential produce 3 round x 2 membri = 6 Ask e metriche per round")]
    public void RunRecursive_Text_R3_AsksThreeRoundsPerMember()
    {
        CreateTwoMemberTeam("rt1");
        TeamServer.Instance.SetRecursion("rt1", 3);
        TeamServer.Instance.SetLatent("rt1", "text");

        var team = TeamServer.Instance.Get("rt1")!;
        var result = TeamOrchestrator.Run(team, "task di prova");

        result.Should().NotBeNullOrEmpty();
        team.Conversation.Should().HaveCount(6, "3 round x 2 membri (LATENT text = nessun tag extra)");
        team.Metadata["lastRunRounds"].Should().Be("3");

        // R6: stats per round registrate.
        var stats = JsonNode.Parse(RecursiveTrainer.GetRuntimeStats("rt1"));
        stats!["team"]!.GetValue<string>().Should().Be("rt1");
        var runs = stats["runs"]!.AsArray();
        runs.Count.Should().Be(3);
        runs[0]!["round"]!.GetValue<int>().Should().Be(1);
    }

    [Fact(DisplayName = "B.17: AI.TEAMRECURSION/TEAMLATENT/TEAMRUNTIMESTATS leggono la config")]
    public void ReadFunctions_ReturnConfiguredValues()
    {
        CreateTwoMemberTeam("rt2");
        TeamServer.Instance.SetRecursion("rt2", 4);
        TeamServer.Instance.SetLatent("rt2", "embed");

        Basic.Agents.TeamServer.Instance.GetRecursion("rt2").Should().Be(4);
        Basic.Agents.TeamServer.Instance.GetLatent("rt2").Should().Be("embed");
        var stats = RecursiveTrainer.GetRuntimeStats("rt2");
        stats.Should().Contain("\"team\":\"rt2\"");
        // team inesistente -> default
        Basic.Agents.TeamServer.Instance.GetRecursion("non-esiste").Should().Be(1);
        Basic.Agents.TeamServer.Instance.GetLatent("non-esiste").Should().Be("text");
    }

    [Fact(DisplayName = "A.3: LATENT embed appende la fingerprint latente (R_in residual + R_out) al contesto")]
    public void Run_Embed_AppendsLatentFingerprint()
    {
        CreateTwoMemberTeam("rt3");
        TeamServer.Instance.SetLatent("rt3", "embed");
        TeamServer.Instance.SetLink("rt3", "inner", "{\"hidden\":64}");
        TeamServer.Instance.SetLink("rt3", "outer", "{\"hidden\":64}");

        var team = TeamServer.Instance.Get("rt3")!;
        var result = TeamOrchestrator.Run(team, "Qual è la capitale?");

        // Il contesto dell'ultimo membro porta la fingerprint latente compatta (bounded).
        var last = team.Conversation[^1];
        result.Should().NotBeNullOrEmpty();
        team.Metadata.ContainsKey("lastLatentFp").Should().BeTrue();
        team.Metadata["lastLatentFp"].Length.Should().BeLessThan(200, "la fingerprint latente deve essere compatta (proxy token saving)");
        // fingerprint formato "idx:val,idx:val,idx:val" (separatore decimale locale-aware)
        team.Metadata["lastLatentFp"].Should().MatchRegex(@"^\d+:[-\d.,]+,\d+:[-\d.,]+,\d+:[-\d.,]+$");
    }

    [Fact(DisplayName = "A.3: LATENT text NON produce fingerprint (default invariato)")]
    public void Run_Text_NoLatentFingerprint()
    {
        CreateTwoMemberTeam("rt4");
        TeamServer.Instance.SetLatent("rt4", "text");

        var team = TeamServer.Instance.Get("rt4")!;
        TeamOrchestrator.Run(team, "task");

        team.Metadata.ContainsKey("lastLatentFp").Should().BeFalse();
        team.Conversation.Any(m => m.Content.Contains("⟨latent⟩")).Should().BeFalse();
    }

    [Fact(DisplayName = "A.4: TEAM SAVE/LOAD persiste i pesi R_in/R_out — la loss riparte dal livello addestrato")]
    public void SaveLoad_PersistsRecursiveLinkWeights()
    {
        CreateTwoMemberTeam("rt5");
        TeamServer.Instance.SetLatent("rt5", "embed");
        TeamServer.Instance.SetLink("rt5", "inner", "{\"hidden\":64}");
        TeamServer.Instance.SetLink("rt5", "outer", "{\"hidden\":64}");

        RecursiveTrainer.TrainInner("rt5", "rt5-a", "toy", steps: 200);
        RecursiveTrainer.TrainOuter("rt5", "toy", steps: 200);

        var path = System.IO.Path.Combine(System.IO.Path.GetTempPath(), "rt5-team-" + Guid.NewGuid().ToString("N"));
        try
        {
            TeamServer.Instance.Save("rt5", path + ".team.json");
            System.IO.File.Exists(path + ".team.json.links.json").Should().BeTrue("il checkpoint link deve essere scritto accanto al team");

            TeamServer.ResetForTests();
            RecursiveTrainer.ResetForTests();

            TeamServer.Instance.Load("rt5", path + ".team.json").Should().BeTrue();

            // La loss di un singolo step su pesi GIÀ addestrati riparte dal livello basso
            // (~0.2), non da ~1.0 (pesi random): prova che gli handle sono stati ripristinati.
            RecursiveTrainer.TrainInner("rt5", "rt5-a", "toy", steps: 1);
            var status = JsonNode.Parse(RecursiveTrainer.GetStatus("rt5"));
            var loss = status!["inner"]!["rt5-a"]!["loss"]!.GetValue<double>();
            loss.Should().BeLessThan(0.9, "i pesi ripristinati devono avere già la loss addestrata (non ~1.0 da random)");
        }
        finally
        {
            if (System.IO.File.Exists(path + ".team.json")) System.IO.File.Delete(path + ".team.json");
            if (System.IO.File.Exists(path + ".team.json.links.json")) System.IO.File.Delete(path + ".team.json.links.json");
        }
    }

    [Fact(DisplayName = "A.5: LoadDataset accetta JSON con campo data (stile HF --dataset_json_field data)")]
    public void LoadDataset_JsonDataField_ParsesRows()
    {
        var path = System.IO.Path.Combine(System.IO.Path.GetTempPath(), "rt6-ds-" + Guid.NewGuid().ToString("N") + ".json");
        try
        {
            System.IO.File.WriteAllText(path, """
                { "data": [
                    { "x": "uno due tre", "y": "due tre" },
                    { "input": "alfa beta", "output": "beta" },
                    ["quattro", "quattro"],
                    "sette|sette"
                ] }
                """);
            var rows = RecursiveTrainer.LoadDataset(path);
            rows.Should().HaveCount(4);
            rows[0].Should().Be(("uno due tre", "due tre"));
            rows[1].Should().Be(("alfa beta", "beta"));
            rows[2].Should().Be(("quattro", "quattro"));
            rows[3].Should().Be(("sette", "sette"));
        }
        finally
        {
            if (System.IO.File.Exists(path)) System.IO.File.Delete(path);
        }
    }

    [Fact(DisplayName = "A.1: RecursiveLink export/import round-trip preserva i pesi (Forward identico)")]
    public void RecursiveLink_ExportImport_PreservesWeights()
    {
        var h1 = RecursiveLink.CreateInner(32, 16);
        RecursiveLink.TrainStep(h1, RecursiveLink.SimpleEmbed("x").AsSpan(0, 32).ToArray(),
            RecursiveLink.SimpleEmbed("y").AsSpan(0, 32).ToArray(), 0.05);
        var before = RecursiveLink.Export(h1)!;

        var h2 = RecursiveLink.Import(before);
        h2.Should().BeGreaterThan(0);

        var inp = Enumerable.Range(0, 32).Select(i => (double)(i % 7) / 3.0).ToArray();
        var out1 = RecursiveLink.Forward(h1, inp)!;
        var out2 = RecursiveLink.Forward(h2, inp)!;
        out1.Should().Equal(out2, (a, b) => Math.Abs(a - b) < 1e-12);
    }
}
