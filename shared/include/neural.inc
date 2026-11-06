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


' ── CNN (conv/pool/im2col/resize/LRN) e FusedMM — configurazioni su handle ──
' Le funzioni C hanno fino a 13 argomenti (>8 del marshalling FFI): la configurazione
' (dimensioni/stride/pad) sta in un handle (1001+), il FORWARD prende solo gli array.
' Layout tensori: CHW (canale, altezza, larghezza), DOUBLE row-major.
' Dimensione di uscita: floor((dim + 2*pad - k)/stride) + 1.
DECLARE FUNCTION OUTSIZE LIB "gmbneural" ALIAS "ofx_nn_outsize" (dim AS LONG, k AS LONG, stride AS LONG, pad AS LONG) AS LONG

' Convoluzione (dil=1 di default; CONVSETDIL per la dilatata). Pesi: out_c×c×kh×kw.
DECLARE FUNCTION CONVINIT LIB "gmbneural" ALIAS "ofx_nn_conv_init" (c AS LONG, h AS LONG, w AS LONG, oc AS LONG, kh AS LONG, kw AS LONG, stride AS LONG, pad AS LONG) AS LONG
DECLARE FUNCTION CONVSETDIL LIB "gmbneural" ALIAS "ofx_nn_conv_set_dil" (lh AS LONG, dil AS LONG) AS LONG
DECLARE FUNCTION CONVRUN LIB "gmbneural" ALIAS "ofx_nn_conv_run" (lh AS LONG, in_() AS DOUBLE, weight() AS DOUBLE, out_() AS DOUBLE) AS LONG
DECLARE FUNCTION CONVRUNB LIB "gmbneural" ALIAS "ofx_nn_conv_runb" (lh AS LONG, in_() AS DOUBLE, weight() AS DOUBLE, bias() AS DOUBLE, out_() AS DOUBLE) AS LONG

' Deconvoluzione (transposed conv): out = (h-1)*stride-2*pad+kh.
DECLARE FUNCTION DECONVINIT LIB "gmbneural" ALIAS "ofx_nn_deconv_init" (c AS LONG, h AS LONG, w AS LONG, oc AS LONG, kh AS LONG, kw AS LONG, stride AS LONG, pad AS LONG) AS LONG
DECLARE FUNCTION DECONVRUN LIB "gmbneural" ALIAS "ofx_nn_deconv_run" (lh AS LONG, in_() AS DOUBLE, weight() AS DOUBLE, out_() AS DOUBLE) AS LONG
DECLARE FUNCTION DECONVRUNB LIB "gmbneural" ALIAS "ofx_nn_deconv_runb" (lh AS LONG, in_() AS DOUBLE, weight() AS DOUBLE, bias() AS DOUBLE, out_() AS DOUBLE) AS LONG

' Winograd F(m×m,3×3), stride 1 pad 1, filtro 3×3; tile 2 o 4 (h,w multipli di tile).
DECLARE FUNCTION WINOINIT LIB "gmbneural" ALIAS "ofx_nn_wino_init" (c AS LONG, h AS LONG, w AS LONG, oc AS LONG, tile AS LONG) AS LONG
DECLARE FUNCTION WINORUN LIB "gmbneural" ALIAS "ofx_nn_wino_run" (lh AS LONG, in_() AS DOUBLE, weight() AS DOUBLE, out_() AS DOUBLE) AS LONG
DECLARE FUNCTION WINORUNB LIB "gmbneural" ALIAS "ofx_nn_wino_runb" (lh AS LONG, in_() AS DOUBLE, weight() AS DOUBLE, bias() AS DOUBLE, out_() AS DOUBLE) AS LONG

