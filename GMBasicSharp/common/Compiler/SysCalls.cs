// FILE: Interpreter.Compiler.SysCall.cs

// ReSharper disable InconsistentNaming

namespace Compiler;

/// <summary>
/// Definisce i codici per le chiamate di sistema (SYSCALL)
/// invocate dall'OpCode OP_SYSCALL. Questa lista contiene
/// comandi di I/O, filesystem e grafica che sono troppo specifici
/// per avere un proprio opcode dedicato.
/// </summary>
public enum SysCall : ushort
{
    // --- Console I/O & Grafica a Caratteri (15) ---
    CLS,
    LOCATE,
    PRINT,
    COLOR,
    INPUT,
    LINE_INPUT,
    INKEY,
    GET_KEY,
    WAITKEY,
    CPRINT,
    CLOCATE,
    CCLS,
    SET_ATTRIBUTE,
    CURSOR_STATE,
    CURSOR_SHAPE,

    // --- File I/O (NON-ISAM) (8) ---
    CLOSE_FILE,
    PRINT_FILE,
    INPUT_FILE,
    LINE_INPUT_FILE,
    WRITE_FILE,
    GET_FILE,
    PUT_FILE,
    SEEK_FILE,

    // --- Filesystem (9) ---
    KILL_FILE,
    RENAME_FILE,
    MKDIR,
    RMDIR,
    CHDIR,
    COPY_FILE,
    MOVE_FILE,
    XCOPY_DIR,

    // --- HTTP Internals (AGGIUNTE) ---

    // --- ISAM (21 membri - AGGIORNATA) ---
    // --- RDD: Replaceable Database Drivers ---
    RDD_SET_DEFAULT,
    RDD_USE,
    RDD_RLOCK,
    RDD_FLOCK,
    RDD_UNLOCK,
    RDD_SETLOCKTIMEOUT,
    RDD_APPENDFROM,
    RDD_GET_FIELD,
    RDD_SET_FIELD,
    RDD_ONEVENT,
    RDD_OFFEVENT,
    RDD_TODAY,
    RDD_CTOD,
    RDD_DTOC,
    RDD_CURVAL,
    RDD_BUFFER,
    RDD_TABLEUPDATE,
    RDD_TABLEREVERT,
    RDD_OLDVAL,
    RDD_GETFLDSTATE,
    RDD_SETREPROCESS,
    RDD_SETORDER,
    RDD_CREATEINDEX,
    RDD_CLOSEINDEX,
    RDD_COPYTO,
    RDD_SORTTO,
    RDD_TOTALON,
    RDD_SETDELETED,
    RDD_VALTYPE,
    RDD_FLOAT,
    RDD_EVAL,
    RDD_DELETE_SCOPED,
    RDD_RECALL_SCOPED,
    RDD_REPLACE_SCOPED,
    // --- ISAM (20) ---
    ISAM_USE, // AGGIUNTA
    ISAM_SELECT, // AGGIUNTA
    ISAM_RECORD,
    ISAM_SKIP,
    ISAM_SEEK, // AGGIUNTA
    ISAM_DELETE,
    ISAM_RECALL,
    ISAM_PACK,
    ISAM_REINDEX,
    ISAM_REPAIR,
    ISAM_BACKUP,
    ISAM_APPEND,
    ISAM_LOCK,
    ISAM_UNLOCK,
    ISAM_SET_ORDER,
    ISAM_SET_RELATION, // AGGIUNTA
    ISAM_CREATE_INDEX,
    ISAM_DROP_INDEX,
    ISAM_VALIDATE, // AGGIUNTA
    ISAM_COPY_TO, // AGGIUNTA
    ISAM_JOIN_WITH, // AGGIUNTA
    ISAM_SORT_TO, // AGGIUNTA
    ISAM_SET_FILTER, // AGGIUNTA
    ISAM_INTEGRITY,  // AGGIUNTA
    ISAM_INFO,       // AGGIUNTA
    ISAM_EOF, // AGGIUNTA
    ISAM_BOF, // AGGIUNTA
    TEAM_CREATE, TEAM_ADD_AGENT, TEAM_REMOVE_AGENT, TEAM_MESSAGE, TEAM_RUN, TEAM_SET_ROLE, TEAM_SET_STRATEGY, TEAM_SET_COORDINATOR, TEAM_SAVE, TEAM_LOAD,
    WORKFLOW_DEF, WORKFLOW_STEP, WORKFLOW_END, WORKFLOW_EXECUTE,
    ISAM_PUT, // AGGIUNTA
    // Allineati ai macro C (gmb_db.h): GMB_SYS_ISAM_SET_REPLICATION=107, APPLY=108, FOLLOW=109.
    // Con i valori impliciti (106/107/108) la VM instradava 106 su ISAM_PUT e 107/108 sul trio
    // sbagliato → ISAM.SETREPLICATION ritornava una stringa vuota
    // (§ MiniTests/dbf/isam_replication.bas: VMC "setrepl: no").
    ISAM_SET_REPLICATION = 107,
    ISAM_APPLY_REPLICATION = 108,
    ISAM_FOLLOW_REPLICATION = 109,
    ISAM_APPENDMANY,         // AGGIUNTA
    BEGIN_TRANSACTION,
    COMMIT,
    ROLLBACK,
    EXPORT_TO_FILE,
    IMPORT_FROM_FILE,
    CONVERT_DBF_TO_ISAM,

