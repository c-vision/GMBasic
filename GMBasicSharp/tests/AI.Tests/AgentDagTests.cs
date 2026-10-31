using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using Basic.Agents;
using Xunit;

namespace AI.Tests;

public sealed class AgentDagTests
{
    [Fact]
    public void ThreeLevelDag_FanOutFanIn_CorrectOrder()
    {
        // root -> 3 nodi paralleli (dipendono da root) -> synthesis (dipende dai 3)
        var nodes = new[]
        {
            new DagNode { Id="root", DependsOn=Array.Empty<string>(), Compute=_ => "base" },
            new DagNode { Id="w0", DependsOn=new[]{"root"}, Compute=r => r + ";w0" },
            new DagNode { Id="w1", DependsOn=new[]{"root"}, Compute=r => r + ";w1" },
            new DagNode { Id="w2", DependsOn=new[]{"root"}, Compute=r => r + ";w2" },
            new DagNode { Id="synth", DependsOn=new[]{"w0","w1","w2"}, Compute=r => "SYNTH(" + r.Replace("\n","|") + ")" },
        };
        var r = AgentDag.Execute(nodes, maxConcurrency: 3);
        Assert.True(r.Ok, r.Error);
        Assert.Equal("base", r.Results["root"]);
        Assert.Contains("w0", r.Results["w0"]);
        Assert.Equal("SYNTH(base;w0|base;w1|base;w2)", r.Results["synth"]);
        Assert.True(r.PeakConcurrency <= 3, $"picco {r.PeakConcurrency} > 3");
    }

    [Fact]
    public void Cycle_ExplicitError()
    {
        var nodes = new[]
        {
            new DagNode { Id="a", DependsOn=new[]{"b"}, Compute=_ => "a" },
            new DagNode { Id="b", DependsOn=new[]{"a"}, Compute=_ => "b" },
        };
        var r = AgentDag.Execute(nodes);
        Assert.False(r.Ok);
        Assert.Contains("ciclo", r.Error);
    }
}
