' signal.inc — namespace SIGNAL via FFI (pacchetto `signal`, lib libgmbsignal).
' Uso: REQUIRE "signal"  →  IMPORT SIGNAL
'
' FORME (vincolo dell'emettitore: una DECLARE non puo' ritornare un array):
'   - le primitive SXF_* scrivono in un buffer del chiamante e ritornano il
'     numero di elementi scritti (<0 = errore: -1 generico, -2 buffer piccolo);
'   - i wrapper PUBLIC allocano il buffer della dimensione giusta e ritornano
'     AS ARRAY OF DOUBLE, quindi la firma pubblica resta quella storica:
'     SIGNAL.HANN(5), SIGNAL.LFILTER(h, x), SIGNAL.CONVOLVE(x, k), ...
'   - il design IIR (BUTTER/CHEBY1/CHEBY2/ELLIP) ritorna un handle LONG nel
'     registro interno della lib, da passare a LFILTER/FILTFILT/FREQZ.
'
' ECCEZIONE — WELCH: il builtin ritorna un handle di matrice; qui la lib
' produce il buffer piatto [freq0, psd0, freq1, psd1, ...] e il wrapper
' ritorna l'array piatto (2*nfreq valori). Convertirlo in handle matrice
' richiede il registro del runtime, che vive nel compilatore (F3).
'
' NOTA DI ATTIVAZIONE: le 18 procedure sono ancora builtin del linguaggio,
' quindi `REQUIRE "signal"` viene respinto dal linker finche' la superficie
' builtin non viene rimossa (stessa condizione del pacchetto `crypto`).

NAMESPACE SIGNAL

