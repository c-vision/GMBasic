' FILE: System/SettingsPaths.bas
' =================================================================================================
'       ESEMPIO: SETTINGS DI GMBASIC E LORO PERCORSO EFFETTIVO
' =================================================================================================
' Da quando i percorsi dati di GMBASIC sono configurabili in system.settings.json (sezione
' "paths" — vedi CommonLib.GmbiPaths nel C# e gmb_paths nel runtime C), i comandi BASIC che
' leggono i settings possono operare anche sul file di settings di GMBasic, purché ne
' conoscano il percorso EFFETTIVO.
'
' Due meccanismi:
'   1) SYSTEM.SYS("SETTINGSPATH") → il percorso effettivo di system.settings.json
'      (risolto dal loader: <root>/config/system.settings.json → <root>/Resources/
'      system.settings.json → eseguibile). Modifica la sezione "paths" del file per cambiare
'      le cartelle senza ricompilare.
'   2) JSON.READVALUE(file$, section$, key$) e JSON.READSECTION(file$, section$) ora sono
'      format-aware: se il file è JSON (o JSONC con commenti, come system.settings.json)
'      leggono l'oggetto annidato (sezione → chiave); se è INI (app.settings.json di
'      progetto) mantengono il comportamento storico. Il default ritorna con chiave null o
'      assente (4° argomento).
'
' =================================================================================================

IMPORT JSON
IMPORT SYSTEM
IMPORT IO

SUB Main()
    PRINT "── Percorso effettivo di system.settings.json ──"
    DIM settingsPath$ = SYSTEM.SYS("SETTINGSPATH")
    PRINT "SETTINGSPATH = "; settingsPath$
    IF NOT IO.FileExists(settingsPath$) THEN
        PRINT "? file di settings non trovato (usa i default in codice)."
    ELSE
        PRINT
        PRINT "── Cartelle dati configurabili (sezione paths) ──"
        PRINT "configDir   = "; JSON.ReadValue(settingsPath$, "paths", "configDir")
        PRINT "skillsDir   = "; JSON.ReadValue(settingsPath$, "paths", "skillsDir")
        PRINT "pluginsDir  = "; JSON.ReadValue(settingsPath$, "paths", "pluginsDir")
        PRINT "logsDir     = "; JSON.ReadValue(settingsPath$, "paths", "logsDir")
        PRINT "ragDir      = "; JSON.ReadValue(settingsPath$, "paths", "ragDir")
        PRINT "graphsDir   = "; JSON.ReadValue(settingsPath$, "paths", "graphsDir")
        PRINT "brainsDir   = "; JSON.ReadValue(settingsPath$, "paths", "brainsDir")
        PRINT "libsDir     = "; JSON.ReadValue(settingsPath$, "paths", "libsDir")
        PRINT
        PRINT "── Chiave mancante/null → default ──"
        PRINT "tempDir     = ["; JSON.ReadValue(settingsPath$, "paths", "tempDir", "(default: tmp del SO)"); "]"
        PRINT
        PRINT "── READSECTION: chiavi della sezione paths ──"
        DIM keys AS JSON = JSON.ReadSection(settingsPath$, "paths")
        PRINT "n. chiavi paths = "; keys.Count
    END IF

    PRINT
    PRINT "── settings di PROGETTO (app.settings.json, formato INI) ──"
    ' JSON.WRITEVALUE/READVALUE SENZA file operano su app.settings.json accanto al .bas
    ' (CWD), formato INI storico. Qui NON scriviamo per non lasciare artefatti: la forma
    ' con file esplicito (usata sopra su system.settings.json) funziona identica sugli INI
    ' di progetto, perché READVALUE è format-aware (JSON se il file inizia con '{', INI no).
    PRINT "READVALUE accetta sia system.settings.json (JSON) sia app.settings.json (INI)."

    PRINT
    PRINT "Fatto. Per cambiare le cartelle senza ricompilare edita la"
    PRINT "sezione paths in <root>/config/system.settings.json (relativi alla"
    PRINT "root del progetto; ~ e variabili d'ambiente sono espansi)."
END SUB
