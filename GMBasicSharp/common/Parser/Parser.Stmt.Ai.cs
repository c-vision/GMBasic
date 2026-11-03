// FILE: Basic.Parser.Parser.Stmt.Ai.cs

using System.Collections.Immutable;

namespace Basic;

public partial class Parser
{





    public static Stmt ParseConvertStatement(Parser parser, string? ns)
    {
        var tok = parser.Consume(TokenKind.ConvDoc, "Expected CONVERT.");
        // Dispatcher (pattern AI): i sottocomandi documento sono identifica da un
        // Identifier come verbo (es. CONVERT EXTRACT "f" TO "out", CONVERT RENDER "f" TO "out.png").
        var peek = parser.Peek();
        bool isExtract = peek.Kind == TokenKind.Extract ||
            (peek.Kind == TokenKind.Identifier && (peek.Lexeme == "EXTRACT" || peek.Lexeme == "extract"));
        bool isRender = peek.Kind == TokenKind.Identifier && (peek.Lexeme == "RENDER" || peek.Lexeme == "render");
        if (isExtract) return ParseConvertExtractCommand(parser, ns, tok);
        if (isRender) return ParseConvertRenderCommand(parser, ns, tok);
        var args = ImmutableArray.CreateBuilder<Expr>();
        args.Add(parser.ParseExpression());
        if (parser.Match(TokenKind.Comma))
            args.Add(parser.ParseExpression());
        parser.Consume(TokenKind.Into, "Expected 'INTO' after CONVERT.");
        var target = parser.ParseTarget();
        var nsTok = new Token(TokenKind.Identifier, "CONVERT", null, tok.Position);
        var call = new BuiltinFunctionCallExpr(nsTok, tok, args.ToImmutable(), new StringType());
        return new LetStmt([new Assignment(target, call)], null);
    }

    // CONVERT EXTRACT "src" TO "dest" — estrae (text-layer + OCR) e salva su file.
    private static Stmt ParseConvertExtractCommand(Parser parser, string? ns, Token tok)
    {
        parser.Advance(); // EXTRACT
        var src = parser.ParseExpression();
        parser.Consume(TokenKind.To, "Expected 'TO' after CONVERT EXTRACT source.");
        var dest = parser.ParseExpression();
        var nsTok = new Token(TokenKind.Identifier, "CONVERT", null, tok.Position);
        var call = new BuiltinFunctionCallExpr(nsTok,
            new Token(TokenKind.ConvertExtractTo, "EXTRACTTO", null, tok.Position), [src, dest], new StringType());
        return new ExpressionStmt(call);
    }

    // CONVERT RENDER "src" TO "out.png" — render pagina 0 a PNG.
    private static Stmt ParseConvertRenderCommand(Parser parser, string? ns, Token tok)
    {
        parser.Advance(); // RENDER
        var src = parser.ParseExpression();
        parser.Consume(TokenKind.To, "Expected 'TO' after CONVERT RENDER source.");
        var dest = parser.ParseExpression();
        var nsTok = new Token(TokenKind.Identifier, "CONVERT", null, tok.Position);
        var call = new BuiltinFunctionCallExpr(nsTok,
            new Token(TokenKind.ConvertRenderTo, "RENDERTO", null, tok.Position), [src, dest], new StringType());
        return new ExpressionStmt(call);
    }

    public static Stmt ParseConvertBatchStatement(Parser parser, string? ns)
    {
        var tok = parser.Consume(TokenKind.CvtBatch, "Expected CONVBAT.");
        var args = ImmutableArray.CreateBuilder<Expr>();
        args.Add(parser.ParseExpression());
        while (parser.Match(TokenKind.Comma))
            args.Add(parser.ParseExpression());
        parser.Consume(TokenKind.Into, "Expected 'INTO' after CONVBAT.");
        var target = parser.ParseTarget();
        var nsTok = new Token(TokenKind.Identifier, "CONVERT", null, tok.Position);
        var call = new BuiltinFunctionCallExpr(nsTok, tok, args.ToImmutable(), new StringType());
        return new LetStmt([new Assignment(target, call)], null);
    }

    public static Stmt ParseWebToMdStatement(Parser parser, string? ns)
    {
        var tok = parser.Consume(TokenKind.WebToMd, "Expected WEB2MD.");
        var args = ImmutableArray.CreateBuilder<Expr>();
        args.Add(parser.ParseExpression());
        parser.Consume(TokenKind.Into, "Expected 'INTO' after WEB2MD.");
        var target = parser.ParseTarget();
        var nsTok = new Token(TokenKind.Identifier, "CONVERT", null, tok.Position);
        var call = new BuiltinFunctionCallExpr(nsTok, tok, args.ToImmutable(), new StringType());
        return new LetStmt([new Assignment(target, call)], null);
    }









    // ═══════════════════════════════════════════════════════════════════════
    // SINTASSI MULTI-TOKEN (direttiva sintassi, 2026-08-19)
    // Comando legacy:  AI SET ACTIVE cap, nome | AI REGISTER role, alias, kind, cfg | ...
    // Qualificata:     AI.SETACTIVE cap, nome | AI.REGISTER ... (senza parentesi)
    // Funzioni:        attivo$ = AI.ACTIVE(%CHAT) | next$ = AI.ROTATE(role) | ...
    // ═══════════════════════════════════════════════════════════════════════

    /// <summary>Dispatcher del comando legacy multi-parola "AI ...". Consuma la keyword
    /// AI e dispatcha sul sottocomando, come DBF/ISAM/SPEED (token iniziale fisso).</summary>
    public static Stmt ParseAiStatement(Parser parser, string expectedNamespace)
    {
        parser.Consume(TokenKind.Ai, "Internal parser error: Expected 'AI' keyword.");
        var commandToken = parser.Peek();

        // AI DECIDE <state>, "<NOUL|SCORE|CHOICE>" [INTO <target>] — decision layer JEV/SystemOne
        // (v0: builtin JSON con provider Rules; la superficie DECISION<T> arriva dopo).
        if (commandToken.Kind == TokenKind.Identifier
            && commandToken.Lexeme.Equals("DECIDE", System.StringComparison.OrdinalIgnoreCase))
            return ParseAiDecideStatement(parser, expectedNamespace);

        // Sottocomandi composti: AI SET ...
        if (commandToken.Kind == TokenKind.Set)
        {
            parser.Advance(); // consuma SET
            var sub = parser.Peek();
            return sub.Kind switch
            {
                TokenKind.AiSubActive => ParseAiSetActiveStatement(parser, expectedNamespace, withProvider: false),
                TokenKind.AiSubModel => ParseAiSetModelStatement(parser, expectedNamespace),
                TokenKind.Key => ParseAiSetKeyStatement(parser, expectedNamespace),
                TokenKind.AiSubTemp => ParseAiSetTempStatement(parser, expectedNamespace),
                TokenKind.Base => ParseAiSetBaseStatement(parser, expectedNamespace),
                TokenKind.AiSubCredits => ParseAiSetCreditsStatement(parser, expectedNamespace),
                TokenKind.Priority => ParseAiSetPriorityStatement(parser, expectedNamespace),
                TokenKind.AiSubWeight => ParseAiSetWeightStatement(parser, expectedNamespace),
                TokenKind.AiSubStatus => ParseAiSetStatusStatement(parser, expectedNamespace),
                TokenKind.Default => ParseAiSetDefaultStatement(parser, expectedNamespace),
                TokenKind.HfToken => ParseAiSetHfTokenStatement(parser, expectedNamespace),
                TokenKind.HfRepo => ParseAiSetHfRepoStatement(parser, expectedNamespace),
                TokenKind.Option => ParseAiSetOptionStatement(parser, expectedNamespace),
                _ => throw Parser.CreateError(sub,
                    $"Expected ACTIVE, PROVIDER, MODEL, KEY, TEMP, BASE, CREDITS, PRIORITY, WEIGHT, STATUS, DEFAULT, HFTOKEN or HFREPO after 'AI SET'.")
            };
        }

        // §4.10 write-side `AI PATCH … END PATCH`: SPOSTATO nel namespace LLM (`LLM PATCH`).

        return commandToken.Kind switch
        {
            TokenKind.Register => ParseAiRegisterStatement(parser, expectedNamespace),
            TokenKind.AiSubConfigure => ParseAiConfigureStatement(parser, expectedNamespace),
            TokenKind.AiSubDiscover => ParseAiDiscoverStatement(parser, expectedNamespace),
            TokenKind.AiSubUnregister => ParseAiUnregisterStatement(parser, expectedNamespace),
            TokenKind.Save => parser.At(1).Kind == TokenKind.Config
                ? ParseAiSaveConfigStatement(parser, expectedNamespace)
                : ParseAiSaveProvStatement(parser, expectedNamespace),
            TokenKind.Load => parser.At(1).Kind == TokenKind.Config
                ? ParseAiLoadConfigStatement(parser, expectedNamespace)
                : ParseAiLoadProvStatement(parser, expectedNamespace),
            TokenKind.AiSubFail => ParseAiFailStatement(parser, expectedNamespace),
            TokenKind.AiSubResetFailed => ParseAiResetFailedStatement(parser, expectedNamespace),
            TokenKind.AiSubTest => ParseAiTestStatement(parser, expectedNamespace),
            TokenKind.Enable => ParseAiEnableStatement(parser),
            TokenKind.Disable => ParseAiDisableStatement(parser),
            TokenKind.Agent => ParseAiAgentStatement(parser, expectedNamespace),
            TokenKind.Channel => ParseAiChannelStatement(parser, expectedNamespace),
            TokenKind.AiPermission => ParseAiPermissionStatement(parser, expectedNamespace),
            TokenKind.Path => ParseAiPathStatement(parser, expectedNamespace),
            TokenKind.AiSubSkills => ParseAiSkillsStatement(parser, expectedNamespace),
            TokenKind.AiSubPlugins => ParseAiPluginsStatement(parser, expectedNamespace),
            // [HARD MOVE AI->LLM 2026-09-19] I verbi del layer motore/serving (MODEL/FINETUNE/LORA/
            // BENCHMARK/PREFILL/DECODE/CACHE/SERVER) sono ORA solo in `LLM …` (ParseLlmStatement).
            // `AI MODEL …` non è più valido: usare `LLM MODEL …`.
            TokenKind.Log => ParseAiLogStatement(parser, expectedNamespace),
            TokenKind.Mcp => ParseMcpStatement(parser, expectedNamespace),
            TokenKind.Connector => ParseConnectorStatement(parser, expectedNamespace),
            TokenKind.Memory => ParseAiMemoryStatement(parser),
            TokenKind.Note => ParseAiNoteStatement(parser),
            TokenKind.Rag => ParseAiRagStatement(parser, expectedNamespace),
            TokenKind.Vault => ParseAiVaultStatement(parser),
            TokenKind.Brain => ParseAiBrainStatement(parser),
            TokenKind.Ingest => ParseAiIngestStatement(parser),
            TokenKind.Sync => ParseAiSyncStatement(parser),
            TokenKind.TeamCreate => Parser.ParseTeamStatement(parser, expectedNamespace),
            TokenKind.WorkflowStatement => ParseAiWorkflowStatement(parser, expectedNamespace),
            TokenKind.Verify => ParseAiVerifyStatement(parser, expectedNamespace),
            // §4.10 write-side `AI PATCH … END PATCH`: SPOSTATO in LLM (`LLM PATCH`).
            // AI OBSERVE SCREEN|FILE|LOG (P1 todo AI-Runtime consolidato).
            TokenKind.AiSubObserve => ParseAiObserveStatement(parser, expectedNamespace),
            // AI CONTEXT SET BUDGET|SELECTOR|POLICY agent, value | AI CONTEXT VIEW agent, text (P2).
            TokenKind.Context => ParseAiContextStatement(parser, expectedNamespace),
            // E1: AI REPLAY AUTO "<sessione>" — riesecuzione automatica degli eventi registrati.
            TokenKind.Replay => ParseAiReplayStatement(parser),
            // AI WORKER / AI STATE (P3).
            TokenKind.AiSubWorker => ParseAiWorkerStatement(parser, expectedNamespace),
            TokenKind.AiSubState => ParseAiStateStatement(parser, expectedNamespace),
            TokenKind.Eval => ParseAiEvalValidateStatement(parser, expectedNamespace),
            TokenKind.AiAskChain => ParseAiAskChainStatement(parser, expectedNamespace),
            _ => throw Parser.CreateError(commandToken,
                $"Expected REGISTER, CONFIGURE, DISCOVER, UNREGISTER, SAVE, LOAD, FAIL, RESETFAILED, TEST, ENABLE, DISABLE, SET, AGENT, PATH, SKILLS, LOG, VECTOR, MEMORY, VAULT, TEAM, WORKFLOW, CONTEXT or OBSERVE after 'AI'.")        };
    }

    /// <summary>Dispatcher del comando legacy multi-parola "LLM ..." (namespace LLM, hard move
    /// AI->LLM). ADDITIVO: riusa i sottoparser dei verbi di AI con expectedNamespace="LLM"; fino
    /// alla pulizia della StdLib i simboli vivono ancora in AI, quindi questo NON rompe nulla.
    /// Verbi: MODEL/FINETUNE/LORA/BENCHMARK/PREFILL/DECODE/CACHE/SERVER (layer motore/serving).</summary>
    public static Stmt ParseLlmStatement(Parser parser, string expectedNamespace)
    {
        parser.Consume(TokenKind.Llm, "Internal parser error: Expected 'LLM' keyword.");
        var commandToken = parser.Peek();
        // §4.10 SEMAPRAX write-side: `LLM PATCH "target", "rev" … END PATCH` (PATCH = Identifier).
        if (commandToken.Kind == TokenKind.Identifier &&
            string.Equals(commandToken.Lexeme, "PATCH", StringComparison.OrdinalIgnoreCase))
            return ParseAiPatchStatement(parser, expectedNamespace);
        // B2: alias TOP-LEVEL del tokenizer/chat-template → `LLM TENSOR <verbo> ...` (riusa il
        // dispatch esistente: nessun nuovo TokenKind/registry; il verbo diventa il 1° argomento).
        if (commandToken.Kind == TokenKind.Identifier && IsLlmTensorAlias(commandToken.Lexeme))
            return ParseLlmTensorAlias(parser);
        return commandToken.Kind switch
        {
            TokenKind.AiSubModel => ParseAiModelStatement(parser, expectedNamespace),
            // [HARD MOVE TRAIN 2026-10-02] FINETUNE/LORA/QUALITY (training/produzione pesi) spostati
            // nel namespace TRAIN: qui non sono più validi (verbi di serving/motore restano).
            TokenKind.AiSubBenchmark => ParseAiBenchmarkStatement(parser, expectedNamespace),
            TokenKind.AiSubPrefill => ParseAiPrefillStatement(parser, expectedNamespace),
            TokenKind.AiSubDecode => ParseAiDecodeStatement(parser, expectedNamespace),
            TokenKind.Cache => ParseAiCacheStatement(parser, expectedNamespace),
            TokenKind.Server => ParseAiServerStatement(parser, expectedNamespace),
            // Model compiler (LLVM) nel namespace LLM. ESEGUIBILE A RUNTIME su tutti i backend
            // (2026-09-23): il verbo diventa un BuiltinFunctionCallExpr (LLMPLAN/…/LLMARCH) che
            // ogni backend risolve al proprio runtime (C: runtime condiviso llmmodelio_model_*; C#:
            // processo esterno / libreria nativa). Mai stub silenziosi: un backend che non lo
            // supporta DEVE dare errore esplicito (§0bis).
            TokenKind.LlmPlan or TokenKind.LlmPolicy or TokenKind.LlmQuant or TokenKind.LlmMerge
                or TokenKind.LlmProfile or TokenKind.Prune or TokenKind.Extract or TokenKind.LlmArch
                or TokenKind.LlmTensor or TokenKind.LlmKernel or TokenKind.LlmModule or TokenKind.LlmKv
                or TokenKind.Compile
                => ParseLlmCompilerCall(parser),
            // [HARD MOVE TRAIN 2026-10-02] FINETUNE/LORA/QUALITY → namespace TRAIN.
            _ => throw Parser.CreateError(commandToken,
                "Expected MODEL, BENCHMARK, PREFILL, DECODE, CACHE, SERVER, PLAN, POLICY, TENSOR, KERNEL, PROFILE, MODULE, QUANT, KV, MERGE, GRAPH or COMPILE after 'LLM'. (FINETUNE/LORA/QUALITY sono ora in TRAIN.)")
        };
    }

    /// <summary>Alias TOP-LEVEL dei sotto-verbi LLM TENSOR (tokenizer/chat-template) — B2.
    /// Non hanno un TokenKind dedicato: vengono riscritti a `LLM TENSOR <verbo>` dal parser.</summary>
    private static bool IsLlmTensorAlias(string lexeme) => lexeme.ToUpperInvariant() switch
    {
        "TOKENIZE" or "TOKENS" or "DETOKENIZE" or "DETOK" or "VOCAB" or "TOKENIZER" or "TOKINFO"
            or "TEMPLATE" or "CHATTEMPLATE" or "TEMPLATETEST" or "APPLYTEMPLATE" or "CHATTEST"
            or "REGISTRY" or "CALIBRATE" or "ACTIVATIONSTATS" or "QUANTERROR" or "COMPARE" or "DENSE" or "DERIVE"
            or "MAKEPATCH" or "APPLYPATCH" => true,
        _ => false,
    };

    /// <summary>TokenKind della versione-FUNZIONE di una famiglia LLM (null = famiglia senza
    /// forma a valore: il comando resta a stampa). `LLM … INTO` usa la stessa mappa.</summary>
    private static TokenKind? LlmFamilyOutKind(TokenKind verb) => verb switch
    {
        TokenKind.LlmTensor => TokenKind.LlmTensorValue,
        TokenKind.LlmPolicy => TokenKind.LlmPolicyOut,
        TokenKind.LlmArch => TokenKind.LlmArchOut,
        TokenKind.LlmModule or TokenKind.GmbModule => TokenKind.LlmModuleOut,
        TokenKind.LlmKernel => TokenKind.LlmKernelOut,
        TokenKind.LlmKv => TokenKind.LlmKvOut,
        _ => null,
    };

