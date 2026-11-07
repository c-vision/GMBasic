' spectral.inc — namespace SPECTRAL: trasformate (gmbmath-numerical, FFI).
'
' Copre SOLO le trasformate che GMBASIC non ha: DCT/DST e inverse, Haar, Hilbert,
' Z-trasformata, STFT/iSTFT. I filtri digitali (butter/cheby/ellip, lfilter,
' filtfilt, filtfilt, firwin, freqz, decimate, resample, welch, savgol) sono già nel
' namespace SIGNAL: qui non vengono duplicati.
'
' Tutti gli array sono DOUBLE piatti e contigui; la lunghezza del segnale è quella
' dell'array (dimensione implicita dell'ABI). Lo spettro è in coppie (re, im)
' interleavate, frame per frame.
'
' Ritorno: >= 0 = valore utile (n campioni, o numero di frame per STFT),
'          -1 = argomento non valido o buffer troppo corto.

NAMESPACE SPECTRAL

' ── DCT / DST (II, ortonormali) ─────────────────────────────────────────────
' DCT-II ortonormale di x() (n = lunghezza dell'array), risultato in out_().
DECLARE FUNCTION DCT LIB "gmbmath" ALIAS "ofx_sp_dct" (x() AS DOUBLE, out_() AS DOUBLE) AS LONG
' DST-II ortonormale.
DECLARE FUNCTION DST LIB "gmbmath" ALIAS "ofx_sp_dst" (x() AS DOUBLE, out_() AS DOUBLE) AS LONG
' Inversa della DCT-II (ricostruisce il segnale).
DECLARE FUNCTION IDCT LIB "gmbmath" ALIAS "ofx_sp_idct" (x() AS DOUBLE, out_() AS DOUBLE) AS LONG

' ── DCT 2D (immagini, rows x cols row-major) ────────────────────────────────
DECLARE FUNCTION DCT2 LIB "gmbmath" ALIAS "ofx_sp_dct2" (x() AS DOUBLE, rows AS LONG, cols AS LONG, out_() AS DOUBLE) AS LONG
DECLARE FUNCTION IDCT2 LIB "gmbmath" ALIAS "ofx_sp_idct2" (x() AS DOUBLE, rows AS LONG, cols AS LONG, out_() AS DOUBLE) AS LONG

' ── altre trasformate ───────────────────────────────────────────────────────
' Wavelet di Haar (n potenza di 2): approssimazione seguita dai dettagli.
DECLARE FUNCTION HAAR LIB "gmbmath" ALIAS "ofx_sp_haar" (x() AS DOUBLE, out_() AS DOUBLE) AS LONG
' Trasformata di Hilbert discreta: la fase del segnale ruota di 90 gradi.
DECLARE FUNCTION HILBERT LIB "gmbmath" ALIAS "ofx_sp_hilbert" (x() AS DOUBLE, out_() AS DOUBLE) AS LONG
' Z-trasformata in z = re + i*im: out_(0) = parte reale, out_(1) = immaginaria.
DECLARE FUNCTION ZTRANSFORM LIB "gmbmath" ALIAS "ofx_sp_ztransform" (x() AS DOUBLE, re AS DOUBLE, im AS DOUBLE, out_() AS DOUBLE) AS LONG

' ── STFT / iSTFT ────────────────────────────────────────────────────────────
' STFT: x() segnale, N = lunghezza della finestra, w() = finestra (N valori),
' hop = passo. out_() = 2*nf*(N/2+1) double, coppie (re, im) frame per frame.
' Ritorna nf = (n-N)/hop + 1, oppure -1.
DECLARE FUNCTION STFT LIB "gmbmath" ALIAS "ofx_sp_stft" (x() AS DOUBLE, N AS LONG, w() AS DOUBLE, hop AS LONG, out_() AS DOUBLE) AS LONG
' iSTFT (overlap-add): X() = 2*nf*(N/2+1) double, out_() riceve i campioni.
' La lunghezza del segnale ricostruito è out_n (dichiarare out_() della dimensione
' voluta, tipicamente n = (nf-1)*hop + N). Ritorna n, oppure -1.
DECLARE FUNCTION ISTFT LIB "gmbmath" ALIAS "ofx_sp_istft" (X() AS DOUBLE, nframes AS LONG, N AS LONG, w() AS DOUBLE, hop AS LONG, out_() AS DOUBLE) AS LONG
