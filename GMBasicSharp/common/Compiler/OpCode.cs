// FILE: Interpreter.Compiler.OpCode.cs

// ReSharper disable InconsistentNaming

namespace Compiler;

/// <summary>
/// Definisce il set di istruzioni (OpCode) per la Virtual Machine BASIC.
/// Ogni istruzione è rappresentata da un singolo byte.
/// </summary>
public enum OpCode : byte
{
    // --- Stack Operations ---

    /// <summary>
    /// Legge un valore dalla "constant pool" e lo spinge sulla stack.
    /// <para>Formato: [OP_PUSH_CONST] [index (ushort)] - 3 bytes</para>
    /// </summary>
    OP_PUSH_CONST = 0,

    /// <summary>
    /// Spinge sulla stack il valore speciale NIL (non inizializzato).
    /// <para>Formato: [OP_PUSH_NIL] - 1 byte</para>
    /// </summary>
    OP_PUSH_NIL = 1,

    /// <summary>
    /// Spinge sulla stack il valore booleano TRUE.
    /// <para>Formato: [OP_PUSH_TRUE] - 1 byte</para>
    /// </summary>
    OP_PUSH_TRUE = 2,

    /// <summary>
    /// Spinge sulla stack il valore booleano FALSE.
    /// <para>Formato: [OP_PUSH_FALSE] - 1 byte</para>
    /// </summary>
    OP_PUSH_FALSE = 3,

    /// <summary>
    /// Rimuove e scarta il valore in cima alla stack.
    /// <para>Formato: [OP_POP] - 1 byte</para>
    /// </summary>
    OP_POP = 4,

    /// <summary>
    /// Duplica il valore in cima alla stack.
    /// <para>Formato: [OP_DUP] - 1 byte</para>
    /// </summary>
    OP_DUP = 5,

    // --- Arithmetic & Logical Operations ---

    /// <summary>
    /// Preleva due valori dalla stack, li somma e spinge il risultato.
    /// <para>Formato: [OP_ADD] - 1 byte</para>
    /// </summary>
    OP_ADD = 6,

    /// <summary>
    /// Preleva due valori dalla stack, li sottrae e spinge il risultato.
    /// <para>Formato: [OP_SUBTRACT] - 1 byte</para>
    /// </summary>
    OP_SUBTRACT = 7,

    /// <summary>
    /// Preleva due valori dalla stack, li moltiplica e spinge il risultato.
    /// <para>Formato: [OP_MULTIPLY] - 1 byte</para>
    /// </summary>
    OP_MULTIPLY = 8,

    /// <summary>
    /// Preleva due valori dalla stack, li divide e spinge il risultato.
    /// <para>Formato: [OP_DIVIDE] - 1 byte</para>
    /// </summary>
    OP_DIVIDE = 9,

    /// <summary>
    /// Preleva due valori dalla stack, esegue la divisione intera e spinge il risultato.
    /// <para>Formato: [OP_INT_DIVIDE] - 1 byte</para>
    /// </summary>
    OP_INT_DIVIDE = 10,

    /// <summary>
    /// Preleva due valori, eleva il primo alla potenza del secondo e spinge il risultato.
    /// <para>Formato: [OP_POWER] - 1 byte</para>
    /// </summary>
    OP_POWER = 11,

    /// <summary>
    /// Preleva un valore dalla stack, ne inverte il segno e spinge il risultato.
    /// <para>Formato: [OP_NEGATE] - 1 byte</para>
    /// </summary>
    OP_NEGATE = 12,

    /// <summary>
    /// Preleva un valore dalla stack, ne esegue la negazione logica e spinge il risultato.
    /// <para>Formato: [OP_NOT] - 1 byte</para>
    /// </summary>
    OP_NOT = 13,

    /// <summary>
    /// Preleva due valori dalla stack, calcola il resto della divisione e spinge il risultato.
    /// <para>Formato: [OP_MOD] - 1 byte</para>
    /// </summary>
    OP_MOD = 14,




    // --- Comparison Operations ---

    /// <summary>
    /// Preleva due valori dalla stack, li confronta per uguaglianza e spinge il risultato booleano.
    /// <para>Formato: [OP_CMP_EQ] - 1 byte</para>
    /// </summary>
    OP_CMP_EQ = 18,

    /// <summary>
    /// Preleva due valori dalla stack, li confronta per disuguaglianza e spinge il risultato booleano.
    /// <para>Formato: [OP_CMP_NE] - 1 byte</para>
    /// </summary>
    OP_CMP_NE = 19,

    /// <summary>
    /// Preleva due valori dalla stack, confronta se il primo è maggiore del secondo e spinge il risultato booleano.
    /// <para>Formato: [OP_CMP_GT] - 1 byte</para>
    /// </summary>
    OP_CMP_GT = 20,

    /// <summary>
    /// Preleva due valori dalla stack, confronta se il primo è maggiore o uguale al secondo e spinge il risultato booleano.
    /// <para>Formato: [OP_CMP_GTE] - 1 byte</para>
    /// </summary>
    OP_CMP_GTE = 21,

    /// <summary>
    /// Preleva due valori dalla stack, confronta se il primo è minore del secondo e spinge il risultato booleano.
    /// <para>Formato: [OP_CMP_LT] - 1 byte</para>
    /// </summary>
    OP_CMP_LT = 22,

    /// <summary>
    /// Preleva due valori dalla stack, confronta se il primo è minore o uguale al secondo e spinge il risultato booleano.
    /// <para>Formato: [OP_CMP_LTE] - 1 byte</para>
    /// </summary>
    OP_CMP_LTE = 23,

    // --- Variable & Data Structure Operations ---

    /// <summary>
    /// Definisce una nuova variabile globale. L'operando è l'indice della variabile. Preleva il valore iniziale dalla stack.
    /// <para>Formato: [OP_DEFINE_GLOBAL] [index (ushort)] - 3 bytes</para>
    /// </summary>
    OP_DEFINE_GLOBAL = 24,

    /// <summary>
    /// Legge il valore di una variabile globale e lo spinge sulla stack. L'operando è l'indice della variabile.
    /// <para>Formato: [OP_LOAD_GLOBAL] [index (ushort)] - 3 bytes</para>
    /// </summary>
    OP_LOAD_GLOBAL = 25,

    /// <summary>
    /// Preleva un valore dalla stack e lo assegna a una variabile globale. L'operando è l'indice della variabile.
    /// <para>Formato: [OP_STORE_GLOBAL] [index (ushort)] - 3 bytes</para>
    /// </summary>
    OP_STORE_GLOBAL = 26,

    /// <summary>
    /// Legge il valore di una variabile locale dallo stack frame corrente e lo spinge sulla stack. L'operando è l'indice (slot) della variabile.
    /// <para>Formato: [OP_LOAD_LOCAL] [slot_index (byte)] - 2 bytes</para>
    /// </summary>
    OP_LOAD_LOCAL = 27,

    /// <summary>
    /// Preleva un valore dalla stack e lo assegna a una variabile locale. L'operando è l'indice (slot) della variabile.
    /// <para>Formato: [OP_STORE_LOCAL] [slot_index (byte)] - 2 bytes</para>
    /// </summary>
    OP_STORE_LOCAL = 28,
    OP_ADJUST_STRING = 29,
    OP_MAKE_BYREF = 30,
    OP_MAKE_BYREF_GLOBAL = 31,
    OP_IS_TYPE = 32,
    OP_CALL_INDIRECT = 33,
    OP_CALL_N = 34,