' im2col / col2im: col = (c*kh*kw) × (oh*ow).
DECLARE FUNCTION IM2COLINIT LIB "gmbneural" ALIAS "ofx_nn_im2col_init" (c AS LONG, h AS LONG, w AS LONG, kh AS LONG, kw AS LONG, stride AS LONG, pad AS LONG) AS LONG
DECLARE FUNCTION IM2COLRUN LIB "gmbneural" ALIAS "ofx_nn_im2col_run" (lh AS LONG, in_() AS DOUBLE, col() AS DOUBLE) AS LONG
DECLARE FUNCTION COL2IMINIT LIB "gmbneural" ALIAS "ofx_nn_col2im_init" (c AS LONG, h AS LONG, w AS LONG, kh AS LONG, kw AS LONG, stride AS LONG, pad AS LONG) AS LONG
DECLARE FUNCTION COL2IMRUN LIB "gmbneural" ALIAS "ofx_nn_col2im_run" (lh AS LONG, col() AS DOUBLE, out_() AS DOUBLE) AS LONG

' Local Response Normalization.
DECLARE FUNCTION LRNINIT LIB "gmbneural" ALIAS "ofx_nn_lrn_init" (c AS LONG, h AS LONG, w AS LONG, size AS LONG, alpha AS DOUBLE, beta AS DOUBLE, biasv AS DOUBLE) AS LONG
DECLARE FUNCTION LRNRUN LIB "gmbneural" ALIAS "ofx_nn_lrn_run" (lh AS LONG, in_() AS DOUBLE, out_() AS DOUBLE) AS LONG

' Resize: kind 0 = nearest, 1 = bilinear.
DECLARE FUNCTION RESIZEINIT LIB "gmbneural" ALIAS "ofx_nn_resize_init" (c AS LONG, h AS LONG, w AS LONG, oh AS LONG, ow AS LONG) AS LONG
DECLARE FUNCTION RESIZERUN LIB "gmbneural" ALIAS "ofx_nn_resize_run" (lh AS LONG, kind AS LONG, in_() AS DOUBLE, out_() AS DOUBLE) AS LONG

' Pooling 2D (size/stride); ARG riceve l'indice ky*size+kx per output.
DECLARE FUNCTION MAXPOOLINIT LIB "gmbneural" ALIAS "ofx_nn_maxpool_init" (c AS LONG, h AS LONG, w AS LONG, size AS LONG, stride AS LONG) AS LONG
DECLARE FUNCTION AVGPOOLINIT LIB "gmbneural" ALIAS "ofx_nn_avgpool_init" (c AS LONG, h AS LONG, w AS LONG, size AS LONG, stride AS LONG) AS LONG
DECLARE FUNCTION POOLRUN LIB "gmbneural" ALIAS "ofx_nn_pool_run" (lh AS LONG, in_() AS DOUBLE, out_() AS DOUBLE) AS LONG
DECLARE FUNCTION POOLRUNARG LIB "gmbneural" ALIAS "ofx_nn_pool_run_arg" (lh AS LONG, in_() AS DOUBLE, out_() AS DOUBLE, argmax() AS LONG) AS LONG
DECLARE FUNCTION UNPOOLINIT LIB "gmbneural" ALIAS "ofx_nn_unpool_init" (c AS LONG, h AS LONG, w AS LONG, size AS LONG, stride AS LONG) AS LONG
DECLARE FUNCTION UNPOOLRUN LIB "gmbneural" ALIAS "ofx_nn_unpool_run" (lh AS LONG, in_() AS DOUBLE, argmax() AS LONG, fill AS DOUBLE, out_() AS DOUBLE) AS LONG

' FusedMM su CSR (GAT/GraphSAGE): val() può essere vuoto (peso 1)? No: passare i pesi.
DECLARE FUNCTION FUSEDMMINIT LIB "gmbneural" ALIAS "ofx_nn_fusedmm_init" (rows AS LONG, cols AS LONG, k AS LONG, nnz AS LONG, alpha AS DOUBLE, beta AS DOUBLE) AS LONG
DECLARE FUNCTION FUSEDMMRUN LIB "gmbneural" ALIAS "ofx_nn_fusedmm_run" (lh AS LONG, rowptr() AS LONG, colidx() AS LONG, val() AS DOUBLE, X() AS DOUBLE, Y() AS DOUBLE, Z() AS DOUBLE) AS LONG
DECLARE FUNCTION AGGINIT LIB "gmbneural" ALIAS "ofx_nn_aggregate_init" (rows AS LONG, cols AS LONG, k AS LONG, nnz AS LONG, alpha AS DOUBLE, beta AS DOUBLE) AS LONG
DECLARE FUNCTION AGGRUN LIB "gmbneural" ALIAS "ofx_nn_aggregate_run" (lh AS LONG, rowptr() AS LONG, colidx() AS LONG, val() AS DOUBLE, Y() AS DOUBLE, Z() AS DOUBLE) AS LONG

