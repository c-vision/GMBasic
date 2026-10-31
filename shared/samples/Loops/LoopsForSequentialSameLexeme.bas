' FILE: Loops/LoopsForSequentialSameLexeme.bas
' =================================================================================================
'       REGRESSIONE: DUE FOR SEQUENZIALI SULLO STESSO LEXEME IN CONTESTO GPM (STEPPER IL)
' =================================================================================================
' Due cicli FOR...NEXT consecutivi che riusano lo stesso identificatore di contatore (I):
'
'     FOR I = 0 TO 4 : S = S + 1  : NEXT     ' atteso: S += 5
'     FOR I = 0 TO 4 : S = S + 10 : NEXT     ' atteso: S += 50
'
' Il linker registra un SymbolId NUOVO per ogni EnterLoopStmt. Se l'init del secondo FOR
' non azzera lo STESSO contatore letto da ForCheck/Next, il secondo corpo non gira
' (oppure il primo non gira) e la somma finale è sbagliata.
'
' La SLEEP 1 in testa forza il may-park → il Main viene eseguito come GPM stepper a chunk
' (il path in cui è apparso il bug: gli init FOR finivano nel chunk di retry, il contatore
' tornava a 0/restava sporco e i loop su stesso lexeme si pestavano).
'
' ESITO ATTESO:
'   Somma = 55 (5 iterazioni da +1 + 5 iterazioni da +10).
' =================================================================================================

10 CLS
20 PRINT "--- Test: due FOR sequenziali sullo stesso lexeme (contesto GPM stepper) ---"
30 PRINT

40 SLEEP 1
50 DIM S AS LONG
60 S = 0

70 FOR I = 0 TO 4
80     S = S + 1
90 NEXT

100 FOR I = 0 TO 4
110     S = S + 10
120 NEXT

130 PRINT "Somma="; S
140 IF S = 55 THEN
150     PRINT "SUCCESSO: entrambi i cicli sono girati (5*1 + 5*10)."
160 ELSE
170     PRINT "FALLITO: somma="; S; " attesa 55"
180 END IF

190 PRINT "--- Test completato ---"