    // --- DBF (33 membri - AGGIORNATA) ---
    DBF_USE, // AGGIUNTA
    DBF_SELECT, // AGGIUNTA
    DBF_GET_RECORD, // AGGIUNTA
    DBF_REPLACE, // AGGIUNTA
    DBF_DELETE, // AGGIUNTA
    DBF_RECALL, // AGGIUNTA
    DBF_SORT, // AGGIUNTA
    DBF_UPDATE, // AGGIUNTA
    DBF_TOTAL_ON, // AGGIUNTA
    DBF_PACK, // AGGIUNTA
    DBF_ZAP, // AGGIUNTA
    DBF_GO, // AGGIUNTA
    DBF_SKIP, // AGGIUNTA
    DBF_SEEK, // AGGIUNTA
    DBF_CLOSE_DATABASES, // AGGIUNTA
    DBF_SET_DELETED, // AGGIUNTA
    DBF_SET_FILTER, // AGGIUNTA
    DBF_COPY_TO, // AGGIUNTA
    DBF_REINDEX, // AGGIUNTA
    DBF_APPEND_RECORD, // AGGIUNTA
    DBF_APPEND_FROM, // AGGIUNTA
    DBF_JOIN_WITH, // AGGIUNTA
    DBF_COPY_STRUCTURE, // AGGIUNTA
    DBF_SET_ORDER, // AGGIUNTA
    DBF_SET_RELATION, // AGGIUNTA
    DBF_INDEX_ON, // AGGIUNTA
    DBF_SET_EXACT, // AGGIUNTA
    DBF_SET_DEFAULT, // AGGIUNTA
    DBF_SET_PATH, // AGGIUNTA
    DBF_SET_EXCLUSIVE, // AGGIUNTA
    DBF_SET_SCHEMA, // AGGIUNTA
    DBF_APPEND_BLANK, // AGGIUNTA
    DBF_VALIDATE, // AGGIUNTA
    DBF_CONVERT_TO_ISAM, // AGGIUNTA (per CONVERT DBF TO ISAM)
    DBF_RECNO, // AGGIUNTA
    DBF_LASTREC, // AGGIUNTA
    DBF_CREATE_TABLE, // AGGIUNTA
    DBF_APPENDMANY, // AGGIUNTA
    DBF_SET_DURABILITY, // AGGIUNTA
    DBF_BEGIN_GROUP_COMMIT, // AGGIUNTA
    DBF_END_GROUP_COMMIT, // AGGIUNTA
    DBF_CLOSE_CURRENT, // AGGIUNTA - chiude solo l'area DBF corrente
    DBF_SET_REPLICATION, // DBF.SETREPLICATION
    DBF_APPLY_REPLICATION, // DBF.APPLYREPLICATION
    DBF_FOLLOW_REPLICATION, // DBF.FOLLOWREPLICATION
    DBF_LUPDATE, // DBF.LUPDATE() — data ultimo aggiornamento dall'header DBF

    // --- SPEED (17) ---
    SPEED_OPEN,
    SPEED_USE,
    SPEED_CREATE_INDEX,
    SPEED_DROP_INDEX,
    SPEED_GET,
    SPEED_PUT,
    SPEED_DELETE_RECORD,
    SPEED_SEEK,
    SPEED_GO,
    SPEED_SKIP,
    SPEED_SET_ORDER,
    SPEED_SET_FILTER,
    SPEED_SET_RELATION,
    SPEED_SELECT,
    SPEED_CREATE_VIEW,
    SPEED_REFRESH_VIEW,
    SPEED_DROP_VIEW,
    SPEED_SETMANY,

