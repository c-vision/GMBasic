// FILE: Ast.Team
using System.Collections.Immutable;
using System.Text.Json.Serialization;

namespace Basic;

[JsonDerivedType(typeof(TeamCreateStmt), nameof(TeamCreateStmt))]
public sealed record TeamCreateStmt(Token TeamName, Token Token) : Stmt(Token.Position)
{
    public override AstNode DeepClone() =>
        new TeamCreateStmt((Token)TeamName.DeepClone(), (Token)Token.DeepClone());
}

[JsonDerivedType(typeof(TeamAddAgentStmt), nameof(TeamAddAgentStmt))]
public sealed record TeamAddAgentStmt(Token TeamName, Token AgentName, Token Token) : Stmt(Token.Position)
{
    public override AstNode DeepClone() =>
        new TeamAddAgentStmt((Token)TeamName.DeepClone(), (Token)AgentName.DeepClone(), (Token)Token.DeepClone());
}

[JsonDerivedType(typeof(TeamRemoveAgentStmt), nameof(TeamRemoveAgentStmt))]
public sealed record TeamRemoveAgentStmt(Token TeamName, Token AgentName, Token Token) : Stmt(Token.Position)
{
    public override AstNode DeepClone() =>
        new TeamRemoveAgentStmt((Token)TeamName.DeepClone(), (Token)AgentName.DeepClone(), (Token)Token.DeepClone());
}

[JsonDerivedType(typeof(TeamMessageStmt), nameof(TeamMessageStmt))]
public sealed record TeamMessageStmt(Token TeamName, Expr Message, Token Token) : Stmt(Token.Position)
{
    public override AstNode DeepClone() =>
        new TeamMessageStmt((Token)TeamName.DeepClone(), (Expr)Message.DeepClone(), (Token)Token.DeepClone());
}


/// <summary>TEAM name RUN task — esegue l'orchestrazione del team su un task.</summary>
[JsonDerivedType(typeof(TeamRunStmt), nameof(TeamRunStmt))]
public sealed record TeamRunStmt(Token TeamName, Expr Task, Token Token) : Stmt(Token.Position)
{
    public override AstNode DeepClone() =>
        new TeamRunStmt((Token)TeamName.DeepClone(), (Expr)Task.DeepClone(), (Token)Token.DeepClone());
}


/// <summary>TEAM name SET ROLE agent role — configura il ruolo di un membro.</summary>
[JsonDerivedType(typeof(TeamSetRoleStmt), nameof(TeamSetRoleStmt))]
public sealed record TeamSetRoleStmt(Token TeamName, Token AgentName, Token RoleName, Token Token) : Stmt(Token.Position)
{
    public override AstNode DeepClone() => new TeamSetRoleStmt(
        (Token)TeamName.DeepClone(), (Token)AgentName.DeepClone(), (Token)RoleName.DeepClone(), (Token)Token.DeepClone());
}

/// <summary>TEAM name SET STRATEGY strat — configura la strategia di orchestrazione.</summary>
[JsonDerivedType(typeof(TeamSetStrategyStmt), nameof(TeamSetStrategyStmt))]
public sealed record TeamSetStrategyStmt(Token TeamName, Token StrategyName, Token Token) : Stmt(Token.Position)
{
    public override AstNode DeepClone() => new TeamSetStrategyStmt(
        (Token)TeamName.DeepClone(), (Token)StrategyName.DeepClone(), (Token)Token.DeepClone());
}

/// <summary>TEAM name SET COORDINATOR agent — imposta l'agente coordinatore.</summary>
[JsonDerivedType(typeof(TeamSetCoordinatorStmt), nameof(TeamSetCoordinatorStmt))]
public sealed record TeamSetCoordinatorStmt(Token TeamName, Token AgentName, Token Token) : Stmt(Token.Position)
{
    public override AstNode DeepClone() => new TeamSetCoordinatorStmt(
        (Token)TeamName.DeepClone(), (Token)AgentName.DeepClone(), (Token)Token.DeepClone());
}


/// <summary>TEAM SAVE name — salva il team su disco (JSON).</summary>
[JsonDerivedType(typeof(TeamSaveStmt), nameof(TeamSaveStmt))]
public sealed record TeamSaveStmt(Token TeamName, Token Token) : Stmt(Token.Position)
{
    public override AstNode DeepClone() => new TeamSaveStmt((Token)TeamName.DeepClone(), (Token)Token.DeepClone());
}

