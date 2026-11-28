' Filtro daltonia — `ui.color_filter` è una voce del REGISTRO OPZIONI, come i
' flag di accessibilità: il valore scritto si rilegge uguale, "" torna al default
' (filtro spento). La trasformazione dei colori è verificata in modo deterministico
' dal selfcheck C `gmb_color_filter_selfcheck` (matrici, luminanza, clamp); qui si
' verifica la superficie BASIC dell'opzione, che è ciò che le 4 gambe condividono.
' EXPECT: default =|deut = deuteranopia|deut on = yes|achrom = achromatopsia|fine =
IMPORT AI WITH ALIAS

SUB Main()
    PRINT "default = "; AI.OPTION("ui.color_filter")

    AI SET OPTION "ui.color_filter", "deuteranopia"
    PRINT "deut = "; AI.OPTION("ui.color_filter")
    IF AI.OPTION("ui.color_filter") = "deuteranopia" THEN PRINT "deut on = yes" ELSE PRINT "deut on = no"

    AI SET OPTION "ui.color_filter", "achromatopsia"
    PRINT "achrom = "; AI.OPTION("ui.color_filter")

    ' la voce vuota è il DEFAULT: filtro spento (nessuna trasformazione)
    AI SET OPTION "ui.color_filter", ""
    PRINT "fine = "; AI.OPTION("ui.color_filter")
END SUB