' FILE: Console/ConstantsModesCoverage.bas
' =================================================================================================
' TEST UNITARIO: CONSOLE — COSTANTI DI SET/MODO (pulsanti, console, joy, file, AI)
' =================================================================================================
' COPERTURA:
' - -- set di pulsanti MSGBOX / stili finestra --: 17 costanti
' - -- modalita' console (CONSOLE n) --: 4 costanti
' - -- joypad --: 6 costanti
' - -- eventi file (ON FILE) --: 4 costanti
' - -- capability AI --: 6 costanti
' - -- reranker del retrieval (AI.SEARCHMULTI) --: 3 costanti
' - -- motore browser (BROWSER.START) --: 3 costanti
' - -- flag ricezione canale (RECEIVE ... OK) --: 3 costanti
' - -- profili governance --: 2 costanti
' - -- allineamento testo box (ALERT/CONFIRM) --: 3 costanti
' - -- altre costanti globali --: 0 costanti
'
' GENERATO da tools/gen_constants_coverage_samples.py — non modificare a mano.
' USO: ALERT("msg", %OKCANCEL) — AI.SEARCHMULTI(..., %RERANK_KEYWORD)
' =================================================================================================
IMPORT CONSOLE

SUB Main()
    DIM i AS INTEGER

    PRINT "-- set di pulsanti MSGBOX / stili finestra --"
    PRINT "  %OK"; TAB(24); %OK
    PRINT "  %CANCEL"; TAB(24); %CANCEL
    PRINT "  %ABORT"; TAB(24); %ABORT
    PRINT "  %RETRY"; TAB(24); %RETRY
    PRINT "  %IGNORE"; TAB(24); %IGNORE
    PRINT "  %YES"; TAB(24); %YES
    PRINT "  %NO"; TAB(24); %NO
    PRINT "  %OKCANCEL"; TAB(24); %OKCANCEL
    PRINT "  %YESNO"; TAB(24); %YESNO
    PRINT "  %YESNOCANCEL"; TAB(24); %YESNOCANCEL
    PRINT "  %ABORTRETRYIGNORE"; TAB(24); %ABORTRETRYIGNORE
    PRINT "  %RETRYCANCEL"; TAB(24); %RETRYCANCEL
    PRINT "  %CLOSED"; TAB(24); %CLOSED
    PRINT "  %NONE"; TAB(24); %NONE
    PRINT "  %SINGLE"; TAB(24); %SINGLE
    PRINT "  %DOUBLE"; TAB(24); %DOUBLE
    PRINT "  %ROUNDED"; TAB(24); %ROUNDED

    PRINT "-- modalita' console (CONSOLE n) --"
    PRINT "  %SDL"; TAB(24); %SDL
    PRINT "  %C64"; TAB(24); %C64
    PRINT "  %ANSI"; TAB(24); %ANSI
    PRINT "  %SYSTEM"; TAB(24); %SYSTEM

    PRINT "-- joypad --"
    PRINT "  %JOYUP"; TAB(24); %JOYUP
    PRINT "  %JOYDOWN"; TAB(24); %JOYDOWN
    PRINT "  %JOYLEFT"; TAB(24); %JOYLEFT
    PRINT "  %JOYRIGHT"; TAB(24); %JOYRIGHT
    PRINT "  %JOYFIRE"; TAB(24); %JOYFIRE
    PRINT "  %JOYBUTTON2"; TAB(24); %JOYBUTTON2

    PRINT "-- eventi file (ON FILE) --"
    PRINT "  %FILE_CREATED"; TAB(24); %FILE_CREATED
    PRINT "  %FILE_DELETED"; TAB(24); %FILE_DELETED
    PRINT "  %FILE_CHANGED"; TAB(24); %FILE_CHANGED
    PRINT "  %FILE_RENAMED"; TAB(24); %FILE_RENAMED

    PRINT "-- capability AI --"
    PRINT "  %CHAT"; TAB(24); %CHAT
    PRINT "  %OCR"; TAB(24); %OCR
    PRINT "  %AUDIO"; TAB(24); %AUDIO
    PRINT "  %EMBEDDING"; TAB(24); %EMBEDDING
    PRINT "  %RERANKER"; TAB(24); %RERANKER
    PRINT "  %TTS"; TAB(24); %TTS

    PRINT "-- reranker del retrieval (AI.SEARCHMULTI) --"
    PRINT "  %RERANK_OFF"; TAB(24); %RERANK_OFF
    PRINT "  %RERANK_KEYWORD"; TAB(24); %RERANK_KEYWORD
    PRINT "  %RERANK_ONNX"; TAB(24); %RERANK_ONNX

    PRINT "-- motore browser (BROWSER.START) --"
    PRINT "  %AUTO"; TAB(24); %AUTO
    PRINT "  %CHROMIUM"; TAB(24); %CHROMIUM
    PRINT "  %OBSCURA"; TAB(24); %OBSCURA

    PRINT "-- flag ricezione canale (RECEIVE ... OK) --"
    PRINT "  %RECEIVED"; TAB(24); %RECEIVED
    PRINT "  %CHCLOSED"; TAB(24); %CHCLOSED
    PRINT "  %K_PRTSC"; TAB(24); %K_PRTSC

    PRINT "-- profili governance --"
    PRINT "  %FAST"; TAB(24); %FAST
    PRINT "  %GOVERNED"; TAB(24); %GOVERNED

    PRINT "-- allineamento testo box (ALERT/CONFIRM) --"
    PRINT "  %LEFT"; TAB(24); %LEFT
    PRINT "  %CENTER"; TAB(24); %CENTER
    PRINT "  %RIGHT"; TAB(24); %RIGHT

    PRINT "-- altre costanti globali --"

    PRINT "Costanti di set/modo coperte: "; 51
    PRINT "Fine mappa costanti."
END SUB