    /// <summary>`LLM TOKENIZE m, x` → `LLM TENSOR TOKENIZE m, x` (stesso dispatch; verbo = 1° arg).</summary>
    private static Stmt ParseLlmTensorAlias(Parser parser)
    {
        var verb = parser.Peek();
        parser.Advance();
        var nsTok = new Token(TokenKind.Identifier, "LLM", null, verb.Position);
        var args = ImmutableArray.CreateBuilder<Expr>();
        args.Add(new StringExpr(verb.Lexeme.ToUpperInvariant(), verb));
        while (!parser.IsAtEndOfStatement() && !parser.Check(TokenKind.Eof))
        {
            if (parser.Check(TokenKind.Into)) break;   // `… INTO <target>`: fine degli argomenti
            if (parser.Match(TokenKind.Comma)) continue;
            var beforePos = parser.Peek().Position;
            args.Add(ParseAiModelArg(parser));
            if (parser.Peek().Position == beforePos) break;
        }
        var list = args.ToImmutable();
        // INTO <target>: il RISULTATO va nella variabile (niente stampa). Le due forme dedicate
        // (TOKENS/DETOKENIZE) restituiscono il dato pulito; ogni altro sotto-verbo passa dalla
        // forma generica LLM.TENSOR(sub, …) che restituisce l'output catturato.
        if (parser.Match(TokenKind.Into))
        {
            var target = parser.ParseTarget();
            var sub = verb.Lexeme.ToUpperInvariant();
            TokenKind vk; ImmutableArray<Expr> vargs;
            if (sub is "TOKENS" or "TOKENIZE")
            {
                vk = TokenKind.LlmTokens;
                vargs = ImmutableArray.Create(list.Length > 1 ? list[1] : new StringExpr("", verb),
                                              list.Length > 2 ? list[2] : new StringExpr("", verb));
            }
            else if (sub is "DETOKENIZE" or "DETOK")
            {
                vk = TokenKind.LlmDetokenize;
                vargs = ImmutableArray.Create(list.Length > 1 ? list[1] : new StringExpr("", verb),
                                              list.Length > 2 ? list[2] : new StringExpr("", verb));
            }
            else
            {
                vk = TokenKind.LlmTensorValue;
                vargs = list;
            }
            var vcall = new BuiltinFunctionCallExpr(nsTok, new Token(vk, "TENSOR", null, verb.Position),
                vargs, new StringType());
            return new LetStmt([new Assignment(target, vcall)], null);
        }
        var call = new BuiltinFunctionCallExpr(nsTok, new Token(TokenKind.LlmTensor, "TENSOR", null, verb.Position),
            list, new StringType());
        return new ExpressionStmt(call);
    }

    /// <summary>`AI REPLAY AUTO "<sessione>"` — riesegue AUTOMATICAMENTE, in ordine, gli eventi
    /// registrati nella sessione: ogni evento è valutato dal valutatore runtime condiviso e l'esito
    /// è registrato come evento `"ok: …"` / `"error: …"` nella sessione `"<sessione>#auto"`
    /// (leggibile con `AI.REPLAYGET`). Mai silenzioso: gli eventi falliti restano tracciati.</summary>
    public static Stmt ParseAiReplayStatement(Parser parser)
    {
        var tok = parser.Consume(TokenKind.Replay, "Expected 'REPLAY' after 'AI'.");
        parser.Consume(TokenKind.Auto, "Expected AUTO after 'AI REPLAY'.");
        var session = parser.ParseExpression();
        var nsTok = new Token(TokenKind.Identifier, "AI", null, tok.Position);
        var nameTok = new Token(TokenKind.AiReplayAuto, "REPLAYAUTO", null, tok.Position);
        return new ExpressionStmt(new BuiltinFunctionCallExpr(nsTok, nameTok,
            ImmutableArray.Create(session), new DoubleType()));
    }

    // AI VERIFY <attemptExpr> REQUIRE <requireExpr> [INTO <target>] — comando (b) 2026-09-09:
    // verifier/retry col REQUIRE valutato sul risultato del tentativo (EVAL con locali del
    // chiamante + RESULT$). attemptExpr produce l'azione (es. "COMPILE <path>"); requireExpr
    // produce l'espressione BASIC di validazione. Esito su variabile (INTO) o discusso.
    public static Stmt ParseAiVerifyStatement(Parser parser, string expectedNamespace)
    {
        var tok = parser.Consume(TokenKind.Verify, "Expected 'VERIFY' after 'AI'.");
        Expr attempt = parser.ParseExpression();
        parser.Consume(TokenKind.Require, "Expected 'REQUIRE' after AI VERIFY attempt.");
        Expr require = parser.ParseExpression();
        var nsTok = new Token(TokenKind.Identifier, "AI", null, tok.Position);
        var nameTok = new Token(TokenKind.AiVerify, "VERIFY", null, tok.Position);
        var call = new BuiltinFunctionCallExpr(nsTok, nameTok,
            ImmutableArray.Create(attempt, require), new StringType());
        if (parser.Match(TokenKind.Into))
        {
            var target = parser.ParseTarget();
            return new LetStmt([new Assignment(target, call)], null);
        }
        return new ExpressionStmt(call);
    }

    // AI OBSERVE <SCREEN|FILE|LOG> <target> [, <tail>] [INTO <v>] — P1 todo AI-Runtime
    // consolidato (context grounding). COMANDO d'azione: registra il ContextObject
    // (id/type/source/artifact/hash/size) nell'ObservationRegistry e ne ritorna l'id
    // (scartato o INTO). Nel context entra SOLO il riferimento (id+hash), mai il contenuto.
    // SCREEN accetta il path di uscita ("" = default del provider); tail di LOG default 200.
    public static Stmt ParseAiObserveStatement(Parser parser, string expectedNamespace)
    {
        var tok = parser.Consume(TokenKind.AiSubObserve, "Expected 'OBSERVE' after 'AI'.");
        var src = parser.Peek();
        TokenKind nameKind; string nameLex;
        switch (src.Kind)
        {
            case TokenKind.Screen:
                nameKind = TokenKind.AiObserveScreen; nameLex = "OBSERVESCREEN";
                break;
            case TokenKind.File:
                nameKind = TokenKind.AiObserveFile; nameLex = "OBSERVEFILE";
                break;
            case TokenKind.Log:
                nameKind = TokenKind.AiObserveLog; nameLex = "OBSERVELOG";
                break;
            // A5: fonti di fase 2.
            case TokenKind.AiSubTest:
                nameKind = TokenKind.AiObserveTest; nameLex = "OBSERVETEST";
                break;
            case TokenKind.Database:
                nameKind = TokenKind.AiObserveDatabase; nameLex = "OBSERVEDATABASE";
                break;
            case TokenKind.Process:
                nameKind = TokenKind.AiObserveProcess; nameLex = "OBSERVEPROCESS";
                break;
            case TokenKind.Browser:
                nameKind = TokenKind.AiObserveBrowser; nameLex = "OBSERVEBROWSER";
                break;
            default:
                throw Parser.CreateError(src, "Expected SCREEN, FILE, LOG, TEST, DATABASE, PROCESS or BROWSER after 'AI OBSERVE'.");
        }
        parser.Consume(src.Kind, "Expected source after 'AI OBSERVE'.");
        ImmutableArray<Expr> callArgs;
        if (nameKind == TokenKind.AiObserveScreen)
            callArgs = parser.Check(TokenKind.Eof) ? ImmutableArray.Create<Expr>(new StringExpr("", tok))
                : ImmutableArray.Create(parser.ParseExpression());
        else if (nameKind == TokenKind.AiObserveLog)
        {
            var path = parser.ParseExpression();
            Expr tail;
            if (parser.Match(TokenKind.Comma)) tail = parser.ParseExpression();
            else tail = new NumberExpr(200, tok);
            callArgs = ImmutableArray.Create(path, tail);
        }
        else if (nameKind == TokenKind.AiObserveDatabase)
        {
            // AI OBSERVE DATABASE "<path>"[, "<query>"] — query opzionale (default: elenco tabelle).
            var path = parser.ParseExpression();
            Expr query = parser.Match(TokenKind.Comma) ? parser.ParseExpression() : new StringExpr("", tok);
            callArgs = ImmutableArray.Create(path, query);
        }
        else
            callArgs = ImmutableArray.Create(parser.ParseExpression());
        var nsTok = new Token(TokenKind.Identifier, "AI", null, tok.Position);
        var nameTok = new Token(nameKind, nameLex, null, tok.Position);
        var call = new BuiltinFunctionCallExpr(nsTok, nameTok, callArgs, new DoubleType());
        if (parser.Match(TokenKind.Into))
        {
            var target = parser.ParseTarget();
            return new LetStmt([new Assignment(target, call)], null);
        }
        return new ExpressionStmt(call);
    }

    // AI WORKER NEW name, {opts} | AI WORKER RUN name | AI WORKER name (P3): worker + workspace.
    // AI STATE SAVE worker, key, value | AI STATE LOAD worker, key (P3): state per-worker.
    public static Stmt ParseAiWorkerStatement(Parser parser, string expectedNamespace)
    {
        parser.Consume(TokenKind.AiSubWorker, "Expected 'WORKER' after 'AI'.");
        if (parser.Match(TokenKind.New))
            return ParseAiVectorCommandArgs(parser, TokenKind.AiWorkerNew, 2);
        if (parser.Match(TokenKind.Run))
            return ParseAiVectorCommandArgs(parser, TokenKind.AiWorkerRun, 1);
        // `AI WORKER MANAGE [, cycles] [REQUIRE "<expr>"]` = MANAGER LOOP sul task graph.
        if (parser.Match(TokenKind.Manage))
        {
            var manTok = parser.Previous();
            // `AI WORKER MANAGE ["<plan>"] [, cycles] [REQUIRE "<expr>"]`: il primo argomento è il
            // NOME DEL PIANO se è un literal stringa (scoping su AI.PLAN*), altrimenti sono i cicli.
            Expr manPlan = new StringExpr("", manTok);
            if (parser.Check(TokenKind.StringLiteral)) manPlan = parser.ParseExpression();
            Expr manCycles = parser.Match(TokenKind.Comma) ? parser.ParseExpression() : new NumberExpr(0, manTok);
            Expr manRequire = parser.Match(TokenKind.Require) ? parser.ParseExpression() : new StringExpr("", manTok);
            return new ExpressionStmt(MakeAiCommand(manTok, TokenKind.AiWorkerManage,
                new[] { manCycles, manRequire, manPlan }));
        }
        // `AI WORKER LOOP "<TASK>"[, rounds] [REQUIRE "<expr>"]` = loop isolato multi-round
        // (budget cumulativo) con AUDIT indipendente opzionale. Sempre 3 argomenti verso il
        // runtime: (task, rounds, require) con require = "" quando la clausola è assente
        // (stessa convenzione di AI VERIFY: la REQUIRE è una STRINGA con l'espressione da
        // valutare a runtime con RESULT$ = risposta del round).
        if (parser.Match(TokenKind.Loop))
        {
            var loopTok = parser.Previous();
            var task = parser.ParseExpression();
            Expr rounds = parser.Match(TokenKind.Comma) ? parser.ParseExpression() : new NumberExpr(0, loopTok);
            Expr require = parser.Match(TokenKind.Require) ? parser.ParseExpression() : new StringExpr("", loopTok);
            return new ExpressionStmt(MakeAiCommand(loopTok, TokenKind.AiWorkerLoop,
                new[] { task, rounds, require }));
        }
        // `AI WORKER "<TASK>"` (spec §2.1) = INVOCAZIONE PULITA sul worker corrente: un solo
        // argomento (il task/fase). Dispatch su TokenKind, mai sul lexeme.
        return ParseAiVectorCommandArgs(parser, TokenKind.AiWorkerInvoke, 1);
    }

    // AI STATE SAVE worker, key, value | AI STATE LOAD worker, key (P3): state per-worker.
    public static Stmt ParseAiStateStatement(Parser parser, string expectedNamespace)
    {
        parser.Consume(TokenKind.AiSubState, "Expected 'STATE' after 'AI'.");
        if (parser.Match(TokenKind.Save))
            return ParseAiVectorCommandArgs(parser, TokenKind.AiStateSave, 3);
        parser.Consume(TokenKind.Load, "Expected 'SAVE' or 'LOAD' after 'AI STATE'.");
        return ParseAiVectorCommandArgs(parser, TokenKind.AiStateLoad, 2);
    }

    // AI CONTEXT SET BUDGET|SELECTOR|POLICY agent, value | AI CONTEXT VIEW agent, text (P2):
    // Context Engine — configura budget, selector, policy o imposta la vista curata.
    public static Stmt ParseAiContextStatement(Parser parser, string expectedNamespace)
    {
        var tok = parser.Consume(TokenKind.Context, "Expected 'CONTEXT' after 'AI'.");
if (parser.Match(TokenKind.Set))
        {
            // P2: AI CONTEXT SET BUDGET agent, chars — budget in caratteri.
            // SELECTOR/POLICY: fase 2 (P3-P4), sub-verb da aggiungere quando servono.
            if (parser.Match(TokenKind.Budget))
                return ParseAiVectorCommandArgs(parser, TokenKind.AiContextSet, 2);
            // POLICY/SELECTOR: sub-verbo risolto QUI in un TokenKind canonico (l'AST e il backend
            // dispatchano su TokenKind, mai sul lexeme). Non sono keyword del linguaggio: POLICY e
            // SELECTOR restano identificatori validi (`FUNCTION F(selector AS STRING)` esiste).
            var sub = parser.Peek();
            // POLICY è anche keyword del lessico (LlmPolicy): accettalo OLTRE all'identificatore
            // (context_engine_policy/worker_*: `AI CONTEXT SET POLICY a, "truncate"`).
            if (sub.Kind == TokenKind.Identifier || sub.Kind == TokenKind.LlmPolicy)
            {
                if (sub.Lexeme.Equals("POLICY", System.StringComparison.OrdinalIgnoreCase))
                {
                    parser.Advance();
                    return ParseAiVectorCommandArgs(parser, TokenKind.AiContextSetPolicy, 2);
                }
                if (sub.Lexeme.Equals("SELECTOR", System.StringComparison.OrdinalIgnoreCase))
                {
                    parser.Advance();
                    return ParseAiVectorCommandArgs(parser, TokenKind.AiContextSetSelector, 2);
                }
            }
            throw CreateError(sub, "Expected BUDGET, POLICY or SELECTOR after 'AI CONTEXT SET'.");
        }
        if (parser.Match(TokenKind.View))
            return ParseAiVectorCommandArgs(parser, TokenKind.AiContextViewCmd, 2);
        // §3.6 PARSEC (lato AGENTE): sub-verbi CREATE/ADD/MAP/TRIM/GC/COMMIT. Risolti dal LESSEMA
        // (come POLICY/SELECTOR) in TokenKind canonici — `COMMIT` è anche keyword del lessico.
        {
            var sv = parser.Peek().Lexeme?.ToUpperInvariant() ?? "";
            switch (sv)
            {
                case "CREATE": parser.Advance(); return ParseAiVectorCommandArgs(parser, TokenKind.AiContextCreate, 1);
                case "ADD":    parser.Advance(); return ParseAiVectorCommandArgs(parser, TokenKind.AiContextAdd, 3);
                case "MAP":    parser.Advance(); return ParseAiVectorCommandArgs(parser, TokenKind.AiContextMap, 1);
                case "TRIM":   parser.Advance(); return ParseAiVectorCommandArgs(parser, TokenKind.AiContextTrim, 1);
                case "GC":     parser.Advance(); return ParseAiVectorCommandArgs(parser, TokenKind.AiContextGc, 1);
                case "COMMIT": parser.Advance(); return ParseAiVectorCommandArgs(parser, TokenKind.AiContextCommit, 1);
            }
        }
        throw CreateError(parser.Peek(), "Expected SET, VIEW, CREATE, ADD, MAP, TRIM, GC or COMMIT after 'AI CONTEXT'.");
    }

    // AI ASKCHAIN <name>, <prompt>, <prefix> [INTO <target>] — comando (T1.2, 2026-09-09):
    // un anello del ragionamento concatenato (Blackboard-as-Artifacts): legge <prefix>_state,
    // compone il prompt col riassunto, genera (history-free = reset implicito) e appende
    // l'artefatto. Esito (la risposta dell'anello) su variabile (INTO) o scartato.
    public static Stmt ParseAiAskChainStatement(Parser parser, string expectedNamespace)
    {
        var tok = parser.Consume(TokenKind.AiAskChain, "Expected 'ASKCHAIN' after 'AI'.");
        Expr name = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after agent name.");
        Expr prompt = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after prompt.");
        Expr prefix = parser.ParseExpression();
        var nsTok = new Token(TokenKind.Identifier, "AI", null, tok.Position);
        var nameTok = new Token(TokenKind.AiAskChain, "ASKCHAIN", null, tok.Position);
        var call = new BuiltinFunctionCallExpr(nsTok, nameTok,
            ImmutableArray.Create(name, prompt, prefix), new StringType());
        if (parser.Match(TokenKind.Into))
        {
            var target = parser.ParseTarget();
            return new LetStmt([new Assignment(target, call)], null);
        }
        return new ExpressionStmt(call);
    }

