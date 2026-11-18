// FILE: Compiler/ChunkMerger.cs
// Linker di chunk P-Code: fonde i chunk VM dei pacchetti REQUIRE (vm-chunk) nel chunk
// principale PRIMA dell'esecuzione (managed PCodeVm e VM/C via serializzazione A1/GMBC).
//
// Il re-basing è per-istruzione (tabella derivata dal Disassembler/PCodeProcessor):
//   - costanti  → +constBase   (indici u16 nella constant pool)
//   - globali   → +globalBase  (slot u16 della tabella globals)
//   - indirizzi → +codeBase    (target assoluti di CALL/GOTO/GOSUB/ON JUMP/GO SELECT...)
//   - dataPos   → +dataBase    (OP_RESTORE)
//   - costanti-indirizzo (OP_PUSH_CODE_REF/OP_CLOSURE): anche il VALORE (double) nella
//     pool è un indirizzo di funzione → +codeBase.
// Locali/upvalue/salti relativi/codici builtin/syscall/subcode/conteggi restano invariati.
// NOTA (limite noto): gli static field (OP_LOAD/STORE_STATIC_FIELD) usano id simbolo VM
// locali al chunk — non ribasati: chunk con static field non vanno fusi in librerie.

using System.Collections;

namespace Compiler;

/// <summary>Fonde chunk P-Code (librerie vm-chunk di pacchetto) nel chunk principale.
/// Se <paramref name="externNames"/> è fornito (nomi dell'API dichiarata nelle exports del
/// pacchetto), le chiamate compile-time del main a quei nomi vengono RIDIRETTE verso
/// l'implementazione nel chunk (linking cross-chunk): senza nuovo syntax, perché una
/// procedura locale non può condividere il nome con un export (errore di definizione
/// duplicata al link). Se <paramref name="externNames"/> è null, linka per nome qualsiasi
/// procedura del main presente anche in una lib.</summary>
public static class ChunkMerger
{
    private enum Kind { Const, Global, Addr, DataPos }