' ── primitive (ABI: DECLARE NATIVE) ──────────────────────────────────────────
DECLARE NATIVE FUNCTION SXF_HANN LIB "gmbsignal" ALIAS "ofx_signal_hann" (n AS LONG, out() AS DOUBLE, cap AS LONG) AS LONG
DECLARE NATIVE FUNCTION SXF_HAMMING LIB "gmbsignal" ALIAS "ofx_signal_hamming" (n AS LONG, out() AS DOUBLE, cap AS LONG) AS LONG
DECLARE NATIVE FUNCTION SXF_BLACKMAN LIB "gmbsignal" ALIAS "ofx_signal_blackman" (n AS LONG, out() AS DOUBLE, cap AS LONG) AS LONG
DECLARE NATIVE FUNCTION SXF_CONVOLVE LIB "gmbsignal" ALIAS "ofx_signal_convolve" (x() AS DOUBLE, nx AS LONG, k() AS DOUBLE, nk AS LONG, out() AS DOUBLE, cap AS LONG) AS LONG
DECLARE NATIVE FUNCTION SXF_CORRELATE LIB "gmbsignal" ALIAS "ofx_signal_correlate" (x() AS DOUBLE, nx AS LONG, k() AS DOUBLE, nk AS LONG, out() AS DOUBLE, cap AS LONG) AS LONG
DECLARE NATIVE FUNCTION SXF_DECIMATE LIB "gmbsignal" ALIAS "ofx_signal_decimate" (x() AS DOUBLE, n AS LONG, q AS LONG, out() AS DOUBLE, cap AS LONG) AS LONG
DECLARE NATIVE FUNCTION SXF_RESAMPLE LIB "gmbsignal" ALIAS "ofx_signal_resample" (x() AS DOUBLE, n AS LONG, num AS LONG, out() AS DOUBLE, cap AS LONG) AS LONG
DECLARE NATIVE FUNCTION SXF_BUTTER LIB "gmbsignal" ALIAS "ofx_signal_butter" (n AS LONG, wn() AS DOUBLE, nwn AS LONG, btype AS STRING) AS LONG
DECLARE NATIVE FUNCTION SXF_CHEBY1 LIB "gmbsignal" ALIAS "ofx_signal_cheby1" (n AS LONG, rp AS DOUBLE, wn() AS DOUBLE, nwn AS LONG, btype AS STRING) AS LONG
DECLARE NATIVE FUNCTION SXF_CHEBY2 LIB "gmbsignal" ALIAS "ofx_signal_cheby2" (n AS LONG, rs AS DOUBLE, wn() AS DOUBLE, nwn AS LONG, btype AS STRING) AS LONG
DECLARE NATIVE FUNCTION SXF_ELLIP LIB "gmbsignal" ALIAS "ofx_signal_ellip" (n AS LONG, rp AS DOUBLE, rs AS DOUBLE, wn() AS DOUBLE, nwn AS LONG, btype AS STRING) AS LONG
DECLARE NATIVE FUNCTION SXF_LFILTER LIB "gmbsignal" ALIAS "ofx_signal_lfilter" (h AS LONG, x() AS DOUBLE, n AS LONG, out() AS DOUBLE, cap AS LONG) AS LONG
DECLARE NATIVE FUNCTION SXF_FILTFILT LIB "gmbsignal" ALIAS "ofx_signal_filtfilt" (h AS LONG, x() AS DOUBLE, n AS LONG, out() AS DOUBLE, cap AS LONG) AS LONG
DECLARE NATIVE FUNCTION SXF_FREQZ LIB "gmbsignal" ALIAS "ofx_signal_freqz" (h AS LONG, npts AS LONG, out() AS DOUBLE, cap AS LONG) AS LONG
DECLARE NATIVE FUNCTION SXF_FIRWIN LIB "gmbsignal" ALIAS "ofx_signal_firwin" (numtaps AS LONG, cutoff() AS DOUBLE, ncut AS LONG, btype AS STRING, window AS STRING, out() AS DOUBLE, cap AS LONG) AS LONG
DECLARE NATIVE FUNCTION SXF_FINDPEAKS LIB "gmbsignal" ALIAS "ofx_signal_find_peaks" (x() AS DOUBLE, n AS LONG, distance AS DOUBLE, out() AS DOUBLE, cap AS LONG) AS LONG
DECLARE NATIVE FUNCTION SXF_SAVGOL LIB "gmbsignal" ALIAS "ofx_signal_savgol" (x() AS DOUBLE, n AS LONG, window AS LONG, polyorder AS LONG, out() AS DOUBLE, cap AS LONG) AS LONG
DECLARE NATIVE FUNCTION SXF_WELCH LIB "gmbsignal" ALIAS "ofx_signal_welch" (x() AS DOUBLE, n AS LONG, nperseg AS LONG, fs AS DOUBLE, out() AS DOUBLE, cap AS LONG) AS LONG
DECLARE NATIVE FUNCTION SXF_FILTERCOUNT LIB "gmbsignal" ALIAS "ofx_signal_filter_count" () AS LONG

' ── finestre ────────────────────────────────────────────────────────────────
' n = numero di campioni (>=1); torna la finestra simmetrica di n valori.
PUBLIC FUNCTION HANN(n AS DOUBLE) AS ARRAY OF DOUBLE
    DIM m AS LONG = CLNG(n)
    DIM o(m - 1) AS DOUBLE
    CALL SXF_HANN(m, o, m)
    RETURN o
END FUNCTION

PUBLIC FUNCTION HAMMING(n AS DOUBLE) AS ARRAY OF DOUBLE
    DIM m AS LONG = CLNG(n)
    DIM o(m - 1) AS DOUBLE
    CALL SXF_HAMMING(m, o, m)
    RETURN o
END FUNCTION

PUBLIC FUNCTION BLACKMAN(n AS DOUBLE) AS ARRAY OF DOUBLE
    DIM m AS LONG = CLNG(n)
    DIM o(m - 1) AS DOUBLE
    CALL SXF_BLACKMAN(m, o, m)
    RETURN o
END FUNCTION