    /// <summary>
    /// Trova una variabile locale, crea un oggetto riferimento ad essa e lo spinge sulla stack. Usato per passare argomenti BYREF.
    /// <para>Formato: [OP_PUSH_REF_LOCAL] [slot_index (byte)] - 2 bytes</para>
    /// </summary>
    OP_PUSH_REF_LOCAL = 36,

    /// <summary>
    /// Trova una variabile globale, crea un oggetto riferimento ad essa e lo spinge sulla stack. Usato per passare argomenti BYREF.
    /// <para>Formato: [OP_PUSH_REF_GLOBAL] [index (ushort)] - 3 bytes</para>
    /// </summary>
    OP_PUSH_REF_GLOBAL = 37,

    /// <summary>
    /// Preleva un array/oggetto e N indici dalla stack, legge l'elemento e spinge il suo valore sulla stack. L'operando (byte) è il numero di indici N.
    /// <para>Formato: [OP_LOAD_ELEMENT] [num_indices (byte)] - 2 bytes</para>
    /// </summary>
    OP_LOAD_ELEMENT = 38,

    /// <summary>
    /// Restituisce la lunghezza di un array in cima allo stack.
    /// <para>Formato: [OP_ARRAY_LEN] - 1 byte</para>
    /// </summary>
    OP_ARRAY_LEN = 39,

    /// <summary>
    /// Preleva un valore, un array/oggetto e N indici dalla stack, e salva il valore nell'elemento specificato. L'operando (byte) è il numero di indici N.
    /// <para>Formato: [OP_STORE_ELEMENT] [num_indices (byte)] - 2 bytes</para>
    /// </summary>
    OP_STORE_ELEMENT = 40,

    /// <summary>
    /// Preleva una struttura (UDT) dalla stack, legge un campo e spinge il suo valore sulla stack. L'operando (short) è l'indice del nome del campo nella constant pool.
    /// <para>Formato: [OP_LOAD_FIELD] [const_index (ushort)] - 3 bytes</para>
    /// </summary>
    OP_LOAD_FIELD = 41,

    /// <summary>
    /// Preleva un valore e una struttura (UDT) dalla stack, e salva il valore nel campo specificato. L'operando (short) è l'indice del nome del campo.
    /// <para>Formato: [OP_STORE_FIELD] [const_index (ushort)] - 3 bytes</para>
    /// </summary>
    OP_STORE_FIELD = 42,

    /// <summary>
    /// Ridimensiona un array. Operandi: isLocal (byte), varIndex (short), #dimensioni (byte). Le nuove dimensioni sono sulla stack.
    /// <para>Formato: [OP_REDIM] [is_local (byte)] [var_index (ushort)] [num_dims (byte)] - 5 bytes</para>
    /// </summary>
    OP_REDIM = 43,

    /// <summary>
    /// Ridimensiona un array conservando i dati esistenti. La sua struttura di operandi è identica a OP_REDIM.
    /// <para>Formato: [OP_REDIM_PRESERVE] [is_local (byte)] [var_index (ushort)] [num_dims (byte)] - 5 bytes</para>
    /// </summary>
    OP_REDIM_PRESERVE = 44,

    /// <summary>
    /// Crea un nuovo array. Preleva N valori dalla stack (dove N è l'operando short) e li usa per popolare un nuovo array.
    /// <para>Formato: [OP_MK_ARRAY] [num_elements (ushort)] - 3 bytes</para>
    /// </summary>
    OP_MK_ARRAY = 45,

    /// <summary>
    /// Inizializza un array già esistente con N valori.
    /// <para>Formato: [OP_INIT_ARRAY] [num_values (ushort)] - 3 bytes</para>
    /// </summary>
    OP_INIT_ARRAY = 46,


    /// Crea un nuovo dizionario. Preleva N coppie chiave-valore dalla stack (dove N è l'operando short) e le usa per popolare il dizionario.
    /// <para>Formato: [OP_MK_DICT] [num_entries (ushort)] - 3 bytes</para>
    /// </summary>
    OP_MK_DICT = 48,

    /// <summary>
    /// Crea una nuova lista. Preleva N valori dalla stack (dove N è l'operando short) e li usa per popolare la lista.
    /// <para>Formato: [OP_MK_LIST] [num_elements (ushort)] - 3 bytes</para>
    /// </summary>
    OP_MK_LIST = 49,

    
    // --- Control Flow & Procedures ---

    /// <summary>
    /// Esegue un salto incondizionato in avanti.
    /// <para>Formato: [OP_JUMP_FORWARD] [offset (ushort)] - 3 bytes</para>
    /// </summary>
    OP_JUMP_FORWARD = 51,

    /// <summary>
    /// Esegue un salto incondizionato all'indietro (per i loop).
    /// <para>Formato: [OP_JUMP_BACKWARD] [offset (ushort)] - 3 bytes</para>
    /// </summary>
    OP_JUMP_BACKWARD = 52,

    /// <summary>
    /// Preleva un valore dalla stack. Se è vero, esegue un salto in avanti.
    /// <para>Formato: [OP_JUMP_IF_TRUE] [offset (ushort)] - 3 bytes</para>
    /// </summary>
    OP_JUMP_IF_TRUE = 53,

    /// <summary>
    /// Preleva un valore dalla stack. Se è falso, esegue un salto in avanti.
    /// <para>Formato: [OP_JUMP_IF_FALSE] [offset (ushort)] - 3 bytes</para>
    /// </summary>
    OP_JUMP_IF_FALSE = 54,

    /// <summary>
    /// Chiama una procedura/funzione a un indirizzo assoluto. Il numero di argomenti è sulla stack.
    /// <para>Formato: [OP_CALL] [address (ushort)] - 3 bytes</para>
    /// </summary>
    OP_CALL = 55,

    /// <summary>
    /// Chiamata a costruttore (BASE.NEW). Stack: [arg1, ..., argN, SELF, num_args].
    /// Crea frame locals e salta al costruttore.
    /// <para>Formato: [OP_CALL_CTOR] [address (ushort)] - 3 bytes</para>
    /// </summary>
    OP_CALL_CTOR = 56,

    /// <summary>
    /// Chiama una funzione predefinita del runtime.
    /// <para>Formato: [OP_CALL_BUILTIN] [function_code (byte)] [arg_count (byte)] - 3 bytes</para>
    /// </summary>
    OP_CALL_BUILTIN = 57,

    /// <summary>
    /// Chiama una funzione esterna da una libreria dinamica (FFI).
    /// <para>Formato: [OP_CALL_FFI] [const_index (ushort)] [arg_count (byte)] - 4 bytes</para>
    /// </summary>
    OP_CALL_FFI = 58,

    /// <summary>
    /// Esegue un salto incondizionato a un indirizzo assoluto.
    /// <para>Formato: [OP_GOTO] [address (ushort)] - 3 bytes</para>
    /// </summary>
    OP_GOTO = 59,

    /// <summary>
    /// Esegue un GOSUB a un indirizzo assoluto, pushando l'indirizzo di ritorno.
    /// <para>Formato: [OP_GOSUB] [address (ushort)] - 3 bytes</para>
    /// </summary>
    OP_GOSUB = 60,

    /// <summary>
    /// Resetta il ReturnValue del frame corrente a null.
    /// <para>Formato: [OP_CLEAR_RETURN] - 1 byte</para>
    /// </summary>
    OP_CLEAR_RETURN = 61,

