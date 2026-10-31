' EXPECT: RUN_WORKER_OK
' RUN "script" — verifica chain-load via shell.
' L'output del FIGLIO non passa dal console del runtime (eredita lo stdout del processo): con
' stdout rediretto il runner gestito (IL/PCode in-process, Console.SetOut) non lo vede, mentre
' la gamba VM/C (processo esterno) sì → divergenza di parità che NON è un difetto di RUN.
' Qui si verifica l'esecuzione del figlio con un effetto osservabile indipendente dalla cattura:
' il worker scrive su file e il test stampa il contenuto.
IMPORT SYSTEM
IMPORT IO

SUB Main()
  DIM p AS STRING, s AS STRING
  SET SHELL ON
  p = "/tmp/gmb_mt_run_worker.txt"
  IF FILEEXISTS(p) THEN KILL p
  RUN "sh run_worker.sh > " + p + " 2>&1"
  s = ""
  IF FILEEXISTS(p) THEN s = TRIM$(IO.READFILE(p))
  PRINT s
END SUB
