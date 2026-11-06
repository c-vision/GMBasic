' ⛔ DIVIETO ASSOLUTO — NON modificare GMBasic (nessun file, nessun commit, nulla) senza
' PERMESSO ESPLICITO dell'utente. GMBasic è un progetto SEPARATO: se serve una modifica,
' FERMARSI e CHIEDERE.
'
' neural.inc — namespace NEURAL: reti ricorrenti (slice 1 di gmbneural, pacchetto gmbneural).
'
' Copre SOLO ciò che non esiste in GMBasic: RNN/LSTM/GRU/BiRNN con BPTT e i momenti PBP.
' MLP/dense/attivazioni/loss sono coperti da TORCH (libtorch) e restano interni al pacchetto;
' kmeans/pca/svm/preprocessing sono coperti da MULTIVARIATE/TORCH e non sono qui.
'
' Convenzioni (come gmbneural):
'   x()  : T × in    DOUBLE row-major     y()/gy() : T × out
'   stato: handle LONG (>0); ZEROGRAD/STEP/FREE sono comuni a tutte le famiglie.
'   STEP applica un ottimizzatore sui gradienti accumulati da BACKWARD:
'         kind = 0 SGD, 1 momentum, 2 Adam; lr > 0.
'   seed: intero (RNG deterministico).

NAMESPACE NEURAL

' ── RNN (tanh) ──────────────────────────────────────────────────────────────
DECLARE FUNCTION RNNINIT LIB "gmbneural" ALIAS "ofx_nn_rnn_init" (in_ AS LONG, hidden AS LONG, out_ AS LONG, seed AS LONG) AS LONG
DECLARE FUNCTION RNNFORWARD LIB "gmbneural" ALIAS "ofx_nn_rnn_forward" (h AS LONG, x() AS DOUBLE, T AS LONG, y() AS DOUBLE) AS LONG
DECLARE FUNCTION RNNBACKWARD LIB "gmbneural" ALIAS "ofx_nn_rnn_backward" (h AS LONG, gy() AS DOUBLE, T AS LONG) AS LONG

' ── LSTM ────────────────────────────────────────────────────────────────────
DECLARE FUNCTION LSTMINIT LIB "gmbneural" ALIAS "ofx_nn_lstm_init" (in_ AS LONG, hidden AS LONG, out_ AS LONG, seed AS LONG) AS LONG
DECLARE FUNCTION LSTMFORWARD LIB "gmbneural" ALIAS "ofx_nn_lstm_forward" (h AS LONG, x() AS DOUBLE, T AS LONG, y() AS DOUBLE) AS LONG
DECLARE FUNCTION LSTMBACKWARD LIB "gmbneural" ALIAS "ofx_nn_lstm_backward" (h AS LONG, gy() AS DOUBLE, T AS LONG) AS LONG

' ── GRU ─────────────────────────────────────────────────────────────────────
DECLARE FUNCTION GRUINIT LIB "gmbneural" ALIAS "ofx_nn_gru_init" (in_ AS LONG, hidden AS LONG, out_ AS LONG, seed AS LONG) AS LONG
DECLARE FUNCTION GRUFORWARD LIB "gmbneural" ALIAS "ofx_nn_gru_forward" (h AS LONG, x() AS DOUBLE, T AS LONG, y() AS DOUBLE) AS LONG
DECLARE FUNCTION GRUBACKWARD LIB "gmbneural" ALIAS "ofx_nn_gru_backward" (h AS LONG, gy() AS DOUBLE, T AS LONG) AS LONG

' ── BiRNN (kind: 0=RNN, 1=LSTM, 2=GRU) ─────────────────────────────────────
DECLARE FUNCTION BIRNNINIT LIB "gmbneural" ALIAS "ofx_nn_birnn_init" (kind AS LONG, in_ AS LONG, hidden AS LONG, out_ AS LONG, seed AS LONG) AS LONG
DECLARE FUNCTION BIRNNFORWARD LIB "gmbneural" ALIAS "ofx_nn_birnn_forward" (h AS LONG, x() AS DOUBLE, T AS LONG, y() AS DOUBLE) AS LONG
DECLARE FUNCTION BIRNNBACKWARD LIB "gmbneural" ALIAS "ofx_nn_birnn_backward" (h AS LONG, gy() AS DOUBLE, T AS LONG) AS LONG

' ── comuni a tutte le famiglie ──────────────────────────────────────────────
' Azzera i gradienti accumulati.
DECLARE FUNCTION ZEROGRAD LIB "gmbneural" ALIAS "ofx_nn_zerograd" (h AS LONG) AS LONG
' Applica l'ottimizzatore (0 SGD, 1 momentum, 2 Adam).
DECLARE FUNCTION STEP LIB "gmbneural" ALIAS "ofx_nn_step" (h AS LONG, kind AS LONG, lr AS DOUBLE) AS LONG
' Libera l'handle (e i buffer dell'ottimizzatore).
DECLARE FUNCTION FREE LIB "gmbneural" ALIAS "ofx_nn_free" (h AS LONG) AS LONG

' ── PBP (Probabilistic Backpropagation, momenti ReLU) ──────────────────────
' out(0)=m_z, out(1)=v_z dei momenti ReLU su N(m_a, v_a).
DECLARE FUNCTION PBPMOMENTS LIB "gmbneural" ALIAS "ofx_nn_pbp_moments" (m_a AS DOUBLE, v_a AS DOUBLE, out() AS DOUBLE) AS LONG
' Strato denso PBP: gli argomenti della funzione C sono 10 (>8 del marshalling FFI),
' quindi i parametri m_w/v_w (out×in) si copiano in un handle con PBPLAYERINIT.
DECLARE FUNCTION PBPLAYERINIT LIB "gmbneural" ALIAS "ofx_nn_pbp_init" (m_w() AS DOUBLE, v_w() AS DOUBLE, in_ AS LONG, out_ AS LONG) AS LONG
' Forward: m_z_prev/v_z_prev = in, m_z/v_z = out; scaling (es. 1/in);
' relu <> 0 applica i momenti ReLU, altrimenti lineare.
DECLARE FUNCTION PBPLAYER LIB "gmbneural" ALIAS "ofx_nn_pbp_run" (h AS LONG, m_z_prev() AS DOUBLE, v_z_prev() AS DOUBLE, scaling AS DOUBLE, relu AS LONG, m_z() AS DOUBLE, v_z() AS DOUBLE) AS LONG

