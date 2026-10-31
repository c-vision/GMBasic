' Utf8ReportLayout.bas — report a larghezza fissa e BUDGET DI BYTE su testo UTF-8.
'
' Esempio realistico che usa tutte le primitive di lunghezza/sottostringa del linguaggio:
'   LEN(s)    = CARATTERI (code point)  → allineamento visivo delle colonne
'   LENB(s)   = BYTE della codifica UTF-8 → limite di buffer / Content-Length
'   LEFT$/RIGHT$/MID$ = sottostringhe per CARATTERE (mai spezzano un carattere)
'   INSTR     = posizione in CARATTERI (per separare "chiave=valore")
'   DUP       = riempimento (padding) con un carattere
'
' Scena: un listino con nomi accentati, un box-drawing e un'emoji. Le colonne devono
' restare allineate (caratteri) e ogni record deve entrare in un payload di massimo
' 64 BYTE (byte), accorciando il nome se serve — senza mai spezzare un carattere.
'
' Esecuzione: `tools/sh/parity4` — output identico su IL, PCodeVM, VM/C, C99Native.
IMPORT STRINGS

DIM n AS LONG
DIM nomi(4) AS STRING
DIM codici(4) AS STRING
DIM prezzi(4) AS DOUBLE
DIM i AS LONG
DIM riga AS STRING
DIM etichetta AS STRING
DIM resto AS LONG
DIM budget AS LONG
DIM q AS STRING          ' virgolette doppie (CHR$(34)): tre virgolette di fila sarebbero una raw string


q = CHR$(34)
n = 4
nomi(0) = "Caffè" : codici(0) = "CAF-01" : prezzi(0) = 2.5
nomi(1) = "Tè verde" : codici(1) = "THE-02" : prezzi(1) = 3.1
nomi(2) = "Tavolo ─ rovere" : codici(2) = "TAV-03" : prezzi(2) = 149.9
nomi(3) = "Cuscino 😀" : codici(3) = "CUS-04" : prezzi(3) = 12

budget = 64
PRINT "CODICE | NOME (14 car.)  |  EUR   | LEN | LENB | PAYLOAD"
PRINT DUP(64, "-")

FOR i = 0 TO n - 1
  ' Allineamento per CARATTERI: tronca/tappa il nome alla colonna da 14.
  etichetta = LEFT$(nomi(i), 14)
  IF LEN(etichetta) < 14 THEN
    etichetta = etichetta + DUP(14 - LEN(etichetta), " ")
  END IF

  ' Payload in stile JSON (senza escape: nomi semplici) con budget in BYTE.
  riga = "{" + q + "code" + q + ":" + q + codici(i) + q + "," + q + "name" + q + ":" + q + nomi(i) + q + "}"
  resto = 0
  DO WHILE LENB(riga) > budget AND LEN(nomi(i)) - resto > 1
    resto = resto + 1
    riga = "{" + q + "code" + q + ":" + q + codici(i) + q + "," + q + "name" + q + ":" + q + LEFT$(nomi(i), LEN(nomi(i)) - resto) + q + "}"
  LOOP

  PRINT codici(i); " | "; etichetta; " | "; prezzi(i); " | "; LEN(nomi(i)); " | "; LENB(nomi(i)); " | "; LENB(riga); " b"
  PRINT "   dentro il budget: "; riga
NEXT i

PRINT DUP(64, "-")

' Separazione "chiave=valore" con INSTR: la posizione è in CARATTERI, non in byte.
DIM campo AS STRING
campo = "nome=Tavolo ─ rovere;prezzo=149.9"
DIM pos AS LONG
pos = INSTR(campo, "=")
PRINT "chiave   = "; LEFT$(campo, pos - 1)
PRINT "valore   = "; MID$(campo, pos + 1, INSTR(campo, ";") - pos - 1)
PRINT "coda     = "; MID$(campo, INSTR(campo, ";") + 1, LEN(campo))

' Tratto finale: 3 caratteri da destra di una stringa con emoji (nessun byte rotto).
PRINT "ultimi 3 caratteri = ["; RIGHT$("Cuscino 😀", 3); "]"
PRINT "byte di 3 spazi    = "; LENB(DUP(3, " "))