    /// <summary>
    /// Termina l'esecuzione di una funzione/sub e ritorna al chiamante.
    /// <para>Formato: [OP_RETURN] - 1 byte</para>
    /// </summary>
    OP_RETURN = 62,

    /// <summary>
    /// Interrompe immediatamente l'esecuzione del programma.
    /// <para>Formato: [OP_HALT] - 1 byte</para>
    /// </summary>
    OP_HALT = 63,



    // --- Concurrency / GoRoutines & Generators ---

    /// <summary>
    /// Crea una nuova GoRoutine e la prepara per l'esecuzione.
    /// <para>Formato: [OP_GO] [address (ushort)] - 3 bytes</para>
    /// </summary>
    OP_GO = 66,

    /// <summary>
    /// Invia un valore a un canale. Il valore e il canale sono sulla stack.
    /// <para>Formato: [OP_SEND] - 1 byte</para>
    /// </summary>
    OP_SEND = 67,

    /// <summary>
    /// Riceve un valore da un canale. Il canale è sulla stack.
    /// <para>Formato: [OP_RECV] - 1 byte</para>
    /// </summary>
    OP_RECV = 68,

    /// <summary>
    /// Attende che un WaitGroup arrivi a zero. Il WaitGroup è sulla stack.
    /// <para>Formato: [OP_WG_WAIT] - 1 byte</para>
    /// </summary>
    OP_WG_WAIT = 69,

    /// <summary>
    /// Riceve da un canale e pusha DUE valori: il valore ricevuto e il flag ok (1 se valore
    /// reale, 0 se il canale è chiuso e svuotato — parità Go v, ok := &lt;-ch). Usato da
    /// `RECEIVE v FROM ch OK ok`. Il canale è sulla stack.
    /// <para>Formato: [OP_RECV_OK] - 1 byte</para>
    /// </summary>
    OP_RECV_OK = 136,

    /// <summary>
    /// Decrementa il contatore di un WaitGroup. Il WaitGroup è sulla stack.
    /// <para>Formato: [OP_WG_DONE] - 1 byte</para>
    /// </summary>
    OP_WG_DONE = 70,

    /// <summary>
    /// Interrompe l'esecuzione della GoRoutine corrente per un numero di millisecondi. Il valore è sulla stack.
    /// <para>Formato: [OP_SLEEP] - 1 byte</para>
    /// </summary>
    OP_SLEEP = 71,

    /// <summary>
    /// Esegue il setup per un'istruzione GO SELECT.
    /// <para>Formato: [OP_GO_SELECT_SETUP] [num_canali (byte)] [num_send (byte)] - 3 bytes</para>
    /// </summary>
    OP_GO_SELECT_SETUP = 72,

    /// <summary>
    /// Avanza un'istanza di generatore e pusha il valore.
    /// <para>Formato: [OP_NEXT_GENERATOR] - 1 byte</para>
    /// </summary>
    OP_NEXT_GENERATOR = 73,

    /// <summary>
    /// Attende il completamento di un FutureValue e pusha il risultato.
    /// <para>Formato: [OP_RESOLVE_FUTURE] - 1 byte</para>
    /// </summary>
    OP_RESOLVE_FUTURE = 74,

    // --- Data Statements ---

    /// <summary>
    /// Legge il prossimo valore dalla tabella DATA e lo lascia sulla stack.
    /// <para>Formato: [OP_READ] - 1 byte</para>
    /// </summary>
    OP_READ = 75,

    /// <summary>
    /// Resetta il "data pointer" della VM alla posizione DATA indicata dall'operando
    /// (0 = inizio pool; RESTORE &lt;riga|etichetta&gt; risolve la posizione a compile-time).
    /// <para>Formato: [OP_RESTORE] [position (ushort)] - 3 bytes</para>
    /// </summary>
    OP_RESTORE = 76,

    // --- Error Handling & Events ---

    /// <summary>
    /// Lancia un'eccezione con un codice e un messaggio (sulla stack).
    /// <para>Formato: [OP_THROW] - 1 byte</para>
    /// </summary>
    OP_THROW = 77,

    /// <summary>
    /// Imposta l'indirizzo del gestore di errori `ON ERROR GOTO`.
    /// <para>Formato: [OP_SET_ERR_HANDLER_GOTO] [address (ushort)] - 3 bytes</para>
    /// </summary>
    OP_SET_ERR_HANDLER_GOTO = 78,

    /// <summary>
    /// Imposta l'indirizzo del gestore di errori `ON ERROR GOSUB`.
    /// <para>Formato: [OP_SET_ERR_HANDLER_GOSUB] [address (ushort)] - 3 bytes</para>
    /// </summary>
    OP_SET_ERR_HANDLER_GOSUB = 79,

    /// <summary>
    /// Riprende l'esecuzione dopo un errore gestito.
    /// <para>Formato: [OP_RESUME] [address (ushort)] - 3 bytes</para>
    /// </summary>
    OP_RESUME = 80,

    /// <summary>
    /// Imposta l'handling "resume next" (`ON ERROR RESUME NEXT`): <b>statement che imposta
    /// l'handler</b>, distinto da <see cref="OP_RESUME"/> (`RESUME NEXT`, che SALTA).
    /// <para>Formato: [OP_SET_ERR_HANDLER_RESUME] - 1 byte</para>
    /// </summary>
    OP_SET_ERR_HANDLER_RESUME = 84,

    /// <summary>
    /// Registra un gestore di eventi per un tasto. Richiede 3 valori sulla stack: KeyCode, ActionType, TargetAddress.
    /// <para>Formato: [OP_ON_KEY] - 1 byte</para>
    /// </summary>
    OP_ON_KEY = 81,

    /// <summary>
    /// Installa un gestore di eccezioni `TRY...CATCH`. Spinge un frame di eccezione sulla call stack.
    /// <para>Formato: [OP_PUSH_TRY] [offset (ushort)] - 3 bytes</para>
    /// </summary>
    OP_PUSH_TRY = 82,

    /// <summary>
    /// Rimuove il gestore di eccezioni `TRY...CATCH` corrente dalla call stack.
    /// <para>Formato: [OP_POP_TRY] - 1 byte</para>
    /// </summary>
    OP_POP_TRY = 83,



    // ========================================================================
    // --- Opcodes a Prefisso (Polimorfici) ---
    // ========================================================================

    /// <summary>
    /// Esegue una chiamata a una funzione di sistema atomica.
    /// <para>Formato: [OP_SYSCALL] [syscall_code (byte)] [arg_count (byte)] - 3 bytes</para>
    /// </summary>
    OP_SYSCALL = 86,

    /// <summary>
    /// Esegue un comando complesso di I/O su file.
    /// <para>Formato: [OP_FILE_CMD] [sub_opcode (byte)] [arg_count (byte)] - 3 bytes</para>
    /// </summary>
    OP_FILE_CMD = 87,

    /// <summary>
    /// Esegue un comando di rete complesso.
    /// <para>Formato: [OP_NET_CMD] [sub_opcode (byte)] [arg_count (byte)] - 3 bytes</para>
    /// </summary>
    OP_NET_CMD = 88,

    /// <summary>
    /// Esegue un comando audio complesso.
    /// <para>Formato: [OP_AUDIO_CMD] [sub_opcode (byte)] [arg_count (byte)] - 3 bytes</para>
    /// </summary>
    OP_AUDIO_CMD = 89,

