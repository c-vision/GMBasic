// FILE: Interpreter.Compiler.Disassembler.cs

using System.Text;


namespace Compiler;

/// <summary>
/// Fornisce metodi per disassemblare un <see cref="CompiledChunk"/>
/// in una rappresentazione testuale leggibile.
/// </summary>
public static class Disassembler
{
    /// <summary>
    /// Disassembles the specified chunk of code into a human-readable string.
    /// </summary>
    /// <param name="chunk">The compiled chunk to disassemble.</param>
    /// <param name="chunkName">The name of the chunk, used for the header.</param>
    /// <returns>A string containing the disassembled code.</returns>
    public static string Disassemble(CompiledChunk chunk, string chunkName)
    {
        var sb = new StringBuilder();
        sb.AppendLine($"== {chunkName} ==");
        var offset = 0;
        while (offset < chunk.Code.Count)
        {
            try { offset = DisassembleInstruction(chunk, offset, sb); }
            catch (Exception ex) { sb.AppendLine($"ERROR: {ex.Message} at offset {offset}"); offset += 3; }
        }

        return sb.ToString();
    }

    /// <summary>
    /// Disassembles a single instruction at a given offset.
    /// </summary>
    /// <param name="chunk">The chunk containing the instruction.</param>
    /// <param name="offset">The offset of the instruction to disassemble.</param>
    /// <param name="sb">The StringBuilder to append the output to.</param>
    /// <returns>The offset of the next instruction.</returns>
    private static int DisassembleInstruction(CompiledChunk chunk, int offset, StringBuilder sb)
    {
        sb.Append($"{offset:D4} ");
        if (offset > 0 && chunk.Lines.Count > offset && chunk.Lines[offset] == chunk.Lines[offset - 1])
        {
            sb.Append("   | ");
        }
        else if (chunk.Lines.Count > offset)
        {
            sb.Append($"{chunk.Lines[offset]:D4} ");
        }

        var instruction = (OpCode)chunk.Code[offset];
        switch (instruction)
        {
            // --- Istruzioni Semplici (1 Byte) ---
            case OpCode.OP_POP:
            case OpCode.OP_PUSH_NIL:
            case OpCode.OP_PUSH_TRUE:
            case OpCode.OP_PUSH_FALSE:
            case OpCode.OP_ADD:
            case OpCode.OP_SUBTRACT:
            case OpCode.OP_MULTIPLY:
            case OpCode.OP_DIVIDE:
            case OpCode.OP_INT_DIVIDE:
            case OpCode.OP_POWER:
            case OpCode.OP_NEGATE:
            case OpCode.OP_NOT:
            case OpCode.OP_MOD:
            case OpCode.OP_CMP_EQ:
            case OpCode.OP_CMP_NE:
            case OpCode.OP_CMP_GT:
            case OpCode.OP_CMP_GTE:
            case OpCode.OP_CMP_LT:
            case OpCode.OP_CMP_LTE:
            case OpCode.OP_DUP:
            case OpCode.OP_RETURN:
            case OpCode.OP_HALT:
            case OpCode.OP_GO_EXIT:
            case OpCode.OP_SEND:
            case OpCode.OP_RECV:
            case OpCode.OP_RECV_OK:
            case OpCode.OP_CHANNEL_CLOSE:
            case OpCode.OP_WG_WAIT:
            case OpCode.OP_WG_DONE:
            case OpCode.OP_SLEEP:
            case OpCode.OP_ON_KEY:
            case OpCode.OP_NEW_MUTEX:
            case OpCode.OP_NEW_RWMUTEX:
            case OpCode.OP_NEW_ONCE:
            case OpCode.OP_NEW_ATOMIC:
            case OpCode.OP_NEW_POOL:
            case OpCode.OP_NEW_COND:
            case OpCode.OP_NEW_CONCURRENTMAP:
            case OpCode.OP_COND_WAIT:
            case OpCode.OP_COND_SIGNAL:
            case OpCode.OP_COND_BROADCAST:
            case OpCode.OP_LOCK:
            case OpCode.OP_RLOCK:
            case OpCode.OP_UNLOCK:
            case OpCode.OP_RUNLOCK:
            case OpCode.OP_ONCE_END:
            case OpCode.OP_DEFER_RET:
            case OpCode.OP_POP_TRY:
            case OpCode.OP_READ:
            case OpCode.OP_THROW:
            case OpCode.OP_NEXT_GENERATOR:
            case OpCode.OP_RESOLVE_FUTURE:
            case OpCode.OP_EVAL_MACRO_STMT:
            case OpCode.OP_EVAL_MACRO_EXPR:
            case OpCode.OP_PRINT_VALUE:
            case OpCode.OP_PRINT_TAB_ZONE:
            case OpCode.OP_PRINT_NEWLINE:
            case OpCode.OP_PRINT_LINE:
            case OpCode.OP_YIELD:
            // OP_BREAKPOINT: definito in OpCode.cs e consumato dal PCodeProcessor, emesso solo
            // dal DebugServer (iniezione nel bytecode). 1 byte, nessun operando.
            case OpCode.OP_BREAKPOINT:
            // Le seguenti 7 mancavano dal case esplicito e ricadevano sul default (branch
            // "Unknown opcode"/l'ora rimosso try-catch che le mascherava come "ERROR: ...") —
            // numericamente innocuo per queste (0 byte operando, stesso avanzamento di
            // SimpleInstruction) ma fuorviante in output, verificato non avere operandi in
            // PCodeProcessor.cs (nessun ReadU8/ReadU16 nel case corrispondente). Porting dal
            // fix equivalente in GMBasicAI (BUG 21).
            case OpCode.OP_CLEAR_RETURN:
            case OpCode.OP_CLONE_UDT:
            case OpCode.OP_NEW_GENERATOR_EXPR:
            case OpCode.OP_NEW_WAITGROUP:
                return SimpleInstruction(instruction.ToString(), offset, sb);

            // OP_NEW_CHANNEL: 1 byte operando = capacity u8 (parità Go make(chan T, n)).
            case OpCode.OP_NEW_CHANNEL:
                return ByteInstruction("OP_NEW_CHANNEL", chunk, offset, sb);

            // --- Istruzioni con Operando a 2 Byte (slot locale u16, formato §20.3) ---
            case OpCode.OP_LOAD_LOCAL:
            case OpCode.OP_STORE_LOCAL:
            case OpCode.OP_PUSH_REF_LOCAL:
                return ShortOperandInstruction(instruction.ToString(), chunk, offset, sb, "slot");

            case OpCode.OP_MAKE_BYREF:
                return ShortOperandInstruction(instruction.ToString(), chunk, offset, sb, "slot");

            case OpCode.OP_FINALIZE_SCOPE:
                return ShortOperandInstruction("OP_FINALIZE_SCOPE", chunk, offset, sb, "slot");

            case OpCode.OP_FOR_PREPARE:
                return ShortOperandInstruction("OP_FOR_PREPARE", chunk, offset, sb, "slot");

            // --- Istruzioni con Operando a 1 Byte ---
            case OpCode.OP_LOAD_UPVALUE:
            case OpCode.OP_STORE_UPVALUE:
            case OpCode.OP_CALL_INDIRECT:
            // OP_IS_TYPE/OP_MAKE_BYREF mancavano dal case esplicito (desync REALE di 1 byte ad
            // ogni occorrenza — verificato: EmitByte(typeCode)/EmitByte(localIdx) lato
            // compilatore, ReadU8(task) lato PCodeProcessor.cs). Porting da GMBasicAI (BUG 21) —
            // questo e' probabilmente il vero motivo del crash DASM su FunctionsParams.bas che
            // era stato "risolto" con un try-catch che salta 3 byte a caso invece di correggere
            // la causa reale.
            case OpCode.OP_IS_TYPE:
            case OpCode.OP_FE_DESTRUCT:
                return ByteInstruction(instruction.ToString(), chunk, offset, sb);

            case OpCode.OP_FINALIZE_GLOBAL:
                return ShortOperandInstruction("OP_FINALIZE_GLOBAL", chunk, offset, sb, "Gbl");
            
            case OpCode.OP_END_VALID:
            // OP_ON_MOUSE mancava dal case esplicito (1 byte, nessun operando, come OP_ON_KEY
            // e OP_END_VALID): ogni evento ON MOUSE nel disassemblaggio causava un desync di
            // 1 byte (letture U16 su operandi non allineati -> "Unknown opcode"/"Invalid
            // constant index" a cascata). Verificato: EmitByte((byte)OP_ON_MOUSE) lato
            // compilatore, nessun ReadU8/ReadU16 nel case del PCodeProcessor.
            case OpCode.OP_ON_MOUSE:
                return SimpleInstruction(instruction.ToString(), offset, sb);

            // --- Istruzioni con Operando a 2 Byte (Short) ---
            case OpCode.OP_PUSH_CONST:
            case OpCode.OP_LOAD_FIELD:
            case OpCode.OP_STORE_FIELD:
            case OpCode.OP_PUSH_CODE_REF:
            case OpCode.OP_NEW_CLASS_INSTANCE:
            case OpCode.OP_NEW_UDT_INSTANCE:
            case OpCode.OP_CALL_MEMBER:
            case OpCode.OP_LOAD_DYNAMIC_SCOPE:
                return ConstantInstruction(instruction.ToString(), chunk, offset, sb);

            case OpCode.OP_DEFINE_GLOBAL:
            case OpCode.OP_LOAD_GLOBAL:
            case OpCode.OP_STORE_GLOBAL:
            case OpCode.OP_PUSH_REF_GLOBAL:
            // OP_MAKE_BYREF_GLOBAL mancava dal case esplicito (desync REALE di 2 byte, stesso
            // formato EmitShort(globalIdx)/ReadU16 delle altre istruzioni su slot globali sopra).
            // Porting da GMBasicAI (BUG 21).
            case OpCode.OP_MAKE_BYREF_GLOBAL:
                return GlobalInstruction(instruction.ToString(), chunk, offset, sb);

            case OpCode.OP_LOAD_MEMBER:
            case OpCode.OP_STORE_MEMBER:
                return ShortOperandInstruction(instruction.ToString(), chunk, offset, sb, "offset");

            // OP_RESTORE ha ora un operando U16 (posizione DATA) — prima era 1 byte senza
            // operando; il DASM andava in desync di 2 byte a ogni RESTORE.
            case OpCode.OP_RESTORE:
                return ShortOperandInstruction(instruction.ToString(), chunk, offset, sb, "dataPos");
            
            case OpCode.OP_MK_ARRAY:
                return ShortOperandInstruction(instruction.ToString(), chunk, offset, sb, "elements");
            case OpCode.OP_MK_DICT:
                return ShortOperandInstruction(instruction.ToString(), chunk, offset, sb, "entries");
            case OpCode.OP_MK_LIST:
                return ShortOperandInstruction(instruction.ToString(), chunk, offset, sb, "elements");
            case OpCode.OP_INIT_ARRAY:
                return ShortOperandInstruction(instruction.ToString(), chunk, offset, sb, "values");
            // OP_ADJUST_STRING/OP_LOAD_STATIC_FIELD/OP_STORE_STATIC_FIELD mancavano dal case
            // esplicito (desync REALE di 2 byte ad ogni occorrenza: EmitShort lato compilatore,
            // ReadU16 lato PCodeProcessor.cs). L'operando e' un valore grezzo (lunghezza stringa
            // / id simbolo), non un indice nella constant pool, quindi ShortOperandInstruction
            // (non ConstantInstruction) e' la resa corretta. Porting da GMBasicAI (BUG 21).
            case OpCode.OP_ADJUST_STRING:
                return ShortOperandInstruction(instruction.ToString(), chunk, offset, sb, "length");
            case OpCode.OP_LOAD_STATIC_FIELD:
            case OpCode.OP_STORE_STATIC_FIELD:
                return ShortOperandInstruction(instruction.ToString(), chunk, offset, sb, "symbolId");

            case OpCode.OP_JUMP_FORWARD:
            case OpCode.OP_JUMP_IF_TRUE:
            case OpCode.OP_JUMP_IF_FALSE:
                return JumpInstruction(instruction.ToString(), 1, chunk, offset, sb);
            case OpCode.OP_ONCE_BEGIN:
                // [OP_ONCE_BEGIN][skip u16]: jump in avanti allo skip se il Once è già Done.
                return JumpInstruction(instruction.ToString(), 1, chunk, offset, sb);
            case OpCode.OP_JUMP_BACKWARD:
                return JumpInstruction(instruction.ToString(), -1, chunk, offset, sb);

            case OpCode.OP_CALL_CTOR:
            case OpCode.OP_CALL:
            case OpCode.OP_GOTO:
            case OpCode.OP_GOSUB:
            case OpCode.OP_GO:
            case OpCode.OP_SET_ERR_HANDLER_GOTO:
            case OpCode.OP_SET_ERR_HANDLER_GOSUB:
            case OpCode.OP_PUSH_TRY:
            case OpCode.OP_RESUME:
            // OP_GO_FUNCTION mancava dal case esplicito (desync REALE di 2 byte — EmitJump lato
            // compilatore emette solo l'indirizzo, nessun byte extra, stesso formato di OP_GO).
            // Porting da GMBasicAI (BUG 21).
            case OpCode.OP_GO_FUNCTION:
                return AddressInstruction(instruction.ToString(), chunk, offset, sb);

            case OpCode.OP_CLOSURE:
                return ClosureInstruction(instruction.ToString(), chunk, offset, sb);
            case OpCode.OP_FOR_LOOP:
                return ForLoopInstruction("OP_FOR_LOOP", chunk, offset, sb);
            case OpCode.OP_NATIVE_LOOP_ACC:
                return NativeLoopAccInstruction(chunk, offset, sb);
            case OpCode.OP_NATIVE_BUILTIN_ASSIGN:
                return NativeBuiltinAssignInstruction(chunk, offset, sb);
            case OpCode.OP_NATIVE_STRING_APPEND_LOOP:
                return NativeStringAppendLoopInstruction(chunk, offset, sb);
            case OpCode.OP_ON_JUMP:
                return OnJumpInstruction(instruction.ToString(), chunk, offset, sb);

            // --- Istruzioni Complesse ---
            case OpCode.OP_CALL_BUILTIN:
                return BuiltInCallInstruction(instruction.ToString(), chunk, offset, sb);
            // OP_LOAD_ELEMENT ha un operando a 1 byte (num_indices) che qui mancava —
            // veniva trattato come SimpleInstruction (0 byte), disallineando ogni
            // istruzione successiva nel chunk di un byte per ogni OP_LOAD_ELEMENT
            // incontrato (stesso tipo di bug già trovato per OP_CALL_INDIRECT).
            case OpCode.OP_LOAD_ELEMENT:
                return ByteInstruction("OP_LOAD_ELEMENT", chunk, offset, sb);
            case OpCode.OP_LOAD_ELEMENT_REF:
                return ByteInstruction("OP_LOAD_ELEMENT_REF", chunk, offset, sb);
            case OpCode.OP_LOAD_ELEMENT_POSITIONAL:
                return ByteInstruction("OP_LOAD_ELEMENT_POSITIONAL", chunk, offset, sb);
            case OpCode.OP_ARRAY_LEN:
                return SimpleInstruction("ARRAY_LEN", offset, sb);
            case OpCode.OP_MAKE_BYREF_ELEMENT:
                return SimpleInstruction("OP_MAKE_BYREF_ELEMENT", offset, sb);
            case OpCode.OP_STORE_ELEMENT:
                return ElementInstruction(instruction.ToString(), chunk, offset, sb);
            case OpCode.OP_REDIM:
            case OpCode.OP_REDIM_PRESERVE:
                return RedimInstruction(instruction.ToString(), chunk, offset, sb);
            case OpCode.OP_GO_SELECT_SETUP:
                return GoSelectSetupInstruction(instruction.ToString(), chunk, offset, sb);

            // --- OPCODES CON SOTTOCOMANDI ---
            case OpCode.OP_SYSCALL:
                return SysCallInstruction(instruction.ToString(), chunk, offset, sb);
            case OpCode.OP_NEW:
                return NewInstruction(instruction.ToString(), chunk, offset, sb);
            case OpCode.OP_FILE_CMD:
                return SubCommandWithArgCountInstruction<FileSubOpCode>(instruction.ToString(), chunk, offset, sb);
            case OpCode.OP_NET_CMD:
                return SubCommandWithArgCountInstruction<NetSubOpCode>(instruction.ToString(), chunk, offset, sb);
            case OpCode.OP_CLIP_CMD:
                return SubCommandWithArgCountInstruction<ClipSubOpCode>(instruction.ToString(), chunk, offset, sb);
            case OpCode.OP_MATRIX_CMD:
                return SubCommandWithArgCountInstruction<MatrixSubOpCode>(instruction.ToString(), chunk, offset, sb);
            case OpCode.OP_VECTOR_CMD:
                return SubCommandWithArgCountInstruction<VectorSubOpCode>(instruction.ToString(), chunk, offset, sb);
            // OP_AUDIO_CMD mancava dal case esplicito — stesso formato
            // [opcode][subcode][argCount] di OP_FILE_CMD/OP_NET_CMD/ecc sopra (verificato in
            // Compiler.Audio.cs: EmitBytes(op, subOpCode, argCount)).
            // Opcode specifici di GMBasic, non presenti in GMBasicAI.
            case OpCode.OP_AUDIO_CMD:
                return SubCommandWithArgCountInstruction<AudioSubOpCode>(instruction.ToString(), chunk, offset, sb);
            case OpCode.OP_FETCH:
                return FetchInstruction(instruction.ToString(), chunk, offset, sb);
            case OpCode.OP_CALL_FFI:
                return FfiCallInstruction(instruction.ToString(), chunk, offset, sb);
            case OpCode.OP_CALL_FFI_T:
                return FfiCallInstruction(instruction.ToString(), chunk, offset, sb, typed: true);
            // OP_CALL_N/OP_NEW_GENERATOR_CALL mancavano dal case esplicito (desync REALE di 3
            // byte ad ogni occorrenza — praticamente ogni chiamata a funzione/sub del programma:
            // EmitJump(indirizzo 2 byte) + EmitByte(argCount) lato compilatore, ReadU16+ReadU8
            // lato PCodeProcessor.cs, stesso formato di OP_CALL_BUILTIN ma con un indirizzo di
            // codice al posto del codice funzione built-in). Porting da GMBasicAI (BUG 21) — con
            // ogni probabilita' la causa REALE del crash DASM su FunctionsParams.bas.
            case OpCode.OP_CALL_N:
            case OpCode.OP_NEW_GENERATOR_CALL:
                return AddressWithArgCountInstruction(instruction.ToString(), chunk, offset, sb);
            default:
                sb.AppendLine($"Unknown opcode {instruction}");
                return offset + 1;
        }
    }

