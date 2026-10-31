// FILE: Basic.Parser.Parser.Stmt.Train.cs — namespace TRAIN (F0, additivo).
// TRAIN è dedicato alla PRODUZIONE/ALTERAZIONE dei pesi (fine-tune, LoRA, distill,
// eval/quality) e coincide col confine di licenza PUL (vedi AGENTS §Confine-PUL).
// F0: il dispatcher DELEGA ai sottoparser già esistenti (FINETUNE/LORA/DISTILL/QUALITY);
// l'AST emesso conserva il namespace runtime "AI"/"LLM" (lì vivono i simboli), quindi
// `TRAIN <verbo>` è un alias di superficie e i vecchi verbi `AI`/`LLM` restano validi.
// Vedi Todo/(open) - TRAIN-Namespace-Todo.md (fasi F0-F5).

using System.Collections.Immutable;

namespace Basic;

public partial class Parser
{
    /// <summary>Dispatcher del comando legacy multi-parola "TRAIN ..." (namespace TRAIN).
    /// ADDITIVO (F0): delega ai sottoparser dei verbi di training già esistenti. Verbi:
    /// FINETUNE (LoRA SFT), LORA APPLY (deploy adapter), DISTILL (self-distillation),
    /// EVAL/QUALITY (NLL/perplexity + gate). `TRAIN EVAL` è l'alias canonico di `LLM QUALITY`.</summary>
    private static Stmt ParseTrainStatement(Parser parser, string expectedNamespace)
    {
        parser.Consume(TokenKind.Train, "Internal parser error: Expected 'TRAIN' keyword.");
        var commandToken = parser.Peek();
        return commandToken.Kind switch
        {
            TokenKind.AiSubFinetune => ParseTrainFinetuneStatement(parser),
            TokenKind.AiSubLora => parser.At(1).Kind == TokenKind.AiSubLoraApply
                ? ParseAiLoraStatement(parser, expectedNamespace)
                : ParseTrainLoraSubStatement(parser),
            TokenKind.Distill => ParseAiDistillStatement(parser, expectedNamespace),
            TokenKind.Dataset => ParseTrainDatasetStatement(parser, expectedNamespace),
            TokenKind.Eval => ParseTrainEvalStatement(parser),
            TokenKind.Resume => ParseTrainResumeStatement(parser),
            TokenKind.Identifier when commandToken.Lexeme == "QUALITY" => ParseLlmCompilerCall(parser),
            TokenKind.Identifier when commandToken.Lexeme == "DPO" => ParseTrainDpoStatement(parser),
            TokenKind.Identifier when commandToken.Lexeme == "GRPO" => ParseTrainGrpoStatement(parser),
            // Comandi di registro/configurazione (§4): emissione delle funzioni LLM.
            TokenKind.Identifier when commandToken.Lexeme == "RUNS" => ParseTrainRunsCommand(parser),
            TokenKind.Config => ParseTrainConfigCommand(parser),
            TokenKind.Identifier or TokenKind.AiSubStatus when commandToken.Lexeme == "STATUS" => ParseTrainStatusCommand(parser),
            TokenKind.Stop => ParseTrainStopCommand(parser),
            TokenKind.Export => ParseTrainExportCommand(parser),
            _ => throw Parser.CreateError(commandToken,
                "Atteso FINETUNE, LORA, DISTILL, DPO, GRPO, EVAL o QUALITY dopo 'TRAIN'.")
        };
    }

    /// <summary>`TRAIN STOP &lt;id&gt;` — rimuove una run dal registro (LLM.RUNSTOP).</summary>
    private static Stmt ParseTrainStopCommand(Parser parser)
    {
        var st = parser.Peek(); parser.Advance();       // STOP
        return TrainFnRet(parser, st, TokenKind.LlmRunStop, "RUNSTOP", TrainNumType, ParseTrainArgOrStr(parser));
    }