    /// <summary>Forma batch di AI DECIDE (una sola `FROM <stato>`, N domande):
    /// <code>
    /// AI DECIDE
    ///     fraud    AS NOUL
    ///     priority AS SCORE
    ///     route    AS CHOICE ("billing", "technical", "sales")
    /// FROM transaction
    /// USING "local"          ' opzionale (default rules)
    /// END DECIDE
    /// </code>
    /// v1: DESUGAR in N assegnazioni sequenziali con lo stesso builtin DECIDE (semantica
    /// batch completa); la fusione prefill/KV (`DECISION_GROUP`) è l'ottimizzazione successiva,
    /// non un cambio di semantica. I nomi delle domande SONO le variabili di destinazione.</summary>
    private static Stmt ParseAiDecideBatch(Parser parser, Token decideTok)
    {
        var questions = new List<(Target Target, Expr Kind)>();
        SkipNewlines(parser);
        while (!parser.Check(TokenKind.From))
        {
            if (parser.Check(TokenKind.End))
                throw Parser.CreateError(parser.Peek(),
                    "AI DECIDE batch: manca FROM <stato> prima di END DECIDE.");
            if (parser.Check(TokenKind.Eof))
                throw Parser.CreateError(parser.Peek(),
                    "AI DECIDE batch: manca FROM <stato> / END DECIDE.");
            var target = parser.ParseTarget();
            parser.Consume(TokenKind.As, "Expected AS after the question name in AI DECIDE batch.");
            var kind = ParseAiDecideBatchKind(parser);
            questions.Add((target, kind));
            SkipNewlines(parser);
        }
        parser.Consume(TokenKind.From, "Expected FROM after the questions in AI DECIDE batch.");
        var state = parser.ParseExpression();
        SkipNewlines(parser);
        Expr? provider = null;
        if (parser.Match(TokenKind.Using))
        {
            provider = parser.ParseExpression();
            SkipNewlines(parser);
        }
        if (parser.Check(TokenKind.Into))
            throw Parser.CreateError(parser.Peek(),
                "AI DECIDE batch: INTO non è supportato; i nomi delle domande sono le destinazioni.");
        parser.Consume(TokenKind.End, "Expected END DECIDE at the end of the batch.");
        var endName = parser.Peek();
        if (endName.Kind != TokenKind.Identifier
            || !endName.Lexeme.Equals("DECIDE", StringComparison.OrdinalIgnoreCase))
            throw Parser.CreateError(endName, "Expected DECIDE after END.");
        parser.Advance();

        var nsTok = new Token(TokenKind.Identifier, "AI", null, decideTok.Position);
        var nameTok = new Token(TokenKind.AiDecide, "DECIDE", null, decideTok.Position);
        var assigns = ImmutableArray.CreateBuilder<Assignment>(questions.Count);
        foreach (var (target, kind) in questions)
        {
            var args = provider is null
                ? ImmutableArray.Create(state, kind)
                : ImmutableArray.Create(state, kind, provider);
            var call = new BuiltinFunctionCallExpr(nsTok, nameTok, args, new StringType());
            Expr value = call;
            if (target is VariableTarget vt && parser._declaredJsonNames.Contains(vt.NameCleaned.Lexeme))
            {
                var parseNs = new Token(TokenKind.Identifier, "JSON", null, decideTok.Position);
                var parseName = new Token(TokenKind.Parse, "PARSE", null, decideTok.Position);
                value = new BuiltinFunctionCallExpr(parseNs, parseName,
                    ImmutableArray.Create<Expr>(call), new JsonType());
            }
            assigns.Add(new Assignment(target, value));
        }
        return new LetStmt(assigns.ToImmutable(), null);
    }

    /// <summary>Kind di una domanda batch: NOUL, SCORE (rubrica solo 0..5 per ora), CHOICE
    /// con etichette stringa opzionali. Ritorna la stringa-kind già nel formato del builtin
    /// (`"NOUL"`, `"SCORE"`, `"CHOICE"`, `"CHOICE:a|b"`).</summary>
    private static Expr ParseAiDecideBatchKind(Parser parser)
    {
        var t = parser.Peek();
        if (t.Kind != TokenKind.Identifier)
            throw Parser.CreateError(t, "AI DECIDE batch: expected NOUL, SCORE or CHOICE after AS.");
        parser.Advance();
        var name = t.Lexeme.ToUpperInvariant();
        Expr kind;
        switch (name)
        {
            case "NOUL":
                kind = new StringExpr("NOUL", t);
                break;
            case "SCORE":
                if (parser.Check(TokenKind.Number))
                {
                    var lo = parser.Advance();
                    parser.Consume(TokenKind.To, "Expected TO after the SCORE minimum.");
                    var hi = parser.Consume(TokenKind.Number, "Expected the SCORE maximum after TO.");
                    if (lo.Literal is not double l || hi.Literal is not double h || l != 0 || h != 5)
                        throw Parser.CreateError(lo,
                            "AI DECIDE batch: SCORE supports only the 0 TO 5 rubric (the 0..10 projection is not implemented yet).");
                }
                kind = new StringExpr("SCORE", t);
                break;
            case "CHOICE":
            {
                var labels = new List<string>();
                if (parser.Match(TokenKind.LeftParen))
                {
                    if (!parser.Check(TokenKind.RightParen))
                    {
                        do
                        {
                            var s = parser.Advance();
                            if (s.Kind != TokenKind.StringLiteral)
                                throw Parser.CreateError(s, "AI DECIDE batch: CHOICE expects string labels.");
                            labels.Add((string)s.Literal!);
                        } while (parser.Match(TokenKind.Comma));
                    }
                    parser.Consume(TokenKind.RightParen, "Expected ')' after the CHOICE labels.");
                }
                kind = new StringExpr(labels.Count > 0 ? "CHOICE:" + string.Join("|", labels) : "CHOICE", t);
                break;
            }
            default:
                throw Parser.CreateError(t,
                    $"AI DECIDE batch: kind '{t.Lexeme}' non valido (NOUL/SCORE/CHOICE).");
        }
        if (!parser.Check(TokenKind.Newline) && !parser.Check(TokenKind.From)
            && !parser.Check(TokenKind.Eof) && !parser.Check(TokenKind.Colon))
            throw Parser.CreateError(parser.Peek(),
                "AI DECIDE batch: unexpected token after the question kind.");
        return kind;
    }

    // AI EVAL VALIDATE <resultExpr> AGAINST [VAULT] <folderExpr> [INTO <target>] — comando
    // (T4.1, 2026-09-09): UAT automatica via vault. Motore VaultValidator (termini, no LLM);
    // esito JSON {"ok":bool,...} su variabile (INTO) o scartato.
    /// <summary>AI DECIDE &lt;state&gt;, "NOUL|SCORE|CHOICE" [INTO target] — v0 del decision layer:
    /// chiama il builtin DECIDE (provider Rules deterministico) e restituisce il JSON del risultato.
    /// La forma tipizzata `DECISION&lt;T&gt;` e il provider locale MLX sono il passo successivo.</summary>
    public static Stmt ParseAiDecideStatement(Parser parser, string expectedNamespace)
    {
        var tok = parser.Advance();   // DECIDE (Identifier: non è una keyword lessicale)
        // Forma BATCH: `AI DECIDE` a fine riga (canonica §5.1 del piano JEV). Le domande
        // seguono una per riga (`nome AS NOUL|SCORE|CHOICE(...)`), poi `FROM <stato>`,
        // `USING <provider>` opzionale e `END DECIDE`.
        if (parser.Check(TokenKind.Newline) || parser.Check(TokenKind.Colon))
            return ParseAiDecideBatch(parser, tok);
        Expr state = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after AI DECIDE state.");
        Expr kind = parser.ParseExpression();
        // [ , "<provider>" ] — selezione esplicita del provider (rules/mock in v0).
        // Forma a comma (non `USING`): il parser delle espressioni non ha un delimite
        // soft-keyword e `USING` verrebbe inghiottito come operando.
        Expr? provider = null;
        if (parser.Match(TokenKind.Comma))
            provider = parser.ParseExpression();
        var nsTok = new Token(TokenKind.Identifier, "AI", null, tok.Position);
        var nameTok = new Token(TokenKind.AiDecide, "DECIDE", null, tok.Position);
        var args = provider is null
            ? ImmutableArray.Create(state, kind)
            : ImmutableArray.Create(state, kind, provider);
        var call = new BuiltinFunctionCallExpr(nsTok, nameTok, args, new StringType());
        if (parser.Match(TokenKind.Into))
        {
            var target = parser.ParseTarget();
            Expr value = call;
            // Target dichiarato AS JSON/DECISION: il builtin restituisce il TESTO JSON,
            // l'assegnazione a un oggetto JSON va fatta con JSON.PARSE, così i membri
            // .VALUE/.PROBABILITY/.CONFIDENCE sono leggibili (IL/PCode; C già lo faceva).
            if (target is VariableTarget vt && parser._declaredJsonNames.Contains(vt.NameCleaned.Lexeme))
            {
                var parseNs = new Token(TokenKind.Identifier, "JSON", null, tok.Position);
                var parseName = new Token(TokenKind.Parse, "PARSE", null, tok.Position);
                value = new BuiltinFunctionCallExpr(parseNs, parseName,
                    ImmutableArray.Create<Expr>(call), new JsonType());
            }
            return new LetStmt([new Assignment(target, value)], null);
        }
        return new ExpressionStmt(call);
    }

    public static Stmt ParseAiEvalValidateStatement(Parser parser, string expectedNamespace)
    {
        var tok = parser.Consume(TokenKind.Eval, "Expected 'EVAL' after 'AI'.");
        parser.Consume(TokenKind.Validate, "Expected 'VALIDATE' after 'AI EVAL'.");
        Expr result = parser.ParseExpression();
        // Clausola 'AGAINST [VAULT]' (AGAINST non è una keyword di linguaggio: è una parola
        // guida della superficie, accettata come identifier case-insensitive).
        if (!(parser.Peek().Kind == TokenKind.Identifier
              && parser.Peek().Lexeme.Equals("AGAINST", System.StringComparison.OrdinalIgnoreCase)))
            throw Parser.CreateError(parser.Peek(), "Expected 'AGAINST' in AI EVAL VALIDATE.");
        parser.Advance();
        if (parser.Match(TokenKind.Vault)) { /* parola chiave opzionale */ }
        Expr folder = parser.ParseExpression();
        var nsTok = new Token(TokenKind.Identifier, "AI", null, tok.Position);
        var nameTok = new Token(TokenKind.AiEvalValidate, "EVALVALIDATE", null, tok.Position);
        var call = new BuiltinFunctionCallExpr(nsTok, nameTok,
            ImmutableArray.Create(result, folder), new StringType());
        if (parser.Match(TokenKind.Into))
        {
            var target = parser.ParseTarget();
            return new LetStmt([new Assignment(target, call)], null);
        }
        return new ExpressionStmt(call);
    }

    /// <summary>AI WORKFLOW — sub-dominio (ex namespace WORKFLOW fuso in AI 2026-08-28):
    /// `AI WORKFLOW <nome>` (def) | `AI WORKFLOW STEP <nome>, <azione>` |
    /// `AI WORKFLOW EXECUTE [WORKFLOW] <nome>`. Il verbo def coincide col dominio
    /// (niente "WORKFLOW WORKFLOW"): dopo il dominio, un identificatore = def,
    /// STEP/EXECUTE = i comandi di passo/esecuzione.</summary>
    private static Stmt ParseAiWorkflowStatement(Parser parser, string expectedNamespace)
    {
        parser.Consume(TokenKind.WorkflowStatement, "Expected 'WORKFLOW' after 'AI'.");
        var next = parser.Peek();
        switch (next.Kind)
        {
            case TokenKind.Step:
                return Parser.ParseWorkflowStepStatement(parser, expectedNamespace);
            case TokenKind.WorkflowExecute:
                return Parser.ParseWorkflowExecuteStatement(parser, expectedNamespace);
            default:
                // AI WORKFLOW <nome> (def): il dominio WORKFLOW è già consumato → solo il nome.
                var name = parser.Consume(TokenKind.Identifier,
                    "Expected workflow name or STEP/EXECUTE after 'AI WORKFLOW'.");
                return new WorkflowDefStmt(name, name);
        }
    }

    /// <summary>AI ENABLE name → comando: abilita un provider (TokenKind.AiOn).</summary>
    private static Stmt ParseAiEnableStatement(Parser parser)
    {
        parser.Advance(); // ENABLE
        var name = parser.ParseExpression();
        return new ExpressionStmt(MakeAiCommand(parser.Previous(), TokenKind.AiOn, [name]));
    }

    /// <summary>AI DISABLE name → comando: disabilita un provider (TokenKind.AiOff).</summary>
    private static Stmt ParseAiDisableStatement(Parser parser)
    {
        parser.Advance(); // DISABLE
        var name = parser.ParseExpression();
        return new ExpressionStmt(MakeAiCommand(parser.Previous(), TokenKind.AiOff, [name]));
    }

    /// <summary>AI SAVE CONFIG → comando: persiste la configurazione AI.</summary>
    private static Stmt ParseAiSaveConfigStatement(Parser parser, string ns)
    {
        parser.Advance(); // SAVE
        parser.Consume(TokenKind.Config, "Expected 'CONFIG' after 'AI SAVE'.");
        return new ExpressionStmt(MakeAiCommand(parser.Previous(), TokenKind.AiSaveConfig, []));
    }

    /// <summary>AI LOAD CONFIG → comando: ricarica la configurazione AI.</summary>
    private static Stmt ParseAiLoadConfigStatement(Parser parser, string ns)
    {
        parser.Advance(); // LOAD
        parser.Consume(TokenKind.Config, "Expected 'CONFIG' after 'AI LOAD'.");
        return new ExpressionStmt(MakeAiCommand(parser.Previous(), TokenKind.AiLoadConfig, []));
    }

    /// <summary>Costruisce un comando AI (BuiltinFunctionCallExpr, nessuna parentesi).</summary>
    private static BuiltinFunctionCallExpr MakeAiCommand(Token tok, TokenKind kind, Expr[] args) =>
        new(new Token(TokenKind.Identifier, "AI", null, tok.Position),
            new Token(kind, "AI", null, tok.Position), ImmutableArray.Create(args), new BitType());

    /// <summary>AI RAG ENSURE COLLECTION | INDEX TEXT|META | REMOVE SOURCE | PURGE DEPRECATED |
    /// LOAD DOCUMENTS | GENTEST — comandi (le letture restano funzioni AI.EMBED(...), AI.SEARCH(...) ...).</summary>
    /// <summary>`AI RAG …` — nuova superficie del dominio RAG (retrieval-augmented).
    /// `AI RAG …` resta come ALIAS deprecato (stesso handler). Vedi knowledge
    /// Suddivisione-Domini-AI-Namespace §6.1 (AI RAG = AI-level; il CRUD puro è di VECTORDB).</summary>
    public static Stmt ParseAiRagStatement(Parser parser, string expectedNamespace)
    {
        parser.Consume(TokenKind.Rag, "Expected 'RAG' after 'AI'.");
        return ParseAiRagSubverbs(parser, "AI RAG");
    }

    /// <summary>Sotto-verbi condivisi tra `AI RAG` (canonico) e `AI RAG` (alias).</summary>
    private static Stmt ParseAiRagSubverbs(Parser parser, string prefix)
    {
        if (parser.Match(TokenKind.Ensure))
        {
            parser.Consume(TokenKind.Collection, $"Expected 'COLLECTION' after '{prefix} ENSURE'.");
            return ParseAiVectorCommandArgs(parser, TokenKind.AiVectorEnsureCollection, 2);
        }
        if (parser.Match(TokenKind.Index))
        {
            if (parser.Match(TokenKind.Text))
                return ParseAiVectorCommandArgs(parser, TokenKind.AiVectorIndexText, 4);
            if (parser.Match(TokenKind.Meta))
                return ParseAiVectorCommandArgs(parser, TokenKind.AiVectorIndexMeta, 4);
            throw CreateError(parser.Peek(), $"Expected 'TEXT' or 'META' after '{prefix} INDEX'.");
        }
        if (parser.Match(TokenKind.Remove))
        {
            parser.Consume(TokenKind.Source, $"Expected 'SOURCE' after '{prefix} REMOVE'.");
            return ParseAiVectorCommandArgs(parser, TokenKind.AiVectorRemoveSource, 3);
        }
        if (parser.Match(TokenKind.Purge))
        {
            parser.Consume(TokenKind.Deprecated, $"Expected 'DEPRECATED' after '{prefix} PURGE'.");
            return ParseAiVectorCommandArgs(parser, TokenKind.AiVectorPurgeDeprecated, 2);
        }
        if (parser.Match(TokenKind.Load))
        {
            parser.Consume(TokenKind.Documents, $"Expected 'DOCUMENTS' after '{prefix} LOAD'.");
            return ParseAiVectorCommandArgs(parser, TokenKind.AiVectorLoadDocuments, 3);
        }
        if (parser.Match(TokenKind.GenTest))
            return ParseAiVectorCommandArgs(parser, TokenKind.AiVectorGenTest, 3);
        throw CreateError(parser.Peek(), $"Expected ENSURE, INDEX, REMOVE, PURGE, LOAD or GENTEST after '{prefix}'.");
    }

    /// <summary>Dispatcher qualificata fusa vettori: AI.VECTORENSURECOLLECTION ... (namespace AI).</summary>

    /// <summary>Legge gli argomenti (posizionali, niente parentesi) e costruisce il comando vettore.</summary>
    private static Stmt ParseAiVectorCommandArgs(Parser parser, TokenKind tokKind, int minArgs)
    {
        var args = ImmutableArray.CreateBuilder<Expr>();
        for (var i = 0; i < minArgs; i++)
        {
            if (i > 0) parser.Consume(TokenKind.Comma, "Expected ',' between arguments.");
            args.Add(parser.ParseExpression());
        }
        while (parser.Match(TokenKind.Comma)) args.Add(parser.ParseExpression());
        var tok = parser.Previous();
        return new ExpressionStmt(MakeAiCommand(tok, tokKind, args.ToArray()));
    }

