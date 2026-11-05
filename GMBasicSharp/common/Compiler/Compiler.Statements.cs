// FILE: Interpreter.Compiler.Compiler.Statements.cs



namespace Compiler;

/// <summary>
/// Contiene i metodi di visita per le istruzioni "core" del linguaggio:
/// flusso di controllo, gestione scope, assegnazioni, dichiarazioni.
/// </summary>
public sealed partial class Compiler
{
    /// <summary>
    /// Dispatcher principale per visitare e compilare un nodo istruzione (Stmt).
    /// Questa versione è stata aggiornata per includere tutti i tipi di statement
    /// definiti nell'AST, con placeholder per le implementazioni mancanti.
    /// </summary>
    /// <param name="stmt">Il nodo da compilare.</param>
    private void VisitStatement(Stmt stmt)
    {
        /* Intervallo di ISTRUZIONE (start,end) per RESUME NEXT: il wrapper registra il
         * bytecode emesso dal dispatcher, compresi i sotto-statement (annidati). */
        int stmtStart = _chunk.Code.Count;
        VisitStatementCore(stmt);
        if (_chunk.Code.Count > stmtStart)
            _chunk.StatementRanges.Add((stmtStart, _chunk.Code.Count));
    }

    private void VisitStatementCore(Stmt stmt)
    {
        // Le etichette sono speciali: rendono il codice successivo raggiungibile.
        // Vanno processate anche se _isReachable è false.
        if (stmt is LabelStmt labelStmt)
        {
            VisitLabelStatement(labelStmt);
            // Non c'è altro da fare per un'etichetta, quindi usciamo.
            return;
        }

        // Se il punto di compilazione corrente non è raggiungibile,
        // ignoriamo qualsiasi altra istruzione.
        if (!_isReachable)
        {
            return;
        }

        // Riga corrente = riga dello statement che stiamo compilando. SourceLineIndex è
        // l'indice 0-based della riga (`_sourceMap`), ma NON tutti i nodi lo ricevono dal
        // parser (-1): in quel caso si usa Position.Line (token, 1-based) — senza questo
        // fallback gli opcode dello statement ereditavano la riga dello statement PRECEDENTE
        // e `ERR.LineNumber` riportava una riga sbagliata (es. la DIM invece della divisione).
        if (stmt.SourceLineIndex >= 0) _currentSourceLineIndex = stmt.SourceLineIndex;
        else if (stmt.Position.Line > 0) _currentSourceLineIndex = stmt.Position.Line - 1;
        switch (stmt)
        {
            // --- Core Language Constructs ---
            case RemStmt: break; // I commenti non generano P-Code
            case DataStmt: break; // Gestito in un passaggio preliminare
            case LetStmt l: VisitLetStatement(l); break;
            case DeclarationStmt d: VisitDeclarationStatement(d); break;
            case LocalProcedureStmt lps: VisitLocalProcedureStatement(lps); break;
            //case ConstStmt c: VisitConstStatement(c); break;
            case RedimStmt r: VisitRedimStatement(r); break;
            case ReadStmt rd: VisitReadStatement(rd); break;
            case RestoreStmt rst: VisitRestoreStatement(rst); break;
            case SwapStmt sw: VisitSwapStatement(sw); break;
            case RandomizeStmt rnd: VisitRandomizeStatement(rnd); break;
            case SystemShellStmt shl: VisitSystemShellStatement(shl); break;
            //case StopStmt stop: VisitStopStatement(stop); break;
            //case RunStmt run: VisitRunStatement(run); break;
            case ExpressionStmt es: VisitExpressionStatement(es); break;
            case SemanticPatchStmt: break;   // §4.10 write-side: applicato in ApplySemanticPatches (no-op)
            case MacroStmt m: VisitMacroStatement(m); break;

            // --- Control Flow & Procedures ---
            case IfStmt i: VisitIfStatement(i); break;
            case SelectStmt s: VisitSelectStatement(s); break;
            case GotoStmt g: VisitGotoStatement(g); break;
            case GosubStmt gs: VisitGosubStatement(gs); break;
            case ReturnStmt rt: VisitReturnStatement(rt); break;
            case EndProcedureStmt ep: VisitEndProcedureStatement(ep); break;
            case EndStmt end: VisitEndStatement(end); break;
            case LabelStmt l: VisitLabelStatement(l); break;
            case OnJumpStmt oj: VisitOnJumpStatement(oj); break;

            // --- Loops ---
            case JumpStmt j: VisitJumpStatement(j); break;
            case ConditionalJumpStmt cj: VisitConditionalJumpStatement(cj); break;
            case ExitStmt e: VisitExitStatement(e); break;
            case ContinueStmt c: VisitContinueStatement(c); break;
            case ForStmt f: VisitForStatement(f); break;
            case ForCheckAndJumpStmt fc: VisitForCheckAndJumpStatement(fc); break;
            case NextStmt n: VisitNextStatement(n); break;
            case EnterLoopStmt el: VisitEnterLoopStatement(el); break;
            case ExitLoopStmt xl: VisitExitLoopStatement(xl); break;

            // --- Error Handling & Events ---
            case TryCatchStmt t: VisitTryCatchStatement(t); break;
            case OnErrorStmt oe: VisitOnErrorStatement(oe); break;
            case OnErrorResumeStmt oer: VisitOnErrorResumeStatement(oer); break;
            case ResumeStmt r: VisitResumeStatement(r); break;
            case ErrorStmt err: VisitErrorStatement(err); break;
            case OnKeyStmt ok: VisitOnKeyStatement(ok); break;

            // --- Concurrency ---
            case GoProcedureStmt go: VisitGoRoutineStatement(go); break;
            case WaitStmt w: VisitWaitStatement(w); break;
            case DoneStmt dn: VisitDoneStatement(dn); break;
            case SendStmt sn: VisitSendStatement(sn); break;
            case ReceiveStmt rcv: VisitReceiveStatement(rcv); break;
            case CloseChannelStmt cc: VisitCloseChannelStatement(cc); break;
            case SleepStmt slp: VisitSleepStatement(slp); break;
            case GoExitStmt ge: VisitGoExitStatement(ge); break;
            case GoSelectStmt gs: VisitGoSelectStatement(gs); break;
            case MutexLockStmt lk: VisitLockStatement(lk); break;
            case MutexUnlockStmt ul: VisitUnlockStatement(ul); break;
            case MutexRLockStmt rlk: VisitRLockStatement(rlk); break;
            case MutexRUnlockStmt rul: VisitRUnlockStatement(rul); break;
            case CondWaitStmt cw: VisitCondWaitStatement(cw); break;
            case CondSignalStmt csg: VisitCondSignalStatement(csg); break;
            case CondBroadcastStmt cb: VisitCondBroadcastStatement(cb); break;
            case OnceStmt os: VisitOnceStatement(os); break;
            case DeferStmt df: VisitDeferStatement(df); break;

            // --- JSON Commands ---
            case JsonWriteValueStmt jwv: VisitJsonWriteValueStatement(jwv); break;
            case JsonWriteSectionStmt jws: VisitJsonWriteSectionStatement(jws); break;

            // --- Arrays Commands ---
            case AAddStmt aadd: VisitAAddStatement(aadd); break;
            case AInsStmt ains: VisitAInsStatement(ains); break;
            case ADelStmt adel: VisitADelStatement(adel); break;
            case ASortStmt asort: VisitASortStatement(asort); break;
            case AShuffleStmt ashuffle: VisitAShuffleStatement(ashuffle); break;
            case AFillStmt afill: VisitAFillStatement(afill); break;
            case ACopyStmt acopy: VisitACopyStatement(acopy); break;
            case AEvalStmt aeval: VisitAEvalStatement(aeval); break;

            // --- Comandi con Opcode Dedicati o Gestiti da CMD ---
            case TcpOpenStmt to: VisitTcpOpenStatement(to); break;
            case TcpConnectStmt tcn: VisitTcpConnectStatement(tcn); break;
            case TcpAcceptStmt ta: VisitTcpAcceptStatement(ta); break;
            case TcpSendStmt ts: VisitTcpSendStatement(ts); break;
            case TcpRecvStmt tr: VisitTcpRecvStatement(tr); break;
            case TcpCloseStmt tcl: VisitTcpCloseStatement(tcl); break;
            case UdpOpenStmt uo: VisitUdpOpenStatement(uo); break;
            case UdpSendStmt us: VisitUdpSendStatement(us); break;
            case UdpRecvStmt ur: VisitUdpRecvStatement(ur); break;
            case UdpCloseStmt uc: VisitUdpCloseStatement(uc); break;
            case ShutdownStmt sh: VisitShutdownStatement(sh); break;
            case SetOptionStmt so: VisitSetOptionStatement(so); break;
            case SslOpenStmt slo: VisitSslOpenStatement(slo); break;
            case SslConnectStmt slc: VisitSslConnectStatement(slc); break;
            case WebSocketConnectStmt wsc: VisitWebSocketConnectStatement(wsc); break;

            // --- NETWORKING (HTTP) ---
            case HttpFetchStmt f: VisitFetchStatement(f); break;
            case HttpServerStmt hs: VisitHttpServerStatement(hs); break;
            case HttpRouteStmt hr: VisitHttpRouteStatement(hr); break;
            case HttpUseStmt hu: VisitHttpUseStatement(hu); break;
            case HttpUseNextStmt hun: VisitHttpUseNextStatement(hun); break;
            case HttpConfigStmt hc: VisitHttpConfigStatement(hc); break;
            case HttpStaticStmt hs: VisitHttpStaticStatement(hs); break;

            // --- NETWORKING (Interni VM) ---

            // --- FILES (Standard I/O & Filesystem) ---
            case OpenStmt o: VisitOpenStatement(o); break;
            case CloseStmt c: VisitCloseStatement(c); break;
            case PrintFileStmt pf: VisitPrintFileStatement(pf); break;
            case WriteFileStmt wf: VisitWriteFileStatement(wf); break;
            case InputFileStmt inf: VisitInputFileStatement(inf); break;
            case LineInputFileStmt lif: VisitLineInputFileStatement(lif); break;
            case GetFileStmt gf: VisitGetFileStatement(gf); break;
            case PutFileStmt puf: VisitPutFileStatement(puf); break;
            case SeekStmt seek: VisitSeekStatement(seek); break;
            case KillStmt k: VisitKillStatement(k); break;
            case RenameStmt rn: VisitRenameStatement(rn); break;
            case MkDirStmt mk: VisitMkDirStatement(mk); break;
            case RmDirStmt rm: VisitRmDirStatement(rm); break;
            case ChDirStmt ch: VisitChDirStatement(ch); break;
            case CopyStmt cpy: VisitCopyStatement(cpy); break;
            case ZipStmt z: VisitZipStatement(z); break;
            case UnzipStmt uz: VisitUnzipStatement(uz); break;
            case MoveStmt mv: VisitMoveStatement(mv); break;
            case XCopyStmt xc: VisitXCopyStatement(xc); break;

            // --- CONSOLE I/O & CLIPBOARD ---
            case GetKeyboardStmt gk: VisitGetKeyboardStatement(gk); break;
            case InkeyStmt ik: VisitInkeyStatement(ik); break;
            case WaitKeyStmt wk: VisitWaitKeyStatement(wk); break;
            case InputStmt inp: VisitInputStatement(inp); break;
            case ClsStmt cls: VisitClsStatement(cls); break;
            case ColorStmt col: VisitColorStatement(col); break;
            case LineInputStmt li: VisitLineInputStatement(li); break;
            case LocateStmt loc: VisitLocateStatement(loc); break;
            case PrintStmt p: VisitPrintStatement(p); break;
            // --- SQL ---
            case SqlOpenStmt sqlOpen: VisitSqlOpenStatement(sqlOpen); break;
            case SqlCloseStmt sqlClose: VisitSqlCloseStatement(sqlClose); break;
            case SqlCloseResultSetStmt sqlCrs: VisitSqlCloseResultSetStatement(sqlCrs); break;
            case SqlExecStmt sqlExec: VisitSqlExecStatement(sqlExec); break;
            case SqlQueryStmt sqlQuery: VisitSqlQueryStatement(sqlQuery); break;
            case SqlBeginTransactionStmt sqlBegin: VisitSqlBeginTransactionStatement(sqlBegin); break;
            case SqlCommitStmt sqlCommit: VisitSqlCommitStatement(sqlCommit); break;
            case SqlRollbackStmt sqlRollback: VisitSqlRollbackStatement(sqlRollback); break;
            case SqlSavepointStmt sqlSavepoint: VisitSqlSavepointStatement(sqlSavepoint); break;
            case SqlReleaseStmt sqlRelease: VisitSqlReleaseStatement(sqlRelease); break;
            case SqlAttachStmt sqlAttach: VisitSqlAttachStatement(sqlAttach); break;
            case SqlDetachStmt sqlDetach: VisitSqlDetachStatement(sqlDetach); break;
            case SqlBackupStmt sqlBackup: VisitSqlBackupStatement(sqlBackup); break;
            // --- VECTORDB (database vettoriale autonoma) ---
            case VectorDbEnsureCollectionStmt vec: VisitVectorDbEnsureCollectionStatement(vec); break;
            case VectorDbIndexTextStmt vi: VisitVectorDbIndexTextStatement(vi); break;
            case VectorDbIndexMetaStmt vm: VisitVectorDbIndexMetaStatement(vm); break;
            case VectorDbSearchStmt vs: VisitVectorDbSearchStatement(vs); break;
            case VectorDbCountStmt vc: VisitVectorDbCountStatement(vc); break;
            case VectorDbListCollectionsStmt vl: VisitVectorDbListCollectionsStatement(vl); break;
            case VectorDbRemoveSourceStmt vr: VisitVectorDbRemoveSourceStatement(vr); break;
            case VectorDbLoadDocumentsStmt vld: VisitVectorDbLoadDocumentsStatement(vld); break;
            case VectorDbPurgeDeprecatedStmt vp: VisitVectorDbPurgeDeprecatedStatement(vp); break;
            // --- TORCH (comandi legacy, nessun valore di ritorno) ---
            case TorchSetDeviceStmt td: VisitTorchSetDeviceStatement(td); break;
            case TorchLoadLibStmt tl: VisitTorchLoadLibStatement(tl); break;
            case TorchUnloadLibStmt tu: VisitTorchUnloadLibStatement(tu); break;
            case TorchSetLibPathStmt tp: VisitTorchSetLibPathStatement(tp); break;
            case TorchInfoStmt ti: VisitTorchInfoStatement(ti); break;
            // --- Clipper TUI ---
            case AtSayStmt atSay: VisitAtSayStatement(atSay); break;
            case AtGetStmt atGet: VisitAtGetStatement(atGet); break;
            case ClipperReadStmt cRead: VisitClipperReadStatement(cRead); break;
            case BoxStmt box: VisitBoxStatement(box); break;
            case ScrollStmt scroll: VisitScrollStatement(scroll); break;
            case CheckBoxStmt cbs: VisitCheckBoxStatement(cbs); break;
            case ListBoxStmt lbs: VisitListBoxStatement(lbs); break;
            case ComboBoxStmt cbs2: VisitComboBoxStatement(cbs2); break;
            case MenuStmt menu: VisitMenuStatement(menu); break;
            case StatusBarStmt sb: VisitStatusBarStatement(sb); break;
            case FileDialogStmt fd: VisitFileDialogStatement(fd); break;
            case PrinterDialogStmt pd: VisitPrinterDialogStatement(pd); break;
            case ToggleStmt tg: VisitToggleStatement(tg); break;
            case RadioGroupStmt rg: VisitRadioGroupStatement(rg); break;
            case SliderStmt sl: VisitSliderStatement(sl); break;
            case NumericUpDownStmt nu: VisitNumericUpDownStatement(nu); break;
            case StarRatingStmt sr: VisitStarRatingStatement(sr); break;
            case ScrollBarStmt sb: VisitScrollBarStatement(sb); break;
            case ScrollViewStmt sv: VisitScrollViewStatement(sv); break;
            case LabelControlStmt lb: VisitLabelStatement(lb); break;
            case DataListStmt dl: VisitDataListStatement(dl); break;
            case PropertyGridStmt pg: VisitPropertyGridStatement(pg); break;
            case LogViewStmt lv: VisitLogViewStatement(lv); break;
            case HexViewStmt hx: VisitHexViewStatement(hx); break;
            case BigTextStmt bt: VisitBigTextStatement(bt); break;
            case CodeAreaStmt ca: VisitCodeAreaStatement(ca); break;
            case ReorderListBoxStmt rl: VisitReorderListBoxStatement(rl); break;
            case AlertBoxStmt ab: VisitAlertBoxStatement(ab); break;
            case ConfirmationBoxStmt cb: VisitConfirmationBoxStatement(cb); break;
            case FileExplorerStmt fe: VisitFileExplorerStatement(fe); break;
            case DashboardStmt db: VisitDashboardStatement(db); break;
            case SplitterStmt sp: VisitSplitterStatement(sp); break;
            case ContextMenuStmt cm: VisitContextMenuStatement(cm); break;            case TabStmt tab: VisitTabStatement(tab); break;
            case GridStmt gr: VisitGridStatement(gr); break;
            case DatePickerStmt dp: VisitDatePickerStatement(dp); break;
            case ColorPickerStmt cp: VisitColorPickerStatement(cp); break;
            case TreeViewStmt tv: VisitTreeViewStatement(tv); break;
            case SpinnerStmt sp: VisitSpinnerStatement(sp); break;
            case TBrowseStmt tb: VisitTBrowseStatement(tb); break;
            case PixelControlStmt pcs: VisitPixelControlStatement(pcs); break;
            case ControlEventStmt ces: VisitControlEventStatement(ces); break;
            case FireEventStmt fes: VisitFireEventStatement(fes); break;
            case ControlMethodStmt cms: VisitControlMethodStatement(cms); break;

            case CreateAgentStmt createAgent: VisitCreateAgentStatement(createAgent); break;
            case DeleteAgentStmt deleteAgent: VisitDeleteAgentStatement(deleteAgent); break;
            case SaveAgentStmt saveAgent: VisitSaveAgentStatement(saveAgent); break;
            case LoadAgentStmt loadAgent: VisitLoadAgentStatement(loadAgent); break;

            // CRYPTO
            case KeyGenStmt kgs: VisitKeyGenStatement(kgs); break;
            case KeyCloseStmt kcs: VisitKeyCloseStatement(kcs); break;

            case LocalTypeStmt lts: EmitExprAndPop(lts); break;
            case ConstStmt c: VisitConstStatement(c); break;
            case StopStmt stop: VisitStopStatement(stop); break;
            case RunStmt run: VisitRunStatement(run); break;
            case WithStmt ws: VisitWithStatement(ws); break;
            case MatchStmt ms: VisitMatchStatement(ms); break;
            case ForEachStmt fes: VisitForEachStatement(fes); break;
            case ForEachInitStmt fei: VisitForEachInitStatement(fei); break;
            case ForEachAdvanceAndAssignStmt fea: VisitForEachAdvanceAndAssignStatement(fea); break;
            case YieldStmt ys: VisitYieldStatement(ys); break;
            case ImportStmt imp: EmitExprAndPop(imp); break;
            case RequireStmt req: EmitExprAndPop(req); break;
            case JsonSaveStmt jss: VisitJsonSaveStatement(jss); break;
            case RethrowStmt rt: VisitRethrowStatement(rt); break;
            case ThrowStmt ts: VisitExpression(ts.Value); EmitByte((byte)OpCode.OP_THROW); break;
            case ViewPrintStmt vps: EmitViewPrintStatement(vps); break;
            case TupleAssignmentStmt tas: VisitTupleAssignmentStatement(tas); break;
            case TupleDestructuringStmt tds: VisitTupleDestructuringStatement(tds); break;
            case ViewPortStmt ws2: EmitWindowViewportStatement(ws2); break;
            case DefineWindowStmt dws: VisitDefineWindowStatement(dws); break;
            case WindowActionStmt was: VisitWindowActionStatement(was); break;
            case WindowPumpStmt wpu: VisitExpression(wpu.Ms); EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.FORM_FRAME); EmitByte((byte)1); EmitByte((byte)OpCode.OP_POP); break;
            case WindowRequestCloseStmt: EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.FORM_CLOSE); EmitByte((byte)0); break;
            case WindowVisibilityStmt wvis: VisitWindowVisibilityStatement(wvis); break;
            case WindowSetTitleStmt wst: VisitWindowSetTitleStatement(wst); break;
            case WindowAppendLineStmt wall: VisitWindowAppendLineStatement(wall); break;
            case WindowSetContentStmt wsc: VisitWindowSetContentStatement(wsc); break;
            case WindowClearStmt wcl: VisitWindowClearStatement(wcl); break;
            case WindowScrollStmt wscr: VisitWindowScrollStatement(wscr); break;
            case WindowEditStmt wed: VisitWindowEditStatement(wed); break;
            case WindowSelectStmt wsel: VisitWindowSelectStatement(wsel); break;
            case WindowThemeStmt wth: VisitWindowThemeStatement(wth); break;
            case WindowDesktopStmt wds: VisitWindowDesktopStatement(wds); break;
            case WindowMoveDesktopStmt wmd: VisitWindowMoveDesktopStatement(wmd); break;
            case WindowFrameStmt wfr: VisitWindowFrameStatement(wfr); break;
            case WindowShadowStmt wsh: VisitWindowShadowStatement(wsh); break;
            case ActivateWindowStmt aws: EmitWindowActionLegacy(WindowAction.Activate, aws.Name); break;
            case DeactivateWindowStmt dws2: EmitWindowActionLegacy(WindowAction.Deactivate, dws2.Name); break;
            case ReleaseWindowStmt rws: EmitWindowActionLegacy(WindowAction.Release, rws.Name); break;
            case MouseConfigStmt mcs: EmitMouseEnableStatement(mcs); break;
            case MouseVisibilityStmt mvs: EmitMouseShowStatement(mvs); break;
            case MouseSetStmt mss: EmitMouseSetPosStatement(mss); break;
            case MouseLimitStmt mls: EmitMouseLimitStatement(mls); break;
            case MouseCursorStmt mcs2: EmitMouseCursorStatement(mcs2); break;
            case MouseWaitStmt mws: EmitMouseWaitStatement(mws); break;
            case OnMouseStmt oms: VisitOnMouseStatement(oms); break;
            case OnSpriteCollisionStmt osc: VisitOnSpriteCollisionStatement(osc); break;
            case ChartPlotStmt cps: EmitChartPlotStatement(cps); break;
            case ChartConfigStmt ccs: VisitChartConfigStatement(ccs); break;
            case ChartDrawStmt cds: VisitChartDrawStatement(cds); break;
            case ChartFinancialStmt cfs: EmitChartFinancialStatement(cfs); break;
            case ChartClearStmt ccs: EmitChartClearStatement(ccs); break;
            case ChartCloseStmt ccs2: EmitChartCloseStatement(ccs2); break;
            case ChartRenderStmt crs: EmitChartRenderStatement(crs); break;
            case CanvasStmt cs: VisitCanvasStatement(cs); break;
            case ConsoleCaptionStmt ccs3: EmitConsoleCaptionStatement(ccs3); break;
            case ConsoleSendKeyStmt csks: EmitConsoleSendKeyStatement(csks); break;
            case ConsoleScrollModeStmt csms: EmitConsoleScrollModeStatement(csms); break;
            case RaiseEventStmt res: EmitRaiseEventStatement(res); break;
            case AddHandlerStmt ahs: EmitAddHandlerStatement(ahs); break;
            case RemoveHandlerStmt rhs: EmitRemoveHandlerStatement(rhs); break;
            case LockStmt ls: EmitFileLockStatement(ls); break;
            case UnlockStmt us: EmitFileUnlockStatement(us); break;
            case IoWatchStmt iows: EmitIoWatchStatement(iows); break;
            case IoUnwatchStmt iouns: EmitIoUnwatchStatement(iouns); break;
            case MemoryMapStmt mms: EmitMemoryMapStatement(mms); break;
            case MemoryMapPokeStmt mmps: EmitMemoryPokeStatement(mmps); break;
            case MemoryMapWriteBytesStmt mmwbs: EmitMemoryWriteBytesStatement(mmwbs); break;
            case EvaluateIndicesStmt eis: EmitExprAndPop(eis); break;
            case UsingStmt us:
                VisitUsingStatement(us);
                break;
            
