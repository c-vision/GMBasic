// FILE: IlBackendCodegen/Partial/IlProgramCompiler.Builtins.cs
// PARTIAL — dominio BUILTIN FUNCTIONS: EmitBuiltinFunctionCall (il dispatch builtin,
// switch TokenKind + namespace). Migrato da IlProgramCompiler.cs (2026-08-30 — split,
// file <= 1200 righe). Core dispatcher (eccezione §0quater).

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

/// <summary>Partial del transpiler IL: dispatch builtin function.</summary>
public sealed partial class IlProgramCompiler
{
    /// <summary>Chiamata a un metodo runtime che riceve TUTTI gli argomenti BASIC come un
    /// singolo <c>object[]</c> (DBF.LOOKUP, replicazione SHIP/RECV di DBF e ISAM): costruisce
    /// l'array boxando i value type. Prima questi case usavano EmitTypedRuntimeCall, che itera
    /// i parametri del METODO (uno solo: object[]) e quindi lanciava
    /// "accetta 1 argomenti, ricevuti N" appena si passava l'arità corretta della StdLib
    /// (§ MiniTests/dbf/dbf_lock_filter.bas: LOOKUP ha 4 argomenti).</summary>
    private void EmitVariadicRuntimeCall(BuiltinFunctionCallExpr expr, string methodName)
    {
        var method = RuntimeMethod(methodName, typeof(object[]));
        _il.Emit(OpCodes.Ldc_I4, expr.Args.Length);
        _il.Emit(OpCodes.Newarr, typeof(object));
        for (int i = 0; i < expr.Args.Length; i++)
        {
            _il.Emit(OpCodes.Dup);
            _il.Emit(OpCodes.Ldc_I4, i);
            EmitExpression(expr.Args[i]);
            var clr = DataTypeMapper.Map(NormalizeInferredType(InferType(expr.Args[i])), _types, _classTypes);
            if (clr.IsValueType) _il.Emit(OpCodes.Box, clr);
            _il.Emit(OpCodes.Stelem_Ref);
        }
        _il.Emit(OpCodes.Call, method);
    }