    /// <summary>AI VAULT CREATE name, path [, pattern] | AI VAULT INDEX [name] — comandi del
    /// second brain L1 (vault = folder tree .md). Le letture sono le funzioni
    /// AI.VAULTINFO(name)/AI.VAULTFOLDERS(name)/AI.VAULTCOUNT(name)/AI.VAULTLIST().</summary>
    private static Stmt ParseAiVaultStatement(Parser parser)
    {
        parser.Consume(TokenKind.Vault, "Expected 'VAULT' after 'AI'.");
        if (parser.Match(TokenKind.Create))
            return ParseAiVaultCreateArgs(parser);
        if (parser.Match(TokenKind.Index))
        {
            // AI VAULT INDEX [name] — senza nome indicizza tutti i vault registrati.
            var tok = parser.Previous();
            var args = ImmutableArray.CreateBuilder<Expr>();
            if (!parser.Check(TokenKind.Eof) && IsValidNameToken(parser.Peek()))
                args.Add(ParseNameAsString(parser));
            return new ExpressionStmt(MakeAiCommand(tok, TokenKind.AiVaultIndex, args.ToArray()));
        }
        if (parser.Match(TokenKind.Watch))
        {
            // AI VAULT WATCH name, AUTO|MANUAL — policy di indicizzazione (watchdog opzionale).
            var tok = parser.Previous();
            var args = ImmutableArray.CreateBuilder<Expr>();
            args.Add(ParseNameAsString(parser));
            parser.Consume(TokenKind.Comma, "Expected ',' after vault name in AI VAULT WATCH.");
            var pol = parser.Advance();
            if (pol.Kind is not (TokenKind.Auto or TokenKind.Manual))
                throw CreateError(pol, "Expected AUTO or MANUAL after 'AI VAULT WATCH name,'.");
            args.Add(new StringExpr(pol.Lexeme.ToUpperInvariant(), pol));
            return new ExpressionStmt(MakeAiCommand(tok, TokenKind.AiVaultWatch, args.ToArray()));
        }
        if (parser.Match(TokenKind.Add))
        {
            // AI VAULT ADD FOLDER name, path [, INDEX ON|OFF]
            parser.Consume(TokenKind.Folder, "Expected 'FOLDER' after 'AI VAULT ADD'.");
            var tok = parser.Previous();
            var args = ImmutableArray.CreateBuilder<Expr>();
            args.Add(ParseNameAsString(parser)); // vault name
            parser.Consume(TokenKind.Comma, "Expected ',' after vault name in AI VAULT ADD FOLDER.");
            args.Add(ParseNameAsString(parser)); // folder path
            if (parser.Match(TokenKind.Comma))
            {
                // [INDEX ON|OFF]
                if (parser.Match(TokenKind.Index))
                {
                    var v = parser.Advance(); // ON|OFF
                    args.Add(new StringExpr(v.Kind == TokenKind.Off ? "0" : "1", v));
                }
            }
            return new ExpressionStmt(MakeAiCommand(tok, TokenKind.AiVaultAddFolder, args.ToArray()));
        }
        if (parser.Match(TokenKind.Remove))
        {
            // AI VAULT REMOVE FOLDER name, path
            parser.Consume(TokenKind.Folder, "Expected 'FOLDER' after 'AI VAULT REMOVE'.");
            var tok = parser.Previous();
            var args = ImmutableArray.CreateBuilder<Expr>();
            args.Add(ParseNameAsString(parser)); // vault name
            parser.Consume(TokenKind.Comma, "Expected ',' after vault name in AI VAULT REMOVE FOLDER.");
            args.Add(ParseNameAsString(parser)); // folder path
            return new ExpressionStmt(MakeAiCommand(tok, TokenKind.AiVaultRemoveFolder, args.ToArray()));
        }
        if (parser.Match(TokenKind.Set))
        {
            // AI VAULT SET FOLDER name, path, INDEX ON|OFF — props per-cartella (INDEX bool).
            parser.Consume(TokenKind.Folder, "Expected 'FOLDER' after 'AI VAULT SET'.");
            var tok = parser.Previous();
            var args = ImmutableArray.CreateBuilder<Expr>();
            args.Add(ParseNameAsString(parser)); // vault name
            parser.Consume(TokenKind.Comma, "Expected ',' after vault name in AI VAULT SET FOLDER.");
            args.Add(ParseNameAsString(parser)); // folder path
            parser.Consume(TokenKind.Comma, "Expected ',' after folder path in AI VAULT SET FOLDER.");
            parser.Consume(TokenKind.Index, "Expected 'INDEX' in AI VAULT SET FOLDER.");
            var v = parser.Advance(); // ON|OFF
            args.Add(new StringExpr(v.Kind == TokenKind.Off ? "0" : "1", v));
            return new ExpressionStmt(MakeAiCommand(tok, TokenKind.AiVaultSetFolderProps, args.ToArray()));
        }
        throw CreateError(parser.Peek(), "Expected CREATE, INDEX, WATCH, ADD, REMOVE or SET after 'AI VAULT'.");
    }

    /// <summary>AI VAULT CREATE name, path [, pattern] — name e path come identificatori
    /// nudi (pattern dei sample) o stringhe; pattern opzionale (default "*.md").</summary>
    private static Stmt ParseAiVaultCreateArgs(Parser parser)
    {
        var args = ImmutableArray.CreateBuilder<Expr>();
        args.Add(ParseNameAsString(parser));
        parser.Consume(TokenKind.Comma, "Expected ',' after vault name.");
        args.Add(ParseNameAsString(parser));
        if (parser.Match(TokenKind.Comma))
            args.Add(ParseNameAsString(parser));
        var tok = parser.Previous();
        return new ExpressionStmt(MakeAiCommand(tok, TokenKind.AiVaultCreate, args.ToArray()));
    }

    /// <summary>Legge il prossimo argomento: una stringa letterale o un identificatore
    /// semplice nudo (che diventa stringa, pattern dei sample AI). Se però l'identificatore
    /// è una VARIABILE DICHIARATA in scope (DIM/param), resta una variabile: prima ogni
    /// identificatore nudo diventava il letterale-NOME (44 siti) → `AI VAULT CREATE "cov", v`
    /// registrava il vault con root "v" invece del VALORE della variabile (divergenza
    /// silenziosa C#/C, bug P4).</summary>
    private static Expr ParseNameAsString(Parser parser)
    {
        var first = parser.ParseExpression();
        if (first is VariableExpr ve && ve.NameCleaned.Lexeme == ve.NameSuffixed.Lexeme
            && !parser._declaredVariableNames.Contains(ve.NameCleaned.Lexeme))
            return new StringExpr(ve.NameCleaned.Lexeme, ve.NameSuffixed);
        return first;
    }

    /// <summary>Come <see cref="ParseNameAsString"/> ma ritorna direttamente il VALORE stringa
    /// (per concatenazioni parse-time, es. AI BRAIN USE a, b → "a,b").</summary>
    private static string ParseNameStringValue(Parser parser)
    {
        var expr = ParseNameAsString(parser);
        if (expr is StringExpr se) return se.Value;
        if (expr is VariableExpr ve) return ve.NameCleaned.Lexeme;
        throw CreateError(parser.Previous(), "Expected a name (string or identifier) here.");
    }

    /// <summary>AI BRAIN USE name [, name...] | DETACH name | INSTALL name, source |
    /// UNINSTALL name — comandi del second brain (collegamento/import di cervelli GIÀ
    /// costruiti). Le letture sono le funzioni AI.BRAINLIST()/AI.BRAININFO(name)/
    /// AI.BRAINUSED().</summary>
    private static Stmt ParseAiBrainStatement(Parser parser)
    {
        parser.Consume(TokenKind.Brain, "Expected 'BRAIN' after 'AI'.");
        var verb = parser.Peek();
        switch (verb.Kind)
        {
            case TokenKind.Use:
            {
                parser.Advance(); // USE
                // AI BRAIN USE a, b, c — concatenati a parse-time in un'unica stringa
                // "a,b,c" (un solo argomento runtime: BrainUse(string names)).
                var names = new List<string>();
                names.Add(ParseNameStringValue(parser));
                while (parser.Match(TokenKind.Comma)) names.Add(ParseNameStringValue(parser));
                var tok = parser.Previous();
                var joined = new StringExpr(string.Join(",", names), tok);
                return new ExpressionStmt(MakeAiCommand(tok, TokenKind.AiBrainUse, [joined]));
            }
            case TokenKind.SqlDetach:
            {
                parser.Advance();
                var tok = parser.Previous();
                return new ExpressionStmt(MakeAiCommand(tok, TokenKind.AiBrainDetach, [ParseNameAsString(parser)]));
            }
            case TokenKind.Install:
            {
                parser.Advance(); // INSTALL
                var args = ImmutableArray.CreateBuilder<Expr>();
                args.Add(ParseNameAsString(parser));
                /* `AI BRAIN INSTALL "nome"` = installa dal MANIFEST (brains-manifest.json, con
                 * download on demand); la forma `"nome", "sorgente"` installa da URL/zip/path. */
                if (parser.Match(TokenKind.Comma))
                    args.Add(ParseNameAsString(parser));
                else
                    args.Add(new StringExpr("", parser.Previous()));
                var tok = parser.Previous();
                return new ExpressionStmt(MakeAiCommand(tok, TokenKind.AiBrainInstall, args.ToArray()));
            }
            case TokenKind.Uninstall:
            {
                parser.Advance();
                var tok = parser.Previous();
                return new ExpressionStmt(MakeAiCommand(tok, TokenKind.AiBrainUninstall, [ParseNameAsString(parser)]));
            }
            default:
                throw CreateError(verb, "Expected USE, DETACH, INSTALL or UNINSTALL after 'AI BRAIN'.");
        }
    }

    /// <summary>AI NOTE CREATE path [, content] [, TAGS "a,b"] | UPDATE path, content |
    /// APPEND path, text | DELETE path | MOVE old, new — comandi del second brain L2.
    /// Il vault è quello ATTIVO (primo brain used); le letture sono AI.NOTEGET/...</summary>
    private static Stmt ParseAiNoteStatement(Parser parser)
    {
        parser.Consume(TokenKind.Note, "Expected 'NOTE' after 'AI'.");
        var verb = parser.Peek();
        switch (verb.Kind)
        {
            case TokenKind.Create:
            {
                parser.Advance();
                var args = ImmutableArray.CreateBuilder<Expr>();
                args.Add(ParseNameAsString(parser)); // path
                while (parser.Match(TokenKind.Comma)) args.Add(ParseNameAsString(parser)); // content [, tags]
                var tok = parser.Previous();
                return new ExpressionStmt(MakeAiCommand(tok, TokenKind.NoteCreate, args.ToArray()));
            }
            case TokenKind.Update:
            {
                parser.Advance();
                var tok = parser.Previous();
                var args = ImmutableArray.CreateBuilder<Expr>();
                args.Add(ParseNameAsString(parser));
                parser.Consume(TokenKind.Comma, "Expected ',' after note path in AI NOTE UPDATE.");
                args.Add(ParseNameAsString(parser));
                return new ExpressionStmt(MakeAiCommand(tok, TokenKind.NoteUpdate, args.ToArray()));
            }
            case TokenKind.Append:
            {
                parser.Advance();
                var tok = parser.Previous();
                var args = ImmutableArray.CreateBuilder<Expr>();
                args.Add(ParseNameAsString(parser));
                parser.Consume(TokenKind.Comma, "Expected ',' after note path in AI NOTE APPEND.");
                args.Add(ParseNameAsString(parser));
                return new ExpressionStmt(MakeAiCommand(tok, TokenKind.NoteAppend, args.ToArray()));
            }
            case TokenKind.Delete:
            {
                parser.Advance();
                var tok = parser.Previous();
                return new ExpressionStmt(MakeAiCommand(tok, TokenKind.NoteDelete, [ParseNameAsString(parser)]));
            }
            case TokenKind.Move:
            {
                parser.Advance();
                var tok = parser.Previous();
                var args = ImmutableArray.CreateBuilder<Expr>();
                args.Add(ParseNameAsString(parser));
                parser.Consume(TokenKind.Comma, "Expected ',' after old path in AI NOTE MOVE.");
                args.Add(ParseNameAsString(parser));
                return new ExpressionStmt(MakeAiCommand(tok, TokenKind.NoteMove, args.ToArray()));
            }
            case TokenKind.Link:
            {
                // AI NOTE LINK from TO to [AS alias] — link manuale persistito (vault = brain attivo).
                parser.Advance();
                var tok = parser.Previous();
                var args = ImmutableArray.CreateBuilder<Expr>();
                args.Add(new StringExpr("", tok)); // vault = brain attivo
                args.Add(ParseNameAsString(parser)); // from
                parser.Consume(TokenKind.To, "Expected 'TO' in AI NOTE LINK.");
                args.Add(ParseNameAsString(parser)); // to
                if (parser.Match(TokenKind.As))
                    args.Add(ParseNameAsString(parser)); // alias
                return new ExpressionStmt(MakeAiCommand(tok, TokenKind.AiNoteLink, args.ToArray()));
            }
            case TokenKind.Unlink:
            {
                // AI NOTE UNLINK from TO to — rimuove il link manuale.
                parser.Advance();
                var tok = parser.Previous();
                var args = ImmutableArray.CreateBuilder<Expr>();
                args.Add(new StringExpr("", tok)); // vault = brain attivo
                args.Add(ParseNameAsString(parser)); // from
                parser.Consume(TokenKind.To, "Expected 'TO' in AI NOTE UNLINK.");
                args.Add(ParseNameAsString(parser)); // to
                return new ExpressionStmt(MakeAiCommand(tok, TokenKind.AiNoteUnlink, args.ToArray()));
            }
            case TokenKind.Set:
            {
                // AI NOTE SET ALIAS from TO to AS alias (l'unico verbo SET di NOTE).
                parser.Advance(); // SET
                parser.Consume(TokenKind.Alias, "Expected 'ALIAS' after 'AI NOTE SET'.");
                var tok = parser.Previous();
                var args = ImmutableArray.CreateBuilder<Expr>();
                args.Add(new StringExpr("", tok)); // vault = brain attivo
                args.Add(ParseNameAsString(parser)); // from
                parser.Consume(TokenKind.To, "Expected 'TO' in AI NOTE SET ALIAS.");
                args.Add(ParseNameAsString(parser)); // to
                parser.Consume(TokenKind.As, "Expected 'AS' in AI NOTE SET ALIAS.");
                args.Add(ParseNameAsString(parser)); // alias
                return new ExpressionStmt(MakeAiCommand(tok, TokenKind.AiNoteSetAlias, args.ToArray()));
            }
            default:
                throw CreateError(verb, "Expected CREATE, UPDATE, APPEND, DELETE, MOVE, LINK, UNLINK or SETALIAS after 'AI NOTE'.");
        }
    }

    /// <summary>AI INGEST FILE path [, rel] | AI INGEST FOLDER path [, preserve] — importa file
    /// esterni (testo/binario/OCR/audio) in una nota del vault (Second Brain L6).</summary>
    private static Stmt ParseAiIngestStatement(Parser parser)
    {
        parser.Consume(TokenKind.Ingest, "Expected 'INGEST' after 'AI'.");
        var verb = parser.Peek();
        if (verb.Kind == TokenKind.File)
        {
            parser.Advance();
            var args = ImmutableArray.CreateBuilder<Expr>();
            args.Add(ParseNameAsString(parser));
            if (parser.Match(TokenKind.Comma)) args.Add(ParseNameAsString(parser));
            var tok = parser.Previous();
            return new ExpressionStmt(MakeAiCommand(tok, TokenKind.IngestFile, args.ToArray()));
        }
        if (verb.Kind == TokenKind.Folder)
        {
            parser.Advance();
            var args = ImmutableArray.CreateBuilder<Expr>();
            args.Add(ParseNameAsString(parser));
            if (parser.Match(TokenKind.Comma)) args.Add(ParseNameAsString(parser));
            var tok = parser.Previous();
            return new ExpressionStmt(MakeAiCommand(tok, TokenKind.IngestFolder, args.ToArray()));
        }
        if (verb.Kind == TokenKind.Web)
        {
            // AI INGEST WEB url [, relPath] — scarica una pagina come nota .md (vault = brain attivo).
            parser.Advance();
            var tok = parser.Previous();
            var args = ImmutableArray.CreateBuilder<Expr>();
            args.Add(new StringExpr("", tok)); // vault = brain attivo
            args.Add(ParseNameAsString(parser));
            if (parser.Match(TokenKind.Comma)) args.Add(ParseNameAsString(parser));
            return new ExpressionStmt(MakeAiCommand(tok, TokenKind.AiIngestWeb, args.ToArray()));
        }
        throw CreateError(verb, "Expected FILE, FOLDER or WEB after 'AI INGEST'.");
    }

    /// <summary>AI SYNC FOLDER src TO vault [, direction] [, conflict] — sincronizza una
    /// cartella esterna con il vault (L6). direction: bidirectional|push_only (default
    /// bidirectional); conflict: newest_wins (default). Ritorna "importati/aggiornati/conflitti".</summary>
    private static Stmt ParseAiSyncStatement(Parser parser)
    {
        parser.Consume(TokenKind.Sync, "Expected 'SYNC' after 'AI'.");
        parser.Consume(TokenKind.Folder, "Expected 'FOLDER' after 'AI SYNC'.");
        var tok = parser.Previous();
        var args = ImmutableArray.CreateBuilder<Expr>();
        args.Add(ParseNameAsString(parser)); // src folder
        parser.Consume(TokenKind.To, "Expected 'TO' in AI SYNC FOLDER.");
        args.Add(ParseNameAsString(parser)); // vault name
        while (parser.Match(TokenKind.Comma))
            args.Add(ParseNameAsString(parser)); // opzionali: direction, conflict
        return new ExpressionStmt(MakeAiCommand(tok, TokenKind.AiSyncFolder, args.ToArray()));
    }

    /// <summary>AI MEMORY WATCH/IMPORT/NOTE/REFRESH/SAVE/FORGET — comandi del second brain.
    /// La lettura e' la funzione AI.MEMORYASK(query).</summary>
    private static Stmt ParseAiMemoryStatement(Parser parser)
    {
        parser.Consume(TokenKind.Memory, "Expected 'MEMORY' after 'AI'.");
        var verb = parser.Peek();
        var (tokKind, minArgs) = verb.Kind switch
        {
            TokenKind.Watch => (TokenKind.MemoryWatch, 1),
            TokenKind.Import => (TokenKind.MemoryImport, 1),
            TokenKind.Note => (TokenKind.MemoryNote, 1),
            TokenKind.Refresh => (TokenKind.MemoryRefresh, 0),
            TokenKind.Save => (TokenKind.MemorySave, 1),
            TokenKind.Forget => (TokenKind.MemoryForget, 1),
            _ => throw CreateError(verb, "Expected WATCH, IMPORT, NOTE, REFRESH, SAVE or FORGET after 'AI MEMORY'.")
        };
        parser.Advance();
        return ParseMemoryCommandArgs(parser, tokKind, minArgs);
    }

    private static Stmt ParseMemoryCommandArgs(Parser parser, TokenKind tokKind, int minArgs)
    {
        var args = ImmutableArray.CreateBuilder<Expr>();
        for (var i = 0; i < minArgs; i++)
        {
            if (i > 0) parser.Consume(TokenKind.Comma, "Expected ',' between arguments.");
            args.Add(parser.ParseExpression());
        }
        while (parser.Match(TokenKind.Comma)) args.Add(parser.ParseExpression());
        var tok = parser.Previous();
        return new ExpressionStmt(MakeAiCommand(tok, tokKind, args.ToArray()));
    }

