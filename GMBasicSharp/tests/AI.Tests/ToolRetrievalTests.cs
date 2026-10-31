// FILE: AI.Tests.ToolRetrievalTests — N1 Needle3 (router a retrieval, offline, deterministico).
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;
using Basic.Agents;
using Basic.Mcp;
using Xunit;

namespace AI.Tests;

public sealed class ToolRetrievalTests
{
    private static McpToolDefinition Tool(string name, string description) => new()
    {
        Name = name,
        Description = description,
        QualifiedProcedureName = "Test:" + name,
        InputSchema = new JsonObject(),
    };

    private static List<McpToolDefinition> Catalog12() => new()
    {
        Tool("gmail.search", "search emails in gmail inbox by sender and subject"),
        Tool("gmail.send", "send an email message to a recipient via gmail"),
        Tool("gmail.delete", "delete an email message from the gmail inbox"),
        Tool("calendar.list", "list upcoming calendar events and meetings"),
        Tool("calendar.create", "create a calendar event with title and time"),
        Tool("calendar.delete", "delete a calendar event by title"),
        Tool("files.read", "read a text file from disk by path"),
        Tool("files.write", "write text content to a file on disk"),
        Tool("files.delete", "delete a file from disk by path"),
        Tool("web.fetch", "fetch a web page url and return its content"),
        Tool("web.search", "search the web for information on a topic"),
        Tool("notes.append", "append a text note to the daily notes file"),
    };

    private static bool ContainsTool(IReadOnlyList<McpToolDefinition> tools, string name)
    {
        foreach (var t in tools)
            if (string.Equals(t.Name, name, StringComparison.OrdinalIgnoreCase)) return true;
        return false;
    }

    // Set etichettato: (richiesta, tool giusto). Il ranker lessicale deve tenerlo nei top-k
    // e il prompt deve restare bounded (k righe, non 12).
    [Theory]
    [InlineData("read the report file from disk", "files.read")]
    [InlineData("search emails in the inbox about the contract", "gmail.search")]
    [InlineData("create a calendar event for tomorrow morning", "calendar.create")]
    [InlineData("fetch the web page at this url", "web.fetch")]
    public void LabeledSet_CorrectToolInTopK_PromptBounded(string request, string expected)
    {
        var catalog = Catalog12();
        var sel = ToolRetrieval.SelectTools(request, catalog);
        Assert.False(sel.Escalated);
        Assert.True(ContainsTool(sel.Tools, expected), $"atteso {expected} nei top-k per '{request}'");
        Assert.True(sel.Tools.Count <= ToolRetrieval.TopK,
            $"prompt non bounded: {sel.Tools.Count} tool su {catalog.Count}");
        Assert.True(sel.Tools.Count < catalog.Count, "il catalogo intero e' entrato nel prompt");
    }

    [Fact]
    public void NoMatch_EscalatesNoTool()
    {
        var catalog = Catalog12();
        var sel = ToolRetrieval.SelectTools("zzblorp qwerty flimflam", catalog);
        Assert.True(sel.Escalated);
        Assert.Equal(EscalationReason.NoTool, sel.EscalationReason);
        Assert.Empty(sel.Tools);
    }

    [Fact]
    public void EmptyCatalog_EscalatesNoTool()
    {
        var sel = ToolRetrieval.SelectTools("read a file", new List<McpToolDefinition>());
        Assert.True(sel.Escalated);
        Assert.Equal(EscalationReason.NoTool, sel.EscalationReason);
    }

    [Fact]
    public void Ranking_IsDeterministic()
    {
        var catalog = Catalog12();
        var a = ToolRetrieval.SelectTools("delete the calendar event", catalog);
        var b = ToolRetrieval.SelectTools("delete the calendar event", catalog);
        Assert.Equal(a.Tools.Count, b.Tools.Count);
        for (int i = 0; i < a.Tools.Count; i++)
            Assert.Equal(a.Tools[i].Name, b.Tools[i].Name);
    }

    [Fact]
    public void OptIn_IsPerAgent_OffByDefault_OnViaSet()
    {
        // L'accensione e' per-agente via AI AGENT SET nome, toolretrieval, ON (AgentServer.Set):
        // default OFF, il comportamento resta quello storico finche' non lo si accende.
        var agent = new AgentRecord { Name = "RetrOptIn" };
        Assert.False(agent.ToolRetrievalEnabled);
        Assert.True(Basic.Agents.AgentServer.Instance.Create("RetrOptInProbe"));
        try
        {
            Assert.True(Basic.Agents.AgentServer.Instance.Set("RetrOptInProbe", "toolretrieval", "ON"));
            var got = Basic.Agents.AgentServer.Instance.List().First(a => a.Name == "RetrOptInProbe");
            Assert.True(got.ToolRetrievalEnabled);
            Assert.True(Basic.Agents.AgentServer.Instance.Set("RetrOptInProbe", "toolretrieval", "OFF"));
            got = Basic.Agents.AgentServer.Instance.List().First(a => a.Name == "RetrOptInProbe");
            Assert.False(got.ToolRetrievalEnabled);
        }
        finally
        {
            Basic.Agents.AgentServer.Instance.Delete("RetrOptInProbe");
        }
    }
}