    /// <summary>Argomento `handle` OPZIONALE dei polling CONTROLS (CLICKED/CHANGED/…):
    /// 0 argomenti → 0.0 (parità PCode/VM-C/C99Native).</summary>
    private void EmitFormHandleArg(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length > 0) EmitExpression(expr.Args[0]);
        else _il.Emit(OpCodes.Ldc_R8, 0.0);
    }

    /// <summary>SPEED.VALIDATE: 4 overload registrati — `()`, `(alias$)`, `(full)`,
    /// `(alias$, full)`. Il runtime espone `SpeedValidate(string? alias)` (-1 valido /
    /// 0 no): l'alias è il primo argomento STRING, il flag `full` viene valutato (effetti)
    /// ma ignorato (l'integrità è sempre completa). Prima si emetteva sempre un cast a
    /// string sul primo argomento → ILGEN "conversione numerica da Double a String".</summary>
    private void EmitSpeedValidateCall(BuiltinFunctionCallExpr expr)
    {
        var aliasArg = expr.Args.FirstOrDefault(a => InferType(a) is StringType);
        foreach (var a in expr.Args)
        {
            if (ReferenceEquals(a, aliasArg)) continue;
            EmitExpression(a);
            _il.Emit(OpCodes.Pop);   /* flag `full` (o argomento non-stringa): nessun uso */
        }
        if (aliasArg is not null) EmitExpression(aliasArg);
        else _il.Emit(OpCodes.Ldnull);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.SpeedValidate), typeof(string)));
    }

    private void EmitBuiltinFunctionCall(BuiltinFunctionCallExpr expr)
    {
        // FORM_* (GUI): controlli pixel — per nome (nessun TokenKind).
        var _formName = expr.Name.Lexeme?.ToUpperInvariant();
        if (_formName == "WINDOWCLOSED") { _il.Emit(OpCodes.Call, IlRuntimeFormClosedMethod); return; }
        switch (_formName)
        {
            /* CLICKED/CHANGED/…: handle OPZIONALE (StdLib.Controls: optFlag, registry arity
             * 0..1). Con 0 argomenti il default è 0 — prima `expr.Args[0]` andava in
             * IndexOutOfRange (ILGEN) mentre PCode/VM-C restituivano 0. */
            case "CLICKED": EmitFormHandleArg(expr); _il.Emit(OpCodes.Call, IlRuntimeFormClickedMethod); return;
            case "CTRLGETPROP": EmitExpression(expr.Args[0]); EmitExpression(expr.Args[1]); _il.Emit(OpCodes.Call, IlRuntimeFormControlGetMethod); return;
            case "CTRLGETPROPS": EmitExpression(expr.Args[0]); EmitExpression(expr.Args[1]); _il.Emit(OpCodes.Call, IlRuntimeFormControlGetStrMethod); return;
            case "DOUBLECLICKED": EmitFormHandleArg(expr); _il.Emit(OpCodes.Call, IlRuntimeFormDblClickedMethod); return;
            case "CHANGED": EmitFormHandleArg(expr); _il.Emit(OpCodes.Call, IlRuntimeFormChangedMethod); return;
            case "GOTFOCUSED": EmitFormHandleArg(expr); _il.Emit(OpCodes.Call, IlRuntimeFormGotFocusedMethod); return;
            case "LOSTFOCUSED": EmitFormHandleArg(expr); _il.Emit(OpCodes.Call, IlRuntimeFormLostFocusedMethod); return;
            case "MOUSECLICKED": _il.Emit(OpCodes.Call, IlRuntimeMouseClickedMethod); return;
            case "MOUSEDBLCLICKED": _il.Emit(OpCodes.Call, IlRuntimeMouseDblClickedMethod); return;
            case "MOUSEPRESSED": _il.Emit(OpCodes.Call, IlRuntimeMousePressedMethod); return;
            case "KEYPRESSED": _il.Emit(OpCodes.Call, IlRuntimeKeyPressedMethod); return;
        }

        // BITS (2026-09-16): namespace dedicato → file di dominio IlProgramCompiler.Bits.cs.
        if (TryEmitBitsCall(expr)) return;
        if (TryEmitTrayCall(expr)) return;

        // Moduli IL esterni caricati con IlModuleLoader (uso generico, non solo PackageManager):
        // `NS.METHOD(...)` → call diretta al MethodInfo statico registrato.
        if (expr.Namespace != null
            && IlModuleLoader.TryGet(expr.Namespace.Lexeme, expr.Name.Lexeme, out var extMethod))
        {
            EmitExternalCall(expr, extMethod);
            return;
        }

        // LEDGER (pattern DBF/ISAM): i TokenKind sono condivisi con DBF/dict/generici —
        // senza il check namespace, LEDGER.EOF(h) andrebbe a DbfEof, LEDGER.GET(h) a
        // DictGet, LEDGER.COUNT(h) a Count generico, ecc. OPEN resta gestito qui dentro
        // (LEDGER.OPEN(path)).
        if (expr.Namespace != null
            && expr.Namespace.Lexeme.Equals("LEDGER", StringComparison.OrdinalIgnoreCase))
        {
            EmitLedgerFunctionCall(expr);
            return;
        }
        switch (expr.Name.Kind)
        {
            case TokenKind.Len: EmitLenCall(expr); break;
            case TokenKind.LenB: EmitLenBCall(expr); break;
            case TokenKind.Left: EmitStringUtilsCall(expr, StringUtilsLeftMethod, 2); break;
            case TokenKind.Right: EmitStringUtilsCall(expr, StringUtilsRightMethod, 2); break;
            case TokenKind.Mid: EmitMidCall(expr); break;
            case TokenKind.Instr: EmitInstrCall(expr); break;
            case TokenKind.Lower: EmitStringInstanceCall(expr, _types.StringToLowerInvariant); break;
            case TokenKind.Upper: EmitStringInstanceCall(expr, _types.StringToUpperInvariant); break;
            case TokenKind.Trim: EmitStringUnaryBuiltinCall(expr, IlRuntimeStringTrimMethod); break;
            case TokenKind.LTrim: EmitStringUnaryBuiltinCall(expr, IlRuntimeStringTrimStartMethod); break;
            case TokenKind.RTrim: EmitStringUnaryBuiltinCall(expr, IlRuntimeStringTrimEndMethod); break;
            case TokenKind.CLng: EmitNumericConversionCall(expr, _types.ConvertToInt64Double); break;
            // CINT produce un intero a 64 bit (INTEGER): con Convert.ToInt16(double) la gamba
            // IL era l'UNICA a lanciare OverflowException su valori legittimi (70000) e a
            // troncare LEN(...) di un corpo >64 KB; le gambe PCode/VM-C/C99Native restituiscono
            // il valore intero (arrotondato all'intero più vicino, 2.5→2 come Convert.ToInt64).
            case TokenKind.CInt: EmitNumericConversionCall(expr, _types.ConvertToInt64Double); break;
            case TokenKind.CCur: EmitNumericConversionTo(expr, _types.Decimal); break;
            case TokenKind.CDbl: EmitNumericConversionTo(expr, _types.Double); break;
            case TokenKind.CSng: EmitNumericConversionTo(expr, _types.Double); break;
            case TokenKind.CByte: EmitNumericConversionTo(expr, _types.Byte); break;
            case TokenKind.Asc: EmitStringUnaryBuiltinCall(expr, StringHelpersAscMethod); break;
            case TokenKind.Val: EmitStringUnaryBuiltinCall(expr, StringHelpersValMethod); break;
            case TokenKind.Chr: EmitChrCall(expr); break;
            case TokenKind.Str: EmitDoubleBuiltinCall(expr, StringHelpersStrMethod); break;
            case TokenKind.Stringify: EmitStringifyCall(expr); break;
            case TokenKind.ObjectToJson: EmitStringifyCall(expr); break;
            case TokenKind.Alert: EmitAlertCall(expr); break;
            case TokenKind.Confirm: EmitConfirmCall(expr); break;
            case TokenKind.Iif: EmitIifCall(expr); break;
            case TokenKind.Choose: EmitChooseCall(expr); break;
            case TokenKind.ValType: EmitValTypeCall(expr); break;
            // LOCALPORT/PORT/REMOTE (handle di rete) — prima "funzione builtin non ancora supportata".
            case TokenKind.LocalPort: EmitObjectArgCall(expr, IlRuntimeLocalPortMethod); break;
            case TokenKind.MutexTryLock: EmitObjectArgCall(expr, IlRuntimeMutexTryLockMethod); break;
            // MESSAGE.DELIVERY(bus) — contatori di consegna del bus (handle numerico su IL).
            case TokenKind.MessageDelivery: EmitTypedRuntimeCall(expr, IlRuntimeMessageDeliveryMethod); break;
            case TokenKind.ContextBackground: EmitNoArgBuiltinCall(expr, IlRuntimeContextBackgroundMethod); break;
            case TokenKind.ContextWithCancel: EmitObjectArgCall(expr, IlRuntimeContextWithCancelMethod); break;
            case TokenKind.ContextCancel: EmitObjectArgCall(expr, IlRuntimeContextCancelMethod); break;
            case TokenKind.ContextDone: EmitObjectArgCall(expr, IlRuntimeContextDoneMethod); break;
            case TokenKind.ContextCancelled: EmitObjectArgCall(expr, IlRuntimeContextCancelledMethod); break;
            case TokenKind.RemotePort: EmitObjectArgCall(expr, IlRuntimePortMethod); break;
            case TokenKind.RemoteIp: EmitObjectArgCall(expr, IlRuntimeRemoteMethod); break;
            case TokenKind.Tab: EmitTabCall(expr); break;
            case TokenKind.Seconds: EmitNoArgBuiltinCall(expr, IlRuntimeSecondsMethod); break;
            case TokenKind.Cdow: EmitDateArgBuiltinCall(expr, IlRuntimeCdowMethod); break;
            case TokenKind.Dow: EmitDateArgBuiltinCall(expr, IlRuntimeDowMethod); break;
            case TokenKind.Dtoc: EmitDateArgBuiltinCall(expr, IlRuntimeDtocMethod); break;
            case TokenKind.Compress: EmitCompressionCall(expr, IlCompressionCompressMethod, null); break;
            case TokenKind.Decompress: EmitCompressionCall(expr, IlCompressionDecompressMethod, null); break;
            case TokenKind.Gzip: EmitCompressionCall(expr, IlCompressionCompressMethod, "GZIP"); break;
            case TokenKind.Ungzip: EmitCompressionCall(expr, IlCompressionDecompressMethod, "GZIP"); break;
            case TokenKind.Brotli: EmitCompressionCall(expr, IlCompressionCompressMethod, "BROTLI"); break;
            case TokenKind.Unbrotli: EmitCompressionCall(expr, IlCompressionDecompressMethod, "BROTLI"); break;
            case TokenKind.Zlib: EmitCompressionCall(expr, IlCompressionCompressMethod, "ZLIB"); break;
            case TokenKind.Unzlib: EmitCompressionCall(expr, IlCompressionDecompressMethod, "ZLIB"); break;
            case TokenKind.Bzip2: EmitCompressionCall(expr, IlCompressionCompressMethod, "BZIP2"); break;
            case TokenKind.Unbzip2: EmitCompressionCall(expr, IlCompressionDecompressMethod, "BZIP2"); break;
            case TokenKind.Deflate: EmitCompressionCall(expr, IlCompressionCompressMethod, "DEFLATE"); break;
            case TokenKind.Undeflate: EmitCompressionCall(expr, IlCompressionDecompressMethod, "DEFLATE"); break;
            case TokenKind.Zstd: EmitCompressionCall(expr, IlCompressionCompressMethod, "ZSTD"); break;
            case TokenKind.Unzstd: EmitCompressionCall(expr, IlCompressionDecompressMethod, "ZSTD"); break;
            case TokenKind.Lz4: EmitCompressionCall(expr, IlCompressionCompressMethod, "LZ4"); break;
            case TokenKind.Unlz4: EmitCompressionCall(expr, IlCompressionDecompressMethod, "LZ4"); break;
            case TokenKind.Xz: EmitCompressionCall(expr, IlCompressionCompressMethod, "XZ"); break;
            case TokenKind.Unxz: EmitCompressionCall(expr, IlCompressionDecompressMethod, "XZ"); break;
            // GREP.* — delega al motore C condiviso via P/Invoke (NativeGrep, sorgente unica).
            case TokenKind.GrepMatches: EmitStringBinaryBuiltinCall(expr, IlGrepMatchesMethod); break;
            case TokenKind.GrepFiles: EmitStringBinaryBuiltinCall(expr, IlGrepFilesMethod); break;
            case TokenKind.GrepCount: EmitTypedRuntimeCall(expr, IlGrepCountMethod); break;
            case TokenKind.Zip: EmitZipCall(expr, isZip: true); break;
            case TokenKind.Unzip: EmitZipCall(expr, isZip: false); break;
            case TokenKind.Algorithms: EmitNoArgBuiltinCall(expr, IlRuntimeAlgorithmsMethod); break;
            case TokenKind.Host: EmitNoArgBuiltinCall(expr, IlRuntimeHostMethod); break;
            case TokenKind.GetTempDirFunc: EmitNoArgBuiltinCall(expr, IlRuntimeTempDirMethod); break;
            case TokenKind.GetHostIp: EmitStringUnaryBuiltinCall(expr, IlRuntimeGetHostIpMethod); break;
            case TokenKind.Peek: EmitPeekCall(expr); break;
            case TokenKind.Petscii: EmitStringUnaryBuiltinCall(expr, IlRuntimePetsciiMethod); break;
            case TokenKind.Screens: EmitNoArgBuiltinCall(expr, IlRuntimeScreensMethod); break;
            case TokenKind.ScreenInfo: EmitNoArgBuiltinCall(expr, IlRuntimeScreenInfoMethod); break;
            case TokenKind.Rgb: EmitRgbCall(expr); break;
            case TokenKind.Cell: EmitCellCall(expr); break;
            case TokenKind.CellStr: EmitCellCall(expr); break;
            case TokenKind.VarPtr:
            case TokenKind.StrPtr: EmitObjectArgCall(expr, IlRuntimeVarPtrMethod); break;
            case TokenKind.TypeInfo: EmitObjectArgCall(expr, IlRuntimeTypeInfoMethod); break;
            case TokenKind.TypeInfoHas: EmitObjectArgCall(expr, IlRuntimeTypeInfoHasMethod); break;
            case TokenKind.MethodInfo: EmitMethodInfoCall(expr, IlRuntimeMethodInfoMethod); break;
            case TokenKind.PropertyInfo: EmitMethodInfoCall(expr, IlRuntimePropertyInfoMethod); break;
            case TokenKind.UdtPacked: EmitObjectArgCall(expr, IlRuntimeUdtPackedMethod); break;
            case TokenKind.UdtAligned: EmitObjectArgCall(expr, IlRuntimeUdtAlignedMethod); break;
            case TokenKind.TextHeight: EmitNoArgBuiltinCall(expr, IlRuntimeTextHeightMethod); break;
            case TokenKind.TextWidth: EmitTextWidthCall(expr); break;
            case TokenKind.C2PX: EmitC2PCall(expr, IlRuntimeC2PXMethod, true); break;
            case TokenKind.C2PY: EmitC2PCall(expr, IlRuntimeC2PYMethod, false); break;
            case TokenKind.P2CX: EmitC2PCall(expr, IlRuntimeP2CXMethod, true); break;
            case TokenKind.P2CY: EmitC2PCall(expr, IlRuntimeP2CYMethod, false); break;
            case TokenKind.Cmonth: EmitDateArgBuiltinCall(expr, IlRuntimeCmonthMethod); break;
            case TokenKind.DateDiff: EmitDateDiffCall(expr); break;
            case TokenKind.CType when TryEmitUserDefinedCTypeCall(expr): break;
            // CTYPE generico (value, "TYPE") — coercizione runtime (parità PCodeProcessor.CTYPE).
            case TokenKind.CType: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.CTypeGeneric), typeof(object), typeof(string))); break;
            case TokenKind.CStr: EmitCStrCall(expr); break;
            case TokenKind.Hex: EmitInt64StringBuiltinCall(expr, StringHelpersHexMethod); break;
            case TokenKind.Oct: EmitInt64StringBuiltinCall(expr, StringHelpersOctMethod); break;
            case TokenKind.Bin: EmitInt64StringBuiltinCall(expr, StringHelpersBinMethod); break;
            case TokenKind.IsNumeric when NormalizeInferredType(InferType(expr.Args[0])) is AnyType:
                EmitObjectBoxedArgument(expr.Args[0]);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.IsNumericValue), typeof(object)));
                break;
            case TokenKind.IsNumeric: EmitStringUnaryBuiltinCall(expr, StringHelpersIsNumericMethod); break;
            case TokenKind.Space: EmitInt64StringBuiltinCall(expr, StringHelpersSpaceMethod); break;
            case TokenKind.Dup: EmitDupCall(expr); break;
            case TokenKind.Reverse: EmitStringUnaryBuiltinCall(expr, StringHelpersReverseMethod); break;
            case TokenKind.Pascal: EmitStringUnaryBuiltinCall(expr, StringHelpersPascalMethod); break;
            case TokenKind.Camel: EmitStringUnaryBuiltinCall(expr, StringHelpersCamelMethod); break;
            case TokenKind.Title: EmitStringUnaryBuiltinCall(expr, StringHelpersTitleMethod); break;
            case TokenKind.Sentence: EmitStringUnaryBuiltinCall(expr, StringHelpersSentenceMethod); break;
            case TokenKind.Replace: EmitStringTernaryBuiltinCall(expr, StringHelpersReplaceMethod); break;
            case TokenKind.Remove: EmitStringBinaryBuiltinCall(expr, StringHelpersRemoveMethod); break;
            case TokenKind.Contains: EmitStringBinaryBuiltinCall(expr, StringHelpersContainsMethod); break;
            case TokenKind.StartsWith: EmitStringBinaryBuiltinCall(expr, StringHelpersStartsWithMethod); break;
            case TokenKind.EndsWith: EmitStringBinaryBuiltinCall(expr, StringHelpersEndsWithMethod); break;
            case TokenKind.Snake: EmitStringUnaryBuiltinCall(expr, StringHelpersSnakeMethod); break;
            case TokenKind.Kebab: EmitStringUnaryBuiltinCall(expr, StringHelpersKebabMethod); break;
            case TokenKind.RepeatFunc: EmitStringRepeatCall(expr); break;
            case TokenKind.PadL: EmitStringPadCall(expr, StringHelpersPadLMethod); break;
            case TokenKind.PadR: EmitStringPadCall(expr, StringHelpersPadRMethod); break;
            case TokenKind.PadC: EmitStringPadCall(expr, StringHelpersPadCMethod); break;
            case TokenKind.Wrap: EmitStringWrapCall(expr); break;
            case TokenKind.Search: EmitStringBinaryBuiltinCall(expr, StringHelpersSearchMethod); break;
            case TokenKind.Split: EmitStringSplitCall(expr); break;
            case TokenKind.Join: EmitStringJoinCall(expr); break;
            case TokenKind.Format: EmitFormatCall(expr); break;
            case TokenKind.HtmlEncode: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.HtmlEncode))); break;
            case TokenKind.UrlEncode: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.UrlEncode))); break;
            case TokenKind.JsEncode: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.JsEncode))); break;
            case TokenKind.IsRequired: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.IsValidRequired))); break;
            case TokenKind.IsEmail: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.IsValidEmail))); break;
            case TokenKind.IsUrl: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.IsValidUrl))); break;
            case TokenKind.IsCreditCard: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.IsValidCreditCard))); break;
            case TokenKind.IsLength: EmitValidationLengthCall(expr); break;
            case TokenKind.JwtCreate: EmitStringBinaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.JwtCreate))); break;
            case TokenKind.JwtVerify: EmitStringBinaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.JwtVerify))); break;
            case TokenKind.JwtPayload: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.JwtPayload))); break;
            case TokenKind.SafeFileName: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.HttpSafeFileName))); break;
            case TokenKind.DetectContentType: EmitCryptoObjectUnaryCall(expr, nameof(IlRuntime.HttpDetectContentType)); break;
            case TokenKind.SessionCreate: EmitStringBinaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.HttpSessionCreate))); break;
            case TokenKind.Count when expr.Args.Length == 1 && TryGetNativeCollectionType(InferType(expr.Args[0]), out _): EmitNativeCollectionCountCall(expr); break;
            case TokenKind.UniqueCount: EmitObjectArgCall(expr, IlRuntimeUniqueCountMethod); break;
            case TokenKind.Fetch: EmitFetchFunctionCall(expr); break;
            case TokenKind.HttpGet: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.HttpGet))); break;
            case TokenKind.HttpDelete: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.HttpDelete))); break;
            case TokenKind.HttpPost: EmitHttpBodyCall(expr, nameof(IlRuntime.HttpPost)); break;
            case TokenKind.HttpPut: EmitHttpBodyCall(expr, nameof(IlRuntime.HttpPut)); break;
            case TokenKind.Count when expr.Args.Length == 1 && NormalizeInferredType(InferType(expr.Args[0])) is ArrayType: EmitArrayLengthCall(expr); break;
            case TokenKind.Count when expr.Args.Length == 1 && NormalizeInferredType(InferType(expr.Args[0])) is AnyType or TreeType or AVLTreeType
                or RedBlackTreeType or BinaryIndexedTreeType or TrieType or BPlusTreeType or GraphType
                or HashRingType or SparseSetType or BinaryHeapType or BagType or CacheType or TimelineType
                or RingBufferType: EmitDynamicCountCall(expr); break;
            // BAG.Count(item) (istanza+item): molteplicità — il fallback stringa sotto la
            // lancerebbe come StringHelpers.Count(string,string) → AccessViolation sul dict.
            case TokenKind.Count when expr.Args.Length >= 2 && IsDynamicDispatchInstance(expr.Args[0]):
                EmitObjectArgCollectionInvokeCall(expr, "Count");
                break;
            case TokenKind.Count: EmitStringBinaryBuiltinCall(expr, StringHelpersCountMethod); break;
            case TokenKind.Now: EmitNoArgBuiltinCall(expr, IlRuntimeNowMethod); break;
            case TokenKind.MemoryMap: EmitMemoryMapFunctionCall(expr); break;
            case TokenKind.MemoryMapPeek: EmitMemoryMapPeekCall(expr); break;
            case TokenKind.MemoryMapReadBytes: EmitMemoryMapReadBytesCall(expr); break;
            case TokenKind.Date: EmitNoArgBuiltinCall(expr, IlRuntimeDaySerialMethod); break;
            case TokenKind.StrDate: EmitNoArgBuiltinCall(expr, IlRuntimeStrDateMethod); break;
            case TokenKind.Today or TokenKind.RddToday: EmitNoArgBuiltinCall(expr, IlRuntimeTodayMethod); break;
            case TokenKind.DateValue: EmitStringUnaryBuiltinCall(expr, IlRuntimeDateValueMethod); break;
            case TokenKind.DateSerial: EmitInt16TernaryBuiltinCall(expr, IlRuntimeDateSerialMethod); break;
            case TokenKind.TimeSerial: EmitInt16TernaryBuiltinCall(expr, IlRuntimeTimeSerialMethod); break;
            case TokenKind.Dtos: EmitDateTimeUnaryBuiltinCall(expr, IlRuntimeDtosMethod); break;
            case TokenKind.Stod: EmitStringUnaryBuiltinCall(expr, IlRuntimeStodMethod); break;
            case TokenKind.Message:
                if (expr.Args.Length == 0) { _il.Emit(OpCodes.Ldnull); _il.Emit(OpCodes.Call, IlMessageCmdMethod); }
                else if (expr.Args.Length == 1) { EmitStringArg(expr.Args[0]); _il.Emit(OpCodes.Call, IlMessageCmdMethod); }
                else throw new NotSupportedException($"Backend IL: MESSAGE accetta al più 1 argomento.");
                break;
            case TokenKind.Ctod: EmitStringUnaryBuiltinCall(expr, IlRuntimeCtODDateMethod); break;
            case TokenKind.Year: EmitDateTimeUnaryBuiltinCall(expr, IlRuntimeYearMethod); break;
            case TokenKind.Month: EmitDateTimeUnaryBuiltinCall(expr, IlRuntimeMonthMethod); break;
            case TokenKind.Day: EmitDateTimeUnaryBuiltinCall(expr, IlRuntimeDayMethod); break;
            case TokenKind.Hour: EmitDateTimeUnaryBuiltinCall(expr, IlRuntimeHourMethod); break;
            case TokenKind.Minute: EmitDateTimeUnaryBuiltinCall(expr, IlRuntimeMinuteMethod); break;
            case TokenKind.Second: EmitDateTimeUnaryBuiltinCall(expr, IlRuntimeSecondMethod); break;
            case TokenKind.Weekday: EmitDateTimeUnaryBuiltinCall(expr, IlRuntimeWeekdayMethod); break;
            case TokenKind.IsLeapYear: EmitDateTimeUnaryBuiltinCall(expr, IlRuntimeIsLeapYearMethod); break;
            case TokenKind.DaysInMonth: EmitDateTimeUnaryBuiltinCall(expr, IlRuntimeDaysInMonthMethod); break;
            case TokenKind.FirstDayOfMonth: EmitDateTimeUnaryBuiltinCall(expr, IlRuntimeFirstDayOfMonthMethod); break;
            case TokenKind.IsoWeekOfYear: EmitDateTimeUnaryBuiltinCall(expr, IlRuntimeIsoWeekOfYearMethod); break;
            case TokenKind.DateFromUnix: EmitInt64UnaryBuiltinCall(expr, IlRuntimeDateFromUnixMethod); break;
            case TokenKind.UnixTimestamp: EmitDateTimeUnaryBuiltinCall(expr, IlRuntimeUnixTimestampMethod); break;
            case TokenKind.Time: EmitNoArgBuiltinCall(expr, IlRuntimeTimeMethod); break;
            case TokenKind.Inkey: case TokenKind.InkeyDollar:
                if (expr.Args.Length == 0) EmitNoArgBuiltinCall(expr, IlInkeyMethod);
                else if (expr.Args.Length == 1) { EmitInt32Argument(expr.Args[0]); _il.Emit(OpCodes.Call, IlInkeyTimeoutMethod); }
                else throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} accetta al più 1 argomento (timeout ms).");
                break;
            case TokenKind.StrTime: EmitNoArgBuiltinCall(expr, IlRuntimeStrTimeMethod); break;
            case TokenKind.Eomonth: EmitEomonthCall(expr); break;
            case TokenKind.IsDate: EmitStringUnaryBuiltinCall(expr, IlRuntimeIsDateMethod); break;
            case TokenKind.Ti: EmitNoArgBuiltinCall(expr, IlRuntimeTiMethod); break;
            case TokenKind.StrTi: EmitNoArgBuiltinCall(expr, IlRuntimeStrTiMethod); break;
            case TokenKind.DateAdd: EmitDateAddCall(expr); break;
            case TokenKind.UBound: EmitUBoundCall(expr); break;
            case TokenKind.LBound: EmitLBoundCall(expr); break;
            case TokenKind.Rnd: EmitRndCall(expr); break;
            case TokenKind.Abs: EmitDoubleBuiltinCall(expr, IlRuntimeAbsMethod); break;
            /* ATN con 2 argomenti = atan2(y, x): il primo caso (con guardia) deve precedere
             * quello a un argomento. L'arity arriva dal link, che tiene il numero REALE di
             * argomenti scritti (nessun padding degli opzionali). */
            case TokenKind.Atn when expr.Args.Length == 2: EmitDoubleBinaryBuiltinCall(expr, IlRuntimeAtn2Method); break;
            case TokenKind.Atn: EmitDoubleBuiltinCall(expr, IlRuntimeAtnMethod); break;
            case TokenKind.Cos: EmitDoubleBuiltinCall(expr, IlRuntimeCosMethod); break;
            case TokenKind.Exp: EmitDoubleBuiltinCall(expr, IlRuntimeExpMethod); break;
            case TokenKind.Log: EmitDoubleBuiltinCall(expr, IlRuntimeLogMethod); break;
            case TokenKind.Log10: EmitDoubleBuiltinCall(expr, IlRuntimeLog10Method); break;
            case TokenKind.Sin: EmitDoubleBuiltinCall(expr, IlRuntimeSinMethod); break;
            case TokenKind.Sqr: EmitDoubleBuiltinCall(expr, IlRuntimeSqrMethod); break;
            case TokenKind.SpecialGamma: EmitNumericCall(expr, IlRuntimeGammaMethod); break;
            case TokenKind.SpecialLGamma: EmitNumericCall(expr, IlRuntimeLGammaMethod); break;
            case TokenKind.SpecialBeta: EmitNumericCall(expr, IlRuntimeBetaMethod); break;
            case TokenKind.SpecialErf: EmitNumericCall(expr, IlRuntimeErfMethod); break;
            case TokenKind.SpecialErfc: EmitNumericCall(expr, IlRuntimeErfcMethod); break;
            case TokenKind.SpecialGammaInc: EmitNumericCall(expr, IlRuntimeGammaIncMethod); break;
            case TokenKind.SpecialGammaIncc: EmitNumericCall(expr, IlRuntimeGammaInccMethod); break;
            case TokenKind.SpecialBetaInc: EmitNumericCall(expr, IlRuntimeBetaIncMethod); break;
            case TokenKind.SpecialDigamma: EmitNumericCall(expr, IlRuntimeDigammaMethod); break;
            case TokenKind.SpecialBesselJ: EmitNumericCall(expr, IlRuntimeBesselJMethod); break;
            case TokenKind.SpecialBesselY: EmitNumericCall(expr, IlRuntimeBesselYMethod); break;
            case TokenKind.SpecialBesselI: EmitNumericCall(expr, IlRuntimeBesselIMethod); break;
            case TokenKind.SpecialBesselK: EmitNumericCall(expr, IlRuntimeBesselKMethod); break;
            case TokenKind.SpecialBetaLn: EmitNumericCall(expr, IlRuntimeBetaLnMethod); break;
            case TokenKind.SpecialErfInv: EmitNumericCall(expr, IlRuntimeErfInvMethod); break;
            case TokenKind.SpecialErfcInv: EmitNumericCall(expr, IlRuntimeErfcInvMethod); break;
            case TokenKind.SpecialGammaIncInv: EmitNumericCall(expr, IlRuntimeGammaIncInvMethod); break;
            case TokenKind.SpecialGammaInccInv: EmitNumericCall(expr, IlRuntimeGammaInccInvMethod); break;
            case TokenKind.SpecialBetaIncInv: EmitNumericCall(expr, IlRuntimeBetaIncInvMethod); break;
            case TokenKind.Tan: EmitDoubleBuiltinCall(expr, IlRuntimeTanMethod); break;
            case TokenKind.Asin: EmitDoubleBuiltinCall(expr, IlRuntimeAsinMethod); break;
            case TokenKind.Acos: EmitDoubleBuiltinCall(expr, IlRuntimeAcosMethod); break;
            case TokenKind.Floor or TokenKind.Int: EmitDoubleBuiltinCall(expr, IlRuntimeFloorMethod); break;
            case TokenKind.Ceiling: EmitDoubleBuiltinCall(expr, IlRuntimeCeilingMethod); break;
            case TokenKind.Sgn: EmitDoubleBuiltinCall(expr, IlRuntimeSgnMethod); break;
            case TokenKind.Pi: EmitPiCall(expr); break;
            case TokenKind.Fix: EmitDoubleBuiltinCall(expr, IlRuntimeFixMethod); break;
            case TokenKind.Round: EmitDecimalSafeCall(expr, IlRuntimeRoundSafeMethod); break;
            case TokenKind.Trunc: EmitDecimalSafeCall(expr, IlRuntimeTruncSafeMethod); break;
            case TokenKind.Sinh: EmitDoubleBuiltinCall(expr, IlRuntimeSinhMethod); break;
            case TokenKind.Cosh: EmitDoubleBuiltinCall(expr, IlRuntimeCoshMethod); break;
            case TokenKind.Tanh: EmitDoubleBuiltinCall(expr, IlRuntimeTanhMethod); break;
            case TokenKind.Deg2Rad: EmitDoubleBuiltinCall(expr, IlRuntimeDeg2RadMethod); break;
            case TokenKind.Rad2Deg: EmitDoubleBuiltinCall(expr, IlRuntimeRad2DegMethod); break;
            case TokenKind.Hypot or TokenKind.Pythagoras: EmitDoubleBinaryBuiltinCall(expr, IlRuntimeHypotMethod); break;
            case TokenKind.Min when expr.Args.Length == 1 && NormalizeInferredType(InferType(expr.Args[0])) is AnyType or TreeType or AVLTreeType
                or RedBlackTreeType or BinaryIndexedTreeType or TrieType or BPlusTreeType or GraphType
                or HashRingType or SparseSetType or BinaryHeapType or BagType or CacheType or TimelineType
                or RingBufferType: EmitCollectionMinMaxCall(expr, min: true); break;
            case TokenKind.Max when expr.Args.Length == 1 && NormalizeInferredType(InferType(expr.Args[0])) is AnyType or TreeType or AVLTreeType
                or RedBlackTreeType or BinaryIndexedTreeType or TrieType or BPlusTreeType or GraphType
                or HashRingType or SparseSetType or BinaryHeapType or BagType or CacheType or TimelineType
                or RingBufferType: EmitCollectionMinMaxCall(expr, min: false); break;
            case TokenKind.Min: EmitDoubleParamsBuiltinCall(expr, IlRuntimeMinMethod); break;
            case TokenKind.Max: EmitDoubleParamsBuiltinCall(expr, IlRuntimeMaxMethod); break;
            case TokenKind.Sum: EmitDoubleParamsBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.Sum))); break;
            case TokenKind.Avg: EmitDoubleParamsBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.Avg))); break;
            case TokenKind.Clamp: EmitDoubleArityBuiltinCall(expr, IlRuntimeClampMethod, 3); break;
            case TokenKind.Fact: EmitInt32BuiltinCall(expr, IlRuntimeFactMethod, 1); break;
            case TokenKind.Perm: EmitInt32BuiltinCall(expr, IlRuntimePermMethod, 2); break;
            case TokenKind.Comb: EmitInt32BuiltinCall(expr, IlRuntimeCombMethod, 2); break;
            case TokenKind.Dist2d or TokenKind.Angle2d: EmitDoubleArityBuiltinCall(expr, expr.Name.Kind == TokenKind.Dist2d ? IlRuntimeDist2dMethod : IlRuntimeAngle2dMethod, 4); break;
            case TokenKind.Dist3d: EmitDoubleArityBuiltinCall(expr, IlRuntimeDist3dMethod, 6); break;
            case TokenKind.PolarX: EmitDoubleBinaryBuiltinCall(expr, IlRuntimePolarXMethod); break;
            case TokenKind.PolarY: EmitDoubleBinaryBuiltinCall(expr, IlRuntimePolarYMethod); break;
            case TokenKind.Mean: EmitStatsArrayCall(expr, IlRuntimeMeanMethod); break;
            case TokenKind.Median: EmitStatsArrayCall(expr, IlRuntimeMedianMethod); break;
            case TokenKind.Variance: EmitStatsArrayCall(expr, IlRuntimeVarianceMethod); break;
            case TokenKind.StdDev: EmitStatsArrayCall(expr, IlRuntimeStdDevMethod); break;
            case TokenKind.Skewness: EmitStatsArrayCall(expr, IlRuntimeSkewnessMethod); break;
            case TokenKind.Kurtosis: EmitStatsArrayCall(expr, IlRuntimeKurtosisMethod); break;
            case TokenKind.Correlation: EmitStatsArrayCall(expr, IlRuntimeCorrelationMethod, 2); break;
            case TokenKind.Percentile: EmitStatsArrayDoubleCall(expr, IlRuntimePercentileMethod); break;
            case TokenKind.Mad: EmitStatsArrayCall(expr, IlRuntimeMadMethod); break;
            case TokenKind.TTest: EmitAnalysisTwoArrayCall(expr, nameof(IlRuntime.TTest)); break;
            case TokenKind.TTestPaired: EmitAnalysisTwoArrayCall(expr, nameof(IlRuntime.TTestPaired)); break;
            case TokenKind.TTestOneSample: EmitAnalysisArrayScalarCall(expr, nameof(IlRuntime.TTestOneSample)); break;
            case TokenKind.StatsAnova: EmitAnalysisMatrixCall(expr, nameof(IlRuntime.StatsAnova)); break;
            case TokenKind.Chi2Test: EmitAnalysisTwoArrayCall(expr, nameof(IlRuntime.Chi2Test)); break;
            case TokenKind.FisherExact: EmitStatsFourScalarCall(expr, nameof(IlRuntime.FisherExact)); break;
            case TokenKind.ShapiroWilk: EmitAnalysisArrayCall(expr, nameof(IlRuntime.ShapiroWilk)); break;
            case TokenKind.NormalTest: EmitAnalysisArrayCall(expr, nameof(IlRuntime.NormalTest)); break;
            case TokenKind.ZTest: EmitStatsArrayTwoScalarCall(expr, nameof(IlRuntime.ZTest)); break;
            case TokenKind.KolmogorovSmirnov: EmitAnalysisTwoArrayCall(expr, nameof(IlRuntime.KolmogorovSmirnov)); break;
            case TokenKind.StatsPearsonR: EmitAnalysisTwoArrayCall(expr, nameof(IlRuntime.PearsonR)); break;
            case TokenKind.StatsSpearmanR: EmitAnalysisTwoArrayCall(expr, nameof(IlRuntime.SpearmanR)); break;
            case TokenKind.StatsLinRegress: EmitAnalysisTwoArrayCall(expr, nameof(IlRuntime.LinRegress)); break;
            case TokenKind.StatsMannWhitney: EmitAnalysisTwoArrayCall(expr, nameof(IlRuntime.MannWhitney)); break;
            case TokenKind.StatsWilcoxon: EmitAnalysisTwoArrayCall(expr, nameof(IlRuntime.Wilcoxon)); break;
            case TokenKind.StatsChi2Contingency: EmitAnalysisMatrixCall(expr, nameof(IlRuntime.Chi2Con)); break;
            case TokenKind.StatsSkewTest: EmitAnalysisArrayCall(expr, nameof(IlRuntime.SkewTest)); break;
            case TokenKind.StatsKurtosisTest: EmitAnalysisArrayCall(expr, nameof(IlRuntime.KurtosisTest)); break;
            case TokenKind.StatsJarqueBera: EmitAnalysisArrayCall(expr, nameof(IlRuntime.JarqueBera)); break;
            case TokenKind.StatsDescribe: EmitAnalysisArrayCall(expr, nameof(IlRuntime.Describe)); break;
            case TokenKind.StatsMode: EmitAnalysisArrayCall(expr, nameof(IlRuntime.Mode)); break;
            case TokenKind.StatsIqr: EmitAnalysisArrayCall(expr, nameof(IlRuntime.Iqr)); break;
            case TokenKind.StatsMoment: EmitAnalysisArrayScalarCall(expr, nameof(IlRuntime.Moment)); break;
            case TokenKind.StatsGammaPdf: EmitTripleScalarCall(expr, nameof(IlRuntime.GammaPdf)); break;
            case TokenKind.StatsGammaCdf: EmitTripleScalarCall(expr, nameof(IlRuntime.GammaCdf)); break;
            case TokenKind.StatsGammaPpf: EmitTripleScalarCall(expr, nameof(IlRuntime.GammaPpf)); break;
            case TokenKind.StatsGammaIsf: EmitTripleScalarCall(expr, nameof(IlRuntime.GammaIsf)); break;
            case TokenKind.AMax: EmitStatsArrayCall(expr, RuntimeMethod(nameof(IlRuntime.AMax))); break;
            case TokenKind.AMin: EmitStatsArrayCall(expr, RuntimeMethod(nameof(IlRuntime.AMin))); break;
            case TokenKind.ASum: EmitStatsArrayCall(expr, RuntimeMethod(nameof(IlRuntime.ASum))); break;
            case TokenKind.NormalPdf: EmitOptionalParamsCall(expr, IlRuntimeNormalPdfMethod, 3, 0.0, 1.0); break;
            case TokenKind.NormalCdf: EmitOptionalParamsCall(expr, IlRuntimeNormalCdfMethod, 3, 0.0, 1.0); break;
            // Inverse CDF (ppf) e ISF — quantili e valori critici.
            case TokenKind.NormalPpf: EmitOptionalParamsCall(expr, IlRuntimeNormalPpfMethod, 3, 0.0, 1.0); break;
            case TokenKind.NormalIsf: EmitOptionalParamsCall(expr, IlRuntimeNormalIsfMethod, 3, 0.0, 1.0); break;
            case TokenKind.LognormalPpf: EmitOptionalParamsCall(expr, IlRuntimeLognormalPpfMethod, 3, 0.0, 1.0); break;
            case TokenKind.ChiSquaredPpf: EmitOptionalParamsCall(expr, IlRuntimeChiSquaredPpfMethod, 2, 1.0); break;
            case TokenKind.ChiSquaredIsf: EmitOptionalParamsCall(expr, IlRuntimeChiSquaredIsfMethod, 2, 1.0); break;
            case TokenKind.ChiPpf: EmitOptionalParamsCall(expr, IlRuntimeChiPpfMethod, 2, 1.0); break;
            case TokenKind.BetaPpf: EmitOptionalParamsCall(expr, IlRuntimeBetaPpfMethod, 3, 1.0, 1.0); break;
            case TokenKind.FPpf: EmitOptionalParamsCall(expr, IlRuntimeFPpfMethod, 3, 1.0, 1.0); break;
            case TokenKind.FIsf: EmitOptionalParamsCall(expr, IlRuntimeFIsfMethod, 3, 1.0, 1.0); break;
            case TokenKind.StudentTPpf: EmitOptionalParamsCall(expr, IlRuntimeStudentTPpfMethod, 2, 1.0); break;
            case TokenKind.StudentTIsf: EmitOptionalParamsCall(expr, IlRuntimeStudentTIsfMethod, 2, 1.0); break;
            case TokenKind.ExponentialPpf: EmitOptionalParamsCall(expr, IlRuntimeExponentialPpfMethod, 2, 1.0); break;
            case TokenKind.ExponentialIsf: EmitOptionalParamsCall(expr, IlRuntimeExponentialIsfMethod, 2, 1.0); break;
            case TokenKind.WeibullPpf: EmitOptionalParamsCall(expr, IlRuntimeWeibullPpfMethod, 3, 1.0, 1.0); break;
            case TokenKind.LogisticPpf: EmitOptionalParamsCall(expr, IlRuntimeLogisticPpfMethod, 3, 0.0, 1.0); break;
            case TokenKind.UniformPpf: EmitOptionalParamsCall(expr, IlRuntimeUniformPpfMethod, 3, 0.0, 1.0); break;
            case TokenKind.ParetoPpf: EmitOptionalParamsCall(expr, IlRuntimeParetoPpfMethod, 3, 1.0, 1.0); break;
            case TokenKind.TriangularPpf: EmitOptionalParamsCall(expr, IlRuntimeTriangularPpfMethod, 4, 0.0, 0.5, 1.0); break;
            case TokenKind.LognormalPdf: EmitOptionalParamsCall(expr, IlRuntimeLognormalPdfMethod, 3, 0.0, 1.0); break;
            case TokenKind.LognormalCdf: EmitOptionalParamsCall(expr, IlRuntimeLognormalCdfMethod, 3, 0.0, 1.0); break;
            case TokenKind.ChiPdf: EmitOptionalParamsCall(expr, IlRuntimeChiPdfMethod, 2, 1.0); break;
            case TokenKind.ChiCdf: EmitOptionalParamsCall(expr, IlRuntimeChiCdfMethod, 2, 1.0); break;
            case TokenKind.ChiSquaredPdf: EmitOptionalParamsCall(expr, IlRuntimeChiSquaredPdfMethod, 2, 1.0); break;
            case TokenKind.ChiSquaredCdf: EmitOptionalParamsCall(expr, IlRuntimeChiSquaredCdfMethod, 2, 1.0); break;
            case TokenKind.BetaPdf: EmitOptionalParamsCall(expr, IlRuntimeBetaPdfMethod, 3, 1.0, 1.0); break;
            case TokenKind.BetaCdf: EmitOptionalParamsCall(expr, IlRuntimeBetaCdfMethod, 3, 1.0, 1.0); break;
            case TokenKind.StudentTPdf: EmitOptionalParamsCall(expr, IlRuntimeStudentTPdfMethod, 2, 1.0); break;
            case TokenKind.StudentTCdf: EmitOptionalParamsCall(expr, IlRuntimeStudentTCdfMethod, 2, 1.0); break;
            case TokenKind.WeibullPdf: EmitOptionalParamsCall(expr, IlRuntimeWeibullPdfMethod, 3, 1.0, 1.0); break;
            case TokenKind.WeibullCdf: EmitOptionalParamsCall(expr, IlRuntimeWeibullCdfMethod, 3, 1.0, 1.0); break;
            case TokenKind.FPdf: EmitOptionalParamsCall(expr, IlRuntimeFPdfMethod, 3, 1.0, 1.0); break;
            case TokenKind.FCdf: EmitOptionalParamsCall(expr, IlRuntimeFCdfMethod, 3, 1.0, 1.0); break;
            case TokenKind.ExponentialPdf: EmitOptionalParamsCall(expr, IlRuntimeExponentialPdfMethod, 2, 1.0); break;
            case TokenKind.ExponentialCdf: EmitOptionalParamsCall(expr, IlRuntimeExponentialCdfMethod, 2, 1.0); break;
            case TokenKind.ParetoPdf: EmitOptionalParamsCall(expr, IlRuntimeParetoPdfMethod, 3, 1.0, 1.0); break;
            case TokenKind.LogisticPdf: EmitOptionalParamsCall(expr, IlRuntimeLogisticPdfMethod, 3, 0.0, 1.0); break;
            case TokenKind.TriangularPdf: EmitOptionalParamsCall(expr, IlRuntimeTriangularPdfMethod, 4, 0.0, 0.5, 1.0); break;
            case TokenKind.UniformPdf: EmitOptionalParamsCall(expr, IlRuntimeUniformPdfMethod, 3, 0.0, 1.0); break;
            case TokenKind.SkewedNormalPdf: EmitOptionalParamsCall(expr, IlRuntimeSkewedNormalPdfMethod, 4, 0.0, 1.0, 0.0); break;
            // BASIC: BernoulliPmf(x, p) -> StdLib.Runtime.BernoulliPmf(p, k=x)
            case TokenKind.BernoulliPmf: EmitPmfBuiltinCall(expr, IlRuntimeBernoulliPmfMethod, 2, [(1, false), (0, true)]); break;
            // BASIC: BinomialPmf(x, n, p) -> StdLib.Runtime.BinomialPmf(p, n, k=x) — permutazione
            // vera [2,1,0], non uno shift semplice: p e' l'ULTIMO parametro BASIC, non il primo.
            case TokenKind.BinomialPmf: EmitPmfBuiltinCall(expr, IlRuntimeBinomialPmfMethod, 3, [(2, false), (1, true), (0, true)]); break;
            // BASIC: PoissonPmf(x, lambda) -> StdLib.Runtime.PoissonPmf(lambda, k=x)
            case TokenKind.PoissonPmf: EmitPmfBuiltinCall(expr, IlRuntimePoissonPmfMethod, 2, [(1, false), (0, true)]); break;
            // BASIC: GeometricPmf(x, p) -> StdLib.Runtime.GeometricPmf(p, k=x)
            case TokenKind.GeometricPmf: EmitPmfBuiltinCall(expr, IlRuntimeGeometricPmfMethod, 2, [(1, false), (0, true)]); break;
            // BASIC: NegativeBinomialPmf(x, r, p) -> StdLib.Runtime.NegativeBinomialPmf(r, p, k=x)
            case TokenKind.NegativeBinomialPmf: EmitPmfBuiltinCall(expr, IlRuntimeNegativeBinomialPmfMethod, 3, [(1, false), (2, false), (0, true)]); break;
            // BASIC: HypergeometricPmf(x, population, successes, draws) ->
            // StdLib.Runtime.HypergeometricPmf(populationSize, successStates, draws, k=x) — TUTTI
            // e 4 i parametri sono int in StdLib.Runtime, non solo k.
            case TokenKind.HypergeometricPmf: EmitPmfBuiltinCall(expr, IlRuntimeHypergeometricPmfMethod, 4, [(1, true), (2, true), (3, true), (0, true)]); break;
            case TokenKind.Pmt: EmitFinanceNumericCall(expr, nameof(IlRuntime.Pmt), 5, 0, 0); break;
            case TokenKind.Fv: EmitFinanceNumericCall(expr, nameof(IlRuntime.Fv), 5, 0, 0); break;
            case TokenKind.Pv: EmitFinanceNumericCall(expr, nameof(IlRuntime.Pv), 5, 0, 0); break;
            case TokenKind.Nper: EmitFinanceNumericCall(expr, nameof(IlRuntime.Nper), 5, 0, 0); break;
            case TokenKind.Rate: EmitFinanceNumericCall(expr, nameof(IlRuntime.Rate), 6, 0, 0, 0.1); break;
            case TokenKind.Cumipmt: EmitFinanceNumericCall(expr, nameof(IlRuntime.Cumipmt), 6); break;
            case TokenKind.Cumprinc: EmitFinanceNumericCall(expr, nameof(IlRuntime.Cumprinc), 6); break;
            case TokenKind.Sln: EmitFinanceNumericCall(expr, nameof(IlRuntime.Sln), 3); break;
            case TokenKind.Syd: EmitFinanceNumericCall(expr, nameof(IlRuntime.Syd), 4); break;
            case TokenKind.Db: EmitFinanceNumericCall(expr, nameof(IlRuntime.Db), 5, 12); break;
            case TokenKind.Ddb: EmitFinanceNumericCall(expr, nameof(IlRuntime.Ddb), 5, 2); break;
            case TokenKind.Npv: EmitFinanceNpvCall(expr); break;
            case TokenKind.Irr: EmitFinanceArrayDoubleCall(expr, nameof(IlRuntime.Irr), 0.1); break;
            case TokenKind.Mirr: EmitFinanceArrayDoubleDoubleCall(expr, nameof(IlRuntime.Mirr)); break;
            case TokenKind.Xnpv: EmitFinanceXnpvCall(expr); break;
            case TokenKind.Xirr: EmitFinanceXirrCall(expr); break;
            case TokenKind.Price: EmitFinanceDateDateNumericCall(expr, nameof(IlRuntime.Price), 4, 0); break;
            case TokenKind.Duration: EmitDurationCall(expr); break;
            case TokenKind.YearFrac: EmitFinanceDateDateNumericCall(expr, nameof(IlRuntime.YearFrac), 0, 0); break;
            case TokenKind.Days360: EmitDays360Call(expr); break;
            case TokenKind.MovingAverage: EmitTimeSeriesArrayLongCall(expr, nameof(IlRuntime.MovingAverage)); break;
            case TokenKind.EMA: EmitTimeSeriesArrayLongCall(expr, nameof(IlRuntime.Ema)); break;
            case TokenKind.RollingStdDev: EmitTimeSeriesArrayLongCall(expr, nameof(IlRuntime.RollingStdDev)); break;
            case TokenKind.RollingMad: EmitTimeSeriesArrayLongCall(expr, nameof(IlRuntime.RollingMad)); break;
            case TokenKind.RollingZScore: EmitTimeSeriesArrayLongCall(expr, nameof(IlRuntime.RollingZScore)); break;
            case TokenKind.RollingAutocorrelation: EmitTimeSeriesArrayLongLongCall(expr, nameof(IlRuntime.RollingAutocorrelation)); break;
            case TokenKind.RollingCovariance: EmitTimeSeriesTwoArrayLongCall(expr, nameof(IlRuntime.RollingCovariance)); break;
            case TokenKind.RollingCorrelation: EmitTimeSeriesTwoArrayLongCall(expr, nameof(IlRuntime.RollingCorrelation)); break;
            case TokenKind.RollingRegression: EmitTimeSeriesTwoArrayLongCall(expr, nameof(IlRuntime.RollingRegression)); break;
            case TokenKind.RollingRegressionSingle: EmitTimeSeriesArrayLongCall(expr, nameof(IlRuntime.RollingRegressionSingle)); break;
            case TokenKind.YuleWalker: EmitTimeSeriesArrayLongCall(expr, nameof(IlRuntime.YuleWalker)); break;
            case TokenKind.MaCoefficients: EmitTimeSeriesArrayLongCall(expr, nameof(IlRuntime.MaCoefficients)); break;
            case TokenKind.EuclideanDistanceSquared: EmitAnalysisTwoArrayCall(expr, nameof(IlRuntime.EuclideanDistanceSquared)); break;
            case TokenKind.MatrixRowSums: EmitLinAlgUnaryHandleCall(expr, RuntimeMethod(nameof(IlRuntime.MatrixRowSums)), vector: false); break;
            case TokenKind.MatrixColSums: EmitLinAlgUnaryHandleCall(expr, RuntimeMethod(nameof(IlRuntime.MatrixColSums)), vector: false); break;
            case TokenKind.MatrixRowMeans: EmitLinAlgUnaryHandleCall(expr, RuntimeMethod(nameof(IlRuntime.MatrixRowMeans)), vector: false); break;
            case TokenKind.MatrixColMeans: EmitLinAlgUnaryHandleCall(expr, RuntimeMethod(nameof(IlRuntime.MatrixColMeans)), vector: false); break;
            case TokenKind.MatrixRowVariances: EmitLinAlgUnaryHandleCall(expr, RuntimeMethod(nameof(IlRuntime.MatrixRowVariances)), vector: false); break;
            case TokenKind.MatrixColVariances: EmitLinAlgUnaryHandleCall(expr, RuntimeMethod(nameof(IlRuntime.MatrixColVariances)), vector: false); break;
            case TokenKind.MatrixFrobeniusNorm: EmitLinAlgUnaryHandleCall(expr, RuntimeMethod(nameof(IlRuntime.MatrixFrobeniusNorm)), vector: false); break;
            case TokenKind.MatrixOneNorm: EmitLinAlgUnaryHandleCall(expr, RuntimeMethod(nameof(IlRuntime.MatrixOneNorm)), vector: false); break;
            case TokenKind.MatrixInfinityNorm: EmitLinAlgUnaryHandleCall(expr, RuntimeMethod(nameof(IlRuntime.MatrixInfinityNorm)), vector: false); break;
            case TokenKind.MatrixIsSymmetric: EmitLinAlgUnaryHandleCall(expr, RuntimeMethod(nameof(IlRuntime.MatrixIsSymmetric)), vector: false); break;
            case TokenKind.MatrixIsOrthogonal: EmitLinAlgUnaryHandleCall(expr, RuntimeMethod(nameof(IlRuntime.MatrixIsOrthogonal)), vector: false); break;
            case TokenKind.MatrixIsUpperTriangular: EmitLinAlgUnaryHandleCall(expr, RuntimeMethod(nameof(IlRuntime.MatrixIsUpperTriangular)), vector: false); break;
            case TokenKind.MatrixIsLowerTriangular: EmitLinAlgUnaryHandleCall(expr, RuntimeMethod(nameof(IlRuntime.MatrixIsLowerTriangular)), vector: false); break;
            case TokenKind.MatrixArgMax: EmitLinAlgUnaryHandleCall(expr, RuntimeMethod(nameof(IlRuntime.MatrixArgMax)), vector: false); break;
            case TokenKind.MatrixArgMin: EmitLinAlgUnaryHandleCall(expr, RuntimeMethod(nameof(IlRuntime.MatrixArgMin)), vector: false); break;
            case TokenKind.MatrixSlice: EmitMatrixSliceCall(expr); break;
            case TokenKind.MatrixReshape: EmitMatrixReshapeCall(expr); break;
            case TokenKind.MatrixConcatRows: EmitLinAlgLongPairCall(expr, nameof(IlRuntime.MatrixConcatRows)); break;
            case TokenKind.MatrixConcatCols: EmitLinAlgLongPairCall(expr, nameof(IlRuntime.MatrixConcatCols)); break;
            case TokenKind.MatrixAddRowVector: EmitMatrixBroadcastCall(expr, nameof(IlRuntime.MatrixAddRowVector)); break;
            case TokenKind.MatrixAddColVector: EmitMatrixBroadcastCall(expr, nameof(IlRuntime.MatrixAddColVector)); break;
            case TokenKind.MatrixMaxValue: EmitLinAlgUnaryHandleCall(expr, RuntimeMethod(nameof(IlRuntime.MatrixMaxValue)), vector: false); break;
            case TokenKind.MatrixMinValue: EmitLinAlgUnaryHandleCall(expr, RuntimeMethod(nameof(IlRuntime.MatrixMinValue)), vector: false); break;
            case TokenKind.MatrixAboveMask: EmitMatrixThresholdCall(expr, nameof(IlRuntime.MatrixAboveMask)); break;
            case TokenKind.MatrixAboveMatrix: EmitMatrixThresholdCall(expr, nameof(IlRuntime.MatrixAboveMatrix)); break;
            case TokenKind.MatrixAsCsv: EmitLinAlgUnaryHandleCall(expr, RuntimeMethod(nameof(IlRuntime.MatrixAsCsv)), vector: false); break;
            case TokenKind.MatrixAsJson: EmitLinAlgUnaryHandleCall(expr, RuntimeMethod(nameof(IlRuntime.MatrixAsJson)), vector: false); break;
            case TokenKind.MatrixRowCount: EmitLinAlgUnaryHandleCall(expr, RuntimeMethod(nameof(IlRuntime.MatrixRowCount)), vector: false); break;
            case TokenKind.MatrixColumnCount: EmitLinAlgUnaryHandleCall(expr, RuntimeMethod(nameof(IlRuntime.MatrixColumnCount)), vector: false); break;
            case TokenKind.MatrixClone: EmitLinAlgUnaryHandleCall(expr, RuntimeMethod(nameof(IlRuntime.MatrixClone)), vector: false); break;
            case TokenKind.MatrixMultiplyVector: EmitMatrixVectorProductCall(expr, nameof(IlRuntime.MatrixMultiplyVector)); break;
            case TokenKind.MatrixMultiplyLeftVector: EmitMatrixVectorProductCall(expr, nameof(IlRuntime.MatrixMultiplyLeftVector)); break;
            case TokenKind.MatrixLoadCsv: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.MatrixLoadCsv))); break;
            case TokenKind.MatrixLoadJson: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.MatrixLoadJson))); break;
            case TokenKind.RollingDelta: EmitTimeSeriesArrayLongCall(expr, nameof(IlRuntime.RollingDelta)); break;
            case TokenKind.Outliers: EmitTimeSeriesArrayDoubleCall(expr, nameof(IlRuntime.Outliers)); break;
            case TokenKind.Difference: EmitTimeSeriesArrayLongCall(expr, nameof(IlRuntime.Difference)); break;
            case TokenKind.Returns: EmitTimeSeriesArrayOnlyCall(expr, nameof(IlRuntime.Returns)); break;
            case TokenKind.Volatility: EmitTimeSeriesArrayOnlyCall(expr, nameof(IlRuntime.Volatility)); break;
            case TokenKind.Autocorrelation: EmitTimeSeriesArrayLongCall(expr, nameof(IlRuntime.Autocorrelation)); break;
            case TokenKind.Deseasonalize: EmitTimeSeriesArrayLongCall(expr, nameof(IlRuntime.Deseasonalize)); break;
            case TokenKind.Decompose: EmitTimeSeriesArrayLongCall(expr, nameof(IlRuntime.Decompose)); break;
            case TokenKind.TimeSeriesCovarianceMatrix: EmitAnalysisMatrixCall(expr, nameof(IlRuntime.TimeSeriesCovarianceMatrix)); break;
            case TokenKind.TimeSeriesMinMax: EmitTimeSeriesArrayOnlyCall(expr, nameof(IlRuntime.TimeSeriesMinMax)); break;
            case TokenKind.RollingMean: EmitTimeSeriesArrayLongCall(expr, nameof(IlRuntime.RollingMean)); break;
            case TokenKind.RollingSkewness: EmitTimeSeriesArrayLongCall(expr, nameof(IlRuntime.RollingSkewness)); break;
            case TokenKind.RollingKurtosis: EmitTimeSeriesArrayLongCall(expr, nameof(IlRuntime.RollingKurtosis)); break;
            case TokenKind.RollingMedian: EmitTimeSeriesArrayLongCall(expr, nameof(IlRuntime.RollingMedian)); break;
            case TokenKind.RollingQuantile: EmitTimeSeriesArrayLongDoubleCall(expr, nameof(IlRuntime.RollingQuantile)); break;
            case TokenKind.RollingSharpe: EmitTimeSeriesArrayLongDoubleCall(expr, nameof(IlRuntime.RollingSharpe)); break;
            case TokenKind.RollingSortino: EmitTimeSeriesArrayLongDoubleDoubleCall(expr, nameof(IlRuntime.RollingSortino)); break;
            case TokenKind.RollingMinMax: EmitTimeSeriesArrayLongCall(expr, nameof(IlRuntime.RollingMinMax)); break;
            case TokenKind.TotalReturn: EmitTimeSeriesArrayOnlyCall(expr, nameof(IlRuntime.TotalReturn)); break;
            case TokenKind.AnnualizedReturn: EmitTimeSeriesArrayLongCall(expr, nameof(IlRuntime.AnnualizedReturn)); break;
            case TokenKind.SortinoRatio: EmitTradingArrayDoubleLongCall(expr, nameof(IlRuntime.SortinoRatio)); break;
            case TokenKind.MaxDrawdown: EmitTimeSeriesArrayOnlyCall(expr, nameof(IlRuntime.MaxDrawdown)); break;
            case TokenKind.CalmarRatio: EmitTimeSeriesArrayLongCall(expr, nameof(IlRuntime.CalmarRatio)); break;
            case TokenKind.InformationRatio: EmitTimeSeriesTwoArrayLongCall(expr, nameof(IlRuntime.InformationRatio)); break;
            case TokenKind.Alpha: EmitTradingTwoArrayDoubleLongCall(expr, nameof(IlRuntime.Alpha)); break;
            case TokenKind.Beta: EmitTradingTwoArrayCall(expr, nameof(IlRuntime.Beta)); break;
            case TokenKind.TreynorRatio: EmitTradingTwoArrayDoubleLongCall(expr, nameof(IlRuntime.TreynorRatio)); break;
            case TokenKind.ValueAtRisk: EmitTimeSeriesArrayDoubleCall(expr, nameof(IlRuntime.ValueAtRisk)); break;
            case TokenKind.ConditionalValueAtRisk: EmitTimeSeriesArrayDoubleCall(expr, nameof(IlRuntime.ConditionalValueAtRisk)); break;
            case TokenKind.DownsideDeviation: EmitTimeSeriesArrayDoubleCall(expr, nameof(IlRuntime.DownsideDeviation)); break;
            case TokenKind.FgRgb: EmitFgBgRgbCall(expr, true); break;
            case TokenKind.BgRgb: EmitFgBgRgbCall(expr, false); break;
            case TokenKind.GetCursorPos: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.GetCursorPos))); break;
            case TokenKind.GetTextMetrics: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.GetTextMetrics))); break;
            case TokenKind.GetPaletteColor: EmitDoubleBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.GetPaletteColor))); break;

            // --- MOUSE (namespace MOUSE; stub headless — il backend reale è SDL) ---
            case TokenKind.MouseX: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.MouseX))); break;
            case TokenKind.MouseY: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.MouseY))); break;
            case TokenKind.MouseWheel: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.MouseWheel))); break;
            case TokenKind.MouseMove: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.MouseMove))); break;
            case TokenKind.MousePresent: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.MousePresent))); break;
            case TokenKind.MouseLeft: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.MouseLeft))); break;
            case TokenKind.MouseRight: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.MouseRight))); break;
            case TokenKind.MouseMiddle: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.MouseMiddle))); break;
            case TokenKind.WindowX: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.WindowX))); break;
            case TokenKind.WindowY: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.WindowY))); break;
            case TokenKind.WindowWidth: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.WindowWidth))); break;
            case TokenKind.WindowHeight: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.WindowHeight))); break;
            case TokenKind.WindowTheme: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.GetThemeName))); break;
            case TokenKind.WindowDesktop: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.GetCurrentDesktop))); break;
            case TokenKind.MouseDblClick: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.MouseDblClick))); break;
            case TokenKind.MouseClick: EmitMouseClickCall(expr); break;
            case TokenKind.MouseButton: EmitMouseButtonCall(expr); break;
            case TokenKind.Make: EmitDoubleArityBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.KeyMake)), 2); break;

            // --- Categoria 6: TRADING/STAT blocco 1 ---
            case TokenKind.SharpeRatio: EmitStatsArrayCall(expr, RuntimeMethod(nameof(IlRuntime.SharpeRatio))); break;
            // --- Categoria 6: PORTFOLIO (ottimizzazioni -> dict campi BASIC) ---
            case TokenKind.MinVolatility: EmitPortfolioCovCall(expr); break;
            case TokenKind.MaxSharpe: EmitPortfolioMaxSharpeCall(expr); break;
            case TokenKind.MeanVariance: EmitPortfolioMeanVarianceCall(expr); break;
            case TokenKind.RiskParity: EmitPortfolioCovCall(expr); break;
            case TokenKind.EquallyWeighted: EmitPortfolioEquallyWeightedCall(expr); break;
            case TokenKind.BlackLitterman: EmitPortfolioBlackLittermanCall(expr); break;
            case TokenKind.PBRatio: EmitDoubleArityBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.PbRatio)), 2); break;
            case TokenKind.PSRatio: EmitDoubleArityBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.PsRatio)), 2); break;
            case TokenKind.IsLongOnly: EmitStatsArrayCall(expr, RuntimeMethod(nameof(IlRuntime.IsLongOnly))); break;
            case TokenKind.FvExt: EmitFinanceNumericCall(expr, nameof(IlRuntime.FvExt), 5, 0, 0); break;
            case TokenKind.PvExt: EmitFinanceNumericCall(expr, nameof(IlRuntime.PvExt), 5, 0, 0); break;
            case TokenKind.PmtExt: EmitFinanceNumericCall(expr, nameof(IlRuntime.PmtExt), 5, 0, 0); break;
            case TokenKind.NperExt: EmitFinanceNumericCall(expr, nameof(IlRuntime.NperExt), 5, 0, 0); break;
            case TokenKind.Mnpv: EmitMnpvCall(expr); break;
            case TokenKind.DividendPayoutRatio: EmitDoubleArityBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.DividendPayoutRatio)), 2); break;
            case TokenKind.ReturnsSkewness: EmitTimeSeriesArrayOnlyCall(expr, nameof(IlRuntime.ReturnsSkewness)); break;
            case TokenKind.ReturnsKurtosis: EmitTimeSeriesArrayOnlyCall(expr, nameof(IlRuntime.ReturnsKurtosis)); break;

            // --- Importato da GMBasic (COLL/STR — 13 funzioni lista/dict, semantica case VM) ---
            case TokenKind.Enqueue: EmitObjectArgsCall(expr, nameof(IlRuntime.Enqueue), 2); break;
            case TokenKind.Dequeue: EmitObjectArgsCall(expr, nameof(IlRuntime.Dequeue), 1); break;
            case TokenKind.First: EmitObjectArgsCall(expr, nameof(IlRuntime.First), 1); break;
            case TokenKind.Last: EmitObjectArgsCall(expr, nameof(IlRuntime.Last), 1); break;
            case TokenKind.Clear: EmitObjectArgsCall(expr, nameof(IlRuntime.Clear), 1); break;
            case TokenKind.IndexOf: EmitObjectArgsCall(expr, nameof(IlRuntime.IndexOf), 2); break;
            case TokenKind.Insert: EmitObjectArgsCall(expr, nameof(IlRuntime.Insert), 3); break;
            case TokenKind.RemoveAt: EmitObjectArgsCall(expr, nameof(IlRuntime.RemoveAt), 2); break;
            case TokenKind.GetRange: EmitObjectArgsCall(expr, nameof(IlRuntime.GetRange), 3); break;
            case TokenKind.Sort: EmitObjectArgsCall(expr, nameof(IlRuntime.Sort), 1); break;
            case TokenKind.AddRange: EmitObjectArgsCall(expr, nameof(IlRuntime.AddRange), 2); break;
            case TokenKind.TryGetValue: EmitObjectArgsCall(expr, nameof(IlRuntime.TryGetValue), 3); break;

            // --- Importato da GMBasic (ANALYSIS blocco 1, altra cat6) ---
            case TokenKind.Anova: EmitAnalysisMatrixCall(expr, nameof(IlRuntime.Anova)); break;
            case TokenKind.Regress: EmitAnalysisTwoArrayCall(expr, nameof(IlRuntime.Regress)); break;
            case TokenKind.KMeans: EmitAnalysisArrayScalarCall(expr, nameof(IlRuntime.KMeans)); break;
            case TokenKind.Arima: EmitAnalysisFourScalarCall(expr, nameof(IlRuntime.Arima)); break;
            case TokenKind.HoltWinters: EmitAnalysisFiveCall(expr, nameof(IlRuntime.HoltWinters)); break;
            case TokenKind.Pca: EmitAnalysisMatrixCall(expr, nameof(IlRuntime.Pca)); break;
            case TokenKind.AdfTest: EmitAnalysisArrayCall(expr, nameof(IlRuntime.AdfTest)); break;
            case TokenKind.Fft: EmitAnalysisArrayCall(expr, nameof(IlRuntime.Fft)); break;
            case TokenKind.SignalConvolve: EmitAnalysisArrayArrayCall(expr, nameof(IlRuntime.Convolve)); break;
            case TokenKind.SignalCorrelate: EmitAnalysisArrayArrayCall(expr, nameof(IlRuntime.Correlate)); break;
            case TokenKind.SignalHann: EmitAnalysisIntArrayCall(expr, nameof(IlRuntime.Hann)); break;
            case TokenKind.SignalHamming: EmitAnalysisIntArrayCall(expr, nameof(IlRuntime.Hamming)); break;
            case TokenKind.SignalBlackman: EmitAnalysisIntArrayCall(expr, nameof(IlRuntime.Blackman)); break;
            // Filtri IIR (SIGNAL): design → handle; LFILTER/FILTFILT/FREQZ applicano.
            case TokenKind.SignalButter: EmitSignalDesignCall(expr, nameof(IlRuntime.SignalButter), 3); break;
            case TokenKind.SignalCheby1: EmitSignalDesignCall(expr, nameof(IlRuntime.SignalCheby1), 4); break;
            case TokenKind.SignalCheby2: EmitSignalDesignCall(expr, nameof(IlRuntime.SignalCheby2), 4); break;
            case TokenKind.SignalEllip: EmitSignalDesignCall(expr, nameof(IlRuntime.SignalEllip), 5); break;
            case TokenKind.SignalLfilter: EmitSignalApplyCall(expr, nameof(IlRuntime.SignalLfilter)); break;
            case TokenKind.SignalFiltfilt: EmitSignalApplyCall(expr, nameof(IlRuntime.SignalFiltfilt)); break;
            case TokenKind.SignalFreqz: EmitSignalFreqzCall(expr, nameof(IlRuntime.SignalFreqz)); break;
            // Wrapper TIMESERIES sui filtri SIGNAL (design+apply).
            case TokenKind.TsLowPass: EmitTsFilterCall(expr, nameof(IlRuntime.TimeSeriesLowPass), 3); break;
            case TokenKind.TsHighPass: EmitTsFilterCall(expr, nameof(IlRuntime.TimeSeriesHighPass), 3); break;
            case TokenKind.TsBandPass: EmitTsFilterCall(expr, nameof(IlRuntime.TimeSeriesBandPass), 4); break;
            case TokenKind.TsBandStop: EmitTsFilterCall(expr, nameof(IlRuntime.TimeSeriesBandStop), 4); break;
            case TokenKind.Ifft: EmitAnalysisMatrixCall(expr, nameof(IlRuntime.Ifft)); break;
            case TokenKind.FftComplex: EmitAnalysisComplexCall(expr, nameof(IlRuntime.FftComplex)); break;
            case TokenKind.IfftComplex: EmitAnalysisComplexCall(expr, nameof(IlRuntime.IfftComplex)); break;
            case TokenKind.Integrate: EmitAnalysisClosureTwoCall(expr, nameof(IlRuntime.Integrate)); break;
            case TokenKind.Differentiate: EmitAnalysisClosureOneCall(expr, nameof(IlRuntime.Differentiate)); break;
            case TokenKind.InterpolateClamp: EmitAnalysisInterpCall(expr, nameof(IlRuntime.InterpolateClamp)); break;
            case TokenKind.InterpolateGeneric: EmitAnalysisInterpGenericCall(expr, nameof(IlRuntime.InterpolateGeneric)); break;
            // --- Nuclei scipy.integrate/interpolate/fft completati (2026-08-25) ---
            case TokenKind.Simpson: EmitAnalysisClosureTwoCall2(expr, nameof(IlRuntime.Simpson)); break;
            case TokenKind.SolveOde: EmitAnalysisOdeCall(expr, nameof(IlRuntime.SolveOde)); break;
            case TokenKind.SolveIvp: EmitSolveIvpCall(expr); break;
            case TokenKind.SignalFirwin: EmitFirwinCall(expr); break;
            case TokenKind.SignalWelch: EmitSignalWelchCall(expr); break;
            case TokenKind.SignalSavgolFilter: EmitSignalIntIntCall(expr, nameof(IlRuntime.SavgolFilter)); break;
            case TokenKind.SignalResample: EmitSignalIntCall(expr, nameof(IlRuntime.Resample)); break;
            case TokenKind.SignalDecimate: EmitSignalIntCall(expr, nameof(IlRuntime.Decimate)); break;
            case TokenKind.InterpSplrep: EmitAnalysisTwoArrayCall(expr, nameof(IlRuntime.Splrep)); break;
            case TokenKind.InterpSplev: EmitSplevCall(expr); break;
            case TokenKind.SpatialPdist: EmitSpatialPdistCall(expr); break;
            case TokenKind.SpatialCdist: EmitSpatialMatrixCall(expr, nameof(IlRuntime.Cdist)); break;
            case TokenKind.SpatialDistanceMatrix: EmitSpatialMatrixCall(expr, nameof(IlRuntime.DistanceMatrix)); break;
            case TokenKind.FftRfft: EmitAnalysisArrayCall(expr, nameof(IlRuntime.Rfft)); break;
            case TokenKind.FftIrfft: EmitAnalysisTwoArrayCall(expr, nameof(IlRuntime.Irfft)); break;
            case TokenKind.SpatialConvexHull: EmitAnalysisMatrixCall(expr, nameof(IlRuntime.ConvexHull)); break;
            case TokenKind.SignalFindPeaks: EmitFindPeaksCall(expr); break;
            case TokenKind.Trapezoid: EmitAnalysisTwoArrayCall(expr, nameof(IlRuntime.Trapezoid)); break;
            case TokenKind.CumTrapezoid: EmitAnalysisTwoArrayCall(expr, nameof(IlRuntime.CumTrapezoid)); break;
            case TokenKind.CubicSpline: EmitAnalysisInterpCall(expr, nameof(IlRuntime.CubicSpline)); break;
            case TokenKind.FftFreq: EmitAnalysisIntDoubleCall(expr, nameof(IlRuntime.FftFreq)); break;
            case TokenKind.FftShift: EmitAnalysisArrayCall(expr, nameof(IlRuntime.FftShift)); break;
            case TokenKind.IfftShift: EmitAnalysisArrayCall(expr, nameof(IlRuntime.IfftShift)); break;
            // --- Importato da GMBasic (CONSOLE-GRAFICA: SCREEN/POINT; headless = 0/-1) ---
            case TokenKind.Screen: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.Screen))); break;
            case TokenKind.Point:
                EmitDoubleBinaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.Point)));
                break;
            // --- Importato da GMBasic (CRYPTO ABI: ABIENCODE/ABIDECODE) ---
            case TokenKind.AbiEncode: EmitAbiEncodeCall(expr); break;
            case TokenKind.AbiDecode: EmitAbiDecodeCall(expr); break;
            case TokenKind.JarqueBeraTest: EmitTimeSeriesArrayOnlyCall(expr, nameof(IlRuntime.JarqueBeraTest)); break;
            case TokenKind.AnnualizedVolatility: EmitTimeSeriesArrayLongCall(expr, nameof(IlRuntime.AnnualizedVolatility)); break;
            case TokenKind.AnnualizedTrackingError: EmitTimeSeriesTwoArrayLongCall(expr, nameof(IlRuntime.AnnualizedTrackingError)); break;
            case TokenKind.AnnualizedDownsideDeviation: EmitTradingArrayDoubleLongCall(expr, nameof(IlRuntime.AnnualizedDownsideDeviation)); break;
            case TokenKind.OmegaRatio: EmitTimeSeriesArrayDoubleCall(expr, nameof(IlRuntime.OmegaRatio)); break;
            case TokenKind.UlcerIndex: EmitTimeSeriesArrayOnlyCall(expr, nameof(IlRuntime.UlcerIndex)); break;
            case TokenKind.StockValueZeroGrowth: EmitDoubleBinaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.StockValueZeroGrowth))); break;
            case TokenKind.StockValueConstGrowth: EmitDoubleArityBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.StockValueConstGrowth)), 3); break;
            case TokenKind.PreferredStockValue: EmitDoubleBinaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.PreferredStockValue))); break;
            case TokenKind.CAPM: EmitDoubleArityBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.Capm)), 3); break;
            case TokenKind.TaxEquivalentYield: EmitDoubleBinaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.TaxEquivalentYield))); break;
            case TokenKind.RiskPremium: EmitDoubleBinaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.RiskPremium))); break;
            case TokenKind.BondPrice: EmitBondCall(expr, nameof(IlRuntime.BondPrice)); break;
            case TokenKind.BondYield: EmitBondCall(expr, nameof(IlRuntime.BondYield)); break;
            case TokenKind.BondEquivalentYield: EmitDoubleArityBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.BondEquivalentYield)), 3); break;
            case TokenKind.CurrentYield: EmitDoubleBinaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.CurrentYield))); break;
            case TokenKind.YieldToMaturity: EmitDoubleArityBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.YieldToMaturity)), 4); break;
            case TokenKind.Yield:
                // TaskYield() — AWAIT TaskYield(): cede il controllo allo scheduler cooperativo.
                // In IL la call è sincrona (niente scheduler): no-op headless (come la VM).
                foreach (var a in expr.Args) { EmitExpression(a); _il.Emit(OpCodes.Pop); }
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.TaskYield)));
                break;
            case TokenKind.ZeroCouponBondValue: EmitDoubleArityBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.ZeroCouponBondValue)), 3); break;
            case TokenKind.ZeroCouponBondYield: EmitDoubleArityBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.ZeroCouponBondYield)), 3); break;
            case TokenKind.TotalStockReturn: EmitDoubleArityBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.TotalStockReturn)), 3); break;
            case TokenKind.CapitalGainsYield: EmitDoubleBinaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.CapitalGainsYield))); break;
            case TokenKind.HoldingPeriodReturn: EmitTimeSeriesArrayOnlyCall(expr, nameof(IlRuntime.HoldingPeriodReturn)); break;
            case TokenKind.GeometricMeanReturn: EmitTimeSeriesArrayOnlyCall(expr, nameof(IlRuntime.GeometricMeanReturn)); break;
            case TokenKind.GrowthRate: EmitDoubleBinaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.GrowthRate))); break;
            case TokenKind.BidAskSpread: EmitDoubleBinaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.BidAskSpread))); break;
            case TokenKind.BookValuePerShare: EmitDoubleBinaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.BookValuePerShare))); break;
            case TokenKind.EPS: EmitDoubleBinaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.Eps))); break;
            case TokenKind.DilutedEPS: EmitDoubleArityBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.DilutedEps)), 3); break;
            case TokenKind.DPS: EmitDoubleBinaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.Dps))); break;
            case TokenKind.EquityMultiplier: EmitDoubleBinaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.EquityMultiplier))); break;
            case TokenKind.NAV: EmitDoubleArityBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.Nav)), 3); break;
            case TokenKind.BsCall: EmitTradingOptionNumericCall(expr, nameof(IlRuntime.BsCall), 6, 0); break;
            case TokenKind.BsPut: EmitTradingOptionNumericCall(expr, nameof(IlRuntime.BsPut), 6, 0); break;
            case TokenKind.MertonCall: EmitTradingOptionNumericCall(expr, nameof(IlRuntime.MertonCall), 6, 0); break;
            case TokenKind.MertonPut: EmitTradingOptionNumericCall(expr, nameof(IlRuntime.MertonPut), 6, 0); break;
            case TokenKind.BinomialOption: EmitTradingOptionStringCall(expr, nameof(IlRuntime.BinomialOption), 6); break;
            case TokenKind.Delta: EmitTradingOptionStringCall(expr, nameof(IlRuntime.Delta), 5); break;
            case TokenKind.Gamma: EmitTradingOptionNumericCall(expr, nameof(IlRuntime.Gamma), 5); break;
            case TokenKind.Theta: EmitTradingOptionStringCall(expr, nameof(IlRuntime.Theta), 5); break;
            case TokenKind.Vega: EmitTradingOptionNumericCall(expr, nameof(IlRuntime.Vega), 5); break;
            case TokenKind.Rho: EmitTradingOptionStringCall(expr, nameof(IlRuntime.Rho), 5); break;
            case TokenKind.Vanna: EmitTradingOptionNumericCall(expr, nameof(IlRuntime.Vanna), 5); break;
            case TokenKind.Volga: EmitTradingOptionNumericCall(expr, nameof(IlRuntime.Volga), 5); break;
            case TokenKind.Charm: EmitTradingOptionStringCall(expr, nameof(IlRuntime.Charm), 5); break;
            case TokenKind.Zomma: EmitTradingOptionNumericCall(expr, nameof(IlRuntime.Zomma), 5); break;
            case TokenKind.DeltaSpeed: EmitTradingOptionNumericCall(expr, nameof(IlRuntime.DeltaSpeed), 5); break;
            case TokenKind.GammaColor: EmitTradingOptionNumericCall(expr, nameof(IlRuntime.GammaColor), 5); break;
            case TokenKind.Md5: EmitCryptoObjectUnaryCall(expr, nameof(IlRuntime.Md5)); break;
            case TokenKind.Sha1: EmitCryptoObjectUnaryCall(expr, nameof(IlRuntime.Sha1)); break;
            case TokenKind.Sha256: EmitCryptoObjectUnaryCall(expr, nameof(IlRuntime.Sha256)); break;
            case TokenKind.Sha512: EmitCryptoObjectUnaryCall(expr, nameof(IlRuntime.Sha512)); break;
            case TokenKind.StrMd5: EmitCryptoObjectUnaryCall(expr, nameof(IlRuntime.StrMd5)); break;
            case TokenKind.StrSha1: EmitCryptoObjectUnaryCall(expr, nameof(IlRuntime.StrSha1)); break;
            case TokenKind.StrSha256: EmitCryptoObjectUnaryCall(expr, nameof(IlRuntime.StrSha256)); break;
            case TokenKind.StrSha512: EmitCryptoObjectUnaryCall(expr, nameof(IlRuntime.StrSha512)); break;
            case TokenKind.HashFile: EmitCryptoHashFileCall(expr, nameof(IlRuntime.HashFile)); break;
            case TokenKind.StrHashFile: EmitCryptoHashFileCall(expr, nameof(IlRuntime.StrHashFile)); break;
            case TokenKind.AesEncrypt: EmitCryptoObjectAesCall(expr, nameof(IlRuntime.AesEncrypt)); break;
            case TokenKind.AesDecrypt: EmitCryptoObjectAesCall(expr, nameof(IlRuntime.AesDecrypt)); break;
            case TokenKind.RandomBytes: EmitInt64UnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.RandomBytes))); break;
            case TokenKind.ToBase64: EmitCryptoObjectUnaryCall(expr, nameof(IlRuntime.ToBase64)); break;
            case TokenKind.FromBase64: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.FromBase64))); break;
            case TokenKind.ToHex: EmitCryptoObjectUnaryCall(expr, nameof(IlRuntime.ToHex)); break;
            case TokenKind.FromHex: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.FromHex))); break;
            case TokenKind.HmacSha256: EmitCryptoObjectBinaryCall(expr, nameof(IlRuntime.HmacSha256)); break;
            case TokenKind.HmacSha384: EmitCryptoObjectBinaryCall(expr, nameof(IlRuntime.HmacSha384)); break;
            case TokenKind.HmacSha512: EmitCryptoObjectBinaryCall(expr, nameof(IlRuntime.HmacSha512)); break;
            case TokenKind.ToBase64Url: EmitCryptoObjectUnaryCall(expr, nameof(IlRuntime.ToBase64Url)); break;
            case TokenKind.FromBase64Url: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.FromBase64Url))); break;
            case TokenKind.RandomInt: EmitCryptoRandomIntCall(expr); break;
            case TokenKind.RandomDouble: EmitCryptoRandomDoubleCall(expr, nameof(IlRuntime.RandomDouble)); break;
            case TokenKind.RandomNormal: EmitCryptoRandomDoubleCall(expr, nameof(IlRuntime.RandomNormal)); break;
            case TokenKind.RandomCryptoBytes: EmitInt64UnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.RandomCryptoBytes))); break;
            case TokenKind.PasswordHash: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.PasswordHash))); break;
            case TokenKind.PasswordVerify: EmitStringBinaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.PasswordVerify))); break;
            case TokenKind.KeyGen: EmitCryptoKeyGenCall(expr); break;
            case TokenKind.KeyExport: EmitInt64UnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.KeyExport))); break;
            case TokenKind.KeyImport: EmitCryptoKeyImportCall(expr); break;
            case TokenKind.Sign: EmitCryptoObjectInt64Call(expr, nameof(IlRuntime.Sign)); break;
            case TokenKind.Verify: EmitCryptoObjectObjectInt64Call(expr, nameof(IlRuntime.Verify)); break;
            case TokenKind.VerifyRaw: EmitCryptoObjectObjectStringCall(expr, nameof(IlRuntime.VerifyRaw)); break;
            case TokenKind.EncryptAsym: EmitCryptoObjectInt64Call(expr, nameof(IlRuntime.EncryptAsym)); break;
            case TokenKind.DecryptAsym: EmitCryptoObjectInt64Call(expr, nameof(IlRuntime.DecryptAsym)); break;
            case TokenKind.Keccak256: EmitCryptoObjectUnaryCall(expr, nameof(IlRuntime.Keccak256)); break;
            case TokenKind.StrKeccak256: EmitCryptoObjectUnaryCall(expr, nameof(IlRuntime.StrKeccak256)); break;
            case TokenKind.RipeMd160: EmitCryptoObjectUnaryCall(expr, nameof(IlRuntime.RipeMd160)); break;
            case TokenKind.StrRipeMd160: EmitCryptoObjectUnaryCall(expr, nameof(IlRuntime.StrRipeMd160)); break;
            case TokenKind.Blake2b256: EmitCryptoObjectUnaryCall(expr, nameof(IlRuntime.Blake2b256)); break;
            case TokenKind.Base58Encode: EmitCryptoObjectUnaryCall(expr, nameof(IlRuntime.Base58Encode)); break;
            case TokenKind.Base58Decode: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.Base58Decode))); break;
            case TokenKind.Base32Encode: EmitCryptoObjectUnaryCall(expr, nameof(IlRuntime.Base32Encode)); break;
            case TokenKind.Base32Decode: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.Base32Decode))); break;
            case TokenKind.RlpEncode: EmitCryptoObjectUnaryCall(expr, nameof(IlRuntime.RlpEncode)); break;
            case TokenKind.VarIntEncode: EmitInt64UnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.VarIntEncode))); break;
            case TokenKind.VarIntDecode: EmitCryptoObjectUnaryCall(expr, nameof(IlRuntime.VarIntDecode)); break;
            case TokenKind.EthAddressFromPub: EmitCryptoObjectUnaryCall(expr, nameof(IlRuntime.EthAddressFromPub)); break;
            case TokenKind.EthAddressChecksum: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.EthAddressChecksum))); break;
            case TokenKind.BtcAddressFromPub: EmitCryptoObjectBoolCall(expr, nameof(IlRuntime.BtcAddressFromPub), false); break;
            case TokenKind.BlockDifficulty: EmitCryptoObjectInt64Call(expr, nameof(IlRuntime.BlockDifficulty)); break;
            case TokenKind.EcdsaSecp256k1Sign: EmitCryptoObjectBinaryCall(expr, nameof(IlRuntime.EcdsaSecp256k1Sign)); break;
            case TokenKind.MnemonicToSeed: EmitStringBinaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.MnemonicToSeed))); break;
            case TokenKind.GetPublicKey: EmitCryptoObjectUnaryCall(expr, nameof(IlRuntime.GetPublicKey)); break;
            case TokenKind.ChaCha20Encrypt: EmitCryptoObjectTernaryCall(expr, nameof(IlRuntime.ChaCha20Encrypt)); break;
            case TokenKind.ChaCha20Decrypt: EmitCryptoObjectTernaryCall(expr, nameof(IlRuntime.ChaCha20Decrypt)); break;
            case TokenKind.ChaCha20Poly1305Encrypt: EmitCryptoPolyEncryptCall(expr); break;
            case TokenKind.ChaCha20Poly1305Decrypt: EmitCryptoPolyDecryptCall(expr); break;
            case TokenKind.LinAlgMatrixCreateFunc: EmitLinAlgLongPairCall(expr, nameof(IlRuntime.MatrixCreate)); break;
            case TokenKind.LinAlgVectorCreateFunc: EmitLinAlgVectorCreateCall(expr); break;
            case TokenKind.LinAlgMultiplyFunc or TokenKind.Multiply: EmitLinAlgLongPairCall(expr, nameof(IlRuntime.MatrixMultiply)); break;
            case TokenKind.LinAlgInverseFunc or TokenKind.Inverse: EmitLinAlgUnaryHandleCall(expr, RuntimeMethod(nameof(IlRuntime.MatrixInverse)), vector: false); break;
            case TokenKind.LinAlgTransposeFunc or TokenKind.Transpose: EmitLinAlgUnaryHandleCall(expr, RuntimeMethod(nameof(IlRuntime.MatrixTranspose)), vector: false); break;
            case TokenKind.Determinant: EmitLinAlgUnaryHandleCall(expr, RuntimeMethod(nameof(IlRuntime.MatrixDeterminant)), vector: false); break;
            case TokenKind.MatrixPseudoInverse: EmitLinAlgUnaryHandleCall(expr, RuntimeMethod(nameof(IlRuntime.MatrixPseudoInverse)), vector: false); break;
            case TokenKind.MatrixExp: EmitLinAlgUnaryHandleCall(expr, RuntimeMethod(nameof(IlRuntime.MatrixExp)), vector: false); break;
            case TokenKind.MatrixKron: EmitLinAlgLongPairCall(expr, nameof(IlRuntime.MatrixKronecker)); break;
            case TokenKind.MatrixSqrt: EmitLinAlgUnaryHandleCall(expr, RuntimeMethod(nameof(IlRuntime.MatrixSqrt)), vector: false); break;
            case TokenKind.MatrixLogm: EmitLinAlgUnaryHandleCall(expr, RuntimeMethod(nameof(IlRuntime.MatrixLogm)), vector: false); break;
            case TokenKind.MatrixHessenberg: EmitLinAlgUnaryHandleCall(expr, RuntimeMethod(nameof(IlRuntime.MatrixHessenberg)), vector: false); break;
            case TokenKind.MatrixBlockDiag: EmitLinAlgLongPairCall(expr, nameof(IlRuntime.MatrixBlockDiag)); break;
            case TokenKind.MatrixNullSpace: EmitLinAlgUnaryHandleCall(expr, RuntimeMethod(nameof(IlRuntime.MatrixNullSpace)), vector: false); break;
            case TokenKind.MatrixSetValue: EmitLinAlgSetValueCall(expr); break;
            case TokenKind.MatrixGetValue: EmitLinAlgGetValueCall(expr); break;
            case TokenKind.MatrixCosm: EmitLinAlgUnaryHandleCall(expr, RuntimeMethod(nameof(IlRuntime.MatrixCosm)), vector: false); break;
            case TokenKind.MatrixSinm: EmitLinAlgUnaryHandleCall(expr, RuntimeMethod(nameof(IlRuntime.MatrixSinm)), vector: false); break;
            case TokenKind.MatrixTanm: EmitLinAlgUnaryHandleCall(expr, RuntimeMethod(nameof(IlRuntime.MatrixTanm)), vector: false); break;
            case TokenKind.MatrixSignm: EmitLinAlgUnaryHandleCall(expr, RuntimeMethod(nameof(IlRuntime.MatrixSignm)), vector: false); break;
            case TokenKind.MatrixOrth: EmitLinAlgUnaryHandleCall(expr, RuntimeMethod(nameof(IlRuntime.MatrixOrth)), vector: false); break;
            case TokenKind.MatrixPolar: EmitLinAlgUnaryHandleCall(expr, RuntimeMethod(nameof(IlRuntime.MatrixPolar)), vector: false); break;
            case TokenKind.MatrixEig: EmitLinAlgUnaryHandleCall(expr, RuntimeMethod(nameof(IlRuntime.MatrixEig)), vector: false); break;
            case TokenKind.MatrixSchur: EmitLinAlgUnaryHandleCall(expr, RuntimeMethod(nameof(IlRuntime.MatrixSchur)), vector: false); break;
            case TokenKind.MatrixCond: EmitLinAlgUnaryHandleCall(expr, RuntimeMethod(nameof(IlRuntime.MatrixCond)), vector: true); break;
            case TokenKind.MatrixNuclearNorm: EmitLinAlgUnaryHandleCall(expr, RuntimeMethod(nameof(IlRuntime.MatrixNuclearNorm)), vector: true); break;
            case TokenKind.MatrixFractionalPower:
                EmitInt64Argument(expr.Args[0]);
                EmitDoubleArgument(expr.Args[1]);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.MatrixFractionalPower)));
                break;
            case TokenKind.MatrixSubspaceAngles:
                if (expr.Args.Length != 2) throw new NotSupportedException("Backend IL: LINALG.SUBSPACEANGLES richiede 2 handle.");
                EmitObjectBoxedArgument(expr.Args[0]);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.LinAlgMatrixHandle), typeof(object)));
                EmitObjectBoxedArgument(expr.Args[1]);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.LinAlgMatrixHandle), typeof(object)));
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.MatrixSubspaceAngles)));
                break;
            case TokenKind.MatrixToeplitz:
                if (expr.Args.Length != 2) throw new NotSupportedException("Backend IL: MATRIX.TOEPLITZ richiede 2 array.");
                EmitDoubleArrayArgument(expr.Args[0]);
                EmitDoubleArrayArgument(expr.Args[1]);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.MatrixToeplitz)));
                break;
            case TokenKind.MatrixHankel:
                if (expr.Args.Length != 2) throw new NotSupportedException("Backend IL: MATRIX.HANKEL richiede 2 array.");
                EmitDoubleArrayArgument(expr.Args[0]);
                EmitDoubleArrayArgument(expr.Args[1]);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.MatrixHankel)));
                break;
            case TokenKind.MatrixCirculant:
                if (expr.Args.Length != 1) throw new NotSupportedException("Backend IL: MATRIX.CIRCULANT richiede 1 array.");
                EmitDoubleArrayArgument(expr.Args[0]);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.MatrixCirculant)));
                break;
            case TokenKind.MatrixSolveTriangular:
                if (expr.Args.Length != 3) throw new NotSupportedException("Backend IL: LINALG.SOLVETRIANGULAR richiede handle, handle, lower.");
                EmitObjectBoxedArgument(expr.Args[0]);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.LinAlgMatrixHandle), typeof(object)));
                EmitObjectBoxedArgument(expr.Args[1]);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.LinAlgVectorHandle), typeof(object)));
                EmitInt64Argument(expr.Args[2]);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.MatrixSolveTriangular)));
                break;
            case TokenKind.MatrixRank: EmitLinAlgUnaryHandleCall(expr, RuntimeMethod(nameof(IlRuntime.MatrixRank)), vector: true); break;
            case TokenKind.MatrixTrace: EmitLinAlgUnaryHandleCall(expr, RuntimeMethod(nameof(IlRuntime.MatrixTrace)), vector: true); break;
            case TokenKind.MatrixNorm: EmitLinAlgUnaryHandleCall(expr, RuntimeMethod(nameof(IlRuntime.MatrixNorm)), vector: true); break;
            case TokenKind.Solve: EmitLinAlgSolveCall(expr); break;
            case TokenKind.DotProduct: EmitLinAlgSolveCall(expr, vectorDot: true); break;
            case TokenKind.Norm: EmitLinAlgUnaryHandleCall(expr, RuntimeMethod(nameof(IlRuntime.VectorNorm)), vector: true); break;
            case TokenKind.VectorManhattan: EmitLinAlgLongPairCall(expr, nameof(IlRuntime.VectorManhattan)); break;
            case TokenKind.VectorChebyshev: EmitLinAlgLongPairCall(expr, nameof(IlRuntime.VectorChebyshev)); break;
            case TokenKind.VectorCosineDistance: EmitLinAlgLongPairCall(expr, nameof(IlRuntime.VectorCosineDistance)); break;
            case TokenKind.VectorMinkowski: EmitLinAlgLongPairDoubleCall(expr, nameof(IlRuntime.VectorMinkowski)); break;
            // KDTree (scipy.spatial.KDTree): CREATE(points)→handle; QUERY/QUERYDIST(h,x,k);
            // QUERYBALL(h,x,r).
            case TokenKind.KdTreeCreate: EmitKdTreeCreateCall(expr); break;
            case TokenKind.KdTreeQuery: EmitKdTreeQueryCall(expr, nameof(IlRuntime.KdTreeQuery)); break;
            case TokenKind.KdTreeQueryDist: EmitKdTreeQueryCall(expr, nameof(IlRuntime.KdTreeQueryDist)); break;
            case TokenKind.KdTreeQueryBall: EmitKdTreeQueryCall(expr, nameof(IlRuntime.KdTreeQueryBall)); break;
            case TokenKind.LinAlgLuFunc or TokenKind.Lu: EmitLinAlgUnaryHandleCall(expr, RuntimeMethod(nameof(IlRuntime.MatrixLu)), vector: false); break;
            case TokenKind.LinAlgQrFunc or TokenKind.Qr: EmitLinAlgUnaryHandleCall(expr, RuntimeMethod(nameof(IlRuntime.MatrixQr)), vector: false); break;
            case TokenKind.LinAlgSvdFunc or TokenKind.Svd: EmitLinAlgUnaryHandleCall(expr, RuntimeMethod(nameof(IlRuntime.MatrixSvd)), vector: false); break;
            case TokenKind.LinAlgEvdFunc or TokenKind.Evd: EmitLinAlgUnaryHandleCall(expr, RuntimeMethod(nameof(IlRuntime.MatrixEvd)), vector: false); break;
            case TokenKind.LinAlgCholeskyFunc or TokenKind.Cholesky: EmitLinAlgUnaryHandleCall(expr, RuntimeMethod(nameof(IlRuntime.MatrixCholesky)), vector: false); break;
            case TokenKind.LinAlgDecompGetFactorFunc: EmitLinAlgDecompGetFactorCall(expr); break;
            case TokenKind.InterpolateLinear: EmitInterpolationCall(expr, nameof(IlRuntime.InterpolateLinear)); break;
            case TokenKind.InterpolateSpline: EmitInterpolationCall(expr, nameof(IlRuntime.InterpolateSpline)); break;
            case TokenKind.InterpolatePolynomial: EmitInterpolationCall(expr, nameof(IlRuntime.InterpolatePolynomial)); break;
            case TokenKind.InterpolatePchip: EmitInterpolationCall(expr, nameof(IlRuntime.InterpolatePchip)); break;
            case TokenKind.InterpolateStep: EmitInterpolationCall(expr, nameof(IlRuntime.InterpolateStep)); break;
            case TokenKind.Sys: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.Sys))); break;
            case TokenKind.Shell: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.SystemShell))); break;
            case TokenKind.ShellOut: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.SystemExec))); break;
            case TokenKind.ShellCode: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.SystemExecCode))); break;
            case TokenKind.GoMaxProcs:
                // Argomento opzionale (`double n = 0`): con 0 argomenti va spinto 0.0, altrimenti `call` a stack vuoto.
                if (expr.Args.Length == 0) _il.Emit(OpCodes.Ldc_R8, 0.0);
                EmitNumericCall(expr, RuntimeMethod(nameof(IlRuntime.GoMaxProcs)));
                break;
            // G8-bis: POOL/SYNCMAP/ATOMIC/SYSTEM.NUMGOROUTINE/NUMCPU (parità PCodeVM).
            case TokenKind.Next:
                // NEXT(generatore): prossimo valore (object) o BasicException(StopIteration) a fine corsa.
                EmitExpression(expr.Args[0]);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.GeneratorNext), typeof(object)));
                break;
            case TokenKind.PoolGet:
                EmitExpression(expr.Args[0]);
                _il.Emit(OpCodes.Castclass, typeof(PCodePool));
                _il.Emit(OpCodes.Callvirt, PoolGetMethod);
                break;
            case TokenKind.PoolPut:
                EmitExpression(expr.Args[0]);
                _il.Emit(OpCodes.Castclass, typeof(PCodePool));
                EmitExpression(expr.Args[1]);
                {
                    // PCodePool.Put(object): un valore numerico (double/long…) va BOXATO, altrimenti
                    // IL invalido («invalid program») con `POOL.PUT(p, 42)`; le stringhe sono già object.
                    var poolItemClr = DataTypeMapper.Map(NormalizeInferredType(InferType(expr.Args[1])), _types, _classTypes);
                    if (poolItemClr.IsValueType) _il.Emit(OpCodes.Box, poolItemClr);
                }
                _il.Emit(OpCodes.Callvirt, PoolPutMethod);
                break;
            case TokenKind.SyncMapStore:
                EmitExpression(expr.Args[0]);
                _il.Emit(OpCodes.Castclass, typeof(PCodeConcurrentMap));
                EmitExpression(expr.Args[1]);
                EmitExpression(expr.Args[2]);
                _il.Emit(OpCodes.Callvirt, ConcurrentMapStoreMethod);
                break;
            case TokenKind.SyncMapLoad:
                EmitExpression(expr.Args[0]);
                _il.Emit(OpCodes.Castclass, typeof(PCodeConcurrentMap));
                EmitExpression(expr.Args[1]);
                _il.Emit(OpCodes.Callvirt, ConcurrentMapLoadMethod);
                break;
            case TokenKind.SyncMapDelete:
                EmitExpression(expr.Args[0]);
                _il.Emit(OpCodes.Castclass, typeof(PCodeConcurrentMap));
                EmitExpression(expr.Args[1]);
                _il.Emit(OpCodes.Callvirt, ConcurrentMapDeleteMethod);
                break;
            case TokenKind.SyncMapLoadOrStore:
                EmitExpression(expr.Args[0]);
                _il.Emit(OpCodes.Castclass, typeof(PCodeConcurrentMap));
                EmitExpression(expr.Args[1]);
                EmitExpression(expr.Args[2]);
                _il.Emit(OpCodes.Callvirt, ConcurrentMapLoadOrStoreMethod);
                break;
            case TokenKind.AtomicSwap:
                EmitExpression(expr.Args[0]);
                _il.Emit(OpCodes.Castclass, typeof(PCodeAtomic));
                EmitExpression(expr.Args[1]);
                _il.Emit(OpCodes.Callvirt, AtomicSwapMethod);
                break;
            case TokenKind.AtomicStore:
                EmitExpression(expr.Args[0]);
                _il.Emit(OpCodes.Castclass, typeof(PCodeAtomic));
                EmitExpression(expr.Args[1]);
                _il.Emit(OpCodes.Callvirt, AtomicStoreMethod);
                break;
            case TokenKind.AtomicFetchValue:
                EmitExpression(expr.Args[0]);
                _il.Emit(OpCodes.Castclass, typeof(PCodeAtomic));
                _il.Emit(OpCodes.Callvirt, AtomicLoadMethod);
                break;
            case TokenKind.AtomicIncrement:
                EmitExpression(expr.Args[0]);
                _il.Emit(OpCodes.Castclass, typeof(PCodeAtomic));
                EmitExpression(expr.Args[1]);
                _il.Emit(OpCodes.Callvirt, AtomicAddMethod);
                break;
            case TokenKind.AtomicCompareSwap:
                EmitExpression(expr.Args[0]);
                _il.Emit(OpCodes.Castclass, typeof(PCodeAtomic));
                EmitExpression(expr.Args[1]);
                EmitExpression(expr.Args[2]);
                _il.Emit(OpCodes.Callvirt, AtomicCompareSwapMethod);
                break;
            case TokenKind.NumGoroutine: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.NumGoroutine))); break;
            case TokenKind.NumCpu: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.NumCpu))); break;
            case TokenKind.Empty: EmitCryptoObjectUnaryCall(expr, nameof(IlRuntime.Empty)); break;
            case TokenKind.TypeName: EmitCryptoObjectUnaryCall(expr, nameof(IlRuntime.TypeName)); break;
            case TokenKind.SizeOf: EmitSizeOfCall(expr); break;
            case TokenKind.CommandCount: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.CommandCount))); break;
            case TokenKind.Command: EmitInt64UnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.Command))); break;
            case TokenKind.EnvGet:
                EmitStringArg(expr.Args[0]);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.EnvGet), typeof(string)));
                break;
            case TokenKind.EnvHas:
                EmitStringArg(expr.Args[0]);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.EnvHas), typeof(string)));
                break;
            case TokenKind.EofFunc:
                // EOF() è condiviso: FILE.EOF(n) usa Eof(long) sugli handle file; ISAM.EOF/
                // DBF.EOF/RDD.EOF(alias) usano DbfEof(alias) (con fallback alle aree ISAM);
                // SPEED.EOF(alias) usa SpeedEof(alias) — senza questo ramo cadeva su Eof(long)
                // (handle FILE) → "Handle file non valido".
                if (expr.Namespace is { } eofNs
                    && eofNs.Lexeme.Equals("SPEED", StringComparison.OrdinalIgnoreCase))
                    EmitDbfAliasCall(expr, nameof(IlRuntime.SpeedEof));
                else if (expr.Namespace is { } eofNs2
                    && (eofNs2.Lexeme.Equals("ISAM", StringComparison.OrdinalIgnoreCase)
                        || eofNs2.Lexeme.Equals("DBF", StringComparison.OrdinalIgnoreCase)
                        || eofNs2.Lexeme.Equals("RDD", StringComparison.OrdinalIgnoreCase)))
                    EmitDbfAliasCall(expr, nameof(IlRuntime.DbfEof));
                else
                    // EOF(file-number) (stile legacy / USING): mappa al file number, non all'handle.
                    EmitInt64UnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.FileEofNumber)));
                break;
            case TokenKind.Lof: EmitInt64UnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.FileLofNumber))); break;
            // --- DBF builtin (porting da GMBasic/GMBasicAI): BOF/FOUND/RECNO/LASTREC/
            //     SELECTED/DELETED/LUPDATE/DBFILTER — 0 o 1 argomento (alias opzionale).
            case TokenKind.Bof: EmitDbfAliasCall(expr, nameof(IlRuntime.DbfBof)); break;
            case TokenKind.Used: EmitDbfAliasCall(expr, nameof(IlRuntime.DbfUsed)); break;
            // ALIAS() ritorna una STRINGA (il nome dell'area): va emessa con il marshalling
            // tipizzato, non con EmitDbfAliasCall che e' pensato per le funzioni DBF che
            // ritornano double (BOF/USED/FOUND/RECNO...). Con la helper sbagliata la stringa
            // veniva interpretata come numero e stampata come indirizzo (es. 4407803608).
            // Stesso difetto di DBRELATION (§ MiniTests/dbf/dbf_state.bas).
            case TokenKind.Alias: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.DbfAlias), typeof(string))); break;
            case TokenKind.Found: EmitDbfAliasCall(expr, nameof(IlRuntime.DbfFound)); break;
            case TokenKind.Recno: EmitDbfAliasCallNum(expr, nameof(IlRuntime.DbfRecno)); break;
            case TokenKind.Lastrec: EmitDbfAliasCallNum(expr, nameof(IlRuntime.DbfLastrec)); break;
            case TokenKind.Selected: EmitDbfAliasCallNum(expr, nameof(IlRuntime.DbfSelected)); break;
            case TokenKind.Deleted: EmitDbfAliasCall(expr, nameof(IlRuntime.DbfDeleted)); break;
            case TokenKind.LUpdate: EmitDbfAliasCall(expr, nameof(IlRuntime.DbfLupdate)); break;
            case TokenKind.DbFilter: EmitDbfAliasCall(expr, nameof(IlRuntime.DbfFilter)); break;
            case TokenKind.RLock or TokenKind.RddRLock: EmitRddLockCall(expr, nameof(IlRuntime.RddRLock)); break;
            case TokenKind.FLock or TokenKind.RddFLock: EmitRddLockCall(expr, nameof(IlRuntime.RddFLock)); break;
            case TokenKind.RddGetField:
                if (expr.Args.Length != 2) throw new NotSupportedException("Backend IL: RDD.GETFIELD richiede 2 argomenti (area, campo).");
                EmitBoxedArg(expr.Args[0]);
                EmitBoxedArg(expr.Args[1]);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.RddGetField), typeof(object), typeof(object)));
                break;
            case TokenKind.Ctod or TokenKind.RddCtod:
                if (expr.Args.Length != 1) throw new NotSupportedException("Backend IL: CTOD richiede 1 argomento.");
                EmitBoxedArg(expr.Args[0]);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.RddCtod), typeof(object)));
                break;
            case TokenKind.Dtoc or TokenKind.RddDtoc:
                if (expr.Args.Length != 1) throw new NotSupportedException("Backend IL: DTOC richiede 1 argomento.");
                EmitBoxedArg(expr.Args[0]);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.RddDtoc), typeof(object)));
                break;
            case TokenKind.RddCurval:
                if (expr.Args.Length != 1) throw new NotSupportedException("Backend IL: CURVAL richiede 1 argomento.");
                EmitBoxedArg(expr.Args[0]);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.RddCurval), typeof(object)));
                break;
            // --- RDD function form (RDD.DELETE/RECALL/REPLACE/BUFFER/SETDELETED/OLDVAL/...) ---
            case TokenKind.RddDeleteScoped:
                if (expr.Args.Length != 2) throw new NotSupportedException("Backend IL: RDD.DELETE richiede 2 argomenti.");
                EmitBoxedArg(expr.Args[0]); EmitBoxedArg(expr.Args[1]);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.RddDeleteScoped), typeof(object), typeof(object)));
                break;
            case TokenKind.RddRecallScoped:
                if (expr.Args.Length != 2) throw new NotSupportedException("Backend IL: RDD.RECALL richiede 2 argomenti.");
                EmitBoxedArg(expr.Args[0]); EmitBoxedArg(expr.Args[1]);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.RddRecallScoped), typeof(object), typeof(object)));
                break;
            case TokenKind.RddReplaceScoped:
                if (expr.Args.Length != 4) throw new NotSupportedException("Backend IL: RDD.REPLACE richiede 4 argomenti.");
                EmitBoxedArg(expr.Args[0]); EmitBoxedArg(expr.Args[1]); EmitBoxedArg(expr.Args[2]); EmitBoxedArg(expr.Args[3]);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.RddReplaceScoped), typeof(object), typeof(object), typeof(object), typeof(object)));
                break;
            case TokenKind.RddOldVal:
                if (expr.Args.Length != 2) throw new NotSupportedException("Backend IL: RDD.OLDVAL richiede 2 argomenti.");
                EmitBoxedArg(expr.Args[0]); EmitBoxedArg(expr.Args[1]);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.RddOldVal), typeof(object), typeof(object)));
                break;
            case TokenKind.RddGetFieldState:
                if (expr.Args.Length != 2) throw new NotSupportedException("Backend IL: RDD.GETFLDSTATE richiede 2 argomenti.");
                EmitBoxedArg(expr.Args[0]); EmitBoxedArg(expr.Args[1]);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.RddGetFieldState), typeof(object), typeof(object)));
                break;
            case TokenKind.RddValType:
                if (expr.Args.Length != 1) throw new NotSupportedException("Backend IL: RDD.VALTYPE richiede 1 argomento.");
                EmitBoxedArg(expr.Args[0]);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.RddValType), typeof(object)));
                break;
            case TokenKind.RddTableRevert:
                if (expr.Args.Length != 1) throw new NotSupportedException("Backend IL: RDD.TABLEREVERT richiede 1 argomento.");
                EmitBoxedArg(expr.Args[0]);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.RddTableRevert), typeof(object)));
                break;
            case TokenKind.RddTableUpdate:
                if (expr.Args.Length != 1) throw new NotSupportedException("Backend IL: RDD.TABLEUPDATE richiede 1 argomento.");
                EmitBoxedArg(expr.Args[0]);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.RddTableUpdate), typeof(object)));
                break;
            case TokenKind.RddBuffer:
                if (expr.Args.Length != 2) throw new NotSupportedException("Backend IL: RDD.BUFFER richiede 2 argomenti.");
                EmitBoxedArg(expr.Args[0]);
                EmitBoxedArg(expr.Args[1]);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.RddBuffer), typeof(object), typeof(object)));
                break;
            case TokenKind.RddSetDeleted:
                if (expr.Args.Length != 2) throw new NotSupportedException("Backend IL: RDD.SETDELETED richiede 2 argomenti.");
                EmitBoxedArg(expr.Args[0]);
                EmitBoxedArg(expr.Args[1]);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.RddSetDeleted), typeof(object), typeof(object)));
                _il.Emit(OpCodes.Conv_I4);
                _il.Emit(OpCodes.Neg);   // true → -1 (verità BASIC), come PCode/C
                _il.Emit(OpCodes.Conv_R8);
                break;
            case TokenKind.DbfSetDurability:
                if (expr.Args.Length != 1) throw new NotSupportedException("Backend IL: DBF.SETDURABILITY richiede 1 argomento (mode).");
                EmitStringArg(expr.Args[0]);
                _il.Emit(OpCodes.Ldnull);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.DbfSetDurability), typeof(string), typeof(string)));
                break;
            case TokenKind.DbfFlushDurability:
                _il.Emit(OpCodes.Ldnull);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.DbfFlushDurability), typeof(string)));
                break;
            case TokenKind.DbfBeginGroupCommit:
                _il.Emit(OpCodes.Ldnull);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.DbfBeginGroupCommit), typeof(string)));
                break;
            case TokenKind.DbfEndGroupCommit:
                _il.Emit(OpCodes.Ldnull);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.DbfEndGroupCommit), typeof(string)));
                break;
            case TokenKind.DbfAppendMany:
                /* La StdLib dichiara TRE forme: APPENDMANY(records), (fileNumber, records),
                 * (alias, records). La lista è sempre l'ULTIMO argomento; l'area (se c'è) il
                 * primo. Prima la forma a 1 argomento lanciava "richiede 2 argomenti". */
                if (expr.Args.Length is < 1 or > 2) throw new NotSupportedException("Backend IL: DBF.APPENDMANY accetta 1 o 2 argomenti (records [, area]).");
                EmitBoxedArg(expr.Args[expr.Args.Length - 1]);
                if (expr.Args.Length == 2 && expr.Args[0] is VariableExpr dbaArea) _il.Emit(OpCodes.Ldstr, dbaArea.NameCleaned.Lexeme);
                else _il.Emit(OpCodes.Ldnull);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.DbfAppendMany), typeof(object), typeof(string)));
                break;
            case TokenKind.DbfFollowReplication:
                if (expr.Args.Length != 1) throw new NotSupportedException("Backend IL: DBF.FOLLOWREPLICATION richiede 1 argomento (replLogPath).");
                EmitStringArg(expr.Args[0]);
                _il.Emit(OpCodes.Ldnull);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.DbfFollowReplication), typeof(string), typeof(string)));
                break;
            case TokenKind.DbfApplyReplication:
                /* La StdLib dichiara DUE forme: APPLYREPLICATION(logPath) e
                 * APPLYREPLICATION(fileNumber|alias, logPath). Qui si accettano entrambe: il
                 * path è l'ULTIMO argomento, l'area (se c'è) il primo. Prima la forma a 1
                 * argomento lanciava "richiede 2 argomenti" (§ MiniTests/dbf/dbf_replication.bas). */
                if (expr.Args.Length is < 1 or > 2) throw new NotSupportedException("Backend IL: DBF.APPLYREPLICATION accetta 1 o 2 argomenti (replLogPath [, area]).");
                EmitStringArg(expr.Args[expr.Args.Length - 1]);
                if (expr.Args.Length == 2 && expr.Args[0] is VariableExpr dbraArea) _il.Emit(OpCodes.Ldstr, dbraArea.NameCleaned.Lexeme);
                else _il.Emit(OpCodes.Ldnull);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.DbfApplyReplication), typeof(string), typeof(string)));
                break;
            case TokenKind.DbfSetReplication:
                /* Anche SETREPLICATION ha due forme nella StdLib: (logPath) e (alias|fileNumber, logPath). */
                if (expr.Args.Length is < 1 or > 2) throw new NotSupportedException("Backend IL: DBF.SETREPLICATION accetta 1 o 2 argomenti (logPath [, area]).");
                EmitStringArg(expr.Args[expr.Args.Length - 1]);
                if (expr.Args.Length == 2 && expr.Args[0] is VariableExpr dbrArea) _il.Emit(OpCodes.Ldstr, dbrArea.NameCleaned.Lexeme);
                else _il.Emit(OpCodes.Ldnull);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.DbfSetReplication), typeof(string), typeof(string)));
                break;
            case TokenKind.SpeedSetDurability:
                /* StdLib: (mode) e (alias, mode) — il modo è l'ULTIMO argomento. */
                if (expr.Args.Length is < 1 or > 2) throw new NotSupportedException("Backend IL: SPEED.SETDURABILITY accetta 1 o 2 argomenti (mode [, area]).");
                EmitStringArg(expr.Args[expr.Args.Length - 1]);
                if (expr.Args.Length == 2 && expr.Args[0] is VariableExpr spdArea) _il.Emit(OpCodes.Ldstr, spdArea.NameCleaned.Lexeme);
                else _il.Emit(OpCodes.Ldnull);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.SpeedSetDurability), typeof(string), typeof(string)));
                break;
            case TokenKind.SpeedFlushDurability:
                if (expr.Args.Length > 0) EmitStringArg(expr.Args[0]); else _il.Emit(OpCodes.Ldnull);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.SpeedFlushDurability), typeof(string)));
                break;
            case TokenKind.SpeedBeginGroupCommit:
                if (expr.Args.Length > 0) EmitStringArg(expr.Args[0]); else _il.Emit(OpCodes.Ldnull);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.SpeedBeginGroupCommit), typeof(string)));
                break;
            case TokenKind.SpeedEndGroupCommit:
                if (expr.Args.Length > 0) EmitStringArg(expr.Args[0]); else _il.Emit(OpCodes.Ldnull);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.SpeedEndGroupCommit), typeof(string)));
                break;
            case TokenKind.IsamAppendMany:
                /* Forme della StdLib: (records), (fileNumber, records), (alias, records):
                 * i record sono l'ULTIMO argomento. */
                if (expr.Args.Length is < 1 or > 2) throw new NotSupportedException("Backend IL: ISAM.APPENDMANY accetta 1 o 2 argomenti (records [, area]).");
                EmitBoxedArg(expr.Args[expr.Args.Length - 1]);
                if (expr.Args.Length == 2 && expr.Args[0] is VariableExpr isaaArea) _il.Emit(OpCodes.Ldstr, isaaArea.NameCleaned.Lexeme);
                else _il.Emit(OpCodes.Ldnull);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.IsamAppendMany), typeof(object), typeof(string)));
                _il.Emit(OpCodes.Conv_I8);   // StdLib: APPENDMANY → LONG; il metodo runtime ritorna double (bit riletti come int64 = 4.6e18)
                break;
            case TokenKind.IsamApplyReplication:
                /* La StdLib dichiara DUE forme: (replLogPath) e (fileNumber|alias, replLogPath):
                 * il path è l'ULTIMO argomento, l'area il primo (se c'è). Prima la forma a 1
                 * argomento lanciava "richiede 2 argomenti".
                 * La StdLib dichiara DUE forme: il path è l'ULTIMO, l'area il primo. */
                if (expr.Args.Length is < 1 or > 2) throw new NotSupportedException("Backend IL: ISAM.APPLYREPLICATION accetta 1 o 2 argomenti (replLogPath [, area]).");
                EmitStringArg(expr.Args[expr.Args.Length - 1]);
                if (expr.Args.Length == 2 && expr.Args[0] is VariableExpr isarArea) _il.Emit(OpCodes.Ldstr, isarArea.NameCleaned.Lexeme);
                else _il.Emit(OpCodes.Ldnull);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.IsamApplyReplication), typeof(string), typeof(string)));
                break;
            case TokenKind.SpeedSetMany:
                if (expr.Args.Length != 2) throw new NotSupportedException("Backend IL: SPEED.SETMANY richiede 2 argomenti (area, lista).");
                EmitBoxedArg(expr.Args[1]);
                if (expr.Args[0] is VariableExpr ssArea) _il.Emit(OpCodes.Ldstr, ssArea.NameCleaned.Lexeme);
                else _il.Emit(OpCodes.Ldnull);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.SpeedSetMany), typeof(object), typeof(string)));
                break;
            case TokenKind.IsamIntegrity:
                EmitDbfAliasCall(expr, nameof(IlRuntime.IsamIntegrity));
                break;
            case TokenKind.IsamInfo:
                EmitDbfAliasCall(expr, nameof(IlRuntime.IsamInfo));
                break;
            case TokenKind.RecSize:
                // Ritorna LONG (tipo dichiarato LongType): box Int64 del chiamante —
                // NO conv.r8 (EmitDbfAliasCallNum), un double renderebbe l'IL invalido.
                EmitDbfAliasCall(expr, nameof(IlRuntime.IsamRecSize));
                break;
            case TokenKind.IsamSetReplication:
                /* La StdLib dichiara DUE forme: (logPath) e (fileNumber|alias, logPath):
                 * il path è l'ULTIMO argomento, l'area il primo (se c'è). Prima la forma a 1
                 * argomento lanciava "richiede 2 argomenti".
                 * La StdLib dichiara DUE forme: il path è l'ULTIMO, l'area il primo. */
                if (expr.Args.Length is < 1 or > 2) throw new NotSupportedException("Backend IL: ISAM.SETREPLICATION accetta 1 o 2 argomenti (logPath [, area]).");
                EmitStringArg(expr.Args[expr.Args.Length - 1]);
                if (expr.Args.Length == 2 && expr.Args[0] is VariableExpr irArea) _il.Emit(OpCodes.Ldstr, irArea.NameCleaned.Lexeme);
                else _il.Emit(OpCodes.Ldnull);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.IsamSetReplication), typeof(string), typeof(string)));
                break;
            case TokenKind.FreeFile: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.FreeFile))); break;
            case TokenKind.FileInput: EmitIoCountHandleCall(expr, nameof(IlRuntime.FileInput)); break;
            case TokenKind.CwdFunc: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.Cwd))); break;
            case TokenKind.FileExistsFunc: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.FileExists))); break;
            case TokenKind.DirExistsFunc: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.DirExists))); break;
            case TokenKind.GetFolderPathFunc:
                if (expr.Args.Length <= 1)
                    EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.GetFolderPath), typeof(string)));
                else
                {
                    EmitExpression(expr.Args[0]);
                    EmitDoubleArgument(expr.Args[1]);
                    _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.GetFolderPath), typeof(string), typeof(double)));
                }
                break;
            case TokenKind.NormalizePathFunc: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.NormalizePath))); break;
            case TokenKind.ResolveFilePathFunc: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.ResolveFilePath))); break;
            case TokenKind.FolderSizeFunc: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.FolderSize))); break;
            case TokenKind.AppPathFunc: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AppPath))); break;
            case TokenKind.GetTempFileFunc: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.TempFile))); break;
            case TokenKind.BinPathFunc: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.BinPath))); break;
            case TokenKind.ProjectFolderFunc: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.ProjectFolder))); break;
            case TokenKind.ParentDirFunc: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.ParentDir))); break;
            case TokenKind.ReadBytesFunc: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.ReadBytes))); break;
            case TokenKind.WriteBytesFunc:
                // WRITEBYTES(path, bytes): il secondo argomento è un byte[] (tipo riferimento),
                // quindi passa all'`object` del runtime senza box; il ritorno long si coerce al
                // tipo inferito come per le altre funzioni IO.
                if (expr.Args.Length != 2)
                    throw new NotSupportedException("Backend IL: IO.WRITEBYTES richiede 2 argomenti (path, bytes).");
                EmitStringArg(expr.Args[0]);
                EmitExpression(expr.Args[1]);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.WriteBytes), typeof(string), typeof(object)));
                EmitCoerceTypes(new LongType(), NormalizeInferredType(InferType(expr)));
                break;
            case TokenKind.TtRenderFunc:
            {
                // FONTS.TTRENDER(path, size, codepoint) -> byte[] (alpha 0..255).
                if (expr.Args.Length != 3)
                    throw new NotSupportedException("Backend IL: FONTS.TTRENDER richiede 3 argomenti (path, size, codepoint).");
                EmitStringArg(expr.Args[0]);
                EmitExpression(expr.Args[1]);
                EmitCoerceTypes(NormalizeInferredType(InferType(expr.Args[1])), new DoubleType());
                EmitExpression(expr.Args[2]);
                EmitCoerceTypes(NormalizeInferredType(InferType(expr.Args[2])), new LongType());
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.TtRender), typeof(string), typeof(double), typeof(long)));
                EmitCoerceTypes(new ArrayType(new ByteType()), NormalizeInferredType(InferType(expr)));
                break;
            }
            case TokenKind.TtMetricsFunc:
            {
                if (expr.Args.Length != 3)
                    throw new NotSupportedException("Backend IL: FONTS.TTMETRICS richiede 3 argomenti (path, size, codepoint).");
                EmitStringArg(expr.Args[0]);
                EmitExpression(expr.Args[1]);
                EmitCoerceTypes(NormalizeInferredType(InferType(expr.Args[1])), new DoubleType());
                EmitExpression(expr.Args[2]);
                EmitCoerceTypes(NormalizeInferredType(InferType(expr.Args[2])), new LongType());
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.TtMetrics), typeof(string), typeof(double), typeof(long)));
                EmitCoerceTypes(new ArrayType(new DoubleType()), NormalizeInferredType(InferType(expr)));
                break;
            }
            case TokenKind.TtGlyphsFunc:
            {
                if (expr.Args.Length != 1)
                    throw new NotSupportedException("Backend IL: FONTS.TTGLYPHS richiede 1 argomento (path).");
                EmitStringArg(expr.Args[0]);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.TtGlyphs), typeof(string)));
                EmitCoerceTypes(new ArrayType(new DoubleType()), NormalizeInferredType(InferType(expr)));
                break;
            }
            case TokenKind.ReadLinesFunc: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.ReadLines))); break;
            case TokenKind.ReadFileFunc: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.ReadFile))); break;
            case TokenKind.CountLinesFunc: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.CountLines))); break;
            case TokenKind.DirFunc: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.Dir))); break;
            case TokenKind.FOpen: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.FOpen))); break;
            case TokenKind.Open:
                // NS.OPEN(path) qualificato come FUNZIONE (es. LEDGER.OPEN("...")): ritorna
                // l'handle (long). Coerce del handle AL TIPO INFERITO del builtin (Long o
                // Double a seconda del ReturnType risolto dal Linker): senza, il valore
                // runtime (int64/double) divergeva dal tipo statico e il box del target
                // (es. Int16 per INTEGER) crashava il JIT (InvalidProgramException).
                if (expr.Args.Length != 1) throw new NotSupportedException("Backend IL: NS.OPEN() richiede 1 argomento (path).");
                EmitStringArg(expr.Args[0]);
                _il.Emit(OpCodes.Ldnull);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.LedgerOpen), typeof(string), typeof(string)));
                EmitCoerceTypes(new LongType(), NormalizeInferredType(InferType(expr)));
                break;
            case TokenKind.FCreate: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.FCreate))); break;
            case TokenKind.FClose: EmitInt64UnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.FClose))); break;
            case TokenKind.FRead: EmitFReadCall(expr); break;
            case TokenKind.FWrite: EmitIoHandleObjectCall(expr, nameof(IlRuntime.FWrite)); break;
            // IO.SEEK(fileNumber) e' la funzione sui NUMERI DI FILE legacy (`OPEN ... AS #n`),
            // non sugli handle di FOpen: prima finiva su Seek(handle) e con un numero legacy
            // lanciava ArgumentException "Handle file non valido" (§ MiniTests/io/io_misc.bas).
            case TokenKind.Seek: EmitInt64UnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.FileSeekNumber))); break;
            case TokenKind.FSeek: EmitIoInt64PairCall(expr, nameof(IlRuntime.FSeek)); break;
            case TokenKind.FError: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.FError))); break;
            case TokenKind.StackPush: EmitNativeCollectionBuiltinCall(expr, "Push"); break;
            case TokenKind.StackPop: EmitNativeCollectionBuiltinCall(expr, "Pop"); break;
            case TokenKind.ToArray: EmitNativeCollectionToArrayCall(expr); break;
            case TokenKind.ToList: EmitNativeCollectionToListCall(expr); break;
            case TokenKind.Keys: EmitNativeDictionaryKeysCall(expr); break;
            case TokenKind.Extract: EmitExtractCall(expr); break;
            // --- Blocco pass-through CommonLib (parità VM) ---
            case TokenKind.Add: EmitDictAddCall(expr); break;
            case TokenKind.DeriveKey: EmitDeriveKeyCall(expr); break;
            case TokenKind.DividendYield: EmitDoubleBinaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.DividendYield))); break;
            case TokenKind.GenerateMnemonic: EmitDoubleBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.GenerateMnemonic))); break;
            case TokenKind.KellyCriterion: EmitArrayDoubleCall(expr, nameof(IlRuntime.PortfolioKellyCriterion)); break;
            case TokenKind.MinCvar: EmitArrayTwoDoublesCall(expr, nameof(IlRuntime.MinCvar)); break;
            case TokenKind.MinCdar: EmitArrayTwoDoublesCall(expr, nameof(IlRuntime.MinCdar)); break;
            case TokenKind.MaxReturn: EmitArrayCall(expr, nameof(IlRuntime.PortfolioMaxReturn)); break;
            case TokenKind.PortfolioExpectedReturn: EmitPortfolioTwoArrayCall(expr, nameof(IlRuntime.PortfolioExpectedReturn)); break;
            case TokenKind.PortfolioSharpeRatio:
                // SHARPERATIO(rendimento, rischio, riskFree) — forma a 3 SCALARI (la StdLib
                // la dichiara cosi; la VM torna 0.0 perché attende liste). Forma a 2 array
                // (pesi, rendimenti) per la variante matriciale.
                if (expr.Args.Length == 3
                    && expr.Args.All(a => NormalizeInferredType(InferType(a)) is DoubleType))
                {
                    EmitDoubleArgument(expr.Args[0]);
                    EmitDoubleArgument(expr.Args[1]);
                    EmitDoubleArgument(expr.Args[2]);
                    _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.SharpeRatio3), typeof(double), typeof(double), typeof(double)));
                }
                else EmitPortfolioTwoArrayCall(expr, nameof(IlRuntime.PortfolioSharpeRatio));
                break;
            case TokenKind.PortfolioStDev: EmitPortfolioTwoArrayCall(expr, nameof(IlRuntime.PortfolioStDev)); break;
            case TokenKind.PriceSimple: EmitPriceSimpleCall(expr); break;
            case TokenKind.PortfolioVar: EmitPortfolioRiskCall(expr, nameof(IlRuntime.PortfolioVar)); break;
            case TokenKind.PortfolioCVar: EmitPortfolioRiskCall(expr, nameof(IlRuntime.PortfolioCVar)); break;
            case TokenKind.PortfolioMad: EmitPortfolioRiskCall(expr, nameof(IlRuntime.PortfolioMad)); break;
            case TokenKind.PortfolioSemiMad: EmitPortfolioRiskCall(expr, nameof(IlRuntime.PortfolioSemiMad)); break;
            case TokenKind.Run: EmitBacktestRunCall(expr); break;
            case TokenKind.Conn: EmitObjectArgCall(expr, RuntimeMethod(nameof(IlRuntime.Conn))); break;
            case TokenKind.MemoryList: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.MemoryList))); break;
            case TokenKind.MemoryStore: EmitMemoryStoreCall(expr); break;
            case TokenKind.MemoryLoad: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.MemoryLoad))); break;
            case TokenKind.MemoryDelete: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.MemoryDelete))); break;
            case TokenKind.MemorySearch: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.MemorySearch))); break;
            // --- AI (AgentServer/AiVector — pass-through) ---
            case TokenKind.AiAgentCreate: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiAgentCreate))); break;
            case TokenKind.AiAgentDelete: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiAgentDelete))); break;
            case TokenKind.AiAgentAsk: EmitStringBinaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiAgentAsk))); break;
            case TokenKind.AiAnalyzeImage: EmitStringBinaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiAnalyzeImage))); break;
            case TokenKind.AiAgentAskStream: EmitAiAgentAskStream(expr); break;
            case TokenKind.AiVerify: EmitAiVerifyCall(expr); break;
            case TokenKind.AiEvalValidate: EmitStringBinaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiEvalValidate))); break;
            case TokenKind.AiDecide:
                if (expr.Args.Length >= 6)
                    EmitAiStringsCall(expr, nameof(IlRuntime.AiDecideVerify), 6);
                else if (expr.Args.Length >= 5)
                    EmitAiStringsCall(expr, nameof(IlRuntime.AiDecideGroup), 5);
                else if (expr.Args.Length >= 4)
                    EmitAiStringsCall(expr, nameof(IlRuntime.AiDecide), 4);
                else if (expr.Args.Length >= 3)
                    EmitStringTernaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiDecide), typeof(string), typeof(string), typeof(string)));
                else
                    EmitStringBinaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiDecide), typeof(string), typeof(string)));
                break;
            case TokenKind.AiAskChain: EmitAiStringsCall(expr, nameof(IlRuntime.AiAskChain), 3); break;
            case TokenKind.AiAgentSet: EmitAiStringsCall(expr, nameof(IlRuntime.AiAgentSet), 3); break;
            case TokenKind.AiPermission:
                // AI PERMISSION DEFAULT lvl (1) | nome, lvl (2) | nome, TOOL tool, policy (3)
                if (expr.Args.Length == 1)
                    EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiPermissionDefault)));
                else if (expr.Args.Length == 2)
                    EmitStringBinaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiPermissionLevel)));
                else
                    EmitAiStringsCall(expr, nameof(IlRuntime.AiPermissionTool), 3);
                break;
            case TokenKind.AiAgentGet: EmitStringBinaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiAgentGet))); break;
            case TokenKind.AiAgentSave: EmitAiAgentSaveLoadCall(expr, nameof(IlRuntime.AiAgentSave)); break;
            case TokenKind.AiAgentLoad: EmitAiAgentSaveLoadCall(expr, nameof(IlRuntime.AiAgentLoad)); break;
            case TokenKind.AiAgentRegisterTool: EmitStringBinaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiAgentRegisterTool))); break;
            case TokenKind.AiAgentServe: EmitDoubleBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiAgentServe))); break;
            case TokenKind.AiAgentStopServer: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiAgentStopServer))); break;
            case TokenKind.AiAgentList: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiAgentList))); break;
            case TokenKind.AiAgentStatus: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiAgentStatus))); break;
            case TokenKind.AiCacheStats: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiCacheStats))); break;
            case TokenKind.AiPrefillStats: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiPrefillStats))); break;
            case TokenKind.AiDecodeStats: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiDecodeStats))); break;
            case TokenKind.AiPrefill: EmitAiPrefillCall(expr); break;
            case TokenKind.AiDecode: EmitAiDecodeCall(expr); break;
            case TokenKind.AiCacheClear: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiCacheClear))); break;
            case TokenKind.AiCacheTrim: EmitAiCacheTrimCall(expr); break;
            case TokenKind.AiModelLoad: EmitAiModelLoadCall(expr); break;
            case TokenKind.AiModelConvert: EmitAiModelConvertCall(expr); break;
            case TokenKind.AiModelQuantize: EmitAiModelQuantizeCall(expr); break;
            case TokenKind.AiModelInspect: EmitAiModelInspectCall(expr); break;
            case TokenKind.AiModelSave: EmitAiModelSaveCall(expr); break;
            case TokenKind.AiModelAblate: EmitAiModelAblateCall(expr); break;
            case TokenKind.AiModelRemove: EmitAiModelRemoveCall(expr); break;
            case TokenKind.AiModelDiff: EmitAiModelDiffCall(expr); break;
            case TokenKind.AiModelCheckpoint: EmitAiModelCheckpointCall(expr); break;
            case TokenKind.AiModelRollback: EmitAiModelRollbackCall(expr); break;
            case TokenKind.AiModelAdd: EmitAiModelAddCall(expr); break;
            case TokenKind.AiModelAdapter: EmitAiModelAdapterCall(expr); break;
            case TokenKind.AiModelModule: EmitAiModelModuleCall(expr); break;
            case TokenKind.AiModelModality: EmitAiModelModalityCall(expr); break;
            case TokenKind.AiModelValidate: EmitAiModelValidateCall(expr); break;
            case TokenKind.AiModelTrace: EmitAiModelTraceCall(expr); break;
            case TokenKind.AiModelUnload: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiModelUnload))); break;
            case TokenKind.AiModelPromote: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiModelPromote))); break;
            case TokenKind.AiModelDemote: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiModelDemote))); break;
            case TokenKind.AiModelCache: EmitAiModelCacheCall(expr); break;
            case TokenKind.AiModelPrefetch: EmitAiModelPrefetchCall(expr); break;
            case TokenKind.AiModelDevice: EmitAiModelDeviceCall(expr); break;
            // MODEL COMPILER LLM (namespace LLM): P/Invoke al runtime CONDIVISO C (dylib gmb_ai_llm).
            // Stessa logica di C99Native/VM-C: una sola sorgente (AGENTS C#→C).
            case TokenKind.LlmPlan: EmitLlmCall(expr, RuntimeMethod(nameof(IlRuntime.LlmPlan), typeof(string), typeof(string)), "ss"); break;
            // LLM POLICY CALLBACK <Proc> (Livello B, mondo gestito): il delegate alla SUB viene
            // passato al runtime, che itera il piano e la invoca (nel task GPM).
            case TokenKind.LlmPolicy when expr.Args.Length >= 2
                && expr.Args[0] is StringExpr lcs && lcs.Value.Equals("CALLBACK", System.StringComparison.OrdinalIgnoreCase)
                && expr.Args[1] is FunctionReferenceExpr lcb:
                EmitFunctionReference(lcb);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.LlmPolicyCallback), typeof(System.Func<string, string, string, string>)));
                break;
            case TokenKind.LlmPolicy: EmitLlmCall(expr, RuntimeMethod(nameof(IlRuntime.LlmPolicy), typeof(string), typeof(string), typeof(string), typeof(string)), "ssss"); break;
            case TokenKind.GmbModule: EmitLlmCall(expr, RuntimeMethod(nameof(IlRuntime.GmbModule), typeof(string), typeof(string), typeof(string)), "sss"); break;
            case TokenKind.LlmQuant when expr.Args.Length >= 3:
                EmitLlmCall(expr, RuntimeMethod(nameof(IlRuntime.LlmQuantAuto), typeof(string), typeof(string), typeof(double)), "ssd"); break;
            case TokenKind.LlmQuant: EmitLlmCall(expr, RuntimeMethod(nameof(IlRuntime.LlmQuant), typeof(string), typeof(string)), "ss"); break;
            case TokenKind.LlmMerge: EmitLlmCall(expr, RuntimeMethod(nameof(IlRuntime.LlmMerge), typeof(string), typeof(string), typeof(string)), "sss"); break;
            case TokenKind.LlmPrune: EmitLlmCall(expr, RuntimeMethod(nameof(IlRuntime.LlmPrune), typeof(string), typeof(string), typeof(double)), "ssd"); break;
            case TokenKind.LlmExtract: EmitLlmCall(expr, RuntimeMethod(nameof(IlRuntime.LlmExtract), typeof(string), typeof(string), typeof(double), typeof(double)), "ssdd"); break;
            case TokenKind.LlmProfile: EmitLlmCall(expr, RuntimeMethod(nameof(IlRuntime.LlmProfile), typeof(string), typeof(string)), "ss"); break;
            case TokenKind.LlmArch: EmitLlmCall(expr, RuntimeMethod(nameof(IlRuntime.LlmArch), typeof(string), typeof(string), typeof(string)), "sss"); break;
            case TokenKind.LlmTensor: EmitLlmCall(expr, RuntimeMethod(nameof(IlRuntime.LlmTensor), typeof(string), typeof(string), typeof(string), typeof(string)), "ssss"); break;
            case TokenKind.LlmTokenCount: EmitLlmCall(expr, RuntimeMethod(nameof(IlRuntime.LlmTokenCount), typeof(string), typeof(string)), "ss"); break;
            case TokenKind.LlmTokens: EmitLlmCall(expr, RuntimeMethod(nameof(IlRuntime.LlmTokens), typeof(string), typeof(string)), "ss"); break;
            case TokenKind.LlmDetokenize: EmitLlmCall(expr, RuntimeMethod(nameof(IlRuntime.LlmDetokenize), typeof(string), typeof(string)), "ss"); break;
            case TokenKind.LlmTensorValue: EmitLlmCall(expr, RuntimeMethod(nameof(IlRuntime.LlmTensorValue), typeof(string), typeof(string), typeof(string), typeof(string)), "ssss"); break;
            case TokenKind.LlmPolicyOut: EmitLlmCall(expr, RuntimeMethod(nameof(IlRuntime.LlmPolicyOut), typeof(string), typeof(string), typeof(string), typeof(string)), "ssss"); break;
            case TokenKind.LlmArchOut: EmitLlmCall(expr, RuntimeMethod(nameof(IlRuntime.LlmArchOut), typeof(string), typeof(string), typeof(string), typeof(string)), "ssss"); break;
            case TokenKind.LlmModuleOut: EmitLlmCall(expr, RuntimeMethod(nameof(IlRuntime.LlmModuleOut), typeof(string), typeof(string), typeof(string), typeof(string)), "ssss"); break;
            case TokenKind.LlmKernelOut: EmitLlmCall(expr, RuntimeMethod(nameof(IlRuntime.LlmKernelOut), typeof(string), typeof(string), typeof(string), typeof(string)), "ssss"); break;
            case TokenKind.LlmKvOut: EmitLlmCall(expr, RuntimeMethod(nameof(IlRuntime.LlmKvOut), typeof(string), typeof(string), typeof(string), typeof(string)), "ssss"); break;
            case TokenKind.AsrTranscribe: EmitLlmCall(expr, RuntimeMethod(nameof(IlRuntime.AsrTranscribe), typeof(string), typeof(string), typeof(string)), "sss"); break;
            case TokenKind.LlmCompile: EmitLlmCall(expr, RuntimeMethod(nameof(IlRuntime.LlmCompile), typeof(string), typeof(string)), "ss"); break;
            case TokenKind.LlmQuality:
                // QUALITY/EVAL: (model, prompt/dataset[, out][, ref]). out e ref opzionali.
                EmitStringArg(expr.Args[0]); EmitStringArg(expr.Args[1]);
                if (expr.Args.Length >= 3) EmitStringArg(expr.Args[2]); else _il.Emit(OpCodes.Ldstr, "");
                if (expr.Args.Length >= 4) EmitStringArg(expr.Args[3]); else _il.Emit(OpCodes.Ldstr, "");
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.LlmQuality), typeof(string), typeof(string), typeof(string), typeof(string)));
                break;
            case TokenKind.LlmKernel: EmitLlmCall(expr, RuntimeMethod(nameof(IlRuntime.LlmKernel), typeof(string), typeof(string), typeof(string)), "sss"); break;
            case TokenKind.LlmModule: EmitLlmCall(expr, RuntimeMethod(nameof(IlRuntime.LlmModule), typeof(string), typeof(string), typeof(string)), "sss"); break;
            case TokenKind.LlmKv: EmitLlmCall(expr, RuntimeMethod(nameof(IlRuntime.LlmKv), typeof(string), typeof(string), typeof(string)), "sss"); break;
            case TokenKind.AiStreamStats: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiStreamStats))); break;
            case TokenKind.AiServerStart: EmitAiServerStartCall(expr); break;
            case TokenKind.AiServerStop: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiServerStop))); break;
            case TokenKind.AiServerStatus: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiServerStatus))); break;
            case TokenKind.AiServerModel: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiServerModel))); break;
            case TokenKind.AiServerUnload: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiServerUnload))); break;
            case TokenKind.AiBenchmark: EmitAiBenchmarkCall(expr); break;
            case TokenKind.AiFinetune: EmitAiFinetuneCall(expr); break;
            case TokenKind.AiDpo: EmitAiDpoCall(expr); break;
            case TokenKind.AiGrpo: EmitAiGrpoCall(expr); break;
            case TokenKind.AiDistillBuild: EmitAiDistillBuildCall(expr); break;
            case TokenKind.AiDistillStatus: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiDistillStatus))); break;
            case TokenKind.AiDistillSample: EmitAiDistillSampleCall(expr); break;
            case TokenKind.AiDistillSampleN: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiDistillSampleN))); break;
            case TokenKind.AiDistillSampleFail: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiDistillSampleFail))); break;
            case TokenKind.AiDistillSampleDone: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiDistillSampleDone))); break;
            case TokenKind.AiDistillEval: EmitAiDistillEvalCall(expr); break;
            case TokenKind.AiDistillEvalTok: EmitAiDistillEvalIdxCall(expr, nameof(IlRuntime.AiDistillEvalTok)); break;
            case TokenKind.AiDistillEvalPass: EmitAiDistillEvalIdxCall(expr, nameof(IlRuntime.AiDistillEvalPass)); break;
            case TokenKind.AiDistillEvalAcc: EmitAiDistillEvalIdxCall(expr, nameof(IlRuntime.AiDistillEvalAcc)); break;
            case TokenKind.AiDistillApply: EmitAiDistillApplyCall(expr); break;
            case TokenKind.AiDistillRestore: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiDistillRestore))); break;
            case TokenKind.AiDistillTrain: EmitAiDistillTrainCall(expr); break;
            case TokenKind.AiDistillLoss: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiDistillLoss))); break;
            case TokenKind.AiDistillHold: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiDistillHold))); break;
            case TokenKind.AiDistillCkpt: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiDistillCkpt))); break;
            case TokenKind.AiDistillDone: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiDistillDone))); break;
            case TokenKind.AiLoraApply: EmitAiLoraApplyCall(expr); break;
            case TokenKind.AiFinetuneStatus: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiFinetuneStatus))); break;
            case TokenKind.AiFinetuneCkpt: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiFinetuneCkpt))); break;
            case TokenKind.AiFinetuneLoss: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiFinetuneLoss))); break;
            case TokenKind.AiFinetuneHold: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiFinetuneHold))); break;
            case TokenKind.AiFinetuneDone: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiFinetuneDone))); break;
            case TokenKind.AiFinetuneGreedy: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiFinetuneGreedy))); break;
            case TokenKind.AiFinetuneGreedyAfter: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiFinetuneGreedyAfter))); break;
            case TokenKind.AiFinetuneDemoChanged: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiFinetuneDemoChanged))); break;
            case TokenKind.AiFinetuneBaseTrain: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiFinetuneBaseTrain))); break;
            case TokenKind.AiFinetuneBaseHold: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiFinetuneBaseHold))); break;
            case TokenKind.LlmRunsJson: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.RunsJson))); break;
            case TokenKind.LlmRunsCount: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.RunsCount))); break;
            case TokenKind.LlmRunsClear: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.RunsClear))); break;
            case TokenKind.LlmTrainConfig: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.TrainConfig))); break;
            case TokenKind.LlmTrainConfigSet: EmitStringBinaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.TrainConfigSet))); break;
            case TokenKind.LlmRunsItem: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.RunsItem))); break;
            case TokenKind.LlmTrainExport: {
                // TRAIN EXPORT <out> | <run>, <out>: 2° argomento opzionale (run). Sempre 2 stringhe.
                if (expr.Args.Length < 1) throw new NotSupportedException("Backend IL: LLM.TRAINEXPORT richiede 1 o 2 argomenti.");
                EmitStringArg(expr.Args[0]);
                if (expr.Args.Length >= 2) EmitStringArg(expr.Args[1]); else _il.Emit(OpCodes.Ldstr, "");
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.TrainExport)));
                break;
            }
            case TokenKind.LlmRunStop: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.RunsStop))); break;
            case TokenKind.LlmEvalGate: EmitStringBinaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.LlmEvalGate))); break;
            case TokenKind.LlmDatasetStats: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.DatasetStats))); break;
            case TokenKind.LlmLoraList: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.LoraList))); break;
            case TokenKind.LlmLoraSave: EmitStringBinaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.LoraSave))); break;
            case TokenKind.LlmLoraLoad: EmitStringBinaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.LoraLoad))); break;
            case TokenKind.AiBenchResult: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiBenchResult))); break;
            case TokenKind.AiAgentTrace: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiAgentTrace))); break;
            case TokenKind.AiAgentTraceOn:
            case TokenKind.AiAgentTraceOff:
            case TokenKind.AiAgentTraceVerbose: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiAgentTrace))); break;
            case TokenKind.AiAgentEvents: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiAgentEvents))); break;
            case TokenKind.AiAgentSend: {
                EmitExpression(expr.Args[0]);
                EmitExpression(expr.Args[1]);
                EmitExpression(expr.Args[2]);
                if (expr.Args.Length > 3) EmitExpression(expr.Args[3]); else _il.Emit(OpCodes.Ldc_R8, 0.0);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.AiAgentSend)));
                break;
            }
            case TokenKind.AiAgentReceive: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiAgentReceive))); break;
            case TokenKind.AiAgentPause: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiAgentPause))); break;
            case TokenKind.AiAgentResume: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiAgentResume))); break;
            case TokenKind.AiAgentStop: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiAgentStop))); break;
            case TokenKind.AiAgentQueueStats: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiAgentQueueStats))); break;
            case TokenKind.AiAgentQueueClear: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiAgentQueueClear))); break;
            case TokenKind.AiChannelCreate: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiChannelCreate))); break;
            case TokenKind.AiChannelPublish: EmitStringBinaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiChannelPublish))); break;
            case TokenKind.AiChannelSubscribe: EmitStringBinaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiChannelSubscribe))); break;
            case TokenKind.AiChannelUnsubscribe: EmitStringBinaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiChannelUnsubscribe))); break;
            case TokenKind.AiAgentCode: {
                if (expr.Args.Length == 1) { EmitExpression(expr.Args[0]); _il.Emit(OpCodes.Ldstr, ""); }
                else { EmitExpression(expr.Args[0]); EmitExpression(expr.Args[1]); }
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.AiAgentCode), typeof(string), typeof(string)));
                break;
            }
            case TokenKind.AiAgentFix: {
                if (expr.Args.Length == 1) { EmitExpression(expr.Args[0]); _il.Emit(OpCodes.Ldstr, ""); }
                else { EmitExpression(expr.Args[0]); EmitExpression(expr.Args[1]); }
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.AiAgentFix), typeof(string), typeof(string)));
                break;
            }
            case TokenKind.AiKgAddNode: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiKgAddNode))); break;
            case TokenKind.AiKgAddEdge: EmitStringBinaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiKgAddEdge))); break;
            case TokenKind.AiKgQuery: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiKgQuery))); break;
            case TokenKind.AiVectorEmbed: EmitAiVectorEmbedCall(expr); break;
            case TokenKind.AiVectorEnsureCollection: EmitAiVectorDimsCall(expr, nameof(IlRuntime.AiVectorEnsureCollection)); break;
            case TokenKind.AiVectorIndexText: EmitAiVectorIndexTextCall(expr, nameof(IlRuntime.AiVectorIndexText)); break;
            case TokenKind.AiVectorIndexMeta: EmitAiVectorIndexTextCall(expr, nameof(IlRuntime.AiVectorIndexMeta)); break;
            case TokenKind.AiVectorRemoveSource: EmitAiVectorIndexTextCall(expr, nameof(IlRuntime.AiVectorRemoveSource)); break;
            case TokenKind.AiVectorPurgeDeprecated: EmitAiVectorPairCall(expr, nameof(IlRuntime.AiVectorPurgeDeprecated)); break;
            case TokenKind.AiVectorCountEmbeddings: EmitAiVectorPairCall(expr, nameof(IlRuntime.AiVectorCountEmbeddings)); break;
            case TokenKind.AiVectorSearch: EmitAiVectorTopKCall(expr, nameof(IlRuntime.AiVectorSearch)); break;
            case TokenKind.AiVectorRagContext: EmitAiVectorTopKCall(expr, nameof(IlRuntime.AiVectorRagContext)); break;
            case TokenKind.AiVectorAsk: EmitAiVectorTopKCall(expr, nameof(IlRuntime.AiVectorAsk)); break;
            case TokenKind.AiVectorLoadDocuments: EmitAiVectorLoadDocsCall(expr); break;
            case TokenKind.AiVectorListCollections: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiVectorListCollections))); break;
            case TokenKind.AiVectorRerank: EmitAiVectorRerankCall(expr); break;
            case TokenKind.AiVectorGenTest: EmitAiVectorGenTestCall(expr); break;
            // RAG eval (2026-09-14): quartetto metriche retrieval (R1) + giudizi generazione (R2) — VECTORDB.
            case TokenKind.VectorDbPrecisionAt: EmitRagEvalMetricCall(expr, nameof(IlRuntime.VectorDbPrecisionAt)); break;
            case TokenKind.VectorDbRecallAt: EmitRagEvalMetricCall(expr, nameof(IlRuntime.VectorDbRecallAt)); break;
            case TokenKind.VectorDbNdcgAt: EmitRagEvalMetricCall(expr, nameof(IlRuntime.VectorDbNdcgAt)); break;
            case TokenKind.VectorDbMrr: EmitAiVectorPairCall(expr, nameof(IlRuntime.VectorDbMrr)); break;
            case TokenKind.VectorDbFaithfulness: EmitAiVectorPairCall(expr, nameof(IlRuntime.VectorDbFaithfulness)); break;
            case TokenKind.VectorDbRelevance: EmitAiVectorPairCall(expr, nameof(IlRuntime.VectorDbRelevance)); break;
            case TokenKind.VectorDbHallucination: EmitAiVectorPairCall(expr, nameof(IlRuntime.VectorDbHallucination)); break;
            case TokenKind.VectorDbCompleteness: EmitAiVectorPairCall(expr, nameof(IlRuntime.VectorDbCompleteness)); break;
            // RAG eval avanzato (2026-09-15): diagnostica/calibrazione/drift/A-B test.
            case TokenKind.VectorDbDiagnose: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.VectorDbDiagResultJson))); break;
            case TokenKind.VectorDbDiagnoseReport: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.VectorDbDiagReport))); break;
            case TokenKind.VectorDbCalibrate: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.VectorDbCalibrate))); break;
            case TokenKind.VectorDbEvalGate: EmitStringBinaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.VectorDbEvalGate))); break;
            // NB: EvalDiff prende DUE stringhe (baseline, current) come la firma runtime
            // `IlRuntime.VectorDbEvalDiff(string, string)` e come i codegen C: emesso no-arg
            // produceva CIL malformato (stack underflow) — vedi
            // Bugs/(open) - VectorDB-Diagnose-Nome-Canonico-Divergente.md.
            case TokenKind.VectorDbEvalDiff: EmitAiVectorPairCall(expr, nameof(IlRuntime.VectorDbEvalDiff)); break;
            case TokenKind.VectorDbBaseline: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.VectorDbBaseline))); break;
            case TokenKind.VectorDbDrift: EmitAiVectorPairCall(expr, nameof(IlRuntime.VectorDbDrift)); break;
            case TokenKind.VectorDbAbTest: EmitStringBinaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.VectorDbAbTest))); break;
            case TokenKind.VectorDbCompare: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.VectorDbCompare))); break;
            // Self-Consistency / Semantic Entropy (2026-09-14)
            case TokenKind.AiSelfConsistency: EmitAiConsistencyCall(expr); break;
            case TokenKind.AiLexicalEntropy: EmitAiLexicalEntropyCall(expr); break;
            case TokenKind.AiConsistencyDetail: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiConsistencyDetail))); break;
            case TokenKind.AiVaultCreate: EmitAiVaultCreateCall(expr); break;
            case TokenKind.AiVaultIndex: EmitAiVaultIndexCall(expr); break;
            case TokenKind.AiVaultInfo: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.VaultInfo))); break;
            case TokenKind.AiVaultFolders: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.VaultFolders))); break;
            case TokenKind.AiVaultCount: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.VaultCount))); break;
            case TokenKind.AiVaultList: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.VaultList))); break;
            case TokenKind.AiVaultWatch: EmitStringBinaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.VaultWatch))); break;
            case TokenKind.AiVaultPolicy: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.VaultPolicy))); break;
            case TokenKind.AiVaultAddFolder: EmitVaultAddFolderCall(expr); break;
            case TokenKind.AiVaultRemoveFolder: EmitStringBinaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.VaultRemoveFolder))); break;
            case TokenKind.AiVaultSetFolderProps: EmitVaultSetFolderPropsCall(expr); break;
            case TokenKind.AiNoteLink: EmitNoteLinkCall(expr); break;
            case TokenKind.AiNoteUnlink: EmitNoteUnlinkCall(expr); break;
            case TokenKind.AiNoteSetAlias: EmitNoteSetAliasCall(expr); break;
            case TokenKind.AiSyncFolder: EmitSyncFolderCall(expr); break;
            case TokenKind.AiIngestWeb: EmitIngestWebCall(expr); break;
            case TokenKind.AiBrainUse: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.BrainUse))); break;
            case TokenKind.AiBrainDetach: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.BrainDetach))); break;
            case TokenKind.AiBrainUninstall: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.BrainUninstall))); break;
            case TokenKind.AiBrainInstall: EmitStringBinaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.BrainInstall))); break;
            case TokenKind.AiBrainList: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.BrainList))); break;
            case TokenKind.AiBrainInfo: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.BrainInfo))); break;
            case TokenKind.AiBrainUsed: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.BrainUsed))); break;
            case TokenKind.AiContextUsed: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiContextUsed))); break;
            case TokenKind.AiContextBudget: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiContextBudget))); break;
            case TokenKind.AiContextOverflow: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiContextOverflow))); break;
            case TokenKind.AiContextView: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiContextView))); break;
            case TokenKind.AiContextViewDebug: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiContextViewDebug))); break;
            case TokenKind.AiContextSet: EmitAiVectorPairCall(expr, nameof(IlRuntime.AiContextSet)); break;
            case TokenKind.AiContextViewCmd: EmitAiVectorPairCall(expr, nameof(IlRuntime.AiContextViewCmd)); break;
            case TokenKind.AiContextSetPolicy: EmitAiVectorPairCall(expr, nameof(IlRuntime.AiContextSetPolicy)); break;
            case TokenKind.AiContextSetSelector: EmitAiVectorPairCall(expr, nameof(IlRuntime.AiContextSetSelector)); break;
            // §3.6 PARSEC: AI CONTEXT builder a SEGMENTI (agente).
            case TokenKind.AiContextCreate: EmitAiStringsCall(expr, nameof(IlRuntime.AiContextCreate), 1); break;
            case TokenKind.AiContextAdd:    EmitAiStringsCall(expr, nameof(IlRuntime.AiContextAdd), 3); break;
            case TokenKind.AiContextMap:    EmitAiStringsCall(expr, nameof(IlRuntime.AiContextMap), 1); break;
            case TokenKind.AiContextTrim:   EmitAiStringsCall(expr, nameof(IlRuntime.AiContextTrim), 1); break;
            case TokenKind.AiContextGc:     EmitAiStringsCall(expr, nameof(IlRuntime.AiContextGc), 1); break;
            case TokenKind.AiContextCommit: EmitAiStringsCall(expr, nameof(IlRuntime.AiContextCommit), 1); break;
            case TokenKind.AiContextHash: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiContextHash))); break;
            case TokenKind.AiContextValid: EmitAiStringsCall(expr, nameof(IlRuntime.AiContextValid), 2); break;
            case TokenKind.AiTaskMetrics: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiTaskMetrics))); break;
            // P3 (2026-09-15): AI WORKER + AI STATE — WORKER lifecycle + state save/load.
            case TokenKind.AiWorkerNew: EmitAiVectorPairCall(expr, nameof(IlRuntime.AiWorkerNew)); break;
            case TokenKind.AiWorkerRun: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiWorkerRun))); break;
            // P3 sostanza: AI WORKER "<TASK>" — invocazione pulita (vista curata).
            case TokenKind.AiWorkerInvoke: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiWorkerInvoke))); break;
            // AI WORKER LOOP "<TASK>", rounds → (string, double): firma tipizzata come AiObserveLog.
            case TokenKind.AiWorkerLoop: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.AiWorkerLoop), typeof(string), typeof(double), typeof(string))); break;
            // AI WORKER MANAGE cycles [, require] [, plan] → (double, string, string)
            case TokenKind.AiWorkerManage: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.AiWorkerManage), typeof(double), typeof(string), typeof(string))); break;
            // E1: AI REPLAY AUTO "<sessione>" → (string) → double (numero di eventi rieseguiti OK)
            case TokenKind.AiReplayAuto: EmitStringToDoubleCall(expr, RuntimeMethod(nameof(IlRuntime.AiReplayAuto), typeof(string))); break;
            // A5: fonti OBSERVE di fase 2 (TEST/PROCESS/BROWSER: string → double; DATABASE: string,string).
            case TokenKind.AiObserveTest: EmitStringToDoubleCall(expr, RuntimeMethod(nameof(IlRuntime.AiObserveTest), typeof(string))); break;
            case TokenKind.AiObserveProcess: EmitStringToDoubleCall(expr, RuntimeMethod(nameof(IlRuntime.AiObserveProcess), typeof(string))); break;
            case TokenKind.AiObserveBrowser: EmitStringToDoubleCall(expr, RuntimeMethod(nameof(IlRuntime.AiObserveBrowser), typeof(string))); break;
            case TokenKind.AiObserveDatabase: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.AiObserveDatabase), typeof(string), typeof(string))); break;
            case TokenKind.AiStateSave: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.AiStateSave), typeof(string), typeof(string), typeof(string))); break;
            case TokenKind.AiStateLoad: EmitAiVectorPairCall(expr, nameof(IlRuntime.AiStateLoad)); break;
            case TokenKind.AiWorkerRounds: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiWorkerRounds))); break;
            case TokenKind.AiWorkerRescued: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiWorkerRescued))); break;
            case TokenKind.AiWorkerSpend: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiWorkerSpend))); break;
            case TokenKind.AiSearchMulti: EmitAiSearchMultiCall(expr); break;
            case TokenKind.AiRagContextMulti: EmitAiRagContextMultiCall(expr); break;
            case TokenKind.AiAskMulti: EmitAiAskMultiCall(expr); break;
            case TokenKind.GraphQueryMulti: EmitGraphQueryMultiCall(expr); break;
            case TokenKind.GraphNeighbors: EmitStringBinaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.GraphNeighbors))); break;
            case TokenKind.GraphPath: EmitGraphPathCall(expr); break;
            case TokenKind.GraphPageRank: EmitGraphPageRankCall(expr); break;
            case TokenKind.GraphConnected: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.GraphConnected))); break;
            case TokenKind.GraphStats: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.GraphStats))); break;
            case TokenKind.GraphStronglyConnected: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.GraphStronglyConnected))); break;
            case TokenKind.GraphTopoSort: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.GraphTopoSort))); break;
            case TokenKind.GraphBipartite: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.GraphBipartite))); break;
            case TokenKind.GraphBetweenness: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.GraphBetweenness))); break;
            case TokenKind.GraphClustering: EmitStringBinaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.GraphClustering))); break;
            case TokenKind.GraphDiameter: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.GraphDiameter))); break;
            case TokenKind.GraphMst: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.GraphMst))); break;
            case TokenKind.GraphLouvain: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.GraphLouvain))); break;
            case TokenKind.GraphMaxFlow: EmitStringTernaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.GraphMaxFlow))); break;
            case TokenKind.GraphDijkstra: EmitGraphDijkstraCall(expr); break;
            case TokenKind.GraphHasCycle: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.GraphHasCycle))); break;
            case TokenKind.GraphEccentricity: EmitStringBinaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.GraphEccentricity))); break;
            case TokenKind.GraphDfs: EmitStringBinaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.GraphDfs))); break;
            case TokenKind.GraphBfs: EmitStringBinaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.GraphBfs))); break;
            case TokenKind.GraphDot: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.GraphDot))); break;
            case TokenKind.GraphSubgraph: EmitGraphSubgraphCall(expr); break;
            case TokenKind.GraphByTag: EmitStringBinaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.GraphByTag))); break;
            case TokenKind.GraphByFolder: EmitStringBinaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.GraphByFolder))); break;
            case TokenKind.GraphIsValid: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.GraphIsValid))); break;
            case TokenKind.GraphBuild: EmitGraphBuildCall(expr); break;
            case TokenKind.GraphUpdate: EmitGraphBuildCall(expr); break;
            case TokenKind.GraphFindPath: EmitGraphFindPathCall(expr); break;
            case TokenKind.GraphRdd: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.GraphRdd))); break;
            case TokenKind.Enrich: EmitEnrichCall(expr); break;
            case TokenKind.QaAnswer: EmitQaAnswerCall(expr); break;
            case TokenKind.SuggestTags: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.SuggestTags))); break;
            case TokenKind.SignNote: EmitSignNoteCall(expr); break;
            case TokenKind.VerifyNote: EmitVerifyNoteCall(expr); break;
            case TokenKind.ShareEncrypt: EmitStringBinaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.ShareEncrypt))); break;
            case TokenKind.ShareDecrypt: EmitStringBinaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.ShareDecrypt))); break;
            case TokenKind.BrainVaultBuild: EmitBrainVaultBuildCall(expr); break;
            case TokenKind.BrainVaultQuery: EmitBrainVaultQueryCall(expr); break;
            case TokenKind.BrainVaultWrite: EmitBrainVaultWriteCall(expr); break;
            case TokenKind.NoteCreate: EmitNoteCreateCall(expr); break;
            case TokenKind.NoteUpdate: EmitNoteCmdCall(expr, nameof(IlRuntime.NoteUpdate)); break;
            case TokenKind.NoteAppend: EmitNoteCmdCall(expr, nameof(IlRuntime.NoteAppend)); break;
            case TokenKind.NoteDelete: EmitNoteCmdCall(expr, nameof(IlRuntime.NoteDelete)); break;
            case TokenKind.NoteMove: EmitNoteCmdCall(expr, nameof(IlRuntime.NoteMove)); break;
            case TokenKind.NoteGet: EmitStringBinaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.NoteGet))); break;
            case TokenKind.NoteTitle: EmitStringBinaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.NoteTitle))); break;
            case TokenKind.NoteTags: EmitStringBinaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.NoteTags))); break;
            case TokenKind.NoteWordCount: EmitStringBinaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.NoteWordCount))); break;
            case TokenKind.NoteExists: EmitStringBinaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.NoteExists))); break;
            case TokenKind.NoteFolder: EmitStringBinaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.NoteFolder))); break;
            case TokenKind.LinksOut: EmitStringBinaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.LinksOut))); break;
            case TokenKind.Backlinks: EmitStringBinaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.Backlinks))); break;
            case TokenKind.LinkDangling: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.LinkDangling))); break;
            case TokenKind.LinkJson: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.LinkJson))); break;
            case TokenKind.IngestFile: EmitIngestFileCall(expr); break;
            case TokenKind.IngestFolder: EmitIngestFolderCall(expr); break;
            case TokenKind.McpListTools: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.McpListTools))); break;
            case TokenKind.McpPrompts: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.McpPrompts))); break;
            case TokenKind.McpRegisterAgent: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.McpRegisterAgent))); break;
            case TokenKind.McpRegisterTool: EmitMcpRegisterToolCall(expr); break;
            case TokenKind.McpResources: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.McpResources))); break;
            case TokenKind.McpServe: EmitMcpServeCall(expr); break;
            case TokenKind.McpStop: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.McpStop))); break;
             case TokenKind.McpUnregisterTool: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.McpUnregisterTool))); break;
             case TokenKind.McpServerList: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.McpServerList))); break;
             case TokenKind.McpServerStatus: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.McpServerStatus))); break;
             case TokenKind.McpServerAdd: EmitStringTernaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.McpServerAdd))); break;
             case TokenKind.McpServerRemove: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.McpServerRemove))); break;
             case TokenKind.McpServerEnable: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.McpServerEnable))); break;
             case TokenKind.McpServerDisable: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.McpServerDisable))); break;
             case TokenKind.McpServerRefresh: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.McpServerRefresh))); break;
             case TokenKind.McpServerMarketplaceList: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.McpServerMarketplaceList))); break;
             case TokenKind.McpServerMarketplaceGet: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.McpServerMarketplaceGet))); break;
             case TokenKind.McpListToolsServer: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.McpListToolsServer))); break;
             case TokenKind.McpCall: EmitStringTernaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.McpCall))); break;
            case TokenKind.ConnectorList: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.ConnectorList))); break;
            case TokenKind.ConnectorStatus: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.ConnectorStatus))); break;
            case TokenKind.ConnectorTools: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.ConnectorTools))); break;
            case TokenKind.ConnectorSearch: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.ConnectorSearch))); break;
            case TokenKind.ConnectorMarketplaceList: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.ConnectorMarketplaceList))); break;
            case TokenKind.ConnectorMarketplaceGet: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.ConnectorMarketplaceGet))); break;
            case TokenKind.ConnectorRegister: EmitConnectorRegisterCall(expr); break;
            case TokenKind.ConnectorUnregister: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.ConnectorUnregister))); break;
            case TokenKind.ConnectorLoad: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.ConnectorLoad))); break;
            case TokenKind.ConnectorEnable: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.ConnectorEnable))); break;
            case TokenKind.ConnectorDisable: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.ConnectorDisable))); break;
            case TokenKind.ConnectorCall: EmitStringTernaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.ConnectorCall))); break;
            case TokenKind.ConnectorRefresh: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.ConnectorRefresh))); break;
            case TokenKind.ConnectorPolicyGet: EmitStringBinaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.ConnectorPolicyGet))); break;
            case TokenKind.ConnectorPolicySet: EmitStringTernaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.ConnectorPolicySet))); break;
            case TokenKind.ConnectorSecretGet: EmitStringBinaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.ConnectorSecretGet))); break;
            case TokenKind.ConnectorSecretSet: EmitStringTernaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.ConnectorSecretSet))); break;
            case TokenKind.ConnectorAuth: EmitStringBinaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.ConnectorAuth))); break;
            case TokenKind.ConnectorResources: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.ConnectorResources))); break;
            case TokenKind.ConnectorPrompts: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.ConnectorPrompts))); break;
