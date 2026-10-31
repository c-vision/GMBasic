' FILE: Strings/StringsRelationalCompare.bas
' ============================================================================
' CONFRONTO RELAZIONALE TRA STRINGHE — ordinamento, filtro e ricerca su rubrica
' ============================================================================
' Esercita TUTTI gli operatori di confronto tra stringhe (`<`, `>`, `<=`, `>=`,
' `=`, `<>`) su letterali, variabili, concatenazioni e risultati di funzione,
' usati in IF, assegnazioni e PRINT, dentro e fuori dai cicli.
'
' Semantica (identica sui 4 backend IL/PCode/VM-C/C99/C99Native):
' - due stringhe si confrontano per ORDINALE lessicografico byte-a-byte
'   (come strcmp): case-SENSITIVE ("Gamma" < "gamma"), i prefissi contano
'   ("abc" < "abcd"), i numeri NON sono speciali ("10" < "9").
' - Il risultato segue la convenzione booleana BASIC: -1 (TRUE), 0 (FALSE).
' MiniTest: minitests/constructs/expr_operators_string_relational.bas
' Guida: Guide/utente/Stringhe-Guida-Utente.md (sezione "Confronto relazionale").
' ============================================================================

SUB Main()
  DIM names(7) AS STRING
  names(0) = "delta"
  names(1) = "Alpha"
  names(2) = "charlie"
  names(3) = "bravo"
  names(4) = "Echo"
  names(5) = "alpha"
  names(6) = "Delta"
  names(7) = "foxtrot"

  ' 1. Ordinamento alfabetico (bubble sort): scambia se fuori ordine (a > b).
  DIM i AS INTEGER
  DIM j AS INTEGER
  DIM tmp AS STRING
  FOR i = 0 TO 6
    FOR j = i + 1 TO 7
      IF names(i) > names(j) THEN
        tmp = names(i)
        names(i) = names(j)
        names(j) = tmp
      END IF
    NEXT
  NEXT

  PRINT "ordinati:"
  FOR i = 0 TO 7
    PRINT names(i)
  NEXT

  ' 2. Verifica che la sequenza sia non-decrescente (ogni coppia <= successiva).
  DIM ok AS INTEGER
  ok = -1
  FOR i = 0 TO 6
    IF (names(i) <= names(i + 1)) <> -1 THEN ok = 0
  NEXT
  IF ok THEN PRINT "sorted-ok"

  ' 3. Minimo e massimo con < e > (scansione).
  DIM mn AS STRING
  DIM mx AS STRING
  mn = names(0)
  mx = names(0)
  FOR i = 1 TO 7
    IF names(i) < mn THEN mn = names(i)
    IF names(i) > mx THEN mx = names(i)
  NEXT
  PRINT "min="; mn
  PRINT "max="; mx

  ' 4. Filtro per iniziale in [b, e): nome >= "b" E nome < "e".
  PRINT "filtro-b-e:"
  DIM n AS INTEGER
  n = 0
  FOR i = 0 TO 7
    IF names(i) >= "b" THEN
      IF names(i) < "e" THEN
        PRINT names(i)
        n = n + 1
      END IF
    END IF
  NEXT
  PRINT "trovati="; n

  ' 5. Uguaglianza esatta (=) e diversità (<>): conta e cerca.
  DIM eq AS INTEGER
  eq = ("alpha" = names(1))
  PRINT "eq="; eq
  DIM diff AS INTEGER
  diff = 0
  FOR i = 0 TO 7
    IF names(i) <> "alpha" THEN diff = diff + 1
  NEXT
  PRINT "diversi-da-alpha="; diff

  ' 6. Confronti su concatenazioni e risultati di funzione.
  PRINT (("al" + "pha") = "alpha")
  PRINT (("al" + "pha") < "beta")
  PRINT ((UPPER("abc")) = "ABC")
  PRINT ((UPPER("abc")) > "ABB")
  PRINT ((LEFT$("abcdef", 3)) < "abd")
  PRINT (("Gamma" < "gamma") AND ("Gamma" <> "gamma"))

  ' 7. Casi limite: stringa vuota e prefissi.
  DIM vuota AS STRING
  vuota = ""
  PRINT (vuota < "a")
  PRINT (vuota = "")
  PRINT (vuota <= vuota)
  PRINT ("abc" < "abcd")
  PRINT ("abcd" > "abc")
  PRINT ("10" < "9")
END SUB