    /// <summary>
    /// Esegue un comando complesso per gli appunti.
    /// <para>Formato: [OP_CLIP_CMD] [sub_opcode (byte)] [arg_count (byte)] - 3 bytes</para>
    /// </summary>
    OP_CLIP_CMD = 90,

    /// <summary>
    /// Esegue un comando complesso per le matrici di trasformazione.
    /// <para>Formato: [OP_MATRIX_CMD] [sub_opcode (byte)] [arg_count (byte)] - 3 bytes</para>
    /// </summary>
    OP_MATRIX_CMD = 92,

    /// <summary>
    /// Esegue un comando vettoriale complesso in batch.
    /// <para>Formato: [OP_VECTOR_CMD] [sub_opcode (byte)] [arg_count (byte)] - 3 bytes</para>
    /// </summary>
    OP_VECTOR_CMD = 93,

    /// <summary>
    /// Istanzia un nuovo oggetto di un tipo specifico (es. HTTPOPTIONS).
    /// <para>Formato: [OP_NEW] [type_code (ushort)] - 3 bytes</para>
    /// </summary>
    OP_NEW = 94,

    /// <summary>
    /// Alloca e inizializza una nuova istanza di un UDT. L'operando (ushort) è un indice al descrittore del tipo.
    /// <para>Formato: [OP_NEW_UDT_INSTANCE] [const_index (ushort)] - 3 bytes</para>
    /// </summary>
    OP_NEW_UDT_INSTANCE = 95,
    
    /// <summary>
    /// Esegue la funzione speciale FETCH. L'operando (byte) indica la variante (sincrona/asincrona).
    /// <para>Formato: [OP_FETCH] [variant (byte)] - 2 bytes</para>
    /// </summary>
    OP_FETCH = 96,

    // ========================================================================
    // --- Opcodes per Classi & Oggetti ---
    // ========================================================================

    /// <summary>
    /// Spinge sulla stack un riferimento a un blocco di codice P-Code compilato.
    /// <para>Formato: [OP_PUSH_CODE_REF] [const_index (ushort)] - 3 bytes</para>
    /// </summary>
    OP_PUSH_CODE_REF = 97,

    /// <summary>
    /// Alloca una nuova istanza di una classe e la spinge sulla stack.
    /// <para>Formato: [OP_NEW_CLASS_INSTANCE] [const_index (ushort)] - 3 bytes</para>
    /// </summary>
    OP_NEW_CLASS_INSTANCE = 98,

    /// <summary>
    /// Carica il valore di un campo/proprietà da un'istanza (SELF) sulla stack.
    /// <para>Formato: [OP_LOAD_MEMBER] [offset (ushort)] - 3 bytes</para>
    /// </summary>
    OP_LOAD_MEMBER = 99,

    /// <summary>
    /// Preleva un valore dalla stack e lo assegna a un campo/proprietà di un'istanza (SELF).
    /// <para>Formato: [OP_STORE_MEMBER] [offset (ushort)] - 3 bytes</para>
    /// </summary>
    OP_STORE_MEMBER = 100,

    /// <summary>
    /// Chiama un metodo o operatore su un'istanza (SELF). L'operando (short) è un indice nella constant pool al nome del metodo.
    /// <para>Formato: [OP_CALL_MEMBER] [const_index (ushort)] - 3 bytes</para>
    /// </summary>
    OP_CALL_MEMBER = 101,

    /// <summary>
    /// Inizia un blocco di finalizzazione dello scope. Usato per i distruttori.
    /// <para>Formato: [OP_FINALIZE_SCOPE] - 1 byte</para>
    /// </summary>
    OP_FINALIZE_SCOPE = 102,

    /// <summary>
    /// Esegue il distruttore (SUB DESTRUCTOR) di una variabile globale, dato l'indice
    /// nel global array. L'istanza deve essere un Dictionary con chiave "__class__".
    /// <para>Formato: [OP_FINALIZE_GLOBAL] [global_index (ushort)]</para>
    /// </summary>
    OP_FINALIZE_GLOBAL = 103,
    
    /// <summary>
    /// Esegue un salto calcolato basato su un indice. Usato per ON...GOTO/GOSUB.
    /// <para>Formato: [OP_ON_JUMP] [jump_type (byte)] [num_targets (byte)] [target1 (ushort)] ... [targetN (ushort)]</para>
    /// </summary>
    OP_ON_JUMP = 104,
    
    // ========================================================================
    // --- Opcodes per Macro ---
    // ========================================================================

    /// <summary>
    /// Valuta una macro usata come istruzione. Preleva una stringa dalla stack,
    /// la compila JIT in un nuovo Stmt e la esegue.
    /// <para>Formato: [OP_EVAL_MACRO_STMT] - 1 byte</para>
    /// </summary>
    OP_EVAL_MACRO_STMT = 105,

    /// <summary>
    /// Valuta una macro usata in un'espressione. Preleva una stringa dalla stack,
    /// la compila JIT in una nuova Expr, la esegue e pusha il risultato sulla stack.
    /// <para>Formato: [OP_EVAL_MACRO_EXPR] - 1 byte</para>
    /// </summary>
    OP_EVAL_MACRO_EXPR = 106,
    
    // ========================================================================
    // --- Opcodes per Procedure Locali e Closure ---
    // ========================================================================

    /// <summary>
    /// Crea una closure per una funzione locale. L'operando (ushort) è un indice
    /// nella constant pool che punta al P-Code del corpo della funzione.
    /// È seguito da una serie di operandi (2 * num_upvalues) che descrivono
    /// quali variabili locali catturare.
    /// <para>Formato: [OP_CLOSURE] [const_index (ushort)] [num_upvalues (byte)] [is_local_1 (byte)] [index_1 (byte)] ...</para>
    /// </summary>
    OP_CLOSURE = 107,

    /// <summary>
    /// Carica il valore di una variabile catturata (upvalue) dalla closure corrente.
    /// <para>Formato: [OP_LOAD_UPVALUE] [index (byte)] - 2 bytes</para>
    /// </summary>
    OP_LOAD_UPVALUE = 108,
    
    /// <summary>
    /// Assegna un valore a una variabile catturata (upvalue) nella closure corrente.
    /// <para>Formato: [OP_STORE_UPVALUE] [index (byte)] - 2 bytes</para>
    /// </summary>
    OP_STORE_UPVALUE = 109,
    
    // ========================================================================
    // --- Opcodes per Cicli Ottimizzati ---
    // ========================================================================

    /// <summary>
    /// Prepara un ciclo FOR. Preleva start, end, e step dalla stack e li memorizza
    /// in uno stato interno della VM. Assegna il valore iniziale al contatore.
    /// <para>Formato: [OP_FOR_PREPARE] [local_slot (byte)] - 2 bytes</para>
    /// </summary>
    OP_FOR_PREPARE = 110,

    /// <summary>
    /// Esegue un'iterazione di un ciclo FOR. Incrementa il contatore, lo confronta
    /// con il limite e salta indietro o in avanti.
    /// <para>Formato: [OP_FOR_LOOP] [local_slot (byte)] [jump_offset (ushort)] - 4 bytes</para>
    /// </summary>
    OP_FOR_LOOP = 111,
    
    // ========================================================================
    // --- Opcodes per FOR EACH ---
    // ========================================================================
    
    
    // ========================================================================
    // --- Opcodes per PRINT Ottimizzato ---
    // ========================================================================
    
