' SURFACE: esercita la superficie del namespace (drill), non uno scenario didattico.
' FILE: Analysis/AnalysisUncoveredCoverage.bas
' =================================================================================================
' TEST UNITARIO: ANALYSIS - FUNZIONI NON ANCORA COPERTE DAGLI ESEMPI
' =================================================================================================
' Questo test copre le funzioni ANALYSIS che non comparivano in alcun esempio .bas:
'   ANOVA, PCA, FFTCOMPLEX, IFFTCOMPLEX.
'
' COPERTURA:
' - ANALYSIS.ANOVA(data)        : analisi della varianza -> tipo AnovaResult.
' - ANALYSIS.PCA(data)          : analisi componenti principali.
' - ANALYSIS.FFTCOMPLEX(x, y)   : FFT complessa.
' - ANALYSIS.IFFTCOMPLEX(x, y)  : IFFT complessa.
' =================================================================================================
IMPORT ANALYSIS
IMPORT MATH

SUB Main()
    CLS
    PRINT "============================================================"
    PRINT "ANALYSIS - Funzioni scoperte dagli esempi"
    PRINT "============================================================"
    PRINT

    ' 1. FFTCOMPLEX / IFFTCOMPLEX (parte reale e immaginaria, stessa lunghezza)
    PRINT "-- FFTCOMPLEX / IFFTCOMPLEX --"
    DIM real(7) AS DOUBLE
    DIM imag(7) AS DOUBLE
    FOR i = 0 TO 7
        real(i) = SIN(i)
        imag(i) = 0.0
    NEXT
    ' FFTCOMPLEX restituisce la matrice [n,2]: colonna 0 parte reale, colonna 1 parte
    ' immaginaria di ogni frequenza. Si leggono le prime due celle della riga 0.
    DIM spettro() = ANALYSIS.FFTCOMPLEX(real, imag)
    PRINT "FFTCOMPLEX(): matrice [n,2], prima riga reale="; spettro(0,0); " imm="; spettro(0,1)
    ' IFFTCOMPLEX: si usa lo stesso input (parte reale/immaginaria) per invertire.
    DIM ii() = ANALYSIS.IFFTCOMPLEX(real, imag)
    PRINT "IFFTCOMPLEX(): prima cella ricostruita="; ii(0,0)
    PRINT

    ' 2. PCA (matrice 2D: osservazioni x variabili)
    PRINT "-- PCA --"
    DIM data(3, 1)
    FOR i = 0 TO 3
        data(i, 0) = RND() * 10.0
        data(i, 1) = RND() * 10.0
    NEXT
    DIM pcaRes AS ANALYSIS.PcaResult
    pcaRes = ANALYSIS.PCA(data)
    PRINT "PCA eseguita."
    PRINT

    ' 3. ANOVA
    PRINT "-- ANOVA --"
    DIM g(1) AS ARRAY OF DOUBLE
    g(0) = {2.0, 3.0, 4.0, 5.0}
    g(1) = {4.0, 5.0, 6.0, 7.0}
    DIM aov AS ANALYSIS.AnovaResult
    aov = ANALYSIS.ANOVA(g)
    PRINT "ANOVA F="; aov.FValue
    PRINT "ANOVA P="; aov.PValue
    PRINT

    PRINT "Test Completato."
END SUB
