' Sample completo — ENV$(nome) / ENV.HAS(nome)
' Lettura ESPLICITA dell'ambiente di processo dal programma BASIC (sola lettura).
' Il motore non usa queste variabili per parametri/comportamento (registro opzioni).
' Eseguibile su IL/PCode/VM-C/C99Native con: tools/sh/parity4 <file>
IMPORT ENV

SUB Main()
    PRINT "=== ENV ==="
    PRINT "PATH presente: "; ENV.HAS("PATH")
    PRINT "PATH lunghezza > 0: "; (LEN(ENV$("PATH")) > 0)
    PRINT "variabile assente: "; ENV.HAS("GMB_ENV_DEMO_ASSENTE")
    PRINT "valore assente: ["; ENV$("GMB_ENV_DEMO_ASSENTE"); "]"
END SUB