    /// <summary>
    /// Preleva un valore dalla stack e lo stampa sulla console/schermo attivo
    /// senza aggiungere un newline. Corrisponde a `PRINT valore;`.
    /// <para>Formato: [OP_PRINT_VALUE] - 1 byte</para>
    /// </summary>
    OP_PRINT_VALUE = 113,

    /// <summary>
    /// Preleva un valore dalla stack, lo stampa, e aggiunge un newline.
    /// Corrisponde a `PRINT valore`.
    /// <para>Formato: [OP_PRINT_LINE] - 1 byte</para>
    /// </summary>
    OP_PRINT_LINE = 114,

    /// <summary>
    /// Sposta il cursore alla successiva "print zone" (tabulazione).
    /// Corrisponde alla virgola (,) in un'istruzione PRINT.
    /// <para>Formato: [OP_PRINT_TAB_ZONE] - 1 byte</para>
    /// </summary>
    OP_PRINT_TAB_ZONE = 115,

    /// <summary>
    /// Stampa un carattere di newline. Corrisponde a `PRINT` senza argomenti.
    /// <para>Formato: [OP_PRINT_NEWLINE] - 1 byte</para>
    /// </summary>
    OP_PRINT_NEWLINE = 116,

    /// <summary>
    /// Chiude un'istruzione PRINT che NON chiede il newline finale (`PRINT x;` / `PRINT x,`):
    /// il parziale accumulato viene COMMESSO come testo, senza a capo.
    /// <para>Perché serve: senza un marcatore di fine istruzione lo staging di stampa resta
    /// pendente e il PRINT di una procedura chiamata DOPO (frame diverso) lo SOSPENDE, così
    /// l'ordine di stampa si inverte (il parziale finisce in fondo). C99Native chiude sempre
    /// lo staging a fine istruzione (<c>gmb_print_stage_end</c>), quindi il parziale è già
    /// emesso prima del PRINT annidato.</para>
    /// <para>Formato: [OP_PRINT_FLUSH] - 1 byte</para>
    /// </summary>
    OP_PRINT_FLUSH = 216,
    
    // ========================================================================
    // --- Opcodes per Cooperativo Scheduling ---
    // ========================================================================
    
    /// <summary>
    /// Yield point volontario. Il task cede il controllo allo scheduler.
    /// Emesso dal compilatore dopo ogni statement per garantire scheduling cooperativo.
    /// <para>Formato: [OP_YIELD] - 1 byte</para>
    /// </summary>
    OP_YIELD = 117,
    
    // ========================================================================
    // --- Opcodes per Concorrenza (WaitGroup) ---
    // ========================================================================
    
    /// <summary>
    /// Crea un nuovo WaitGroupInfo (contatore) e lo spinge sulla stack.
    /// <para>Formato: [OP_NEW_WAITGROUP] - 1 byte</para>
    /// </summary>
    OP_NEW_WAITGROUP = 118,
    
    /// <summary>
    /// Crea un nuovo canale (ChannelInfo) e lo spinge sulla stack.
    /// <para>Formato: [OP_NEW_CHANNEL] - 1 byte</para>
    /// </summary>
    OP_NEW_CHANNEL = 119,

    /// <summary>
    /// Copia per valore un'istanza UDT sulla cima dello stack: sostituisce il dizionario con
    /// una SUA copia superficiale indipendente (stessi valori, nuovo contenitore), cosi' che
    /// due variabili UDT dopo un'assegnazione (`P2 = P1`) non condividano piu' lo stesso
    /// dizionario sottostante. Se il valore in cima non e' un dizionario (es. NIL), non fa
    /// nulla — nessun'altra assegnazione (numeri, stringhe, CLASS per riferimento) ne ha
    /// bisogno, solo un target di tipo UserDefinedType lo emette (vedi VisitLetStatement).
    /// <para>Formato: [OP_CLONE_UDT] - 1 byte</para>
    /// </summary>
    OP_CLONE_UDT = 120,

    /// <summary>
    /// Legge il valore corrente di un campo STATIC di CLASS (condiviso da tutte le istanze,
    /// vedi PCodeProcessor.StaticFieldValues) e lo spinge sulla stack. A differenza di
    /// OP_LOAD_MEMBER, non richiede un'istanza sulla stack: il campo non appartiene a nessun
    /// oggetto in particolare.
    /// <para>Formato: [OP_LOAD_STATIC_FIELD] [symbolId (ushort)] - 3 bytes</para>
    /// </summary>
    OP_LOAD_STATIC_FIELD = 121,

    /// <summary>
    /// Scrive il valore in cima alla stack in un campo STATIC di CLASS (condiviso da tutte le
    /// istanze). A differenza di OP_STORE_MEMBER, non richiede un'istanza sulla stack.
    /// <para>Formato: [OP_STORE_STATIC_FIELD] [symbolId (ushort)] - 3 bytes</para>
    /// </summary>
    OP_STORE_STATIC_FIELD = 122,

    /// <summary>
    /// Costruisce un oggetto generatore lazy da un'espressione generatore
    /// <c>(expr FOR var IN source [IF cond])</c>. Preleva dalla stack, in ordine, le 3 closure
    /// pre-compilate (sorgente a 0 argomenti, condizione a 1 argomento o NIL se assente, output a
    /// 1 argomento — l'argomento è il valore corrente dell'iteratore) e le impacchetta in un
    /// oggetto runtime che implementa <c>IEnumerable</c> (consumabile direttamente da
    /// OP_FOR_EACH_ADVANCE, che già accetta qualunque IEnumerable) e da NEXT().
    /// <para>Formato: [OP_NEW_GENERATOR_EXPR] - 1 byte. Stack: [..., sourceClosure, conditionClosureOrNil, outputClosure] → [..., generatorObj]</para>
    /// </summary>
    OP_NEW_GENERATOR_EXPR = 123,

    /// <summary>
    /// Chiama un GENERATOR PROCEDURE (<c>GENERATOR Nome(...) AS T ... END GENERATOR</c>) SENZA
    /// eseguirne il corpo: crea un PCodeTask dedicato, lega gli argomenti nei suoi locals (stessa
    /// convenzione posizionale di OP_CALL_N, ma sull'array locals del NUOVO task, non del
    /// chiamante) e lo lascia sospeso — l'esecuzione parte solo al primo NEXT(). Analogo a OP_GO
    /// ma senza lanciare un thread reale: il task dedicato viene guidato in modo sincrono da
    /// NEXT() (ExecuteBuiltIn, BuiltInFunction.NEXT), mai eseguito in parallelo al chiamante.
    /// <para>Formato: [OP_NEW_GENERATOR_CALL] [address (ushort)] [argCount (byte)] - 4 bytes. Stack: [..., arg1, ..., argN] → [..., generatorInstance]</para>
    /// </summary>
    OP_NEW_GENERATOR_CALL = 124,

    /// <summary>
    /// Lancia una <c>GO FUNCTION</c> (a differenza di OP_GO/GO SUB, il chiamante vuole il
    /// risultato). Stessa meccanica di accodamento cooperativo di OP_GO (nessun thread reale,
    /// PCodeScheduler.Schedule), ma in più crea un <c>PCodeFutureValue</c> collegato al task
    /// tramite <c>PCodeTask.FutureToResolve</c> (risolto dal driver quando il task finisce, con
    /// il valore lasciato sullo stack da RETURN — vedi PCodeVm.Run/HandlePostTick) e lo PUSHA
    /// sullo stack del chiamante: <c>GO FUNCTION F(...) INTO X</c> compila come
    /// <c>X = &lt;GoFunctionExpr&gt;</c>, quindi l'espressione DEVE lasciare esattamente un
    /// valore sullo stack (il future), a differenza di GO SUB che è uno statement e non lascia
    /// nulla.
    /// <para>Formato: [OP_GO_FUNCTION] [address (ushort)] - 3 bytes. Stack: [..., arg1, ..., argN, wgOrNil] → [..., futureValue]</para>
    /// </summary>
    OP_GO_FUNCTION = 125,

