' FILE: ClipperSyntax.bas
' =================================================================================================
'       GUIDA TEORICA: SINTASSI ESTESA IN STILE CLIPPER
' =================================================================================================
'
' Questo file di test dimostra e valida l'integrazione di diverse funzionalità sintattiche
' tipiche del linguaggio Clipper 5.3 all'interno di GM-BASIC.
'
' 1. UNIFICAZIONE DEI NOMI DI VARIABILE CON SUFFISSO
'    - Problema Storico: `DIM X AS STRING` e `DIM X$` venivano trattate come due variabili distinte.
'    - Soluzione: Il compilatore ora normalizza i nomi. `X$` viene riconosciuto come la
'      variabile `X` di tipo `STRING`. Il suffisso agisce come una dichiarazione di tipo implicita
'      che è semanticamente equivalente alla clausola `AS`.
'    - Esempio: `miaVar$` e `miaVar AS STRING` puntano alla stessa cella di memoria.
'
' 2. LETTERALI E OPERATORI LOGICI IN STILE xBASE
'    - Letterali Booleani: `.T.` per il valore `TRUE` e `.F.` per il valore `FALSE`.
'    - Operatori Logici: `.AND.`, `.OR.`, e `.NOT.` sono alternative a `AND`, `OR`, `NOT`.
'    - Operatore di Negazione Unaria: `!` è un'alternativa a `.NOT.` (es. `IF !condizione`).
'
' 3. IL VALORE `NIL`
'    - `NIL` (e il suo sinonimo `NULL`) è un valore speciale che rappresenta l'assenza di un valore.
'    - A differenza dei dialetti BASIC tradizionali, dove le variabili non inizializzate
'      assumono un valore di default (0 o ""), in GM-BASIC `NIL` deve essere assegnato esplicitamente.
'    - Semantica di Confronto:
'      - `NIL == NIL` restituisce `.T.`.
'      - `NIL == <qualsiasi_altro_valore>` restituisce `.F.`.
'      - Le operazioni aritmetiche o logiche su `NIL` (diverse dal confronto) causano un errore
'        a runtime. In contesti logici (`IF`, `WHILE`), `NIL` viene trattato come `.F.`.
'
' 4. ASSEGNAZIONE INLINE (`:=`) E MULTIPLA
'    - L'operatore `:=` permette di eseguire un'assegnazione all'interno di un'espressione.
'    - L'intera espressione di assegnazione (`variabile := valore`) restituisce il `valore`
'      assegnato, che può essere immediatamente utilizzato in un'operazione successiva.
'    - Esempio Classico: `WHILE (nPos := InStr(testo, "a")) > 0`
'    - L'operatore è associativo a destra, permettendo assegnazioni multiple concatenate:
'      `Z := Y := X := 10` assegna 10 a X, poi il risultato (10) a Y, e infine a Z.
'
' 5. OPERATORE DI CONFRONTO ESATTO (`==`)
'    - L'operatore `==` viene introdotto per fornire un confronto "esatto", specialmente
'      utile per le stringhe.
'    - La sua semantica precisa dipende dall'impostazione `SET EXACT`. Generalmente, `==`
'      esegue un confronto che considera anche gli spazi finali, a differenza di `=` che
'      potrebbe ignorarli. Questo test ne verifica il riconoscimento sintattico.
'
' =================================================================================================

SUB PrintTestHeader(title AS STRING)
    PRINT
    PRINT "---[ " + title + " ]---"
END SUB

SUB TestVariableSuffixUnification()
    PrintTestHeader("Test Unificazione Nomi Variabile (Suffisso vs AS)")

    DIM sharedVar AS STRING

    sharedVar$ = "Hello from suffix"

    PRINT "Valore dopo ri-dichiarazione con AS: " + sharedVar

    IF sharedVar = "Hello from suffix" THEN
        PRINT "RISULTATO: SUCCESSO - La variabile è la stessa."
    ELSE
        PRINT "RISULTATO: FALLITO - Le variabili sono diverse."
    END IF
END SUB

