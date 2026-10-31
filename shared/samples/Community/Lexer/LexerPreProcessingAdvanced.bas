' FILE: Lexer/LexerPreProcessingAdvanced.bas
' ======================================================================================================================
' GUIDA TEORICA: PRE-PROCESSORE AVANZATO — COMPILAZIONE CONDIZIONALE CON ESPRESSIONI
' ======================================================================================================================
' Oltre ai semplici controlli #IFDEF/#IFUNDEF, GM-BASIC supporta la compilazione
' condizionale basata su espressioni costanti valutate a compile-time tramite #IF.
'
' 1. DIRETTIVA #IF expr THEN
'    Valuta un'espressione costante all'interno di #IF. Se il risultato è diverso
'    da zero (true), il blocco seguente viene incluso. Altrimenti viene saltato.
'    - Supporta operatori aritmetici: +, -, *, /, \, MOD, ^
'    - Supporta operatori di confronto: =, <>, <, >, <=, >=
'    - Supporta operatori logici: AND, OR, NOT
'    - Supporta operatori bitwise: <<, >>
'    - Può usare simboli #DEF come variabili con valore numerico
'
' 2. DIRETTIVA #ELSEIF expr THEN
'    Permette catene di condizioni multiple senza annidamento.
'
' 3. DIRETTIVA #ELSE
'    Blocco di fallback quando nessuna condizione precedente è vera.
'
' 4. DIRETTIVA #END IF
'    Termina un blocco #IF (nota: spazio tra END e IF).
' ======================================================================================================================

' --- PARTE 1: TEST COMPILAZIONE CONDIZIONALE AVANZATA ---

' Definisce simboli con valori numerici
#DEF VERSION_MAJOR 2
#DEF VERSION_MINOR 5
#DEF PLATFORM_WINDOWS 0
#DEF PLATFORM_MACOS 1
#DEF PLATFORM_LINUX 0
#DEF TARGET_PLATFORM 1
#DEF DEBUG_LEVEL 3

' 1a. #IF con espressione aritmetica semplice
#if VERSION_MAJOR >= 2 then
    DIM greeting$ = "Benvenuto in GM-BASIC v2.x"
#else
    DIM greeting$ = "Benvenuto in GM-BASIC legacy"
#endif

' 1b. #ELSEIF con espressioni multiple
#if TARGET_PLATFORM = 0 then
    DIM os_name$ = "Windows"
#elseif TARGET_PLATFORM = 1 then
    DIM os_name$ = "macOS"
#elseif TARGET_PLATFORM = 2 then
    DIM os_name$ = "Linux"
#else
    DIM os_name$ = "Sconosciuto"
#endif

' 1c. #IF con espressioni logiche (AND, OR, NOT)
#if (VERSION_MAJOR >= 2) AND (DEBUG_LEVEL > 0) then
    DIM debug_mode = 1
#else
    DIM debug_mode = 0
#endif

' 1d. #IF annidato
#if VERSION_MAJOR >= 2 then
    #if VERSION_MINOR >= 5 then
        DIM feature_level$ = "Completo"
    #else
        DIM feature_level$ = "Parziale"
    #endif
#else
    DIM feature_level$ = "Minimo"
#endif

' --- PARTE 2: ESECUZIONE TEST ---
CLS
PRINT "╔══════════════════════════════════════════════════════════════════════╗"
PRINT "║ LexerPreProcessingAdvanced - Compilazione Condizionale Avanzata      ║"
PRINT "╚══════════════════════════════════════════════════════════════════════╝"
PRINT
PRINT "Saluto: "; greeting$
PRINT "Sistema operativo: "; os_name$
PRINT "Debug mode: "; debug_mode
PRINT "Feature level: "; feature_level$
PRINT
PRINT "Test completato."

END