    // --- Ledger (12) ---
    LEDGER_OPEN,
    LEDGER_ADD,
    LEDGER_REGISTER,
    LEDGER_BALANCE,
    LEDGER_GET,
    LEDGER_EDIT,
    LEDGER_DELETE,
    LEDGER_SAVE,
    LEDGER_CLOSE,
    LEDGER_PACK,
    LEDGER_ACCOUNTS,
    LEDGER_VALIDATE,
    LEDGER_BUDGET,
    LEDGER_VARIANCE,
    LEDGER_PLOT,
    LEDGER_IMPORT,

    // --- SQL (12) ---
    SQL_OPEN,
    SQL_CLOSE,
    SQL_EXEC,
    SQL_QUERY,
    SQL_FETCH,
    SQL_CLOSE_RESULT_SET,
    SQL_BEGIN_TRANSACTION,
    SQL_COMMIT,
    SQL_ROLLBACK,
    SQL_SAVEPOINT,
    SQL_RELEASE_SAVEPOINT,
    SQL_ROLLBACK_TO_SAVEPOINT,
    SQL_ATTACH,
    SQL_DETACH,
    SQL_BACKUP,

    // --- Error Handling (1) ---
    THROW_ERROR,

    // --- Grafica a Pixel (20) ---
    SCREEN,
    PSET,
    PRESET,
    POINT,
    LINE,
    CIRCLE,
    PAINT,
    DRAW,
    PCOPY,
    SCREENSET,
    PIXEL_BUFFER,
    FONT,
    IMAGEGET,
    IMAGEPUT,
    IMAGEPUTS,
    POLYGON,
    PALETTE,
    SAVE_SCREEN,
    RESTORE_SCREEN,
    CSAVE_SCREEN,
    CRESTORE_SCREEN,
    VIEWPORT,
    WINDOW,
    BSAVE,
    BLOAD,
    CLEAR_RECT,
    DRAW_TEXTURE,
    LOAD_TEXTURE,
    UNLOAD_RESOURCE,
    DRAW_TEXTURES,
    SYNC,

    // --- Grafica Interna (Generate da PRINT) (AGGIUNTE) ---
    GFX_TAB_ZONE, // AGGIUNTA
    GFX_NEWLINE, // AGGIUNTA
    GFX_TAB, // AGGIUNTA

    // --- Dati & Tipi (5) ---
    SWAP,
    RANDOMIZE,
    JSON_SAVE,
    JSON_WRITE_VALUE,
    JSON_WRITE_SECTION,
    
    RUN,
    STOP,
    
    KEY_GEN,
    VECTORDB_ENSURE_COLLECTION, // AIVECTOR.ENSURECOLLECTION
    VECTORDB_INDEX_TEXT,         // AIVECTOR.INDEXTEXT
    VECTORDB_INDEX_META,         // AIVECTOR.INDEXMETA
    VECTORDB_SEARCH,             // AIVECTOR.SEARCH
    VECTORDB_COUNT,              // AIVECTOR.COUNT
    VECTORDB_LIST_COLLECTIONS,   // AIVECTOR.LISTCOLLECTIONS
    VECTORDB_REMOVE_SOURCE,      // AIVECTOR.REMOVESOURCE
    VECTORDB_LOAD_DOCUMENTS,     // AIVECTOR.LOADDOCUMENTS
    VECTORDB_PURGE_DEPRECATED,   // AIVECTOR.PURGEDEPRECATED
    VECTORDB_GENTEST,            // AIVECTOR.GENTEST
    
    IO_COPY,                     // IO.COPY
    IO_MKDIR,                    // IO.MKDIR
    IO_RMDIR,                    // IO.RMDIR
    IO_CHDIR,                    // IO.CHDIR
    IO_KILL,                     // IO.KILL

    KEY_CLOSE,

    ARRAY_SLICE,
    JSON_TO_OBJECT,