    /// <summary>
    /// Disassembles a simple instruction with no operands.
    /// </summary>
    private static int SimpleInstruction(string name, int offset, StringBuilder sb)
    {
        sb.AppendLine(name);
        return offset + 1;
    }

    /// <summary>
    /// Disassembles an instruction with a single byte operand (e.g., a local variable slot index).
    /// </summary>
    private static int ByteInstruction(string name, CompiledChunk chunk, int offset, StringBuilder sb)
    {
        if (offset + 1 >= chunk.Code.Count) return PrintTruncated(offset, sb, name);
        var slot = chunk.Code[offset + 1];
        sb.AppendLine($"{name,-20} Slot={slot}");
        return offset + 2;
    }

    /// <summary>
    /// Disassembles an instruction with a single short (2-byte) operand.
    /// </summary>
    private static int ShortOperandInstruction(string name, CompiledChunk chunk, int offset, StringBuilder sb,
        string operandName)
    {
        if (offset + 2 >= chunk.Code.Count) return PrintTruncated(offset, sb, name);
        var value = (chunk.Code[offset + 1] << 8) | chunk.Code[offset + 2];
        sb.AppendLine($"{name,-20} ({value} {operandName})");
        return offset + 3;
    }

    /// <summary>
    /// Disassembles an instruction whose operand is an index into the constant pool.
    /// </summary>
    private static int ConstantInstruction(string name, CompiledChunk chunk, int offset, StringBuilder sb)
    {
        if (offset + 2 >= chunk.Code.Count) return PrintTruncated(offset, sb, name);
        var constIndex = (chunk.Code[offset + 1] << 8) | chunk.Code[offset + 2];
        if (constIndex >= chunk.Constants.Count)
            return PrintInvalid(offset, $"Invalid constant index {constIndex}", sb);

        var constantValue = chunk.Constants[constIndex];
        sb.AppendLine($"{name,-20} Const[{constIndex:D4}] '{constantValue}'");
        return offset + 3;
    }

