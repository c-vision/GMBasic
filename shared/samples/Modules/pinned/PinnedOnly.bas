' FILE: samples/modules/pinned/PinnedOnly.bas
' Trovato da `importFiles: ["../pinned/PinnedOnly.bas"]` di app.settings.json: fuori
' dalla radice del programma, caricato SEMPRE anche senza essere nella mappa dei namespace.
NAMESPACE DEMO_PINNED

PUBLIC FUNCTION PinnedOrigin() AS STRING
    RETURN "../pinned/PinnedOnly.bas (importFiles)"
END FUNCTION
