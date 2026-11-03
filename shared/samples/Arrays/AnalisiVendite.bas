' FILE: Arrays/AnalisiVendite.bas
' SCENARIO DIDATTICO — analisi di un trimestre di vendite con ARRAYS.
'
' Un negozio registra le vendite mensili e vuole un piccolo report: totale, migliore e
' peggiore mese, suddivisione in settimane (blocchi), completamento dell'anno (padding),
' abbinamento mese→valore e lettura di una riga CSV di prodotti.
'
' Domini: ARRAYS (ACHUNK/APAD/AZIP/ATOKENS/AMAX/AMIN/ASUM) + base (cicli, stringhe).
' Eseguibile su tutte le gambe: nessun I/O, nessuna rete, output deterministico.
IMPORT ARRAYS

SUB Main()
    PRINT "== Vendite del trimestre =="

    ' Vendite mensili (in migliaia di euro).
    DIM vendite(2) AS DOUBLE
    vendite(0) = 12.5
    vendite(1) = 18.0
    vendite(2) = 9.5

    ' Statistiche di base: massimo, minimo e totale.
    PRINT "mese migliore: "; ARRAYS.AMAX(vendite); "k"
    PRINT "mese peggiore: "; ARRAYS.AMIN(vendite); "k"
    PRINT "totale:        "; ARRAYS.ASUM(vendite); "k"

    ' ACHUNK: dividiamo il trimestre in due periodi da confrontare.
    DIM periodi = ARRAYS.ACHUNK(vendite, 2)
    PRINT "periodi: "; UBOUND(periodi) + 1

    ' APAD: completiamo a 12 mesi (gli altri mesi valgono 0).
    DIM anno = ARRAYS.APAD(vendite, 12, 0.0)
    PRINT "mesi nell'anno: "; UBOUND(anno) + 1

    ' AZIP: abbiniamo ogni mese al suo nome.
    DIM mesi(2) AS STRING
    mesi(0) = "gen"
    mesi(1) = "feb"
    mesi(2) = "mar"
    DIM abbinati = ARRAYS.AZIP(mesi, vendite)
    PRINT "abbinamenti: "; UBOUND(abbinati) + 1

    ' ATOKENS: leggiamo l'elenco dei prodotti da una riga CSV.
    DIM prodotti = ARRAYS.ATOKENS("penne,quaderni,righelli", ",")
    PRINT "prodotti: "; UBOUND(prodotti) + 1
    FOR EACH p IN prodotti
        PRINT "  - "; p
    NEXT
END SUB