    // --- Clipper TUI ---
    AT_SAY,       // @ row, col SAY expr [PICTURE pic]
    AT_GET,       // @ row, col GET variable [options]
    CLIPPER_READ, // READ (loop interattivo)
    BOX_DRAW,      // BOX statement
    SCROLL_REGION, // SCROLL statement
    // --- Clipper TUI Controls ---
    TUI_CHECKBOX,    // CHECKBOX var, row, col, "caption"
    TUI_LISTBOX,     // LISTBOX var, row, col, w, h, items()
    TUI_COMBOBOX,    // COMBOBOX var, row, col, w, items()
    TUI_MENU,        // MENU var, row, col, items()
    TUI_STATUSBAR,   // STATUSBAR "text"
    TUI_FILEDIALOG,  // FILEDIALOG var, "mask", "title"
    TUI_PRINTER,     // PRINTERDIALOG var, copies, duplex, "title"
    TUI_TOGGLE,      // TOGGLE var, row, col, "caption"
    TUI_RADIOGROUP,  // RADIOGROUP var, row, col, items()
    TUI_SLIDER,      // SLIDER var, r, c, len, min, max, step
    TUI_TAB,         // TAB var, r, c, w, h, tabs()
    TUI_GRID,        // GRID var, r, c, w, h, headers(), data()
    TUI_DATEPICKER,  // DATEPICKER var, r, c
    TUI_COLORPICKER, // COLORPICKER var, r, c, cols
    TUI_TREEVIEW,    // TREEVIEW var, r, c, w, h, tree()
    TUI_SPINNER,     // SPINNER r, c, "preset"
    TUI_TBROWSE,     // TBROWSE var, r, c, w, h, cols(), data() [, opts()]
    TUI_LAST_HANDLE, // () -> handle TUI dell'ultimo controllo creato (ID)
    CONTROL_APPLY_PROPS, // name,tag,tooltip,tabindex,visible,enabled,tabstop -> applica clausole all'ultimo controllo
    SET_CLIPPER,     // SET BELL/CONFIRM/CURSOR/CENTURY/THEME/DATEFORMAT
    // --- Sprite / Tick / Sync ---
    SPRITE_DEF,
    SPRITE_CMD,
    TILEMAP_CMD,
    TICK_PROCESS,
    SYNC_PROCESS,
    ON_WINDOW_EVENT,
    WINDOW_ACTION,
    WINDOW_DEFINE,
    WINDOW_APPEND,      // (name, text) → IlRuntime/VM: aggiunge una riga al buffer finestra
    WINDOW_SET_CONTENT, // (name, lines[]) → imposta il buffer finestra
    WINDOW_CLEAR,       // (name) → svuota il buffer
    WINDOW_SCROLL,      // (name, delta) → scrolla
    WINDOW_EDIT,        // (name, on) → editable
    WINDOW_SELECT,      // (target) → finestra attiva (nome o 0=schermo base)
    WINDOW_THEME,       // (themeName) → imposta tema globale finestre
    WINDOW_DESKTOP,     // (desktopNum) → passa a desktop virtuale
    WINDOW_MOVE_DESKTOP,// (name, desktopNum) → sposta finestra su desktop virtuale
    WINDOW_FRAME,       // (name?, style) → imposta stile cornice (Double/Single/Rounded)
    WINDOW_SHADOW,      // (name?, on) → imposta ombreggiatura 3D
    VIEW_GFX,

    // --- MEMORY MAPPED FILES (IO.MEMORYMAP/MMAPPEEK/MMAPPOKE/MMAPREADBYTES/MMAPWRITEBYTES) ---
    MEMORYMAP_OPEN,       // (path) → handle LONG
    MEMORYMAP_PEEK,       // (handle, offset) → BYTE
    MEMORYMAP_POKE,       // (handle, offset, value)
    MEMORYMAP_READBYTES,  // (handle, offset, count) → BYTE()
    MEMORYMAP_WRITEBYTES, // (handle, offset, byteArray[, count])

    // --- CONSOLE (Caption/SendKey/ScrollMode) ---
    CONSOLE_CAPTION,    // (title) → ConsoleService.Title
    CONSOLE_SENDKEY,    // (keys) → KeyboardService.SendKey
    CONSOLE_SCROLLMODE, // (top, bottom) → ConsoleService.SetScrollRegion
    SOUND_LOAD,         // (filePath, handle) → AudioManager.Load
    SOUND_PLAY_FILE,    // (handle, channel, loops) → AudioManager.PlayAsync
    SOUND_VOLUME,       // (volume, channel) → AudioManager.SetVolume