    /// <summary>`TRAIN EXPORT &lt;out&gt;` — esporta l'adapter dell'ultima run (LLM.TRAINEXPORT).</summary>
    private static Stmt ParseTrainExportCommand(Parser parser)
    {
        var ex = parser.Peek(); parser.Advance();       // EXPORT
        var first = ParseTrainArgOrStr(parser);
        // TRAIN EXPORT <out> | <run>, <out>: 2 arg → emette LLM.TRAINEXPORT(run, out).
        if (parser.Match(TokenKind.Comma))
            return TrainFnRet(parser, ex, TokenKind.LlmTrainExport, "TRAINEXPORT", TrainNumType,
                first, ParseTrainArgOrStr(parser));
        return TrainFnRet(parser, ex, TokenKind.LlmTrainExport, "TRAINEXPORT", TrainNumType, first);
    }

    /// <summary>Un termine non quotato è una STRINGA letterale (convenzione dei comandi).</summary>
    private static Expr ParseTrainArgOrStr(Parser parser)
    {
        var e = parser.ParseExpression();
        if (e is VariableExpr ve && ve.NameCleaned.Lexeme == ve.NameSuffixed.Lexeme)
            return new StringExpr(ve.NameCleaned.Lexeme, ve.NameSuffixed);
        return e;
    }

    private static bool LexIs(Token t, string s) => string.Equals(t.Lexeme, s, StringComparison.OrdinalIgnoreCase);

    /// <summary>Comandi-query (§4): il comando AGISCE e STAMPA il risultato (non è una funzione,
    /// §1). Emette `PRINT LLM.<fn>(...)`: stesso path di codegen del PRINT (parità 4 backend).</summary>
    private static Stmt TrainFn(Parser parser, Token verb, TokenKind tok, string fnName, params Expr[] args)
        => TrainFnRet(parser, verb, tok, fnName, new StringType(), args);

    /// <summary>Come <see cref="TrainFn"/>, ma con tipo di ritorno esplicito (stringa o numero):
    /// il PRINT deve sapere se stampare una stringa o un valore numerico (parità 4 backend).</summary>
    private static Stmt TrainFnRet(Parser parser, Token verb, TokenKind tok, string fnName, DataType ret, params Expr[] args)
    {
        var call = new BuiltinFunctionCallExpr(
            new Token(TokenKind.Identifier, "LLM", null, verb.Position),
            new Token(tok, fnName, null, verb.Position), args.ToImmutableArray(), ret);
        return new PrintStmt(ImmutableArray.Create(new PrintArg(call, null)), verb);
    }

    private static readonly DataType TrainNumType = new DoubleType();

    /// <summary>`TRAIN RUNS LIST | SHOW &lt;id&gt; | PRUNE` — registro run (alias delle funzioni LLM).</summary>
    private static Stmt ParseTrainRunsCommand(Parser parser)
    {
        var runs = parser.Peek(); parser.Advance();     // RUNS
        var sub = parser.Peek();
        if (LexIs(sub, "LIST")) { parser.Advance(); return TrainFn(parser, runs, TokenKind.LlmRunsJson, "RUNSJSON"); }
        if (LexIs(sub, "PRUNE")) { parser.Advance(); return TrainFnRet(parser, runs, TokenKind.LlmRunsClear, "RUNSCLEAR", TrainNumType); }
        if (LexIs(sub, "SHOW"))
        {
            parser.Advance();
            var id = parser.ParseExpression();
            return TrainFn(parser, runs, TokenKind.LlmRunsItem, "RUNSITEM", id);
        }
        throw Parser.CreateError(sub, "Atteso LIST, SHOW o PRUNE dopo 'TRAIN RUNS'.");
    }

    /// <summary>`TRAIN CONFIG SHOW | SET &lt;chiave&gt;, &lt;valore&gt;` — default di sessione.</summary>
    private static Stmt ParseTrainConfigCommand(Parser parser)
    {
        var cfg = parser.Consume(TokenKind.Config, "Expected 'CONFIG' after 'TRAIN'.");
        var sub = parser.Peek();
        if (LexIs(sub, "SHOW")) { parser.Advance(); return TrainFn(parser, cfg, TokenKind.LlmTrainConfig, "TRAINCONFIG"); }
        if (LexIs(sub, "SET"))
        {
            parser.Advance();
            var k = parser.ParseExpression();
            if (parser.Match(TokenKind.Comma)) { }
            var v = parser.ParseExpression();
            return TrainFn(parser, cfg, TokenKind.LlmTrainConfigSet, "TRAINCONFIGSET", k, v);
        }
        throw Parser.CreateError(sub, "Atteso SHOW o SET dopo 'TRAIN CONFIG'.");
    }