    /// <summary>
    /// Variante di OP_LOAD_ELEMENT usata ESCLUSIVAMENTE da FOR EACH per avanzare su un
    /// IDictionary: l'indice è un contatore posizionale (0, 1, 2, ...), non una chiave reale —
    /// legge la N-esima entry nell'ordine di enumerazione (DictionaryEntry con Key/Value), non
    /// dict[N]. Su Array/IList/string il comportamento è identico a OP_LOAD_ELEMENT (l'indice e'
    /// gia' posizionale per quei tipi). Introdotta per rompere l'ambiguita' storica di
    /// OP_LOAD_ELEMENT, che trattava QUALUNQUE chiave numerica su un IDictionary come indice
    /// posizionale, rendendo un vero DICT&lt;LONG,V&gt; illeggibile via indicizzatore (d[5]
    /// restituiva sempre l'entry sbagliata invece del valore alla chiave 5), mentre
    /// OP_STORE_ELEMENT (scrittura) trattava la stessa chiave sempre come reale. Unico
    /// emettitore: VisitForEachAdvanceAndAssignStatement (Compiler.Others.cs). Porting da
    /// GMBasicAI (commit 701da5e).
    /// <para>Formato: [OP_LOAD_ELEMENT_POSITIONAL] [num_indices (byte)] - 2 bytes</para>
    /// </summary>
    OP_LOAD_ELEMENT_POSITIONAL = 126,
    OP_ON_MOUSE = 127,

    /// <summary>
    /// Termina una subroutine di predicato VALID (generata dal compilatore per
    /// `@ row,col GET var ... VALID expr`). Il flusso NON la esegue mai da solo: la VM la
    /// chiama (salvando/ripristinando Ip) quando deve rivalutare il predicato col valore
    /// corrente del campo; il risultato (bool) resta in cima allo stack.
    /// <para>Formato: [OP_END_VALID] - 1 byte</para>
    /// </summary>
    OP_END_VALID = 128,

    OP_BREAKPOINT = 129,

    /// <summary>
    /// Chiama una procedura dell'HOST da una macro/EVAL annidata (S_eval §ii).
    /// L'indice (ushort) punta nella tabella host-procs in-memory del chunk ospite
    /// (MAI serializzata: PCodeVm.MacroHostTable) — MAI nel formato A1/GMBC.
    /// Gli argomenti sono già sullo stack (in ordine); il risultato va sullo stack.
    /// VM/C e C99 non lo incontrano mai (chiamate native già ok lì).
    /// <para>Formato: [OP_CALL_HOST] [table_index (ushort)] - 3 bytes</para>
    /// </summary>
    OP_CALL_HOST = 156,

    /// <summary>
    /// Destruttura il valore di iterazione di un FOR EACH su MATRICE in N elementi.
    /// Formato: [OP_FE_DESTRUCT][N]. Lo stack: ... valore. Se valore è un Array (riga di
    /// matrice), ne estrae gli elementi e li pusha in ordine (elemento 0 in cima? no: il
    /// primo elemento resta come nuovo valore, gli altri N-1 vengono pushati sopra).
    /// Se valore NON è un Array (array 1D normale), è l'elemento di iterazione singolo e
    /// il destructuring non si applica: l'opcode lascia il valore invariato e il compiler
    /// usa il flusso (indice, valore) standard. Semantica:
    ///   FOR EACH a, b IN m(2,2)  →  a = riga[0], b = riga[1]
    ///   FOR EACH v IN arr1d      →  v = elemento (invariato)
    /// </summary>
    OP_FE_DESTRUCT = 130,

    /// <summary>
    /// Loop di accumulazione NATIVO (C): esegue il corpo del FOR interamente in C, evitando
    /// il dispatch interpretato per iterazione. Emesso dal compilatore quando il corpo di un
    /// FOR è un accumulatore puro su un termine che dipende dal contatore.
    /// <para>Formato (11 byte):
    ///   [OP_NATIVE_LOOP_ACC] [acc_slot u8] [ctr_slot u8] [acc_op u8] [term_mode u8]
    ///   [f_code u16] [f_code2 u16] [m_idx u16]</para>
    /// <para>acc_op: 0=+=, 1=-=, 2=*=, 3=/=.</para>
    /// <para>term_mode: 0=f(I)*m, 1=f(I), 2=I*I, 3=I*m, 4=I, 5=f(I)+g(I). f_code/f_code2 =
    /// builtin (mode 0/1/5), m_idx = costante moltiplicatore (mode 0/3).</para>
    /// <para>Il range [start, end, step] è già nei for_counter/for_end/for_step del task
    /// (impostati da OP_FOR_PREPARE); il loop usa ctr_slot come contatore e scrive acc_slot.</para>
    /// </summary>
    OP_NATIVE_LOOP_ACC = 131,

    /// <summary>
    /// Assegnazione builtin NATIVA: `x = f(y)` con f builtin matematica pura — UN opcode che
    /// carica y, chiama f e salva x in C nativo (niente 3 dispatch separati LOAD/CALL/STORE).
    /// <para>Formato: [OP_NATIVE_BUILTIN_ASSIGN] [src_slot u8] [dst_slot u8] [f_code u16]
    /// - 5 byte.</para>
    /// </summary>
    OP_NATIVE_BUILTIN_ASSIGN = 132,

    /// <summary>
    /// Termina la goroutine corrente attraversando prima tutti i cleanup/FINALLY attivi.
    /// OP_HALT resta invece il terminatore fisico del chunk e non avvia un unwind.
    /// <para>Formato: [OP_GO_EXIT] - 1 byte.</para>
    /// </summary>
    OP_GO_EXIT = 133,

    /// <summary>
    /// Fusione nativa del self-append stringa in loop: `s$ = s$ + term` dentro un FOR a
    /// trip-count noto ('&amp;' NON è un operatore binario in GMBASIC — è l'operatore
    /// macro/EVAL unario — quindi la concatenazione è solo '+'). L'intero loop viene
    /// eseguito nativamente su un buffer con capacità
    /// di scorta NON visibile al BASIC; il risultato finale è scritto nello slot solo alla
    /// fine (mai stack/refcount per iterazione — O(1) ammortizzato invece di O(n²)).
    /// <para>Formato (8 byte):
    ///   [OP_NATIVE_STRING_APPEND_LOOP] [acc_slot u8] [ctr_slot u8] [term_mode u8]
    ///   [f_code u16] [const_idx u16]</para>
    /// <para>term_mode: 0=letterale stringa (const_idx nel pool costanti),
    /// 1=builtin pura num→str sul contatore (f_code: 31=STR, 8=CSTR).</para>
    /// <para>Il range [start, end, step] è già nei for_counter/for_end/for_step del task
    /// (impostati da OP_FOR_PREPARE); il loop usa ctr_slot come contatore e scrive acc_slot.</para>
    /// </summary>
    OP_NATIVE_STRING_APPEND_LOOP = 134,