' ── convoluzione / correlazione ─────────────────────────────────────────────
' "full": nx+nk-1 valori.
PUBLIC FUNCTION CONVOLVE(x() AS DOUBLE, k() AS DOUBLE) AS ARRAY OF DOUBLE
    DIM nx AS LONG = UBOUND(x) + 1
    DIM nk AS LONG = UBOUND(k) + 1
    DIM o(nx + nk - 2) AS DOUBLE
    CALL SXF_CONVOLVE(x, nx, k, nk, o, nx + nk - 1)
    RETURN o
END FUNCTION

PUBLIC FUNCTION CORRELATE(x() AS DOUBLE, k() AS DOUBLE) AS ARRAY OF DOUBLE
    DIM nx AS LONG = UBOUND(x) + 1
    DIM nk AS LONG = UBOUND(k) + 1
    DIM o(nx + nk - 2) AS DOUBLE
    CALL SXF_CORRELATE(x, nx, k, nk, o, nx + nk - 1)
    RETURN o
END FUNCTION

' ── ricampionamento / decimazione ───────────────────────────────────────────
' RESAMPLE: num campioni (FFT, come scipy.signal.resample).
PUBLIC FUNCTION RESAMPLE(x() AS DOUBLE, num AS LONG) AS ARRAY OF DOUBLE
    DIM n AS LONG = UBOUND(x) + 1
    DIM o(num - 1) AS DOUBLE
    CALL SXF_RESAMPLE(x, n, num, o, num)
    RETURN o
END FUNCTION

' DECIMATE: fattore q; ceil(n/q) campioni. q = 1 copia l'ingresso.
PUBLIC FUNCTION DECIMATE(x() AS DOUBLE, q AS LONG) AS ARRAY OF DOUBLE
    DIM n AS LONG = UBOUND(x) + 1
    DIM m AS LONG = n
    IF q > 1 THEN m = CLNG((n + q - 1) / q)
    DIM o(m - 1) AS DOUBLE
    CALL SXF_DECIMATE(x, n, q, o, m)
    RETURN o
END FUNCTION

' ── design IIR: handle > 0 (registro della lib), 0 = errore ─────────────────
' wn = frequenza/i normalizzata/e (0..1, 1 = Nyquist); btype = "low"/"high"/
' "band"/"stop". L'handle si usa con LFILTER/FILTFILT/FREQZ.
PUBLIC FUNCTION BUTTER(n AS DOUBLE, wn() AS DOUBLE, btype AS STRING) AS LONG
    RETURN SXF_BUTTER(CLNG(n), wn, UBOUND(wn) + 1, btype)
END FUNCTION

PUBLIC FUNCTION CHEBY1(n AS DOUBLE, rp AS DOUBLE, wn() AS DOUBLE, btype AS STRING) AS LONG
    RETURN SXF_CHEBY1(CLNG(n), rp, wn, UBOUND(wn) + 1, btype)
END FUNCTION

PUBLIC FUNCTION CHEBY2(n AS DOUBLE, rs AS DOUBLE, wn() AS DOUBLE, btype AS STRING) AS LONG
    RETURN SXF_CHEBY2(CLNG(n), rs, wn, UBOUND(wn) + 1, btype)
END FUNCTION

PUBLIC FUNCTION ELLIP(n AS DOUBLE, rp AS DOUBLE, rs AS DOUBLE, wn() AS DOUBLE, btype AS STRING) AS LONG
    RETURN SXF_ELLIP(CLNG(n), rp, rs, wn, UBOUND(wn) + 1, btype)
END FUNCTION

' ── applicazione del filtro ─────────────────────────────────────────────────
PUBLIC FUNCTION LFILTER(handle AS LONG, x() AS DOUBLE) AS ARRAY OF DOUBLE
    DIM n AS LONG = UBOUND(x) + 1
    DIM o(n - 1) AS DOUBLE
    CALL SXF_LFILTER(handle, x, n, o, n)
    RETURN o
END FUNCTION

