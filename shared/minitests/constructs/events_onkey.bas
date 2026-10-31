' Oracle = golden: ON KEY(90) GOSUB — l'handler parte alla LETTURA del tasto (INKEY).
' Tasto 'Z' (90) iniettato in typeahead con SENDKEY (deterministico, headless).
' Sintassi canone: samples/Community/Mouse/MouseGraphicState.bas (ON KEY(27) GOSUB).
' 2026-09-22: FIX emitter C# (bug "ONKEY-Dispatch-Mancante-VMC-C99", ultima gamba —
' IL/PCode/VM-C/C99Native gia' risolti). NON era un bug di emissione/dispatch (verificato
' corretto riga per riga sul C generato): il gate C99 preiniettava GMB_TUI_SCRIPT (256x ESC)
' nella typeahead PRIMA di Main — ma il ring buffer condiviso gmb_clipper_input_keyboard ha
' capacita' FISSA (GMB_CLIPPER_TYPEAHEAD_MAX=64): i 256 ESC lo riempivano per primi e il
' SENDKEY "Z" del programma (in coda dopo) veniva SCARTATO IN SILENZIO (typeahead_count >=
' MAX) — la 'Z' non entrava mai in coda, nessun drain lato .bas poteva recuperarla (verificato
' dal vivo: un ciclo INKEY di 300 iterazioni non la trova mai, perche' non e' mai stata
' accodata). Fix nel gate (tests/GMBASIC.Backend.C99.Tests/Program.cs): skip del canary per i
' sample che usano gia' SENDKEY (gestiscono da soli la tastiera headless, non serve la
' rete di sicurezza pensata per chiudere i modali TUI). Runtime/emitter non toccati.
' EXPECT: hit= 1|len= 1
DIM Hit AS LONG = 0

SUB OnHit()
    Hit = Hit + 1
END SUB

SUB Main()
    ON KEY(90) GOSUB OnHit
    SENDKEY "Z"
    DIM k AS STRING
    INKEY k
    SLEEP 200  ' park: il task handler (spawmato alla lettura del tasto) riceve il turno
    PRINT "hit="; Hit
    PRINT "len="; LEN(k)
END SUB
