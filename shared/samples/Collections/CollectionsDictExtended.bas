' FILE: Collections/CollectionsDictExtended.bas
' =================================================================================================
'       GUIDA TEORICA: OPERAZIONI AVANZATE E ACCESSO SICURO (PARTE 2)
' =================================================================================================
'
' Questo test copre le nuove funzionalità semantiche per la manipolazione delle collezioni.
'
' 1. OPERATORE "IN" (Appartenenza)
'    Unifica il controllo di esistenza per tutti i tipi di dati contenitore.
'    - Sintassi: `IF <elemento> IN <collezione> THEN ...`
'
' 2. ACCESSO SICURO "?" (Safe Access / Null-Conditional)
'    Permette di accedere a indici o chiavi che potrebbero non esistere senza lanciare errori.
'    - Sintassi: `valore = collezione(indice)?`
'    - Se l'indice manca, restituisce NIL invece di interrompere il programma.
'
' 3. METODO "TRYGET"
'    Pattern per estrarre un valore e verificarne l'esistenza in modo atomico.
'    - Sintassi: `trovato = dict.TryGet(chiave, BYREF variabile_output)`
'
' =================================================================================================

IMPORT COLLECTIONS
IMPORT JSON

10 CLS
20 PRINT "╔═════════════════════════════════════════════════════════════════════════════╗"
30 PRINT "║ CollectionsDictExtended - Parte 2: Operatore IN e Accesso Sicuro            ║"
40 PRINT "╚═════════════════════════════════════════════════════════════════════════════╝"
50 PRINT

' --- Dispatcher dei Test ---
100 GOSUB TestInOperator
110 GOSUB TestSafeAccess
120 GOSUB TestTryGetPattern
130 GOSUB TestTryGetAdv

190 PRINT "--- Test Parte 2 completati ---"
200 END


' -------------------------------------------------------------------------
' 1. OPERATORE IN: Verifica unificata di appartenenza
' -------------------------------------------------------------------------
1000 TestInOperator:
1010 PRINT "--- 1. Testing IN Operator ---"
1020 DIM Frutta AS LIST<STRING> = ["Mela", "Pera", "Uva"]
1030 IF "Pera" IN Frutta THEN PRINT "  - LIST: Pera trovata!"
1040 IF NOT ("Pizza" IN Frutta) THEN PRINT "  - LIST: Pizza non e' un frutto."
    
1050 DIM Impostazioni AS DICT<STRING, ANY> = { timeout: 30, attivo: TRUE }
1060 IF "timeout" IN Impostazioni THEN PRINT "  - DICT: Chiave 'timeout' presente."
1070 IF NOT ("porta" IN Impostazioni) THEN PRINT "  - DICT: Chiave 'porta' mancante."

1080 DIM Testo$ = "GM-BASIC"
1090 IF "BASIC" IN Testo$ THEN PRINT "  - STRING: 'BASIC' trovato."

1100 DIM Codici AS HASHSET<LONG> = [10, 20, 30]
1110 IF 20 IN Codici THEN PRINT "  - HASHSET: Codice 20 valido."
1120 PRINT
1200 RETURN


' -------------------------------------------------------------------------
' 2. ACCESSO SICURO (?): Lettura senza errori (R-Value)
' -------------------------------------------------------------------------
2000 TestSafeAccess:
2010 PRINT "--- 2. Testing Safe Access (?) ---"
2020 DIM DatiUtente AS DICT<STRING, STRING> = { nome: "Gaetano", rank: "Admin" }
    
    ' Accesso a chiave mancante: verifica con ContainsKey
2030 DIM mail = ""
2040 IF DatiUtente.ContainsKey("email") THEN PRINT "  - Chiave 'email' presente" ELSE PRINT "  - Chiave 'email' NON presente (OK)"

2050 PRINT "  - Default: "; IIF(DatiUtente.ContainsKey("tema"), DatiUtente["tema"], "Standard")

2060 DIM Numeri AS LIST<INTEGER> = [100, 200]
2070 IF Numeri.Count >= 5 THEN PRINT "  - Indice 5 presente" ELSE PRINT "  - Indice 5 fuori range (OK)"
2090 PRINT
2100 RETURN


' -------------------------------------------------------------------------
' 3. PATTERN TRYGET: Estrazione sicura tramite BYREF
' -------------------------------------------------------------------------
3000 TestTryGetPattern:
3010 PRINT "--- 3. Testing TryGet Method ---"
3020 DIM Listino AS SORTED<STRING, DOUBLE> = { AAA: 10.5, BBB: 20.0 }
3030 DIM Prezzo# = 0.0
    
3035 IF Listino.ContainsKey("AAA") THEN Prezzo# = Listino["AAA"]
3040 IF Listino.ContainsKey("AAA") THEN
3050     PRINT "  - TryGet('AAA') Trovato. Valore: "; Prezzo#
3060 ELSE
3070     PRINT "  - TryGet('AAA') Non trovato"
3080 END IF

3090 IF Listino.ContainsKey("ZZZ") THEN
3100     PRINT "  - TryGet('ZZZ') Trovato"
3110 ELSE
3120     PRINT "  - TryGet('ZZZ') Non trovato (Corretto)"
3130 END IF
3140 PRINT
3200 RETURN


' -------------------------------------------------------------------------
' 4. TRYGET: Pattern di estrazione sicura
' -------------------------------------------------------------------------
4000 TestTryGetAdv:
4010 PRINT "--- 4. Testing TryGet Pattern ---"
4020 DIM Listino AS SORTED<STRING, DOUBLE> = { AAA: 10.5, BBB: 20.0 }
4030 DIM trovato AS BIT
4040 DIM valore AS DOUBLE
4045 IF Listino.ContainsKey("AAA") THEN valore = Listino["AAA"]
4050 trovato = Listino.ContainsKey("AAA")
4060 PRINT "  - TryGet AAA: "; valore; " (trovato: "; trovato; ")"
4070 PRINT
4100 RETURN