    /// <summary>`TRAIN STATUS [&lt;id&gt;]` — stato del registro run (intero o di una run).</summary>
    private static Stmt ParseTrainStatusCommand(Parser parser)
    {
        var st = parser.Peek(); parser.Advance();       // STATUS
        if (parser.IsAtEndOfStatement() || parser.Check(TokenKind.Eof))
            return TrainFn(parser, st, TokenKind.LlmRunsJson, "RUNSJSON");
        var id = parser.ParseExpression();
        return TrainFn(parser, st, TokenKind.LlmRunsItem, "RUNSITEM", id);
    }

    /// <summary>Chiamata a un builtin del namespace AI (FINETUNE/DPO).</summary>
    private static BuiltinFunctionCallExpr TrainAiCall(Token verb, TokenKind kind, string name, params Expr[] args)
        => new(new Token(TokenKind.Identifier, "AI", null, verb.Position),
               new Token(kind, name, null, verb.Position), args.ToImmutableArray(), new StringType());

    /// <summary>Ripiega gli argomenti POSIZIONALI del §4 nelle opzioni (dict):
    ///   <c>&lt;model&gt;, &lt;k2&gt;, &lt;k3&gt;[, {opts}]</c> → <c>{"&lt;k2&gt;": .., "&lt;k3&gt;": .., ...opts}</c>
    /// (o <c>&lt;model&gt;[, {opts}]</c>, forma storica). Ritorna null se non c'è alcun argomento dopo il modello.</summary>
    private static Expr? TrainFoldOpts(Parser parser, Token tok, string k2, string k3)
    {
        if (!parser.Match(TokenKind.Comma)) return null;
        var a2 = parser.ParseExpression();
        if (a2 is DictionaryLiteralExpr d0) return d0;      // <model>, {opts}
        var entries = ImmutableArray.CreateBuilder<DictionaryEntry>();
        if (k2.Length > 0) entries.Add(new DictionaryEntry(new StringExpr(k2, tok), a2));
        if (parser.Match(TokenKind.Comma))
        {
            var a3 = parser.ParseExpression();
            if (a3 is DictionaryLiteralExpr d3) entries.AddRange(d3.Entries);
            else
            {
                if (k3.Length > 0) entries.Add(new DictionaryEntry(new StringExpr(k3, tok), a3));
                if (parser.Match(TokenKind.Comma) && parser.ParseExpression() is DictionaryLiteralExpr d4) entries.AddRange(d4.Entries);
            }
        }
        return new DictionaryLiteralExpr(entries.ToImmutable(), tok);
    }

    /// <summary>`TRAIN FINETUNE &lt;model&gt;, &lt;dataset&gt;, &lt;out&gt;[, {rank,layer,lr,steps,batch,seed,frozen,engine}]`
    /// (§4) oppure `&lt;model&gt;[, {opts}]`. Il dataset e l'out posizionali sono ripiegati in
    /// `{dataset:.., ckpt:..}` (emette il builtin `AI.FINETUNE(model, opts)`).</summary>
    private static Stmt ParseTrainFinetuneStatement(Parser parser)
    {
        var tok = parser.Consume(TokenKind.AiSubFinetune, "Expected 'FINETUNE' after 'TRAIN'.");
        var model = ParseTrainArgOrStr(parser);
        var opts = TrainFoldOpts(parser, tok, "dataset", "ckpt");
        return new ExpressionStmt(opts is null
            ? TrainAiCall(tok, TokenKind.AiFinetune, "FINETUNE", model)
            : TrainAiCall(tok, TokenKind.AiFinetune, "FINETUNE", model, opts));
    }

