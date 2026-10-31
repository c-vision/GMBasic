' FILE: Functions/FunctionsParams.bas
' ======================================================================================================================
'       GUIDA TEORICA: PARAMETRI AVANZATI (PARAMARRAY E ARGOMENTI NOMINALI)
' ======================================================================================================================
'
' Questo test copre le nuove funzionalità avanzate per la definizione e l'invocazione 
' di procedure (SUB e FUNCTION) in GM-BASIC.
'
' 1. PARAMETRI VARIADICI (PARAMARRAY) (TODO #80)
'    Permette a una procedura di accettare un numero variabile di argomenti dello stesso tipo.
'    - Regola 1: Deve essere l'ultimo parametro della firma.
'    - Regola 2: Deve essere dichiarato con le parentesi vuote (es. `PARAMARRAY nomi()`).
'    - Regola 3: All'interno della funzione, il parametro viene visto come un normale ARRAY.
'
' 2. ARGOMENTI NOMINALI (NAMED ARGUMENTS) (TODO #81)
'    Permette di invocare una procedura specificando il nome del parametro tramite ':=', 
'    indipendentemente dalla sua posizione nella firma.
'    - Vincolo: Una volta usato un argomento nominale, tutti quelli successivi devono essere nominali.
'
' ======================================================================================================================

IMPORT COLLECTIONS
IMPORT JSON

10 CLS
20 PRINT "╔══════════════════════════════════════════════════════════════════════════════════════════════════════╗"
30 PRINT "║ FunctionsParams - Parametri Variadici (PARAMARRAY) e Nominali (:=)                                   ║"
40 PRINT "╚══════════════════════════════════════════════════════════════════════════════════════════════════════╝"
50 PRINT

' --- Dispatcher dei Test ---
100 GOSUB TestParamArray
110 GOSUB TestNamedArguments
120 GOSUB TestCombinedUsage

190 PRINT "--- Test parametri completati ---"
200 END


' ----------------------------------------------------------------------------------------------------------------------
' 1. PARAMARRAY: Numero variabile di argomenti
' ----------------------------------------------------------------------------------------------------------------------
1000 TestParamArray:
1010 PRINT "--- 1. Testing PARAMARRAY ---"
1020 PRINT "  - Somma(vuota): "; SommaTutto()
1030 PRINT "  - Somma(10.5, 20.0, 30.5): "; SommaTutto(10.5, 20.0, 30.5)
1040 DIM numeri#() = {1.1, 2.2, 3.3}
1050 PRINT "  - Somma(array pre-esistente): "; SommaTutto(numeri#())
1060 PRINT
1200 RETURN

PUBLIC FUNCTION SommaTutto(PARAMARRAY valori() AS DOUBLE) AS DOUBLE
    DIM totale# = 0.0
    FOR EACH n# IN valori()
        totale# = totale# + n#
    NEXT
    RETURN totale#
END FUNCTION


' ----------------------------------------------------------------------------------------------------------------------
' 2. ARGOMENTI NOMINALI: Invocazione tramite nome parametro
' ----------------------------------------------------------------------------------------------------------------------
2000 TestNamedArguments:
2010 PRINT "--- 2. Testing Named Arguments (:=) ---"
2020 PRINT "  - Chiamata posizionale standard:" : VisualizzaInfo(1, "Mario", "Admin")
2030 PRINT "  - Ordine invertito:"
2040 VisualizzaInfo(Nome := "Luigi", ID := 2, Ruolo := "Editor")
2050 PRINT "  - Salto parametro opzionale:"
2060 VisualizzaInfo(ID := 3, Nome := "Peach")
2070 PRINT
2100 RETURN

PUBLIC SUB VisualizzaInfo(ID AS LONG, Nome AS STRING, OPTIONAL Ruolo AS STRING = "Guest")
    PRINT "    [USER] ID:"; ID; " | Nome: "; Nome; " | Ruolo: "; Ruolo
END SUB


' ----------------------------------------------------------------------------------------------------------------------
' 3. USO COMBINATO: Parametri fissi + ParamArray
' ----------------------------------------------------------------------------------------------------------------------
3000 TestCombinedUsage:
3010 PRINT "--- 3. Testing Combined Syntax ---"
3020 PRINT "  - Messaggio Log con tag multipli (Fissi nominali + Variadici posizionali):"
3030 InviaLog(Livello := "INFO", Msg := "Sistema avviato", "Main", "Network", "Security")
3040 PRINT
3200 RETURN

PUBLIC SUB InviaLog(Msg AS STRING, Livello AS STRING, PARAMARRAY Tags() AS STRING)
    PRINT "    LOG ["; Livello; "]: "; Msg
    IF UBOUND(Tags()) >= 0 THEN
        PRINT "    TAGS: "; JSON.Stringify(Tags())
    END IF
END SUB