' filtro a fase nulla (avanti + indietro): n valori.
PUBLIC FUNCTION FILTFILT(handle AS LONG, x() AS DOUBLE) AS ARRAY OF DOUBLE
    DIM n AS LONG = UBOUND(x) + 1
    DIM o(n - 1) AS DOUBLE
    CALL SXF_FILTFILT(handle, x, n, o, n)
    RETURN o
END FUNCTION

' risposta in frequenza: n valori di |H| (0..pi).
PUBLIC FUNCTION FREQZ(handle AS LONG, n AS DOUBLE) AS ARRAY OF DOUBLE
    DIM m AS LONG = CLNG(n)
    DIM o(m - 1) AS DOUBLE
    CALL SXF_FREQZ(handle, m, o, m)
    RETURN o
END FUNCTION

' ── FIR, picchi, smoothing ──────────────────────────────────────────────────
' FIRWIN: numtaps coefficienti; cutoff = 1 o 2 frequenze normalizzate;
' btype = "lowpass"/"highpass"/"bandpass"/"bandstop"; window = "hann"/
' "hamming"/"blackman".
PUBLIC FUNCTION FIRWIN(numtaps AS LONG, cutoff() AS DOUBLE, btype AS STRING, window AS STRING) AS ARRAY OF DOUBLE
    DIM o(numtaps - 1) AS DOUBLE
    CALL SXF_FIRWIN(numtaps, cutoff, UBOUND(cutoff) + 1, btype, window, o, numtaps)
    RETURN o
END FUNCTION

' FINDPEAKS: indici dei massimi locali; distance = distanza minima (campioni).
' Senza picchi ritorna un array di 1 elemento a 0 (il builtin ritorna vuoto).
PUBLIC FUNCTION FINDPEAKS(x() AS DOUBLE, distance AS DOUBLE) AS ARRAY OF DOUBLE
    DIM n AS LONG = UBOUND(x) + 1
    DIM buf(n - 1) AS DOUBLE
    DIM cnt AS LONG = SXF_FINDPEAKS(x, n, distance, buf, n)
    IF cnt < 1 THEN
        DIM vuoto(0) AS DOUBLE
        RETURN vuoto
    END IF
    DIM o(cnt - 1) AS DOUBLE
    FOR i = 0 TO cnt - 1
        o(i) = buf(i)
    NEXT i
    RETURN o
END FUNCTION

' SAVGOLFILTER: n valori; window_length dispari > polyorder.
PUBLIC FUNCTION SAVGOLFILTER(x() AS DOUBLE, window_length AS LONG, polyorder AS LONG) AS ARRAY OF DOUBLE
    DIM n AS LONG = UBOUND(x) + 1
    DIM o(n - 1) AS DOUBLE
    CALL SXF_SAVGOL(x, n, window_length, polyorder, o, n)
    RETURN o
END FUNCTION

' ── densità spettrale di potenza (Welch) ────────────────────────────────────
' Ritorna l'array piatto [freq0, psd0, freq1, psd1, ...] con nfreq = nperseg/2+1
' righe (nperseg viene ridotto a n se piu' grande), quindi 2*nfreq valori.
PUBLIC FUNCTION WELCH(x() AS DOUBLE, nperseg AS LONG, fs AS DOUBLE) AS ARRAY OF DOUBLE
    DIM n AS LONG = UBOUND(x) + 1
    DIM ps AS LONG = nperseg
    IF ps > n THEN ps = n
    DIM nfreq AS LONG = CLNG(ps / 2) + 1
    DIM o((nfreq * 2) - 1) AS DOUBLE
    CALL SXF_WELCH(x, n, nperseg, fs, o, nfreq * 2)
    RETURN o
END FUNCTION

' ── diagnostica ─────────────────────────────────────────────────────────────
' Handle di filtro attualmente allocati (test/diagnostica).
PUBLIC FUNCTION FILTERCOUNT() AS LONG
    RETURN SXF_FILTERCOUNT()
END FUNCTION