    /// <summary>`TRAIN DPO &lt;model&gt;, &lt;pairs&gt;, &lt;out&gt;[, {beta,lr,steps,engine}]` (§4) oppure
    /// `&lt;model&gt;, {chosen,rejected,ckpt,…}`. La coppia `<pairs>` è un PREFISSO: il runtime usa
    /// `&lt;pairs&gt;.chosen.bin`/`&lt;pairs&gt;.rejected.bin`; `<out>` = ckpt. Emette `AI.DPO(model, opts)`.</summary>
    private static Stmt ParseTrainDpoStatement(Parser parser)
    {
        var tok = parser.Peek();
        parser.Advance();   // DPO
        var model = ParseTrainArgOrStr(parser);
        var opts = TrainFoldOpts(parser, tok, "pairs", "ckpt");
        return new ExpressionStmt(opts is null
            ? TrainAiCall(tok, TokenKind.AiDpo, "DPO", model)
            : TrainAiCall(tok, TokenKind.AiDpo, "DPO", model, opts));
    }

    /// <summary>`TRAIN GRPO &lt;model&gt;, &lt;dataset&gt;, &lt;out&gt;[, {group,maxnew,clip,kl,lr,steps,temperature}]`
    /// (§4) oppure `&lt;model&gt;, {dataset,ckpt,…}` — RLHF senza critic (RLVR). Il dataset e l'out
    /// posizionali sono ripiegati in `{dataset:.., ckpt:..}` (emette `AI.GRPO(model, opts)`).</summary>
    private static Stmt ParseTrainGrpoStatement(Parser parser)
    {
        var tok = parser.Peek();
        parser.Advance();   // GRPO
        var model = ParseTrainArgOrStr(parser);
        var opts = TrainFoldOpts(parser, tok, "dataset", "ckpt");
        return new ExpressionStmt(opts is null
            ? TrainAiCall(tok, TokenKind.AiGrpo, "GRPO", model)
            : TrainAiCall(tok, TokenKind.AiGrpo, "GRPO", model, opts));
    }

    /// <summary>`TRAIN LORA MERGE|SAVE|LOAD …` — alias dei comandi `LLM MODEL`:
    ///   MERGE &lt;base&gt;, &lt;adapter&gt;, &lt;out&gt;[, &lt;alpha&gt;] → `LLM.MODELADAPTER` (fonde A/B nella base)
    ///   SAVE  &lt;model&gt;, &lt;out&gt;                         → `LLM.MODELSAVE` (esporta)
    ///   LOAD  &lt;model&gt;, &lt;path&gt;[, RESIDENT]              → `LLM.MODELLOAD` (carica)
    /// Emette gli STESSI builtin LLM → runtime condiviso, parità 4 backend.</summary>
    private static Stmt ParseTrainLoraSubStatement(Parser parser)
    {
        var loraTok = parser.Consume(TokenKind.AiSubLora, "Expected 'LORA' after 'TRAIN'.");
        var sub = parser.Peek();
        TokenKind verbKind;
        if (sub.Kind == TokenKind.LlmMerge || string.Equals(sub.Lexeme, "MERGE", StringComparison.OrdinalIgnoreCase))
            verbKind = TokenKind.AiModelAdapter;
        else if (sub.Kind == TokenKind.Save || string.Equals(sub.Lexeme, "SAVE", StringComparison.OrdinalIgnoreCase))
            verbKind = TokenKind.LlmLoraSave;   // §4: exporter adapter RAW -> .safetensors
        else if (sub.Kind == TokenKind.Load || string.Equals(sub.Lexeme, "LOAD", StringComparison.OrdinalIgnoreCase))
            verbKind = TokenKind.LlmLoraLoad;   // §4: importer .safetensors -> adapter RAW
        else if (string.Equals(sub.Lexeme, "INFO", StringComparison.OrdinalIgnoreCase))
        {
            // TRAIN LORA INFO — registry adapter (alias di LLM.LORALIST()).
            parser.Advance();
            return TrainFn(parser, loraTok, TokenKind.LlmLoraList, "LORALIST");
        }
        else
            throw Parser.CreateError(sub, "Atteso APPLY, MERGE, SAVE, LOAD o INFO dopo 'TRAIN LORA'.");
        parser.Advance();
        var args = ImmutableArray.CreateBuilder<Expr>();
        args.Add(ParseAiModelArg(parser));
        while (parser.Match(TokenKind.Comma)) args.Add(ParseAiModelArg(parser));
        var nsTok = new Token(TokenKind.Identifier, "LLM", null, loraTok.Position);
        var call = new BuiltinFunctionCallExpr(nsTok,
            new Token(verbKind, sub.Lexeme, null, loraTok.Position),
            args.ToImmutable(), new StringType());
        return new ExpressionStmt(call);
    }