    /// <summary>Ritorna il chunk principale con i chunk delle librerie appesi (re-basati) e
    /// le chiamate cross-chunk linkate.</summary>
    public static CompiledChunk Merge(CompiledChunk main, IReadOnlyList<CompiledChunk> libraries,
        IReadOnlySet<string>? externNames = null)
    {
        var merged = new CompiledChunk
        {
            EntryPoint = main.EntryPoint,
            GlobalCount = main.GlobalCount,
            LocalCount = main.LocalCount,
            GlobalNames = main.GlobalNames.Length > 0 ? [.. main.GlobalNames] : new string[main.GlobalCount],
        };
        merged.Code.AddRange(main.Code);
        merged.Lines.AddRange(main.Lines);
        merged.Constants.AddRange(main.Constants);
        merged.Data.AddRange(main.Data);
        foreach (var (k, v) in main.ProcedureAddresses) merged.ProcedureAddresses[k] = v;
        foreach (var (k, v) in main.ProcedureArities) merged.ProcedureArities[k] = v;
        foreach (var (tryIp, region) in main.TryRegions) merged.TryRegions[tryIp] = region;
        merged.StatementRanges.AddRange(main.StatementRanges);
        merged.UsesTray = main.UsesTray;
        foreach (var (k, v) in main.ProcedureLocalNumeric) merged.ProcedureLocalNumeric[k] = v;

        // nome → indirizzo ribasato nella lib (le lib successive sovrascrivono)
        var libProcs = new Dictionary<string, int>(StringComparer.OrdinalIgnoreCase);

        foreach (var lib in libraries)
        {
            if (lib == null || lib.Code.Count == 0) continue;
            var codeBase = merged.Code.Count;
            var constBase = merged.Constants.Count;
            var globalBase = merged.GlobalCount;
            var dataBase = merged.Data.Count;
            merged.LocalCount = Math.Max(merged.LocalCount, lib.LocalCount);

            var addrConsts = new HashSet<int>();
            merged.Code.AddRange(RebaseCode(lib.Code, constBase, globalBase, codeBase, dataBase, addrConsts));
            merged.Lines.AddRange(lib.Lines.Count > 0
                ? lib.Lines.Select(l => l)
                : Enumerable.Repeat(0, lib.Code.Count));

            for (var i = 0; i < lib.Constants.Count; i++)
            {
                var c = lib.Constants[i];
                if (addrConsts.Contains(i) && c is double d)
                    merged.Constants.Add(d + codeBase); // costante-indirizzo → +codeBase
                else
                    merged.Constants.Add(c);
            }

            merged.Data.AddRange(lib.Data);

            // "Main"/"__main"/"__global_init"/"__static_init" NON sono mai stub da linkare
            // (vedi l'esclusione per NOME sotto) — ma il compilatore registra per OGNI
            // procedura, oltre al nome nudo, un alias "Nome#arità" (Compiler.cs:1198,
            // "Main#0") e un alias "@sym<SymbolId>" (Compiler.cs:1202) che puntano alla STESSA
            // procedura. Un alias NON è escluso dal controllo per nome (è una stringa
            // diversa), quindi "Main#0" di una libreria rediriggeva silenziosamente "Main#0"
            // del main (stessa chiave, presente in OGNI programma con 0 parametri) verso
            // l'indirizzo della libreria — bug: un chunk fuso eseguiva il Main della libreria
            // anche partendo dal proprio EntryPoint invariato (riproducibile SENZA alcuna
            // FFI). Qui si esclude dal linking qualunque NOME (nudo o alias) la cui ADDRESS
            // ORIGINALE (non ribasata) coincide con quella di uno dei 4 nomi riservati in
            // QUESTA libreria — indipendentemente dalla stringa dell'alias.
            var reservedLibAddrs = new HashSet<int>();
            foreach (var reservedName in new[] { "Main", "__main", "__global_init", "__static_init" })
                if (lib.ProcedureAddresses.TryGetValue(reservedName, out var reservedAddr))
                    reservedLibAddrs.Add(reservedAddr);
            foreach (var (name, addr) in lib.ProcedureAddresses)
            {
                merged.ProcedureAddresses[name] = addr + codeBase;
                if (!reservedLibAddrs.Contains(addr)) libProcs[name] = addr + codeBase;
            }
            foreach (var (addr, arity) in lib.ProcedureArities)
                merged.ProcedureArities[addr + codeBase] = arity;
            foreach (var (st, en) in lib.StatementRanges)
                merged.StatementRanges.Add((st + codeBase, en + codeBase));
            foreach (var (addr, flags) in lib.ProcedureLocalNumeric)
                merged.ProcedureLocalNumeric.TryAdd(addr + codeBase, flags);
            merged.UsesTray |= lib.UsesTray;
            foreach (var (tryIp, region) in lib.TryRegions)
                merged.TryRegions[tryIp + codeBase] = new TryRegionMetadata(
                    region.FinallyIp + codeBase,
                    region.EndIp + codeBase,
                    region.HasCatch);

            // GlobalNames: estende il vettore al nuovo GlobalCount con i nomi della lib.
            var names = merged.GlobalNames;
            var oldLen = names.Length;
            var newLen = merged.GlobalCount + lib.GlobalCount;
            Array.Resize(ref names, newLen);
            for (var i = 0; i < lib.GlobalCount; i++)
                names[oldLen + i] = i < lib.GlobalNames.Length ? lib.GlobalNames[i] ?? "" : "";
            merged.GlobalNames = names;
            merged.GlobalCount = newLen;
        }

        // --- Linking cross-chunk: ridirige le chiamate del main verso le implementazioni nei
        // chunk dei pacchetti (nomi dell'API dichiarata nelle exports / nomi in comune). ---
        if (libProcs.Count > 0)
        {
            var mainCodeLen = main.Code.Count;
            var redirect = new Dictionary<int, int>();
            foreach (var (name, libAddr) in libProcs)
            {
                // "Main" (entry-point auto-eseguito) e i nomi sintetizzati dal compilatore
                // ("__main"/"__global_init"/"__static_init", uno per chunk) NON sono mai stub
                // da linkare a un'implementazione esterna: main e libreria hanno ENTRAMBI una
                // propria versione con semantica indipendente e NON derivata da import/export
                // (il main/init del programma e l'eventuale main/init del pacchetto REQUIRE,
                // mai eseguiti come entry quando fusi come libreria). Senza questa esclusione,
                // qualunque riferimento nel codice del main al proprio "Main"/"__main" veniva
                // silenziosamente rediretto verso l'omonimo della libreria (bug: chunk fuso
                // eseguiva una procedura diversa pur partendo dal proprio EntryPoint invariato).
                if (name.Equals("Main", StringComparison.OrdinalIgnoreCase)
                    || name.Equals("__main", StringComparison.OrdinalIgnoreCase)
                    || name.Equals("__global_init", StringComparison.OrdinalIgnoreCase)
                    || name.Equals("__static_init", StringComparison.OrdinalIgnoreCase))
                    continue;
                if (externNames != null && !externNames.Contains(name)) continue;
                if (main.ProcedureAddresses.TryGetValue(name, out var stubAddr) && stubAddr != libAddr)
                    redirect[stubAddr] = libAddr;
            }
            if (externNames != null)
            {
                // ogni nome dell'API deve avere un'implementazione in qualche chunk
                var missing = externNames.Where(n => !libProcs.ContainsKey(n)).ToList();
                if (missing.Count > 0)
                    throw new InvalidOperationException(
                        $"Chunk extern senza implementazione nei pacchetti vm-chunk risolti: {string.Join(", ", missing)}. " +
                        "Il chunk del pacchetto deve fornire ogni procedura dichiarata nelle exports.");
            }
            if (redirect.Count > 0)
                ApplyRedirects(merged.Code, mainCodeLen, redirect, merged.Constants);
        }

        return merged;
    }

