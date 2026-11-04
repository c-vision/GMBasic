' FILE: System/ScadenzeRate.bas
' SCENARIO DIDATTICO — scadenzario di un finanziamento in 6 rate mensili.
'
' Un ufficio deve stampare le scadenze di un finanziamento che parte il 15/01/2026:
' per ogni rata servono la data, il giorno della settimana (per capire se cade di
' sabato/domenica), il nome del mese e la settimana ISO. Alla fine si controlla
' quanti giorni mancano al saldo, l'ultimo giorno del mese e se l'anno è bisestile.
'
' Domini: DATETIME + MATH. Nessuna funzione "adesso": output deterministico.
IMPORT DATETIME

' Una data cade nel fine settimana? (DOW: 1=domenica .. 7=sabato; BIT canonico -1/0)
FUNCTION NelWeekend(d AS DATETIME) AS LONG
    DIM g AS LONG
    g = DATETIME.DOW(d)
    RETURN (g = 1 OR g = 7)
END FUNCTION

' Giorno a due cifre: 5 -> "05", 15 -> "15".
FUNCTION DueCifre(n AS LONG) AS STRING
    DIM s AS STRING
    s = TRIM$(STR$(n))
    IF LEN(s) < 2 THEN s = "0" + s
    RETURN s
END FUNCTION

SUB Main()
    DIM partenza AS DATETIME
    partenza = DATETIME.CTOD("2026-01-15")

    PRINT "== Scadenzario =="
    PRINT "rata  data        giorno     mese     sett.ISO  weekend"

    DIM rata AS LONG
    DIM dataRata AS DATETIME
    DIM spostate AS LONG
    spostate = 0

    FOR rata = 1 TO 6
        ' Ogni rata scade lo stesso giorno del mese, rata-1 mesi dopo la partenza.
        dataRata = DATETIME.DATEADD("M", rata - 1, partenza)
        IF NelWeekend(dataRata) THEN spostate = spostate + 1
        PRINT DueCifre(rata); "     "; _
              YEAR(dataRata); "-"; DueCifre(MONTH(dataRata)); "-"; DueCifre(DAY(dataRata)); "  "; _
              DATETIME.CDOW(dataRata); "  "; _
              DATETIME.CMONTH(dataRata); "  "; _
              DATETIME.ISOWEEKOFYEAR(dataRata); "        "; _
              NelWeekend(dataRata)
    NEXT rata

    PRINT "rate nel fine settimana: "; spostate

    PRINT "== Controlli di calendario =="
    ' Ultimo giorno del mese di partenza e primo giorno del mese successivo.
    PRINT "fine mese partenza: "; DAY(DATETIME.EOMONTH(partenza))
    PRINT "inizio mese dopo:   "; DAY(DATETIME.FIRSTDAYOFMONTH(DATETIME.DATEADD("M", 1, partenza)))

    ' Quanti giorni dal 15/01/2026 al 15/07/2026 (data dell'ultima rata)?
    DIM ultima AS DATETIME
    ultima = DATETIME.CTOD("2026-07-15")
    DIM giorni AS LONG
    giorni = DATETIME.DATEDIFF("D", partenza, ultima)
    PRINT "giorni al saldo:    "; giorni
    PRINT "mesi equivalenti:   "; MATH.ROUND(giorni / 30.0, 1)

    ' Il 2026 non è bisestile: febbraio ha 28 giorni (limite per la rata di febbraio).
    PRINT "2026 bisestile:     "; DATETIME.ISLEAPYEAR(partenza)
    PRINT "giorni di febbraio: "; DATETIME.DAYSINMONTH(DATETIME.CTOD("2026-02-01"))
END SUB