' ── Pruning: 2:4 strutturato e channel pruning (da lightNet) ────────────────
' SPARSIFY24 azzera, in ogni finestra di `window` pesi consecutivi, tutti tranne i
' `nonzeros` di |w| maggiore; ritorna il numero di pesi azzerati (window=4,nonzeros=2 = 2:4).
DECLARE FUNCTION SPARSIFY24 LIB "gmbneural" ALIAS "ofx_nn_sparsify24" (w() AS DOUBLE, n AS LONG, window AS LONG, nonzeros AS LONG) AS LONG
' Norma L1 dei pesi di ogni canale di uscita: w out_c×per_ch -> l1(out_c).
DECLARE FUNCTION PRUNECHANNELL1 LIB "gmbneural" ALIAS "ofx_nn_prune_channel_l1" (w() AS DOUBLE, out_c AS LONG, per_ch AS LONG, l1() AS DOUBLE) AS LONG
' Maschera 1/0 (LONG) dei canali tenuti: top `keep_frac` (0..1) degli score L1; ritorna quanti.
DECLARE FUNCTION PRUNECHANNELMASK LIB "gmbneural" ALIAS "ofx_nn_prune_channels_mask" (l1() AS DOUBLE, out_c AS LONG, keep_frac AS DOUBLE, mask() AS LONG) AS LONG