    /// <summary>`TRAIN RESUME &lt;model&gt; [, {opts}]` — avvia/riprende il fine-tune LoRA del modello
    /// (alias di `LLM FINETUNE`; `{ckpt}` seleziona il checkpoint da cui continuare). Emette lo
    /// STESSO builtin `AI.FINETUNE` → runtime e parità 4 backend invariati.</summary>
    private static Stmt ParseTrainResumeStatement(Parser parser)
    {
        var tok = parser.Consume(TokenKind.Resume, "Expected 'RESUME' after 'TRAIN'.");
        var model = ParseTrainArgOrStr(parser);   // <run> = modello/ckpt di base della run
        if (!parser.Match(TokenKind.Comma))       // TRAIN RESUME <model>
            return new ExpressionStmt(TrainAiCall(tok, TokenKind.AiFinetune, "FINETUNE", model));
        var a2 = parser.ParseExpression();
        Expr opts;
        if (a2 is DictionaryLiteralExpr d) opts = d;   // <model>, {opts}
        else
        {
            // §4: <run>, <steps>[, {opts}] → {"steps": <n>, ...opts}
            var entries = ImmutableArray.CreateBuilder<DictionaryEntry>();
            entries.Add(new DictionaryEntry(new StringExpr("steps", tok), a2));
            if (parser.Match(TokenKind.Comma) && parser.ParseExpression() is DictionaryLiteralExpr d2) entries.AddRange(d2.Entries);
            opts = new DictionaryLiteralExpr(entries.ToImmutable(), tok);
        }
        return new ExpressionStmt(TrainAiCall(tok, TokenKind.AiFinetune, "FINETUNE", model, opts));
    }

    /// <summary>`TRAIN DATASET BUILD|SAMPLE &lt;jsonl&gt; [, {opts}]` — alias canonico di
    /// `AI DISTILL BUILD|SAMPLE` (preparazione dataset: tokenizza+finestra → .train/.hold.bin).
    /// Emette lo STESSO builtin (`AI.DISTILLBUILD`/`AI.DISTILLSAMPLE`), parità 4 backend.</summary>
    private static Stmt ParseTrainDatasetStatement(Parser parser, string expectedNamespace)
    {
        var tok = parser.Consume(TokenKind.Dataset, "Expected 'DATASET' after 'TRAIN'.");
        var verb = parser.Peek();
        // `TRAIN DATASET STATS <file>` — comando (llm namespace): statistiche del dataset.
        // Emette la FUNZIONE `LLM.DATASETSTATS(file)` (builtin 2306, runtime condiviso), così la
        // parità 4 backend resta invariata. Il valore JSON è scartato (comando); pari al C.
        if (LexIs(verb, "STATS"))
        {
            parser.Advance();
            return TrainFn(parser, tok, TokenKind.LlmDatasetStats, "DATASETSTATS", ParseTrainArgOrStr(parser));
        }
        TokenKind? callKind = verb.Kind switch
        {
            TokenKind.Build => TokenKind.AiDistillBuild,
            TokenKind.Sample => TokenKind.AiDistillSample,
            _ => null,
        };
        if (callKind is null)
            throw Parser.CreateError(verb, "Atteso BUILD, SAMPLE o STATS dopo 'TRAIN DATASET'.");
        parser.Advance();
        var args = ImmutableArray.CreateBuilder<Expr>();
        args.Add(parser.ParseExpression());   // <jsonl>
        if (parser.Match(TokenKind.Comma))
        {
            var second = parser.ParseExpression();
            if (second is DictionaryLiteralExpr dl) args.Add(dl);   // {opts} canonico
            else
            {
                // Forma §4 `TRAIN DATASET BUILD <jsonl>, <out>[, {opts}]`: il positional <out>
                // viene ripiegato in `{"out": <expr>}` (il C manda la stringa grezza; il runtime
                // tratta entrambe → out identico, parità 4 backend).
                var entries = ImmutableArray.CreateBuilder<DictionaryEntry>();
                entries.Add(new DictionaryEntry(new StringExpr("out", tok), second));
                if (parser.Match(TokenKind.Comma) && parser.ParseExpression() is DictionaryLiteralExpr dl2)
                    entries.AddRange(dl2.Entries);
                args.Add(new DictionaryLiteralExpr(entries.ToImmutable(), tok));
            }
        }
        var nsTok = new Token(TokenKind.Identifier, "AI", null, tok.Position);
        var call = new BuiltinFunctionCallExpr(nsTok,
            new Token(callKind.Value, "DISTILL" + verb.Lexeme, null, tok.Position),
            args.ToImmutable(), new StringType());
        return new ExpressionStmt(call);
    }