    // --- MOUSE (config — SDL reale via GraphicsManager) ---
    MOUSE_ENABLE,   // (enabled) → SDL_ShowCursor
    MOUSE_SHOW,     // (visible) → SDL_ShowCursor
    MOUSE_SETPOS,   // (x, y) → SDL_WarpMouseInWindow
    MOUSE_CURSOR,   // (cursorId) → SDL_SetCursor
    MOUSE_LIMIT,    // (x1, y1, x2, y2) → clamp per MouseSetPos
    MOUSE_WAIT,     // (nessun arg) → sync point

    // --- CHART (plotting headless — OxyPlot via GraphicsManager) ---
    CHART_CREATE,     // (title, xLabel?, yLabel?) → handle LONG
    CHART_PLOT,       // (handle, x[], y[], plotType, legend?)
    CHART_FINANCIAL,  // (handle, ohlc[], plotType, legend?)
    CHART_CLEAR,      // (handle)
    CHART_CLOSE,      // (handle)
    CHART_RENDER,     // (isSave, handle, ...)

    // --- PALETTE (console — IConsoleService) ---
    PALETTE_SET,      // (index, r, g, b) → Console.SetPaletteColor
    PALETTE_USE,      // (name) → Console.SetActivePalette
    PALETTE_DEFINE,   // (name, colors) → Console.SetActivePalette (define+use)
    PALETTE_REPLACE,  // (name, colors) → Console.SetActivePalette

    // --- FILE LOCK/UNLOCK ---
    FILE_LOCK,        // (fileNumber, recordNumber?)
    FILE_UNLOCK,      // (fileNumber, recordNumber?)

    // --- EVENTI generici (RAISE/ADD/REMOVE HANDLER) ---
    RAISE_EVENT,      // (eventName, args...)
    ADD_HANDLER,      // (eventName, handlerName)
    REMOVE_HANDLER,   // (eventName, handlerName)
    IO_WATCH,         // (path, eventsMask) → FileSystemWatcher
    IO_UNWATCH,       // (path)
    WINDOW_VIEWPORT,  // (x1, y1, x2, y2, screen) → GraphicsManager.SetWindow/ResetViewport
    PALETTE_SAVE,     // (fileName) → salva la palette attiva in JSON
    PALETTE_LOAD,     // (fileName) → carica la palette da JSON
    DRAW_TEXT,        // (text, x, y, rotation?, align, color?) → testo grafico
    VIEW_PRINT,       // (top, bottom) → ConsoleService.SetScrollRegion (0,0 = reset)
    ISAM_RECNO,       // (alias?) → numero record corrente, mancava un dispatch dedicato: ISAM.RECNO
                      // cadeva nel BuiltInFunction.RECNO generico condiviso con SQL (semantica
                      // "area valida?" 0/1, non il vero numero di record)

    // --- TORCH (comandi legacy, nessun valore di ritorno) ---
    TORCH_SET_DEVICE,   // (deviceName) → imposta il device attivo (auto/cpu/cuda/rocm/mlx)
    TORCH_LOAD_LIB,     // () → carica la lib ML (idempotente)
    TORCH_UNLOAD_LIB,   // () → scarica la lib ML
    TORCH_SET_LIB_PATH, // (dir) → imposta la directory delle librerie
    TORCH_INFO,         // () → stampa lo stato (device attivo, lib path, loaded) su console

    // --- DBF: report/query/listing (Cat3 — implementati nel motore C) ---
    DBF_LIST,           // LIST [fields] [TO FILE/PRINT]
    DBF_CREATE_VIEW,    // CREATE VIEW <nome> ...
    DBF_AGGREGATE,      // AGGREGATE <key> TO <file> FIELDS <agg>...
    DBF_REPORT,         // REPORT FORM <file> ...

    // --- TEAM RecursiveMAS R1/R4 (syscall, in coda per non rinumerare TEAM/WORKFLOW 92-105) ---
    TEAM_SET_RECURSION, // AI TEAM nome SET RECURSION n
    TEAM_SET_LATENT,    // AI TEAM nome SET LATENT mode
    TEAM_SET_LINK,      // AI TEAM nome SET LINK which cfg
    TEAM_SET_THOUGHTS,  // AI TEAM nome SET THOUGHTS n
    TEAM_TRAIN_INNER,   // AI TEAM TRAIN INNER team, agent, dataset (R4)
    TEAM_TRAIN_OUTER,   // AI TEAM TRAIN OUTER team, dataset (R4)