' ── Quantizzazione affine INT8/UINT8 (XNNPACK/QNNPACK/NNoM) ──────────────────
' Gli array a byte viaggiano come LONG (il marshalling FFI conosce DOUBLE e LONG).
' x -> int8: q = round(x/scale) + zero_point, clamp [-128,127].
DECLARE FUNCTION QUANTI8 LIB "gmbneural" ALIAS "ofx_nn_quant_i8" (x() AS DOUBLE, n AS LONG, scale AS DOUBLE, zero_point AS LONG, out() AS LONG) AS LONG
' int8 -> double: x = (q - zero_point) * scale.
DECLARE FUNCTION DEQUANTI8 LIB "gmbneural" ALIAS "ofx_nn_dequant_i8" (q() AS LONG, n AS LONG, scale AS DOUBLE, zero_point AS LONG, out() AS DOUBLE) AS LONG
' Requantizzazione di un accumulatore int32 tra due griglie int8.
DECLARE FUNCTION REQUANTI8 LIB "gmbneural" ALIAS "ofx_nn_requant_i8" (acc() AS LONG, n AS LONG, in_scale AS DOUBLE, in_zp AS LONG, out_scale AS DOUBLE, out_zp AS LONG, out() AS LONG) AS LONG
' out(0) = scale, out(1) = zero_point per la griglia int8 dai min/max di x.
DECLARE FUNCTION QUANTPARAMS LIB "gmbneural" ALIAS "ofx_nn_quant_params" (x() AS DOUBLE, n AS LONG, out() AS DOUBLE) AS LONG
' GEMM uint8: out(m×n) = A(m×k)·B(k×n), accumulo int32, zero-point, bias int32(n), requant.
DECLARE FUNCTION QUANTGEMMU8 LIB "gmbneural" ALIAS "ofx_nn_quant_gemm_u8" (out() AS LONG, a() AS LONG, b() AS LONG, bias() AS LONG, par() AS DOUBLE, m AS LONG, k AS LONG, n AS LONG) AS LONG
' par(0..5) = scale_a, zp_a, scale_b, zp_b, scale_out, zp_out (firma a 8 argomenti: la VM/C non marshalla oltre 8).
' Requantizzazione a shift (CMSIS-NN): out = clamp((acc + (1<<(rshift-1))) >> rshift).
DECLARE FUNCTION QUANTREQSHIFTI8 LIB "gmbneural" ALIAS "ofx_nn_quant_requant_shift_i8" (acc() AS LONG, n AS LONG, rshift AS LONG, out() AS LONG) AS LONG
' Conv2D quantizzata uint8->int8: in c×h×w, weight out_c×c×kh×kw, bias int32(out_c).
DECLARE FUNCTION QUANTCONV2DI8 LIB "gmbneural" ALIAS "ofx_nn_quant_conv2d_i8" (out() AS LONG, in_() AS LONG, weight() AS LONG, bias() AS LONG, cfg() AS LONG) AS LONG
' cfg(0..9) = c, h, w, out_c, kh, kw, stride, pad, bias_shift, out_shift.
' Clamp di un tensore uint8 in [lo,hi] (in place).
DECLARE FUNCTION QUANTCLAMPU8 LIB "gmbneural" ALIAS "ofx_nn_quant_clamp_u8" (x() AS LONG, n AS LONG, lo AS LONG, hi AS LONG) AS LONG
' Per-canale: scale(out_c) e zero-point(out_c) sulla griglia uint8 [0,255].
DECLARE FUNCTION QUANTPARAMSCHANNEL LIB "gmbneural" ALIAS "ofx_nn_quant_params_channel" (w() AS DOUBLE, out_c AS LONG, per_ch AS LONG, scale() AS DOUBLE, zp() AS LONG) AS LONG
' Conv2D uint8 con scala di uscita per-canale: M(out_c) = scale_in*scale_w/scale_out.
DECLARE FUNCTION QUANTCONV2DI8CHANNEL LIB "gmbneural" ALIAS "ofx_nn_quant_conv2d_i8_channel" (out() AS LONG, in_() AS LONG, weight() AS LONG, bias() AS LONG, M() AS DOUBLE, cfg() AS LONG) AS LONG
' cfg(0..9) = c, h, w, out_c, kh, kw, stride, pad, zp_in, zp_out.

' ── Ternario (1.58-bit, bit-slicing) — handle 2001+ ─────────────────────────
' Quantizza W (rows×cols) a {-1,0,+1}: scale = media(|W|), bitplane w_pos/w_neg.
DECLARE FUNCTION TERNARYINIT LIB "gmbneural" ALIAS "ofx_nn_ternary_init" (w() AS DOUBLE, rows AS LONG, cols AS LONG) AS LONG
DECLARE FUNCTION TERNARYFREE LIB "gmbneural" ALIAS "ofx_nn_ternary_free" (h AS LONG) AS LONG
' Byte per il vettore di attivazione binaria di `dim` bit (multiplo di 8).
DECLARE FUNCTION TERNARYACTBYTES LIB "gmbneural" ALIAS "ofx_nn_ternary_act_bytes" (dim AS LONG) AS LONG
' Prodotto scalare bit-sliced della riga: a() = byte dell'attivazione in LONG.
DECLARE FUNCTION TERNARYDOTROW LIB "gmbneural" ALIAS "ofx_nn_ternary_dotrow" (h AS LONG, row AS LONG, a() AS LONG, nbits AS LONG) AS LONG
' forward: x (batch×cols) -> out (batch×rows) = scale·(W ⊙ sign(x)).
DECLARE FUNCTION TERNARYFORWARD LIB "gmbneural" ALIAS "ofx_nn_ternary_forward" (h AS LONG, x() AS DOUBLE, batch AS LONG, out() AS DOUBLE) AS LONG
' Dequantizza W in DOUBLE row-major (rows×cols).
DECLARE FUNCTION TERNARYTOMAT LIB "gmbneural" ALIAS "ofx_nn_ternary_tomat" (h AS LONG, w() AS DOUBLE) AS LONG