    /// <summary>`TRAIN EVAL &lt;modello&gt;, &lt;prompt/dataset&gt; [, &lt;out&gt;]` — alias canonico di
    /// `LLM QUALITY` (NLL/perplexity + gate). Emette lo STESSO builtin (`LLM.QUALITY`) così il
    /// runtime e la parità 4 backend non cambiano; gli argomenti non quotati sono stringhe
    /// letterali (convenzione dei comandi, come `ParseAiModelArg`).</summary>
    private static Stmt ParseTrainEvalStatement(Parser parser)
    {
        // `TRAIN EVAL VALIDATE <result> AGAINST [VAULT] <folder>` — alias di `AI EVAL VALIDATE` (§4):
        // eval-runtime (match del risultato contro il vault). Emette lo stesso builtin AI.EVALVALIDATE.
        if (parser.Peek().Kind == TokenKind.Eval && parser.At(1).Kind == TokenKind.Validate)
            return ParseAiEvalValidateStatement(parser, "TRAIN");
        var verb = parser.Consume(TokenKind.Eval, "Expected 'EVAL' after 'TRAIN'.");
        // `TRAIN EVAL GATE <ref>, <cand>` — comando gate DEDICATO (§4): confronta la perplexity
        // del candidato vs il riferimento → verdetto ACCEPT/REJECT-FLAG (builtin LLM.EVALGATE).
        // Stampa il verdetto (§1: comando). Il token GATE disambigua dal `TRAIN EVAL` semplice.
        if (LexIs(parser.Peek(), "GATE"))
        {
            parser.Advance();   // GATE
            var refM = ParseAiModelArg(parser);
            if (parser.Match(TokenKind.Comma)) { }
            var candM = ParseAiModelArg(parser);
            while (parser.Match(TokenKind.Comma)) ParseAiModelArg(parser);   // {metric} opzionale (ignorato)
            var gateCall = new BuiltinFunctionCallExpr(
                new Token(TokenKind.Identifier, "LLM", null, verb.Position),
                new Token(TokenKind.LlmEvalGate, "EVALGATE", null, verb.Position),
                ImmutableArray.Create(refM, candM), new StringType());
            return new PrintStmt(ImmutableArray.Create(new PrintArg(gateCall, null)), verb);
        }
        var nsTok = new Token(TokenKind.Identifier, "LLM", null, verb.Position);
        var args = ImmutableArray.CreateBuilder<Expr>();
        while (!parser.IsAtEndOfStatement() && !parser.Check(TokenKind.Eof))
        {
            if (parser.Match(TokenKind.Comma)) continue;
            var beforePos = parser.Peek().Position;
            args.Add(ParseAiModelArg(parser));
            if (parser.Peek().Position == beforePos) break;   // nessun avanzamento: evita loop
        }
        var call = new BuiltinFunctionCallExpr(nsTok,
            new Token(TokenKind.LlmQuality, "QUALITY", null, verb.Position),
            args.ToImmutable(), new StringType());
        return new ExpressionStmt(call);
    }
}