    /// <summary>
    /// Chiude un canale (parità Go close(ch)). Il canale è sulla stack. Dopo la chiusura:
    /// SEND su chiuso produce errore; RECEIVE su chiuso+buffer vuoto restituisce subito
    /// (valore null, flag ok=0); i receiver in attesa vengono svegliati con ok=0.
    /// <para>Formato: [OP_CHANNEL_CLOSE] - 1 byte</para>
    /// </summary>
    OP_CHANNEL_CLOSE = 135,

    /// <summary>
    /// Crea un nuovo MUTEX (parità Go sync.Mutex) e lo pusha sullo stack.
    /// <para>Formato: [OP_NEW_MUTEX] - 1 byte</para>
    /// </summary>
    OP_NEW_MUTEX = 137,
    /// <summary>OP_NEW_RWMUTEX - 1 byte: pusha un nuovo RWMUTEX non detenuto (parità Go
    /// sync.RWMutex).</summary>
    OP_NEW_RWMUTEX = 142,
    /// <summary>OP_RLOCK - 1 byte: acquisisce il lock in LETTURA di un RWMUTEX (parità Go
    /// mu.RLock()); parcheggia se un scrittore detiene il lock.</summary>
    OP_RLOCK = 143,
    /// <summary>OP_RUNLOCK - 1 byte: rilascia il lock in LETTURA di un RWMUTEX (parità Go
    /// mu.RUnlock()).</summary>
    OP_RUNLOCK = 144,
    /// <summary>OP_NEW_ONCE - 1 byte: pusha un nuovo ONCE non eseguito (parità Go sync.Once).</summary>
    OP_NEW_ONCE = 145,
    /// <summary>OP_ONCE_BEGIN [skip u16] - 3 byte: inizia un blocco ONCE RUN. Se il Once è
    /// NotRun → lo marca Running e prosegue (esegue la procedura); se Done → salta allo skip;
    /// se Running (già in esecuzione) → parcheggia il task (ri-esegue al wake, ora Done → skip).</summary>
    OP_ONCE_BEGIN = 146,
    /// <summary>OP_ONCE_END - 1 byte: completa il Once (segna Done) e sveglia i waiter.</summary>
    OP_ONCE_END = 147,
    /// <summary>OP_NEW_ATOMIC - 1 byte: pusha un nuovo box ATOMIC a 0 (parità Go sync/atomic).</summary>
    OP_NEW_ATOMIC = 148,
    /// <summary>OP_NEW_POOL - 1 byte: pusha un nuovo POOL vuoto (parità Go sync.Pool).</summary>
    OP_NEW_POOL = 149,
    /// <summary>OP_NEW_COND - 1 byte: pusha una nuova variabile di condizione (parità Go sync.Cond).</summary>
    OP_NEW_COND = 150,
    /// <summary>OP_COND_WAIT - 1 byte: popa (cond, mutex), rilascia il mutex e parcheggia (parità Go c.Wait()).</summary>
    OP_COND_WAIT = 151,
    /// <summary>OP_COND_SIGNAL - 1 byte: popa cond e sveglia un waiter (parità Go c.Signal()).</summary>
    OP_COND_SIGNAL = 152,
    /// <summary>OP_COND_BROADCAST - 1 byte: popa cond e sveglia tutti i waiter (parità Go c.Broadcast()).</summary>
    OP_COND_BROADCAST = 153,
    /// <summary>OP_NEW_CONCURRENTMAP - 1 byte: pusha una mappa concorrente vuota (parità Go sync.Map).</summary>
    OP_NEW_CONCURRENTMAP = 154,

    /// <summary>
    /// Acquisisce il mutex sullo stack (parità Go mu.Lock()): se già detenuto, parcheggia
    /// il task finché non viene rilasciato.
    /// <para>Formato: [OP_LOCK] - 1 byte</para>
    /// </summary>
    OP_LOCK = 138,

    /// <summary>
    /// Rilascia il mutex sullo stack (parità Go mu.Unlock()) e sveglia al più un waiter.
    /// <para>Formato: [OP_UNLOCK] - 1 byte</para>
    /// </summary>
    OP_UNLOCK = 139,

    /// <summary>
    /// Registra il body DEFER della procedura corrente (parità Go defer): l'operando è
    /// l'indirizzo del body, eseguito a fine procedura in ordine LIFO (RETURN, GOSUB-RETURN,
    /// unwind, eccezione). Il body termina con OP_DEFER_RET.
    /// <para>Formato: [OP_DEFER] [addr u16]</para>
    /// </summary>
    OP_DEFER = 140,

    /// <summary>
    /// Termina il body DEFER: torna all'uscita della procedura che ha registrato il defer.
    /// <para>Formato: [OP_DEFER_RET] - 1 byte</para>
    /// </summary>
    OP_DEFER_RET = 141,

    /// <summary>
    /// Carica per NOME un valore dal "dynamic scope" del processore corrente — meccanismo
    /// per identificatori riservati che NON sono dichiarabili con DIM (keyword del linguaggio,
    /// es. FORM/QUERY nelle pagine BSP) e quindi non hanno mai uno slot globale reale: il
    /// Linker li marca con SymbolId = SymbolTable.DynamicScopeSymbolId invece di assegnare un
    /// indice, e l'host (es. TryServeBspPage) semina il valore per nome PRIMA di eseguire il
    /// chunk (vedi PCodeProcessor.SetDynamicScope). Se il nome non è mai stato seminato,
    /// pusha NIL (nessun errore: stesso comportamento "non presente" del backend IL,
    /// IlRuntime.DynamicScopeGet). Parità con IlRuntime.DynamicScopeGet/Set (IL backend).
    /// <para>Formato: [OP_LOAD_DYNAMIC_SCOPE] [const_index nome (ushort)] - 3 byte</para>
    /// </summary>
    OP_LOAD_DYNAMIC_SCOPE = 155,

    /// <summary>
    /// Variante "by-reference" di OP_LOAD_ELEMENT, usata SOLO quando un elemento di array è la
    /// STRUTTURA intermedia di un target di assegnazione a campo (`Arr(i).Campo = valore` —
    /// vedi AssignField, Compiler.UserTypes.cs). Sui backend dove un'istanza CLASS/UDT
    /// (dizionario chiave/valore) ha già identità di riferimento (PCodeVM C#, IL: un
    /// Dictionary&lt;string,object?&gt; NON viene mai copiato da un semplice indicizzatore di
    /// array), il comportamento è identico a OP_LOAD_ELEMENT — separata comunque per dare al
    /// backend VM in C (native/gmb_vm_c) un punto distinto dove SALTARE la copia superficiale
    /// che il suo adattatore valore→stack applica di norma a un GMB_OBJ_DICT/GMB_OBJ_LIST letto
    /// da un array (semantica "UDT è un tipo valore" per un normale RVALUE, es. `DIM x =
    /// Arr(i)`) — quella copia è corretta per un RVALUE ma rompeva la persistenza di
    /// `Arr(i).Campo = valore` (scriveva sulla copia, mai sull'elemento realmente in lista).
    /// Bug scoperto 2026-09-02, vedi Classi-Distruttore-Ereditarieta-CrossFile-Bugs.
    /// <para>Formato: [OP_LOAD_ELEMENT_REF] [num_indices (byte)] - 2 bytes</para>
    /// </summary>
    /// Nota numerica: 156 è occupato SOLO nell'enum interno del backend VM in C
    /// (native/gmb_vm_c/gmb_vm_core.c, OP_FE_NEXT) — un opcode mai emesso dal compilatore
    /// C#, usato internamente da quella VM per una propria riscrittura del bytecode
    /// (FOR EACH). Le due enumerazioni condividono i valori SOLO per gli opcode che il
    /// compilatore C# emette davvero; 157 evita la collisione con quel valore interno.
    OP_LOAD_ELEMENT_REF = 157,

