' FILE: Analysis/ControlloQualita.bas
' SCENARIO DIDATTICO — controllo qualità di tre linee di produzione.
'
' Il responsabile qualità ha tre serie di misure (una per linea) e una tabella di
' due grandezze correlate (temperatura e pressione) rilevate sullo stesso lotto.
' Deve rispondere a tre domande:
'   1) le tre linee producono misure statisticamente DIVERSE?  -> ANOVA
'   2) temperatura e pressione misurano lo stesso fenomeno?    -> PCA
'   3) a che frequenza vibra il macchinario?                   -> FFT
'
' Domini: ANALYSIS + MATH. Input fissi: l'output è riproducibile.

IMPORT MULTIVARIATE
IMPORT STATS
SUB Main()
    ' ── 1. ANOVA: tre linee, quattro pezzi ciascuna ──────────────────────────
    ' Se le medie delle linee fossero uguali, F sarebbe vicino a 0 e P alto.
    DIM linee(2) AS ARRAY OF DOUBLE
    linee(0) = {10.1, 10.3, 9.9, 10.0}     ' linea A: stretta intorno a 10
    linee(1) = {10.4, 10.6, 10.2, 10.5}    ' linea B: spostata in alto
    linee(2) = {9.7, 9.9, 9.5, 9.8}        ' linea C: spostata in basso
    DIM aov AS STATS.AnovaResult
    aov = STATS.Anova(linee)
    PRINT "== 1. ANOVA: le linee differiscono? =="
    PRINT "F = "; MATH.ROUND(aov.FValue, 4)
    PRINT "P = "; MATH.ROUND(aov.PValue, 6)
    IF aov.PValue < 0.05 THEN
        PRINT "esito: differenza significativa (P < 0,05) -> controllare le linee"
    ELSE
        PRINT "esito: nessuna differenza significativa"
    END IF

    ' ── 2. PCA: temperatura e pressione ──────────────────────────────────────
    ' Se le due grandezze crescono insieme, la prima componente spiega quasi
    ' tutta la variabilità (autovalore 1 molto maggiore dell'autovalore 2).
    DIM misure(5, 1) AS DOUBLE
    misure(0,0) = 20.0 : misure(0,1) = 1.01
    misure(1,0) = 21.0 : misure(1,1) = 1.02
    misure(2,0) = 22.0 : misure(2,1) = 1.03
    misure(3,0) = 23.0 : misure(3,1) = 1.04
    misure(4,0) = 24.0 : misure(4,1) = 1.05
    misure(5,0) = 25.0 : misure(5,1) = 1.06
    DIM pcaOut(5) AS DOUBLE
    CALL MULTIVARIATE.PCA(misure, pcaOut)
    PRINT "== 2. PCA: temperatura e pressione =="
    PRINT "autovalore 1 = "; MATH.ROUND(pcaOut(0), 6)
    PRINT "autovalore 2 = "; MATH.ROUND(pcaOut(1), 6)
    DIM quota AS DOUBLE
    quota = pcaOut(0) / (pcaOut(0) + pcaOut(1)) * 100.0
    PRINT "prima componente spiega il "; MATH.ROUND(quota, 2); "% della variabilita'"

    ' ── 3. FFT: frequenza di vibrazione del macchinario ──────────────────────
    ' Campioniamo 32 punti di un segnale che oscilla 4 volte per giro: lo spettro
    ' deve mostrare un picco nella riga 4 (indice di frequenza).
    DIM segnale(31) AS DOUBLE
    DIM i AS LONG
    FOR i = 0 TO 31
        segnale(i) = SIN(2 * MATH.PI() * 4 * i / 32.0)
    NEXT i
    DIM spettro = NUMERICAL.Fft(segnale)
    PRINT "== 3. FFT: dove sta il picco? =="
    ' Si confronta il modulo della riga 4 con quello delle righe 1..3 e 5..7.
    DIM picco AS LONG
    picco = 0
    DIM massimo AS DOUBLE
    massimo = 0
    FOR i = 0 TO 15
        DIM modulo AS DOUBLE
        modulo = SQR(spettro(i, 0) * spettro(i, 0) + spettro(i, 1) * spettro(i, 1))
        IF modulo > massimo THEN
            massimo = modulo
            picco = i
        END IF
    NEXT i
    PRINT "frequenza dominante: "; picco; " (atteso 4)"
    PRINT "ampiezza del picco: "; MATH.ROUND(massimo, 2)

    ' ── 4. Ritorno al dominio del tempo (IFFT) ───────────────────────────────
    ' La trasformata è reversibile: ricostruendo il segnale si ritrova il valore
    ' di partenza in ogni punto (qui il primo e l'ottavo).
    DIM ricostruito = NUMERICAL.Ifft(spettro)
    PRINT "== 4. IFFT: il segnale torna indietro =="
    PRINT "punto 0:  "; MATH.ROUND(ricostruito(0, 0), 6); " (originale "; MATH.ROUND(segnale(0), 6); ")"
    PRINT "punto 8:  "; MATH.ROUND(ricostruito(8, 0), 6); " (originale "; MATH.ROUND(segnale(8), 6); ")"
END SUB