SUB TestNilValue()
    PrintTestHeader("Test Valore NIL")
    
    DIM a, b, c
    a = NIL
    b = NIL
    c = 0
    
    PRINT "Confronto 'a = NIL', 'b = NIL', 'c = 0'"
    
    IF a == b THEN
        PRINT "RISULTATO: SUCCESSO - 'a == b' è vero."
    ELSE
        PRINT "RISULTATO: FALLITO - 'a == b' è falso."
    END IF
    
    IF a == c THEN
        PRINT "RISULTATO: FALLITO - 'a == c' è vero."
    ELSE
        PRINT "RISULTATO: SUCCESSO - 'a == c' è falso."
    END IF

    IF a != c THEN
        PRINT "RISULTATO: SUCCESSO - 'a != c' è vero."
    ELSE
        PRINT "RISULTATO: FALLITO - 'a != c' è falso."
    END IF

    IF a THEN
        PRINT "RISULTATO: FALLITO - 'IF NIL' è stato valutato come vero."
    ELSE
        PRINT "RISULTATO: SUCCESSO - 'IF NIL' è stato valutato come falso."
    END IF
END SUB

SUB TestClipperLogicals()
    PrintTestHeader("Test Operatori e Letterali Logici Clipper")

    DIM T, F
    T = .T.
    F = .F.

    PRINT "Test: .T. AND .F."
    IF T .AND. F THEN
        PRINT "RISULTATO: FALLITO"
    ELSE
        PRINT "RISULTATO: SUCCESSO"
    END IF

    PRINT "Test: .T. OR .F."
    IF T .OR. F THEN
        PRINT "RISULTATO: SUCCESSO"
    ELSE
        PRINT "RISULTATO: FALLITO"
    END IF

    PRINT "Test: .NOT. F"
    IF .NOT. F THEN
        PRINT "RISULTATO: SUCCESSO"
    ELSE
        PRINT "RISULTATO: FALLITO"
    END IF
    
    PRINT "Test: !T (operatore di negazione)"
    IF !T THEN
        PRINT "RISULTATO: FALLITO"
    ELSE
        PRINT "RISULTATO: SUCCESSO"
    END IF
END SUB

SUB TestInlineAssignment()
    PrintTestHeader("Test Assegnazione Inline (:=)")
    
    DIM haystack$, needle$, pos
    haystack$ = "La casa blu sulla collina blu"
    needle$ = "blu"
    
    PRINT "Cerco '" + needle$ + "' in '" + haystack$ + "'"
    
    ' L'assegnazione a 'pos' avviene prima del confronto > 0
    WHILE (pos := InStr(pos + 1, haystack$, needle$)) > 0
        PRINT "Trovato '" + needle$ + "' alla posizione: " + pos
    WEND
    PRINT "Fine ricerca."
    PRINT
    PRINT "Test Assegnazione Multipla (x := y := z := 123)"
    DIM x, y, z
    x := y := z := 123
    PRINT "Valore di x: " + x
    PRINT "Valore di y: " + y
    PRINT "Valore di z: " + z
    IF x = 123 AND y = 123 AND z = 123 THEN
        PRINT "RISULTATO: SUCCESSO"
    ELSE
        PRINT "RISULTATO: FALLITO"
    END IF
END SUB

SUB TestExactlyEqualOperator()
    PrintTestHeader("Test Operatore di Uguaglianza Esatta (==)")
    
    DIM s1$, s2$
    s1$ = "test"
    s2$ = "test  " ' con spazi finali
    
    PRINT "Confronto '" + s1$ + "' con '" + s2$ + "'"
    
    PRINT "Usando '=' (uguaglianza lasca):"
    IF s1$ = s2$ THEN
        PRINT "RISULTATO: Le stringhe sono uguali (comportamento atteso con SET EXACT OFF)."
    ELSE
        PRINT "RISULTATO: Le stringhe sono diverse (comportamento atteso con SET EXACT ON)."
    END IF
    
    PRINT "Usando '==' (uguaglianza esatta):"
    IF s1$ == s2$ THEN
        PRINT "RISULTATO: FALLITO - '==' ha considerato le stringhe uguali."
    ELSE
        PRINT "RISULTATO: SUCCESSO - '==' ha considerato le stringhe diverse."
    END IF
END SUB


SUB Main()
    CLS
    PRINT "╔═════════════════════════════════════════════════════════════════════════════╗"
    PRINT "║ ClipperSyntax.bas - Test delle funzionalità di sintassi estesa              ║"
    PRINT "╚═════════════════════════════════════════════════════════════════════════════╝"
    
    TestVariableSuffixUnification()
    TestNilValue()
    TestClipperLogicals()
    TestInlineAssignment()
    TestExactlyEqualOperator()
    
    PRINT
    PRINT "Tutti i test sono stati eseguiti."
END SUB