    /// <summary>
    /// Disassembles a jump instruction, calculating the relative target address.
    /// </summary>
    private static int JumpInstruction(string name, int sign, CompiledChunk chunk, int offset, StringBuilder sb)
    {
        if (offset + 2 >= chunk.Code.Count) return PrintTruncated(offset, sb, name);
        var jump = (chunk.Code[offset + 1] << 8) | chunk.Code[offset + 2];
        var targetAddress = offset + 3 + (sign * jump);
        sb.AppendLine($"{name,-20} -> {targetAddress:D4}");
        return offset + 3;
    }

    /// <summary>
    /// Disassembles an instruction whose operand is an absolute code address (e.g., for CALL, GOSUB).
    /// </summary>
    private static int AddressInstruction(string name, CompiledChunk chunk, int offset, StringBuilder sb)
    {
        if (offset + 2 >= chunk.Code.Count) return PrintTruncated(offset, sb, name);
        var address = (chunk.Code[offset + 1] << 8) | chunk.Code[offset + 2];
        sb.AppendLine($"{name,-20} @{address:D4}");
        return offset + 3;
    }

    /// <summary>
    /// Disassembles an instruction whose operand is an index into the globals table.
    /// </summary>
    private static int GlobalInstruction(string name, CompiledChunk chunk, int offset, StringBuilder sb)
    {
        if (offset + 2 >= chunk.Code.Count) return PrintTruncated(offset, sb, name);
        var globalIndex = (chunk.Code[offset + 1] << 8) | chunk.Code[offset + 2];
        sb.AppendLine($"{name,-20} Gbl[{globalIndex:D4}]");
        return offset + 3;
    }

