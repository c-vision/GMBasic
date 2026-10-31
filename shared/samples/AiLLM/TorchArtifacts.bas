' FILE: AiLLM/TorchArtifacts.bas
' =================================================================================================
' TEST UNITARIO: TORCH — ARTEFATTI DEL MODELLO (imatrix e export)
' =================================================================================================
' Tre funzioni che lavorano su artefatti PRODOTTI dal model compiler (gmbllm):
'   TORCH.IMATRIXLOAD(path)                  -> handle: carica la matrice di importanza
'                                               (sensibilità per-layer usata dalla quantizzazione)
'   TORCH.IMATRIXRANKS(path, min_rank, max)  -> handle: mappa rank derivata dall'imatrix
'   TORCH.MODELEXPORT(handle, path[, formato]) -> LONG: export esplicito (merge+save flat)
'
' Richiedono un imatrix/checkpoint esistente su disco: il sample verifica FIRMA, ARITÀ e
' sintassi del nome qualificato con la chiamata in `IF 0 THEN` (stessa convenzione di
' minitests/misc/surface_missing_symbols.bas per i simboli legati a una risorsa esterna).
'
' USO REALE: generare l'imatrix con il tool del model compiler, poi rimuovere la guardia.
' =================================================================================================
IMPORT TORCH

SUB Main()
    DIM imatrix AS STRING
    DIM h AS LONG

    imatrix = "models/example.imatrix"

    IF 0 THEN
        h = TORCH.IMATRIXLOAD(imatrix)
        h = TORCH.IMATRIXRANKS(imatrix, 4, 64)
        h = TORCH.MODELEXPORT(h, "work/example.gmbllm", "gguf")
    END IF

    PRINT "TORCH — artefatti del modello"
    PRINT "--------------------------------------------------"
    PRINT "imatrix atteso: "; imatrix
    PRINT "IMATRIXLOAD   -> sensibilità per-layer"
    PRINT "IMATRIXRANKS  -> mappa rank (min 4, max 64)"
    PRINT "MODELEXPORT   -> export flat del modello"
    PRINT "Sintassi verificata; l'esecuzione reale richiede l'imatrix su disco."
    PRINT "--------------------------------------------------"
    PRINT "Fine."
END SUB
