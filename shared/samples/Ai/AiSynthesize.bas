' FILE: samples/Community/Ai/AiSynthesize.bas
' =================================================================================================
'       AI.SYNTHESIZE — sintesi vocale (TTS) via motore di sistema
'
'       Scrive un WAV in output (macOS `say`, Linux espeak) e ritorna 1 (ok) / 0 (errore).
'       Output in work/ (locale al sample). Guida: Guide/utente/AI-OCR-Audio-Guide.md
' =================================================================================================

IMPORT AI

SUB Main()
    DIM ok AS LONG
    ok = AI.SYNTHESIZE("Buongiorno, come posso aiutarla?", "work/ai_tts.wav")
    PRINT "SYNTHESIZE: ", ok

    ' Testo vuoto → 0 (errore esplicito, nessuna eccezione).
    PRINT "SYNTHESIZE: ", AI.SYNTHESIZE("", "work/ai_tts.wav")
END SUB