    /// <summary>Legacy: AI LOG PATH <path> | AI LOG WRITE <file>, <testo> — logging agente.</summary>
    private static Stmt ParseAiLogStatement(Parser parser, string ns)
    {
        var logTok = parser.Consume(TokenKind.Log, "Expected 'LOG' after 'AI'.");
        var sub = parser.Peek();
        var nsTok = new Token(TokenKind.Identifier, "AI", null, logTok.Position);
        var args = ImmutableArray.CreateBuilder<Expr>();

        TokenKind tokKind;
        if (sub.Kind == TokenKind.Path)
        {
            parser.Advance(); // PATH
            tokKind = TokenKind.AiLogPath;
            args.Add(parser.ParseExpression());
        }
        else if (sub.Kind == TokenKind.Write)
        {
            parser.Advance(); // WRITE
            tokKind = TokenKind.AiLogWrite;
            args.Add(parser.ParseExpression());
            if (parser.Match(TokenKind.Comma))
                args.Add(parser.ParseExpression());
        }
        else if (sub.Kind == TokenKind.WriteLine)
        {
            parser.Advance(); // WRITELINE
            tokKind = TokenKind.AiLogWriteLine;
            args.Add(parser.ParseExpression());
            if (parser.Match(TokenKind.Comma))
                args.Add(parser.ParseExpression());
        }
        else
        {
            throw Parser.CreateError(sub, "Expected PATH, WRITE or WRITELINE after 'AI LOG'.");
        }

        var call = new BuiltinFunctionCallExpr(nsTok, new Token(tokKind, "LOG", null, logTok.Position),
            args.ToImmutable(), new BitType());
        return new ExpressionStmt(call);
    }

    /// <summary>Legacy: AI PATH MODEL|SKILL ADD|REMOVE <path> — gestione percorsi AI.</summary>
    private static Stmt ParseAiPathStatement(Parser parser, string ns)
    {
        parser.Consume(TokenKind.Path, "Expected 'PATH' after 'AI'.");
        var sub = parser.Peek();
        // AI PATH MODEL ... | AI PATH SKILL ...
        TokenKind kind = sub.Kind == TokenKind.AiSubModel ? TokenKind.AiSubModel
                     : sub.Kind == TokenKind.AiSubSkills ? TokenKind.AiSubSkills
                     : sub.Kind == TokenKind.AiSubPlugins ? TokenKind.AiSubPlugins
                     : throw Parser.CreateError(sub, "Expected MODEL, SKILL or PLUGIN after 'AI PATH'.");
        parser.Advance(); // consuma MODEL/SKILL

        var op = parser.Peek();
        var tokKind = (kind, op.Kind) switch
        {
            (TokenKind.AiSubModel, TokenKind.Add) => TokenKind.AiPathModelAdd,
            (TokenKind.AiSubModel, TokenKind.Remove) => TokenKind.AiPathModelRemove,
            (TokenKind.AiSubSkills, TokenKind.Add) => TokenKind.AiPathSkillAdd,
            (TokenKind.AiSubSkills, TokenKind.Remove) => TokenKind.AiPathSkillRemove,
            (TokenKind.AiSubPlugins, TokenKind.Add) => TokenKind.AiPathPluginAdd,
            (TokenKind.AiSubPlugins, TokenKind.Remove) => TokenKind.AiPathPluginRemove,
            _ => throw Parser.CreateError(op, "Expected ADD or REMOVE after 'AI PATH MODEL|SKILL|PLUGIN'.")
        };
        parser.Advance(); // consuma ADD/REMOVE
        var arg = parser.ParseExpression();
        var nsTok = new Token(TokenKind.Identifier, "AI", null, sub.Position);
        var call = new BuiltinFunctionCallExpr(nsTok, new Token(tokKind, "PATH", null, sub.Position),
            [arg], new BitType());
        return new ExpressionStmt(call);
    }

    /// <summary>Legacy: AI SKILLS <verbo> ... — gestione skills (ADD/REMOVE/REFRESH/...).</summary>
    private static Stmt ParseAiSkillsStatement(Parser parser, string ns)
    {
        var skillsTok = parser.Consume(TokenKind.AiSubSkills, "Expected 'SKILLS' after 'AI'.");
        var op = parser.Peek();
        var nsTok = new Token(TokenKind.Identifier, "AI", null, skillsTok.Position);
        // Verbi composti: AI SKILLS SET INDEX <path>
        if (op.Kind == TokenKind.Set)
        {
            parser.Advance(); // SET
            parser.Consume(TokenKind.Index, "Expected 'INDEX' after 'AI SKILLS SET'.");
            var arg = parser.ParseExpression();
            var call = new BuiltinFunctionCallExpr(nsTok,
                new Token(TokenKind.AiSkillsSetIndex, "SETINDEX", null, skillsTok.Position), [arg], new BitType());
            return new ExpressionStmt(call);
        }

        var args = ImmutableArray.CreateBuilder<Expr>();
        var tokKind = op.Kind switch
        {
            TokenKind.Add => TokenKind.AiSkillsAdd,
            TokenKind.Remove => TokenKind.AiSkillsRemove,
            TokenKind.Refresh => TokenKind.AiSkillsRefresh,
            TokenKind.AiSubMarketplace => TokenKind.AiSkillsMarketplace,
            _ => throw Parser.CreateError(op,
                $"Expected ADD, REMOVE, REFRESH, SET INDEX or MARKETPLACE after 'AI SKILLS'.")
        };
        parser.Advance(); // consuma il verbo
        while (!parser.IsAtEndOfStatement() && !parser.Check(TokenKind.Newline))
        {
            args.Add(parser.ParseExpression());
            if (!parser.Match(TokenKind.Comma)) break;
        }
        var call2 = new BuiltinFunctionCallExpr(nsTok, new Token(tokKind, "SKILLS", null, skillsTok.Position),
            args.ToImmutable(), new BitType());
        return new ExpressionStmt(call2);
    }

    /// <summary>Legacy: AI PLUGINS <verbo> ... — gestione plugin operativi
    /// (ADD/REMOVE/REFRESH/SET INDEX/LIST/ENABLE/DISABLE/MARKETPLACE/CREATE).</summary>
    private static Stmt ParseAiPluginsStatement(Parser parser, string ns)
    {
        var plugTok = parser.Consume(TokenKind.AiSubPlugins, "Expected 'PLUGINS' after 'AI'.");
        var op = parser.Peek();
        var nsTok = new Token(TokenKind.Identifier, "AI", null, plugTok.Position);
        if (op.Kind == TokenKind.Set)
        {
            parser.Advance();
            parser.Consume(TokenKind.Index, "Expected 'INDEX' after 'AI PLUGINS SET'.");
            var arg = parser.ParseExpression();
            var callSet = new BuiltinFunctionCallExpr(nsTok,
                new Token(TokenKind.AiPluginsSetIndex, "SETINDEX", null, plugTok.Position), [arg], new BitType());
            return new ExpressionStmt(callSet);
        }

        var args = ImmutableArray.CreateBuilder<Expr>();
        var tokKind = op.Kind switch
        {
            TokenKind.Add => TokenKind.AiPluginsAdd,
            TokenKind.Remove => TokenKind.AiPluginsRemove,
            TokenKind.Refresh => TokenKind.AiPluginsRefresh,
            TokenKind.List => TokenKind.AiPluginsList,
            TokenKind.Enable => TokenKind.AiPluginsEnable,
            TokenKind.Disable => TokenKind.AiPluginsDisable,
            TokenKind.AiSubMarketplace => TokenKind.AiPluginsMarketplace,
            TokenKind.Create => TokenKind.AiPluginsCreate,
            _ => throw Parser.CreateError(op,
                $"Expected ADD, REMOVE, REFRESH, SET INDEX, LIST, ENABLE, DISABLE, MARKETPLACE or CREATE after 'AI PLUGINS'.")
        };
        parser.Advance();
        while (!parser.IsAtEndOfStatement() && !parser.Check(TokenKind.Newline))
        {
            args.Add(parser.ParseExpression());
            if (!parser.Match(TokenKind.Comma)) break;
        }
        var callPlug = new BuiltinFunctionCallExpr(nsTok,
            new Token(tokKind, "PLUGINS", null, plugTok.Position), args.ToImmutable(), new BitType());
        return new ExpressionStmt(callPlug);
    }

    /// <summary>Legacy: AI AGENT <verbo> ... — agente con verbi multi-parola
    /// (REGISTER TOOL, STOP SERVER). La qualificata fonde: AI.AGENTREGISTERTOOL.</summary>
    private static Stmt ParseAiAgentStatement(Parser parser, string ns)
    {
        parser.Consume(TokenKind.Agent, "Expected 'AGENT' after 'AI'.");
        var verb = parser.Peek();

        // Verbi composti multi-parola.
        if (verb.Kind == TokenKind.Register)
        {
            parser.Advance(); // REGISTER
            parser.Consume(TokenKind.AiSubTool, "Expected 'TOOL' after 'AI AGENT REGISTER'.");
            return ParseAiAgentArgs(parser, TokenKind.AiAgentRegisterTool, 2, new BitType());
        }
        if (verb.Kind == TokenKind.Stop)
        {
            parser.Advance(); // STOP
            if (parser.Check(TokenKind.Server))
            {
                parser.Advance(); // SERVER
                return ParseAiAgentArgs(parser, TokenKind.AiAgentStopServer, 0, new BitType());
            }
            // STOP AgentName — queue stop
            return ParseAiAgentQueueOp(parser, TokenKind.AiAgentStop);
        }
        if (verb.Kind == TokenKind.Send)
        {
            parser.Advance(); // SEND
            return ParseAiAgentSendStatement(parser);
        }
        if (verb.Kind == TokenKind.Receive)
        {
            parser.Advance(); // RECEIVE
            return ParseAiAgentReceiveStatement(parser);
        }
        if (verb.Kind == TokenKind.Pause)
        {
            parser.Advance(); // PAUSE
            return ParseAiAgentQueueOp(parser, TokenKind.AiAgentPause);
        }
        if (verb.Kind == TokenKind.Resume)
        {
            parser.Advance(); // RESUME
            return ParseAiAgentQueueOp(parser, TokenKind.AiAgentResume);
        }
        if (verb.Lexeme.Equals("CODE", StringComparison.OrdinalIgnoreCase))
        {
            parser.Advance(); // CODE
            return ParseAiAgentCodeStatement(parser);
        }
        if (verb.Lexeme.Equals("FIX", StringComparison.OrdinalIgnoreCase))
        {
            parser.Advance(); // FIX
            return ParseAiAgentFixStatement(parser);
        }

        return verb.Kind switch
        {
            TokenKind.Create => ConsumeVerbThenArgs(parser, TokenKind.AiAgentCreate, 1, new BitType()),
            TokenKind.Delete => ConsumeVerbThenArgs(parser, TokenKind.AiAgentDelete, 1, new BitType()),
            TokenKind.Set => ParseAiAgentSetStatement(parser),
            TokenKind.Save => ConsumeVerbThenArgs(parser, TokenKind.AiAgentSave, 1, new BitType()),
            TokenKind.Load => ConsumeVerbThenArgs(parser, TokenKind.AiAgentLoad, 1, new BitType()),
            TokenKind.AiSubServe => ConsumeVerbThenArgs(parser, TokenKind.AiAgentServe, 1, new BitType()),
            TokenKind.AiSubTrace => ParseAiAgentTraceStatement(parser, TokenKind.AiAgentTrace),
            TokenKind.AiVectorAsk => ParseAiAgentAskStreamStatement(parser),
            _ => throw Parser.CreateError(verb,
                $"Expected CREATE, DELETE, SET, SAVE, LOAD, REGISTER TOOL, SERVE, STOP SERVER, SEND, RECEIVE, PAUSE, RESUME, TRACE or ASK after 'AI AGENT'.")
        };
    }

    private static Stmt ParseAiAgentSendStatement(Parser parser)
    {
        // AI AGENT SEND To, Task, Input [, Payment] — legacy a spazio con virgole
        var args = ImmutableArray.CreateBuilder<Expr>();
        var toExpr = parser.ParseExpression(); // To (agent name, may be identifier → string literal)
        if (toExpr is VariableExpr ve) toExpr = new StringExpr(ve.NameCleaned.Lexeme, ve.NameCleaned);
        args.Add(toExpr);
        parser.Consume(TokenKind.Comma, "Expected ',' after SEND target agent.");
        args.Add(parser.ParseExpression()); // Task
        parser.Consume(TokenKind.Comma, "Expected ',' after SEND task.");
        args.Add(parser.ParseExpression()); // Input
        if (parser.Match(TokenKind.Comma))
            args.Add(parser.ParseExpression()); // Payment optional
        else
            args.Add(new NumberExpr(0.0, new Token(TokenKind.Number, "0", 0.0, parser.Previous().Position)));
        var tok = parser.Previous();
        var nsTok = new Token(TokenKind.Identifier, "AI", null, tok.Position);
        var call = new BuiltinFunctionCallExpr(nsTok, new Token(TokenKind.AiAgentSend, "SEND", null, tok.Position), args.ToImmutable(), new BitType());
        return new ExpressionStmt(call);
    }

    private static Stmt ParseAiAgentReceiveStatement(Parser parser)
    {
        // AI AGENT RECEIVE Agent INTO Var  — legacy, con INTO
        var agentExpr = parser.ParseExpression();
        if (agentExpr is VariableExpr ve) agentExpr = new StringExpr(ve.NameCleaned.Lexeme, ve.NameCleaned);
        parser.Consume(TokenKind.Into, "Expected 'INTO' after RECEIVE agent.");
        var target = parser.ParseTarget();
        var tok = parser.Previous();
        var nsTok = new Token(TokenKind.Identifier, "AI", null, tok.Position);
        // RECEIVE come LetStmt con assignment: Var = AI.AGENTRECEIVE(Agent)
        var call = new BuiltinFunctionCallExpr(nsTok, new Token(TokenKind.AiAgentReceive, "RECEIVE", null, tok.Position), [agentExpr], new StringType());
        return new LetStmt([new Assignment(target, call)], null);
    }

    private static Stmt ParseAiAgentQueueOp(Parser parser, TokenKind kind)
    {
        var arg = parser.ParseExpression();
        if (arg is VariableExpr ve) arg = new StringExpr(ve.NameCleaned.Lexeme, ve.NameCleaned);
        var tok = parser.Previous();
        var nsTok = new Token(TokenKind.Identifier, "AI", null, tok.Position);
        var call = new BuiltinFunctionCallExpr(nsTok, new Token(kind, kind.ToString(), null, tok.Position), [arg], new BitType());
        return new ExpressionStmt(call);
    }

    private static Stmt ParseAiAgentCodeStatement(Parser parser)
    {
        var task = parser.ParseExpression();
        Expr? outPath = null;
        if (parser.Match(TokenKind.Comma)) outPath = parser.ParseExpression();
        var tok = parser.Previous();
        var nsTok = new Token(TokenKind.Identifier, "AI", null, tok.Position);
        var args = outPath != null ? new[] { task, outPath } : new[] { task };
        var call = new BuiltinFunctionCallExpr(nsTok, new Token(TokenKind.AiAgentCode, "CODE", null, tok.Position), args.ToImmutableArray(), new StringType());
        return new ExpressionStmt(call);
    }

    private static Stmt ParseAiAgentFixStatement(Parser parser)
    {
        var err = parser.ParseExpression();
        Expr? outPath = null;
        if (parser.Match(TokenKind.Comma)) outPath = parser.ParseExpression();
        var tok = parser.Previous();
        var nsTok = new Token(TokenKind.Identifier, "AI", null, tok.Position);
        var args = outPath != null ? new[] { err, outPath } : new[] { err };
        var call = new BuiltinFunctionCallExpr(nsTok, new Token(TokenKind.AiAgentFix, "FIX", null, tok.Position), args.ToImmutableArray(), new StringType());
        return new ExpressionStmt(call);
    }

    private static Stmt ParseAiChannelStatement(Parser parser, string expectedNamespace)
    {
        parser.Consume(TokenKind.Channel, "Expected 'CHANNEL' after 'AI'.");
        var verb = parser.Peek();
        if (verb.Kind == TokenKind.Create)
        {
            parser.Advance();
            var ch = parser.ParseExpression();
            if (ch is VariableExpr ve) ch = new StringExpr(ve.NameCleaned.Lexeme, ve.NameCleaned);
            var tok = parser.Previous();
            var nsTok = new Token(TokenKind.Identifier, "AI", null, tok.Position);
            var call = new BuiltinFunctionCallExpr(nsTok, new Token(TokenKind.AiChannelCreate, "CHANNELCREATE", null, tok.Position), [ch], new BitType());
            return new ExpressionStmt(call);
        }
        if (verb.Lexeme.Equals("PUBLISH", StringComparison.OrdinalIgnoreCase))
        {
            parser.Advance();
            var ch = parser.ParseExpression();
            if (ch is VariableExpr ve) ch = new StringExpr(ve.NameCleaned.Lexeme, ve.NameCleaned);
            parser.Consume(TokenKind.Comma, "Expected ',' after CHANNEL PUBLISH channel.");
            var msg = parser.ParseExpression();
            var tok = parser.Previous();
            var nsTok = new Token(TokenKind.Identifier, "AI", null, tok.Position);
            var call = new BuiltinFunctionCallExpr(nsTok, new Token(TokenKind.AiChannelPublish, "CHANNELPUBLISH", null, tok.Position), [ch, msg], new BitType());
            return new ExpressionStmt(call);
        }
        if (verb.Lexeme.Equals("SUBSCRIBE", StringComparison.OrdinalIgnoreCase))
        {
            parser.Advance();
            var ch = parser.ParseExpression();
            if (ch is VariableExpr ve) ch = new StringExpr(ve.NameCleaned.Lexeme, ve.NameCleaned);
            parser.Consume(TokenKind.Comma, "Expected ',' after CHANNEL SUBSCRIBE channel.");
            var handler = parser.ParseExpression();
            if (handler is VariableExpr veh) handler = new StringExpr(veh.NameCleaned.Lexeme, veh.NameCleaned);
            var tok = parser.Previous();
            var nsTok = new Token(TokenKind.Identifier, "AI", null, tok.Position);
            var call = new BuiltinFunctionCallExpr(nsTok, new Token(TokenKind.AiChannelSubscribe, "CHANNELSUBSCRIBE", null, tok.Position), [ch, handler], new BitType());
            return new ExpressionStmt(call);
        }
        if (verb.Lexeme.Equals("UNSUBSCRIBE", StringComparison.OrdinalIgnoreCase))
        {
            parser.Advance();
            var ch = parser.ParseExpression();
            if (ch is VariableExpr ve) ch = new StringExpr(ve.NameCleaned.Lexeme, ve.NameCleaned);
            parser.Consume(TokenKind.Comma, "Expected ',' after CHANNEL UNSUBSCRIBE channel.");
            var handler = parser.ParseExpression();
            if (handler is VariableExpr veh) handler = new StringExpr(veh.NameCleaned.Lexeme, veh.NameCleaned);
            var tok = parser.Previous();
            var nsTok = new Token(TokenKind.Identifier, "AI", null, tok.Position);
            var call = new BuiltinFunctionCallExpr(nsTok, new Token(TokenKind.AiChannelUnsubscribe, "CHANNELUNSUBSCRIBE", null, tok.Position), [ch, handler], new BitType());
            return new ExpressionStmt(call);
        }
        throw Parser.CreateError(verb, $"Expected CREATE, PUBLISH, SUBSCRIBE or UNSUBSCRIBE after 'AI CHANNEL'.");
    }