    // --- TEAM estensioni §9 (9.3-9.6) ---
    TEAM_SET_DAG,           // AI TEAM nome SET DAG d,l,n (AGoT 9.4)
    TEAM_SET_COMPLEXITY,    // AI TEAM nome SET COMPLEXITY threshold (AGoT 9.4)
    TEAM_SET_REFLECTION,    // AI TEAM nome SET REFLECTION mode (MAR/MIRROR 9.5)
    TEAM_SET_CRITIC,        // AI TEAM nome SET CRITIC personas, judge (MAR 9.5)
    TEAM_SET_PRUNE,         // AI TEAM nome SET PRUNE topK (ReTreVal 9.5)
    TEAM_TRAIN_THINKING,    // AI TEAM TRAIN THINKING nome, dataset (Quiet-STaR 9.3)
    TEAM_TRAIN_ACTOR_CRITIC, // AI TEAM TRAIN ACTOR CRITIC nome, dataset (DPSDP 9.6)
    TEAM_DISTILL_THINKING,  // AI TEAM DISTILL THINKING nome, dataset (Quiet-STaR 9.3)
    TEAM_BROADCAST,         // AI TEAM name BROADCAST message (L1)
    TEAM_REDEFINE,          // AI TEAM name REDEFINE definition (L1)

    // --- SPEED CLOSE (append-only: in coda per NON rinumerare i codici esistenti) ---
    SPEED_CLOSE,            // SPEED CLOSE [alias | DATABASES] → GMB_SYS_SPEED_CLOSE=380

    // --- CONSOLE mode (TEXTMODE n) — in coda per NON rinumerare (append-only) ---
    TEXTMODE,               // TEXTMODE n — modalita' console (0=%SDL,1=%C64,2=%ANSI,3=%SYSTEM)

