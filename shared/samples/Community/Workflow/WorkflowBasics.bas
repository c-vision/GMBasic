' ==============================================================================
' FILE: Workflow/WorkflowBasics.bas
' WORKFLOW — definizione ed esecuzione di workflow di passi
' (prima era no-op: EmitExprAndPop, nessun runtime)
' ==============================================================================
' Sintassi:
'   WORKFLOW nome          — inizia la definizione
'   STEP nome, azione      — aggiunge un passo
'   END WORKFLOW           — chiude la definizione
'   EXECUTE WORKFLOW nome  — esegue il workflow
' ==============================================================================

IMPORT AI

SUB Main()
    PRINT "=== WORKFLOW Basics ==="
    PRINT

    ' Definisci un workflow di 3 passi
    AI WORKFLOW PianoLavoro
    AI WORKFLOW STEP estrai, "EXTRACT dati"
    AI WORKFLOW STEP analizza, "ANALYZE risultati"
    AI WORKFLOW STEP salva, "SAVE report"
    END WORKFLOW
    PRINT "Workflow 'PianoLavoro' definito (3 passi)"

    ' Esegui il workflow
    AI WORKFLOW EXECUTE PianoLavoro
    PRINT "Workflow eseguito"

    ' Interroga stato e passi
    DIM S AS STRING
    S = AI.WorkflowStatus("PianoLavoro")
    PRINT "Stato: " + S
    DIM P AS STRING
    P = AI.WorkflowSteps("PianoLavoro")
    PRINT "Passi: " + P

    PRINT
    PRINT "Completato."
END SUB