    /// <summary>
    /// Disassembles a call to a built-in function.
    /// <para>Format: [OP_CALL_BUILTIN] [function_code (ushort)] [arg_count (byte)]</para>
    /// </summary>
    private static int BuiltInCallInstruction(string name, CompiledChunk chunk, int offset, StringBuilder sb)
    {
        if (offset + 3 >= chunk.Code.Count) return PrintTruncated(offset, sb, name);
        var functionCodeValue = (chunk.Code[offset + 1] << 8) | chunk.Code[offset + 2];
        var functionCode = (BuiltInFunction)functionCodeValue;
        var argCount = chunk.Code[offset + 3];
        sb.AppendLine($"{name,-20} {functionCode} ({argCount} args)");
        return offset + 4;
    }

    /// <summary>
    /// Disassembles a call to a compiled function/sub at a fixed code address (OP_CALL_N,
    /// OP_NEW_GENERATOR_CALL). Stesso formato di BuiltInCallInstruction ma con un indirizzo di
    /// codice al posto del codice funzione built-in. Porting da GMBasicAI (BUG 21).
    /// <para>Format: [OP_CODE] [address (ushort)] [arg_count (byte)]</para>
    /// </summary>
    private static int AddressWithArgCountInstruction(string name, CompiledChunk chunk, int offset, StringBuilder sb)
    {
        if (offset + 3 >= chunk.Code.Count) return PrintTruncated(offset, sb, name);
        var address = (chunk.Code[offset + 1] << 8) | chunk.Code[offset + 2];
        var argCount = chunk.Code[offset + 3];
        sb.AppendLine($"{name,-20} @{address:D4} ({argCount} args)");
        return offset + 4;
    }

    /// <summary>
    /// Disassembles an element access instruction (LOAD_ELEMENT, STORE_ELEMENT).
    /// <para>Format: [OP_CODE] [num_indices (byte)]</para>
    /// </summary>
    private static int ElementInstruction(string name, CompiledChunk chunk, int offset, StringBuilder sb)
    {
        if (offset + 1 >= chunk.Code.Count) return PrintTruncated(offset, sb, name);
        var dimCount = chunk.Code[offset + 1];
        sb.AppendLine($"{name,-20} ({dimCount} dims)");
        return offset + 2;
    }

    /// <summary>
    /// Disassembles an OP_NEW instruction.
    /// <para>Format: [OP_NEW] [TypeCode (ushort)]</para>
    /// </summary>
    private static int NewInstruction(string name, CompiledChunk chunk, int offset, StringBuilder sb)
    {
        if (offset + 2 >= chunk.Code.Count) return PrintTruncated(offset, sb, name);
        var typeCode = (chunk.Code[offset + 1] << 8) | chunk.Code[offset + 2];
        sb.AppendLine($"{name,-20} TypeCode={typeCode:D4}");
        return offset + 3;
    }