    // --- Controlli pixel + form (append-only: 400..409, NON rinumerare i precedenti) ---
    SET_MESSAGE = 481,     // (text$) -> MESSAGE dell'ultimo campo @GET
    SET_ERROR = 482,       // (text$) -> ERROR dell'ultimo campo @GET
    PIXEL_CTRL = 400,      // (kind$, name$, spec$, args[]) -> crea controllo pixel
    FORM_FRAME = 401,      // (ms)
    FORM_CLOSED = 402,     // () -> double (1 chiuso)
    FORM_CLOSE = 403,      // ()
    FORM_CTRL_GET = 404,   // (name$, prop$) -> double
    FORM_CTRL_GETS = 405,  // (name$, prop$) -> string
    FORM_CTRL_SET = 406,   // (name$, prop$, num, str$)
    FORM_CTRL_MOVE = 407,  // (name$, x, y, w, h)
    FORM_CTRL_FOCUS = 408, // (name$)
    FORM_CTRL_ZORDER = 409,// (name$, front)
    FORM_LAST_HANDLE = 410,// () -> handle dell'ultimo controllo creato (ID h)
    FORM_CLICKED = 411,    // (handle) -> 1 se l'ultimo evento di quel controllo e' CLICK
    FORM_DBLCLICKED = 412, // (handle)
    FORM_CHANGED = 413,    // (handle)
    FORM_GOTFOCUSED = 414, // (handle)
    FORM_LOSTFOCUSED = 415,// (handle)
    MOUSE_CLICKED = 416,   // () -> 1 se l'ultimo evento mouse globale e' CLICK
    MOUSE_DBLCLICKED = 417,// ()
    MOUSE_PRESSED = 418,   // () -> 1 se l'ultimo evento mouse globale e' MOUSEDOWN
    KEY_PRESSED = 419,     // () -> 1 se un KEYPRESS globale e' avvenuto (sticky)
    FORM_SET_EVENT_ID = 420,// (handle, event$, sub$) -> registra/rimuove (sub$="" = STOP)
    FORM_FIRE = 421,       // (handle, event$) -> innalza un evento controllo (sincrono)
    MESSAGE_OP,      // MESSAGE <verbo>: (subop, a, b, c)
    ON_MESSAGE,      // ON MESSAGE <sub> CALL <handler>: (sub, handler)
    TUI_NUMERICUPDOWN = 424, // NUMERICUPDOWN var, r, c, len, min, max, step
    TUI_STARRATING = 425,    // STARRATING var, r, c, len, max, value
    TUI_SCROLLBAR = 426,     // SCROLLBAR var, r, c, len, min, max, value
    TUI_SCROLLVIEW = 427,    // SCROLLVIEW var, r, c, w, h, content()
    TUI_LABEL = 428,         // LABEL r, c, w, "text" [INTO var]
    TUI_DATALIST = 429,      // DATALIST var, r, c, w, h, items()
    TUI_PROPERTYGRID = 430,  // PROPERTYGRID var, r, c, w, h, items()
    TUI_LOGVIEW = 431,       // LOGVIEW var, r, c, w, h, records()
    TUI_HEXVIEW = 432,       // HEXVIEW var, r, c, w, h, data$
    TUI_BIGTEXT = 433,       // BIGTEXT r, c, "text" [INTO var]
    TUI_CODEAREA = 434,      // CODEAREA var, r, c, w, h, lines()
    TUI_REORDERLISTBOX = 435, // REORDERLISTBOX var, r, c, w, h, items()
    TUI_ALERTBOX = 436,      // ALERTBOX r, c, w, h, "text" [INTO var]
    TUI_CONFIRMATIONBOX = 437, // CONFIRMATIONBOX var, r, c, w, h, "text"
    TUI_FILEEXPLORER = 438,  // FILEEXPLORER var, r, c, w, h, "path"
    TUI_DASHBOARD = 439,     // DASHBOARD var, r, c, w, h, metrics()
    TUI_SPLITTER = 451,      // SPLITTER var, r, c, w, h, orient
    TUI_CONTEXTMENU = 452,   // CONTEXTMENU var, r, c, w, h, items()
    ISAM_CLOSE = 440,        // ISAM CLOSE [alias]: chiude l'area ISAM (arg: alias stringa o nil = area corrente). Valore esplicito: allineato a GMB_SYS_ISAM_CLOSE (gmb_db.h).
    FORM_CTRL_ADD = 441,     // CONTROL ADD instance, x, y, w, h -> registra un UserControl (classe con metodo Paint)
    FORM_CTRL_REMOVE = 442,  // CONTROL REMOVE instance -> deregistra/Unload il UserControl
    FORM_CTRL_DRAW = 443,    // CONTROL DRAW handle, op, a, b, c, d, str$ -> primitiva del GraphicsContext
    CHART_SET = 444,         // CHART SET handle, "prop", num, "str" -> configurazione grafico
    CHART_GET = 445,         // CHART GET handle, "prop" -> double (INTO var)
    CHART_GETS = 446,        // CHART GETS handle, "prop" -> string (INTO var$)
    CHART_DRAW = 447,        // CHART DRAW handle, "op", args[] -> id (INTO var)
    FORM_CTRL_INVALIDATE = 448, // CONTROL INVALIDATE name -> marca da ridisegnare (§11.7)
    FORM_CTRL_REFRESH = 449,    // CONTROL REFRESH name -> repaint (§11.7)
    FORM_BIND_UC_EVENT = 450,   // (§11.7-3) handle, evIndex, method$, instance -> evento UserControl → metodo-istanza (task GPM)
    PROGRAM_EXIT = 453,         // (n) -> EXIT CODE del processo (FUNCTION Main return); consuma n e pusha 0
    SIGNAL_SUBSCRIBE = 454,     // SIGNAL SUBSCRIBE: (id, types, asset, market, symbols, timeframe) — gmb_signal.h
    ON_SIGNAL = 455,            // ON SIGNAL id CALL handler: (id, handler) — arità 4/5 risolta a runtime
    SIGNAL_EMIT = 456,          // SIGNAL EMIT: (type, symbol, value, asset, market, timeframe)
    SIGNAL_UNSUBSCRIBE = 457,   // SIGNAL UNSUBSCRIBE: (id) — "" = tutte
    SIGNAL_PING = 458,          // SIGNAL PING: nessun argomento
    DBF_CLOSE_AREA = 459,       // DBF CLOSE <alias|area>: chiude SOLO l'area indicata (1 arg)
}

/// <summary>
/// Definisce i codici per i tipi di contenuto speciali gestiti dal runtime.
/// </summary>
public enum ContentTypeCode : byte
{
    Default = 0, // File di testo o binario standard
    Json = 1,
    // In futuro potremmo aggiungere: Xml = 2, Csv = 3, etc.
}