    /// <summary>AI PERMISSION — modello di permessi dichiarativo (Sandbox/Supervised/YOLO).
    ///   AI PERMISSION DEFAULT sandbox|supervised|yolo          → default per i nuovi agenti
    ///   AI PERMISSION nome, sandbox|supervised|yolo            → livello di un agente
    ///   AI PERMISSION nome, TOOL tool, allow|deny|approve      → override per-tool
    /// Compila a BuiltinFunctionCallExpr(TokenKind.AiPermission) con 1/2/3 argomenti
    /// (il codegen dispatcha per arità sul metodo runtime).</summary>
    private static Stmt ParseAiPermissionStatement(Parser parser, string expectedNamespace)
    {
        parser.Advance(); // PERMISSION

        // Forma 1: AI PERMISSION DEFAULT <livello>
        if (parser.Check(TokenKind.Default))
        {
            parser.Advance();
            var level = ParseAiValueToken(parser);
            var tok = parser.Previous();
            var nsTok = new Token(TokenKind.Identifier, "AI", null, tok.Position);
            var call = new BuiltinFunctionCallExpr(nsTok,
                new Token(TokenKind.AiPermission, "PERMISSION", null, tok.Position),
                ImmutableArray.Create<Expr>(level), new BitType());
            return new ExpressionStmt(call);
        }

        var name = parser.ParseExpression();
        // `AI PERMISSION A, ...` — un identificatore semplice è il NOME dell'agente (stringa
        // letterale), non una variabile (stesso pattern di ParseAiAgentArgs).
        if (name is VariableExpr veName && veName.NameCleaned.Lexeme == veName.NameSuffixed.Lexeme)
            name = new StringExpr(veName.NameCleaned.Lexeme, veName.NameSuffixed);
        parser.Consume(TokenKind.Comma, "Expected ',' after agent name.");

        // Forma 3: AI PERMISSION nome, TOOL tool, policy
        if (parser.Match(TokenKind.AiSubTool))
        {
            var tool = ParseAiValueToken(parser);
            parser.Consume(TokenKind.Comma, "Expected ',' after tool name.");
            var policy = ParseAiValueToken(parser);
            var tok = parser.Previous();
            var nsTok = new Token(TokenKind.Identifier, "AI", null, tok.Position);
            var call = new BuiltinFunctionCallExpr(nsTok,
                new Token(TokenKind.AiPermission, "PERMISSION", null, tok.Position),
                ImmutableArray.Create<Expr>(name, tool, policy), new BitType());
            return new ExpressionStmt(call);
        }

        // Forma 2: AI PERMISSION nome, <livello>
        var lvl = ParseAiValueToken(parser);
        var tok2 = parser.Previous();
        var nsTok2 = new Token(TokenKind.Identifier, "AI", null, tok2.Position);
        var call2 = new BuiltinFunctionCallExpr(nsTok2,
            new Token(TokenKind.AiPermission, "PERMISSION", null, tok2.Position),
            ImmutableArray.Create<Expr>(name, lvl), new BitType());
        return new ExpressionStmt(call2);
    }

    /// <summary>AI AGENT ASK nome, prompt INTO OnToken — streaming con callback per token.
    /// Se manca INTO, è una normale ASK (funzione). Con INTO: comando che avvia lo
    /// streaming e chiama la SUB callback per ogni token.</summary>
    private static Stmt ParseAiAgentAskStreamStatement(Parser parser)
    {
        var askTok = parser.Consume(TokenKind.AiVectorAsk, "Expected 'ASK' after 'AI AGENT'.");
        parser.Consume(TokenKind.Stream, "Expected 'STREAM' after 'AI AGENT ASK'.");
        var args = ImmutableArray.CreateBuilder<Expr>();
        args.Add(parser.ParseExpression());
        if (parser.Match(TokenKind.Comma)) args.Add(parser.ParseExpression());
        parser.Consume(TokenKind.Into, "Expected 'INTO' after 'AI AGENT ASK STREAM nome, prompt'.");
        var callback = parser.Consume(TokenKind.Identifier, "Expected a subroutine name after 'INTO'.");
        var nsTok = new Token(TokenKind.Identifier, "AI", null, askTok.Position);
        // Passa il nome della SUB callback come 3° argomento (l'emitter crea il delegate).
        var call = new BuiltinFunctionCallExpr(nsTok,
            new Token(TokenKind.AiAgentAskStream, "AGENTASKSTREAM", null, askTok.Position),
            [args[0], args[1], new StringExpr(callback.Lexeme, callback)], new BitType());
        return new ExpressionStmt(call);
    }

    /// <summary>AI AGENT TRACE ON|OFF|VERBOSE — il sottocomando è un token, non una stringa.
    /// AI AGENT TRACE ON → token fuso AiAgentTraceOn → runtime AiAgentTrace("ON").</summary>
    private static Stmt ParseAiAgentTraceStatement(Parser parser, TokenKind baseKind)
    {
        parser.Advance(); // consuma TRACE
        var sub = parser.Peek();
        var (tokKind, mode) = sub.Kind switch
        {
            TokenKind.On => (TokenKind.AiAgentTraceOn, "ON"),
            TokenKind.Off => (TokenKind.AiAgentTraceOff, "OFF"),
            TokenKind.Verbose => (TokenKind.AiAgentTraceVerbose, "VERBOSE"),
            _ => throw Parser.CreateError(sub,
                $"Expected ON, OFF or VERBOSE after 'AI AGENT TRACE'.")
        };
        parser.Advance(); // consuma ON/OFF/VERBOSE
        var tok = parser.Previous();
        var nsTok = new Token(TokenKind.Identifier, "AI", null, tok.Position);
        var call = new BuiltinFunctionCallExpr(nsTok, new Token(tokKind, "AGENTTRACE", null, tok.Position),
            [new StringExpr(mode, tok)], new BitType());
        return new ExpressionStmt(call);
    }

    /// <summary>Consuma il verbo (già letto) e poi legge gli argomenti.</summary>
    private static Stmt ConsumeVerbThenArgs(Parser parser, TokenKind tokKind, int minArgs, DataType ret)
    {
        parser.Advance(); // consuma il verbo
        return ParseAiAgentArgs(parser, tokKind, minArgs, ret);
    }

    /// <summary>Helper: legge gli argomenti separati da virgola e costruisce la chiamata
    /// agente con il TokenKind fuso (forma legacy, il verbo è già stato consumato).</summary>
    private static Stmt ParseAiAgentArgs(Parser parser, TokenKind tokKind, int minArgs, DataType ret)
    {
        var args = ImmutableArray.CreateBuilder<Expr>();
        if (minArgs > 0)
        {
            var first = parser.ParseExpression();
            // Legacy `AI AGENT CREATE Support` (identificatore = NOME dell'agente, senza
            // virgolette, pattern dei sample): un identificatore semplice è una STRINGA
            // letterale, non una variabile — altrimenti AiAgentCreate riceve il valore di
            // una variabile mai dichiarata (0/null) e l'agente non viene creato.
            if (first is VariableExpr ve && ve.NameCleaned.Lexeme == ve.NameSuffixed.Lexeme)
                first = new StringExpr(ve.NameCleaned.Lexeme, ve.NameSuffixed);
            args.Add(first);
        }
        while (parser.Match(TokenKind.Comma)) args.Add(parser.ParseExpression());
        var tok = parser.Previous();
        var nsTok = new Token(TokenKind.Identifier, "AI", null, tok.Position);
        var call = new BuiltinFunctionCallExpr(nsTok, new Token(tokKind, "AGENT", null, tok.Position),
            args.ToImmutable(), ret);
        return new ExpressionStmt(call);
    }

    /// <summary>AI SERVER START <modelDir> | STOP | STATUS — comando legacy a spazio (AGENTS.md
    /// §2.0: nessuna parentesi). Apre un BuiltinFunctionCallExpr AI.<azione> come gli altri
    /// comandi AI; l'esecuzione vive in IlRuntime.AiServer* / AiServerManager (processo esterno).</summary>
    private static Stmt ParseAiServerStatement(Parser parser, string ns)
    {
        parser.Advance(); // consuma 'SERVER'
        var verb = parser.Peek();
        return verb.Kind switch
        {
            TokenKind.Start => ConsumeAiServerVerb(parser, TokenKind.AiServerStart, 1),
            TokenKind.Stop => ConsumeAiServerVerb(parser, TokenKind.AiServerStop, 0),
            TokenKind.AiSubStatus => ConsumeAiServerVerb(parser, TokenKind.AiServerStatus, 0),
            TokenKind.AiSubModel => ConsumeAiServerVerb(parser, TokenKind.AiServerModel, 1),
            TokenKind.Unload => ConsumeAiServerVerb(parser, TokenKind.AiServerUnload, 0),
            _ => throw Parser.CreateError(verb, "Expected START, STOP, STATUS, MODEL or UNLOAD after 'AI SERVER'.")
        };
    }

    private static Stmt ConsumeAiServerVerb(Parser parser, TokenKind action, int argc)
    {
        parser.Advance(); // consuma il verbo
        var args = ImmutableArray.CreateBuilder<Expr>();
        if (argc > 0)
        {
            var first = parser.ParseExpression();
            if (first is VariableExpr ve && ve.NameCleaned.Lexeme == ve.NameSuffixed.Lexeme)
                first = new StringExpr(ve.NameCleaned.Lexeme, ve.NameSuffixed);
            args.Add(first);
            // START <dir> [port] — porta opzionale come secondo argomento in riga (es. 8023)
            if (action == TokenKind.AiServerStart
                && !parser.Check(TokenKind.Eof)
                && parser.Peek().Position.Line == parser.Previous().Position.Line)
                args.Add(parser.ParseExpression());
        }
        var tok = parser.Previous();
        var nsTok = new Token(TokenKind.Identifier, "AI", null, tok.Position);
        var call = new BuiltinFunctionCallExpr(nsTok, new Token(action, "SERVER", null, tok.Position),
            args.ToImmutable(), new StringType());
        return new ExpressionStmt(call);
    }

    /// <summary>`LLM MODEL LOAD &lt;nome&gt;, &lt;path&gt; [, RESIDENT]` | UNLOAD &lt;nome&gt; |
    /// CACHE &lt;nome&gt;, &lt;mb&gt; | PREFETCH &lt;nome&gt;, &lt;topn&gt; | DEVICE &lt;nome&gt;, &lt;AUTO|CPU|METAL&gt; |
    /// PROMOTE/DEMOTE &lt;nome&gt; — residenza/streaming del modello MoE out-of-core (GGUF).
    /// CONVERT | QUANTIZE | INSPECT — model compiler. Comando legacy a spazio.
    /// (Ripristinati 2026-09-25: erano stati rimossi per errore insieme al formato .gmoe; il
    /// runtime che li sostiene è gmb_moe_runtime, indipendente dal container.)</summary>
    private static Stmt ParseAiModelStatement(Parser parser, string ns)
    {
        parser.Advance(); // consuma 'MODEL'
        var verb = parser.Peek();
        return verb.Kind switch
        {
            TokenKind.Load => ParseAiModelLoad(parser),
            TokenKind.Unload => ConsumeAiModelVerb(parser, TokenKind.AiModelUnload),
            TokenKind.Cache => ConsumeAiModelVerb(parser, TokenKind.AiModelCache),
            TokenKind.Prefetch => ConsumeAiModelVerb(parser, TokenKind.AiModelPrefetch),
            TokenKind.Device => ConsumeAiModelVerb(parser, TokenKind.AiModelDevice),
            TokenKind.ConvDoc => ConsumeAiModelVerb(parser, TokenKind.AiModelConvert),
            TokenKind.Promote => ConsumeAiModelVerb(parser, TokenKind.AiModelPromote),
            TokenKind.Demote => ConsumeAiModelVerb(parser, TokenKind.AiModelDemote),
            TokenKind.Quantize => ConsumeAiModelVerb(parser, TokenKind.AiModelQuantize),
            TokenKind.Inspect => ConsumeAiModelVerb(parser, TokenKind.AiModelInspect),
            TokenKind.Save => ConsumeAiModelVerb(parser, TokenKind.AiModelSave),
            TokenKind.Identifier when verb.Lexeme == "ABLATE" => ConsumeAiModelVerb(parser, TokenKind.AiModelAblate),
            TokenKind.Remove => ConsumeAiModelVerb(parser, TokenKind.AiModelRemove),
            TokenKind.Identifier when verb.Lexeme == "DIFF" => ConsumeAiModelVerb(parser, TokenKind.AiModelDiff),
            TokenKind.Identifier when verb.Lexeme == "CHECKPOINT" => ConsumeAiModelVerb(parser, TokenKind.AiModelCheckpoint),
            TokenKind.Rollback => ConsumeAiModelVerb(parser, TokenKind.AiModelRollback),
            TokenKind.Add => ConsumeAiModelVerb(parser, TokenKind.AiModelAdd),
            TokenKind.Identifier when verb.Lexeme == "ADAPTER" => ConsumeAiModelVerb(parser, TokenKind.AiModelAdapter),
            TokenKind.LlmModule => ConsumeAiModelVerb(parser, TokenKind.AiModelModule),
            TokenKind.Identifier when verb.Lexeme == "MODALITY" => ConsumeAiModelVerb(parser, TokenKind.AiModelModality),
            TokenKind.Validate => ConsumeAiModelVerb(parser, TokenKind.AiModelValidate),
            // K1: LLM MODEL TRACE nome, path | OFF — trace esperti (replay offline). Il lexer
            // mappa "TRACE" su AiSubTrace (AI AGENT TRACE); qui il verbo e' quello.
            TokenKind.AiSubTrace => ConsumeAiModelVerb(parser, TokenKind.AiModelTrace),
            _ => throw Parser.CreateError(verb, "Expected LOAD, UNLOAD, CACHE, PREFETCH, DEVICE, CONVERT, PROMOTE, DEMOTE, QUANTIZE, INSPECT, SAVE, ABLATE, DIFF, CHECKPOINT, ROLLBACK, ADD, ADAPTER, MODULE, MODALITY, VALIDATE or TRACE after 'LLM MODEL'.")
        };
    }

    /// <summary>LLM MODEL LOAD nome, path [, RESIDENT]: RESIDENT = tutto in RAM; default SWAP
    /// (streaming esperti da disco).</summary>
    private static Stmt ParseAiModelLoad(Parser parser)
    {
        parser.Advance(); // consuma LOAD
        var args = ImmutableArray.CreateBuilder<Expr>();
        args.Add(ParseAiModelArg(parser)); // nome
        while (parser.Match(TokenKind.Comma))
        {
            if (parser.Check(TokenKind.Resident))
            {
                var rt = parser.Peek();
                parser.Advance();
                args.Add(new StringExpr("RESIDENT", rt));
            }
            else
            {
                args.Add(ParseAiModelArg(parser)); // path
            }
        }
        return BuildAiModelCall(parser, TokenKind.AiModelLoad, args.ToImmutable());
    }

    private static Stmt ConsumeAiModelVerb(Parser parser, TokenKind action)
    {
        parser.Advance(); // consuma il verbo
        var args = ImmutableArray.CreateBuilder<Expr>();
        args.Add(ParseAiModelArg(parser));
        while (parser.Match(TokenKind.Comma)) args.Add(ParseAiModelArg(parser));
        return BuildAiModelCall(parser, action, args.ToImmutable());
    }

    private static Stmt BuildAiModelCall(Parser parser, TokenKind action, ImmutableArray<Expr> args)
    {
        var tok = parser.Previous();
        // HARD MOVE AI->LLM: il layer motore vive sotto LLM → ns canonico "LLM".
        var nsTok = new Token(TokenKind.Identifier, "LLM", null, tok.Position);
        var call = new BuiltinFunctionCallExpr(nsTok, new Token(action, "MODEL", null, tok.Position),
            args, new StringType());
        return new ExpressionStmt(call);
    }

