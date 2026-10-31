// FILE: Interpreter.Parser.Stmt.Team.cs

namespace Basic;

public sealed partial class Parser
{
    public static Stmt ParseTeamStatement(Parser parser, string expectedNamespace)
    {
        parser.Consume(TokenKind.TeamCreate, "Expected 'TEAM'.");

        // TEAM CREATE name
        if (parser.Match(TokenKind.Create))
        {
            var name = parser.Consume(TokenKind.Identifier, "Expected team name after 'TEAM CREATE'.");
            return new TeamCreateStmt(name, name);
        }

        // TEAM SAVE name — salva il team su disco
        if (parser.Match(TokenKind.Save))
        {
            var name = parser.Consume(TokenKind.Identifier, "Expected team name after 'TEAM SAVE'.");
            return new TeamSaveStmt(name, name);
        }

        // TEAM LOAD name — carica il team da disco
        if (parser.Match(TokenKind.Load))
        {
            var name = parser.Consume(TokenKind.Identifier, "Expected team name after 'TEAM LOAD'.");
            return new TeamLoadStmt(name, name);
        }

        // TEAM MESSAGE name, message — quando TEAM vive dentro AI (AI TEAM MESSAGE)
        if (parser.Match(TokenKind.Message))
        {
            var msgTeamName = parser.Consume(TokenKind.Identifier, "Expected team name after 'TEAM MESSAGE'.");
            parser.Consume(TokenKind.Comma, "Expected ',' after team name.");
            var message = parser.ParseExpression();
            return new TeamMessageStmt(msgTeamName, message, msgTeamName);
        }

        // TEAM TRAIN INNER team, agent, dataset | TEAM TRAIN OUTER team, dataset — R4
        if (parser.Match(TokenKind.Train))
        {
            if (parser.Match(TokenKind.Inner))
            {
                var innerTeam = parser.Consume(TokenKind.Identifier, "Expected team name after 'TEAM TRAIN INNER'.");
                parser.Consume(TokenKind.Comma, "Expected ',' after team name.");
                var agent = parser.ParseExpression();
                // Legacy `TEAM TRAIN INNER T, A1, "ds"`: A1 come identificatore semplice è una
                // STRINGA letterale (nome agente), non una variabile — stesso pattern di AI AGENT CREATE.
                if (agent is VariableExpr ve && ve.NameCleaned.Lexeme == ve.NameSuffixed.Lexeme)
                    agent = new StringExpr(ve.NameCleaned.Lexeme, ve.NameSuffixed);
                parser.Consume(TokenKind.Comma, "Expected ',' after agent.");
                var dsInner = parser.ParseExpression();
                return new TeamTrainInnerStmt(innerTeam, agent, dsInner, innerTeam);
            }
            if (parser.Match(TokenKind.Outer))
            {
                var outerTeam = parser.Consume(TokenKind.Identifier, "Expected team name after 'TEAM TRAIN OUTER'.");
                parser.Consume(TokenKind.Comma, "Expected ',' after team name.");
                var dsOuter = parser.ParseExpression();
                return new TeamTrainOuterStmt(outerTeam, dsOuter, outerTeam);
            }
            // 9.3: TEAM TRAIN THINKING team, dataset — distillazione Quiet-STaR
            if (parser.Match(TokenKind.Thinking))
            {
                var thTeam = parser.Consume(TokenKind.Identifier, "Expected team name after 'TEAM TRAIN THINKING'.");
                parser.Consume(TokenKind.Comma, "Expected ',' after team name.");
                var dsTh = parser.ParseExpression();
                return new TeamTrainThinkingStmt(thTeam, dsTh, thTeam);
            }
            // 9.6: TEAM TRAIN ACTOR CRITIC team, dataset — DPSDP actor-critic
            if (parser.Match(TokenKind.Actor))
            {
                parser.Consume(TokenKind.Critic, "Expected CRITIC after 'TRAIN ACTOR'.");
                var acTeam = parser.Consume(TokenKind.Identifier, "Expected team name after 'TEAM TRAIN ACTOR CRITIC'.");
                parser.Consume(TokenKind.Comma, "Expected ',' after team name.");
                var dsAc = parser.ParseExpression();
                return new TeamTrainActorCriticStmt(acTeam, dsAc, acTeam);
            }
            throw CreateError(parser.Peek(), "Expected INNER, OUTER, THINKING or ACTOR CRITIC after 'TEAM TRAIN'.");
        }

        // 9.3: TEAM DISTILL THINKING team, dataset — distillazione Fast Quiet-STaR NTP
        if (parser.Match(TokenKind.Distill))
        {
            parser.Consume(TokenKind.Thinking, "Expected THINKING after 'TEAM DISTILL'.");
            var dsTeam = parser.Consume(TokenKind.Identifier, "Expected team name after 'TEAM DISTILL THINKING'.");
            parser.Consume(TokenKind.Comma, "Expected ',' after team name.");
            var dsDist = parser.ParseExpression();
            return new TeamDistillThinkingStmt(dsTeam, dsDist, dsTeam);
        }

        // TEAM name ADD AGENT agent
        var teamName = parser.Consume(TokenKind.Identifier, "Expected team name or 'CREATE' after 'TEAM'.");
        if (parser.Match(TokenKind.Add))
        {
            parser.Consume(TokenKind.Agent, "Expected 'AGENT' after 'ADD'.");
            // "Critic" è ora una keyword (§9) ma resta un nome agente valido.
            var agentName = parser.Peek().Kind is TokenKind.Identifier or TokenKind.Critic
                ? parser.Advance()
                : throw CreateError(parser.Peek(), "Expected agent name.");
            return new TeamAddAgentStmt(teamName, agentName, teamName);
        }

        // TEAM name REMOVE AGENT agent
        if (parser.Match(TokenKind.Remove))
        {
            parser.Consume(TokenKind.Agent, "Expected 'AGENT' after 'REMOVE'.");
            var agentName = parser.Peek().Kind is TokenKind.Identifier or TokenKind.Critic
                ? parser.Advance()
                : throw CreateError(parser.Peek(), "Expected agent name.");
            return new TeamRemoveAgentStmt(teamName, agentName, teamName);
        }

        // TEAM name RUN task — orchestrazione del team su un task
        if (parser.Match(TokenKind.Run))
        {
            var task = parser.ParseExpression();
            return new TeamRunStmt(teamName, task, teamName);
        }

        // TEAM name BROADCAST message — L1 broadcast
        if (parser.Peek().Lexeme.Equals("BROADCAST", StringComparison.OrdinalIgnoreCase))
        {
            parser.Advance();
            var msg = parser.ParseExpression();
            return new TeamBroadcastStmt(teamName, msg, teamName);
        }

        // TEAM name REDEFINE definition — L1 redefine in-flight
        if (parser.Peek().Lexeme.Equals("REDEFINE", StringComparison.OrdinalIgnoreCase))
        {
            parser.Advance();
            var def = parser.ParseExpression();
            return new TeamRedefineStmt(teamName, def, teamName);
        }

        // TEAM SAVE name — salva il team su disco
        if (parser.Check(TokenKind.Save))
        {
            parser.Advance();
            var name = parser.Consume(TokenKind.Identifier, "Expected team name after 'TEAM SAVE'.");
            return new TeamSaveStmt(name, name);
        }

        // TEAM LOAD name — carica il team da disco
        if (parser.Check(TokenKind.Load))
        {
            parser.Advance();
            var name = parser.Consume(TokenKind.Identifier, "Expected team name after 'TEAM LOAD'.");
            return new TeamLoadStmt(name, name);
        }

        // TEAM name SET ROLE agent role — configura il ruolo di un membro
        if (parser.Match(TokenKind.Set))
        {
            if (parser.Match(TokenKind.Role))
            {
                var agentName = parser.Consume(TokenKind.Identifier, "Expected agent name after 'SET ROLE'.");
                // CRITIC è ora una keyword (estensione §9) ma resta un VALORE di ruolo valido.
                // Ruoli: OBSERVER è un Identifier, CRITIC/COORDINATOR/WORKER sono KEYWORD del
                // lexer (TokenKind.Critic/Coordinator/AiSubWorker) → vanno accettati tutti,
                // altrimenti `SET ROLE <agente> WORKER` falliva in parse su TUTTE le gambe
                // (trovato da minitests/misc/team_domain.bas, stale per questo bug).
                var roleName = parser.Peek().Kind is TokenKind.Identifier or TokenKind.Critic
                    or TokenKind.Coordinator or TokenKind.AiSubWorker
                    ? parser.Advance()
                    : throw CreateError(parser.Peek(), "Expected role name (COORDINATOR, WORKER, CRITIC, OBSERVER).");
                return new TeamSetRoleStmt(teamName, agentName, roleName, teamName);
            }
            if (parser.Match(TokenKind.Strategy))
            {
                // SEQUENTIAL/ROUNDROBIN/COORDINATOR/MIXTURE/DISTILLATION/DELIBERATION sono
                // Identifier; DAG è una keyword (TokenKind.Dag) → accettare entrambi.
                var stratName = parser.Peek().Kind is TokenKind.Identifier or TokenKind.Dag
                    ? parser.Advance()
                    : throw CreateError(parser.Peek(), "Expected strategy name (SEQUENTIAL, ROUNDROBIN, COORDINATOR, MIXTURE, DISTILLATION, DELIBERATION, DAG).");
                return new TeamSetStrategyStmt(teamName, stratName, teamName);
            }
            if (parser.Match(TokenKind.Coordinator))
            {
                var agentName = parser.Consume(TokenKind.Identifier, "Expected agent name.");
                return new TeamSetCoordinatorStmt(teamName, agentName, teamName);
            }
            // R1: TEAM name SET RECURSION n
            if (parser.Match(TokenKind.Recursion))
            {
                var n = parser.ParseExpression();
                return new TeamSetRecursionStmt(teamName, n, teamName);
            }
            // R1: TEAM name SET LATENT mode (text|embed|native|kv) — i valori collidono con
            // keyword esistenti (TEXT → TokenKind.Text, EMBED → TokenKind.AiVectorEmbed):
            // si accettano per TokenKind esplicito, mai per lexeme.
            if (parser.Match(TokenKind.Latent))
            {
                var mode = parser.Peek().Kind switch
                {
                    TokenKind.Text or TokenKind.AiVectorEmbed or TokenKind.Identifier => parser.Advance(),
                    _ => throw CreateError(parser.Peek(), "Expected latent mode (TEXT, EMBED, NATIVE, KV).")
                };
                return new TeamSetLatentStmt(teamName, mode, teamName);
            }
            // R1: TEAM name SET LINK which cfg (inner|outer|distill — INNER/OUTER sono keyword)
            if (parser.Match(TokenKind.Link))
            {
                var which = parser.Peek().Kind switch
                {
                    TokenKind.Identifier or TokenKind.Outer or TokenKind.Inner => parser.Advance(),
                    _ => throw CreateError(parser.Peek(), "Expected link type (INNER, OUTER, DISTILL).")
                };
                Expr cfg = new StringExpr("", which);
                if (parser.Match(TokenKind.Comma))
                    cfg = parser.ParseExpression();
                else if (!parser.Check(TokenKind.Semicolon) && !parser.IsAtEnd())
                {
                    try { cfg = parser.ParseExpression(); } catch { /* keep default */ }
                }
                return new TeamSetLinkStmt(teamName, which, cfg, teamName);
            }
            // R1: TEAM name SET THOUGHTS m
            if (parser.Match(TokenKind.Thoughts))
            {
                var m = parser.ParseExpression();
                return new TeamSetThoughtsStmt(teamName, m, teamName);
            }
            // 9.4: TEAM name SET DAG d_max,l_max,n_max
            if (parser.Match(TokenKind.Dag))
            {
                var d = parser.ParseExpression();
                parser.Consume(TokenKind.Comma, "Expected ',' after d_max.");
                var l = parser.ParseExpression();
                parser.Consume(TokenKind.Comma, "Expected ',' after l_max.");
                var n = parser.ParseExpression();
                return new TeamSetDagStmt(teamName, d, l, n, teamName);
            }
            // 9.4: TEAM name SET COMPLEXITY threshold
            if (parser.Match(TokenKind.Complexity))
            {
                var th = parser.ParseExpression();
                return new TeamSetComplexityStmt(teamName, th, teamName);
            }
            // 9.5: TEAM name SET REFLECTION mode (intra|inter|both|off)
            if (parser.Match(TokenKind.Reflection))
            {
                var mode = parser.Peek().Kind is TokenKind.Identifier or TokenKind.Off
                    ? parser.Advance()
                    : throw CreateError(parser.Peek(), "Expected reflection mode (INTRA, INTER, BOTH, OFF).");
                return new TeamSetReflectionStmt(teamName, mode, teamName);
            }
            // 9.5: TEAM name SET CRITIC personas, judge
            if (parser.Match(TokenKind.Critic))
            {
                var personas = parser.Peek().Kind is TokenKind.Identifier or TokenKind.StringLiteral
                    ? parser.Advance()
                    : throw CreateError(parser.Peek(), "Expected critic personas (es. spec|edge|logic).");
                Expr judge = new StringExpr(personas.Lexeme, personas);
                if (parser.Match(TokenKind.Comma))
                {
                    judge = parser.ParseExpression();
                    // il judge è un nome agente (identificatore semplice) → stringa letterale
                    if (judge is VariableExpr jve && jve.NameCleaned.Lexeme == jve.NameSuffixed.Lexeme)
                        judge = new StringExpr(jve.NameCleaned.Lexeme, jve.NameSuffixed);
                }
                return new TeamSetCriticStmt(teamName, personas, judge, teamName);
            }
            // 9.5: TEAM name SET PRUNE topK
            if (parser.Match(TokenKind.Prune))
            {
                var k = parser.ParseExpression();
                return new TeamSetPruneStmt(teamName, k, teamName);
            }
        }

        throw CreateError(parser.Peek(), "Expected 'CREATE', 'ADD', 'REMOVE', 'RUN', 'TRAIN INNER|OUTER' or 'SET ROLE/STRATEGY/COORDINATOR/RECURSION/LATENT/LINK/THOUGHTS' after 'TEAM'.");
    }
}