    /// <summary>
    /// Disassembles a generic syscall instruction.
    /// <para>Format: [OP_SYSCALL] [syscall_code (ushort, big-endian)] [arg_count (byte)]</para>
    /// </summary>
    /// <remarks>
    /// BUG STORICO: leggeva syscall_code come UN SOLO byte (chunk.Code[offset+1]), ma
    /// Compiler.Emitters.cs/EmitSysCall lo emette come USHORT (2 byte) da quando SysCall è
    /// stato esteso oltre 255 membri (vedi commit "SysCall enum esteso a ushort") — questo
    /// leggeva 1 byte in meno per OGNI singola istruzione OP_SYSCALL, disallineando in modo
    /// CUMULATIVO ogni istruzione successiva nel chunk di un byte per ogni OP_SYSCALL
    /// incontrato (stesso tipo di bug già trovato e corretto per OP_LOAD_ELEMENT/
    /// OP_CALL_INDIRECT altrove in questo file). Per file con pochi OP_SYSCALL il
    /// disallineamento accumulato spesso non basta a produrre un opcode non valido prima
    /// della fine del chunk (output disassemblato comunque sbagliato, ma senza crash
    /// visibile). Per file con MOLTI OP_SYSCALL (es. Clipper/ClipperDbfLateBinding.bas, uso
    /// intensivo di DBF.*) il disallineamento cumulato arriva a decodificare byte casuali
    /// come un OP_CLOSURE con un constIndex fuori range, causando un crash
    /// (ArgumentOutOfRangeException in ClosureInstruction) — sintomo scambiato per un bug di
    /// late binding DBF nel linker (mai esistito: il file compila correttamente, solo la sua
    /// disassemblazione andava in crash).
    /// </remarks>
    private static int SysCallInstruction(string name, CompiledChunk chunk, int offset, StringBuilder sb)
    {
        if (offset + 3 >= chunk.Code.Count) return PrintTruncated(offset, sb, name);
        var syscallCode = (SysCall)((chunk.Code[offset + 1] << 8) | chunk.Code[offset + 2]);
        var argCount = chunk.Code[offset + 3];
        sb.AppendLine($"{name,-20} {syscallCode} ({argCount} args)");
        return offset + 4;
    }

    /// <summary>
    /// Disassembles a prefix opcode followed by a sub-command byte and an argument count byte.
    /// <para>Format: [OP_CODE] [sub_opcode (byte)] [arg_count (byte)]</para>
    /// </summary>
    private static int SubCommandWithArgCountInstruction<T>(string name, CompiledChunk chunk, int offset,
        StringBuilder sb) where T : Enum
    {
        if (offset + 2 >= chunk.Code.Count) return PrintTruncated(offset, sb, name);
        var subOpCodeByte = chunk.Code[offset + 1];
        var subOpCode = (T)Enum.ToObject(typeof(T), subOpCodeByte);
        var argCount = chunk.Code[offset + 2];
        sb.AppendLine($"{name,-20} {subOpCode} ({argCount} args)");
        return offset + 3;
    }

    /// <summary>
    /// Disassembles an OP_FOR_LOOP instruction.
    /// <para>Formato: [OP_FOR_LOOP] [local_slot (byte)] [jump_offset (ushort)]</para>
    /// </summary>
    private static int ForLoopInstruction(string name, CompiledChunk chunk, int offset, StringBuilder sb)
    {
        if (offset + 4 >= chunk.Code.Count) return PrintTruncated(offset, sb, name);
        var slot = (chunk.Code[offset + 1] << 8) | chunk.Code[offset + 2];
        var jump = (chunk.Code[offset + 3] << 8) | chunk.Code[offset + 4];
        var targetAddress = offset + 5 - jump;
        sb.AppendLine($"{name,-20} Slot={slot} -> {targetAddress:D4}");
        return offset + 5;
    }

    /// <summary>Disassembles an OP_NATIVE_LOOP_ACC instruction (9 bytes).</summary>
    private static int NativeLoopAccInstruction(CompiledChunk chunk, int offset, StringBuilder sb)
    {
        if (offset + 12 >= chunk.Code.Count) return PrintTruncated(offset, sb, "OP_NATIVE_LOOP_ACC");
        var accSlot = (chunk.Code[offset + 1] << 8) | chunk.Code[offset + 2];
        var ctrSlot = (chunk.Code[offset + 3] << 8) | chunk.Code[offset + 4];
        var accOp = chunk.Code[offset + 5];
        var termMode = chunk.Code[offset + 6];
        var fCode = (chunk.Code[offset + 7] << 8) | chunk.Code[offset + 8];
        var fCode2 = (chunk.Code[offset + 9] << 8) | chunk.Code[offset + 10];
        var mIdx = (chunk.Code[offset + 11] << 8) | chunk.Code[offset + 12];
        var accOpS = accOp switch { 0 => "+=", 1 => "-=", 2 => "*=", 3 => "/=", _ => "?" };
        var termS = termMode switch { 0 => "f(I)*m", 1 => "f(I)", 2 => "I*I", 3 => "I*m", 4 => "I", 5 => "f(I)+g(I)", _ => "?" };
        sb.AppendLine($"OP_NATIVE_LOOP_ACC acc={accSlot} ctr={ctrSlot} op={accOpS} term={termS} f={fCode} f2={fCode2} m={mIdx}");
        return offset + 13;
    }

    /// <summary>Disassembles an OP_NATIVE_BUILTIN_ASSIGN instruction (5 bytes).</summary>
    private static int NativeBuiltinAssignInstruction(CompiledChunk chunk, int offset, StringBuilder sb)
    {
        if (offset + 6 >= chunk.Code.Count) return PrintTruncated(offset, sb, "OP_NATIVE_BUILTIN_ASSIGN");
        var srcSlot = (chunk.Code[offset + 1] << 8) | chunk.Code[offset + 2];
        var dstSlot = (chunk.Code[offset + 3] << 8) | chunk.Code[offset + 4];
        var fCode = (chunk.Code[offset + 5] << 8) | chunk.Code[offset + 6];
        sb.AppendLine($"OP_NATIVE_BUILTIN_ASSIGN src={srcSlot} dst={dstSlot} f={fCode}");
        return offset + 7;
    }

    /// <summary>Disassembles an OP_NATIVE_STRING_APPEND_LOOP instruction (8 bytes).</summary>
    private static int NativeStringAppendLoopInstruction(CompiledChunk chunk, int offset, StringBuilder sb)
    {
        if (offset + 7 >= chunk.Code.Count) return PrintTruncated(offset, sb, "OP_NATIVE_STRING_APPEND_LOOP");
        var accSlot = chunk.Code[offset + 1];
        var ctrSlot = chunk.Code[offset + 2];
        var termMode = chunk.Code[offset + 3];
        var fCode = (chunk.Code[offset + 4] << 8) | chunk.Code[offset + 5];
        var constIdx = (chunk.Code[offset + 6] << 8) | chunk.Code[offset + 7];
        var termS = termMode switch { 0 => $"lit[{constIdx}]", 1 => $"f={fCode}(I)", _ => "?" };
        sb.AppendLine($"OP_NATIVE_STRING_APPEND_LOOP acc={accSlot} ctr={ctrSlot} term={termS}");
        return offset + 8;
    }
    