case TokenKind.MemoryWatch: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.MemoryWatch))); break;
              case TokenKind.MemoryImport: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.MemoryImport))); break;
              case TokenKind.MemoryNote: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.MemoryNote))); break;
             case TokenKind.MemoryRefresh: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.MemoryRefresh))); break;
             case TokenKind.MemorySave: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.MemorySave))); break;
             case TokenKind.MemoryForget: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.MemoryForget))); break;
             case TokenKind.MemoryAsk: EmitMemoryAskCall(expr); break;
             case TokenKind.MemoryRank: EmitMemoryAskCall(expr, nameof(IlRuntime.MemoryRank)); break;
             case TokenKind.MemoryConsolidate: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.MemoryConsolidate))); break;
            case TokenKind.WorkflowList: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.WorkflowList))); break;
            case TokenKind.WorkflowStatus: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.WorkflowStatus))); break;
            case TokenKind.WorkflowSteps: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.WorkflowSteps))); break;
            case TokenKind.SqlChanges: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.SqlChanges))); break;
            case TokenKind.SqlExec: EmitSqlExecFnCall(expr); break;
            case TokenKind.SqlFetch: EmitDoubleBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.SqlFetch))); break;
            case TokenKind.SqlLastInsertRowId: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.SqlLastInsertRowId))); break;
            case TokenKind.SqlDfSchema: EmitSqlDfSchemaCall(expr); break;
            case TokenKind.SqlCol: EmitSqlColCall(expr); break;
            case TokenKind.SqlToCsv: EmitSqlToCsvCall(expr); break;
            case TokenKind.SqlFromCsv: EmitSqlFromCsvCall(expr); break;
            case TokenKind.TeamList: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.TeamList))); break;
            case TokenKind.TeamAgents: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.TeamAgents))); break;
            case TokenKind.TeamTrainStatus: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.TeamTrainStatus))); break;
            case TokenKind.TeamLinkInfo: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.TeamLinkInfo))); break;
            case TokenKind.TeamRecursion: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.TeamRecursion))); break;
            case TokenKind.TeamLatent: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.TeamLatent))); break;
            case TokenKind.TeamRuntimeStats: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.TeamRuntimeStats))); break;
            case TokenKind.TeamRunStart: EmitStringBinaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.TeamRunStart), typeof(string), typeof(string))); break;
            case TokenKind.TeamRunWait: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.TeamRunWait))); break;
            case TokenKind.TeamOnStream: EmitStringToDoubleCall(expr, RuntimeMethod(nameof(IlRuntime.TeamOnStream))); break;
            case TokenKind.BlackboardWrite: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.BlackboardWrite), typeof(string), typeof(string))); break;
            case TokenKind.BlackboardRead: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.BlackboardRead))); break;
            case TokenKind.BlackboardWait: { EmitStringArg(expr.Args[0]); EmitDoubleArgument(expr.Args[1]); _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.BlackboardWait), typeof(string), typeof(double))); break; }
            case TokenKind.AgentSwarm: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.AiAgentSwarm), typeof(double), typeof(string), typeof(string), typeof(double))); break;
            // Orchestrazione agenti (A1): categorie di task → provider/modello.
            case TokenKind.AiCategorySet: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.CategorySet), typeof(string), typeof(string))); break;
            case TokenKind.AiCategoryFallback: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.CategoryFallback), typeof(string), typeof(string))); break;
            case TokenKind.AiCategoryAuto: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.CategoryAuto), typeof(double))); break;
            case TokenKind.AiCategoryPick: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.CategoryPick), typeof(string))); break;
            case TokenKind.AiCategoryList: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.CategoryList))); break;
            // Orchestrazione agenti (B1): Task Graph con dipendenze.
            case TokenKind.AiTaskDefine: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.TaskDefine), typeof(string), typeof(string))); break;
            case TokenKind.AiTaskDepends: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.TaskDepends), typeof(string), typeof(string))); break;
            case TokenKind.AiTaskAssign: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.TaskAssign), typeof(string), typeof(string))); break;
            case TokenKind.AiTaskStatus: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.TaskStatus), typeof(string))); break;
            case TokenKind.AiTaskList: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.TaskList))); break;
            case TokenKind.AiTaskRun: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.TaskRun))); break;
            // Orchestrazione agenti (A2): Planner — piani con approvazione.
            case TokenKind.AiPlanDefine: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.PlanDefine), typeof(string), typeof(string))); break;
            case TokenKind.AiPlanAdd: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.PlanAdd), typeof(string), typeof(string))); break;
            case TokenKind.AiPlanApprove: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.PlanApprove), typeof(string))); break;
            case TokenKind.AiPlanStatus: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.PlanStatus), typeof(string))); break;
            case TokenKind.AiPlanList: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.PlanList))); break;
            case TokenKind.AiPlanRun: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.PlanRun), typeof(string))); break;
            // Orchestrazione agenti (A3): lifecycle hooks.
            case TokenKind.AiHookRegister: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.HookRegister), typeof(string), typeof(string))); break;
            case TokenKind.AiHookUnregister: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.HookUnregister), typeof(string), typeof(string))); break;
            case TokenKind.AiHookList: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.HookList))); break;
            case TokenKind.AiHookEvents: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.HookEvents))); break;
            case TokenKind.AiHookFire: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.HookFire), typeof(string), typeof(string))); break;
            // Orchestrazione agenti (A6): Artifact Store.
            case TokenKind.AiArtifactPut: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.ArtifactPut), typeof(string), typeof(string))); break;
            case TokenKind.AiArtifactLink: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.ArtifactLink), typeof(string), typeof(string))); break;
            case TokenKind.AiArtifactGet: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.ArtifactGet), typeof(string))); break;
            case TokenKind.AiArtifactList: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.ArtifactList))); break;
            case TokenKind.AiArtifactRemove: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.ArtifactRemove), typeof(string))); break;
            // Orchestrazione agenti (A7): Resource Manager.
            case TokenKind.AiResourceSet: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.ResourceSet), typeof(string), typeof(string))); break;
            case TokenKind.AiResourceGet: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.ResourceGet), typeof(string))); break;
            case TokenKind.AiResourceList: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.ResourceList))); break;
            case TokenKind.AiResourceAvailable: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.ResourceAvailable), typeof(string))); break;
            case TokenKind.AiResourceSwarmSize: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.ResourceSwarmSize), typeof(double))); break;
            // Orchestrazione agenti (A9): Handoff.
            case TokenKind.AiHandoff: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.Handoff), typeof(string), typeof(string))); break;
            case TokenKind.AiHandoffComplete: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.HandoffComplete), typeof(string), typeof(string))); break;
            case TokenKind.AiHandoffStatus: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.HandoffStatus), typeof(string))); break;
            case TokenKind.AiHandoffList: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.HandoffList))); break;
            case TokenKind.AiHandoffClear: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.HandoffClear))); break;
            // Orchestrazione agenti (A5): Rules injection gerarchica.
            case TokenKind.AiRulesAdd: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.RulesAdd), typeof(string), typeof(string))); break;
            case TokenKind.AiRulesFor: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.RulesFor), typeof(string))); break;
            case TokenKind.AiRulesList: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.RulesList))); break;
            case TokenKind.AiRulesRemove: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.RulesRemove), typeof(string))); break;
            case TokenKind.AiRulesClear: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.RulesClear))); break;
            // Orchestrazione agenti (A4): Goal + idle continuation.
            case TokenKind.AiGoalSet: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.GoalSet), typeof(string))); break;
            case TokenKind.AiGoalGet: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.GoalGet))); break;
            case TokenKind.AiGoalStatus: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.GoalStatus))); break;
            case TokenKind.AiGoalTick: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.GoalTick))); break;
            case TokenKind.AiGoalClear: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.GoalClear))); break;
            // Orchestrazione agenti (A8): Session Replay.
            case TokenKind.AiReplayStart: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.ReplayStart), typeof(string))); break;
            case TokenKind.AiReplayRecord: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.ReplayRecord), typeof(string))); break;
            case TokenKind.AiReplayStop: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.ReplayStop))); break;
            case TokenKind.AiReplayGet: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.ReplayGet), typeof(string))); break;
            case TokenKind.AiReplayList: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.ReplayList))); break;
            case TokenKind.AiReplayClear: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.ReplayClear))); break;
            case TokenKind.AiReplayRun: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.ReplayRun), typeof(string), typeof(string))); break;
            case TokenKind.AiGoalAgent: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.GoalAgent), typeof(string))); break;
            // Orchestrazione agenti (B6): metriche fleet-level.
            case TokenKind.AiMetricRecord: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.MetricRecord), typeof(string), typeof(string), typeof(double))); break;
            case TokenKind.AiMetricGet: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.MetricGet), typeof(string), typeof(string))); break;
            case TokenKind.AiMetricList: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.MetricList))); break;
            case TokenKind.AiFleetStats: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.FleetStats))); break;
            case TokenKind.AiMetricReset: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.MetricReset))); break;
            // Orchestrazione agenti (A10): Hashline.
            case TokenKind.AiHashLine: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.HashLine), typeof(string))); break;
            case TokenKind.AiHashEdit: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.HashEdit), typeof(string), typeof(string), typeof(string))); break;
            case TokenKind.AiHashVerify: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.HashVerify), typeof(string), typeof(string))); break;
            // Orchestrazione agenti (B7): checkpoint/recovery.
            case TokenKind.AiCheckpoint: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.Checkpoint), typeof(string), typeof(string))); break;
            case TokenKind.AiResume: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.Resume), typeof(string))); break;
            case TokenKind.AiCheckpointList: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.CheckpointList))); break;
            case TokenKind.AiCheckpointClear: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.CheckpointClear))); break;
            // Persistenza orchestrazione su disco.
            case TokenKind.AiOrchSave: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.OrchSave), typeof(string))); break;
            case TokenKind.AiOrchLoad: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.OrchLoad), typeof(string))); break;
            case TokenKind.AiTaskClear: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.TaskClear))); break;
            case TokenKind.AiPlanClear: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.PlanClear))); break;
            case TokenKind.Contract: EmitContractCall(expr); break;
            case TokenKind.ProviderActive: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.ProviderActive))); break;
            case TokenKind.ProviderList: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.ProviderList))); break;
            case TokenKind.ProviderUnregister: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.ProviderUnregister))); break;
            case TokenKind.ProviderRegister: EmitProviderStringsCall(expr, nameof(IlRuntime.ProviderRegister), 4); break;
            case TokenKind.ProviderConfigure: EmitProviderStringsCall(expr, nameof(IlRuntime.ProviderConfigure), 3); break;
            case TokenKind.ProviderPriority: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.ProviderPriority))); break;
            case TokenKind.ProviderSetPriority: EmitProviderNameValueCall(expr, nameof(IlRuntime.ProviderSetPriority)); break;
            case TokenKind.ProviderWeight: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.ProviderWeight))); break;
            case TokenKind.ProviderSetWeight: EmitProviderNameValueCall(expr, nameof(IlRuntime.ProviderSetWeight)); break;
            case TokenKind.ProviderDiscover: EmitProviderDiscoverCall(expr); break;
            case TokenKind.ProviderSetCredits: EmitProviderNameValueCall(expr, nameof(IlRuntime.ProviderSetCredits)); break;
            case TokenKind.ProviderCredits: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.ProviderCredits))); break;
            case TokenKind.ProviderMarkFailed: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.ProviderMarkFailed))); break;
            case TokenKind.ProviderResetFailed: EmitProviderResetCall(expr); break;
            case TokenKind.ProviderStatus: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.ProviderStatus))); break;
            case TokenKind.ProviderSetStatus: EmitProviderStringsCall(expr, nameof(IlRuntime.ProviderSetStatus), 2); break;
            case TokenKind.ProviderRotate: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.ProviderRotate))); break;
             case TokenKind.ProviderSetActive: EmitProviderStringsCall(expr, nameof(IlRuntime.ProviderSetActive), 2); break;
             case TokenKind.ProviderTest: EmitProviderStringsCall(expr, nameof(IlRuntime.ProviderTest), 2); break;
             case TokenKind.ProviderSave: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.ProviderSave))); break;
             case TokenKind.ProviderLoad: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.ProviderLoad))); break;
             // AI namespace-command (token fusi qualificati) → stessi runtime provider.
             case TokenKind.AiSetActive:
             case TokenKind.AiSetActiveProvider: EmitProviderStringsCall(expr, nameof(IlRuntime.ProviderSetActive), 2); break;
             case TokenKind.AiRegister: EmitAiRegisterCall(expr); break;
             case TokenKind.AiConfigure: EmitProviderStringsCall(expr, nameof(IlRuntime.ProviderConfigure), 3); break;
             case TokenKind.AiDiscover: EmitProviderDiscoverCall(expr); break;
             case TokenKind.AiSetCredits: EmitProviderNameValueCall(expr, nameof(IlRuntime.ProviderSetCredits)); break;
             case TokenKind.AiCredits: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.ProviderCredits))); break;
             case TokenKind.AiRotate: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.ProviderRotate))); break;
             case TokenKind.AiActive: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.ProviderActive))); break;
             case TokenKind.AiList: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.ProviderList))); break;
             case TokenKind.AiUnregister: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.ProviderUnregister))); break;
             case TokenKind.AiSaveProv: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.ProviderSave))); break;
             case TokenKind.AiLoadProv: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.ProviderLoad))); break;
             case TokenKind.AiTest: EmitProviderStringsCall(expr, nameof(IlRuntime.ProviderTest), 2); break;
             case TokenKind.AiSetPriority: EmitProviderNameValueCall(expr, nameof(IlRuntime.ProviderSetPriority)); break;
             case TokenKind.AiPriority: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.ProviderPriority))); break;
             case TokenKind.AiSetWeight: EmitProviderNameValueCall(expr, nameof(IlRuntime.ProviderSetWeight)); break;
             case TokenKind.AiWeight: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.ProviderWeight))); break;
             case TokenKind.AiFail: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.ProviderMarkFailed))); break;
             case TokenKind.AiResetFailed: EmitProviderResetCall(expr); break;
             case TokenKind.AiSetStatus: EmitProviderStringsCall(expr, nameof(IlRuntime.ProviderSetStatus), 2); break;
             case TokenKind.AiStatus: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.ProviderStatus))); break;
              // AI config (token fusi) → runtime AI.
              case TokenKind.AiSetModel: EmitAiModelCall(expr); break;
              case TokenKind.AiGetModel: EmitAiModelCall(expr); break;
              case TokenKind.AiSetKey: EmitStringBinaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiSetKey))); break;
              case TokenKind.AiSetHfToken: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiSetHfToken))); break;
              case TokenKind.AiSetOption: EmitStringBinaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiSetOption))); break;
              case TokenKind.AiOption: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiOption))); break;
              case TokenKind.AiSetHfRepo: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiSetHfRepo))); break;
              case TokenKind.AiHfRepo: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiHfRepo))); break;
              case TokenKind.AiGetKey: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiGetKey))); break;
              case TokenKind.AiSetTemp: EmitStringBinaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiSetTemp))); break;
              case TokenKind.AiGetTemp: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiGetTemp))); break;
              case TokenKind.AiSetBase: EmitStringBinaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiSetBase))); break;
              case TokenKind.AiGetBase: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiGetBase))); break;
              // AI PATH MODEL/SKILL ADD|REMOVE
              case TokenKind.AiPathModelAdd: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiPathModelAdd))); break;
              case TokenKind.AiPathModelRemove: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiPathModelRemove))); break;
              case TokenKind.AiPathSkillAdd: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiPathSkillAdd))); break;
              case TokenKind.AiPathSkillRemove: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiPathSkillRemove))); break;
              // AI SKILLS ...
              case TokenKind.AiSkillsAdd: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiSkillsAdd))); break;
              case TokenKind.AiSkillsRemove: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiSkillsRemove))); break;
              case TokenKind.AiSkillsRefresh: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiSkillsRefresh))); break;
              case TokenKind.AiSkillsSetIndex: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiSkillsSetIndex))); break;
              case TokenKind.AiSkillsMarketplace: EmitAiSkillsMarketplaceCall(expr); break;
              case TokenKind.AiSkillsPrompt: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiSkillsPrompt))); break;
              case TokenKind.AiSkillsList: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiSkillsList))); break;
              case TokenKind.AiSkillsPaths: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiSkillsPaths))); break;
              // AI PLUGINS ...
              case TokenKind.AiPluginsAdd: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiPluginsAdd))); break;
              case TokenKind.AiPluginsRemove: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiPluginsRemove))); break;
              case TokenKind.AiPluginsRefresh: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiPluginsRefresh))); break;
              case TokenKind.AiPluginsSetIndex: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiPluginsSetIndex))); break;
              case TokenKind.AiPluginsList: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiPluginsList))); break;
              case TokenKind.AiPluginsEnable: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiPluginsEnable))); break;
              case TokenKind.AiPluginsDisable: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiPluginsDisable))); break;
              case TokenKind.AiPluginsCreate: EmitAiPluginsCreateCall(expr); break;
              case TokenKind.AiPluginsMarketplace: EmitAiPluginsMarketplaceCall(expr); break;
              case TokenKind.AiPluginCall: EmitAiPluginCallCall(expr); break;
              case TokenKind.Process: EmitProcessCall(expr); break;
              case TokenKind.AiPluginInfo: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiPluginInfo))); break;
              case TokenKind.AiPluginList: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiPluginList))); break;
              case TokenKind.AiPathPluginAdd: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiPathPluginAdd))); break;
              case TokenKind.AiPathPluginRemove: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiPathPluginRemove))); break;
              case TokenKind.AiLogPath: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiLogPath))); break;
              case TokenKind.AiLogWrite: EmitStringBinaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiLogWrite))); break;
              case TokenKind.AiLogWriteLine: EmitStringBinaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiLogWriteLine))); break;
            case TokenKind.ProviderHealthCheckInterval: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.ProviderHealthCheckInterval))); break;
            case TokenKind.ProviderRunHealthCheck: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.ProviderRunHealthCheck))); break;
            case TokenKind.ProviderStartHealthCheck: EmitDoubleBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.ProviderStartHealthCheck))); break;
            case TokenKind.ProviderStopHealthCheck: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.ProviderStopHealthCheck))); break;
             case TokenKind.Get when expr.Namespace!.Lexeme.Equals("CLIP", StringComparison.OrdinalIgnoreCase):
                 // CLIP.GET(): legge dagli appunti (headless → stringa vuota).
                 EmitClipCall(expr);
                 break;
             case TokenKind.Get: EmitDictGetCall(expr); break;
            case TokenKind.Set: EmitDictSetCall(expr); break;
            case TokenKind.Range: EmitRangeCall(expr); break;
            case TokenKind.PERatio: EmitDoubleBinaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.PeRatio))); break;
            case TokenKind.KeyClose: EmitInt64UnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.KeyClose))); break;
            case TokenKind.ReadValue: EmitReadValueCall(expr); break;
            case TokenKind.ReadSection: EmitReadSectionCall(expr); break;
            case TokenKind.ToDict: EmitToDictCall(expr); break;
            // --- Funzioni ARRAY in stile Clipper (16 espressioni; le 7 forme statement
            // — AADD/AINS/ADEL/ASORT/AEVAL/AFILL/ACOPY — sono AAddStmt/AInsStmt/ecc.,
            // gestite in EmitStatement, non qui) ---
            case TokenKind.AClone: EmitArrayGenericCall(expr, "ACLONE", ArrayHelpersACloneMethod, sameElementType: true); break;
            case TokenKind.AUnique: EmitArrayGenericCall(expr, "AUNIQUE", ArrayHelpersAUniqueMethod, sameElementType: true); break;
            case TokenKind.AReverse: EmitArrayUtilsCloneThenMutate(expr, "AREVERSE", ArrayUtilsReverseMethod); break;
            case TokenKind.AShuffle: EmitArrayUtilsMutateInPlace(expr, "ASHUFFLE", ArrayUtilsShuffleMethod); break;
            case TokenKind.ABinarySearch: EmitABinarySearchCall(expr); break;
            case TokenKind.AJoin: EmitAJoinCall(expr); break;
            case TokenKind.AFlat: EmitAFlatCall(expr); break;
            case TokenKind.APad: EmitAPadCall(expr); break;
            case TokenKind.AChunk: EmitChunkCall(expr); break;
            case TokenKind.AZip: EmitZipCall(expr); break;
            // ACHUNK/AZIP NON implementate qui deliberatamente: il loro risultato è
            // strutturalmente "array di array" (un livello di nesting IN PIÙ rispetto
            // all'input), ma l'inferenza di tipo del Linker (InferGenericPolymorphicReturnType,
            // "Caso 2" in Linker.Rewrites.Utils.cs) assegna loro lo STESSO tipo elemento
            // dell'array di input (corretto per ASORT/ACLONE/AUNIQUE/AREVERSE/APAD/AFLAT, che
            // non cambiano la struttura, ma sbagliato qui) — un DIM tipizzato che riceve il
            // risultato (es. `DIM B() = ACHUNK(A, 2)`) genererebbe IL non valido (cast
            // impossibile tra Array[] e T[]). Richiede un fix separato nel Linker prima di
            // poter essere implementate correttamente qui — vedi TODO Obsidian.
            case TokenKind.ATokens: EmitATokensCall(expr); break;
            case TokenKind.AMap: EmitAMapCall(expr); break;
            case TokenKind.AFilter: EmitAFilterCall(expr); break;
            case TokenKind.AReduce: EmitAReduceCall(expr); break;
            case TokenKind.ASort: EmitASortFunctionCall(expr); break;
            case TokenKind.ACount: EmitACountCall(expr); break;
            case TokenKind.AScan: EmitAScanCall(expr); break;
            // JSON.PARSE come espressione generica (es. DIM x AS JSON = JSON.PARSE("...")) —
            // il percorso FOR EACH (EmitForEachCollectionAsObject) copre solo l'uso come
            // collezione diretta di un ciclo, non un'assegnazione. Stesso IlRuntime.JsonParse.
            case TokenKind.Parse: EmitStringUnaryBuiltinCall(expr, IlJsonParseMethod); break;
            case TokenKind.Load when expr.Args.Length == 1: EmitStringUnaryBuiltinCall(expr, IlJsonLoadFileMethod); break;
            // CONVERT/AI: emitter mai stati committati con successo (esistevano solo
            // transitoriamente nel working tree di un'altra sessione concorrente durante una
            // race di build — persi in una ricostruzione di questo file dopo un incidente di
            // sovrascrittura concorrente; vedi AiWiki/09-Errors/GMBasic/ConvertAi-QualifiedCall-Bug.md).
            // IlRuntime ha già i wrapper (ConvertToMd/Html/Text/Batch/Web/Formats, AiOcr/
            // Transcribe/Providers/SetKey/SetModel, AiStub) — mancava solo questo dispatch.
            case TokenKind.ConvDoc: EmitConvDocCall(expr); break;
            case TokenKind.ConvHtml: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.ConvertToHtml))); break;
            case TokenKind.ConvText: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.ConvertToText))); break;
            case TokenKind.WebToMd: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.ConvertWeb))); break;
            case TokenKind.ConvFormats: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.ConvertFormats))); break;
             case TokenKind.CvtBatch: EmitConvertBatchCall(expr); break;
             // DOCUMENT/PDF namespace (Fase 4): handle-based PDF via NativeDocPdf.
             case TokenKind.DocExtract: EmitDocExtractCall(expr); break;
             case TokenKind.DocExtractEngine: EmitDocExtractEngineCall(expr); break;
             case TokenKind.ConvertExtractTo: EmitConvertExtractToCall(expr); break;
             case TokenKind.ConvertRenderTo: EmitConvertRenderToCall(expr); break;
             case TokenKind.MdParse: EmitMdParseCall(expr); break;
             case TokenKind.MdHeadings: EmitMdHeadingsCall(expr); break;
             case TokenKind.MdLinks: EmitMdDocCall(expr, nameof(IlRuntime.MdLinks)); break;
             case TokenKind.MdImages: EmitMdDocCall(expr, nameof(IlRuntime.MdImages)); break;
             case TokenKind.MdTables: EmitMdDocCall(expr, nameof(IlRuntime.MdTables)); break;
             case TokenKind.MdCodeBlocks: EmitMdDocCall(expr, nameof(IlRuntime.MdCodeBlocks)); break;
             case TokenKind.MdTree: EmitMdDocCall(expr, nameof(IlRuntime.MdTree)); break;
             case TokenKind.MdToHtml: EmitMdDocCall(expr, nameof(IlRuntime.MdToHtml)); break;
             case TokenKind.MdToText: EmitMdDocCall(expr, nameof(IlRuntime.MdToText)); break;
             case TokenKind.MdValidate: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.MdValidate))); break;
             case TokenKind.MdQuery: EmitMdQueryCall(expr); break;
             case TokenKind.MdStrip: EmitMdStripCall(expr); break;
             case TokenKind.MdFormat: EmitMdDocCall(expr, nameof(IlRuntime.MdFormat)); break;
             case TokenKind.DocOpen: EmitDocOpenCall(expr); break;
             case TokenKind.DocClose: EmitDocCloseCall(expr); break;
             case TokenKind.DocPageCount: EmitDocPageCountCall(expr); break;
             case TokenKind.DocText: EmitDocPageStringCall(expr, nameof(IlRuntime.DocText)); break;
             case TokenKind.DocMarkdown: EmitDocPageStringCall(expr, nameof(IlRuntime.DocMarkdown)); break;
             case TokenKind.DocHtml: EmitDocPageStringCall(expr, nameof(IlRuntime.DocHtml)); break;
             case TokenKind.DocMetadata: EmitDocMetadataCall(expr); break;
             case TokenKind.DocSearch: EmitDocSearchCall(expr); break;
             case TokenKind.DocImages: EmitDocImagesCall(expr); break;
             case TokenKind.DocRender: EmitDocRenderCall(expr); break;
            case TokenKind.AiTranscribe: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiTranscribe))); break;
            case TokenKind.AiKeys: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiProviders))); break;
            case TokenKind.AiKey: EmitAiSetKeyCall(expr); break;
            case TokenKind.AiOcr: EmitAiOcrCall(expr); break;
            // F3 multimodale (2026-09-11).
            case TokenKind.AiImageInfo: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiImageInfo))); break;
            case TokenKind.AiImageResize: EmitAiImageResizeCall(expr); break;
            case TokenKind.AiImageCrop: EmitAiImageCropCall(expr); break;
            case TokenKind.AiImageConvert: EmitAiImageConvertCall(expr); break;
            // F2 multimodale (2026-09-11).
            case TokenKind.AiDocFields: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiDocFields))); break;
            case TokenKind.AiDocExtract: EmitAiDocExtractCall(expr); break;
            // F4 multimodale (2026-09-11).
            case TokenKind.AiSynthesize: EmitAiSynthesizeCall(expr); break;
            // F5 multimodale (2026-09-11).
            case TokenKind.AiImageEmbed: EmitAiImageEmbedCall(expr); break;
            case TokenKind.AiImageCompare: EmitAiImageCompareCall(expr); break;
            case TokenKind.AiImageDetect: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiImageDetect))); break;
            case TokenKind.AiImageClassify: EmitAiImageClassifyCall(expr); break;
            // F6 multimodale (2026-09-11).
            case TokenKind.AiAgentAskFile: EmitAiAgentAskFileCall(expr); break;
            // F7 multimodale (2026-09-11).
            case TokenKind.AiVideoInfo: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiVideoInfo))); break;
            case TokenKind.AiVideoFrame: EmitAiVideoFrameCall(expr); break;
            case TokenKind.AiVideoSnapshot: EmitAiVideoSnapshotCall(expr); break;
            case TokenKind.AiModel: EmitAiModelCall(expr); break;
             case TokenKind.AiInfo: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiInfo))); break;
              case TokenKind.AiSave: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiSaveConfig))); break;
              case TokenKind.AiLoad: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiLoadConfig))); break;
              case TokenKind.AiSaveConfig: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiSaveConfig))); break;
              case TokenKind.AiLoadConfig: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiLoadConfig))); break;
              case TokenKind.AiDef: EmitAiConfigBinaryOrUnaryCall(expr, nameof(IlRuntime.AiSetDefault), nameof(IlRuntime.AiGetDefault)); break;
              case TokenKind.AiSetDefault: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiSetDefault))); break;
              case TokenKind.AiGetDefault: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiGetDefault))); break;
             case TokenKind.AiOn: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiEnable))); break;
             case TokenKind.AiOff: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiDisable))); break;
             case TokenKind.AiTemp: EmitAiConfigBinaryOrUnaryCall(expr, nameof(IlRuntime.AiSetTemp), nameof(IlRuntime.AiGetTemp)); break;
             case TokenKind.AiBase: EmitAiConfigBinaryOrUnaryCall(expr, nameof(IlRuntime.AiSetBase), nameof(IlRuntime.AiGetBase)); break;
            case TokenKind.Collide: EmitSpriteCollideCall(expr, batch: false); break;
            case TokenKind.CollideBatch: EmitSpriteCollideCall(expr, batch: true); break;
            case TokenKind.JoyCount: EmitNoArgBuiltinCall(expr, IlRuntimeJoyCountMethod); break;
            case TokenKind.JoyPresent: EmitJoySingleArgCall(expr, IlRuntimeJoyPresentMethod); break;
            case TokenKind.JoyName: EmitJoySingleArgCall(expr, IlRuntimeJoyNameMethod); break;
            case TokenKind.Joy: EmitJoySingleArgCall(expr, IlRuntimeJoyMethod); break;
             case TokenKind.Create: EmitChartCreateCall(expr); break;
             case TokenKind.ChartCreate: EmitChartCreateCall(expr); break;

             // ==================================================================
             // GAP CHIUSI 2026-08-29 — 69 builtin ora dispatchati a IlRuntime
             // (runtime condiviso con PCodeVM, regola §0bis: una sola implementazione).
             // ==================================================================
             // JSON.ToObject
             case TokenKind.JsonToObject: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.JsonToObject), typeof(string), typeof(string))); break;
             // RDD.FLOAT / PROVIDER.REGISTERLOCAL
             case TokenKind.RddFloat: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.RddFloat), typeof(object))); break;
             case TokenKind.ProviderRegisterLocal: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.ProviderRegisterLocal), typeof(string))); break;
             // ── TORCH: DEVICE/LIBPATH/LOADED (letture) ──
             case TokenKind.TorchDevice: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.TorchDevice))); break;
             case TokenKind.TorchLibPath: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.TorchLibPath))); break;
             case TokenKind.TorchLoaded: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.TorchLoaded))); break;
             case TokenKind.TorchAvailable: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.TorchAvailable))); break;
             case TokenKind.TorchDtype: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.TorchDtype), typeof(long))); break;
             case TokenKind.TorchDeviceOf: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.TorchDeviceOf), typeof(long))); break;
             // ── TORCH: costruttori/aritmetica (handle long) ──
             case TokenKind.TorchCreate: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.TorchCreate), typeof(long), typeof(long))); break;
             case TokenKind.TorchFrom: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.TorchFrom), typeof(double[]))); break;
             case TokenKind.TorchAdd: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.TorchAdd), typeof(long), typeof(long))); break;
             case TokenKind.TorchSub: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.TorchSub), typeof(long), typeof(long))); break;
             case TokenKind.TorchMul: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.TorchMul), typeof(long), typeof(long))); break;
             case TokenKind.TorchDiv: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.TorchDiv), typeof(long), typeof(long))); break;
             case TokenKind.TorchMatmul: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.TorchMatmul), typeof(long), typeof(long))); break;
             case TokenKind.TorchRelu: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.TorchRelu), typeof(long))); break;
             case TokenKind.TorchZeros: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.TorchZeros), typeof(long), typeof(long))); break;
             case TokenKind.TorchOnes: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.TorchOnes), typeof(long), typeof(long))); break;
             case TokenKind.TorchEye: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.TorchEye), typeof(long))); break;
             case TokenKind.TorchRandn: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.TorchRandn), typeof(long), typeof(long))); break;
             case TokenKind.TorchTranspose: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.TorchTranspose), typeof(long))); break;
             case TokenKind.TorchReshape: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.TorchReshape), typeof(long), typeof(double), typeof(double))); break;
             case TokenKind.TorchSigmoid: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.TorchSigmoid), typeof(long))); break;
             case TokenKind.TorchTanh: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.TorchTanh), typeof(long))); break;
             case TokenKind.TorchSoftmax: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.TorchSoftmax), typeof(long))); break;
             case TokenKind.TorchExp: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.TorchExp), typeof(long))); break;
             case TokenKind.TorchLog: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.TorchLog), typeof(long))); break;
             case TokenKind.TorchSqrt: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.TorchSqrt), typeof(long))); break;
             case TokenKind.TorchPow: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.TorchPow), typeof(long), typeof(double))); break;
             case TokenKind.TorchAbs: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.TorchAbs), typeof(long))); break;
             case TokenKind.TorchGelu: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.TorchGelu), typeof(long))); break;
             case TokenKind.TorchPermute: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.TorchPermute), typeof(long), typeof(double), typeof(double))); break;
             case TokenKind.TorchSlice: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.TorchSlice), typeof(long), typeof(double), typeof(double), typeof(double))); break;
             case TokenKind.TorchConcat: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.TorchConcat), typeof(long), typeof(long))); break;
             case TokenKind.TorchStack: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.TorchStack), typeof(long), typeof(long))); break;
             case TokenKind.TorchSplit: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.TorchSplit), typeof(long), typeof(double), typeof(double))); break;
             case TokenKind.TorchFree: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.TorchFree), typeof(long))); break;
             case TokenKind.TorchSet: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.TorchSet), typeof(long), typeof(double), typeof(double), typeof(double))); break;
             case TokenKind.TorchModelCreate: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.TorchModelCreate), typeof(string), typeof(string))); break;
             case TokenKind.TorchModelTrainStep when expr.Args.Length > 6: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.TorchModelTrainStepEx), typeof(long), typeof(long), typeof(long), typeof(string), typeof(string), typeof(double), typeof(string))); break;
             case TokenKind.TorchModelTrainStep: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.TorchModelTrainStep), typeof(long), typeof(long), typeof(long), typeof(string), typeof(string), typeof(double))); break;
             case TokenKind.TorchModelForward: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.TorchModelForward), typeof(long), typeof(long))); break;
             case TokenKind.TorchModelParameters: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.TorchModelParameters), typeof(long))); break;
             case TokenKind.TorchModelSave: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.TorchModelSave), typeof(long), typeof(string))); break;
             case TokenKind.TorchModelReset: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.TorchModelReset), typeof(long))); break;
             case TokenKind.TorchModelInput: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.TorchModelInput), typeof(long))); break;
             case TokenKind.TorchModelOutput: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.TorchModelOutput), typeof(long))); break;
             case TokenKind.TorchModelState: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.TorchModelState), typeof(long))); break;
             case TokenKind.TorchModelFree: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.TorchModelFree), typeof(long))); break;
             // ── TORCH: scalari (double) ──
             case TokenKind.TorchGet: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.TorchGet), typeof(long), typeof(double), typeof(double))); break;
             case TokenKind.TorchSum: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.TorchSum), typeof(long))); break;
             case TokenKind.TorchMean: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.TorchMean), typeof(long))); break;
             case TokenKind.TorchMax: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.TorchMax), typeof(long))); break;
             case TokenKind.TorchMin: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.TorchMin), typeof(long))); break;
             case TokenKind.TorchSimilarity: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.TorchSimilarity), typeof(long), typeof(long), typeof(string))); break;
             case TokenKind.TorchBleu: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.TorchBleu), typeof(string), typeof(string))); break;
             case TokenKind.TorchRouge: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.TorchRouge), typeof(string), typeof(string))); break;
             case TokenKind.TorchDpoStep when expr.Args.Length > 5: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.TorchModelDpoStep), typeof(long), typeof(long), typeof(long), typeof(long), typeof(long), typeof(string))); break;
             case TokenKind.TorchDpoStep: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.TorchModelDpoStep), typeof(long), typeof(long), typeof(long), typeof(long), typeof(long))); break;
             case TokenKind.TorchTextEncode: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.TorchTextEncode), typeof(string))); break;
             case TokenKind.TorchTextDecode: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.TorchTextDecode), typeof(long))); break;
             case TokenKind.TorchDatasetLoad when expr.Args.Length > 1: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.TorchDatasetLoad), typeof(string), typeof(string))); break;
             case TokenKind.TorchDatasetLoad: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.TorchDatasetLoad), typeof(string))); break;
             case TokenKind.TorchModelExport when expr.Args.Length > 2: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.TorchModelExport), typeof(long), typeof(string), typeof(string))); break;
             case TokenKind.TorchModelExport: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.TorchModelExport), typeof(long), typeof(string))); break;
             case TokenKind.TorchImatrixLoad: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.TorchImatrixLoad), typeof(string))); break;
             case TokenKind.TorchImatrixRanks: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.TorchImatrixRanks), typeof(string), typeof(long), typeof(long))); break;
             case TokenKind.TorchLossMse: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.TorchLossMse), typeof(long), typeof(long))); break;
             case TokenKind.TorchLossCrossEntropy: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.TorchLossCrossEntropy), typeof(long), typeof(long))); break;
             case TokenKind.TorchLossBinary: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.TorchLossBinary), typeof(long), typeof(long))); break;
             case TokenKind.TorchAccuracy: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.TorchAccuracy), typeof(long), typeof(long))); break;
             case TokenKind.TorchModelLoss: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.TorchModelLoss), typeof(long), typeof(long), typeof(long))); break;
             // ── TORCH: bool ──
             case TokenKind.TorchAllclose: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.TorchAllclose), typeof(long), typeof(long), typeof(double))); break;
             // ── TORCH: array (double[]) ──
             case TokenKind.TorchToArray: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.TorchToArray), typeof(long))); break;
             case TokenKind.TorchTopK: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.TorchTopK), typeof(long), typeof(double))); break;
             case TokenKind.TorchArgmax: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.TorchArgmax), typeof(long))); break;
             case TokenKind.TorchArgsort: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.TorchArgsort), typeof(long))); break;
             case TokenKind.TorchOneHot: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.TorchOneHot), typeof(double), typeof(double))); break;
             case TokenKind.TorchClip: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.TorchClip), typeof(long), typeof(double), typeof(double))); break;
             case TokenKind.TorchNormalize: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.TorchNormalize), typeof(long))); break;
             case TokenKind.TorchStandardize: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.TorchStandardize), typeof(long))); break;
             case TokenKind.TorchShape: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.TorchShape), typeof(long))); break;

             // ==================================================================
             // GAP CHIUSI 2026-08-29 (2ª ondata) — letture DBF/ISAM/SPEED/RDD/NET/ARRAYS
             // ==================================================================
             case TokenKind.Sleep: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.SleepAsync), typeof(double))); break;
             case TokenKind.DbRelation: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.DbfRelation), typeof(string))); break;
             case TokenKind.IndexKey: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.DbfIndexKey), typeof(string))); break;
             case TokenKind.IndexOrd: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.DbfIndexOrd), typeof(string))); break;
             case TokenKind.FieldName: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.DbfFieldName), typeof(object), typeof(object))); break;             case TokenKind.NetErr: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.DbfNetErr))); break;
             case TokenKind.DbfLookup: EmitVariadicRuntimeCall(expr, nameof(IlRuntime.DbfLookup)); break;
            case TokenKind.DbfReplicationShipTcp: EmitVariadicRuntimeCall(expr, nameof(IlRuntime.DbfReplicationShip)); break;
            case TokenKind.DbfReplicationRecvTcp: EmitVariadicRuntimeCall(expr, nameof(IlRuntime.DbfReplicationRecv)); break;
            case TokenKind.SpeedInfo: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.SpeedInfo), typeof(string))); break;
            case TokenKind.SpeedValidate: EmitSpeedValidateCall(expr); break;
            case TokenKind.ToolSearch: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.ToolSearch))); break;
            case TokenKind.ToolRegister: EmitToolRegisterCall(expr); break;
            case TokenKind.UiFind: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.UiFind))); break;
            case TokenKind.UiClick: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.UiClick))); break;
            case TokenKind.UiFindAll: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.UiFindAll))); break;
            case TokenKind.UiType: EmitUiTypeCall(expr); break;
            case TokenKind.BrowserNavigate: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.BrowserNavigate))); break;
            case TokenKind.BrowserSnapshot: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.BrowserSnapshot))); break;
            case TokenKind.BrowserAccessTree: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.BrowserAccessTree))); break;
            case TokenKind.BrowserScreenshot: EmitBrowserScreenshotCall(expr); break;
            case TokenKind.BrowserGetCookies: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.BrowserGetCookies))); break;
            case TokenKind.BrowserSetCookie: EmitBrowserSetCookieCall(expr); break;
            case TokenKind.BrowserClearCookies: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.BrowserClearCookies))); break;
            case TokenKind.BrowserSetHeader: EmitStringBinaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.BrowserSetHeader))); break;
            case TokenKind.BrowserSetUserAgent: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.BrowserSetUserAgent))); break;
            case TokenKind.BrowserHumanClick: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.BrowserHumanClick))); break;
            case TokenKind.BrowserHumanFill: EmitStringBinaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.BrowserHumanFill))); break;
            case TokenKind.BrowserHumanScroll: EmitBrowserHumanScrollCall(expr); break;
            case TokenKind.BrowserObserve: EmitBrowserObserveCall(expr); break;
            case TokenKind.BrowserFind: EmitBrowserFindCall(expr); break;
            case TokenKind.BrowserNetLog: EmitBrowserNetLogCall(expr); break;
            case TokenKind.BrowserConsole: EmitBrowserConsoleCall(expr); break;
            case TokenKind.BrowserAuth: EmitBrowserAuthCall(expr); break;
            case TokenKind.BrowserClick: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.BrowserClick))); break;
            case TokenKind.BrowserFill: EmitStringBinaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.BrowserFill))); break;
            case TokenKind.BrowserEvaluate: EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.BrowserEvaluate))); break;
            case TokenKind.BrowserStart: EmitBrowserStartCall(expr); break;
            case TokenKind.AgentRun: EmitAgentRunCall(expr); break;
            case TokenKind.AgentRunCua: EmitAgentRunCuaCall(expr); break;
            case TokenKind.AgentRunGraph: EmitAgentRunGraphCall(expr); break;
            case TokenKind.AgentRunCode: EmitAgentRunCodeCall(expr); break;
            case TokenKind.AgentRunRag: EmitAgentRunRagCall(expr); break;
            case TokenKind.ComputerScreenshot: EmitComputerScreenshotCall(expr); break;
            case TokenKind.ComputerScreenRecord: EmitComputerScreenRecordCall(expr); break;
            case TokenKind.ComputerScreenRecordStart: EmitComputerScreenRecordCall(expr); break;
            case TokenKind.ComputerScreenRecordStop: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.ComputerScreenRecordStop))); break;
            case TokenKind.ComputerScreenRecordStatus: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.ComputerScreenRecordStatus))); break;
            case TokenKind.ComputerClick: EmitNumericCall(expr, RuntimeMethod(nameof(IlRuntime.ComputerClick), typeof(double), typeof(double))); break;
            case TokenKind.ComputerMove: EmitNumericCall(expr, RuntimeMethod(nameof(IlRuntime.ComputerMove), typeof(double), typeof(double))); break;
            case TokenKind.ComputerType: EmitStringToDoubleCall(expr, RuntimeMethod(nameof(IlRuntime.ComputerType), typeof(string))); break;
            case TokenKind.ComputerKey: EmitStringToDoubleCall(expr, RuntimeMethod(nameof(IlRuntime.ComputerKey), typeof(string))); break;
            case TokenKind.ComputerDoubleClick: EmitNumericCall(expr, RuntimeMethod(nameof(IlRuntime.ComputerDoubleClick), typeof(double), typeof(double))); break;
            case TokenKind.ComputerHotKey: EmitStringToDoubleCall(expr, RuntimeMethod(nameof(IlRuntime.ComputerHotKey), typeof(string))); break;
            // AI OBSERVE (P1 todo AI-Runtime): azioni del comando + letture registry.
            case TokenKind.AiObserveFile: EmitStringToDoubleCall(expr, RuntimeMethod(nameof(IlRuntime.AiObserveFile), typeof(string))); break;
            case TokenKind.AiObserveLog: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.AiObserveLog), typeof(string), typeof(double))); break;
            case TokenKind.AiObserveScreen: EmitStringToDoubleCall(expr, RuntimeMethod(nameof(IlRuntime.AiObserveScreen), typeof(string))); break;
            case TokenKind.AiObservationLatest: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.ObservationLatest))); break;
            case TokenKind.AiObservationGet: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.Observation), typeof(double))); break;
            case TokenKind.AiObservationHash: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.ObservationHash), typeof(double))); break;
            case TokenKind.AiObservationCount: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.ObservationCount))); break;
            case TokenKind.AiObservationList: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.ObservationList))); break;
            case TokenKind.AiObservationFile: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.ObservationFile), typeof(double))); break;
            case TokenKind.ComputerWindows: EmitNoArgBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.ComputerWindows))); break;
             case TokenKind.IsamLog: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.IsamLog), typeof(string))); break;
             case TokenKind.IsamFollowReplication: EmitVariadicRuntimeCall(expr, nameof(IlRuntime.IsamFollowReplication)); break;
             case TokenKind.IsamReplicationShipTcp: EmitVariadicRuntimeCall(expr, nameof(IlRuntime.IsamReplicationShip)); break;
             case TokenKind.IsamReplicationRecvTcp: EmitVariadicRuntimeCall(expr, nameof(IlRuntime.IsamReplicationRecv)); break;
             case TokenKind.AAdd: EmitArrayTypedCall(expr, nameof(IlRuntime.AAdd)); break;
             case TokenKind.AIns: EmitArrayTypedCall(expr, nameof(IlRuntime.AIns)); break;
             case TokenKind.ADel: EmitArrayTypedCall(expr, nameof(IlRuntime.ADel)); break;
             case TokenKind.AFill: EmitArrayTypedCall(expr, nameof(IlRuntime.AFill)); break;
             case TokenKind.ACopy: EmitArrayTypedCall(expr, nameof(IlRuntime.ACopy)); break;
             case TokenKind.AEval: EmitAEvalCall(expr); break;
             case TokenKind.Recv: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.NetRecv), typeof(object))); break;
             case TokenKind.Evolve: EmitTypedRuntimeCall(expr, RuntimeMethod(nameof(IlRuntime.PortfolioEvolve), typeof(double[]), typeof(double[][]), typeof(object))); break;
             default:
                if (Environment.GetEnvironmentVariable("GAI_DBG") == "1")
                    Console.WriteLine($"[BUILTIN-DBG] kind={expr.Name.Kind} token={expr.OriginalToken.Kind} lex={expr.OriginalToken.Lexeme} ns={expr.Namespace!.Lexeme}");
                throw new NotSupportedException(
                    $"Backend IL: funzione builtin '{expr.OriginalToken.Lexeme}' non ancora supportata.");
        }
    }
}