    /// <summary>
    /// Costruisce un ByRefBox su un ELEMENTO DI ARRAY (mono-dimensionale) invece che su un
    /// local/global — usato per passare `Arr(i)` come argomento a un parametro BYREF (es.
    /// `ModificaPersona(Elenco(1), ...)`). Prima di questo opcode, il binding BYREF di un
    /// argomento (VisitFunctionExpression/VisitArrayAccessExpression, Compiler.Expressions.cs)
    /// riconosceva SOLO `argExpr is VariableExpr` — un `ArrayAccessExpr` (Elenco(1)) cadeva
    /// sempre nel ramo BYVAL (VisitExpression semplice, via OP_LOAD_ELEMENT): il parametro
    /// riceveva un valore, non un riferimento scrivibile, quindi una modifica dentro la
    /// procedura non si rifletteva mai sull'array del chiamante dopo il ritorno. Bug scoperto
    /// 2026-09-02 verificando OP_LOAD_ELEMENT_REF, riproducibile identicamente su PCodeVM e
    /// VMC (bug del compilatore condiviso, non specifico di un backend).
    /// Formato: [array...][indice] sullo stack (stesso ordine di OP_LOAD_ELEMENT con
    /// numIndices=1) — SOLO 1 indice supportato (array multi-dimensionali non gestiti da
    /// questo opcode, restano BYVAL come prima).
    /// <para>Formato: [OP_MAKE_BYREF_ELEMENT] - 1 byte</para>
    /// </summary>
    OP_MAKE_BYREF_ELEMENT = 158,

    /// <summary>Allegа il descrittore UDT del tipo TARGET a un'istanza già sullo stack
    /// (un dict prodotto da JSON.TOOBJECT o da un altro valore dict-like): il descrittore
    /// è quello della DICHIARAZIONE (i suoi FieldNames/FieldOffsets usano gli symbol id del
    /// sito d'uso, che possono differire da quelli del descrittore serializzato nel JSON).
    /// Formato: [val] → [OP_ATTACH_UDT_DESC][typeIdx u16] → [val con __class__=descriptor].
    /// Usato dal compilatore per JSON.TOOBJECT con target UDT (VM/C e PCodeVM risolvono i
    /// campi per offset→nome via __class__).</summary>
    OP_ATTACH_UDT_DESC = 210,

    /// <summary>Chiamata FFI tipizzata (DECLARE ... LIB con firma LONG omogenea):
    /// marshalling int64 nativo invece del contratto all-double di OP_CALL_FFI.
    /// Formato: [lib_const u16][name_const u16][arg_count u8][kind u8][array_mask u8]
    /// [array_elem_int_mask u8][string_mask u8][ret_is_string u8] — kind:
    /// 0 = double (tutto), 1 = int64 (parametri e ritorno, unico valore emesso oggi). Il
    /// compilatore emette questa variante SOLO quando tutti i parametri e il ritorno sono
    /// interi (LONG/INTEGER/...) o array (che viaggiano come puntatore, classe di registri
    /// intera) — firma numerica mista long/double = errore di compilazione esplicito (il cast
    /// double↔long per argomento non è esprimibile con un solo function-pointer C). Stringhe →
    /// resta OP_CALL_FFI (comportamento invariato). `array_mask` (2026-09-28): bit i settato se
    /// il parametro logico i è un array BYREF — il runtime (PCodeVM/VM-C) lo usa per pinnare/
    /// marshallare quello slot come puntatore invece di leggerlo come scalare int64 puro.
    /// `array_elem_int_mask` (2026-09-28, fix parità): bit i settato se l'array al parametro i
    /// ha elementi interi (AS LONG/...) invece di DOUBLE — PCodeVM (object[] boxati)/VM-C
    /// (gmb_val_t taggati) non preservano il tipo elemento per-valore a runtime in modo
    /// affidabile, quindi il tipo dichiarato a compile-time deve viaggiare nel bytecode.
    /// `string_mask` (2026-09-28, supporto STRING): bit i settato se il parametro logico i è
    /// STRING — viaggia come puntatore (const char*), stessa classe di registri degli array.
    /// `ret_is_string` (0/1): il ritorno è STRING — il valore int64 della chiamata nativa è un
    /// puntatore da copiare (copy-out) in una nuova stringa BASIC, non un numero.
    /// `scalar_kind_mask u8` (2026-10, firma mista LONG/DOUBLE fra scalari semplici, SUBITO
    /// dopo `kind`): bit i = il parametro SCALARE SEMPLICE alla posizione i è Int64 (0=Double)
    /// — significativo SOLO per le posizioni non-array/STRING/BYREF/callback/UDT (altrove
    /// ignorato, quelle posizioni restano classificate dalle rispettive maschere). Ammesso solo
    /// quando non c'è nessun array/STRING/BYREF/callback/UDT nella firma (quelli continuano a
    /// imporre un kind unico su TUTTA la firma, incluso `kind`/ritorno — comportamento
    /// invariato). Nel caso omogeneo legacy ogni bit coincide con `kind`, zero cambio di
    /// comportamento. Vedi Compiler.Expressions.cs, FfiService.cs, gmb_vm_core.c.</summary>
    OP_CALL_FFI_T = 211,

    /// <summary>XOR bitwise: pop b, pop a, push (double)((long)a ^ (long)b).</summary>
    OP_XOR = 212,
    /// <summary>AND bitwise (simbolo &&): pop b, pop a, push (double)((long)a & (long)b).</summary>
    OP_BITAND = 220,
    /// <summary>OR bitwise (simbolo ||): push (double)((long)a | (long)b).</summary>
    OP_BITOR = 221,

    // === SCOPE DINAMICO (DYNAMIC) — variabili a visibilità per CATENA DI CHIAMATA =========
    // La catena dei frame di chiamata È la catena dello scope dinamico: ogni frame porta le
    // proprie dichiarazioni DYNAMIC; la lettura risale la catena, la scrittura colpisce il
    // frame più interno che definisce il nome. Alla pop del frame le dichiarazioni spariscono
    // (ripristino automatico — spec Variables-Scope §2.5). Nessun push/pop esplicito: la
    // struttura è quella dei frame. Vedi PCodeProcessor (OP_DYN_*) e gmb_vm_core.c.

    /// <summary>Scope dinamico: dichiara il NOME indicato (operando U16 = indice di costante
    /// del nome) nel frame corrente, col valore in cima allo stack (consumato).</summary>
    OP_DYN_DECLARE = 213,

    /// <summary>Scope dinamico: carica il NOME (U16) cercando nella catena dei frame
    /// (top-down) e ne pusha il valore. Operando U8 = tipo per il DEFAULT quando assente:
    /// 1 = STRING (""), 0 = numerico (0.0). Parità con gmb_dscope_get_string/get_double.</summary>
    OP_DYN_LOAD = 214,

    /// <summary>Scope dinamico: pop del valore e scrittura del NOME (U16) nel frame più
    /// interno che lo definisce, altrimenti nel frame corrente.</summary>
    OP_DYN_STORE = 215,

}