/// <summary>TEAM LOAD name — carica il team da disco (JSON).</summary>
[JsonDerivedType(typeof(TeamLoadStmt), nameof(TeamLoadStmt))]
public sealed record TeamLoadStmt(Token TeamName, Token Token) : Stmt(Token.Position)
{
    public override AstNode DeepClone() => new TeamLoadStmt((Token)TeamName.DeepClone(), (Token)Token.DeepClone());
}

/// <summary>TEAM name SET RECURSION n — imposta round ricorsivi (R1).</summary>
[JsonDerivedType(typeof(TeamSetRecursionStmt), nameof(TeamSetRecursionStmt))]
public sealed record TeamSetRecursionStmt(Token TeamName, Expr Rounds, Token Token) : Stmt(Token.Position)
{
    public override AstNode DeepClone() => new TeamSetRecursionStmt((Token)TeamName.DeepClone(), (Expr)Rounds.DeepClone(), (Token)Token.DeepClone());
}

/// <summary>TEAM name SET LATENT mode — text|embed|native|kv (R1).</summary>
[JsonDerivedType(typeof(TeamSetLatentStmt), nameof(TeamSetLatentStmt))]
public sealed record TeamSetLatentStmt(Token TeamName, Token Mode, Token Token) : Stmt(Token.Position)
{
    public override AstNode DeepClone() => new TeamSetLatentStmt((Token)TeamName.DeepClone(), (Token)Mode.DeepClone(), (Token)Token.DeepClone());
}

/// <summary>TEAM name SET LINK which cfg — inner|outer|distill (R1).</summary>
[JsonDerivedType(typeof(TeamSetLinkStmt), nameof(TeamSetLinkStmt))]
public sealed record TeamSetLinkStmt(Token TeamName, Token Which, Expr Config, Token Token) : Stmt(Token.Position)
{
    public override AstNode DeepClone() => new TeamSetLinkStmt((Token)TeamName.DeepClone(), (Token)Which.DeepClone(), (Expr)Config.DeepClone(), (Token)Token.DeepClone());
}

/// <summary>TEAM name SET THOUGHTS m — latent thoughts per agente (R1).</summary>
[JsonDerivedType(typeof(TeamSetThoughtsStmt), nameof(TeamSetThoughtsStmt))]
public sealed record TeamSetThoughtsStmt(Token TeamName, Expr Count, Token Token) : Stmt(Token.Position)
{
    public override AstNode DeepClone() => new TeamSetThoughtsStmt((Token)TeamName.DeepClone(), (Expr)Count.DeepClone(), (Token)Token.DeepClone());
}

/// <summary>TEAM TRAIN INNER team, agent, dataset — inner loop (cos regression) R4.</summary>
[JsonDerivedType(typeof(TeamTrainInnerStmt), nameof(TeamTrainInnerStmt))]
public sealed record TeamTrainInnerStmt(Token TeamName, Expr Agent, Expr Dataset, Token Token) : Stmt(Token.Position)
{
    public override AstNode DeepClone() => new TeamTrainInnerStmt((Token)TeamName.DeepClone(), (Expr)Agent.DeepClone(), (Expr)Dataset.DeepClone(), (Token)Token.DeepClone());
}

/// <summary>TEAM TRAIN OUTER team, dataset — outer loop (joint) R4.</summary>
[JsonDerivedType(typeof(TeamTrainOuterStmt), nameof(TeamTrainOuterStmt))]
public sealed record TeamTrainOuterStmt(Token TeamName, Expr Dataset, Token Token) : Stmt(Token.Position)
{
    public override AstNode DeepClone() => new TeamTrainOuterStmt((Token)TeamName.DeepClone(), (Expr)Dataset.DeepClone(), (Token)Token.DeepClone());
}

// ── Estensioni RecursiveMAS §9 ───────────────────────────────────────────────

/// <summary>TEAM name SET DAG d_max,l_max,n_max — AGoT DAG (9.4).</summary>
[JsonDerivedType(typeof(TeamSetDagStmt), nameof(TeamSetDagStmt))]
public sealed record TeamSetDagStmt(Token TeamName, Expr DMax, Expr LMax, Expr NMax, Token Token) : Stmt(Token.Position)
{
    public override AstNode DeepClone() => new TeamSetDagStmt((Token)TeamName.DeepClone(), (Expr)DMax.DeepClone(), (Expr)LMax.DeepClone(), (Expr)NMax.DeepClone(), (Token)Token.DeepClone());
}

/// <summary>TEAM name SET COMPLEXITY threshold — soglia di espansione AGoT (9.4).</summary>
[JsonDerivedType(typeof(TeamSetComplexityStmt), nameof(TeamSetComplexityStmt))]
public sealed record TeamSetComplexityStmt(Token TeamName, Expr Threshold, Token Token) : Stmt(Token.Position)
{
    public override AstNode DeepClone() => new TeamSetComplexityStmt((Token)TeamName.DeepClone(), (Expr)Threshold.DeepClone(), (Token)Token.DeepClone());
}