            // --- LINEAR ALGEBRA COMMANDS ---
            case VectorCommandStmt vcs: VisitVectorCommandStatement(vcs); break;
            case MatrixCommandStmt mcs: VisitMatrixCommandStatement(mcs); break;

            // --- PALETTE COMMANDS (console/graphics, handled by GPM at runtime) ---
            case PaletteStmt ps: EmitPaletteStatement(ps); break;
            case PaletteUseStmt pus: EmitPaletteUseStatement(pus); break;
            case PaletteDefineStmt pds: EmitPaletteDefineStatement(pds); break;
            case PaletteReplaceStmt prs: EmitPaletteReplaceStatement(prs); break;
            case PaletteSaveStmt pss: EmitPaletteSave(pss); break;
            case PaletteLoadStmt pls: EmitPaletteLoad(pls); break;

            // --- CONSOLE/CLIPBOARD/CURSOR (handled by GPM at runtime) ---
            case ClipStmt cs: EmitClipStatement(cs); break;
            case CursorStateStmt css: EmitCursorStateStatement(css); break;
            case CursorShapeStmt csh: EmitCursorShapeStatement(csh); break;
            case SetAttributeStmt sas: VisitSetClipper(sas); break;
            case SetThemeStmt sts: VisitSetThemeStmt(sts); break;
            case SetDateFormatStmt sdf: VisitSetDateFormatStmt(sdf); break;
            case SetClipperStmt scs: VisitSetClipperStatement(scs); break;
            case SaveScreenStmt sss: EmitSaveScreenStatement(sss); break;
            case RestoreScreenStmt rss: EmitRestoreScreenStatement(rss); break;
            case CSaveScreenStmt css4: EmitCSaveScreen(css4); break;
            case CRestoreScreenStmt crs2: EmitCRestoreScreen(crs2); break;

