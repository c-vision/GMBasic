' MiniTest NAMESPACE FINANCE — tutti i 23 simboli della superficie FINANCE.
'
' Superficie (shared/spec/registry/surface.csv): CUMIPMT, CUMPRINC, DAYS360, DB,
' DDB, FV, FVEXT, IRR, MIRR, MNPV, NPER, NPEREXT, NPV, PMT, PMTEXT, PV, PVEXT,
' RATE, SLN, SYD, XIRR, XNPV, YEARFRAC.
'
' Scopo: verificare che i 4 backend supportino la STESSA versione del namespace,
' non un'applicazione. Tutti i risultati sono arrotondati (ROUND) per non dipendere
' dall'ultimo bit delle librerie matematiche.
' EXPECT: pmt=-88.85|pmtext=-87.97|pv3= 1125.51|pv= 1125.51|pvext= 1136.76|fv= 1268.25|fvext= 1280.93|nper= 10.59|nperext= 10.478145|rate= 0.029229|cumipmt= 198.73|cumprinc=-1264.92|sln= 300|syd1= 450 |syd2= 300|db= 3690|ddb= 4000|npvVar= 11307.29|npvArr= 1188.44|mnpv= 1188.44|irr= 0.163406|mirr= 0.140507|xirr= 0.163241|xnpv= 1305.05|days360= 30|days360eu= 28|yf0= 0.25|yf1= 0.248634|yf2= 0.252778|yf3= 0.249315
IMPORT FINANCE
IMPORT DATETIME

SUB Main()
  ' ── prestiti: rate, interessi, capitale ──────────────────────────────────
  PRINT "pmt="; ROUND(FINANCE.PMT(0.01, 12, 1000), 2)
  PRINT "pmtext="; ROUND(FINANCE.PMTEXT(0.01, 12, 1000, 0, 1), 2)
  PRINT "pv3="; ROUND(FINANCE.PV(0.01, 12, -100), 2)   ' fv opzionale (dai Sample)
  PRINT "pv="; ROUND(FINANCE.PV(0.01, 12, -100, 0), 2)
  PRINT "pvext="; ROUND(FINANCE.PVEXT(0.01, 12, -100, 0, 1), 2)
  PRINT "fv="; ROUND(FINANCE.FV(0.01, 12, -100, 0), 2)
  PRINT "fvext="; ROUND(FINANCE.FVEXT(0.01, 12, -100, 0, 1), 2)
  PRINT "nper="; ROUND(FINANCE.NPER(0.01, -100, 1000), 2)
  PRINT "nperext="; ROUND(FINANCE.NPEREXT(0.01, -100, 1000, 0, 1), 6)
  PRINT "rate="; ROUND(FINANCE.RATE(12, -100, 1000), 6)

  PRINT "cumipmt="; ROUND(FINANCE.CUMIPMT(0.01, 12, 1000, 1, 12, 0), 2)
  PRINT "cumprinc="; ROUND(FINANCE.CUMPRINC(0.01, 12, 1000, 1, 12, 0), 2)

  ' ── ammortamenti ─────────────────────────────────────────────────────────
  PRINT "sln="; ROUND(FINANCE.SLN(1000, 100, 3), 2)
  PRINT "syd1="; ROUND(FINANCE.SYD(1000, 100, 3, 1), 2); "|syd2="; ROUND(FINANCE.SYD(1000, 100, 3, 2), 2)
  PRINT "db="; ROUND(FINANCE.DB(10000, 1000, 5, 1, 12), 2)
  PRINT "ddb="; ROUND(FINANCE.DDB(10000, 1000, 5, 1, 2), 2)

  ' ── flussi di cassa ──────────────────────────────────────────────────────
  DIM flussi(3) AS DOUBLE = {-10000, 3000, 4200, 6800}
  PRINT "npvVar="; ROUND(FINANCE.NPV(0.1, 3000, 4200, 6800), 2)
  PRINT "npvArr="; ROUND(FINANCE.NPV(0.1, flussi), 2)
  PRINT "mnpv="; ROUND(FINANCE.MNPV(0.1, flussi), 2)
  PRINT "irr="; ROUND(FINANCE.IRR(flussi), 6)
  PRINT "mirr="; ROUND(FINANCE.MIRR(flussi, 0.05, 0.08), 6)

  DIM datef(3) AS DATETIME
  datef(0) = DATETIME.DATEVALUE("2023-01-01")
  datef(1) = DATETIME.DATEVALUE("2024-01-01")
  datef(2) = DATETIME.DATEVALUE("2025-01-01")
  datef(3) = DATETIME.DATEVALUE("2026-01-01")
  PRINT "xirr="; ROUND(FINANCE.XIRR(flussi, datef), 6)
  PRINT "xnpv="; ROUND(FINANCE.XNPV(0.1, flussi, datef), 2)

  ' ── giorni e frazioni d'anno ─────────────────────────────────────────────
  PRINT "days360="; FINANCE.DAYS360(DATETIME.DATEVALUE("2024-01-01"), DATETIME.DATEVALUE("2024-01-31"))
  PRINT "days360eu="; FINANCE.DAYS360(DATETIME.DATEVALUE("2024-01-31"), DATETIME.DATEVALUE("2024-02-28"), 1)
  PRINT "yf0="; ROUND(FINANCE.YEARFRAC(DATETIME.DATEVALUE("2024-01-01"), DATETIME.DATEVALUE("2024-04-01"), 0), 4)
  PRINT "yf1="; ROUND(FINANCE.YEARFRAC(DATETIME.DATEVALUE("2024-01-01"), DATETIME.DATEVALUE("2024-04-01"), 1), 6)
  PRINT "yf2="; ROUND(FINANCE.YEARFRAC(DATETIME.DATEVALUE("2024-01-01"), DATETIME.DATEVALUE("2024-04-01"), 2), 6)
  PRINT "yf3="; ROUND(FINANCE.YEARFRAC(DATETIME.DATEVALUE("2024-01-01"), DATETIME.DATEVALUE("2024-04-01"), 3), 6)
END SUB