    /// <summary>Ridirige nel codice del main (primi <paramref name="length"/> byte) ogni
    /// riferimento all'indirizzo di uno stub dell'API verso l'implementazione nel chunk.
    /// PATCHA IN PLACE <paramref name="code"/>; le costanti-indirizzo (OP_PUSH_CODE_REF /
    /// OP_CLOSURE) vengono aggiornate in <paramref name="constants"/>.</summary>
    private static void ApplyRedirects(List<byte> code, int length, Dictionary<int, int> redirect,
        List<object?> constants)
    {
        var offset = 0;
        while (offset < length)
        {
            var op = (OpCode)code[offset];
            var o = offset + 1;

            void PatchAddr()
            {
                if (o + 1 >= code.Count) { o += 2; return; }
                var v = (code[o] << 8) | code[o + 1];
                if (redirect.TryGetValue(v, out var nv))
                {
                    code[o] = (byte)((nv >> 8) & 0xFF);
                    code[o + 1] = (byte)(nv & 0xFF);
                }
                o += 2;
            }
            void PatchAddrConst()
            {
                if (o + 1 >= code.Count) { o += 2; return; }
                var ci = (code[o] << 8) | code[o + 1];
                if (ci >= 0 && ci < constants.Count && constants[ci] is double d
                    && redirect.TryGetValue((int)d, out var nv))
                    constants[ci] = (double)nv;
                o += 2;
            }
            void CopyU16() { o += 2; }
            void CopyU8() { o += 1; }

            switch (op)
            {
                case OpCode.OP_CALL_CTOR: case OpCode.OP_CALL: case OpCode.OP_GOTO: case OpCode.OP_GOSUB:
                case OpCode.OP_GO: case OpCode.OP_SET_ERR_HANDLER_GOTO: case OpCode.OP_SET_ERR_HANDLER_GOSUB:
                case OpCode.OP_PUSH_TRY: case OpCode.OP_RESUME: case OpCode.OP_GO_FUNCTION:
                    PatchAddr(); break;
                case OpCode.OP_CALL_N: case OpCode.OP_NEW_GENERATOR_CALL:
                    PatchAddr(); CopyU8(); break;
                case OpCode.OP_PUSH_CODE_REF:
                    PatchAddrConst(); break;
                case OpCode.OP_CLOSURE:
                    PatchAddrConst();
                    { var n = o < code.Count ? code[o] : 0; CopyU8(); for (var i = 0; i < n; i++) { CopyU8(); CopyU8(); } }
                    break;
                case OpCode.OP_ON_JUMP:
                {
                    var n = o < code.Count ? code[o] : 0; CopyU8(); CopyU8();
                    for (var i = 0; i < n; i++) PatchAddr();
                    break;
                }
                case OpCode.OP_GO_SELECT_SETUP:
                {
                    var n = o + 1 < code.Count ? (code[o] << 8) | code[o + 1] : 0;
                    CopyU16(); CopyU16();
                    PatchAddr();
                    for (var i = 0; i < n; i++) PatchAddr();
                    break;
                }

                // --- istruzioni senza indirizzi assoluti: avanza con le stesse larghezze ---
                case OpCode.OP_JUMP_FORWARD: case OpCode.OP_JUMP_IF_TRUE: case OpCode.OP_JUMP_IF_FALSE:
                case OpCode.OP_JUMP_BACKWARD:
                    CopyU16(); break;
                case OpCode.OP_CALL_FFI:
                    CopyU16(); CopyU16(); CopyU8(); break;
                case OpCode.OP_CALL_FFI_T:
                {
                    // Formato completo in Compiler/FfiCallFormat.cs (descrittore per-argomento +
                    // coda foglie UDT + tabella firme callback): nessun operando è un indirizzo
                    // di salto qui, quindi si avanza soltanto.
                    var lay = FfiCallFormat.Describe(code, offset);
                    o = offset + lay.Length;
                    break;
                }
                case OpCode.OP_CALL_BUILTIN:
                    CopyU16(); CopyU8(); break;
                case OpCode.OP_SYSCALL:
                    CopyU16(); CopyU8(); break;
                case OpCode.OP_FILE_CMD: case OpCode.OP_NET_CMD: case OpCode.OP_CLIP_CMD:
                case OpCode.OP_MATRIX_CMD: case OpCode.OP_VECTOR_CMD: case OpCode.OP_AUDIO_CMD:
                    CopyU8(); CopyU8(); break;
                case OpCode.OP_NEW_CHANNEL:
                    CopyU8(); break; // capacity u8 (make(chan T, n))
                case OpCode.OP_DEFER:
                    CopyU16(); break; // indirizzo body DEFER (interno, no rebase)
                case OpCode.OP_ONCE_BEGIN:
                    CopyU16(); break; // indirizzo skip (interno, no rebase)
                case OpCode.OP_REDIM: case OpCode.OP_REDIM_PRESERVE:
                    CopyU8(); CopyU16(); break;
                case OpCode.OP_RESTORE:
                    CopyU16(); break;
                case OpCode.OP_NEW:
                case OpCode.OP_LOAD_MEMBER: case OpCode.OP_STORE_MEMBER: case OpCode.OP_MK_ARRAY:
                case OpCode.OP_MK_DICT: case OpCode.OP_MK_LIST: case OpCode.OP_INIT_ARRAY:
                case OpCode.OP_ADJUST_STRING: case OpCode.OP_LOAD_STATIC_FIELD: case OpCode.OP_STORE_STATIC_FIELD:
                    CopyU16(); break;
                case OpCode.OP_FOR_LOOP:
                    CopyU16(); CopyU16(); break;
                case OpCode.OP_NATIVE_LOOP_ACC:
                    o += 12; break;
                case OpCode.OP_NATIVE_BUILTIN_ASSIGN:
                    o += 6; break;
                case OpCode.OP_NATIVE_STRING_APPEND_LOOP:
                    o += 7; break;
                case OpCode.OP_POP: case OpCode.OP_PUSH_NIL: case OpCode.OP_PUSH_TRUE: case OpCode.OP_PUSH_FALSE:
                case OpCode.OP_ADD: case OpCode.OP_SUBTRACT: case OpCode.OP_MULTIPLY: case OpCode.OP_DIVIDE:
                case OpCode.OP_INT_DIVIDE: case OpCode.OP_POWER: case OpCode.OP_NEGATE: case OpCode.OP_NOT:
                case OpCode.OP_MOD: case OpCode.OP_CMP_EQ: case OpCode.OP_CMP_NE: case OpCode.OP_CMP_GT:
                case OpCode.OP_CMP_GTE: case OpCode.OP_CMP_LT: case OpCode.OP_CMP_LTE: case OpCode.OP_DUP:
                case OpCode.OP_RETURN: case OpCode.OP_HALT: case OpCode.OP_GO_EXIT: case OpCode.OP_SEND: case OpCode.OP_RECV:
                case OpCode.OP_WG_WAIT: case OpCode.OP_WG_DONE: case OpCode.OP_SLEEP: case OpCode.OP_ON_KEY:
                case OpCode.OP_RECV_OK: case OpCode.OP_CHANNEL_CLOSE:
                case OpCode.OP_NEW_MUTEX: case OpCode.OP_NEW_RWMUTEX: case OpCode.OP_NEW_ONCE: case OpCode.OP_NEW_ATOMIC: case OpCode.OP_NEW_POOL: case OpCode.OP_NEW_COND: case OpCode.OP_NEW_CONCURRENTMAP: case OpCode.OP_LOCK: case OpCode.OP_RLOCK: case OpCode.OP_UNLOCK: case OpCode.OP_RUNLOCK: case OpCode.OP_ONCE_END: case OpCode.OP_COND_WAIT: case OpCode.OP_COND_SIGNAL: case OpCode.OP_COND_BROADCAST:
                case OpCode.OP_POP_TRY: case OpCode.OP_READ: case OpCode.OP_THROW: case OpCode.OP_NEXT_GENERATOR:
                case OpCode.OP_RESOLVE_FUTURE: case OpCode.OP_EVAL_MACRO_STMT: case OpCode.OP_EVAL_MACRO_EXPR:
                case OpCode.OP_PRINT_VALUE: case OpCode.OP_PRINT_TAB_ZONE: case OpCode.OP_PRINT_NEWLINE:
                case OpCode.OP_PRINT_LINE: case OpCode.OP_YIELD: case OpCode.OP_BREAKPOINT:
                case OpCode.OP_CLEAR_RETURN: case OpCode.OP_CLONE_UDT:
                case OpCode.OP_NEW_GENERATOR_EXPR: case OpCode.OP_NEW_WAITGROUP:
                case OpCode.OP_END_VALID: case OpCode.OP_ON_MOUSE:
                case OpCode.OP_ARRAY_LEN: case OpCode.OP_MAKE_BYREF_ELEMENT:
                    break;
                case OpCode.OP_LOAD_LOCAL: case OpCode.OP_STORE_LOCAL: case OpCode.OP_PUSH_REF_LOCAL:
                case OpCode.OP_LOAD_UPVALUE: case OpCode.OP_STORE_UPVALUE: case OpCode.OP_CALL_INDIRECT:
                case OpCode.OP_IS_TYPE: case OpCode.OP_MAKE_BYREF: case OpCode.OP_FE_DESTRUCT:
                case OpCode.OP_FINALIZE_SCOPE: case OpCode.OP_FOR_PREPARE: case OpCode.OP_LOAD_ELEMENT:
                case OpCode.OP_LOAD_ELEMENT_REF:
                case OpCode.OP_LOAD_ELEMENT_POSITIONAL: case OpCode.OP_STORE_ELEMENT: case OpCode.OP_FETCH:
                case OpCode.OP_PUSH_CONST: case OpCode.OP_LOAD_FIELD: case OpCode.OP_STORE_FIELD:
                case OpCode.OP_NEW_CLASS_INSTANCE: case OpCode.OP_NEW_UDT_INSTANCE: case OpCode.OP_ATTACH_UDT_DESC: case OpCode.OP_CALL_MEMBER:
                case OpCode.OP_DEFINE_GLOBAL: case OpCode.OP_LOAD_GLOBAL: case OpCode.OP_STORE_GLOBAL:
                case OpCode.OP_PUSH_REF_GLOBAL: case OpCode.OP_MAKE_BYREF_GLOBAL: case OpCode.OP_FINALIZE_GLOBAL:
                case OpCode.OP_LOAD_DYNAMIC_SCOPE:
                    CopyU16(); break;
                default:
                    offset = length; break;
            }
            offset = o;
        }
    }