    /// <summary>
    /// Disassembles an OP_REDIM or OP_REDIM_PRESERVE instruction.
    /// <para>Formato (compilatore → VM): [OP_CODE] [dimCount (byte)] [udtDescIdx (ushort)]</para>
    /// <para>Il vecchio formato [isLocal][varIndex(ushort)][dimCount] qui documentato NON
    /// corrispondeva a ciò che Compiler.Arrays.cs emette (EmitByte(dimCount) + EmitShort(udtDescIdx))
    /// né a ciò che PCodeProcessor.cs legge (ReadU8 + ReadU16): avanzava di 5 byte invece di 4,
    /// disallineando CUMULATIVAMENTE ogni istruzione successiva per ogni OP_REDIM/OP_REDIM_PRESERVE
    /// nel chunk (sintomo visibile: "Invalid constant index 65535/512/..." a cascata — es.
    /// Arrays/ArraysRedim.bas). Corretto il 2026-08-12.</para>
    /// </summary>
    private static int RedimInstruction(string name, CompiledChunk chunk, int offset, StringBuilder sb)
    {
        if (offset + 3 >= chunk.Code.Count) return PrintTruncated(offset, sb, name);
        var dimCount = chunk.Code[offset + 1];
        var udtDescIdx = (chunk.Code[offset + 2] << 8) | chunk.Code[offset + 3];
        sb.AppendLine($"{name,-20} ({dimCount} dims) UdtDesc={udtDescIdx}");
        return offset + 4;
    }

    /// <summary>
    /// Disassembles an OP_FETCH instruction.
    /// <para>Format: [OP_FETCH] [Variant (byte)]</para>
    /// </summary>
    private static int FetchInstruction(string name, CompiledChunk chunk, int offset, StringBuilder sb)
    {
        if (offset + 1 >= chunk.Code.Count) return PrintTruncated(offset, sb, name);
        var variant = chunk.Code[offset + 1];
        sb.AppendLine($"{name,-20} Variant={variant}");
        return offset + 2;
    }

