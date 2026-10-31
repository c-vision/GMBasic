' FILE: Types/TypesNumericLiterals.bas
' ======================================================================================================================
' GUIDA TEORICA: LETTERALI NUMERICI IN BASI DIVERSE
' ======================================================================================================================
' Oltre ai numeri decimali standard, GM-BASIC supporta la rappresentazione di numeri interi in diverse basi,
' una funzionalità comune nei linguaggi di programmazione moderni per la manipolazione di dati a basso livello.
'
' 1. SINTASSI DEI PREFISSI
'    - Esadecimale (Base 16): `0x` o `0X` seguito da cifre `0-9` e lettere `A-F`.
'      Esempio: `0xFF` (equivale a 255 in decimale).
'    - Ottale (Base 8): `0o` o `0O` seguito da cifre `0-7`.
'      Esempio: `0o10` (equivale a 8 in decimale).
'    - Binario (Base 2): `0b` o `0B` seguito da cifre `0` e `1`.
'      Esempio: `0b1010` (equivale a 10 in decimale).
'
' 2. UTILIZZO
'    - I letterali numerici in basi diverse possono essere usati ovunque sia atteso un valore numerico:
'      in assegnazioni, espressioni aritmetiche, come argomenti di funzioni, e persino come
'      destinazioni per comandi come `GOTO`.
'
' 3. COMPORTAMENTO DEL COMPILATORE
'    - Il Lexer (analizzatore lessicale) riconosce questi prefissi e converte immediatamente il valore
'      nel suo equivalente numerico (trattato internamente come `DOUBLE`), prima ancora che il Parser
'      costruisca l'AST.
'    - Di conseguenza, per il resto del compilatore e per l'Executor, `0x10`, `16`, e `0b10000` sono
'      indistinguibili: sono tutti lo stesso valore numerico.
'
' 4. NUMERI DI RIGA
'    - Anche i numeri di riga possono essere specificati usando queste notazioni. Ad esempio, la riga
'      `100` può essere scritta come `0x64`. Il `GOTO 0x64` salterà correttamente alla riga `100`.
' ======================================================================================================================

  CLS
  PRINT "╔═════════════════════════════════════════════════════════════════════════╗"
  PRINT "║ TypesNumericLiterals - Letterali Numerici                               ║"
  PRINT "╚═════════════════════════════════════════════════════════════════════════╝"
  PRINT
  PRINT "--- Test Letterali Numerici ---"
  
  DIM H AS LONG, O AS LONG, B AS LONG
  
  ' Assegnazione di letterali in diverse basi.
  H = 0x10      ' Esadecimale (16)
  O = 0o20      ' Ottale (16)
  B = 0b10000   ' Binario (16)
  
  PRINT "Valore Esadecimale 0x10 ="; H; "(Atteso: 16)"
  PRINT "Valore Ottale 0o20 ="; O; "(Atteso: 16)"
  PRINT "Valore Binario 0b10000 ="; B; "(Atteso: 16)"
  PRINT
  
  PRINT "Test con valori grandi:"
  DIM GRANDE AS QUAD
  ' Il tipo QUAD (64-bit) è necessario per contenere valori così grandi.
  GRANDE = 0xFFFFFFFF ' 4294967295
  PRINT "0xFFFFFFFF ="; GRANDE
  PRINT
  
  PRINT "Test in espressioni:"
  PRINT "0x0A + 0x02 ="; 0x0A + 0x02; "(Atteso: 12)"
  PRINT
  
  ' ' VERIFICA DI COMPLETEZZA
  ' Aggiunto test per verificare la sensibilità al maiuscolo/minuscolo nei prefissi e nelle cifre esadecimali.
  PRINT "--- Test Case-Insensitivity ---"
  PRINT "0Xff (maiuscolo) ="; 0Xff
  PRINT "0OfF (misto) ="; 0xff
  PRINT
  
  PRINT "--- Test GOTO verso Numero di Riga ---"
  ' Salta alla riga 100.
  GOTO 100

REM --- CODICE NUMERATO GLOBALE ---

' La riga 100 può essere referenziata anche con il suo equivalente esadecimale.
100 PRINT "OK: Saltato correttamente alla linea 100 (0x64)." : GOTO 150

120 PRINT "ERRORE: Riga 120 non doveva essere eseguita."

150 PRINT "--- Test completato ---"
160 END