/// <summary>TEAM name SET REFLECTION mode — MAR/MIRROR reflection (9.5).</summary>
[JsonDerivedType(typeof(TeamSetReflectionStmt), nameof(TeamSetReflectionStmt))]
public sealed record TeamSetReflectionStmt(Token TeamName, Token Mode, Token Token) : Stmt(Token.Position)
{
    public override AstNode DeepClone() => new TeamSetReflectionStmt((Token)TeamName.DeepClone(), (Token)Mode.DeepClone(), (Token)Token.DeepClone());
}

/// <summary>TEAM name SET CRITIC personas, judge — MAR critici (9.5).</summary>
[JsonDerivedType(typeof(TeamSetCriticStmt), nameof(TeamSetCriticStmt))]
public sealed record TeamSetCriticStmt(Token TeamName, Token Personas, Expr Judge, Token Token) : Stmt(Token.Position)
{
    public override AstNode DeepClone() => new TeamSetCriticStmt((Token)TeamName.DeepClone(), (Token)Personas.DeepClone(), (Expr)Judge.DeepClone(), (Token)Token.DeepClone());
}

/// <summary>TEAM name SET PRUNE topK — ReTreVal prune (9.5).</summary>
[JsonDerivedType(typeof(TeamSetPruneStmt), nameof(TeamSetPruneStmt))]
public sealed record TeamSetPruneStmt(Token TeamName, Expr TopK, Token Token) : Stmt(Token.Position)
{
    public override AstNode DeepClone() => new TeamSetPruneStmt((Token)TeamName.DeepClone(), (Expr)TopK.DeepClone(), (Token)Token.DeepClone());
}

/// <summary>TEAM TRAIN THINKING team, dataset — distillazione Quiet-STaR (9.3).</summary>
[JsonDerivedType(typeof(TeamTrainThinkingStmt), nameof(TeamTrainThinkingStmt))]
public sealed record TeamTrainThinkingStmt(Token TeamName, Expr Dataset, Token Token) : Stmt(Token.Position)
{
    public override AstNode DeepClone() => new TeamTrainThinkingStmt((Token)TeamName.DeepClone(), (Expr)Dataset.DeepClone(), (Token)Token.DeepClone());
}

/// <summary>TEAM TRAIN ACTOR CRITIC team, dataset — DPSDP actor-critic (9.6).</summary>
[JsonDerivedType(typeof(TeamTrainActorCriticStmt), nameof(TeamTrainActorCriticStmt))]
public sealed record TeamTrainActorCriticStmt(Token TeamName, Expr Dataset, Token Token) : Stmt(Token.Position)
{
    public override AstNode DeepClone() => new TeamTrainActorCriticStmt((Token)TeamName.DeepClone(), (Expr)Dataset.DeepClone(), (Token)Token.DeepClone());
}

/// <summary>TEAM DISTILL THINKING team, dataset — distillazione Fast Quiet-STaR NTP (9.3).</summary>
[JsonDerivedType(typeof(TeamDistillThinkingStmt), nameof(TeamDistillThinkingStmt))]
public sealed record TeamDistillThinkingStmt(Token TeamName, Expr Dataset, Token Token) : Stmt(Token.Position)
{
    public override AstNode DeepClone() => new TeamDistillThinkingStmt((Token)TeamName.DeepClone(), (Expr)Dataset.DeepClone(), (Token)Token.DeepClone());
}

/// <summary>TEAM BROADCAST team, message — broadcast a tutti gli agenti del team (L1).</summary>
[JsonDerivedType(typeof(TeamBroadcastStmt), nameof(TeamBroadcastStmt))]
public sealed record TeamBroadcastStmt(Token TeamName, Expr Message, Token Token) : Stmt(Token.Position)
{
    public override AstNode DeepClone() => new TeamBroadcastStmt((Token)TeamName.DeepClone(), (Expr)Message.DeepClone(), (Token)Token.DeepClone());
}

/// <summary>TEAM REDEFINE team, definition — ridefinisce il team in-flight (L1).</summary>
[JsonDerivedType(typeof(TeamRedefineStmt), nameof(TeamRedefineStmt))]
public sealed record TeamRedefineStmt(Token TeamName, Expr Definition, Token Token) : Stmt(Token.Position)
{
    public override AstNode DeepClone() => new TeamRedefineStmt((Token)TeamName.DeepClone(), (Expr)Definition.DeepClone(), (Token)Token.DeepClone());
}