    /// <summary>
    /// Disassembles an OP_CALL_FFI instruction.
    /// <para>Formato: [OP_CALL_FFI] [lib_const (ushort)] [name_const (ushort)] [arg_count (byte)]</para>
    /// <para>Il vecchio formato leggeva solo [const_index(ushort)]+[arg_count(byte)] (4 byte),
    /// ma Compiler.Expressions.cs emette due costanti (EmitShort(libIndex)+EmitShort(nameIndex))
    /// e PCodeProcessor.cs ne legge due (ReadU16+ReadU16+ReadU8): mancavano 2 byte per ogni
    /// OP_CALL_FFI, disallineando CUMULATIVAMENTE il resto del chunk (es. FFI/CallGoMath.bas,
    /// "Invalid constant index 369 a valle"). Corretto il 2026-08-12.</para>
    /// <para>OP_CALL_FFI_T (typed) aggiunge, dopo [kind (byte)], un ottavo byte
    /// [array_mask (byte)] (2026-09-28, array BYREF): bit i settato se il parametro logico i è
    /// un array (puntatore) — vedi Compiler.Expressions.cs e PCodeProcessor.cs OP_CALL_FFI_T.
    /// Un nono byte [array_elem_int_mask (byte)] (2026-09-28, fix parità): bit i settato se
    /// l'array al parametro i ha elementi interi (AS LONG/...) invece di DOUBLE — necessario
    /// perché PCodeVM/VM-C non preservano il tipo elemento a runtime (vedi Compiler.Expressions.cs
    /// per il dettaglio del bug che questo campo risolve). Un decimo byte [string_mask (byte)] e
    /// un undicesimo [ret_is_string (byte)] (2026-09-28, supporto STRING): bit i settato in
    /// string_mask = parametro i è STRING (puntatore); ret_is_string = il ritorno è STRING (il
    /// valore int64 va copiato in una nuova stringa BASIC, non trattato come numero).</para>
    /// <para>Un byte [scalar_kind_mask (byte)] (2026-10, firma mista LONG/DOUBLE fra scalari
    /// semplici) SUBITO DOPO [kind], prima di [array_mask]: bit i = il parametro scalare
    /// semplice alla posizione i è Int64 (0=Double) — significativo solo per le posizioni non-
    /// array/STRING/BYREF/callback/UDT. Sposta tutti gli offset successivi di +1 rispetto alle
    /// note storiche sotto (che restano accurate sull'ORDINE dei campi, non sull'offset
    /// assoluto — vedi gli offset calcolati nel codice).</para>
    /// <para>Un dodicesimo byte [byref_scalar_mask (byte)] e un tredicesimo
    /// [byref_scalar_elem_int_mask (byte)] (2026-09-28, BYREF scalare): bit i settato in
    /// byref_scalar_mask = il parametro i è BYREF scalare (puntatore a un singolo valore,
    /// marshalling copy-in/copy-out); byref_scalar_elem_int_mask = quel valore è intero
    /// (AS LONG/QUAD/...) invece di DOUBLE, stesso ruolo di array_elem_int_mask.</para>
    /// <para>Otto byte [udt_param_field_count[8] (byte ciascuno)] e un ultimo byte
    /// [ret_udt_field_count (byte)] (2026-09-28, struct/UDT per valore): a differenza di
    /// array/STRING/BYREF (un bit, sempre puntatore) un parametro UDT si "espande" in N slot
    /// nativi (uno per campo, per valore) — un byte PER POSIZIONE parametro (0-7) col numero di
    /// campi (0 = non è un UDT). Il kind dei campi è SEMPRE 'kind' (la firma omogenea lo
    /// garantisce). ret_udt_field_count: stesso significato per il ritorno.</para>
    /// <para>Otto byte [callback_param_arity[8] (byte ciascuno)] e due byte
    /// [callback_param_kind_mask]/[callback_param_has_ret_mask] (2026-09-29, callback come
    /// parametro FFI): callback_param_arity[8] è 255 se quella posizione parametro NON è un
    /// callback, altrimenti l'arità 0-8 del callback. callback_param_kind_mask: bit i
    /// settato se il callback alla posizione i ha firma interna INT64. callback_param_has_ret_
    /// mask: bit i settato se il callback alla posizione i ha un ritorno (AS FUNCTION, non AS
    /// SUB). Scaffolding bytecode — il runtime che genera/seleziona i trampolini nativi non è
    /// ancora implementato (vedi PCodeProcessor.cs/gmb_vm_core.c OP_CALL_FFI_T).</para>
    /// </summary>
    private static int FfiCallInstruction(string name, CompiledChunk chunk, int offset, StringBuilder sb, bool typed = false)
    {
        var instrLen = typed ? 42 : 6;   // T: + kind + scalar_kind_mask + array_mask + array_elem_int_mask + string_mask + ret_is_string + byref_scalar_mask + byref_scalar_elem_int_mask + udt_param_field_count[8] + udt_param_field_kind_mask[8] + ret_udt_field_count + ret_udt_field_kind_mask + callback_param_arity[8] + callback_param_kind_mask + callback_param_has_ret_mask (u8 each)
        if (offset + instrLen - 1 >= chunk.Code.Count) return PrintTruncated(offset, sb, name);
        var libIndex = (chunk.Code[offset + 1] << 8) | chunk.Code[offset + 2];
        var nameIndex = (chunk.Code[offset + 3] << 8) | chunk.Code[offset + 4];
        var argCount = chunk.Code[offset + 5];
        var kind = typed ? chunk.Code[offset + 6] : 0;
        // scalarKindMask (2026-10, firma mista scalari semplici): vedi nota sopra FfiCallInstruction.
        var scalarKindMask = typed ? chunk.Code[offset + 7] : 0;
        var arrayMask = typed ? chunk.Code[offset + 8] : 0;
        var arrayElemIntMask = typed ? chunk.Code[offset + 9] : 0;
        var stringMask = typed ? chunk.Code[offset + 10] : 0;
        var retIsString = typed ? chunk.Code[offset + 11] : 0;
        var byrefScalarMask = typed ? chunk.Code[offset + 12] : 0;
        var byrefScalarElemIntMask = typed ? chunk.Code[offset + 13] : 0;
        var udtParamFieldCounts = new int[8];
        for (var pi = 0; pi < 8; pi++)
            udtParamFieldCounts[pi] = typed ? chunk.Code[offset + 14 + pi] : 0;
        // udtParamFieldKindMask[8] (2026-09-29, kind misto come parametro): bit i = campo i del
        // parametro UDT alla posizione pi è INTERO (0=DOUBLE) — vedi Compiler.Expressions.cs.
        var udtParamFieldKindMasks = new int[8];
        for (var pi = 0; pi < 8; pi++)
            udtParamFieldKindMasks[pi] = typed ? chunk.Code[offset + 22 + pi] : 0;
        var retUdtFieldCount = typed ? chunk.Code[offset + 30] : 0;
        // retUdtFieldKindMask (2026-09-29, kind misto sul ritorno): bit i = campo i del ritorno
        // UDT è INTERO (0=DOUBLE) — vedi Compiler.Expressions.cs.
        var retUdtFieldKindMask = typed ? chunk.Code[offset + 31] : 0;
        // callbackParamArity[8]/callbackParamKindMask/callbackParamHasRetMask (2026-09-29,
        // callback come parametro FFI): un byte PER POSIZIONE parametro (255 = non è un
        // callback, altrimenti l'arità 0-8), poi un bit-mask del kind interno e uno di
        // presenza-ritorno — vedi Compiler.Expressions.cs.
        var callbackParamArity = new int[8];
        for (var pi = 0; pi < 8; pi++)
            callbackParamArity[pi] = typed ? chunk.Code[offset + 32 + pi] : 255;
        var callbackParamKindMask = typed ? chunk.Code[offset + 40] : 0;
        var callbackParamHasRetMask = typed ? chunk.Code[offset + 41] : 0;
        var libPath = libIndex >= 0 && libIndex < chunk.Constants.Count ? chunk.Constants[libIndex] : null;
        var funcName = nameIndex >= 0 && nameIndex < chunk.Constants.Count ? chunk.Constants[nameIndex] : null;
        var kindTxt = typed ? (kind == 1 ? " int64" : " double") : "";
        var fullMaskForArgc = argCount >= 8 ? 0xFF : (1 << argCount) - 1;
        var scalarKindMaskTxt = typed && scalarKindMask != (kind == 1 ? fullMaskForArgc : 0) ? $" scalarKindMask={scalarKindMask:X2}" : "";
        var maskTxt = typed && (arrayMask != 0 || stringMask != 0 || retIsString != 0 || byrefScalarMask != 0)
            ? $" arrayMask={arrayMask:X2} arrayElemIntMask={arrayElemIntMask:X2} stringMask={stringMask:X2} retIsString={retIsString} byrefScalarMask={byrefScalarMask:X2} byrefScalarElemIntMask={byrefScalarElemIntMask:X2}"
            : "";
        // Nomi campo + SymbolId UDT (2026-09-28): coda A LUNGHEZZA VARIABILE — DUE u16 (indice
        // costante del nome, poi SymbolId) PER CAMPO, nell'ordine parametri (posizione 0..7,
        // solo quelli con field count>0) poi ritorno — vedi Compiler.Expressions.cs. instrLen
        // fisso (22) NON la include: va calcolata qui, altrimenti il resto del chunk disallinea
        // (stesso bug-class della nota storica in cima a questo metodo sull'OP_CALL_FFI non typed).
        var totalUdtFields = udtParamFieldCounts.Sum() + retUdtFieldCount;
        var udtFieldNames = new List<string?>();
        if (typed && totalUdtFields > 0)
        {
            var fieldsOffset = offset + instrLen;
            for (var k = 0; k < totalUdtFields; k++)
            {
                var idxPos = fieldsOffset + k * 4;
                if (idxPos + 1 >= chunk.Code.Count) break;
                var fIdx = (chunk.Code[idxPos] << 8) | chunk.Code[idxPos + 1];
                udtFieldNames.Add(fIdx >= 0 && fIdx < chunk.Constants.Count ? chunk.Constants[fIdx] as string : null);
            }
            instrLen += totalUdtFields * 4;
        }
        var udtTxt = typed && (udtParamFieldCounts.Any(c => c != 0) || retUdtFieldCount != 0)
            ? $" udtParamFieldCounts=[{string.Join(",", udtParamFieldCounts)}] udtParamFieldKindMasks=[{string.Join(",", udtParamFieldKindMasks.Select(m => m.ToString("X2")))}] retUdtFieldCount={retUdtFieldCount} retUdtFieldKindMask={retUdtFieldKindMask:X2} udtFields=[{string.Join(",", udtFieldNames)}]"
            : "";
        var callbackTxt = typed && callbackParamArity.Any(a => a != 255)
            ? $" callbackParamArity=[{string.Join(",", callbackParamArity)}] callbackParamKindMask={callbackParamKindMask:X2} callbackParamHasRetMask={callbackParamHasRetMask:X2}"
            : "";
        sb.AppendLine($"{name,-20} Lib[{libIndex:D4}] '{libPath}' Func[{nameIndex:D4}] '{funcName}' ({argCount} args){kindTxt}{scalarKindMaskTxt}{maskTxt}{udtTxt}{callbackTxt}");
        return offset + instrLen;
    }

