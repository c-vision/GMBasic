// FILE: IlBackendCodegen/Partial/IlProgramCompiler.Statements.Network.cs
// PARTIAL — dominio STATEMENTS/FILE+NET+HTTP: close/open/write/read/seek/kill/lock,
// TCP/UDP/SSL/WS/HTTP, memory-map, set-option/shutdown/using.
// Migrato da IlProgramCompiler.Statements.cs (2026-08-30 — split, file <= 1200 righe).

using System.Collections.Immutable;
using System.Linq;
using System.Reflection;
using System.Reflection.Emit;
using System.Reflection.Metadata;
using System.Reflection.Metadata.Ecma335;
using System.Reflection.PortableExecutable;
using Basic.IlBackend.Gpm;
using Basic.Runtime.Gpm;
using Basic;

namespace Basic.IlBackend;

/// <summary>Partial del transpiler IL: statement file/rete/http/memorymap.</summary>
public sealed partial class IlProgramCompiler
{
    private void EmitCloseStatement(CloseStmt stmt)
    {
        if (stmt.IsIsam)
        {
            // ISAM CLOSE [alias, ...]: SEMPRE IlRuntime.IsamClose (il nodo e' marcato dal parser: nessuna
            // euristica sul tipo). Prima un alias bare non dichiarato risolveva a tipo numerico e finiva su
            // FileCloseNumber(0) = no-op: l'area restava aperta. Senza argomenti = area corrente (null).
            if (stmt.FileNumbers.Length == 0)
            {
                _il.Emit(OpCodes.Ldnull);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.IsamClose), typeof(string)));
                return;
            }
            foreach (var area in stmt.FileNumbers)
            {
                if (area is VariableExpr isamVe) _il.Emit(OpCodes.Ldstr, isamVe.NameCleaned.Lexeme);
                else if (area is StringExpr isamSe) _il.Emit(OpCodes.Ldstr, isamSe.Value ?? "");
                else EmitStringArg(area);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.IsamClose), typeof(string)));
            }
            return;
        }
        if (stmt.FileNumbers.Length == 0)
        {
            // CLOSE senza handle: chiude TUTTI i file aperti (parità VM).
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.FileCloseAll)));
            return;
        }
        foreach (var fileNumber in stmt.FileNumbers)
        {
            // Bug ISAM-Comandi-Parita-5-Backend (BUG 3): CloseStmt e' l'AST CONDIVISO tra
            // CLOSE #n (file I/O) e ISAM CLOSE <alias> — ParseIsamCloseStatement produce
            // lo stesso nodo, nessun modo di distinguerli qui se non dal TIPO
            // dell'espressione. Un alias ISAM bare (es. "C") veniva sempre trattato come
            // file number numerico via EmitInt64Argument: se l'alias non collide con
            // nessuna variabile risolveva silenziosamente a 0 (chiudeva l'area sbagliata);
            // se collide con una variabile DIM NON numerica omonima (es. un UDT — bug
            // reale trovato: ALIAS C + DIM c AS Rec), EmitInt64Argument tenta di
            // convertire il valore di quella variabile a Int64 → IL non valido
            // (InvalidProgramException). Se l'espressione e' una bare variabile il cui
            // tipo risolto NON e' numerico, e' un alias ISAM: nome letterale (come
            // EmitIsamAliasArg) + IlRuntime.IsamClose(string), mai FileCloseNumber(long).
            if (fileNumber is VariableExpr fcVe &&
                !IlConversion.IsNumeric(NormalizeInferredType(_program.Symbols.GetSymbol(fcVe.SymbolId).DataType)))
            {
                _il.Emit(OpCodes.Ldstr, fcVe.NameCleaned.Lexeme);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.IsamClose), typeof(string)));
                continue;
            }
            EmitInt64Argument(fileNumber);
            _il.Emit(OpCodes.Call, FileCloseNumberMethod);
            _il.Emit(OpCodes.Pop);
        }
    }

    private void EmitOpenStatement(OpenStmt stmt)
    {
        var modeStr = stmt.Mode switch
        {
            FileOpenMode.Input => "r",
            FileOpenMode.Output => "w",
            FileOpenMode.Append => "a",
            FileOpenMode.Binary => "rb",
            FileOpenMode.Random => "r+",
            _ => throw new NotSupportedException($"Backend IL: OPEN FOR {stmt.Mode} non ancora supportato.")
        };
        EmitExpression(stmt.FileName);
        _il.Emit(OpCodes.Ldstr, modeStr);
        EmitInt64Argument(stmt.FileNumber);
        // Content type (1 = JSON, parità PCode OPEN): serve al PRINT #h per serializzare JSON.
        _il.Emit(OpCodes.Ldc_I8, stmt.FileContentType is JsonType ? 1L : 0L);
        _il.Emit(OpCodes.Call, FileOpenJsonMethod);
        _il.Emit(OpCodes.Pop); // scarta long di ritorno
        // OPEN … FOR RANDOM [LEN = n]: abilita il record I/O (PUT/GET su UDT, layout canonico SIZEOF).
        if (stmt.Mode == FileOpenMode.Random)
        {
            EmitInt64Argument(stmt.FileNumber);
            if (stmt.RecordLength is not null) EmitInt64Argument(stmt.RecordLength);
            else _il.Emit(OpCodes.Ldc_I8, 0L);
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.FileMarkRandom), typeof(long), typeof(long)));
        }
    }

    /// <summary>
    /// Campi dell'UDT ammessi nel record di un file RANDOM: (FieldBuilder del campo struct, token) in ordine
    /// di dichiarazione, più il tipo CLR della struct; false se l'UDT ha campi non ammessi
    /// (TypeLayout.RandomRecordToken) — PUT/GET restano sul percorso legacy.
    /// </summary>
    private bool TryRandomRecordFields(DataType type, out List<(TypeField Field, FieldInfo Info)> fields, out string tokens, out Type? structClr)
    {
        fields = new List<(TypeField, FieldInfo)>();
        tokens = "";
        structClr = null;
        if (NormalizeInferredType(type) is not UserDefinedType udt
            || !TypeLayout.TryRandomRecordDescriptor(udt, null, out var desc))
            return false;
        tokens = CommonLib.RandomRecordCodec.ParseDescriptor(desc).Tokens;
        foreach (var field in udt.Fields)
        {
            if (field.SymbolId < 0 || !_classFields.TryGetValue(field.SymbolId, out var fb)
                || fb.IsStatic || !fb.DeclaringType!.IsValueType)
                return false;
            structClr ??= fb.DeclaringType;
            fields.Add((field, fb));
        }
        return fields.Count > 0;
    }

    private void EmitWriteFileStatement(WriteFileStmt stmt)
    {
        // Formato WRITE # stile BASIC (parità PCode WRITE_FILE): campi formattati da
        // IlRuntime.WriteField (stringhe quotate, -1/0 double → TRUE/FALSE...), separati
        // da virgola, riga chiusa da newline. Prima scriveva il testo nudo concatenato.
        for (int i = 0; i < stmt.Arguments.Length; i++)
        {
            if (i > 0)
            {
                EmitInt64Argument(stmt.FileNumber);
                _il.Emit(OpCodes.Ldstr, ",");
                _il.Emit(OpCodes.Call, FileWriteNumberMethod);
                _il.Emit(OpCodes.Pop);
            }
            EmitInt64Argument(stmt.FileNumber);
            var arg = stmt.Arguments[i];
            EmitExpression(arg);
            var argClr = DataTypeMapper.Map(NormalizeInferredType(InferType(arg)), _types, _classTypes);
            if (argClr.IsValueType) _il.Emit(OpCodes.Box, argClr);
            _il.Emit(OpCodes.Call, FileWriteFieldMethod);
            _il.Emit(OpCodes.Call, FileWriteNumberMethod);
            _il.Emit(OpCodes.Pop);
        }
        EmitInt64Argument(stmt.FileNumber);
        _il.Emit(OpCodes.Ldstr, "\n");
        _il.Emit(OpCodes.Call, FileWriteNumberMethod);
        _il.Emit(OpCodes.Pop);
    }

    private void EmitPrintFileStatement(PrintFileStmt stmt)
    {
        foreach (var arg in stmt.Arguments)
        {
            EmitInt64Argument(stmt.FileNumber);
            if (arg.Expression is not null)
            {
                EmitExpression(arg.Expression);
                // Box a object: il runtime decide (JSON per un handle JSON, ToString altrimenti).
                var argClr = DataTypeMapper.Map(NormalizeInferredType(InferType(arg.Expression)), _types, _classTypes);
                if (argClr.IsValueType) _il.Emit(OpCodes.Box, argClr);
            }
            else
            {
                // Separatore: "," → tab (parità PRINT), ";" → nulla.
                _il.Emit(OpCodes.Ldstr, arg.Separator == "," ? "\t" : "");
            }
            _il.Emit(OpCodes.Call, FileWriteNumberOrJsonMethod);
            _il.Emit(OpCodes.Pop);
        }
        // PRINT # termina con ';' o ',' come ULTIMO argomento → NESSUN a capo (parità BASIC).
        var endsWithSeparator = stmt.Arguments.Length > 0
            && stmt.Arguments[^1] is { Expression: null, Separator: ";" or "," };
        if (endsWithSeparator) return;
        EmitInt64Argument(stmt.FileNumber);
        _il.Emit(OpCodes.Ldstr, "\n");
        _il.Emit(OpCodes.Call, FileWriteNumberMethod);
        _il.Emit(OpCodes.Pop);
    }

    private void EmitInputFileStatement(InputFileStmt stmt)
    {
        // INPUT # canonico (parità PCode/VM-C/C99*): legge UNA riga, la divide nei campi
        // CSV (quote-aware, TRUE/FALSE) e assegna il campo i-esimo a ogni target con la
        // conversione del tipo. Prima: una RIGA INTERA per target → multi-var leggeva
        // righe diverse (garbage) e i numerici non convertivano (divergenza 4 backend).
        var inputFileNum = _il.DeclareLocal(typeof(long));
        EmitInt64Argument(stmt.FileNumber);
        _il.Emit(OpCodes.Stloc, inputFileNum);

        // Canale JSON (`JSON OPEN ... AS #n`): INPUT # legge l'intero documento nelle variabili JSON/ANY (parità
        // PCode INPUT_FILE con content type 1). Il controllo e' a RUNTIME (il tipo del canale non e' noto a compile
        // time) ed e' emesso solo se c'e' almeno un target JSON/ANY.
        var inputJsonTargets = stmt.Variables
            .OfType<VariableTarget>()
            .Where(v => NormalizeInferredType(_program.Symbols.GetSymbol(v.SymbolId).DataType) is Basic.JsonType or AnyType)
            .ToList();
        var inputCsvLabel = _il.DefineLabel();
        var inputEndLabel = _il.DefineLabel();
        if (inputJsonTargets.Count > 0)
        {
            _il.Emit(OpCodes.Ldloc, inputFileNum);
            _il.Emit(OpCodes.Call, FileIsJsonMethod);
            _il.Emit(OpCodes.Brfalse, inputCsvLabel);
            var parsedLocal = _il.DeclareLocal(typeof(object));
            _il.Emit(OpCodes.Ldloc, inputFileNum);
            _il.Emit(OpCodes.Call, FileReadJsonNumberMethod);
            _il.Emit(OpCodes.Stloc, parsedLocal);
            foreach (var jt in inputJsonTargets)
                EmitStoreVariable(jt.SymbolId, () => _il.Emit(OpCodes.Ldloc, parsedLocal));
            _il.Emit(OpCodes.Br, inputEndLabel);
        }
        _il.MarkLabel(inputCsvLabel);

        _il.Emit(OpCodes.Ldloc, inputFileNum);
        _il.Emit(OpCodes.Call, FileReadCsvLineNumberMethod);
        var fieldsLocal = _il.DeclareLocal(typeof(string[]));
        _il.Emit(OpCodes.Stloc, fieldsLocal);

        for (var i = 0; i < stmt.Variables.Length; i++)
        {
            if (stmt.Variables[i] is not VariableTarget vt) continue;
            var targetType = NormalizeInferredType(_program.Symbols.GetSymbol(vt.SymbolId).DataType);
            if (targetType is StringType)
            {
                var fieldIndex = i;
                EmitStoreVariable(vt.SymbolId, () =>
                {
                    _il.Emit(OpCodes.Ldloc, fieldsLocal);
                    _il.Emit(OpCodes.Ldc_I4, fieldIndex);
                    _il.Emit(OpCodes.Call, CsvFieldToStringMethod);
                });
            }
            else
            {
                var fieldIndex = i;
                var targetClr = DataTypeMapper.Map(targetType, _types, _classTypes);
                EmitStoreVariable(vt.SymbolId, () =>
                {
                    _il.Emit(OpCodes.Ldloc, fieldsLocal);
                    _il.Emit(OpCodes.Ldc_I4, fieldIndex);
                    _il.Emit(OpCodes.Call, CsvFieldToDoubleMethod);
                    IlConversion.EmitConvert(_il, _types, new DoubleType(), _types.Double, targetClr);
                });
            }
        }
        _il.MarkLabel(inputEndLabel);
    }

    private void EmitLineInputFileStatement(LineInputFileStmt stmt)
    {
        EmitReadFileLineInto(stmt.Variable, stmt.FileNumber);
    }

    /// <summary>Legge una riga da un file number e la scrive nel target. Usa
    /// <see cref="EmitStoreVariable(int,Action)"/> (GLOBAL-AWARE: local/global/static/param)
    /// invece di uno <c>Stloc</c> diretto: con <c>Stloc GetOrDeclareLocal</c> un target di
    /// MODULO (codice top-level, es. `USING … INPUT #G, s$` o `INPUT #2, s$` senza SUB Main)
    /// NON veniva scritto (il global restava vuoto → "INPUT #" a top-level leggeva vuoto).</summary>
    private void EmitReadFileLineInto(Target? target, Expr fileNumber)
    {
        if (target is VariableTarget vt)
        {
            EmitStoreVariable(vt.SymbolId, () =>
            {
                EmitInt64Argument(fileNumber);
                _il.Emit(OpCodes.Call, FileReadLineNumberMethod);
            });
        }
        else
        {
            EmitInt64Argument(fileNumber);
            _il.Emit(OpCodes.Call, FileReadLineNumberMethod);
            _il.Emit(OpCodes.Pop);
        }
    }

    private void EmitFetchStatement(HttpFetchStmt stmt)
    {
        // FETCH asincrono con CALLBACK (FETCH url[, opts] INTO Sub): il delegate della SUB
        // callback (Action<string,string>) viene creato a runtime via reflection sul SymbolId
        // (ldftn su un MethodBuilder non ancora CreateType produrrebbe InvalidProgramException).
        // Il runtime esegue la richiesta in background e invoca Sub(response$, error$).
        if (stmt.CallbackName is not null && stmt.CallbackName.Kind != TokenKind.Eof)
        {
            EmitExpression(stmt.Url);
            EmitConvertToString(InferType(stmt.Url));
            if (stmt.Options is not null) EmitExpression(stmt.Options);
            else _il.Emit(OpCodes.Ldnull);

            var normalized = stmt.CallbackName.Lexeme.Replace("_", "");
            var definition = _procedureDefinitions.Values
                .FirstOrDefault(d => d.OriginalName.Lexeme.Replace("_", "").Equals(normalized, StringComparison.OrdinalIgnoreCase));
            if (definition is null)
                throw new NotSupportedException($"Backend IL: procedura '{stmt.CallbackName.Lexeme}' non trovata per la callback FETCH.");
            if (definition.Parameters.Length != 2
                || definition.Parameters[0].DataType is not StringType
                || definition.Parameters[1].DataType is not StringType)
                throw new NotSupportedException($"Backend IL: la callback '{stmt.CallbackName.Lexeme}' deve essere una SUB con due parametri STRING (response$, error$).");

        // La callback è dispatchata come TASK GPM (IlRuntime.DispatchHandlerAsGpmOrDirect) —
        // mai in diretta sul thread di Task.Run.
        _il.Emit(OpCodes.Ldc_I4, definition.SymbolId);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.HttpFetchAsync)));
        return;
        }
        EmitExpression(stmt.Url);
        EmitConvertToString(InferType(stmt.Url));
        _il.Emit(OpCodes.Call, IslandRuntimeFetchMethod);
        _il.Emit(OpCodes.Pop); // scarta risultato (FETCH BASIC è fire-and-forget)
    }

    private void EmitSeekStatement(SeekStmt stmt)
    {
        // SEEK n, pos — COMANDO legacy, posizione 1-based (`SEEK 1, 1` = torna all'inizio).
        // Il primo argomento e' un NUMERO DI FILE (`OPEN ... AS #n`), non un handle di FOpen:
        // prima veniva passato a IFileService.Seek come handle, quindi il seek non aveva
        // effetto (finiva su un altro stream). Ora risolve il numero e sposta il cursore,
        // con la stessa semantica del C99 (C#) che emette (pos - 1) (§ MiniTests/io/io_misc.bas).
        EmitInt64Argument(stmt.FileNumber);
        EmitInt64Argument(stmt.PositionExpr);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.FileSetPosNumber)));
        _il.Emit(OpCodes.Pop);
    }

    private void EmitKillStatement(KillStmt stmt)
    {
        EmitLoadFileService(); EmitStringArg(stmt.FileName);
        _il.Emit(OpCodes.Callvirt, FileDeleteMethod);
    }

    private void EmitLockStatement(LockStmt stmt)
    {
        EmitInt64Argument(stmt.FileNumber);
        _il.Emit(OpCodes.Ldc_I8, 0L);
        _il.Emit(OpCodes.Ldc_I8, 1L);
        _il.Emit(OpCodes.Call, FileLockMethod);
    }

    private void EmitUnlockStatement(UnlockStmt stmt)
    {
        EmitInt64Argument(stmt.FileNumber);
        _il.Emit(OpCodes.Ldc_I8, 0L);
        _il.Emit(OpCodes.Ldc_I8, 1L);
        _il.Emit(OpCodes.Call, FileUnlockMethod);
    }

    

    private void EmitTcpConnectStatement(TcpConnectStmt stmt)
    {
        EmitLoadNetworkService();
        EmitStringArg(stmt.Host); EmitInt32Arg(stmt.Port);
        _il.Emit(OpCodes.Callvirt, TcpConnectMethod);
        if (stmt.Handle is VariableTarget vt) _il.Emit(OpCodes.Stloc, GetOrDeclareLocal(vt.SymbolId));
        else _il.Emit(OpCodes.Pop);
    }

    // ── MESSAGE (namespace MESSAGE: messaging generico) ──
    private void EmitMessageConnectStatement(MessageConnectStmt stmt)
    {
        EmitStringArg(stmt.Uri);
        _il.Emit(OpCodes.Call, IlRuntimeMessageConnectMethod);
        if (stmt.Handle is VariableTarget vt) _il.Emit(OpCodes.Stloc, GetOrDeclareLocal(vt.SymbolId));
        else _il.Emit(OpCodes.Pop);
    }
    private void EmitMessageSubscribeStatement(MessageSubscribeStmt stmt)
    {
        EmitInt64Argument(stmt.Target);
        if (stmt.Topic != null) EmitStringArg(stmt.Topic); else _il.Emit(OpCodes.Ldstr, "");
        if (stmt.Queue != null) EmitStringArg(stmt.Queue); else _il.Emit(OpCodes.Ldstr, "");
        if (stmt.Alias != null) EmitStringArg(stmt.Alias); else _il.Emit(OpCodes.Ldstr, "");
        _il.Emit(OpCodes.Call, IlRuntimeMessageSubscribeMethod);
        _il.Emit(OpCodes.Pop);
    }
    private void EmitMessageUnsubscribeStatement(MessageUnsubscribeStmt stmt)
    {
        if (stmt.Sub != null) EmitStringArg(stmt.Sub); else _il.Emit(OpCodes.Ldstr, "");
        _il.Emit(OpCodes.Call, IlRuntimeMessageUnsubscribeMethod);
    }
    private void EmitMessagePublishStatement(MessagePublishStmt stmt)
    {
        EmitInt64Argument(stmt.Target);
        if (stmt.Topic != null) EmitStringArg(stmt.Topic); else _il.Emit(OpCodes.Ldstr, "");
        EmitStringArg(stmt.Data);
        _il.Emit(OpCodes.Call, IlRuntimeMessagePublishMethod);
    }
    private void EmitMessagePingStatement(MessagePingStmt stmt)
    {
        if (stmt.Target != null) EmitInt64Argument(stmt.Target); else _il.Emit(OpCodes.Ldc_I8, 0L);
        _il.Emit(OpCodes.Call, IlRuntimeMessagePingMethod);
    }
    private void EmitMessageCloseStatement(MessageCloseStmt stmt)
    {
        if (stmt.Target != null) EmitInt64Argument(stmt.Target); else _il.Emit(OpCodes.Ldc_I8, 0L);
        _il.Emit(OpCodes.Call, IlRuntimeMessageCloseMethod);
    }
    private void EmitOnMessageStatement(OnMessageStmt stmt)
    {
        if (stmt.Sub != null) EmitStringArg(stmt.Sub); else _il.Emit(OpCodes.Ldstr, "");
        int sym = -1;
        switch (stmt.Action)
        {
            case MessageCallAction mc: sym = ResolveMessageHandlerSymbolId(mc.Handler); break;
            case MessageGosubAction mg: sym = ResolveWindowGosubSymbolId(new GosubAction(mg.TargetLabel)); break;
        }
        _il.Emit(OpCodes.Ldc_I4, sym);
        _il.Emit(OpCodes.Call, IlRuntimeMessageHandlerBySymbolMethod);
    }

    // ── SIGNAL (namespace SIGNAL: segnali con filtro strutturato) ──
    private void EmitSignalSubscribeStatement(SignalSubscribeStmt stmt)
    {
        // Il runtime riceve (id, types, asset, market, symbols, timeframe).
        EmitStringArg(stmt.Id);
        if (stmt.Types != null) EmitStringArg(stmt.Types); else _il.Emit(OpCodes.Ldstr, "");
        if (stmt.Asset != null) EmitStringArg(stmt.Asset); else _il.Emit(OpCodes.Ldstr, "");
        if (stmt.Market != null) EmitStringArg(stmt.Market); else _il.Emit(OpCodes.Ldstr, "");
        if (stmt.Symbols != null) EmitStringArg(stmt.Symbols); else _il.Emit(OpCodes.Ldstr, "");
        if (stmt.Timeframe != null) EmitStringArg(stmt.Timeframe); else _il.Emit(OpCodes.Ldstr, "");
        _il.Emit(OpCodes.Call, IlRuntimeSignalSubscribeMethod);
    }
    private void EmitSignalEmitStatement(SignalEmitStmt stmt)
    {
        EmitStringArg(stmt.Type);
        EmitStringArg(stmt.Symbol);
        EmitDoubleArgument(stmt.Value);
        if (stmt.Asset != null) EmitStringArg(stmt.Asset); else _il.Emit(OpCodes.Ldstr, "");
        if (stmt.Market != null) EmitStringArg(stmt.Market); else _il.Emit(OpCodes.Ldstr, "");
        if (stmt.Timeframe != null) EmitStringArg(stmt.Timeframe); else _il.Emit(OpCodes.Ldstr, "");
        _il.Emit(OpCodes.Call, IlRuntimeSignalEmitMethod);
        _il.Emit(OpCodes.Pop);   // conteggio degli handler: SIGNAL EMIT non è una funzione
    }
    private void EmitSignalUnsubscribeStatement(SignalUnsubscribeStmt stmt)
    {
        if (stmt.Id != null) EmitStringArg(stmt.Id); else _il.Emit(OpCodes.Ldstr, "");
        _il.Emit(OpCodes.Call, IlRuntimeSignalUnsubscribeMethod);
    }
    private void EmitSignalPingStatement(SignalPingStmt stmt) => _il.Emit(OpCodes.Call, IlRuntimeSignalPingMethod);
    private void EmitOnSignalStatement(OnSignalStmt stmt)
    {
        if (stmt.Sub != null) EmitStringArg(stmt.Sub); else _il.Emit(OpCodes.Ldstr, "");
        if (stmt.Action is MessageStopAction)
        {
            _il.Emit(OpCodes.Call, IlRuntimeSignalUnregisterHandlerMethod);
            return;
        }
        int sym = stmt.Action is MessageCallAction mc
            ? ResolveMessageHandlerSymbolId(mc.Handler)
            : stmt.Action is MessageGosubAction mg
                ? ResolveWindowGosubSymbolId(new GosubAction(mg.TargetLabel))
                : -1;
        _il.Emit(OpCodes.Ldc_I4, sym);
        _il.Emit(OpCodes.Call, IlRuntimeSignalHandlerBySymbolMethod);
    }

    private void EmitTcpSendStatement(TcpSendStmt stmt)
    {
        EmitLoadNetworkService();
        EmitInt64Argument(stmt.ClientHandle);
        EmitExpression(stmt.Data);
        EmitConvertToString(InferType(stmt.Data));
        _il.Emit(OpCodes.Call, ValueToBytesMethod);
        _il.Emit(OpCodes.Callvirt, TcpSendMethod);
    }

    private void EmitTcpCloseStatement(TcpCloseStmt stmt)
    {
        EmitLoadNetworkService();
        EmitInt64Argument(stmt.Handle);
        _il.Emit(OpCodes.Callvirt, TcpCloseMethod);
    }

    private void EmitUdpSendStatement(UdpSendStmt stmt)
    {
        EmitLoadNetworkService();
        EmitInt64Argument(stmt.Handle);
        EmitStringArg(stmt.Host); EmitInt32Arg(stmt.Port);
        EmitExpression(stmt.Data);
        EmitConvertToString(InferType(stmt.Data));
        _il.Emit(OpCodes.Call, ValueToBytesMethod);
        _il.Emit(OpCodes.Callvirt, UdpSendMethod);
    }

    private void EmitUdpCloseStatement(UdpCloseStmt stmt)
    {
        EmitLoadNetworkService();
        EmitInt64Argument(stmt.Handle);
        _il.Emit(OpCodes.Callvirt, UdpCloseMethod);
    }

    private void EmitUdpOpenStatement(UdpOpenStmt stmt)
    {
        EmitLoadNetworkService();
        if (stmt.Port != null) EmitInt32Arg(stmt.Port); else _il.Emit(OpCodes.Ldc_I4_0);
        _il.Emit(OpCodes.Callvirt, UdpOpenMethod);
        // Target può essere GLOBALE (DIM a livello top): EmitAssignTargetFromTop gestisce
        // StoreGlobal — prima un semplice Stloc scriveva il local, mai letto (handle nullo).
        if (stmt.Handle is VariableTarget vt) EmitAssignTargetFromTop(vt);
        else _il.Emit(OpCodes.Pop);
    }

    private void EmitUdpRecvStatement(UdpRecvStmt stmt)
    {
        // Riceve il datagramma e lo impacchetta (UdpReceiveUnpack): object[] {data, ip, port}.
        EmitInt64Argument(stmt.Handle);
        _il.Emit(OpCodes.Call, IlRuntimeUdpReceiveUnpackMethod);
        var arrLocal = _il.DeclareLocal(typeof(object[]));
        _il.Emit(OpCodes.Stloc, arrLocal);
        EmitUdpRecvStoreElement(stmt.Buffer, arrLocal, 0);      // data (string)
        EmitUdpRecvStoreElement(stmt.RemoteIp, arrLocal, 1);    // remote ip (string)
        EmitUdpRecvStoreElement(stmt.RemotePort, arrLocal, 2);  // remote port (double)
    }

    /// <summary>Assegna un elemento dell'object[] (risultato UdpReceiveUnpack) al target,
    /// gestendo globale (StoreGlobal) vs locale e il cast CLR (string / valore numerico).</summary>
    private void EmitUdpRecvStoreElement(Target target, LocalBuilder arrLocal, int index)
    {
        _il.Emit(OpCodes.Ldloc, arrLocal);
        _il.Emit(OpCodes.Ldc_I4, index);
        _il.Emit(OpCodes.Ldelem_Ref);
        if (target is not VariableTarget vt) { _il.Emit(OpCodes.Pop); return; }
        if (IsGlobalSymbol(vt.SymbolId))
        {
            var tmp = _il.DeclareLocal(typeof(object));
            _il.Emit(OpCodes.Stloc, tmp);
            _il.Emit(OpCodes.Ldc_I4, vt.SymbolId);
            _il.Emit(OpCodes.Ldloc, tmp);
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.StoreGlobal), typeof(int), typeof(object)));
            return;
        }
        var local = GetOrDeclareLocal(vt.SymbolId);
        if (local.LocalType == typeof(string)) _il.Emit(OpCodes.Castclass, typeof(string));
        else if (local.LocalType.IsValueType) _il.Emit(OpCodes.Unbox_Any, local.LocalType);
        _il.Emit(OpCodes.Stloc, local);
    }

    private void EmitTcpOpenStatement(TcpOpenStmt stmt)
    {
        EmitLoadNetworkService();
        _il.Emit(OpCodes.Ldstr, "0.0.0.0"); EmitInt32Arg(stmt.Port);
        _il.Emit(OpCodes.Callvirt, TcpOpenMethod);
        if (stmt.Handle is VariableTarget vt) _il.Emit(OpCodes.Stloc, GetOrDeclareLocal(vt.SymbolId));
        else _il.Emit(OpCodes.Pop);
    }

    private void EmitTcpAcceptStatement(TcpAcceptStmt stmt)
    {
        EmitLoadNetworkService();
        EmitInt64Argument(stmt.ServerHandle);
        _il.Emit(OpCodes.Callvirt, TcpAcceptMethod);
        if (stmt.ClientHandle is VariableTarget vt) _il.Emit(OpCodes.Stloc, GetOrDeclareLocal(vt.SymbolId));
        else _il.Emit(OpCodes.Pop);
    }

    private void EmitTcpRecvStatement(TcpRecvStmt stmt)
    {
        // byte[]? in un temporale, poi DUE target con i convertitori condivisi col park v2
        // (EmitTcpRecvPark): prima l'emissione faceva solo Stloc su Buffer (local, mai
        // StoreGlobal) e non assegnava MAI BytesReceived → DO...LOOP WHILE bytesReceived>0
        // usciva alla prima iterazione con risposta vuota (NetSslConnect su IL flat).
        EmitLoadNetworkService();
        EmitInt64Argument(stmt.ClientHandle);
        _il.Emit(OpCodes.Ldc_I4, 4096);
        _il.Emit(OpCodes.Callvirt, TcpReceiveMethod);   // byte[]? (null = canale chiuso)
        var data = _il.DeclareLocal(typeof(byte[]));
        _il.Emit(OpCodes.Stloc, data);
        if (stmt.Buffer is VariableTarget bt)
        {
            _il.Emit(OpCodes.Ldloc, data);
            _il.Emit(OpCodes.Call, IlRuntimeNetworkDataToStringMethod);
            EmitAssignTargetFromTop(bt);
        }
        if (stmt.BytesReceived is VariableTarget nrt)
        {
            _il.Emit(OpCodes.Ldloc, data);
            _il.Emit(OpCodes.Box, typeof(byte[]));
            _il.Emit(OpCodes.Call, IlRuntimeBytesLengthMethod);   // double
            EmitAssignTargetFromTop(nrt);
        }
    }

    private void EmitSslConnectStatement(SslConnectStmt stmt)
    {
        // Guard: mai bloccare un worker GPM senza park (lancia esplicito).
        EmitThrowIfWorker("SSL CONNECT");
        EmitLoadNetworkService();
        EmitStringArg(stmt.Host);
        if (stmt.Port != null) EmitInt32Arg(stmt.Port); else _il.Emit(OpCodes.Ldc_I4, 443);
        _il.Emit(OpCodes.Ldnull);
        _il.Emit(OpCodes.Callvirt, SslConnectMethod);
        // Target GLOBALE (GLOBAL clientHandle a livello top, NetSslConnect): StoreGlobal, non
        // Stloc su un local mai letto — stesso fix già applicato a UDP OPEN (:428).
        if (stmt.Handle is VariableTarget vt) EmitAssignTargetFromTop(vt);
        else _il.Emit(OpCodes.Pop);
    }

    private void EmitThrowIfWorker(string op)
    {
        _il.Emit(OpCodes.Ldstr, op);
        _il.Emit(OpCodes.Call, IlGpmRuntimeThrowIfWorkerMethod);
    }

    private void EmitSslOpenStatement(SslOpenStmt stmt)
    {
        // Guard: mai bloccare un worker GPM senza park.
        EmitThrowIfWorker("SSL OPEN");
        EmitLoadNetworkService();
        EmitInt32Arg(stmt.Port);
        EmitStringArg(stmt.CertificatePath);
        if (stmt.CertificatePassword != null) EmitStringArg(stmt.CertificatePassword); else _il.Emit(OpCodes.Ldnull);
        _il.Emit(OpCodes.Callvirt, SslListenMethod);
        if (stmt.Handle is VariableTarget vt) _il.Emit(OpCodes.Stloc, GetOrDeclareLocal(vt.SymbolId));
        else _il.Emit(OpCodes.Pop);
    }

    private void EmitWebSocketStatement(WebSocketConnectStmt stmt)
    {
        // Guard: mai bloccare un worker GPM senza park.
        EmitThrowIfWorker("WEB SOCKET CONNECT");
        EmitStringArg(stmt.Host); EmitInt32Arg(stmt.Port);
        if (stmt.Path is not null) EmitStringArg(stmt.Path); else _il.Emit(OpCodes.Ldnull);
        // Il timeout è `object?` nel runtime (IlRuntime.WsConnect): un valore numerico deve
        // essere boxato, altrimenti il metodo riceve un int32 dove attende object → IL invalido
        // (InvalidProgramException — colpiva WEBSOCKET CONNECT host, port, ws, path, timeout).
        if (stmt.Timeout is not null)
        {
            EmitExpression(stmt.Timeout);
            var tClr = DataTypeMapper.Map(NormalizeInferredType(InferType(stmt.Timeout)), _types, _classTypes);
            if (tClr.IsValueType) _il.Emit(OpCodes.Box, tClr);
        }
        else _il.Emit(OpCodes.Ldnull);
        _il.Emit(OpCodes.Call, IlRuntimeWsConnectMethod);
        // Il target può essere una GLOBALE (DIM a livello top) o una locale (DIM in una SUB):
        // EmitAssignTargetFromTop gestisce il caso globale (StoreGlobal) — prima un semplice
        // Stloc sul local scriveva solo il local, e la lettura della globale restituiva null
        // (handle mai salvato → ws.Send/OnMessage no-op silenziosi).
        if (stmt.Handle is VariableTarget vt) EmitAssignTargetFromTop(vt);
        else _il.Emit(OpCodes.Pop);
    }

    // ── HTTP Server ───────────────────────────────────────────────────
    private void EmitHttpServerStatement(HttpServerStmt stmt)
    {
        if (stmt.IsStopCommand)
        {
            _il.Emit(OpCodes.Call, IlRuntimeHttpServerStopMethod);
        }
        else
        {
            if (stmt.Port is not null) EmitStringArg(stmt.Port);
            else _il.Emit(OpCodes.Ldstr, "8080");
            _il.Emit(OpCodes.Ldstr, stmt.Callback?.Lexeme ?? string.Empty);
            _il.Emit(OpCodes.Call, IlRuntimeHttpServerStartMethod);
            _il.Emit(OpCodes.Pop);
        }
    }

    private void EmitHttpConfigStatement(HttpConfigStmt stmt)
    {
        // Passa l'OGGETTO config (dict di NEW HTTPCONFIG) al runtime — prima lo si convertiva a
        // stringa (ToString di un Dictionary → nome tipo, inutile), rendendo HTTP CONFIG un no-op.
        EmitExpression(stmt.ConfigObject);
        var clr = DataTypeMapper.Map(NormalizeInferredType(InferType(stmt.ConfigObject)), _types, _classTypes);
        if (clr.IsValueType) _il.Emit(OpCodes.Box, clr);
        _il.Emit(OpCodes.Call, IlRuntimeHttpConfigMethod);
    }

    /// <summary>
    /// Prima passava il NOME grezzo del callback (`stmt.Callback.Lexeme`) a runtime
    /// (`IlRuntime.HttpAddRoute(string,string,string)`), che però non lo risolveva mai a un
    /// metodo reale — nessun chiamante avrebbe potuto farlo senza accesso a
    /// <see cref="_procedureDefinitions"/> (solo compile-time). Risolve qui il nome a un
    /// SymbolId, stesso pattern già usato per `AI AGENT ASK ... INTO callback` (vedi
    /// <see cref="EmitAiAgentAskStream"/>): il SymbolId (non il nome) attraversa il confine
    /// compile/runtime, e <c>IlRuntime.HttpAddRoute</c> lo usa per trovare via reflection il
    /// metodo statico <c>Proc_&lt;symbolId&gt;</c> generato per la SUB.
    /// </summary>
    private void EmitHttpRouteStatement(HttpRouteStmt stmt)
    {
        var normalized = stmt.Callback.Lexeme.Replace("_", "");
        var definition = _procedureDefinitions.Values
            .FirstOrDefault(d => d.OriginalName.Lexeme.Replace("_", "").Equals(normalized, StringComparison.OrdinalIgnoreCase));
        if (definition is null)
            throw new NotSupportedException($"Backend IL: procedura '{stmt.Callback.Lexeme}' non trovata per HTTP ROUTE.");
        // Handler a 3 parametri (req AS HTTPREQUEST, res AS HTTPRESPONSE, ws AS WEBSOCKET):
        // route WebSocket (upgrade HTTP) — il path si registra senza metodo (le WS usano GET).
        if (definition.Parameters.Length == 3)
        {
            EmitStringArg(stmt.Path);
            _il.Emit(OpCodes.Ldc_I4, definition.SymbolId);
            _il.Emit(OpCodes.Call, IlRuntimeHttpAddWebSocketRouteMethod);
            return;
        }
        if (definition.Parameters.Length != 2)
            throw new NotSupportedException(
                $"Backend IL: la callback '{stmt.Callback.Lexeme}' di HTTP ROUTE deve avere 2 parametri (req AS HTTPREQUEST, res AS HTTPRESPONSE) per una route HTTP, o 3 (req, res, ws AS WEBSOCKET) per una route WebSocket.");
        EmitStringArg(stmt.Method); EmitStringArg(stmt.Path);
        _il.Emit(OpCodes.Ldc_I4, definition.SymbolId);
        _il.Emit(OpCodes.Call, IlRuntimeHttpAddRouteMethod);
    }

    private void EmitHttpStaticStatement(HttpStaticStmt stmt)
    {
        EmitStringArg(stmt.UrlPath); EmitStringArg(stmt.PhysicalPath);
        _il.Emit(OpCodes.Call, IlRuntimeHttpAddStaticMethod);
    }

    /// <summary>
    /// Prima passava il nome grezzo del callback a runtime (mai risolvibile a un metodo reale,
    /// stesso identico gap già risolto per HTTP ROUTE — vedi EmitHttpRouteStatement). Risolve qui
    /// il nome a un SymbolId, stesso pattern; il middleware può avere 2 parametri (req, res) o 3
    /// (req, res, next — se il corpo usa `HTTP USE NEXT` senza argomento esplicito, che assume
    /// un parametro chiamato "next", vedi Parser.Stmt.Network.cs ParseHttpUseNextStatement).
    /// </summary>
    private void EmitHttpUseStatement(HttpUseStmt stmt)
    {
        if (stmt.Path is not null) EmitStringArg(stmt.Path);
        else _il.Emit(OpCodes.Ldnull);
        var normalized = stmt.Callback.Lexeme.Replace("_", "");
        var definition = _procedureDefinitions.Values
            .FirstOrDefault(d => d.OriginalName.Lexeme.Replace("_", "").Equals(normalized, StringComparison.OrdinalIgnoreCase));
        if (definition is null)
            throw new NotSupportedException($"Backend IL: procedura '{stmt.Callback.Lexeme}' non trovata per HTTP USE.");
        if (definition.Parameters.Length is not (2 or 3))
            throw new NotSupportedException(
                $"Backend IL: la callback '{stmt.Callback.Lexeme}' di HTTP USE deve avere 2 parametri (req, res) o 3 (req, res, next).");
        _il.Emit(OpCodes.Ldc_I4, definition.SymbolId);
        _il.Emit(OpCodes.Call, IlRuntimeHttpAddMiddlewareMethod);
    }

    /// <summary>
    /// Prima non valutava affatto `stmt.NextHandle` — chiamava `IlRuntime.HttpUseNext()` senza
    /// argomenti, che era comunque un no-op incondizionato lato runtime (vedi IlRuntime.cs). Il
    /// valore dell'espressione resta non necessario nel modello IL reale (un flag statico, non un
    /// vero oggetto "next" continuabile — vedi commento su IlRuntime.HttpUseNext), ma la si valuta
    /// comunque (Pop) per coerenza/effetti collaterali, invece di ignorarla silenziosamente.
    /// </summary>
    private void EmitHttpUseNextStatement(HttpUseNextStmt stmt)
    {
        EmitExpression(stmt.NextHandle);
        _il.Emit(OpCodes.Pop);
        _il.Emit(OpCodes.Call, IlRuntimeHttpUseNextMethod);
    }

    // ── Events ────────────────────────────────────────────────────────
    private void EmitEventStatement(Stmt stmt)
    {
        var evStmt = (EventHandlerStmt)stmt;
        EmitEventSource(evStmt.EventAccess);
        _il.Emit(OpCodes.Ldstr, ExtractEventName(evStmt.EventAccess));
        // Handler per symbolId → il delegate si crea a RUNTIME (ldftn su MethodBuilder non
        // ancora CreateType → InvalidProgramException; stesso schema di ON KEY ... GOSUB).
        _il.Emit(OpCodes.Ldc_I4, ResolveEventHandlerSymbolId(evStmt.HandlerExpr));
        _il.Emit(OpCodes.Call, stmt is AddHandlerStmt
            ? RuntimeMethod(nameof(IlRuntime.EventSubscribeBySymbol), typeof(object), typeof(string), typeof(int))
            : RuntimeMethod(nameof(IlRuntime.EventUnsubscribeBySymbol), typeof(object), typeof(string), typeof(int)));
    }

    /// <summary>SymbolId della procedura handler di un evento (nome), o -1 se non è una
    /// procedura (label/evento non risolvibile).</summary>
    private int ResolveEventHandlerSymbolId(Expr handlerExpr)
    {
        var name = handlerExpr switch
        {
            FunctionExpr fe => fe.OriginalToken.Lexeme,
            FunctionReferenceExpr fre => fre.OriginalToken.Lexeme,
            VariableExpr ve => ve.NameCleaned.Lexeme,
            _ => null
        };
        if (name is null) return -1;
        foreach (var (sid, def) in _procedureDefinitions)
            if (def.OriginalName.Lexeme.Equals(name, StringComparison.OrdinalIgnoreCase)
                || def.OriginalName.Lexeme.Equals(name + "$", StringComparison.OrdinalIgnoreCase))
                return sid;
        return -1;
    }

    /// <summary>Spinge sullo stack l'oggetto SOURCE dell'evento (`c.Incrementato` → `c`;
    /// un evento nudo → null, dispatch globale per nome).</summary>
    private void EmitEventSource(Expr eventAccess)
    {
        switch (eventAccess)
        {
            case FieldAccessExpr fa:
                EmitExpression(fa.Structure);
                return;
            case PropertyAccessExpr pa:
                EmitExpression(pa.Instance);
                return;
            default:
                _il.Emit(OpCodes.Ldnull);
                return;
        }
    }

    private string ExtractEventName(Expr eventAccess)
    {
        switch (eventAccess)
        {
            case FieldAccessExpr fa: return fa.Field.Lexeme;
            case PropertyAccessExpr pa: return pa.PropertyName.Lexeme;
            case VariableExpr ve: return ve.NameCleaned.Lexeme;
            default: return eventAccess.ToString();
        }
    }

    private void EmitRaiseEventStatement(RaiseEventStmt stmt)
    {
        // Source = SELF/ME se dentro un metodo/costruttore di classe (il campo arg 0 è
        // `this`), altrimenti null (evento a livello modulo, dispatch globale per nome).
        if (_isInstanceMember)
            _il.Emit(OpCodes.Ldarg_0);
        else
            _il.Emit(OpCodes.Ldnull);
        _il.Emit(OpCodes.Ldstr, stmt.EventName.Lexeme);
        _il.Emit(OpCodes.Ldc_I4, stmt.Arguments.Length);
        _il.Emit(OpCodes.Newarr, typeof(object));
        for (int i = 0; i < stmt.Arguments.Length; i++)
        {
            _il.Emit(OpCodes.Dup); _il.Emit(OpCodes.Ldc_I4, i);
            EmitEventArgumentBoxed(stmt.Arguments[i]);
            _il.Emit(OpCodes.Stelem_Ref);
        }
        _il.Emit(OpCodes.Call, IlRuntimeRaiseEventMethod);
    }

    // ── File I/O Advanced ─────────────────────────────────────────────
    private void EmitGetFileStatement(GetFileStmt stmt)
    {
        // IlRuntime.GetFile è STATIC: NON caricare il FileService (stack residuo).
        var targetIsString = stmt.Destination is VariableTarget svt
            && NormalizeInferredType(_program.Symbols.GetSymbol(svt.SymbolId).DataType) is StringType;
        if (targetIsString)
        {
            EmitInt64Argument(stmt.FileNumber);
            if (stmt.RecordNumber is not null) EmitInt64Argument(stmt.RecordNumber);
            else { _il.Emit(OpCodes.Ldc_I8, -1L); }
            _il.Emit(OpCodes.Call, IlRuntimeGetFileMethod);
            if (stmt.Destination is VariableTarget svt2) _il.Emit(OpCodes.Stloc, GetOrDeclareLocal(svt2.SymbolId));
            else _il.Emit(OpCodes.Pop);
            return;
        }
        // GET #n, pos, <array di BYTE>: legge dest.Length byte grezzi nell'array (in place).
        if (!stmt.IsIsam && stmt.Destination is VariableTarget bvt
            && NormalizeInferredType(_program.Symbols.GetSymbol(bvt.SymbolId).DataType) is ArrayType { ElementType: ByteType })
        {
            EmitInt64Argument(stmt.FileNumber);
            if (stmt.RecordNumber is not null) EmitInt64Argument(stmt.RecordNumber);
            else _il.Emit(OpCodes.Ldc_I8, 0L);
            EmitExpression(new VariableExpr(bvt.SymbolId, bvt.NameSuffixed, bvt.NameCleaned));
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.FileGetBytes), typeof(long), typeof(long), typeof(byte[])));
            return;
        }
        // GET #n, rec, <UDT> da un file RANDOM: decodifica il record (FileGetRecord) e popola i campi
        // della struct; ISAM GET (stmt.IsIsam) e file non RANDOM restano sul percorso sotto.
        if (!stmt.IsIsam && stmt.Destination is VariableTarget rvt
            && TryRandomRecordFields(_program.Symbols.GetSymbol(rvt.SymbolId).DataType, out var rFields, out var rTokens, out var rStruct)
            && rStruct is not null)
        {
            var rLocal = GetOrDeclareLocal(rvt.SymbolId);
            var valsLocal = _il.DeclareLocal(typeof(object[]));
            EmitInt64Argument(stmt.FileNumber);
            if (stmt.RecordNumber is not null) EmitInt64Argument(stmt.RecordNumber);
            else _il.Emit(OpCodes.Ldc_I8, 0L);
            _il.Emit(OpCodes.Ldstr, rTokens);
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.FileGetRecord), typeof(long), typeof(long), typeof(string)));
            _il.Emit(OpCodes.Stloc, valsLocal);
            for (var i = 0; i < rFields.Count; i++)
            {
                _il.Emit(OpCodes.Ldloca, rLocal);
                _il.Emit(OpCodes.Ldloc, valsLocal);
                _il.Emit(OpCodes.Ldc_I4, i);
                _il.Emit(OpCodes.Ldelem_Ref);
                var fType = NormalizeInferredType(_program.Symbols.GetSymbol(rFields[i].Field.SymbolId).DataType);
                if (IlConversion.IsNumeric(fType))
                {
                    _il.Emit(OpCodes.Call, _types.IlRuntimeToDoubleMethod);
                    var toClr = DataTypeMapper.Map(fType, _types, _classTypes);
                    IlConversion.EmitConvert(_il, _types, new DoubleType(), _types.Double, toClr);
                }
                else _il.Emit(OpCodes.Castclass, _types.String);   // STRING * N (già riempita a N dal codec)
                _il.Emit(OpCodes.Stfld, rFields[i].Info);
            }
            if (IsGlobalSymbol(rvt.SymbolId))
            {
                _il.Emit(OpCodes.Ldc_I4, rvt.SymbolId);
                _il.Emit(OpCodes.Ldloc, rLocal);
                _il.Emit(OpCodes.Box, rStruct);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.StoreGlobal), typeof(int), typeof(object)));
            }
            return;
        }
        // GET su target UDT (record ISAM): pattern PCodeVM — il runtime produce il dict
        // {nomeCampo: valore} dal record corrente; l'emitter popola i campi della struct.
        if (stmt.Destination is VariableTarget vtUdt)
        {
            var udtType = _program.Symbols.GetSymbol(vtUdt.SymbolId).DataType is UserDefinedType uUdt
                ? uUdt : null;
            if (udtType is null)
                throw new NotSupportedException("Backend IL: GET ISAM su target non-UDT.");
            var dictLocal = _il.DeclareLocal(typeof(System.Collections.IDictionary));
            EmitIsamAliasArg(stmt.FileNumber);
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.IsamReadCurrentRecord), typeof(string)));
            _il.Emit(OpCodes.Stloc, dictLocal);
            var local = GetOrDeclareLocal(vtUdt.SymbolId);
            Type? structClr = null;
            foreach (var field in udtType.Fields)
            {
                if (field.SymbolId < 0) continue;
                if (!_classFields.TryGetValue(field.SymbolId, out var fieldBuilder)) continue;
                if (fieldBuilder.IsStatic || !fieldBuilder.DeclaringType!.IsValueType) continue;
                structClr ??= fieldBuilder.DeclaringType;
                _il.Emit(OpCodes.Ldloca, local);
                _il.Emit(OpCodes.Ldloc, dictLocal);
                _il.Emit(OpCodes.Ldstr, field.Name.Lexeme);
                _il.Emit(OpCodes.Call, IlJsonFieldGetMethod);
                var fieldDataType2 = NormalizeInferredType(_program.Symbols.GetSymbol(field.SymbolId).DataType);
                if (IlConversion.IsNumeric(fieldDataType2))
                {
                    // Il dict contiene object (boxed double/int/string): ToDouble dispatch sul
                    // boxed reale, poi conv al tipo del campo. EmitCoerceTypes(AnyType, ...)
                    // usciva subito (AnyType non numerico) lasciando l'object → Stfld in un
                    // campo double/int = corruzione (Price denormal 2.4E-314).
                    _il.Emit(OpCodes.Call, _types.IlRuntimeToDoubleMethod);
                    var toClr = DataTypeMapper.Map(fieldDataType2, _types, _classTypes);
                    IlConversion.EmitConvert(_il, _types, new DoubleType(), _types.Double, toClr);
                }
                else if (fieldDataType2 is DateType or DateTimeType or TimeType or TimeSpanType)
                {
                    // Campo DATE/TIME/DATETIME (CLR DateTime): il dict porta un valore boxed
                    // (DateTime/string/OA double) → converto via ToDateTime(object). Prima
                    // EmitConvertToString produceva una stringa → Stfld string in campo
                    // DateTime = IL invalido (StackUnexpected, trovato da ILVerify).
                    _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.ToDateTime), typeof(object)));
                }
                else if (fieldDataType2 is not StringType)
                {
                    EmitConvertToString(fieldDataType2);
                }
                else
                {
                    // Campo STRING: JsonFieldGet ritorna object (string boxed) — senza
                    // castclass lo Stfld object→string è IL invalido (InvalidProgram).
                    _il.Emit(OpCodes.Castclass, _types.String);
                }
                _il.Emit(OpCodes.Stfld, fieldBuilder);
            }
            // Il target può essere una variabile GLOBALE (DIM a livello modulo): il
            // popolamento è avvenuto sul local temp, va ricopiato nel bag globale
            // (boxing) così le letture successive (LoadGlobal) trovano il record.
            if (structClr is not null && IsGlobalSymbol(vtUdt.SymbolId))
            {
                _il.Emit(OpCodes.Ldc_I4, vtUdt.SymbolId);
                _il.Emit(OpCodes.Ldloc, local);
                _il.Emit(OpCodes.Box, structClr);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.StoreGlobal), typeof(int), typeof(object)));
            }
        }
        else _il.Emit(OpCodes.Pop);
    }

    private void EmitPutFileStatement(PutFileStmt stmt)
    {
        // PUT di un array di BYTE: byte grezzi, uno per elemento (pos > 0 = offset di byte pos-1).
        if (NormalizeInferredType(InferType(stmt.Source)) is ArrayType { ElementType: ByteType })
        {
            EmitInt64Argument(stmt.FileNumber);
            if (stmt.RecordNumber is not null) EmitInt64Argument(stmt.RecordNumber);
            else _il.Emit(OpCodes.Ldc_I8, 0L);
            EmitExpression(stmt.Source);
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.FilePutBytes), typeof(long), typeof(long), typeof(byte[])));
            return;
        }
        // PUT di un UDT: se il file è RANDOM → record a layout canonico (campi dell'UDT); altrimenti
        // (BINARY, …) il percorso invariato sotto.
        if (TryRandomRecordFields(InferType(stmt.Source), out var recFields, out var recTokens, out var recStruct)
            && recStruct is not null)
        {
            var tmp = _il.DeclareLocal(recStruct);
            EmitExpression(stmt.Source);
            _il.Emit(OpCodes.Stloc, tmp);
            var legacy = _il.DefineLabel();
            var end = _il.DefineLabel();
            EmitInt64Argument(stmt.FileNumber);
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.FileIsRandom), typeof(long)));
            _il.Emit(OpCodes.Brfalse, legacy);
            EmitInt64Argument(stmt.FileNumber);
            if (stmt.RecordNumber is not null) EmitInt64Argument(stmt.RecordNumber);
            else _il.Emit(OpCodes.Ldc_I8, 0L);
            _il.Emit(OpCodes.Ldstr, recTokens);
            _il.Emit(OpCodes.Ldc_I4, recFields.Count);
            _il.Emit(OpCodes.Newarr, typeof(object));
            for (var i = 0; i < recFields.Count; i++)
            {
                _il.Emit(OpCodes.Dup);
                _il.Emit(OpCodes.Ldc_I4, i);
                _il.Emit(OpCodes.Ldloca, tmp);
                _il.Emit(OpCodes.Ldfld, recFields[i].Info);
                if (recFields[i].Info.FieldType.IsValueType) _il.Emit(OpCodes.Box, recFields[i].Info.FieldType);
                _il.Emit(OpCodes.Stelem_Ref);
            }
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.FilePutRecord), typeof(long), typeof(long), typeof(string), typeof(object[])));
            _il.Emit(OpCodes.Br, end);
            _il.MarkLabel(legacy);
            EmitInt64Argument(stmt.FileNumber);
            _il.Emit(OpCodes.Ldloc, tmp);
            _il.Emit(OpCodes.Box, recStruct);
            _il.Emit(OpCodes.Call, IlRuntimePutFileMethod);
            _il.MarkLabel(end);
            return;
        }
        // IlRuntime.PutFile è STATIC (long, object): NON caricare il FileService —
        // il service caricato restava sullo stack (stack residuo → InvalidProgram).
        EmitInt64Argument(stmt.FileNumber);
        EmitExpression(stmt.Source);
        var st = InferType(stmt.Source);
        if (st is StringType) EmitConvertToString(st);
        else
        {
            // UDT struct non boxato → InvalidProgram (PutFile vuole object).
            var clr = DataTypeMapper.Map(NormalizeInferredType(st), _types, _classTypes);
            if (clr.IsValueType) _il.Emit(OpCodes.Box, clr);
        }
        _il.Emit(OpCodes.Call, IlRuntimePutFileMethod);
    }

    // ── Memory Mapping ────────────────────────────────────────────────
    private void EmitMemoryMapStatement(MemoryMapStmt stmt)
    {
        // IlRuntime.MemoryMap è STATIC (string → long): niente service da caricare.
        EmitStringArg(stmt.FilePath);
        _il.Emit(OpCodes.Call, IlRuntimeMemoryMapMethod);
        if (stmt.Handle is VariableTarget vt) _il.Emit(OpCodes.Stloc, GetOrDeclareLocal(vt.SymbolId));
        else _il.Emit(OpCodes.Pop);
    }

    private void EmitMemoryPokeStatement(MemoryMapPokeStmt stmt)
    {
        EmitInt64Argument(stmt.Handle);
        EmitInt64Argument(stmt.Offset);
        EmitDoubleArgument(stmt.Value);
        _il.Emit(OpCodes.Call, IlRuntimeMemoryPokeMethod);
    }

    private void EmitMemoryWriteBytesStatement(MemoryMapWriteBytesStmt stmt)
    {
        EmitInt64Argument(stmt.Handle);
        EmitInt64Argument(stmt.Offset);
        EmitExpression(stmt.SourceArray);
        if (stmt.Count is not null)
            EmitInt64Argument(stmt.Count);
        else
            _il.Emit(OpCodes.Ldc_I8, -1L); // -1 = scrivi l'intero array
        _il.Emit(OpCodes.Call, IlRuntimeMemoryWriteBytesMethod);
    }

    // Funzioni MEMORYMAP.MEMORYMAP(path) / MEMORYMAP.MMAPPEEK(h, off) / MEMORYMAP.MMAPREADBYTES(h, off, count)
    private void EmitMemoryMapFunctionCall(BuiltinFunctionCallExpr expr)
    {
        // IlRuntime.MemoryMap è STATIC (string → long): niente service da caricare.
        EmitStringArg(expr.Args[0]);
        _il.Emit(OpCodes.Call, IlRuntimeMemoryMapMethod);
    }

    private void EmitMemoryMapPeekCall(BuiltinFunctionCallExpr expr)
    {
        EmitInt64Argument(expr.Args[0]);
        EmitInt64Argument(expr.Args[1]);
        _il.Emit(OpCodes.Call, IlRuntimeMemoryPeekMethod);
        // Niente Conv_I8: la call ritorna byte (già int32 sullo stack CLR),
        // coerente col ByteType inferito — Conv_I8 produceva int64 e rompeva
        // ogni espressione binaria sul risultato (InvalidProgram, mmap.bas).
    }

    private void EmitMemoryMapReadBytesCall(BuiltinFunctionCallExpr expr)
    {
        EmitInt64Argument(expr.Args[0]);
        EmitInt64Argument(expr.Args[1]);
        EmitInt64Argument(expr.Args[2]);
        _il.Emit(OpCodes.Call, IlRuntimeMemoryReadBytesMethod);
    }

    // ── Network Config ────────────────────────────────────────────────
    private void EmitSetOptionStatement(SetOptionStmt stmt)
    {
        EmitLoadNetworkService();
        EmitInt64Argument(stmt.Handle);
        EmitStringArg(stmt.Option); EmitStringArg(stmt.Value);
        _il.Emit(OpCodes.Callvirt, NetworkSetOptionMethod);
    }

    private void EmitShutdownStatement(ShutdownStmt stmt)
    {
        EmitLoadNetworkService();
        EmitInt64Argument(stmt.Handle);
        EmitStringArg(stmt.Mode);
        _il.Emit(OpCodes.Callvirt, NetworkShutdownMethod);
    }

    // ── Using Block ───────────────────────────────────────────────────
    private void EmitUsingStatement(UsingStmt stmt)
    {
        // USING F = OPEN(path, mode): la FUNZIONE OPEN apre il file e registra il
        // fileNum sintetico 0; F = 0 (il fileNum) — il PRINT #F/INPUT #F/EOF(F) del
        // corpo usano il fileNum. Il END USING chiude il file (chiusura automatica).
        var fLocal = stmt.ResourceTarget is VariableTarget vt2 ? GetOrDeclareLocal(vt2.SymbolId) : null;
        var isFileOpen = stmt.ResourceInit is ArrayAccessExpr openAa2 && openAa2.NameCleaned.Kind == TokenKind.Open;
        if (isFileOpen)
        {
            var openAa = (ArrayAccessExpr)stmt.ResourceInit!;
            if (openAa.Indices.Length != 2)
                throw new NotSupportedException("Backend IL: OPEN come funzione richiede (path, mode).");
            EmitExpression(openAa.Indices[0]);            // path
            // mode: può essere un TOKEN (`OUTPUT`/`INPUT`/`APPEND`) NON una stringa — EmitStringArg
            // lo trattava come variabile → mode vuoto → FileOpenNumber cadeva sul default (FOpen in
            // LETTURA su un file appena creato → -1 = handle invalido = "Handle file non valido"
            // al PRINT #h). Mappa il token alla modalità testuale.
            EmitOpenFunctionModeArg(openAa.Indices[1]);
            _il.Emit(OpCodes.Ldc_I8, 0L);                 // fileNum sintetico 0
            _il.Emit(OpCodes.Call, FileOpenMethod);       // → handle (scartato)
            _il.Emit(OpCodes.Pop);
            if (fLocal is not null) { _il.Emit(OpCodes.Ldc_I8, 0L); _il.Emit(OpCodes.Stloc, fLocal); }
        }
        else if (stmt.ResourceInit is not null)
        {
            // Bug scoperto insieme a BUG-8 (destructor non chiamato su USING, vedi
            // Classi-Distruttore-Ereditarieta-CrossFile-Bugs): ResourceTarget non aveva mai un
            // tipo risolto (BindUsingStatement lo registra come InferredType, mai corretto in
            // seguito) — DataTypeMapper mappa InferredType su CLR double, quindi PRIMA del fix
            // in Linker.Rewrites.cs (RewriteUsingStatement, inferenza tipo da ResourceInit) il
            // local IL era SEMPRE double, indipendentemente dal vero tipo della risorsa (`r.Nome`
            // su un'istanza di CLASS leggeva un valore spazzatura). Ora che il tipo è corretto,
            // usa EmitStoreVariable (stesso helper di una normale DIM/LET, gestisce
            // conversioni/coerce per qualunque tipo CLR) invece del vecchio Stloc/Conv_I8 ad-hoc,
            // che assumeva sempre un tipo numerico (Conv_I8 su un riferimento a oggetto produceva
            // IL non valido — InvalidProgramException — una volta che il tipo non era più double).
            if (stmt.ResourceTarget is VariableTarget vtStore)
                EmitStoreVariable(vtStore.SymbolId, stmt.ResourceInit);
            else
            {
                EmitExpression(stmt.ResourceInit);
                _il.Emit(OpCodes.Pop);
            }
        }
        foreach (var bodyStmt in stmt.Body) EmitStatement(bodyStmt);
        // Chiusura automatica (END USING): CLOSE #0, ma SOLO per il caso file (OPEN) — per una
        // risorsa non-file (es. istanza di CLASS) chiamare FileCloseNumberMethod su un
        // riferimento a oggetto sarebbe IL non valido. Nessuna chiamata deterministica al
        // DESTRUCTOR per il caso CLASS: il backend IL affida la finalizzazione delle istanze al
        // GC (Finalize, non deterministico — scelta di design 2026-07-21, vedi
        // IlProgramCompiler.Host.Compile.cs, DestructorMember) — residuo noto, USING su
        // un'istanza di classe non chiude nulla di deterministico qui, diversamente dal
        // PCodeVM (dove USING chiama il DESTRUCTOR a END USING, vedi BUG-8).
        if (fLocal is not null && isFileOpen)
        {
            _il.Emit(OpCodes.Ldloc, fLocal);
            _il.Emit(OpCodes.Call, FileCloseNumberMethod);
            _il.Emit(OpCodes.Pop);
        }
    }

    /// <summary>Modalità di `OPEN(path, MODE)` in forma FUNZIONE (USING): il secondo argomento
    /// può essere un TOKEN (`OUTPUT`/`INPUT`/`APPEND`/`BINARY`/`RANDOM`, oppure `WRITE`/`READ`)
    /// invece di una stringa. <see cref="EmitStringArg"/> lo trattava come variabile → mode vuoto
    /// → <c>FileOpenNumber</c> cadeva sul default (FOpen in LETTURA su un file appena creato →
    /// -1 = handle invalido → "Handle file non valido" al PRINT #h). Mappa il token alla modalità
    /// testuale; per una vera stringa (o un'espressione) delega a <see cref="EmitStringArg"/>.</summary>
    private void EmitOpenFunctionModeArg(Expr modeExpr)
    {
        var modeName = modeExpr switch
        {
            VariableExpr ve => ve.NameCleaned.Lexeme.ToUpperInvariant(),
            _ => null,
        };
        var modeStr = modeName switch
        {
            "OUTPUT" or "WRITE" or "W" => "w",
            "INPUT" or "READ" or "R" => "r",
            "APPEND" or "A" => "a",
            "BINARY" => "rb",
            "RANDOM" => "r+",
            _ => null,
        };
        if (modeStr is not null) { _il.Emit(OpCodes.Ldstr, modeStr); return; }
        EmitStringArg(modeExpr);
    }
}
