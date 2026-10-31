' FILE: samples/modules/shared/SharedModule.bas
' Trovato da `importFolders: ["../shared"]` di app.settings.json: è FUORI dalla radice
' del programma, quindi la sola scansione ricorsiva non lo vedrebbe.
NAMESPACE DEMO_SHARED

PUBLIC FUNCTION SharedOrigin() AS STRING
    RETURN "../shared (importFolders)"
END FUNCTION