    /// <summary>Verbi del model compiler LLM (PLAN/POLICY/QUANT/MERGE/PRUNE/EXTRACT/PROFILE/ARCH):
    /// il verbo diventa un BuiltinFunctionCallExpr (ns "LLM") coi termini come argomenti; il
    /// primo termine di ARCH è il sotto-verbo (RESOLVE/SHOW/LIST/CHECK). Gli argomenti non
    /// quotati sono stringhe letterali (convenzione dei comandi, come ParseAiModelArg).
    /// La risoluzione TokenKind → BuiltInFunction è nella mappa del Compiler (LLMPLAN/…/LLMARCH):
    /// così ogni backend (IL, PCodeVM, VM/C, C99Native) lo riceve come builtin e lo
    /// esegue al proprio runtime — mai un no-op silenzioso.</summary>
    private static Stmt ParseLlmCompilerCall(Parser parser)
    {
        var verb = parser.Peek();
        parser.Advance(); // consuma il verbo
        var nsTok = new Token(TokenKind.Identifier, "LLM", null, verb.Position);
        var args = ImmutableArray.CreateBuilder<Expr>();
        // Argomenti a SPAZIO (come il parser C: `LLM ARCH CHECK "dir"`, `LLM PLAN "a", "b"`),
        // con virgole opzionali. Si legge un termine per volta fino a fine statement.
        // ECCEZIONE: `POLICY CALLBACK <Proc>` -> il callback e' un NOME DI PROCEDURA (proc-ref,
        // non una stringa), come `ON WATCH ... callback` / `AEVAL array, fn` (pattern consolidato):
        // si parsa con ParseExpression (nome nudo -> VariableExpr risolto dal linker).
        bool expectingPolicyCallback = false;
        var policyVerbKind = verb.Kind == TokenKind.Prune ? TokenKind.LlmPrune
            : verb.Kind == TokenKind.Extract ? TokenKind.LlmExtract
            : verb.Kind == TokenKind.Compile ? TokenKind.LlmCompile
            : (verb.Kind == TokenKind.Identifier && verb.Lexeme == "QUALITY") ? TokenKind.LlmQuality
            : verb.Kind;
        while (!parser.IsAtEndOfStatement() && !parser.Check(TokenKind.Eof))
        {
            if (parser.Check(TokenKind.Into)) break;   // `… INTO <target>`: fine degli argomenti
            if (parser.Match(TokenKind.Comma)) continue;
            var beforePos = parser.Peek().Position;
            if (expectingPolicyCallback) args.Add(parser.ParseExpression());
            else args.Add(ParseAiModelArg(parser));
            if (policyVerbKind == TokenKind.LlmPolicy && args.Count == 1
                && args[0] is StringExpr se0 && se0.Value.Equals("CALLBACK", System.StringComparison.OrdinalIgnoreCase))
                expectingPolicyCallback = true;
            if (parser.Peek().Position == beforePos) break;   // nessun avanzamento: evita loop
        }
        // PRUNE/EXTRACT hanno la grafia CONDIVISA (JSON.Extract, SET PRUNE) → il TokenKind
        // dedicato LLM (LlmPrune/LlmExtract) evita la collisione nella mappa globale del
        // Compiler (Dictionary.Add) e instrada il comando giusto. Gli altri verbi LLM hanno
        // già il proprio TokenKind (LlmPlan/LlmPolicy/…/LlmArch).
        var verbKind = verb.Kind switch
        {
            TokenKind.Prune => TokenKind.LlmPrune,
            TokenKind.Extract => TokenKind.LlmExtract,
            TokenKind.Compile => TokenKind.LlmCompile,
            TokenKind.Identifier when verb.Lexeme == "QUALITY" => TokenKind.LlmQuality,
            _ => verb.Kind,
        };
        // INTO <target>: il valore del sotto-verbo va nella variabile invece di essere stampato.
        // Oggi vale per la famiglia TENSOR (il cui output e' gia' catturabile in C con `_cmd_to`);
        // per le altre famiglie (KERNEL/MODULE/KV/ARCH/POLICY/…) il comando resta a stampa.
        if (parser.Match(TokenKind.Into) && LlmFamilyOutKind(verbKind) is TokenKind outKind)
        {
            var target = parser.ParseTarget();
            var vcall = new BuiltinFunctionCallExpr(nsTok, new Token(outKind, verb.Lexeme, null, verb.Position),
                args.ToImmutable(), new StringType());
            return new LetStmt([new Assignment(target, vcall)], null);
        }
        var call = new BuiltinFunctionCallExpr(nsTok, new Token(verbKind, verb.Lexeme, null, verb.Position),
            args.ToImmutable(), new StringType());
        return new ExpressionStmt(call);
    }

    /// <summary>Un identificatore non quotato e' una STRINGA letterale (convenzione dei comandi
    /// AI: `METAL`, `RESIDENT`, un nome modello senza virgolette).</summary>
    private static Expr ParseAiModelArg(Parser parser)
    {
        var e = parser.ParseExpression();
        if (e is VariableExpr ve && ve.NameCleaned.Lexeme == ve.NameSuffixed.Lexeme)
            return new StringExpr(ve.NameCleaned.Lexeme, ve.NameSuffixed);
        return e;
    }

    /// <summary>AI AGENT SET — sintassi canonica con VIRGOLE o con le clausole esplicite
    /// <c>KEY … VALUE …</c>:
    ///   AI AGENT SET "nome", chiave valore
    ///   AI AGENT SET "nome", chiave, valore
    ///   AI AGENT SET "nome" KEY chiave VALUE valore
    /// (la forma a spazio senza virgole/KEY resta accettata come leniente, per compatibilità).
    /// Chiave non quotata → stringa letterale; valore opzionale.</summary>
    private static Stmt ParseAiAgentSetStatement(Parser parser)
    {
        parser.Advance(); // SET
        var args = ImmutableArray.CreateBuilder<Expr>();
        args.Add(ParseAiAgentNameToken(parser)); // nome agente
        ParseAiAgentSetKeyValue(parser, args);
        var tok = parser.Previous();
        var nsTok = new Token(TokenKind.Identifier, "AI", null, tok.Position);
        var call = new BuiltinFunctionCallExpr(nsTok, new Token(TokenKind.AiAgentSet, "SET", null, tok.Position),
            args.ToImmutable(), new BitType());
        return new ExpressionStmt(call);
    }

    /// <summary>AI.AGENTSET nome chiave [valore] — qualificata fusa (virgole/KEY-VALUE ammessi).</summary>
    private static Stmt ParseAiQualifiedAgentSet(Parser parser)
    {
        var args = ImmutableArray.CreateBuilder<Expr>();
        args.Add(ParseAiAgentNameToken(parser)); // nome agente
        ParseAiAgentSetKeyValue(parser, args);
        var tok = parser.Previous();
        var nsTok = new Token(TokenKind.Identifier, "AI", null, tok.Position);
        var call = new BuiltinFunctionCallExpr(nsTok, new Token(TokenKind.AiAgentSet, "AGENTSET", null, tok.Position),
            args.ToImmutable(), new BitType());
        return new ExpressionStmt(call);
    }

    /// <summary>Chiave+[valore] di AI AGENT SET: accetta virgola opzionale dopo il nome, la
    /// clausola <c>KEY</c> esplicita, e dopo la chiave virgola opzionale e/o <c>VALUE</c>
    /// esplicita. Aggiunge [chiave, valore] ad <paramref name="args"/>.</summary>
    private static void ParseAiAgentSetKeyValue(Parser parser, ImmutableArray<Expr>.Builder args)
    {
        parser.Match(TokenKind.Comma);   // virgola opzionale dopo il nome
        parser.Match(TokenKind.Key);     // clausola 'KEY' esplicita opzionale
        args.Add(ParseAiKeyToken(parser));
        parser.Match(TokenKind.Comma);   // virgola opzionale dopo la chiave
        parser.Match(TokenKind.Value);   // clausola 'VALUE' esplicita opzionale
        if (!parser.Check(TokenKind.Eof) && parser.Peek().Position.Line == parser.Previous().Position.Line)
        {
            args.Add(ParseAiValueToken(parser));
        }
        else
        {
            args.Add(new StringExpr("", parser.Previous()));
        }
    }

    /// <summary>Nome agente del SET a spazio: stringa quotata -> consuma SOLO quel token
    /// (bypassa parser.ParseExpression, che a livello ParseTerm concatena implicitamente
    /// letterali stringa adiacenti — "Support" "systemPrompt" "..." diventava UN SOLO
    /// argomento concatenato invece di tre, con key/value che restavano vuoti a runtime:
    /// bug L18, "Parameter 'key' vuoto"). Altrimenti (variabile, espressione) resta
    /// parser.ParseExpression per non perdere il supporto a nomi agente dinamici.</summary>
    private static Expr ParseAiAgentNameToken(Parser parser)
    {
        var tok = parser.Peek();
        if (tok.Kind == TokenKind.StringLiteral)
        {
            parser.Advance();
            return new StringExpr((string)tok.Literal!, tok);
        }
        // Nome nudo: se è una VARIABILE DICHIARATA resta tale (valore a runtime),
        // altrimenti è il NOME letterale dell'agente (parità C99Native: `AI AGENT SET
        // Assist ...` — prima era una VariableExpr non inizializzata → "0").
        if (tok.Kind == TokenKind.Identifier && !parser._declaredVariableNames.Contains(tok.Lexeme))
        {
            parser.Advance();
            return new StringExpr(tok.Lexeme, tok);
        }
        return parser.ParseExpression();
    }

    /// <summary>Chiave del SET: identifier non quotato -> stringa letterale (altrimenti il
    /// linker lo tratterebbe come variabile), stringa quotata -> resta, keyword di chiave
    /// nota (es. BROWSER, PROVIDER) -> lexeme come letterale (altrimenti finirebbe valutata
    /// come espressione vuota — regressione PCode della sintassi senza virgole).</summary>
    private static readonly System.Collections.Generic.HashSet<string> AiSetKeys = new(
        new[] { "model", "temperature", "maxtokens", "systemprompt", "provider",
                "skills", "skillsenabled", "clienttools", "clienttoolsenabled",
                "memory", "memoryenabled", "plugins", "pluginsenabled",
                "thinking", "thinkingenabled", "permission", "toolpolicy",
                "thoughttokens", "reflection", "critic",
                "browser", "browserprovider", "sessionstreaming", "sessionstreamingenabled" },
        System.StringComparer.OrdinalIgnoreCase);

    private static Expr ParseAiKeyToken(Parser parser)
    {
        var tok = parser.Peek();
        if (tok.Kind == TokenKind.Identifier)
        {
            parser.Advance();
            return new StringExpr(tok.Lexeme, tok);
        }
        if (!string.IsNullOrEmpty(tok.Lexeme) && AiSetKeys.Contains(tok.Lexeme) && tok.Kind != TokenKind.StringLiteral)
        {
            parser.Advance();
            return new StringExpr(tok.Lexeme, tok);
        }
        // Chiave quotata (es. "systemPrompt"): consuma SOLO questo token — stesso bug L18
        // di ParseAiAgentNameToken se si cade nel fallback ParseExpression sotto, con il
        // valore adiacente ("...") concatenato implicitamente nella chiave invece di restare
        // il terzo argomento separato.
        if (tok.Kind == TokenKind.StringLiteral)
        {
            parser.Advance();
            return new StringExpr((string)tok.Literal!, tok);
        }
        return parser.ParseExpression();
    }

    /// <summary>Valore del SET senza virgola: ON/OFF -> "ON"/"OFF"; identifier -> stringa
    /// letterale; stringa quotata -> resta; altrimenti espressione.</summary>
    private static Expr ParseAiValueToken(Parser parser)
    {
        var tok = parser.Peek();
        // Costante globale %NOME (es. %CHROMIUM, %RERANK_KEYWORD): resta un'espressione
        // (VariableExpr) così il linker la folda al NUMERO — mai stringhe nel valore.
        if (!string.IsNullOrEmpty(tok.Lexeme) && tok.Lexeme[0] == '%')
            return parser.ParseExpression();
        if (tok.Kind is TokenKind.On or TokenKind.Off)
        {
            parser.Advance();
            return new StringExpr(tok.Kind == TokenKind.On ? "ON" : "OFF", tok);
        }
        if (tok.Kind == TokenKind.Identifier)
        {
            parser.Advance();
            return new StringExpr(tok.Lexeme, tok);
        }
        if (tok.Kind == TokenKind.StringLiteral)
        {
            parser.Advance();
            // Usa il VALORE decodificato (Literal, senza virgolette) come fa ParseExpression
            // per un StringLiteral — non il Lexeme (che include le " di contorno). Bug 2026-09-02:
            // con Lexeme il valore arrivava come '"0.42"' al runtime (C# lo compensava con
            // Trim('"'), ma il runtime C atof/atoi falliva: temperature=0, parità rotta).
            return new StringExpr((string)tok.Literal!, tok);
        }
        return parser.ParseExpression();
    }

    /// <summary>Dispatcher della forma qualificata "AI.SETACTIVE cap, nome". Il token
    /// corrente è l'Identifier del nome fuso (già letto dal DispatchRegisteredCommand).</summary>

    /// <summary>Helper: legge una lista di argomenti separati da virgola e costruisce la
    /// chiamata con il TokenKind fuso dedicato (forma qualificata senza parentesi).</summary>
    private static Stmt ParseAiQualifiedArgs(Parser parser, TokenKind tokKind, int minArgs, DataType ret)
    {
        var args = ImmutableArray.CreateBuilder<Expr>();
        for (var i = 0; i < minArgs; i++)
        {
            if (i > 0) parser.Consume(TokenKind.Comma, "Expected ',' between arguments.");
            args.Add(parser.ParseExpression());
        }
        while (parser.Match(TokenKind.Comma)) args.Add(parser.ParseExpression());
        var tok = parser.Previous();
        var nsTok = new Token(TokenKind.Identifier, "AI", null, tok.Position);
        var call = new BuiltinFunctionCallExpr(nsTok, new Token(tokKind, "AI", null, tok.Position),
            args.ToImmutable(), ret);
        return new ExpressionStmt(call);
    }

    /// <summary>AI.AGENTTRACEON (fuso) → runtime AiAgentTrace("ON"), senza argomenti.</summary>
    private static Stmt ParseAiQualifiedTrace(Parser parser, TokenKind tokKind, string mode)
    {
        var tok = parser.Previous();
        var nsTok = new Token(TokenKind.Identifier, "AI", null, tok.Position);
        var call = new BuiltinFunctionCallExpr(nsTok, new Token(tokKind, "AGENTTRACE" + mode, null, tok.Position),
            [new StringExpr(mode, tok)], new BitType());
        return new ExpressionStmt(call);
    }

    /// <summary>Legacy: AI SET ACTIVE cap, nome → PROVIDERSETACTIVE(cap, nome).</summary>
    private static Stmt ParseAiSetActiveStatement(Parser parser, string ns, bool withProvider)
    {
        var tok = parser.Consume(TokenKind.AiSubActive, "Expected 'ACTIVE' after 'AI SET'.");
        if (parser.Match(TokenKind.Provider)) withProvider = true; // AI SET ACTIVE PROVIDER ...
        var cap = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after capability.");
        var name = parser.ParseExpression();
        var nsTok = new Token(TokenKind.Identifier, "AI", null, tok.Position);
        var call = new BuiltinFunctionCallExpr(nsTok,
            new Token(withProvider ? TokenKind.AiSetActiveProvider : TokenKind.AiSetActive, "SETACTIVE", null, tok.Position),
            [cap, name], new BitType());
        return new ExpressionStmt(call);
    }

    /// <summary>AI REGISTER alias, capability, source [, param] (alias-first) → AiRegisterSource.</summary>
    private static Stmt ParseAiRegisterStatement(Parser parser, string ns)
    {
        var tok = parser.Consume(TokenKind.Register, "Expected 'REGISTER' after 'AI'.");
        // Nuova firma: AI REGISTER alias, capability, source [, param] → RegisterSource.
        var alias = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after alias.");
        var capability = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after capability.");
        var source = parser.ParseExpression();
        var args = ImmutableArray.CreateBuilder<Expr>();
        args.Add(alias); args.Add(capability); args.Add(source);
        if (parser.Match(TokenKind.Comma)) args.Add(parser.ParseExpression());
        var nsTok = new Token(TokenKind.Identifier, "AI", null, tok.Position);
        // Lessema "REGISTERSOURCE", non "REGISTER": la risoluzione candidati del Linker
        // (RewriteBuiltinFunctionCall) cerca per NOME case-insensitive dentro il namespace,
        // non solo per TokenKind — "REGISTER" collideva con PROVIDER.REGISTER (StdLib.AI.cs,
        // firma role-first) SOLO per coincidenza testuale, anche se i due TokenKind (AiRegister
        // vs ProviderRegister) sono diversi. Il Linker riscriveva silenziosamente ogni "AI
        // REGISTER alias, capability, source" (alias-first) nella chiamata role-first di
        // PROVIDER.REGISTER, scambiando alias<->capability — bug cross-backend (IL e PCodeVM
        // entrambi colpiti, non solo PCodeVM: verificato con IlProgramCompiler.EmitAiRegisterCall
        // mai raggiunto). Un lessema univoco (nessun NativeProcedureDefinition registrato con
        // questo nome) fa fallire il match per nome, il Linker preserva bce.Name.Kind originale
        // (percorso "bypass" in RewriteBuiltinFunctionCall), e AiRegister arriva intatto al
        // backend (IL: EmitAiRegisterCall; PCodeVM: BuiltInFunction.AIREGISTERSOURCE).
        var call = new BuiltinFunctionCallExpr(nsTok,
            new Token(TokenKind.AiRegister, "REGISTERSOURCE", null, tok.Position), args.ToImmutable(), new BitType());
        return new ExpressionStmt(call);
    }

    /// <summary>Legacy: AI CONFIGURE alias, key, value.</summary>
    private static Stmt ParseAiConfigureStatement(Parser parser, string ns)
    {
        var tok = parser.Consume(TokenKind.AiSubConfigure, "Expected 'CONFIGURE' after 'AI'.");
        var a = ImmutableArray.CreateBuilder<Expr>();
        a.Add(parser.ParseExpression());
        while (parser.Match(TokenKind.Comma)) a.Add(parser.ParseExpression());
        var nsTok = new Token(TokenKind.Identifier, "AI", null, tok.Position);
        var call = new BuiltinFunctionCallExpr(nsTok,
            new Token(TokenKind.AiConfigure, "CONFIGURE", null, tok.Position), a.ToImmutable(), new BitType());
        return new ExpressionStmt(call);
    }

    /// <summary>Legacy: AI DISCOVER pfx, cap, url, key [, soloFree].</summary>
    private static Stmt ParseAiDiscoverStatement(Parser parser, string ns)
    {
        var tok = parser.Consume(TokenKind.AiSubDiscover, "Expected 'DISCOVER' after 'AI'.");
        var args = ImmutableArray.CreateBuilder<Expr>();
        args.Add(parser.ParseExpression());
        while (parser.Match(TokenKind.Comma)) args.Add(parser.ParseExpression());
        var nsTok = new Token(TokenKind.Identifier, "AI", null, tok.Position);
        var call = new BuiltinFunctionCallExpr(nsTok,
            new Token(TokenKind.AiDiscover, "DISCOVER", null, tok.Position), args.ToImmutable(), new ArrayType(new StringType()));
        return new ExpressionStmt(call);
    }

    /// <summary>Legacy: AI UNREGISTER nome.</summary>
    private static Stmt ParseAiUnregisterStatement(Parser parser, string ns)
    {
        var tok = parser.Consume(TokenKind.AiSubUnregister, "Expected 'UNREGISTER' after 'AI'.");
        var a = parser.ParseExpression();
        var nsTok = new Token(TokenKind.Identifier, "AI", null, tok.Position);
        var call = new BuiltinFunctionCallExpr(nsTok,
            new Token(TokenKind.AiUnregister, "UNREGISTER", null, tok.Position), [a], new BitType());
        return new ExpressionStmt(call);
    }