            // --- GRAPHICS COMMANDS (handled by GPM at runtime) ---
            case ScreenStmt scst: VisitScreenStatement(scst); break;
            case TextModeStmt tmst: VisitTextModeStatement(tmst); break;
            case PixelBufferStmt pbst: EmitPixelBufferStatement(pbst); break;
            case ScreenSetStmt sss2: EmitScreenSetStatement(sss2); break;
            case DrawStmt ds: EmitDrawStatement(ds); break;
            case DrawTextStmt dts: EmitDrawTextStatement(dts); break;
            case PSetStmt pset: VisitExpression(pset.X); VisitExpression(pset.Y); if (pset.Color!=null) VisitExpression(pset.Color); else EmitByte((byte)OpCode.OP_PUSH_NIL); EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.PSET); EmitByte((byte)3); break;
            case PresetStmt prst: VisitExpression(prst.X); VisitExpression(prst.Y); EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.PRESET); EmitByte((byte)2); break;
            case FontStmt fs: VisitFontStatement(fs); break;
            case PCopyStmt pcs: VisitPCopyStatement(pcs); break;
            case PointStmt pts: VisitPointStatement(pts); break;
            case BLoadStmt bls: VisitBLoadStatement(bls); break;
            case GraphicsPrintExprStmt gpes: EmitExprAndPop(gpes); break;
            case GraphicsTabStmt gts: EmitExprAndPop(gts); break;
            // Era EmitExprAndPop (no-op): il PAINT non veniva MAI eseguito (bug 5 di
            // IL-Graphics-Test-Bugs). VisitPaintStatement emette il syscall PAINT.
            case PaintStmt paint: VisitPaintStatement(paint); break;
            case ImageGetStmt igs: VisitImageGetStatement(igs); break;
            case ImagePutStmt ips: VisitImagePutStatement(ips); break;
            case BSaveStmt bss: VisitBSaveStatement(bss); break;
            case ImagePutsStmt imps: VisitImagePutsStatement(imps); break;
            case DrawTextureStmt dtx: VisitDrawTextureStatement(dtx); break;
            case SpriteCommandStmt scs: VisitSpriteCmdStmtC(scs); break;
            case TilemapCommandStmt tcs2: VisitTilemapCmdStmtC(tcs2); break;
            case SpritedefStmt sds: VisitSpriteDefStmtC(sds); break;
            case CircleStmt circ: VisitCircleStatement(circ); break;
            case LineStmt line: VisitLineStatement(line); break;
            case PolygonStmt poly: VisitPolygonStatement(poly); break;
            case LoadTextureStmt lts: VisitLoadTextureStatement(lts); break;
            case DrawTexturesStmt dts2: VisitDrawTexturesStatement(dts2); break;
            case UnloadResourceStmt urs: VisitUnloadResourceStatement(urs); break;
            case TickStmt tick: VisitTickStmtC(tick); break;
            case TickEndStmt te: VisitTickEndStmtC(te); break;
            case SyncStmt sync: VisitSyncStmtC(sync); break;
            case OnWindowStmt ows: VisitOnWindowStatementC(ows); break;
            case AmbiguousMethodStmt ams: EmitExprAndPop(ams); break;
            case GraphicsNewlineStmt gns: EmitExprAndPop(gns); break;
            case GraphicsTabZoneStmt gtz: EmitExprAndPop(gtz); break;
            case CCLSStmt ccls: VisitCClsStatement(ccls); break;
            case CPrintStmt cpr: VisitCPrintStatement(cpr); break;
            case CLocateStmt clo: VisitCLocateStatement(clo); break;
            case ClearRectStmt crs: VisitClearRectStatement(crs); break;
            case ViewStmt vs: VisitViewStmtC(vs); break;

            // --- AUDIO COMMANDS (collegati 2026-08-07: prima EmitExprAndPop no-op) ---
            case SoundStmt snd: VisitSoundStatement(snd); break;
            case PlayStmt ply: VisitPlayStatement(ply); break;
            case SoundLoadStmt sls: EmitSoundLoadStatement(sls); break;
            case SoundPlayStmt sps: EmitSoundPlayFileStatement(sps); break;
            case SoundVolumeStmt svs: EmitSoundVolumeStatement(svs); break;
            case SetFilterStmt sfs: VisitSetFilterStatement(sfs); break;
            case AudioControlStmt acs: VisitAudioControlStatement(acs); break;

            // --- TEAM / WORKFLOW (handled by GPM at runtime) ---
            case TeamCreateStmt tcs: VisitTeamCreateStatement(tcs); break;
            case TeamAddAgentStmt taas: VisitTeamAddAgentStatement(taas); break;
            case TeamRemoveAgentStmt tras: VisitTeamRemoveAgentStatement(tras); break;
            case MessageConnectStmt mcc: EmitMessageOp(0, HandleNameExpr(mcc.Handle), mcc.Uri, null); break;
            case MessageSubscribeStmt msu: EmitMessageOp(1, HandleNameExpr(msu.Target), msu.Topic, msu.Alias); break;
            case MessageUnsubscribeStmt mun: EmitMessageOp(2, mun.Sub, null, null); break;
            case MessagePublishStmt mpu: EmitMessageOp(3, HandleNameExpr(mpu.Target), mpu.Topic, mpu.Data); break;
            case MessagePingStmt mpi: EmitMessageOp(4, HandleNameExpr(mpi.Target), null, null); break;
            case MessageCloseStmt mcl: EmitMessageOp(5, HandleNameExpr(mcl.Target), null, null); break;
            case OnMessageStmt mon: VisitOnMessageStatement(mon); break;
            case SignalSubscribeStmt ssub: VisitSignalSubscribeStatement(ssub); break;
            case SignalEmitStmt semit: VisitSignalEmitStatement(semit); break;
            case SignalUnsubscribeStmt sunsub: VisitSignalUnsubscribeStatement(sunsub); break;
            case SignalPingStmt: EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.SIGNAL_PING); EmitByte((byte)0); break;
            case OnSignalStmt son: VisitOnSignalStatement(son); break;
            case TeamMessageStmt tms: VisitTeamMessageStatement(tms); break;
            case TeamRunStmt trs: VisitTeamRunStatement(trs); break;
            case TeamBroadcastStmt tbs: VisitTeamBroadcastStatement(tbs); break;
            case TeamRedefineStmt trd: VisitTeamRedefineStatement(trd); break;
            case TeamSetRoleStmt tsr: VisitTeamSetRoleStatement(tsr); break;
            case TeamSetStrategyStmt tss: VisitTeamSetStrategyStatement(tss); break;
            case TeamSetCoordinatorStmt tsc: VisitTeamSetCoordinatorStatement(tsc); break;
            case TeamSetRecursionStmt tsrec: VisitTeamSetRecursionStatement(tsrec); break;
            case TeamSetLatentStmt tsl: VisitTeamSetLatentStatement(tsl); break;
            case TeamSetLinkStmt tslink: VisitTeamSetLinkStatement(tslink); break;
            case TeamSetThoughtsStmt tsth: VisitTeamSetThoughtsStatement(tsth); break;
            case TeamTrainInnerStmt ttin: VisitTeamTrainInnerStatement(ttin); break;
            case TeamTrainOuterStmt ttout: VisitTeamTrainOuterStatement(ttout); break;
            case TeamSaveStmt tsv: VisitTeamSaveStatement(tsv); break;
            case TeamLoadStmt tld: VisitTeamLoadStatement(tld); break;
            case TeamSetDagStmt tsd: VisitTeamSetDagStatement(tsd); break;
            case TeamSetComplexityStmt tsc: VisitTeamSetComplexityStatement(tsc); break;
            case TeamSetReflectionStmt tsr: VisitTeamSetReflectionStatement(tsr); break;
            case TeamSetCriticStmt tsc: VisitTeamSetCriticStatement(tsc); break;
            case TeamSetPruneStmt tsp: VisitTeamSetPruneStatement(tsp); break;
            case TeamTrainThinkingStmt tth: VisitTeamTrainThinkingStatement(tth); break;
            case TeamTrainActorCriticStmt tac: VisitTeamTrainActorCriticStatement(tac); break;
            case TeamDistillThinkingStmt tdt: VisitTeamDistillThinkingStatement(tdt); break;
            case WorkflowDefStmt wds: VisitWorkflowDefStatement(wds); break;
            case WorkflowStepStmt wss: VisitWorkflowStepStatement(wss); break;
            case WorkflowExecuteStmt wes: VisitWorkflowExecuteStatement(wes); break;
            case WorkflowEndStmt we: VisitWorkflowEndStatement(we); break;
            
            // ISAM — proper handlers (dispatch to Compiler.Isam.cs)
            case IsamUseStmt s: VisitIsamUseStatement(s); break;
            case IsamSelectWorkAreaStmt s: VisitIsamSelectWorkAreaStatement(s); break;
            case IsamRecordStmt s: VisitIsamRecordStatement(s); break;
            case IsamSkipStmt s: VisitIsamSkipStatement(s); break;
            case IsamSeekStmt s: VisitIsamSeekStatement(s); break;
            case IsamAppendStmt s: VisitIsamAppendStatement(s); break;
            case IsamPutStmt s: VisitIsamPutStatement(s); break;
            case IsamDeleteStmt s: VisitIsamDeleteStatement(s); break;
            case IsamRecallStmt s: VisitIsamRecallStatement(s); break;
            case IsamPackStmt s: VisitIsamPackStatement(s); break;
            case IsamReindexStmt s: VisitIsamReindexStatement(s); break;
            case IsamRepairStmt s: VisitIsamRepairStatement(s); break;
            case IsamBackupStmt s: VisitIsamBackupStatement(s); break;
            case IsamLockStmt s: VisitIsamLockStatement(s); break;
            case IsamUnlockStmt s: VisitIsamUnlockStatement(s); break;
            case IsamSetOrderStmt s: VisitIsamSetOrderStatement(s); break;
            case IsamSetRelationStmt s: VisitIsamSetRelationStatement(s); break;
            case IsamCreateIndexStmt s: VisitIsamCreateIndexStatement(s); break;
            case IsamDropIndexStmt s: VisitIsamDropIndexStatement(s); break;
            case IsamValidateStmt s: VisitIsamValidateStatement(s); break;
            case IsamCopyToStmt s: VisitIsamCopyToStatement(s); break;
            case IsamJoinWithStmt s: VisitIsamJoinWithStatement(s); break;
            case IsamSortToStmt s: VisitIsamSortToStatement(s); break;
            case IsamSetFilterStmt s: VisitIsamSetFilterStatement(s); break;
            
            // ISAM OPEN -> delegates to ISAM USE
            case IsamOpenStmt s: VisitIsamUseStatement(new IsamUseStmt(s.FileName,
                -1, s.OriginalTypeName, s.Locking, s.IsUnsafe, s.IndexedBy,
                s.OriginalAliasToken != null ? s.OriginalAliasToken.GetHashCode() : -1,
                s.OriginalAliasToken ?? new Token(TokenKind.Identifier, "", null, s.Token.Position),
                s.Token) { ScopeId = s.ScopeId }); break;
            
            // Other database/graphics — no-op for now
            case DbfUseStmt s: VisitDbfUseStatement(s); break;
            case DbfCloseStmt s: VisitDbfCloseDatabasesStatement(s); break;
            case DbfGoStmt s: VisitDbfGoStatement(s); break;
            case DbfSkipStmt s: VisitDbfSkipStatement(s); break;
            case DbfSeekStmt s: VisitDbfSeekStatement(s); break;
            case DbfGetRecordStmt s: VisitDbfGetRecordStatement(s); break;
            case DbfReplaceStmt s: VisitDbfReplaceStatement(s); break;
            case DbfDeleteStmt s: VisitDbfDeleteStatement(s); break;
            case DbfRecallStmt s: VisitDbfRecallStatement(s); break;
            case DbfAppendBlankStmt s: VisitDbfAppendBlankStatement(s); break;
            case DbfAppendRecordStmt s: VisitDbfAppendRecordStatement(s); break;
            case DbfAppendFromStmt s: VisitDbfAppendFromStatement(s); break;
            case DbfPackStmt s: VisitDbfPackStatement(s); break;
            case DbfZapStmt s: VisitDbfZapStatement(s); break;
            case DbfReindexStmt s: VisitDbfReindexStatement(s); break;
            case DbfSetOrderStmt s: VisitDbfSetOrderStatement(s); break;
            case DbfSetDeletedStmt s: VisitDbfSetDeletedStatement(s); break;
            case DbfSetExactStmt s: VisitDbfSetExactStatement(s); break;
            case DbfIndexOnStmt s: VisitDbfIndexOnStatement(s); break;
            case DbfSetDefaultStmt s: VisitDbfSetDefaultStatement(s); break;
            case RddSetDefaultStmt s: VisitRddSetDefaultStatement(s); break;
            case RddSetDeletedStmt s: VisitRddSetDeletedStatement(s); break;
            case RddUseStmt s: VisitRddUseStatement(s); break;
            case RddLockStmt s: VisitRddLockStatement(s); break;
            case RddSetFieldStmt s: VisitRddSetFieldStatement(s); break;
            case RddDeleteStmt s: VisitRddDeleteStatement(s); break;
            case RddRecallStmt s: VisitRddRecallStatement(s); break;
            case RddReplaceStmt s: VisitRddReplaceStatement(s); break;
            case RddSetReprocessStmt s: VisitRddSetReprocessStatement(s); break;
            case RddSetOrderStmt s: VisitRddSetOrderStatement(s); break;
            case RddSetLockTimeoutStmt s: VisitRddSetLockTimeoutStatement(s); break;
            case RddOnEventStmt s: VisitRddOnEventStatement(s); break;
            case RddOffEventStmt s: VisitRddOffEventStatement(s); break;
            case RddCreateIndexStmt s: VisitRddCreateIndexStatement(s); break;
            case RddCloseIndexStmt s: VisitRddCloseIndexStatement(s); break;
            case RddCopyToStmt s: VisitRddCopyToStatement(s); break;
            case RddSortToStmt s: VisitRddSortToStatement(s); break;
            case RddTotalOnStmt s: VisitRddTotalOnStatement(s); break;
            case DbfCopyStructureStmt s: VisitDbfCopyStructureStatement(s); break;
            case DbfSelectWorkAreaStmt s: VisitDbfSelectWorkAreaStatement(s); break;
            case DbfSetFilterStmt s: VisitDbfSetFilterStatement(s); break;
            case DbfCopyToStmt s: VisitDbfCopyToStatement(s); break;
            case DbfJoinWithStmt s: VisitDbfJoinWithStatement(s); break;
            case DbfSetRelationStmt s: VisitDbfSetRelationStatement(s); break;
            case DbfSetPathStmt s: VisitDbfSetPathStatement(s); break;
            case DbfSetExclusiveStmt s: VisitDbfSetExclusiveStatement(s); break;
            case DbfSetSchemaStmt s: VisitDbfSetSchemaStatement(s); break;
            case DbfValidateStmt s: VisitDbfValidateStatement(s); break;
            case DbfCreateStmt s: VisitDbfCreateStatement(s); break;
            case DbfTotalOnStmt s: VisitDbfTotalOnStatement(s); break;
            case DbfSortStmt s: VisitDbfSortStatement(s); break;
            case DbfUpdateStmt s: VisitDbfUpdateStatement(s); break;
            case DbfAggregateStmt s: VisitDbfAggregateStatement(s); break;
            case DbfCreateViewStmt s: VisitDbfCreateViewStatement(s); break;
            case DbfListStmt s: VisitDbfListStatement(s); break;
            case DbfReportStmt s: VisitDbfReportStatement(s); break;
            case LedgerOpenStmt s: VisitLedgerOpenStatement(s); break;
            case LedgerAddStmt s: VisitLedgerAddStatement(s); break;
            case LedgerRegisterStmt s: VisitLedgerRegisterStatement(s); break;
            case LedgerBalanceStmt s: VisitLedgerBalanceStatement(s); break;
            case LedgerGetStmt s: VisitLedgerGetStatement(s); break;
            case LedgerEditStmt s: VisitLedgerEditStatement(s); break;
            case LedgerDeleteStmt s: VisitLedgerDeleteStatement(s); break;
            case LedgerSaveStmt s: VisitLedgerSaveStatement(s); break;
            case LedgerCloseStmt s: VisitLedgerCloseStatement(s); break;
            case LedgerPackStmt s: VisitLedgerPackStatement(s); break;
            case LedgerAccountsStmt s: VisitLedgerAccountsStatement(s); break;
            case LedgerValidateStmt s: VisitLedgerValidateStatement(s); break;
            case LedgerBudgetStmt s: VisitLedgerBudgetStatement(s); break;
            case LedgerVarianceStmt s: VisitLedgerVarianceStatement(s); break;
            case LedgerPlotStmt s: VisitLedgerPlotStatement(s); break;
            case LedgerImportStmt s: VisitLedgerImportStatement(s); break;
            case SpeedOpenStmt s: VisitSpeedOpenStatement(s); break;
            case SpeedCloseStmt sc: VisitSpeedCloseStatement(sc); break;
            case SpeedUseStmt s: VisitSpeedUseStatement(s); break;
            case SpeedGoStmt s: VisitSpeedGoStatement(s); break;
            case SpeedSkipStmt s: VisitSpeedSkipStatement(s); break;
            case SpeedSeekStmt s: VisitSpeedSeekStatement(s); break;
            case SpeedGetStmt s: VisitSpeedGetStatement(s); break;
            case SpeedPutStmt s: VisitSpeedPutStatement(s); break;
            case SpeedDeleteStmt s: VisitSpeedDeleteStatement(s); break;
            case SpeedCreateIndexStmt s: VisitSpeedCreateIndexStatement(s); break;
            case SpeedDropIndexStmt s: VisitSpeedDropIndexStatement(s); break;
            case SpeedSetOrderStmt s: VisitSpeedSetOrderStatement(s); break;
            case SpeedSetFilterStmt s: VisitSpeedSetFilterStatement(s); break;
            case SpeedSetRelationStmt s: VisitSpeedSetRelationStatement(s); break;
            case SpeedSelectStmt s: VisitSpeedSelectStatement(s); break;
            case SpeedCreateViewStmt s: VisitSpeedCreateViewStatement(s); break;
            case SpeedRefreshViewStmt s: VisitSpeedRefreshViewStatement(s); break;
            case SpeedDropViewStmt s: VisitSpeedDropViewStatement(s); break;
            case BeginTransactionStmt bts: VisitBeginTransactionStatement(bts); break;
            case CommitStmt cs: VisitCommitStatement(cs); break;
            case RollbackStmt rs: VisitRollbackStatement(rs); break;
            case ExportToFileStmt ets: VisitExportToFileStatement(ets); break;
            case ImportFromFileStmt ifs: VisitImportFromFileStatement(ifs); break;
            case ConvertDbfToIsamStmt cdis: VisitConvertDbfToIsamStatement(cdis); break;
            
            default:
                throw new NotImplementedException(
                    $"Compiler support for statement type '{stmt.GetType().Name}' is not yet implemented.");
        }
        
        // OP_YIELD: NON più emesso incondizionatamente dopo gli statement bloccanti.
        //
        // ALLINEAMENTO AL MODELLO GO (2026-08-18): in Go (e nella GPM C99) una goroutine NON cede
        // mai a meno che l'operazione non si blocchi DAVVERO (canale pieno/vuoto, wg>0, sleep,
        // select senza case pronto) o ci sia un Gosched() esplicito (TaskYield). Un SEND su canale
        // con spazio, o un RECEIVE con dato disponibile, proseguono immediatamente.
        //
        // L'OP_YIELD incondizionato replicava l'interleaving dell'AST (granularità "statement che
        // PUÒ bloccarsi"), non di Go. Con la granularità a blocco reale gli opcode di blocco
        // (OP_SEND/OP_RECV/OP_WG_WAIT/OP_SLEEP/OP_GO_SELECT_SETUP) ritornano 0 SOLO quando si
        // bloccano davvero — il tick prosegue altrimenti. TaskYield() è l'analogo di Gosched() e
        // viene gestito come builtin (vedi gmb_vm_core.c, case YIELD con 0 argomenti).
        //
        // Storia (per non ripetere): aggiungere GoProcedureStmt alla vecchia lista (yield subito
        // dopo aver accodato una goroutine) era stato provato e SCARTATO per GoSelectNoSleep.bas /
        // GoRoutinesMinimal.bas / GoSelectSimple.bas — ma quello era il tentativo di replicare
        // l'AST, non Go. L'ordine relativo tra i due modelli può differire; canali/waitgroup/select
        // consegnano comunque i valori giusti (stesso principio della Regola d'Oro #7 in
        // GMBasic-GoRoutines-Hard-Lessons).
    }

    /// <summary>
    /// Emette P-Code per valutare tutte le espressioni figlie di uno statement e poi pulire lo stack.
    /// Usato per statement non ancora implementati: almeno le espressioni vengono valutate.
    /// </summary>
    private void EmitExprAndPop(Stmt stmt)
    {
        foreach (var prop in stmt.GetType().GetProperties())
        {
            var val = prop.GetValue(stmt);
            if (val is Expr expr) { VisitExpression(expr); EmitByte((byte)OpCode.OP_POP); }
            else if (val is System.Collections.Immutable.ImmutableArray<Expr> exprs)
                foreach (var e in exprs) { VisitExpression(e); EmitByte((byte)OpCode.OP_POP); }
            else if (val is System.Collections.Immutable.ImmutableArray<Stmt> body)
                foreach (var s in body) VisitStatement(s);
        }
    }

    /// <summary>
    /// Compila una dichiarazione di procedura locale (LOCAL SUB/FUNCTION).
    /// Questa è l'operazione più complessa, poiché richiede la gestione delle closure.
    /// </summary>
    private void VisitLocalProcedureStatement(LocalProcedureStmt stmt)
    {
        // 0. Register in procedure map BEFORE compiling the child body,
        //    so recursive calls inside the body can find this function.
        var procName = stmt.Procedure.OriginalName.Lexeme;
        var procKey = (procName, stmt.Procedure.Parameters.Length);
        if (!_procedureMap.ContainsKey(procKey))
            _procedureMap[procKey] = stmt.Procedure;

        // 1. Crea un compilatore figlio per compilare il corpo della funzione locale.
        var functionCompiler = new Compiler(this);
        var functionChunk = functionCompiler.CompileProcedureBody(stmt.Procedure);

        // 1c. Emetti un salto in avanti per saltare il corpo della funzione
        var skipJumpPos = EmitJump((byte)OpCode.OP_JUMP_FORWARD);
        var functionAddress = _chunk.Code.Count;

        // 1b. Inserisci le istruzioni STORE_LOCAL per i parametri all'inizio del chunk figlio
        var paramCount = stmt.Procedure.Parameters.Length;
        // S_eval: shift del codice figlio dovuto al prologo inserito in testa (0 se nessun parametro).
        var childCodeShift = 0;
        if (paramCount > 0)
        {
            // Salva i jumps del figlio PRIMA degli inserimenti (gli offset sono pre-shift)
            var childJumps = functionCompiler.GetJumpsToPatch().ToList();

            // Lo slot 0 e' riservato al return-slot SOLO per una FUNCTION (CompileProcedureBody
            // (ProcedureDefinition) chiama AddLocal(proc.OriginalName, ...) PRIMA di aggiungere
            // i parametri — vedi lo stesso schema nel loop top-level di CompileProcedures,
            // Compiler.cs). Una LOCAL SUB non ha quel local aggiuntivo: i suoi parametri
            // iniziano quindi da slot 0, non 1 — usare sempre "1 + i" scriveva i parametri
            // di una LOCAL SUB nello slot SBAGLIATO (off-by-one).
            var slotOffset = stmt.Procedure is FunctionDefinition ? 1 : 0;

            // BUG (scoperto correggendo l'assegnazione bare al return-slot, vedi
            // ParseFunctionReturnAssignmentStatement/RewritePreResolvedSymbol): lo slot di
            // OP_STORE_LOCAL e' un operando a 2 byte ovunque nel resto del compilatore
            // (EmitShort, letto da ReadU16 nella VM) — qui veniva scritto un SOLO byte,
            // disallineando la lettura di OGNI istruzione successiva del chunk figlio non
            // appena lo slot superava 255 o anche solo per il byte immediatamente seguente
            // interpretato come parte dello slot. Con un solo parametro (slot 1) l'unico
            // sintomo visibile era il valore di ritorno stantio (bug: `FUNCTION Quadrato(N):
            // Quadrato = N * N: END FUNCTION` restituiva N invece di N*N sul motore
            // PCode-managed) perche' il CLEAR_RETURN successivo veniva comunque decodificato
            // (per coincidenza di byte), ma con più parametri l'intero corpo si disallineava.
            var popCodes = new List<byte>();
            for (int i = paramCount - 1; i >= 0; i--)
            {
                var slot = slotOffset + i;
                popCodes.Add((byte)OpCode.OP_STORE_LOCAL);
                popCodes.Add((byte)((slot >> 8) & 0xFF));
                popCodes.Add((byte)(slot & 0xFF));
            }
            var popBytes = popCodes.Count;
            functionChunk.Code.InsertRange(0, popCodes);
            functionChunk.Code.InsertRange(popBytes, new byte[] { (byte)OpCode.OP_CLEAR_RETURN });

            // Dopo gli inserimenti, gli offset del figlio sono shiftati di (popBytes + 1)
            var shiftAmount = popBytes + 1;
            childCodeShift = shiftAmount;
            foreach (var (childOffset, target) in childJumps)
            {
                _jumpsToPatch.Add((functionAddress + childOffset + shiftAmount, target));
            }
        }

        // 2. Il compilatore figlio ha prodotto il P-Code per la funzione.
        _chunk.Code.AddRange(functionChunk.Code);
        _chunk.Lines.AddRange(functionChunk.Lines);
        // S_eval: propaga lo snapshot locali del figlio (shift = prologo param/CLEAR_RETURN).
        _chunk.MergeChildLocalNames(functionChunk, functionAddress, childCodeShift);

        // Propaga i jumps del figlio al genitore (solo se non ci sono parametri,
        // altrimenti è già stato fatto dentro il blocco paramCount > 0 con shift).
        if (paramCount == 0)
        {
            foreach (var (childOffset, target) in functionCompiler.GetJumpsToPatch())
            {
                _jumpsToPatch.Add((functionAddress + childOffset, target));
            }
        }

        // Patch the skip jump: offset = after_func - (skipJumpPos + 2)
        var afterFunc = _chunk.Code.Count;
        var jumpOffset = afterFunc - (skipJumpPos + 2);
        _chunk.Code[skipJumpPos] = (byte)((jumpOffset >> 8) & 0xFF);
        _chunk.Code[skipJumpPos + 1] = (byte)(jumpOffset & 0xFF);

        // Register in procedure address map
        _procedureAddresses[procName] = functionAddress;
        _procedureAddresses[stmt.Procedure.OriginalName.Lexeme] = functionAddress;
        // 2026-09-24: indirizzo per-Simbolo (disambigua le omonime in namespace diversi).
        if (stmt.Procedure.SymbolId >= 0)
            _procedureAddresses[$"@sym{stmt.Procedure.SymbolId}"] = functionAddress;
        // Add constants from child chunk
        foreach (var constant in functionChunk.Constants)
        {
            AddConstant(constant);
        }

        // 3. Emetti OP_CLOSURE
        EmitByte((byte)OpCode.OP_CLOSURE);
        var constIndex = AddConstant((double)functionAddress);
        EmitShort(constIndex);
        if (_enclosing != null) _pendingClosureFixes.Add((constIndex, functionAddress));
        // Riloca gli indirizzi-closure del figlio (anche annidati): + base del chunk nel
        // genitore (+ shift del prologo param/CLEAR_RETURN).
        foreach (var (fixIdx, fixAddr) in functionCompiler.GetPendingClosureFixes())
        {
            var relocated = fixAddr + functionAddress + childCodeShift;
            if (_enclosing == null) _chunk.Constants[fixIdx] = (double)relocated;
            else _pendingClosureFixes.Add((fixIdx, relocated));
        }

        // 4. Emetti le upvalues
        var upvalues = functionCompiler.GetUpvalues();
        EmitByte((byte)upvalues.Count);
        foreach (var upvalue in upvalues)
        {
            EmitByte(upvalue.Kind);
            EmitByte((byte)(upvalue.Index));
        }

        // 5. Assegna la closure a una variabile locale con FunctionPointerType
        var paramTypes = stmt.Procedure.Parameters.Select(p => p.DataType).ToImmutableArray();
        var returnType = (stmt.Procedure as FunctionDefinition)?.ReturnType ?? new VoidType();
        AddLocal(stmt.Procedure.OriginalName, new FunctionPointerType(paramTypes, returnType));
        var locIdx = _locals.Count - 1;
        EmitByte((byte)OpCode.OP_STORE_LOCAL); EmitShort((ushort)locIdx);
        _procedureAddresses[stmt.Procedure.OriginalName.Lexeme] = functionAddress;
    }

    /// <summary>Compila WINDOW name ACTION (sistema finestre TUI) — syscall WINDOW_ACTION.</summary>
    private void VisitWindowActionStatement(WindowActionStmt stmt)
    {
        EmitConstant((double)(int)stmt.Action);
        EmitConstant(stmt.Name.Lexeme);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.WINDOW_ACTION); EmitByte((byte)2);
    }

    /// <summary>WINDOW SHOW | WINDOW HIDE (globale): riusa WINDOW_ACTION con nome vuoto
    /// e azione Show(6)/Hide(7). I runtime mappano (nome=="", azione 6/7) sulla
    /// visibilita' dello screen grafico.</summary>
    private void VisitWindowVisibilityStatement(WindowVisibilityStmt stmt)
    {
        EmitConstant((double)(int)(stmt.Show ? WindowAction.Show : WindowAction.Hide));
        EmitConstant("");
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.WINDOW_ACTION); EmitByte((byte)2);
    }

    /// <summary>Compila WINDOW SET TITLE "x" — riusa il runtime BuiltInFunction.SETWINDOWTITLE.</summary>
    private void VisitWindowSetTitleStatement(WindowSetTitleStmt stmt)
    {
        VisitExpression(stmt.Title);
        EmitByte((byte)OpCode.OP_CALL_BUILTIN);
        EmitShort((ushort)BuiltInFunction.SETWINDOWTITLE);
        EmitByte((byte)1);
        EmitByte((byte)OpCode.OP_POP); // scarta il valore di ritorno: è un comando
    }
    private void VisitWindowAppendLineStatement(WindowAppendLineStmt stmt)
    {
        VisitExpression(stmt.Text);
        EmitConstant(stmt.Name.Lexeme);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.WINDOW_APPEND); EmitByte((byte)2);
    }
    private void VisitWindowSetContentStatement(WindowSetContentStmt stmt)
    {
        VisitExpression(stmt.Lines);
        EmitConstant(stmt.Name.Lexeme);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.WINDOW_SET_CONTENT); EmitByte((byte)2);
    }
    private void VisitWindowClearStatement(WindowClearStmt stmt)
    {
        EmitConstant(stmt.Name.Lexeme);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.WINDOW_CLEAR); EmitByte((byte)1);
    }
    private void VisitWindowScrollStatement(WindowScrollStmt stmt)
    {
        VisitExpression(stmt.Delta);
        EmitConstant(stmt.Name.Lexeme);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.WINDOW_SCROLL); EmitByte((byte)2);
    }
    private void VisitWindowEditStatement(WindowEditStmt stmt)
    {
        /* enable come ARGOMENTO STACK (come il case PCode: PopInt(enable) poi PopRef(name)):
         * il vecchio EmitByte post-argc non era consumato dal decode OP_SYSCALL → desync
         * del flusso bytecode (il byte finiva interpretato come opcode). */
        EmitConstant(stmt.Name.Lexeme);
        EmitConstant(stmt.Enable ? 1.0 : 0.0);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.WINDOW_EDIT); EmitByte((byte)2);
    }
    private void VisitWindowSelectStatement(WindowSelectStmt stmt)
    {
        VisitExpression(stmt.Target);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.WINDOW_SELECT); EmitByte((byte)1);
    }
    private void VisitWindowThemeStatement(WindowThemeStmt stmt)
    {
        VisitExpression(stmt.ThemeName);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.WINDOW_THEME); EmitByte((byte)1);
    }

    private void VisitWindowDesktopStatement(WindowDesktopStmt stmt)
    {
        VisitExpression(stmt.Desktop);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.WINDOW_DESKTOP); EmitByte((byte)1);
    }

    private void VisitWindowMoveDesktopStatement(WindowMoveDesktopStmt stmt)
    {
        EmitConstant(stmt.Name.Lexeme);
        VisitExpression(stmt.Desktop);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.WINDOW_MOVE_DESKTOP); EmitByte((byte)2);
    }

    private void VisitWindowFrameStatement(WindowFrameStmt stmt)
    {
        if (stmt.Name != null) EmitConstant(stmt.Name.Lexeme);
        else EmitConstant("");
        EmitConstant((double)(int)stmt.Style);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.WINDOW_FRAME); EmitByte((byte)2);
    }

    private void VisitWindowShadowStatement(WindowShadowStmt stmt)
    {
        if (stmt.Name != null) EmitConstant(stmt.Name.Lexeme);
        else EmitConstant("");
        EmitConstant((double)(stmt.Enable ? 1 : 0));
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.WINDOW_SHADOW); EmitByte((byte)2);
    }

    /// <summary>Compila WINDOW DEFINE ... (sistema finestre TUI/pixel) — syscall WINDOW_DEFINE.</summary>
    private void VisitDefineWindowStatement(DefineWindowStmt stmt)
    {
        VisitExpression(stmt.Title ?? new StringExpr(stmt.Name.Lexeme, stmt.Name));
        VisitExpression(stmt.Top);
        VisitExpression(stmt.Left);
        VisitExpression(stmt.Bottom);
        VisitExpression(stmt.Right);
        EmitConstant((double)(stmt.Pixel ? 1 : 0));
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.WINDOW_DEFINE); EmitByte((byte)6);
    }

    // ── MEMORY MAPPED FILES (IO.MEMORYMAP / MMAPPOKE / MMAPWRITEBYTES) ──────────────────

    /// <summary>Compila MEMORYMAP filePath AS handle — syscall MEMORYMAP_OPEN (pusha l'handle).</summary>
    /// <summary>TEAM CREATE name — crea un team (era EmitExprAndPop no-op).</summary>
    private void VisitTeamCreateStatement(TeamCreateStmt stmt)
    {
        EmitConstant(stmt.TeamName.Lexeme);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.TEAM_CREATE); EmitByte((byte)1);
    }

    /// <summary>TEAM name ADD AGENT agent — aggiunge un agente al team.</summary>
    private void VisitTeamAddAgentStatement(TeamAddAgentStmt stmt)
    {
        EmitConstant(stmt.AgentName.Lexeme);
        EmitConstant(stmt.TeamName.Lexeme);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.TEAM_ADD_AGENT); EmitByte((byte)2);
    }

    /// <summary>TEAM name REMOVE AGENT agent — rimuove un agente dal team.</summary>
    private void VisitTeamRemoveAgentStatement(TeamRemoveAgentStmt stmt)
    {
        EmitConstant(stmt.AgentName.Lexeme);
        EmitConstant(stmt.TeamName.Lexeme);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.TEAM_REMOVE_AGENT); EmitByte((byte)2);
    }

    /// <summary>MESSAGE name, testo — accoda un messaggio nella conversazione del team.</summary>
    private void VisitTeamMessageStatement(TeamMessageStmt stmt)
    {
        VisitExpression(stmt.Message);
        EmitConstant(stmt.TeamName.Lexeme);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.TEAM_MESSAGE); EmitByte((byte)2);
    }

    private static Expr? HandleNameExpr(Target? t)
        => t is VariableTarget vt ? new StringExpr(vt.NameCleaned.Lexeme, vt.NameCleaned) : null;

    /// <summary>MESSAGE: P-CodeVM e VM/C identificano il bus col NOME della variabile
    /// (`_msgBus["bus"]`, `vmsg_find_bus("bus")`), e CONNECT lo registra così. Se SUBSCRIBE/
    /// PUBLISH/PING/CLOSE passassero il VALORE, il bus non verrebbe mai trovato: nessun
    /// SUB/PUB verso il broker e PUBLISH ripiegherebbe sulla consegna LOCALE (parità solo
    /// apparente su un test TCP). I backend a handle numerico (IL, C99) non passano da qui.
    /// Un target non-variabile (es. `MESSAGE PUBLISH GetBus() ...`) resta com'è.</summary>
    private static Expr? HandleNameExpr(Expr? e)
        => e is VariableExpr ve ? new StringExpr(ve.NameCleaned.Lexeme, ve.NameCleaned) : e;

    private void EmitMessageOp(int subop, Expr? a, Expr? b, Expr? c)
    {
        if (c != null) VisitExpression(c); else EmitConstant("");
        if (b != null) VisitExpression(b); else EmitConstant("");
        if (a != null) VisitExpression(a); else EmitConstant("");
        EmitConstant((double)subop);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.MESSAGE_OP); EmitByte((byte)4);
    }

    private void VisitOnMessageStatement(OnMessageStmt stmt)
    {
        var sub = stmt.Sub is StringExpr se ? se.Value : "";
        var handler = stmt.Action is MessageCallAction mca ? mca.Handler.Lexeme : "";
        EmitConstant(handler);
        EmitConstant(sub);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.ON_MESSAGE); EmitByte((byte)2);
    }

    // ── SIGNAL (namespace SIGNAL) ──
    // Push nell'ordine del layout della VM (l'ULTIMO push è il PRIMO pop).
    private void EmitSignalStrArg(Expr? e)
    {
        if (e is StringExpr se) EmitConstant(se.Value);
        else if (e is null) EmitConstant("");
        else VisitExpression(e);
    }

    private void VisitSignalSubscribeStatement(SignalSubscribeStmt stmt)
    {
        // push: id, types, asset, market, symbols, timeframe → pop: timeframe, symbols, market, asset, types, id
        EmitSignalStrArg(stmt.Id);
        EmitSignalStrArg(stmt.Types);
        EmitSignalStrArg(stmt.Asset);
        EmitSignalStrArg(stmt.Market);
        EmitSignalStrArg(stmt.Symbols);
        EmitSignalStrArg(stmt.Timeframe);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.SIGNAL_SUBSCRIBE); EmitByte((byte)6);
    }

    private void VisitSignalEmitStatement(SignalEmitStmt stmt)
    {
        // push: type, symbol, value, asset, market, timeframe
        EmitSignalStrArg(stmt.Type);
        EmitSignalStrArg(stmt.Symbol);
        VisitExpression(stmt.Value);
        EmitSignalStrArg(stmt.Asset);
        EmitSignalStrArg(stmt.Market);
        EmitSignalStrArg(stmt.Timeframe);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.SIGNAL_EMIT); EmitByte((byte)6);
    }

    private void VisitSignalUnsubscribeStatement(SignalUnsubscribeStmt stmt)
    {
        EmitSignalStrArg(stmt.Id);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.SIGNAL_UNSUBSCRIBE); EmitByte((byte)1);
    }

    private void VisitOnSignalStatement(OnSignalStmt stmt)
    {
        var id = stmt.Sub is StringExpr se ? se.Value : "";
        var handler = stmt.Action is MessageCallAction mca ? mca.Handler.Lexeme : "";
        EmitConstant(handler);
        EmitConstant(id);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.ON_SIGNAL); EmitByte((byte)2);
    }

    /// <summary>TEAM name RUN task — orchestrazione (syscall TEAM_RUN).</summary>
    private void VisitTeamRunStatement(TeamRunStmt stmt)
    {
        VisitExpression(stmt.Task);
        EmitConstant(stmt.TeamName.Lexeme);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.TEAM_RUN); EmitByte((byte)2);
    }

    private void VisitTeamBroadcastStatement(TeamBroadcastStmt stmt)
    {
        VisitExpression(stmt.Message);
        EmitConstant(stmt.TeamName.Lexeme);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.TEAM_BROADCAST); EmitByte((byte)2);
    }

    private void VisitTeamRedefineStatement(TeamRedefineStmt stmt)
    {
        VisitExpression(stmt.Definition);
        EmitConstant(stmt.TeamName.Lexeme);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.TEAM_REDEFINE); EmitByte((byte)2);
    }

    private void VisitTeamSetRoleStatement(TeamSetRoleStmt stmt)
    {
        EmitConstant(stmt.RoleName.Lexeme);
        EmitConstant(stmt.AgentName.Lexeme);
        EmitConstant(stmt.TeamName.Lexeme);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.TEAM_SET_ROLE); EmitByte((byte)3);
    }

    private void VisitTeamSetStrategyStatement(TeamSetStrategyStmt stmt)
    {
        EmitConstant(stmt.StrategyName.Lexeme);
        EmitConstant(stmt.TeamName.Lexeme);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.TEAM_SET_STRATEGY); EmitByte((byte)2);
    }

    private void VisitTeamSetCoordinatorStatement(TeamSetCoordinatorStmt stmt)
    {
        EmitConstant(stmt.AgentName.Lexeme);
        EmitConstant(stmt.TeamName.Lexeme);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.TEAM_SET_COORDINATOR); EmitByte((byte)2);
    }

    private void VisitTeamSetRecursionStatement(TeamSetRecursionStmt stmt)
    {
        VisitExpression(stmt.Rounds);
        EmitConstant(stmt.TeamName.Lexeme);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.TEAM_SET_RECURSION); EmitByte((byte)2);
    }

    private void VisitTeamSetLatentStatement(TeamSetLatentStmt stmt)
    {
        EmitConstant(stmt.Mode.Lexeme);
        EmitConstant(stmt.TeamName.Lexeme);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.TEAM_SET_LATENT); EmitByte((byte)2);
    }

    private void VisitTeamSetLinkStatement(TeamSetLinkStmt stmt)
    {
        VisitExpression(stmt.Config);
        EmitConstant(stmt.Which.Lexeme);
        EmitConstant(stmt.TeamName.Lexeme);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.TEAM_SET_LINK); EmitByte((byte)3);
    }

    private void VisitTeamSetThoughtsStatement(TeamSetThoughtsStmt stmt)
    {
        VisitExpression(stmt.Count);
        EmitConstant(stmt.TeamName.Lexeme);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.TEAM_SET_THOUGHTS); EmitByte((byte)2);
    }

    private void VisitTeamTrainInnerStatement(TeamTrainInnerStmt stmt)
    {
        VisitExpression(stmt.Agent);
        VisitExpression(stmt.Dataset);
        EmitConstant(stmt.TeamName.Lexeme);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.TEAM_TRAIN_INNER); EmitByte((byte)3);
    }

    private void VisitTeamTrainOuterStatement(TeamTrainOuterStmt stmt)
    {
        VisitExpression(stmt.Dataset);
        EmitConstant(stmt.TeamName.Lexeme);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.TEAM_TRAIN_OUTER); EmitByte((byte)2);
    }

    private void VisitTeamSaveStatement(TeamSaveStmt stmt)
    {
        EmitConstant(stmt.TeamName.Lexeme);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.TEAM_SAVE); EmitByte((byte)1);
    }

    private void VisitTeamLoadStatement(TeamLoadStmt stmt)
    {
        EmitConstant(stmt.TeamName.Lexeme);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.TEAM_LOAD); EmitByte((byte)1);
    }

    private void VisitTeamSetDagStatement(TeamSetDagStmt stmt)
    {
        VisitExpression(stmt.NMax);
        VisitExpression(stmt.LMax);
        VisitExpression(stmt.DMax);
        EmitConstant(stmt.TeamName.Lexeme);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.TEAM_SET_DAG); EmitByte((byte)4);
    }

    private void VisitTeamSetComplexityStatement(TeamSetComplexityStmt stmt)
    {
        VisitExpression(stmt.Threshold);
        EmitConstant(stmt.TeamName.Lexeme);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.TEAM_SET_COMPLEXITY); EmitByte((byte)2);
    }

    private void VisitTeamSetReflectionStatement(TeamSetReflectionStmt stmt)
    {
        EmitConstant(stmt.Mode.Lexeme);
        EmitConstant(stmt.TeamName.Lexeme);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.TEAM_SET_REFLECTION); EmitByte((byte)2);
    }

    private void VisitTeamSetCriticStatement(TeamSetCriticStmt stmt)
    {
        VisitExpression(stmt.Judge);
        EmitConstant(stmt.Personas.Lexeme);
        EmitConstant(stmt.TeamName.Lexeme);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.TEAM_SET_CRITIC); EmitByte((byte)3);
    }

    private void VisitTeamSetPruneStatement(TeamSetPruneStmt stmt)
    {
        VisitExpression(stmt.TopK);
        EmitConstant(stmt.TeamName.Lexeme);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.TEAM_SET_PRUNE); EmitByte((byte)2);
    }

    private void VisitTeamTrainThinkingStatement(TeamTrainThinkingStmt stmt)
    {
        VisitExpression(stmt.Dataset);
        EmitConstant(stmt.TeamName.Lexeme);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.TEAM_TRAIN_THINKING); EmitByte((byte)2);
    }

    private void VisitTeamTrainActorCriticStatement(TeamTrainActorCriticStmt stmt)
    {
        VisitExpression(stmt.Dataset);
        EmitConstant(stmt.TeamName.Lexeme);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.TEAM_TRAIN_ACTOR_CRITIC); EmitByte((byte)2);
    }

    private void VisitTeamDistillThinkingStatement(TeamDistillThinkingStmt stmt)
    {
        VisitExpression(stmt.Dataset);
        EmitConstant(stmt.TeamName.Lexeme);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.TEAM_DISTILL_THINKING); EmitByte((byte)2);
    }

    private void VisitWorkflowDefStatement(WorkflowDefStmt stmt)
    {
        EmitConstant(stmt.WorkflowName.Lexeme);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.WORKFLOW_DEF); EmitByte((byte)1);
    }

    private void VisitWorkflowStepStatement(WorkflowStepStmt stmt)
    {
        // Order: timeout, retry, action, name (name top) → PCode pops name, action, retry, timeout
        if (stmt.TimeoutMs is not null) VisitExpression(stmt.TimeoutMs); else EmitConstant(0.0);
        if (stmt.RetryCount is not null) VisitExpression(stmt.RetryCount); else EmitConstant(0.0);
        VisitExpression(stmt.Action);
        EmitConstant(stmt.StepName.Lexeme);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.WORKFLOW_STEP); EmitByte((byte)4);
    }

    private void VisitWorkflowEndStatement(WorkflowEndStmt stmt)
    {
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.WORKFLOW_END); EmitByte((byte)0);
    }

    private void VisitWorkflowExecuteStatement(WorkflowExecuteStmt stmt)
    {
        EmitConstant(stmt.WorkflowName.Lexeme);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.WORKFLOW_EXECUTE); EmitByte((byte)1);
    }

    private void EmitMemoryMapStatement(MemoryMapStmt stmt)
    {
        VisitExpression(stmt.FilePath);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.MEMORYMAP_OPEN); EmitByte((byte)1);
        AssignVariable(stmt.Handle, false);
    }

    /// <summary>
    /// Compila `USING var = expr ... END USING`. Prima di questo fix (BUG-8) il valore di
    /// ResourceInit veniva valutato e SUBITO scartato (OP_POP) — ResourceTarget non veniva MAI
    /// legata a un local/global: `r.Campo` dentro il corpo leggeva un valore vuoto/nil, non
    /// l'istanza appena creata (bug scoperto insieme a "DESTRUCTOR non chiamato", più basilare
    /// di quanto descritto nel report originale). Ora il valore viene assegnato a
    /// ResourceTarget con lo stesso meccanismo di una assegnazione normale (AssignVariable,
    /// stesso pattern di EmitMemoryMapStatement sopra), poi — a END USING — si finalizza
    /// (DESTRUCTOR) l'istanza se ResourceTarget contiene un oggetto di classe. No-op innocuo per
    /// risorse non-oggetto (es. handle file numerici): FinalizeInstance esce subito per valori
    /// non-dict, quindi il comportamento esistente per `USING F = OPEN(...)` resta invariato.
    /// </summary>
    private void VisitUsingStatement(UsingStmt stmt)
    {
        // USING F = OPEN(path, MODE) — forma FUNZIONE di OPEN: apre il file con fileNum 0
        // (come lo statement `OPEN path FOR MODE AS #0`) e lega F = 0, così PRINT #F / INPUT #F
        // del corpo usano il fileNum 0. Senza, `OPEN(path, MODE)` veniva valutato come
        // espressione generica (ArrayAccess) → F non era il fileNum corretto → INPUT #F leggeva
        // vuoto/0 (parità col backend IL, che già special-casa questo caso).
        if (stmt.ResourceInit is ArrayAccessExpr usingOpen
            && usingOpen.NameCleaned.Kind == TokenKind.Open && usingOpen.Indices.Length == 2)
        {
            EmitUsingFunctionOpen(usingOpen.Indices[0], usingOpen.Indices[1]);
            EmitConstant(0.0); // F = fileNum 0
            AssignVariable(stmt.ResourceTarget, false);
        }
        else
        {
            VisitExpression(stmt.ResourceInit);
            AssignVariable(stmt.ResourceTarget, false);
        }

        foreach (var s in stmt.Body) VisitStatement(s);

        // END USING per `OPEN(path, MODE)`: CHIUDE il file (fileNum 0) — flush + rilascio
        // (parità IL: senza, la scrittura non è flushata e una rilettura immediata legge vuoto).
        if (stmt.ResourceInit is ArrayAccessExpr closeOpen
            && closeOpen.NameCleaned.Kind == TokenKind.Open && closeOpen.Indices.Length == 2)
        {
            EmitConstant(0.0); // fileNum 0
            EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.CLOSE_FILE); EmitByte((byte)1);
        }

        if (stmt.ResourceTarget is VariableTarget vt)
        {
            var name = vt.NameCleaned.Lexeme;
            var localIdx = ResolveLocal(name);
            if (localIdx != -1)
            {
                EmitByte((byte)OpCode.OP_FINALIZE_SCOPE);
                EmitShort((ushort)localIdx);
                // Evita la doppia finalizzazione quando EndScope/EmitFinalizeBodyLocals
                // raggiungeranno più tardi lo stesso local (blocco annidato o fine SUB/FUNCTION).
                _locals[localIdx].AlreadyFinalized = true;
            }
            else if (TryResolveOrRegisterGlobal(name, out var globalIndex))
            {
                EmitByte((byte)OpCode.OP_FINALIZE_GLOBAL);
                EmitShort(globalIndex);
            }
        }
    }

    /// <summary>Emette l'apertura di file per `USING F = OPEN(path, MODE)` (forma funzione):
    /// stessa sequenza dello statement `OPEN path FOR MODE AS #0` (OP_FILE_CMD/OPEN, 9 argomenti)
    /// con fileNum = 0. La modalità è un TOKEN (`OUTPUT`/`INPUT`/...) mappato all'enum.</summary>
    private void EmitUsingFunctionOpen(Expr pathExpr, Expr modeExpr)
    {
        VisitExpression(pathExpr);
        EmitConstant((double)UsingOpenModeFromToken(modeExpr)); // Mode (FileOpenMode)
        EmitConstant(0.0);                                       // LockMode Default
        EmitConstant(0.0);                                       // FileNumber = 0
        EmitByte((byte)OpCode.OP_PUSH_NIL);                      // RecordLength
        EmitConstant(0.0);                                       // ContentType Default
        EmitConstant(0.0);                                       // KeyCount
        EmitConstant(0.0);                                       // IndexCount
        EmitConstant(0.0);                                       // Unsafe
        EmitBytes((byte)OpCode.OP_FILE_CMD, (byte)FileSubOpCode.OPEN, (byte)9);
    }

    /// <summary>Mappa il token di modalità di `OPEN(path, MODE)` (OUTPUT/WRITE, INPUT/READ,
    /// APPEND, RANDOM, BINARY) all'enum `FileOpenMode`. Default: INPUT.</summary>
    private static FileOpenMode UsingOpenModeFromToken(Expr modeExpr)
    {
        var name = (modeExpr as VariableExpr)?.NameCleaned.Lexeme.ToUpperInvariant();
        return name switch
        {
            "OUTPUT" or "WRITE" or "W" => FileOpenMode.Output,
            "APPEND" or "A" => FileOpenMode.Append,
            "RANDOM" => FileOpenMode.Random,
            "BINARY" => FileOpenMode.Binary,
            _ => FileOpenMode.Input, // INPUT / READ / R
        };
    }

    /// <summary>Compila MMAPPOKE handle, offset, value — syscall MEMORYMAP_POKE.</summary>
    private void EmitMemoryPokeStatement(MemoryMapPokeStmt stmt)
    {
        VisitExpression(stmt.Handle);
        VisitExpression(stmt.Offset);
        VisitExpression(stmt.Value);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.MEMORYMAP_POKE); EmitByte((byte)3);
    }

    /// <summary>Compila MMAPWRITEBYTES handle, offset, sourceArray [, count] — syscall MEMORYMAP_WRITEBYTES.</summary>
    private void EmitMemoryWriteBytesStatement(MemoryMapWriteBytesStmt stmt)
    {
        VisitExpression(stmt.Handle);
        VisitExpression(stmt.Offset);
        VisitExpression(stmt.SourceArray);
        if (stmt.Count is not null)
        {
            VisitExpression(stmt.Count);
        }
        else
        {
            EmitConstant(-1.0); // -1 = scrivi l'intero array
        }
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.MEMORYMAP_WRITEBYTES); EmitByte((byte)4);
    }

    // ── CONSOLE (Caption/SendKey/ScrollMode) ────────────────────────────────

    // ── AUDIO (file/volume — syscall dedicati) ────────────────────────────────

    /// <summary>Compila SOUND LOAD "file" AS #handle — syscall SOUND_LOAD (pusha l'handle).</summary>
    private void EmitSoundLoadStatement(SoundLoadStmt stmt)
    {
        VisitExpression(stmt.FilePath);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.SOUND_LOAD); EmitByte((byte)1);
        AssignVariable(stmt.Handle, false);
    }

    /// <summary>Compila SOUND PLAY #handle [, channel [, loops]] — syscall SOUND_PLAY_FILE.</summary>
    private void EmitSoundPlayFileStatement(SoundPlayStmt stmt)
    {
        VisitExpression(stmt.Handle);
        if (stmt.Channel is not null) VisitExpression(stmt.Channel); else EmitByte((byte)OpCode.OP_PUSH_NIL);
        if (stmt.Loops is not null) VisitExpression(stmt.Loops); else EmitByte((byte)OpCode.OP_PUSH_NIL);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.SOUND_PLAY_FILE); EmitByte((byte)3);
    }

    /// <summary>Compila SOUND VOLUME vol [, channel] — syscall SOUND_VOLUME.</summary>
    private void EmitSoundVolumeStatement(SoundVolumeStmt stmt)
    {
        VisitExpression(stmt.Volume);
        if (stmt.Channel is not null) VisitExpression(stmt.Channel); else EmitByte((byte)OpCode.OP_PUSH_NIL);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.SOUND_VOLUME); EmitByte((byte)2);
    }

    // ── CLIP / CURSOR / SAVE-RESTORE SCREEN ─────────────────────────────────────

    private void EmitClipStatement(ClipStmt stmt)
    {
        // CLIP GET/SET/CLEAR — OP_CLIP_CMD con il sub-opcode e gli argomenti.
        // L'argomento va pushato PRIMA dell'opcode (era emesso dopo: la syscall
        // poppava a stack vuoto e l'argomento restava orfano — SET non memorizzava).
        // GET pusha il testo letto: va assegnato a Destination (mancava del tutto —
        // il valore restava orfano sullo stack e la variabile restava vuota).
        if (stmt.Argument is not null)
            VisitExpression(stmt.Argument);
        var sub = stmt.Command.Kind switch
        {
            TokenKind.Get => (byte)0,
            TokenKind.Set => (byte)1,
            _ => (byte)2 // CLEAR
        };
        EmitBytes((byte)OpCode.OP_CLIP_CMD, sub, (byte)(stmt.Argument is not null ? 1 : 0));
        if (stmt.Destination is not null)
            AssignVariable(stmt.Destination, isLet: false);
    }

    private void EmitCursorStateStatement(CursorStateStmt stmt)
    {
        EmitConstant(stmt.State.Kind == TokenKind.On ? 1.0 : 0.0);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.CURSOR_STATE); EmitByte((byte)1);
    }

    private void EmitCursorShapeStatement(CursorShapeStmt stmt)
    {
        VisitExpression(stmt.Shape);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.CURSOR_SHAPE); EmitByte((byte)1);
    }

    private void EmitSaveScreenStatement(SaveScreenStmt stmt)
    {
        VisitExpression(stmt.Name);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.SAVE_SCREEN); EmitByte((byte)1);
    }

    private void EmitRestoreScreenStatement(RestoreScreenStmt stmt)
    {
        VisitExpression(stmt.Name);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.RESTORE_SCREEN); EmitByte((byte)1);
    }

    private void EmitPaletteSave(PaletteSaveStmt stmt)
    {
        VisitExpression(stmt.FileName);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.PALETTE_SAVE); EmitByte((byte)1);
    }

    private void EmitPaletteLoad(PaletteLoadStmt stmt)
    {
        VisitExpression(stmt.FileName);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.PALETTE_LOAD); EmitByte((byte)1);
    }

    private void EmitCSaveScreen(CSaveScreenStmt stmt)
    {
        VisitExpression(stmt.Name);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.SAVE_SCREEN); EmitByte((byte)1);
    }

    private void EmitCRestoreScreen(CRestoreScreenStmt stmt)
    {
        VisitExpression(stmt.Name);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.RESTORE_SCREEN); EmitByte((byte)1);
    }

    private void EmitViewPrintStatement(ViewPrintStmt stmt)
    {
        // VIEW PRINT [top, bottom] — scroll region della console; senza argomenti = reset.
        if (stmt.TopLine is not null) VisitExpression(stmt.TopLine); else EmitByte((byte)OpCode.OP_PUSH_NIL);
        if (stmt.BottomLine is not null) VisitExpression(stmt.BottomLine); else EmitByte((byte)OpCode.OP_PUSH_NIL);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.VIEW_PRINT); EmitByte((byte)2);
    }

    // ── SCREENSET / DRAW / DRAW TEXT (grafici) ───────────────────────────────────

    private void EmitScreenSetStatement(ScreenSetStmt stmt)
    {
        if (stmt.ActivePage is not null) VisitExpression(stmt.ActivePage); else EmitByte((byte)OpCode.OP_PUSH_NIL);
        if (stmt.VisiblePage is not null) VisitExpression(stmt.VisiblePage); else EmitByte((byte)OpCode.OP_PUSH_NIL);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.SCREENSET); EmitByte((byte)2);
    }

    private void EmitPixelBufferStatement(PixelBufferStmt stmt)
    {
        VisitExpression(stmt.Width); VisitExpression(stmt.Height); VisitExpression(stmt.Buffer);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.PIXEL_BUFFER); EmitByte((byte)3);
    }

    private void EmitDrawStatement(DrawStmt stmt)
    {
        VisitExpression(stmt.CommandString);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.DRAW); EmitByte((byte)1);
    }

    private void EmitDrawTextStatement(DrawTextStmt stmt)
    {
        VisitExpression(stmt.Text); VisitExpression(stmt.X); VisitExpression(stmt.Y);
        if (stmt.Rotation is not null) VisitExpression(stmt.Rotation); else EmitByte((byte)OpCode.OP_PUSH_NIL);
        EmitConstant((double)(int)stmt.Align);
        if (stmt.Color is not null) VisitExpression(stmt.Color); else EmitByte((byte)OpCode.OP_PUSH_NIL);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.DRAW_TEXT); EmitByte((byte)6);
    }

    // ── WINDOW (viewport grafico + azioni legacy per nome) ────────────────────────

    /// <summary>Compila WINDOW [SCREEN] (x1,y1)-(x2,y2) — syscall WINDOW_VIEWPORT.
    /// Senza argomenti (WINDOW) ripristina la finestra di default.</summary>
    private void EmitWindowViewportStatement(ViewPortStmt stmt)
    {
        // WINDOW [SCREEN] senza coordinate → ViewPortStmt(_, 0,0,0,0): la window degenerata
        // (0,0)-(0,0) con ww=0→1 rende px = wx*viewportWidth (fuori schermo → clip).
        // Trattiamo il caso come reset.
        if (stmt.X1 is NumberExpr n1 && n1.Value == 0 &&
            stmt.Y1 is NumberExpr y1 && y1.Value == 0 &&
            stmt.X2 is NumberExpr n2 && n2.Value == 0 &&
            stmt.Y2 is NumberExpr y2 && y2.Value == 0)
        {
            EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.WINDOW_VIEWPORT); EmitByte((byte)0);
            return;
        }
        if (stmt.X1 is not null && stmt.Y1 is not null && stmt.X2 is not null && stmt.Y2 is not null)
        {
            VisitExpression(stmt.X1); VisitExpression(stmt.Y1);
            VisitExpression(stmt.X2); VisitExpression(stmt.Y2);
            EmitConstant(stmt.Screen ? 1.0 : 0.0);
            EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.WINDOW_VIEWPORT); EmitByte((byte)5);
        }
        else
        {
            // WINDOW senza argomenti = reset: NESSUN push → il case WINDOW_VIEWPORT
            // vede lo stack vuoto e fa ResetViewportAndWindow. Il PUSH_NIL precedente
            // faceva cadere nel ramo SetWindow(0,0,0,0,false) → _windowActive=true con
            // coordinate degenerate → tutti i disegni successivi fuori schermo.
            EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.WINDOW_VIEWPORT); EmitByte((byte)0);
        }
    }

    /// <summary>Compila WINDOW ACTIVATE/DEACTIVATE/RELEASE name (sintassi legacy) —
    /// syscall WINDOW_ACTION. Ordine argomenti come il TUI: action (numero) POI name.</summary>
    private void EmitWindowActionLegacy(WindowAction action, Token name)
    {
        EmitConstant((double)(int)action);
        EmitConstant(name.Lexeme);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.WINDOW_ACTION); EmitByte((byte)2);
    }

    // ── PALETTE (console) ─────────────────────────────────────────────────────────

    private void EmitPaletteStatement(PaletteStmt stmt)
    {
        VisitExpression(stmt.Index); VisitExpression(stmt.R); VisitExpression(stmt.G); VisitExpression(stmt.B);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.PALETTE_SET); EmitByte((byte)4);
    }

    private void EmitPaletteUseStatement(PaletteUseStmt stmt)
    {
        VisitExpression(stmt.Name);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.PALETTE_USE); EmitByte((byte)1);
    }

    private void EmitPaletteDefineStatement(PaletteDefineStmt stmt)
    {
        VisitExpression(stmt.Name); VisitExpression(stmt.Colors);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.PALETTE_DEFINE); EmitByte((byte)2);
    }

    private void EmitPaletteReplaceStatement(PaletteReplaceStmt stmt)
    {
        VisitExpression(stmt.Name); VisitExpression(stmt.Colors);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.PALETTE_REPLACE); EmitByte((byte)2);
    }

    // ── FILE LOCK/UNLOCK ──────────────────────────────────────────────────────────

    private void EmitFileLockStatement(LockStmt stmt)
    {
        VisitExpression(stmt.FileNumber);
        if (stmt.RecordNumber is not null) VisitExpression(stmt.RecordNumber); else EmitByte((byte)OpCode.OP_PUSH_NIL);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.FILE_LOCK); EmitByte((byte)2);
    }

    private void EmitFileUnlockStatement(UnlockStmt stmt)
    {
        VisitExpression(stmt.FileNumber);
        if (stmt.RecordNumber is not null) VisitExpression(stmt.RecordNumber); else EmitByte((byte)OpCode.OP_PUSH_NIL);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.FILE_UNLOCK); EmitByte((byte)2);
    }

    // ── EVENTI generici ───────────────────────────────────────────────────────────

    private void EmitRaiseEventStatement(RaiseEventStmt stmt)
    {
        EmitConstant(stmt.EventName.Lexeme);
        foreach (var a in stmt.Arguments) VisitExpression(a);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.RAISE_EVENT); EmitByte((byte)(stmt.Arguments.Length + 1));
    }

    private void EmitAddHandlerStatement(AddHandlerStmt stmt)
    {
        // ADD HANDLER obj.Event, MyHandler → registra (nomeEvento, nomeProcedura).
        var evName = ResolveEventName(stmt.EventAccess);
        var hName = ResolveHandlerName(stmt.HandlerExpr);
        if (evName is null || hName is null)
            throw new NotSupportedException(
                "Backend VM: ADD HANDLER richiede un evento membro (obj.Event) e un handler procedure per nome.");
        EmitConstant(evName); EmitConstant(hName);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.ADD_HANDLER); EmitByte((byte)2);
    }

    private void EmitRemoveHandlerStatement(RemoveHandlerStmt stmt)
    {
        var evName = ResolveEventName(stmt.EventAccess);
        var hName = ResolveHandlerName(stmt.HandlerExpr);
        if (evName is null || hName is null)
            throw new NotSupportedException(
                "Backend VM: REMOVE HANDLER richiede un evento membro (obj.Event) e un handler procedure per nome.");
        EmitConstant(evName); EmitConstant(hName);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.REMOVE_HANDLER); EmitByte((byte)2);
    }

    /// <summary>Estrae il nome dell'evento da obj.Event (FieldAccessExpr) o da un evento
    /// GLOBALE nudo (VariableExpr: `ADD HANDLER before_task, Handler`).</summary>
    private static string? ResolveEventName(Expr e) => e switch
    {
        FieldAccessExpr fa => fa.Field.Lexeme,
        VariableExpr ve => ve.NameCleaned.Lexeme,
        _ => null
    };

    /// <summary>Estrae il nome della procedura handler (FunctionExpr o VariableExpr).</summary>
    private static string? ResolveHandlerName(Expr e) => e switch
    {
        FunctionExpr fe => fe.OriginalToken.Lexeme,
        FunctionReferenceExpr fre => fre.OriginalToken.Lexeme,
        VariableExpr ve => ve.NameCleaned.Lexeme,
        _ => null
    };

    // ── IO WATCH (FileSystemWatcher — ancora da cablare) ──────────────────────────

    private void EmitIoWatchStatement(IoWatchStmt stmt)
    {
        VisitExpression(stmt.Path);
        VisitExpression(stmt.EventsMask);
        /* Il nome della SUB callback va passato al runtime: senza, il watcher
         * registra la path ma non puo' dispattare l'evento (era un registro inerte). */
        EmitConstant(stmt.Callback.Lexeme);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.IO_WATCH); EmitByte((byte)3);
    }

    private void EmitIoUnwatchStatement(IoUnwatchStmt stmt)
    {
        VisitExpression(stmt.Path);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.IO_UNWATCH); EmitByte((byte)1);
    }

    // ── CHART (plotting headless — syscall → GraphicsManager OxyPlot) ────────────

    private void EmitChartPlotStatement(ChartPlotStmt stmt)
    {
        foreach (var a in stmt.Args) VisitExpression(a);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.CHART_PLOT); EmitByte((byte)stmt.Args.Length);
    }

    private void EmitChartFinancialStatement(ChartFinancialStmt stmt)
    {
        foreach (var a in stmt.Args) VisitExpression(a);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.CHART_FINANCIAL); EmitByte((byte)stmt.Args.Length);
    }

    private void EmitChartClearStatement(ChartClearStmt stmt)
    {
        VisitExpression(stmt.Handle);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.CHART_CLEAR); EmitByte((byte)1);
    }

    private void EmitChartCloseStatement(ChartCloseStmt stmt)
    {
        VisitExpression(stmt.Handle);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.CHART_CLOSE); EmitByte((byte)1);
    }

    private void EmitChartRenderStatement(ChartRenderStmt stmt)
    {
        EmitConstant(stmt.IsSave ? 1.0 : 0.0);
        foreach (var a in stmt.Args) VisitExpression(a);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.CHART_RENDER); EmitByte((byte)(stmt.Args.Length + 1));
    }

    // ── MOUSE (config — syscall → GraphicsManager SDL reale) ───────────────────

    /// <summary>Compila MOUSE ENABLE / MOUSE.ENABLE(flag) — syscall MOUSE_ENABLE.</summary>
    private void EmitMouseEnableStatement(MouseConfigStmt stmt)
    {
        EmitConstant(stmt.Enabled ? 1.0 : 0.0);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.MOUSE_ENABLE); EmitByte((byte)1);
    }

    /// <summary>Compila MOUSE SHOW/HIDE — syscall MOUSE_SHOW.</summary>
    private void EmitMouseShowStatement(MouseVisibilityStmt stmt)
    {
        EmitConstant(stmt.Visible ? 1.0 : 0.0);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.MOUSE_SHOW); EmitByte((byte)1);
    }

    /// <summary>Compila MOUSE SETPOS x, y — syscall MOUSE_SETPOS.</summary>
    private void EmitMouseSetPosStatement(MouseSetStmt stmt)
    {
        VisitExpression(stmt.X);
        VisitExpression(stmt.Y);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.MOUSE_SETPOS); EmitByte((byte)2);
    }

    /// <summary>Compila MOUSE CURSOR id — syscall MOUSE_CURSOR.</summary>
    private void EmitMouseCursorStatement(MouseCursorStmt stmt)
    {
        VisitExpression(stmt.CursorId);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.MOUSE_CURSOR); EmitByte((byte)1);
    }

    /// <summary>Compila MOUSE LIMIT [x1, y1, x2, y2] — syscall MOUSE_LIMIT (4 arg, NIL se omessi).</summary>
    private void EmitMouseLimitStatement(MouseLimitStmt stmt)
    {
        if (stmt.X1 is not null) VisitExpression(stmt.X1); else EmitByte((byte)OpCode.OP_PUSH_NIL);
        if (stmt.Y1 is not null) VisitExpression(stmt.Y1); else EmitByte((byte)OpCode.OP_PUSH_NIL);
        if (stmt.X2 is not null) VisitExpression(stmt.X2); else EmitByte((byte)OpCode.OP_PUSH_NIL);
        if (stmt.Y2 is not null) VisitExpression(stmt.Y2); else EmitByte((byte)OpCode.OP_PUSH_NIL);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.MOUSE_LIMIT); EmitByte((byte)4);
    }

    /// <summary>Compila MOUSE WAIT — syscall MOUSE_WAIT (sync point).</summary>
    private void EmitMouseWaitStatement(MouseWaitStmt stmt)
    {
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.MOUSE_WAIT); EmitByte((byte)0);
    }

    /// <summary>Compila CONSOLE.CAPTION / CAPTION "titolo" — syscall CONSOLE_CAPTION.</summary>
    private void EmitConsoleCaptionStatement(ConsoleCaptionStmt stmt)
    {
        VisitExpression(stmt.TitleExpression);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.CONSOLE_CAPTION); EmitByte((byte)1);
    }

    /// <summary>Compila CONSOLE.SENDKEY / SENDKEY "keys" — syscall CONSOLE_SENDKEY.</summary>
    private void EmitConsoleSendKeyStatement(ConsoleSendKeyStmt stmt)
    {
        VisitExpression(stmt.KeysExpression);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.CONSOLE_SENDKEY); EmitByte((byte)1);
    }

    /// <summary>Compila CONSOLE.SCROLLMODE / SCROLLMODE mode — syscall CONSOLE_SCROLLMODE
    /// (0 = disabilita, != 0 = abilita una regione 0..25, come l'IL).</summary>
    private void EmitConsoleScrollModeStatement(ConsoleScrollModeStmt stmt)
    {
        VisitExpression(stmt.ModeExpression);
        EmitByte((byte)OpCode.OP_SYSCALL); EmitSysCall(SysCall.CONSOLE_SCROLLMODE); EmitByte((byte)1);
    }
}
