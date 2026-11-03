' FILE: Json/ConfigurazioneNegozio.bas
' SCENARIO DIDATTICO — configurazione di un negozio salvata in JSON.
'
' Il programma costruisce la configurazione (nome, orari, reparti), la serializza con
' JSON.ToJson, la salva su file, la rilegge con JSON.Load e ne estrae i campi con
' JSON.Extract. Mostra il ciclo completo oggetto → JSON → file → oggetto.
'
' Domini: JSON + IO. Output deterministico; il file di lavoro sta in work/.
IMPORT JSON
IMPORT IO

SUB Main()
    MKDIR "work"

    ' 1) Configurazione in memoria.
    DIM cfg = {"nome": "Bottega", "orario": "09-18", "reparti": 3, "aperto": TRUE}

    ' 2) Serializzazione e salvataggio.
    DIM testo AS STRING
    testo = JSON.TOJSON(cfg)
    DIM h AS LONG
    h = IO.FCREATE("work/negozio.json")
    IO.FWRITE(h, testo)
    IO.FCLOSE(h)
    PRINT "file scritto: "; testo

    ' 3) Rilettura dal file e accesso ai campi.
    DIM letto AS JSON
    letto = JSON.LOAD("work/negozio.json")
    PRINT "nome:   "; letto.nome
    PRINT "orario: "; letto.orario
    PRINT "reparti: "; letto.reparti

    ' 4) Estrazione con path e verifica del tipo.
    PRINT "stringify: "; JSON.STRINGIFY(letto)

    KILL "work/negozio.json"
END SUB