    /// <summary>Legacy: AI SAVE path.</summary>
    private static Stmt ParseAiSaveProvStatement(Parser parser, string ns)
    {
        var tok = parser.Consume(TokenKind.Save, "Expected 'SAVE' after 'AI'.");
        var a = parser.ParseExpression();
        var nsTok = new Token(TokenKind.Identifier, "AI", null, tok.Position);
        var call = new BuiltinFunctionCallExpr(nsTok,
            new Token(TokenKind.AiSaveProv, "SAVE", null, tok.Position), [a], new BitType());
        return new ExpressionStmt(call);
    }

    /// <summary>Legacy: AI LOAD path.</summary>
    private static Stmt ParseAiLoadProvStatement(Parser parser, string ns)
    {
        var tok = parser.Consume(TokenKind.Load, "Expected 'LOAD' after 'AI'.");
        var a = parser.ParseExpression();
        var nsTok = new Token(TokenKind.Identifier, "AI", null, tok.Position);
        var call = new BuiltinFunctionCallExpr(nsTok,
            new Token(TokenKind.AiLoadProv, "LOAD", null, tok.Position), [a], new BitType());
        return new ExpressionStmt(call);
    }

    /// <summary>Legacy: AI FAIL nome.</summary>
    private static Stmt ParseAiFailStatement(Parser parser, string ns)
    {
        var tok = parser.Consume(TokenKind.AiSubFail, "Expected 'FAIL' after 'AI'.");
        var a = parser.ParseExpression();
        var nsTok = new Token(TokenKind.Identifier, "AI", null, tok.Position);
        var call = new BuiltinFunctionCallExpr(nsTok,
            new Token(TokenKind.AiFail, "FAIL", null, tok.Position), [a], new BitType());
        return new ExpressionStmt(call);
    }

    /// <summary>Legacy: AI RESETFAILED [nome].</summary>
    private static Stmt ParseAiResetFailedStatement(Parser parser, string ns)
    {
        var tok = parser.Consume(TokenKind.AiSubResetFailed, "Expected 'RESETFAILED' after 'AI'.");
        var args = ImmutableArray.CreateBuilder<Expr>();
        if (!parser.IsAtEndOfStatement()) args.Add(parser.ParseExpression());
        var nsTok = new Token(TokenKind.Identifier, "AI", null, tok.Position);
        var call = new BuiltinFunctionCallExpr(nsTok,
            new Token(TokenKind.AiResetFailed, "RESETFAILED", null, tok.Position), args.ToImmutable(), new BitType());
        return new ExpressionStmt(call);
    }

    /// <summary>Legacy: AI TEST role, nome.</summary>
    private static Stmt ParseAiTestStatement(Parser parser, string ns)
    {
        var tok = parser.Consume(TokenKind.AiSubTest, "Expected 'TEST' after 'AI'.");
        var a = ImmutableArray.CreateBuilder<Expr>();
        a.Add(parser.ParseExpression());
        while (parser.Match(TokenKind.Comma)) a.Add(parser.ParseExpression());
        var nsTok = new Token(TokenKind.Identifier, "AI", null, tok.Position);
        var call = new BuiltinFunctionCallExpr(nsTok,
            new Token(TokenKind.AiTest, "TEST", null, tok.Position), a.ToImmutable(), new StringType());
        return new ExpressionStmt(call);
    }

    /// <summary>Legacy: AI SET CREDITS nome, n.</summary>
    private static Stmt ParseAiSetCreditsStatement(Parser parser, string ns)
    {
        var tok = parser.Consume(TokenKind.AiSubCredits, "Expected 'CREDITS' after 'AI SET'.");
        var a = ImmutableArray.CreateBuilder<Expr>();
        a.Add(parser.ParseExpression());
        while (parser.Match(TokenKind.Comma)) a.Add(parser.ParseExpression());
        var nsTok = new Token(TokenKind.Identifier, "AI", null, tok.Position);
        var call = new BuiltinFunctionCallExpr(nsTok,
            new Token(TokenKind.AiSetCredits, "SETCREDITS", null, tok.Position), a.ToImmutable(), new BitType());
        return new ExpressionStmt(call);
    }

    /// <summary>Legacy: AI SET PRIORITY nome, p.</summary>
    private static Stmt ParseAiSetPriorityStatement(Parser parser, string ns)
    {
        var tok = parser.Consume(TokenKind.Priority, "Expected 'PRIORITY' after 'AI SET'.");
        var a = ImmutableArray.CreateBuilder<Expr>();
        a.Add(parser.ParseExpression());
        while (parser.Match(TokenKind.Comma)) a.Add(parser.ParseExpression());
        var nsTok = new Token(TokenKind.Identifier, "AI", null, tok.Position);
        var call = new BuiltinFunctionCallExpr(nsTok,
            new Token(TokenKind.AiSetPriority, "SETPRIORITY", null, tok.Position), a.ToImmutable(), new BitType());
        return new ExpressionStmt(call);
    }

    /// <summary>Legacy: AI SET WEIGHT nome, w.</summary>
    private static Stmt ParseAiSetWeightStatement(Parser parser, string ns)
    {
        var tok = parser.Consume(TokenKind.AiSubWeight, "Expected 'WEIGHT' after 'AI SET'.");
        var a = ImmutableArray.CreateBuilder<Expr>();
        a.Add(parser.ParseExpression());
        while (parser.Match(TokenKind.Comma)) a.Add(parser.ParseExpression());
        var nsTok = new Token(TokenKind.Identifier, "AI", null, tok.Position);
        var call = new BuiltinFunctionCallExpr(nsTok,
            new Token(TokenKind.AiSetWeight, "SETWEIGHT", null, tok.Position), a.ToImmutable(), new BitType());
        return new ExpressionStmt(call);
    }

    /// <summary>Legacy: AI SET STATUS nome, s.</summary>
    private static Stmt ParseAiSetStatusStatement(Parser parser, string ns)
    {
        var tok = parser.Consume(TokenKind.AiSubStatus, "Expected 'STATUS' after 'AI SET'.");
        var a = ImmutableArray.CreateBuilder<Expr>();
        a.Add(parser.ParseExpression());
        while (parser.Match(TokenKind.Comma)) a.Add(parser.ParseExpression());
        var nsTok = new Token(TokenKind.Identifier, "AI", null, tok.Position);
        var call = new BuiltinFunctionCallExpr(nsTok,
            new Token(TokenKind.AiSetStatus, "SETSTATUS", null, tok.Position), a.ToImmutable(), new BitType());
        return new ExpressionStmt(call);
    }

    /// <summary>Legacy: AI SET MODEL alias, model.</summary>
    private static Stmt ParseAiSetModelStatement(Parser parser, string ns)
    {
        var tok = parser.Consume(TokenKind.AiSubModel, "Expected 'MODEL' after 'AI SET'.");
        var a = ImmutableArray.CreateBuilder<Expr>();
        a.Add(parser.ParseExpression());
        while (parser.Match(TokenKind.Comma)) a.Add(parser.ParseExpression());
        var nsTok = new Token(TokenKind.Identifier, "AI", null, tok.Position);
        var call = new BuiltinFunctionCallExpr(nsTok,
            new Token(TokenKind.AiSetModel, "SETMODEL", null, tok.Position), a.ToImmutable(), new StringType());
        return new ExpressionStmt(call);
    }

    /// <summary>Legacy: AI SET KEY nome, key.</summary>
    private static Stmt ParseAiSetKeyStatement(Parser parser, string ns)    {
        var tok = parser.Consume(TokenKind.Key, "Expected 'KEY' after 'AI SET'.");
        var a = ImmutableArray.CreateBuilder<Expr>();
        a.Add(parser.ParseExpression());
        while (parser.Match(TokenKind.Comma)) a.Add(parser.ParseExpression());
        var nsTok = new Token(TokenKind.Identifier, "AI", null, tok.Position);
        var call = new BuiltinFunctionCallExpr(nsTok,
            new Token(TokenKind.AiSetKey, "SETKEY", null, tok.Position), a.ToImmutable(), new StringType());
        return new ExpressionStmt(call);
    }

    /// <summary>AI SET OPTION nome, valore — voce del registro opzioni generico (gmb_opt_set).</summary>
    private static Stmt ParseAiSetOptionStatement(Parser parser, string ns)
    {
        var tok = parser.Consume(TokenKind.Option, "Expected 'OPTION' after 'AI SET'.");
        var a = ImmutableArray.CreateBuilder<Expr>();
        a.Add(parser.ParseExpression());
        parser.Consume(TokenKind.Comma, "Expected ',' after option name.");
        a.Add(parser.ParseExpression());
        var nsTok = new Token(TokenKind.Identifier, "AI", null, tok.Position);
        var call = new BuiltinFunctionCallExpr(nsTok,
            new Token(TokenKind.AiSetOption, "SETOPTION", null, tok.Position), a.ToImmutable(), new StringType());
        return new ExpressionStmt(call);
    }

    /// <summary>AI SET HFTOKEN token — token HuggingFace per il marketplace RecursiveMAS.</summary>
    private static Stmt ParseAiSetHfTokenStatement(Parser parser, string ns)
    {
        var tok = parser.Consume(TokenKind.HfToken, "Expected 'HFTOKEN' after 'AI SET'.");
        var a = ImmutableArray.CreateBuilder<Expr>();
        a.Add(parser.ParseExpression());
        var nsTok = new Token(TokenKind.Identifier, "AI", null, tok.Position);
        var call = new BuiltinFunctionCallExpr(nsTok,
            new Token(TokenKind.AiSetHfToken, "SETHFTOKEN", null, tok.Position), a.ToImmutable(), new StringType());
        return new ExpressionStmt(call);
    }

    /// <summary>AI SET HFREPO repo — repo HuggingFace di default per il marketplace RecursiveMAS.</summary>
    private static Stmt ParseAiSetHfRepoStatement(Parser parser, string ns)
    {
        var tok = parser.Consume(TokenKind.HfRepo, "Expected 'HFREPO' after 'AI SET'.");
        var a = ImmutableArray.CreateBuilder<Expr>();
        a.Add(parser.ParseExpression());
        var nsTok = new Token(TokenKind.Identifier, "AI", null, tok.Position);
        var call = new BuiltinFunctionCallExpr(nsTok,
            new Token(TokenKind.AiSetHfRepo, "SETHFREPO", null, tok.Position), a.ToImmutable(), new StringType());
        return new ExpressionStmt(call);
    }

    /// <summary>Legacy: AI SET TEMP nome, t.</summary>
    private static Stmt ParseAiSetTempStatement(Parser parser, string ns)
    {
        var tok = parser.Consume(TokenKind.AiSubTemp, "Expected 'TEMP' after 'AI SET'.");
        var a = ImmutableArray.CreateBuilder<Expr>();
        a.Add(parser.ParseExpression());
        while (parser.Match(TokenKind.Comma)) a.Add(parser.ParseExpression());
        var nsTok = new Token(TokenKind.Identifier, "AI", null, tok.Position);
        var call = new BuiltinFunctionCallExpr(nsTok,
            new Token(TokenKind.AiSetTemp, "SETTEMP", null, tok.Position), a.ToImmutable(), new StringType());
        return new ExpressionStmt(call);
    }

    /// <summary>Legacy: AI SET BASE nome, url.</summary>
    private static Stmt ParseAiSetBaseStatement(Parser parser, string ns)
    {
        var tok = parser.Consume(TokenKind.Base, "Expected 'BASE' after 'AI SET'.");
        var a = ImmutableArray.CreateBuilder<Expr>();
        a.Add(parser.ParseExpression());
        while (parser.Match(TokenKind.Comma)) a.Add(parser.ParseExpression());
        var nsTok = new Token(TokenKind.Identifier, "AI", null, tok.Position);
        var call = new BuiltinFunctionCallExpr(nsTok,
            new Token(TokenKind.AiSetBase, "SETBASE", null, tok.Position), a.ToImmutable(), new StringType());
        return new ExpressionStmt(call);
    }

    /// <summary>Legacy: AI SET DEFAULT [name].</summary>
    private static Stmt ParseAiSetDefaultStatement(Parser parser, string ns)
    {
        var tok = parser.Consume(TokenKind.Default, "Expected 'DEFAULT' after 'AI SET'.");
        var args = ImmutableArray.CreateBuilder<Expr>();
        if (!parser.IsAtEndOfStatement()) args.Add(parser.ParseExpression());
        var nsTok = new Token(TokenKind.Identifier, "AI", null, tok.Position);
        var call = new BuiltinFunctionCallExpr(nsTok,
            new Token(TokenKind.AiDef, "SETDEFAULT", null, tok.Position), args.ToImmutable(), new StringType());
        return new ExpressionStmt(call);
    }

    /// <summary>Legacy: AI PREFILL alias, "testo" [, {json}].</summary>
    private static Stmt ParseAiPrefillStatement(Parser parser, string ns)
    {
        var tok = parser.Consume(TokenKind.AiSubPrefill, "Expected 'PREFILL' after 'AI'.");
        var a = ImmutableArray.CreateBuilder<Expr>();
        var first = parser.ParseExpression();
        if (first is VariableExpr ve && ve.NameCleaned.Lexeme == ve.NameSuffixed.Lexeme)
            first = new StringExpr(ve.NameCleaned.Lexeme, ve.NameSuffixed);
        a.Add(first);
        while (parser.Match(TokenKind.Comma)) a.Add(parser.ParseExpression());
        var nsTok = new Token(TokenKind.Identifier, "AI", null, tok.Position);
        var call = new BuiltinFunctionCallExpr(nsTok,
            new Token(TokenKind.AiPrefill, "PREFILL", null, tok.Position), a.ToImmutable(), new StringType());
        return new ExpressionStmt(call);
    }

    /// <summary>Legacy: AI DECODE alias [, max_tokens].</summary>
    private static Stmt ParseAiDecodeStatement(Parser parser, string ns)
    {
        var tok = parser.Consume(TokenKind.AiSubDecode, "Expected 'DECODE' after 'AI'.");
        var a = ImmutableArray.CreateBuilder<Expr>();
        var first = parser.ParseExpression();
        if (first is VariableExpr ve && ve.NameCleaned.Lexeme == ve.NameSuffixed.Lexeme)
            first = new StringExpr(ve.NameCleaned.Lexeme, ve.NameSuffixed);
        a.Add(first);
        while (parser.Match(TokenKind.Comma)) a.Add(parser.ParseExpression());
        var nsTok = new Token(TokenKind.Identifier, "AI", null, tok.Position);
        var call = new BuiltinFunctionCallExpr(nsTok,
            new Token(TokenKind.AiDecode, "DECODE", null, tok.Position), a.ToImmutable(), new StringType());
        return new ExpressionStmt(call);
    }

    /// <summary>Legacy: AI BENCHMARK alias, PREFILL|DECODE|ALL [, {json}] — esegue N
    /// iterazioni di prefill/decode sulla sessione KV dell'agente e registra l'ultimo
    /// benchmark (lettura via AI.BENCHRESULT(alias)). json opzionale {iterations:N, text:"..."}.</summary>
    private static Stmt ParseAiBenchmarkStatement(Parser parser, string ns)
    {
        var tok = parser.Consume(TokenKind.AiSubBenchmark, "Expected 'BENCHMARK' after 'AI'.");
        var a = ImmutableArray.CreateBuilder<Expr>();
        var first = parser.ParseExpression();
        if (first is VariableExpr ve && ve.NameCleaned.Lexeme == ve.NameSuffixed.Lexeme)
            first = new StringExpr(ve.NameCleaned.Lexeme, ve.NameSuffixed);
        a.Add(first);
        parser.Consume(TokenKind.Comma, "Expected ',' after benchmark alias.");
        // modalità: PREFILL | DECODE | ALL
        if (parser.Check(TokenKind.AiSubPrefill)) { parser.Advance(); a.Add(new StringExpr("PREFILL", parser.Previous())); }
        else if (parser.Check(TokenKind.AiSubDecode)) { parser.Advance(); a.Add(new StringExpr("DECODE", parser.Previous())); }
        else if (parser.Check(TokenKind.All)) { parser.Advance(); a.Add(new StringExpr("ALL", parser.Previous())); }
        else
            throw Parser.CreateError(parser.Peek(), "Expected PREFILL, DECODE or ALL as benchmark mode after 'AI BENCHMARK alias,'.");
        // opzionale [, {json}]
        while (parser.Match(TokenKind.Comma)) a.Add(parser.ParseExpression());
        var nsTok = new Token(TokenKind.Identifier, "AI", null, tok.Position);
        var call = new BuiltinFunctionCallExpr(nsTok,
            new Token(TokenKind.AiBenchmark, "BENCHMARK", null, tok.Position), a.ToImmutable(), new StringType());
        return new ExpressionStmt(call);
    }

    /// <summary>Legacy: AI CACHE CLEAR alias | AI CACHE TRIM alias, <n>.</summary>
    private static Stmt ParseAiCacheStatement(Parser parser, string ns)
    {
        parser.Consume(TokenKind.Cache, "Expected 'CACHE' after 'AI'.");
        var verb = parser.Peek();
        return verb.Kind switch
        {
            TokenKind.Clear => ConsumeVerbThenArgs(parser, TokenKind.AiCacheClear, 1, new StringType()),
            TokenKind.Trim => ConsumeVerbThenArgs(parser, TokenKind.AiCacheTrim, 2, new StringType()),
            _ => throw Parser.CreateError(verb, "Expected CLEAR or TRIM after 'AI CACHE'.")
        };
    }


    /// <summary>§4.10 SEMAPRAX — `AI PATCH "target", "expectedRev" ... END PATCH` (rev-bound, lato codice).</summary>
    public static Stmt ParseAiPatchStatement(Parser parser, string ns)
    {
        var verb = parser.Peek();                  // AI PATCH
        parser.Advance();                          // consuma PATCH
        var target = parser.ParseExpression();
        string expectedRev = "";
        if (parser.Match(TokenKind.Comma))
        {
            var revExpr = parser.ParseExpression();
            if (revExpr is StringExpr rs) expectedRev = rs.Value;
        }
        var body = parser.ParseBlockBodyUntil(null, TokenKind.End);
        parser.Consume(TokenKind.End, "Expected END PATCH.");
        if (parser.Peek().Kind == TokenKind.AiPatch ||
            (parser.Peek().Kind == TokenKind.Identifier &&
             string.Equals(parser.Peek().Lexeme, "PATCH", StringComparison.OrdinalIgnoreCase)))
            parser.Advance();
        var targetName = target is StringExpr tse ? tse.Value : "";
        return new SemanticPatchStmt(targetName, expectedRev, body, verb);
    }
}
