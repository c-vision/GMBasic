' FILE: Math/PianificazioneRisparmi.bas
' SCENARIO DIDATTICO — pianificazione dei risparmi mensili con MATH.
'
' Una famiglia registra quanto mette da parte ogni mese e vuole capire l'andamento:
' media mensile (MATH.AVG), arrotondamento all'euro (MATH.ROUND), proiezione annuale
' e confronto con un obiettivo. Mostra anche come si combinano MATH e FINANCE.
'
' Domini: MATH + FINANCE + base. Output deterministico, nessun I/O.
IMPORT MATH
IMPORT FINANCE

SUB Main()
    DIM risparmi(4) AS DOUBLE
    risparmi(0) = 120.0
    risparmi(1) = 80.0
    risparmi(2) = 150.0
    risparmi(3) = 90.0

    DIM media AS DOUBLE
    media = MATH.AVG(risparmi)
    PRINT "risparmio medio: "; MATH.ROUND(media, 2); " EUR"

    ' Proiezione su 12 mesi al ritmo medio.
    DIM annuo AS DOUBLE
    annuo = media * 12
    PRINT "proiezione annua: "; MATH.ROUND(annuo, 2); " EUR"

    ' Obiettivo: 1500 EUR. Quanto manca (o quanto avanza)?
    DIM obiettivo AS DOUBLE
    obiettivo = 1500.0
    DIM scarto AS DOUBLE
    scarto = annuo - obiettivo
    IF scarto >= 0 THEN
        PRINT "obiettivo raggiunto, margine: "; MATH.ROUND(scarto, 2); " EUR"
    ELSE
        PRINT "mancano: "; MATH.ROUND(-scarto, 2); " EUR"
    END IF

    ' Valore futuro di un versamento mensile costante (interesse composto).
    DIM fv AS DOUBLE
    fv = FINANCE.FV(0.03 / 12, 12, -media, 0, 0)
    PRINT "montante a 12 mesi: "; MATH.ROUND(fv, 2); " EUR"
END SUB
