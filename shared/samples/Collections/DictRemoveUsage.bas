' FILE: Collections/DictRemoveUsage.bas
' ======================================================================================================================
' ESEMPIO COMPLETO: `DICT.Remove(k)` come PREDICATO in un flusso reale (4 gambe).
' ======================================================================================================================
' `Remove(k)` compie SEMPRE l'effetto (toglie la chiave se c'è) e restituisce un valore:
'   -1  = la chiave esisteva ed è stata rimossa
'    0  = la chiave non c'era
' Per questo si può usare direttamente in IF / WHILE / AND / OR senza un ContainsKey prima.
'
' Scenario: un magazzino con scorte e una lista di ordini. Si evadono gli ordini finché c'è
' merce, si annullano quelli non coperti e si riporta lo stato finale. L'ordine degli ordini
' è esplicito (array), così l'output è deterministico: l'ordine di un DICT non è garantito.
'
' Output atteso (identico su IL, PCode, VM-C, C99Native):
'   spedito ordine A (mela)
'   spedito ordine B (mela)
'   spedito ordine C (pera)
'   annullato ordine D (pera)
'   rimasti in magazzino: mela= 0  pera= 0
'   ordini aperti:  0 
'   ripulito:  1
IMPORT COLLECTIONS

SUB Main()
  ' --- scorte: articolo -> pezzi disponibili -------------------------------------------
  DIM scorte AS DICT OF STRING, INTEGER
  scorte("mela") = 2
  scorte("pera") = 1

  ' --- ordini: id -> articolo richiesto -------------------------------------------------
  DIM ordini AS DICT OF STRING, STRING
  ordini("A") = "mela"
  ordini("B") = "mela"
  ordini("C") = "pera"
  ordini("D") = "pera"

  ' ordine di lavorazione ESPLICITO (non l'ordine interno del DICT)
  DIM ids(3) AS STRING
  ids(0) = "A" : ids(1) = "B" : ids(2) = "C" : ids(3) = "D"

  DIM k AS INTEGER
  FOR k = 0 TO 3
    DIM id AS STRING
    id = ids(k)
    ' l'ordine può essere già stato rimosso in un giro precedente
    IF ordini.ContainsKey(id) THEN
      DIM articolo AS STRING
      articolo = ordini(id)
      IF scorte(articolo) > 0 THEN
        scorte(articolo) = scorte(articolo) - 1
        ' (1) IF: `Remove` toglie l'ordine e dice se c'era davvero (qui: VERO)
        IF ordini.Remove(id) THEN
          PRINT "spedito ordine "; id; " ("; articolo; ")"
        END IF
      ELSE
        IF ordini.Remove(id) THEN
          PRINT "annullato ordine "; id; " ("; articolo; ")"
        END IF
      END IF
    END IF
  NEXT k

  ' (2) stato finale delle scorte (le chiavi con 0 restano: rimuoverle è un'altra scelta)
  PRINT "rimasti in magazzino: mela="; scorte("mela"); " pera="; scorte("pera")

  ' (3) `Remove` su una chiave assente: NON è un errore, restituisce 0 (ramo THEN mai preso)
  DIM aperti AS INTEGER
  aperti = 0
  IF ordini.Remove("Z") THEN aperti = aperti + 1
  PRINT "ordini aperti: "; aperti

  ' (4) ripulire una scorta esaurita: la prima Remove è vera, la seconda è 0 (non un errore)
  DIM ripulite AS INTEGER
  ripulite = 0
  IF scorte.Remove("mela") THEN ripulite = ripulite + 1
  IF scorte.Remove("mela") THEN ripulite = ripulite + 1
  PRINT "ripulito: "; ripulite
END SUB