    /// <summary>Re-basa gli operandi del bytecode di una libreria (indici costanti/globali,
    /// indirizzi assoluti, dataPos). Restituisce il bytecode patchato e l'insieme degli
    /// indici costante che contengono INDIRIZZI di funzione (valori da ribasare a loro volta).</summary>
    private static List<byte> RebaseCode(IReadOnlyList<byte> code, int constBase, int globalBase,
        int codeBase, int dataBase, HashSet<int> addrConsts)
    {
        var result = new List<byte>(code.Count);
        var offset = 0;
        while (offset < code.Count)
        {
            var op = (OpCode)code[offset];
            // copia l'opcode
            result.Add(code[offset]);
            var o = offset + 1;

            // Legge un u16 big-endian a `o`, lo ribasa e lo appende; avanza `o` di 2.
            void PatchU16(Kind kind)
            {
                if (o + 1 >= code.Count) { o += 2; return; }
                var v = (code[o] << 8) | code[o + 1];
                v = kind switch
                {
                    Kind.Const => v + constBase,
                    Kind.Global => v + globalBase,
                    Kind.Addr => v + codeBase,
                    Kind.DataPos => v + dataBase,
                    _ => v
                };
                result.Add((byte)((v >> 8) & 0xFF));
                result.Add((byte)(v & 0xFF));
                o += 2;
            }
            void CopyRaw(int n)
            {
                for (var i = 0; i < n && o + i < code.Count; i++) result.Add(code[o + i]);
                o += n;
            }
            void CopyU16()
            {
                if (o + 1 < code.Count) { result.Add(code[o]); result.Add(code[o + 1]); }
                o += 2;
            }
            void CopyU8()
            {
                if (o < code.Count) result.Add(code[o]);
                o += 1;
            }

            switch (op)
            {
                // ── 1 byte, nessun operando ──
                case OpCode.OP_POP: case OpCode.OP_PUSH_NIL: case OpCode.OP_PUSH_TRUE: case OpCode.OP_PUSH_FALSE:
                case OpCode.OP_ADD: case OpCode.OP_SUBTRACT: case OpCode.OP_MULTIPLY: case OpCode.OP_DIVIDE:
                case OpCode.OP_INT_DIVIDE: case OpCode.OP_POWER: case OpCode.OP_NEGATE: case OpCode.OP_NOT:
                case OpCode.OP_MOD: case OpCode.OP_CMP_EQ: case OpCode.OP_CMP_NE: case OpCode.OP_CMP_GT:
                case OpCode.OP_CMP_GTE: case OpCode.OP_CMP_LT: case OpCode.OP_CMP_LTE: case OpCode.OP_DUP:
                case OpCode.OP_RETURN: case OpCode.OP_HALT: case OpCode.OP_GO_EXIT: case OpCode.OP_SEND: case OpCode.OP_RECV:
                case OpCode.OP_WG_WAIT: case OpCode.OP_WG_DONE: case OpCode.OP_SLEEP: case OpCode.OP_ON_KEY:
                case OpCode.OP_RECV_OK: case OpCode.OP_CHANNEL_CLOSE:
                case OpCode.OP_NEW_MUTEX: case OpCode.OP_NEW_RWMUTEX: case OpCode.OP_NEW_ONCE: case OpCode.OP_NEW_ATOMIC: case OpCode.OP_NEW_POOL: case OpCode.OP_NEW_COND: case OpCode.OP_NEW_CONCURRENTMAP: case OpCode.OP_LOCK: case OpCode.OP_RLOCK: case OpCode.OP_UNLOCK: case OpCode.OP_RUNLOCK: case OpCode.OP_ONCE_END: case OpCode.OP_COND_WAIT: case OpCode.OP_COND_SIGNAL: case OpCode.OP_COND_BROADCAST:
                case OpCode.OP_POP_TRY: case OpCode.OP_READ: case OpCode.OP_THROW: case OpCode.OP_NEXT_GENERATOR:
                case OpCode.OP_RESOLVE_FUTURE: case OpCode.OP_EVAL_MACRO_STMT: case OpCode.OP_EVAL_MACRO_EXPR:
                case OpCode.OP_PRINT_VALUE: case OpCode.OP_PRINT_TAB_ZONE: case OpCode.OP_PRINT_NEWLINE:
                case OpCode.OP_PRINT_LINE: case OpCode.OP_YIELD: case OpCode.OP_BREAKPOINT:
                case OpCode.OP_CLEAR_RETURN: case OpCode.OP_CLONE_UDT:
                case OpCode.OP_NEW_GENERATOR_EXPR: case OpCode.OP_NEW_WAITGROUP:
                case OpCode.OP_END_VALID: case OpCode.OP_ON_MOUSE:
                case OpCode.OP_ARRAY_LEN: case OpCode.OP_MAKE_BYREF_ELEMENT:
                    break;

                // ── 2 byte operando (slot locale u16, formato §20.3) — niente rebase ──
                case OpCode.OP_LOAD_LOCAL: case OpCode.OP_STORE_LOCAL: case OpCode.OP_PUSH_REF_LOCAL:
                case OpCode.OP_MAKE_BYREF: case OpCode.OP_FINALIZE_SCOPE: case OpCode.OP_FOR_PREPARE:
                    CopyU16();
                    break;

                // ── 1 byte operando (conteggio / tipoCode) — niente rebase ──
                case OpCode.OP_LOAD_UPVALUE: case OpCode.OP_STORE_UPVALUE: case OpCode.OP_CALL_INDIRECT:
                case OpCode.OP_IS_TYPE: case OpCode.OP_FE_DESTRUCT: case OpCode.OP_LOAD_ELEMENT:
                case OpCode.OP_LOAD_ELEMENT_REF:
                case OpCode.OP_LOAD_ELEMENT_POSITIONAL: case OpCode.OP_STORE_ELEMENT: case OpCode.OP_FETCH:
                case OpCode.OP_NEW_CHANNEL:
                    CopyU8();
                    break;

                case OpCode.OP_DEFER:
                    CopyU16(); break; // indirizzo body DEFER (interno, no rebase)
                case OpCode.OP_ONCE_BEGIN:
                    CopyU16(); break; // indirizzo skip (interno, no rebase)

                // ── 2 byte operando: indice costante → rebase ──
                case OpCode.OP_PUSH_CONST: case OpCode.OP_LOAD_FIELD: case OpCode.OP_STORE_FIELD:
                case OpCode.OP_NEW_CLASS_INSTANCE: case OpCode.OP_NEW_UDT_INSTANCE: case OpCode.OP_ATTACH_UDT_DESC: case OpCode.OP_CALL_MEMBER:
                case OpCode.OP_LOAD_DYNAMIC_SCOPE:
                    PatchU16(Kind.Const);
                    break;

                // costante-indirizzo (valore double = indirizzo funzione): rebase indice E valore
                case OpCode.OP_PUSH_CODE_REF:
                    if (o + 1 < code.Count) addrConsts.Add((code[o] << 8) | code[o + 1]);
                    PatchU16(Kind.Const);
                    break;

                // ── 2 byte operando: slot globale → rebase ──
                case OpCode.OP_DEFINE_GLOBAL: case OpCode.OP_LOAD_GLOBAL: case OpCode.OP_STORE_GLOBAL:
                case OpCode.OP_PUSH_REF_GLOBAL: case OpCode.OP_MAKE_BYREF_GLOBAL: case OpCode.OP_FINALIZE_GLOBAL:
                    PatchU16(Kind.Global);
                    break;

                // ── 2 byte operando: indirizzo assoluto → rebase ──
                case OpCode.OP_CALL_CTOR: case OpCode.OP_CALL: case OpCode.OP_GOTO: case OpCode.OP_GOSUB:
                case OpCode.OP_GO: case OpCode.OP_SET_ERR_HANDLER_GOTO: case OpCode.OP_SET_ERR_HANDLER_GOSUB:
                case OpCode.OP_PUSH_TRY: case OpCode.OP_RESUME: case OpCode.OP_GO_FUNCTION:
                    PatchU16(Kind.Addr);
                    break;

                // ── indirizzo assoluto + argc (u16+u8) → rebase addr ──
                case OpCode.OP_CALL_N: case OpCode.OP_NEW_GENERATOR_CALL:
                    PatchU16(Kind.Addr);
                    CopyU8();
                    break;

                // ── closure: constIndice (u16) + numUpvals (u8) + coppie isLocal/index (u8×2×n) ──
                case OpCode.OP_CLOSURE:
                    if (o + 1 < code.Count) addrConsts.Add((code[o] << 8) | code[o + 1]);
                    PatchU16(Kind.Const);
                    {
                        var upvals = o < code.Count ? code[o] : 0;
                        CopyU8();
                        for (var i = 0; i < upvals; i++) { CopyU8(); CopyU8(); }
                    }
                    break;

                // ── saltI relativi (u16) — niente rebase ──
                case OpCode.OP_JUMP_FORWARD: case OpCode.OP_JUMP_IF_TRUE: case OpCode.OP_JUMP_IF_FALSE:
                case OpCode.OP_JUMP_BACKWARD:
                    CopyU16();
                    break;

                // ── FFI: libConst (u16) + nameConst (u16) + argc (u8) → rebase costanti ──
                case OpCode.OP_CALL_FFI:
                    PatchU16(Kind.Const);
                    PatchU16(Kind.Const);
                    CopyU8();
                    break;

                // ── FFI tipizzata: formato in Compiler/FfiCallFormat.cs. lib/name e le due u16
                // per foglia UDT sono INDICI COSTANTE → rebase; il resto è copiato. ──
                case OpCode.OP_CALL_FFI_T:
                {
                    var lay = FfiCallFormat.Describe(code, offset);
                    var constAt = new HashSet<int>(lay.ConstOffsets);
                    while (o < offset + lay.Length)
                    {
                        if (constAt.Contains(o)) PatchU16(Kind.Const);
                        else CopyU8();
                    }
                    break;
                }

                // ── builtin: funcCode (u16) + argc (u8) — niente rebase ──
                case OpCode.OP_CALL_BUILTIN:
                    CopyU16(); CopyU8();
                    break;

                // ── syscall: code (u16) + argc (u8) — niente rebase ──
                case OpCode.OP_SYSCALL:
                    CopyU16(); CopyU8();
                    break;

                // ── subcommand: sub (u8) + argc (u8) — niente rebase ──
                case OpCode.OP_FILE_CMD: case OpCode.OP_NET_CMD: case OpCode.OP_CLIP_CMD:
                case OpCode.OP_MATRIX_CMD: case OpCode.OP_VECTOR_CMD: case OpCode.OP_AUDIO_CMD:
                    CopyU8(); CopyU8();
                    break;

                // ── on jump: tipo (u8) + count (u8) + addr u16×count → rebase addrs ──
                case OpCode.OP_ON_JUMP:
                {
                    var count = o < code.Count ? code[o] : 0;
                    CopyU8(); CopyU8();
                    for (var i = 0; i < count; i++) PatchU16(Kind.Addr);
                    break;
                }

                // ── go select: channels (u16) + sends (u16) + default (u16) + case u16×channels ──
                case OpCode.OP_GO_SELECT_SETUP:
                {
                    var channels = o + 1 < code.Count ? (code[o] << 8) | code[o + 1] : 0;
                    CopyU16(); CopyU16();
                    PatchU16(Kind.Addr); // default
                    for (var i = 0; i < channels; i++) PatchU16(Kind.Addr);
                    break;
                }

                // ── redim: dims (u8) + udtDescIdx (u16 = indice costante) → rebase ──
                case OpCode.OP_REDIM: case OpCode.OP_REDIM_PRESERVE:
                    CopyU8();
                    PatchU16(Kind.Const);
                    break;

                // ── restore: dataPos (u16) → rebase ──
                case OpCode.OP_RESTORE:
                    PatchU16(Kind.DataPos);
                    break;

                // ── new: typeCode (u16 builtin) — niente rebase ──
                case OpCode.OP_NEW:
                    CopyU16();
                    break;

                // ── 2 byte grezzi (valori) — niente rebase ──
                case OpCode.OP_LOAD_MEMBER: case OpCode.OP_STORE_MEMBER: case OpCode.OP_MK_ARRAY:
                case OpCode.OP_MK_DICT: case OpCode.OP_MK_LIST: case OpCode.OP_INIT_ARRAY:
                case OpCode.OP_ADJUST_STRING: case OpCode.OP_LOAD_STATIC_FIELD: case OpCode.OP_STORE_STATIC_FIELD:
                    CopyU16();
                    break;

                // ── for loop: slot (u8) + jump u16 (relativo) — niente rebase ──
                case OpCode.OP_FOR_LOOP:
                    CopyU8(); CopyU16();
                    break;

                // ── native loop acc: 10 byte (locali + funcCode + mIdx) — niente rebase ──
                case OpCode.OP_NATIVE_LOOP_ACC:
                    CopyRaw(10);
                    break;

                // ── native builtin assign: 4 byte (locali + funcCode) — niente rebase ──
                case OpCode.OP_NATIVE_BUILTIN_ASSIGN:
                    CopyRaw(4);
                    break;

                // ── native string append loop: 7 byte (locali + termMode + fCode + constIdx) — niente rebase ──
                case OpCode.OP_NATIVE_STRING_APPEND_LOOP:
                    CopyRaw(7);
                    break;

                default:
                    // Opcode sconosciuto: copia il resto tal quale (best-effort, senza crash).
                    for (var i = o; i < code.Count; i++) result.Add(code[i]);
                    o = code.Count;
                    break;
            }
            offset = o;
        }
        return result;
    }
}
