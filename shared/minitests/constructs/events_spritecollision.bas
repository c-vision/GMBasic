' TAG: interactive
' Oracle = golden: ON SPRITE COLLISION — l'handler riceve (x, y) della collisione
' rilevata dal motore al SYNC (GraphicsManager.DetectCollisions, dispatch software).
' Due sprite sovrapposti alla stessa posizione -> 1 collisione al SYNC.
' Ambiente: harness grafico headless (test_graphics_headless, SDL_VIDEODRIVER=dummy).
' Sintassi canone: samples/Community/Sprites/SpritesExamples.bas
' ENV-GATED (nessun EXPECT, aggiornato 2026-09-21 notte):
'  - L'harness grafico headless ESISTE: test_graphics_headless (SDL_VIDEODRIVER=dummy,
'    pump reale su main thread). Non è il modo GMBASIC_HEADLESS=1 (che droppa le op
'    grafiche per design: bug GPM-Graphics-Park-...).
'  - RUNNER: SDL_VIDEODRIVER=dummy GAI_NO_ILOPT=1 dotnet run --project
'    test_graphics_headless -- minitests/constructs/events_spritecollision.bas
'    (GAI_NO_ILOPT=1 OBBLIGATORIO: ILOptim rompe gli stepper — Hard-Lessons/IL-Optim;
'    senza: "Common Language Runtime detected an invalid program" in Proc_544).
'  - STATO verificato (bisect deterministico): SCREEN/SYNC/ON SPRITE COLLISION/handler
'    e SPRITE LOAD passano da soli ("a|b", gid=1); muore SILENZIOSAMENTE a runtime il
'    primo SPRITE SET POSITION o SPRITE ON dopo un LOAD (v1/v2, no output, no
'    eccezione catturata) — IL a vista coerente: gap IlProgramCompiler.Graphics da
'    investigare PRIMA di toccare (direttiva utente: niente fix non capiti).
'  - C99Native: SPRITE SET POSITION non emesso (errore esplicito, §0bis ok).
'  - NON rimosso da IsParityExcluded (finché il die non è risolto).
'  - WIP grafico headless software (stato sprite/collisioni senza SDL): stash@{0} +
'    /tmp/gmbasic-backup-spritecollision-headless.patch (REVERTITO: crash su PCode).
DIM Hits AS LONG = 0

SUB OnCollision(x AS DOUBLE, y AS DOUBLE)
    Hits = Hits + 1
END SUB

SUB Main()
    SCREEN 64, 64
    DIM SpA
    DIM SpB
    SPRITE LOAD "/Users/gaetano/Documents/Projects/GMBasicMonoRepo/shared/samples/Fixtures/sprite_dot.png", SpA
    SPRITE LOAD "/Users/gaetano/Documents/Projects/GMBasicMonoRepo/shared/samples/Fixtures/sprite_dot.png", SpB
    SPRITE SET POSITION SpA, 10, 10
    SPRITE SET POSITION SpB, 10, 10
    SPRITE ON SpA
    SPRITE ON SpB
    ON SPRITE COLLISION OnCollision
    SYNC
    PRINT "hits="; Hits
END SUB