    /// <summary>
    /// Disassembles an OP_ON_JUMP instruction.
    /// <para>Formato: [OP_ON_JUMP] [jump_type (byte)] [num_targets (byte)] [target1 (ushort)] ... [targetN (ushort)]</para>
    /// </summary>
    private static int OnJumpInstruction(string name, CompiledChunk chunk, int offset, StringBuilder sb)
    {
        if (offset + 2 >= chunk.Code.Count) return PrintTruncated(offset, sb, name);
        var jumpType = (JumpType)chunk.Code[offset + 1];
        var targetCount = chunk.Code[offset + 2];
        var newOffset = offset + 3;
        sb.Append($"{name,-20} {jumpType} ({targetCount} targets) ->");

        for (var i = 0; i < targetCount; i++)
        {
            if (newOffset + 1 >= chunk.Code.Count) return PrintTruncated(offset, sb, "ON_JUMP target");
            var targetAddr = (chunk.Code[newOffset] << 8) | chunk.Code[newOffset + 1];
            sb.Append($" @{targetAddr:D4}");
            newOffset += 2;
        }

        sb.AppendLine();
        return newOffset;
    }

    /// <summary>
    /// Disassembles an OP_CLOSURE instruction.
    /// <para>Formato: [OP_CLOSURE] [const_index (ushort)] [num_upvalues (byte)] [is_local_1 (byte)] [index_1 (byte)] ...</para>
    /// </summary>
    private static int ClosureInstruction(string name, CompiledChunk chunk, int offset, StringBuilder sb)
    {
        var currentOffset = offset + 1;
        if (currentOffset + 2 >= chunk.Code.Count) return PrintTruncated(offset, sb, name);

        var constIndex = (chunk.Code[currentOffset++] << 8) | chunk.Code[currentOffset++];
        var upvalueCount = chunk.Code[currentOffset++];

        var functionAddress = chunk.Constants[constIndex];
        sb.AppendLine($"{name,-20} Const[{constIndex:D4}] @{functionAddress} ({upvalueCount} upvalues)");

        for (var i = 0; i < upvalueCount; i++)
        {
            if (currentOffset + 1 >= chunk.Code.Count) return PrintTruncated(offset, sb, "Closure Upvalue");
            var isLocal = chunk.Code[currentOffset++] == 1;
            var index = chunk.Code[currentOffset++];
            sb.AppendLine($"{offset:D4}    |                     {(isLocal ? "local" : "upvalue"),-7} {index}");
        }

        return currentOffset;
    }

    /// <summary>
    /// Disassembles an OP_GO_SELECT_SETUP instruction.
    /// <para>Formato (compilatore → VM): [OP_GO_SELECT_SETUP] [num_cases (byte)] [num_sends (byte)]
    /// [default_offset (ushort)] [case_addr (ushort) × num_cases]</para>
    /// <para>Il vecchio formato leggeva solo 3 byte ([num_canali][num_send]), saltando
    /// default_offset (2 byte) e gli indirizzi di ogni CASE (2 byte × N): ogni GO SELECT
    /// disallineava il resto del chunk di 2+2N byte (latente su GM: nessun sample con
    /// GO SELECT — scoperto da test_dasm su GMBasicAI con GoSelect*.bas). Corretto il
    /// 2026-08-12.</para>
    /// </summary>
    private static int GoSelectSetupInstruction(string name, CompiledChunk chunk, int offset, StringBuilder sb)
    {
        if (offset + 4 >= chunk.Code.Count) return PrintTruncated(offset, sb, name);
        var totalChannels = (chunk.Code[offset + 1] << 8) | chunk.Code[offset + 2];
        var sendOps = (chunk.Code[offset + 3] << 8) | chunk.Code[offset + 4];
        var newOffset = offset + 5;
        if (newOffset + 1 >= chunk.Code.Count) return PrintTruncated(offset, sb, name);
        var defaultOffset = (chunk.Code[newOffset] << 8) | chunk.Code[newOffset + 1];
        newOffset += 2;
        sb.Append($"{name,-20} Total={totalChannels}, Sends={sendOps}, Default=");
        sb.Append(defaultOffset == 0xFFFF ? "none" : $"@{defaultOffset:D4}");
        sb.Append(" ->");

        for (var i = 0; i < totalChannels; i++)
        {
            if (newOffset + 1 >= chunk.Code.Count) return PrintTruncated(offset, sb, "GO_SELECT case");
            var caseAddr = (chunk.Code[newOffset] << 8) | chunk.Code[newOffset + 1];
            sb.Append($" @{caseAddr:D4}");
            newOffset += 2;
        }

        sb.AppendLine();
        return newOffset;
    }

    /// <summary>
    /// Prints a message for a truncated instruction, indicating that the bytecode is incomplete.
    /// </summary>
    /// <returns>An offset far beyond the chunk size to terminate the disassembly loop.</returns>
    private static int PrintTruncated(int offset, StringBuilder sb, string name)
    {
        sb.AppendLine($"ERROR: Truncated instruction '{name}' at offset {offset}.");
        return offset + 1000;
    }

    /// <summary>
    /// Prints a message for an instruction with an invalid operand (e.g., an out-of-bounds index).
    /// </summary>
    /// <returns>The offset of the next instruction.</returns>
    private static int PrintInvalid(int offset, string message, StringBuilder sb)
    {
        sb.AppendLine($"ERROR: {message} at offset {offset}.");
        return offset + 3; // Assume a 2-byte operand for error recovery
    }
}
