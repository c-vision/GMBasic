' Sample completo — ENV$(nome) / ENV.HAS(nome)
' Lettura ESPLICITA dell'ambiente di processo dal programma BASIC (sola lettura).
' Il motore non usa queste variabili per parametri/comportamento (registro opzioni).
' Esercita: argomento letterale e variabile, variabile presente/assente, nome vuoto,
' fallback di configurazione e uso in IF. Eseguibile su IL/PCode/VM-C/C99Native.
IMPORT ENV

SUB Main()
    DIM names(3) AS STRING
    DIM i AS INTEGER
    DIM mode AS STRING
    DIM n AS STRING

    names(1) = "PATH"
    names(2) = "HOME"
    names(3) = "GMB_ENV_DEMO_ASSENTE"

    PRINT "=== ENV ==="
    FOR i = 1 TO 3
        PRINT names(i); " present="; ENV.HAS(names(i))
    NEXT i

    ' Argomento variabile + valore: PATH deve essere non vuoto.
    n = "PATH"
    PRINT "PATH non vuoto="; (LEN(ENV$(n)) > 0)

    ' Configurazione con fallback: modo da ambiente, altrimenti "default".
    IF ENV.HAS("GMB_ENV_DEMO_MODE") THEN
        mode = ENV$("GMB_ENV_DEMO_MODE")
    ELSE
        mode = "default"
    END IF
    PRINT "mode="; mode

    PRINT "assente=["; ENV$("GMB_ENV_DEMO_ASSENTE"); "]"
    PRINT "vuoto=["; ENV$(""); "]"
END SUB
