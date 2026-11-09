' signal_suite.bas — pacchetto `signal` (FFI su libgmbsignal).
'
' Esempio completo del dominio: usa TUTTE le 18 funzioni del namespace SIGNAL
' con gli stessi ingressi e gli stessi valori attesi dei MiniTest
' (shared/minitests/signal), così il risultato è la prova di parità col builtin.
'
' Esecuzione:
'   gmb pkg install --local packages/signal/0.1.0
'   gmb run shared/samples/packages/signal@0.1.0/signal_suite.bas
'
' Il pacchetto è self-contained (solo libm): nessuna dipendenza di sistema.
'
' EXPECT: hann= 5 |mid= 1 |end= 0|hamming= 5 |mid= 1 |end= 0.08|blackman= 5 |mid= 1 |end= 0|butter= 0.131106439916626 , 0.622465736636155 , 1.47863552962276 , 2.50954045478109 , 3.57180355424326|cheby1= 0.175287 , 3.886115|cheby2= 0.111903 , 1.963141|ellip= 0.056526 , 3.041275|filtfilt= 0.998435 , 6.039977|convolve= 1 , 3 , 5 , 7 , 4|correlate= 1 , 3 , 5 , 7 , 4|findpeaks= 1 , 3|firwin= 0 , 0.151365345728131 , 0.6 , 0.151365345728131 , 0|freqz= 1 , 0.834275 , 0.251286 , 0.044499|resample= 1.5 , 1.08578643762691 , 1.5 , 2.5 , 3.5 , 3.91421356237309 , 3.5 , 2.5|decimate= 0.8 , 2.4|savgol= 1.548571 , 5.342857 , 3.497143|welch= 3 |f0= 0 |f1= 0.25

REQUIRE "signal"
IMPORT SIGNAL

SUB Main()
  ' ── finestre ─────────────────────────────────────────────────────────────
  DIM hann() = SIGNAL.HANN(5)
  PRINT "hann="; LEN(hann); "|mid="; ROUND(hann(2), 6); "|end="; ROUND(hann(0), 6)
  DIM ham() = SIGNAL.HAMMING(5)
  PRINT "hamming="; LEN(ham); "|mid="; ROUND(ham(2), 6); "|end="; ROUND(ham(0), 6)
  DIM blk() = SIGNAL.BLACKMAN(5)
  PRINT "blackman="; LEN(blk); "|mid="; ROUND(blk(2), 6); "|end="; ABS(ROUND(blk(0), 6))

  ' ── filtri IIR + LFILTER ─────────────────────────────────────────────────
  DIM wn(0) AS DOUBLE = {0.3}
  DIM x(4) AS DOUBLE = {1, 2, 3, 4, 5}

  DIM hb = SIGNAL.BUTTER(2, wn, "low")
  DIM yb() = SIGNAL.LFILTER(hb, x)
  PRINT "butter="; yb(0); ","; yb(1); ","; yb(2); ","; yb(3); ","; yb(4)

  DIM hc1 = SIGNAL.CHEBY1(2, 0.5, wn, "low")
  DIM yc1() = SIGNAL.LFILTER(hc1, x)
  PRINT "cheby1="; ROUND(yc1(0), 6); ","; ROUND(yc1(4), 6)

  DIM hc2 = SIGNAL.CHEBY2(2, 20, wn, "low")
  DIM yc2() = SIGNAL.LFILTER(hc2, x)
  PRINT "cheby2="; ROUND(yc2(0), 6); ","; ROUND(yc2(4), 6)

  DIM he = SIGNAL.ELLIP(2, 0.5, 20, wn, "low")
  DIM ye() = SIGNAL.LFILTER(he, x)
  PRINT "ellip="; ROUND(ye(0), 6); ","; ROUND(ye(4), 6)

  ' ── filtro a fase nulla ──────────────────────────────────────────────────
  DIM x6(5) AS DOUBLE = {1, 2, 3, 4, 5, 6}
  DIM yf() = SIGNAL.FILTFILT(hb, x6)
  PRINT "filtfilt="; ROUND(yf(0), 6); ","; ROUND(yf(5), 6)

  ' ── convoluzione e correlazione (risultato "full") ───────────────────────
  DIM s(3) AS DOUBLE = {1, 2, 3, 4}
  DIM k(1) AS DOUBLE = {1, 1}
  DIM cv() = SIGNAL.CONVOLVE(s, k)
  PRINT "convolve="; cv(0); ","; cv(1); ","; cv(2); ","; cv(3); ","; cv(4)
  DIM cr() = SIGNAL.CORRELATE(s, k)
  PRINT "correlate="; cr(0); ","; cr(1); ","; cr(2); ","; cr(3); ","; cr(4)

  ' ── picchi, FIR, risposta in frequenza ───────────────────────────────────
  DIM pk(6) AS DOUBLE = {1, 3, 2, 5, 4, 6}
  DIM fp() = SIGNAL.FINDPEAKS(pk, 1)
  PRINT "findpeaks="; fp(0); ","; fp(1)

  DIM cut(0) AS DOUBLE = {0.3}
  DIM fw() = SIGNAL.FIRWIN(5, cut, "lowpass", "hann")
  PRINT "firwin="; fw(0); ","; fw(1); ","; fw(2); ","; fw(3); ","; fw(4)

  DIM fz() = SIGNAL.FREQZ(hb, 4)
  PRINT "freqz="; ROUND(fz(0), 6); ","; ROUND(fz(1), 6); ","; ROUND(fz(2), 6); ","; ROUND(fz(3), 6)

  ' ── ricampionamento ──────────────────────────────────────────────────────
  DIM rs() = SIGNAL.RESAMPLE(s, 8)
  PRINT "resample="; rs(0); ","; rs(1); ","; rs(2); ","; rs(3); ","; rs(4); ","; rs(5); ","; rs(6); ","; rs(7)
  DIM dc() = SIGNAL.DECIMATE(s, 2)
  PRINT "decimate="; dc(0); ","; dc(1)

  ' ── smoothing ────────────────────────────────────────────────────────────
  DIM sv(6) AS DOUBLE = {1, 4, 2, 8, 5, 7, 3}
  DIM ys() = SIGNAL.SAVGOLFILTER(sv, 5, 2)
  PRINT "savgol="; ROUND(ys(0), 6); ","; ROUND(ys(3), 6); ","; ROUND(ys(6), 6)

  ' ── densità spettrale (Welch, forma piatta del pacchetto) ────────────────
  ' [freq0, psd0, freq1, psd1, ...]: 2*nfreq valori; nperseg 4 su 8 campioni
  ' dà nfreq = 3, quindi 6 valori con frequenze 0 / 0.25 / 0.5 (fs = 1).
  DIM wx(7) AS DOUBLE = {1, 2, 3, 4, 5, 4, 3, 2}
  DIM wh() = SIGNAL.WELCH(wx, 4, 1.0)
  PRINT "welch="; LEN(wh) / 2; "|f0="; wh(0); "|f1="; wh(2)

  ' ── handle: la lib li tiene in un registro interno ───────────────────────
  PRINT "filters="; SIGNAL.FILTERCOUNT()
END SUB
