' ⛔ DIVIETO ASSOLUTO — NON modificare GMBasic (nessun file, nessun commit, nulla) senza
' PERMESSO ESPLICITO dell'utente. GMBasic è un progetto SEPARATO: se serve una modifica,
' FERMARSI e CHIEDERE.
'
' neural.inc — namespace NEURAL: reti, alberi, quantizzazione e autodiff (gmbneural).
'
' Copre SOLO ciò che non esiste in GMBasic: RNN/LSTM/GRU/BiRNN con BPTT, PBP, CNN,
' pruning/quantizzazione, alberi e foreste, autodiff, GP e utilità numeriche.
' MLP/dense/attivazioni/loss chiavi-in-mano sono in TORCH; kmeans/pca/svm in MULTIVARIATE.
'
' Convenzioni (come gmbneural):
'   x()  : T × in    DOUBLE row-major     y()/gy() : T × out
'   Le funzioni che prendono matrici accettano anche un array 2-D di BASIC
'   (DIM m(righe-1, colonne-1)): il buffer e' contiguo row-major e le dimensioni
'   restano parametri espliciti (l'ABI C non le passa).
'   stato: handle LONG (>0); ZEROGRAD/STEP/FREE per le famiglie con parametri.
'   STEP applica un ottimizzatore sui gradienti accumulati da BACKWARD:
'         kind = 0 SGD, 1 momentum, 2 Adam; lr > 0.
'   seed: intero (RNG deterministico).
'   Gli strati senza stato (conv, pooling, winograd, im2col, LRN, resize, FusedMM,
'   PBP, YOLO, KNN) NON usano handle: dimensioni e configurazione sono parametri.

NAMESPACE NEURAL

' ── RNN (tanh) ──────────────────────────────────────────────────────────────
DECLARE FUNCTION RNNINIT LIB "gmbneural" ALIAS "ofx_nn_rnn_init" (in_ AS LONG, hidden AS LONG, out_ AS LONG, seed AS LONG) AS LONG
DECLARE NATIVE FUNCTION RNNFORWARD LIB "gmbneural" ALIAS "ofx_nn_rnn_forward__nat" (h AS LONG, x() AS DOUBLE, T AS LONG, y() AS DOUBLE) AS LONG
DECLARE NATIVE FUNCTION RNNBACKWARD LIB "gmbneural" ALIAS "ofx_nn_rnn_backward__nat" (h AS LONG, gy() AS DOUBLE, T AS LONG) AS LONG

' ── LSTM ────────────────────────────────────────────────────────────────────
DECLARE FUNCTION LSTMINIT LIB "gmbneural" ALIAS "ofx_nn_lstm_init" (in_ AS LONG, hidden AS LONG, out_ AS LONG, seed AS LONG) AS LONG
DECLARE NATIVE FUNCTION LSTMFORWARD LIB "gmbneural" ALIAS "ofx_nn_lstm_forward__nat" (h AS LONG, x() AS DOUBLE, T AS LONG, y() AS DOUBLE) AS LONG
DECLARE NATIVE FUNCTION LSTMBACKWARD LIB "gmbneural" ALIAS "ofx_nn_lstm_backward__nat" (h AS LONG, gy() AS DOUBLE, T AS LONG) AS LONG

' ── GRU ─────────────────────────────────────────────────────────────────────
DECLARE FUNCTION GRUINIT LIB "gmbneural" ALIAS "ofx_nn_gru_init" (in_ AS LONG, hidden AS LONG, out_ AS LONG, seed AS LONG) AS LONG
DECLARE NATIVE FUNCTION GRUFORWARD LIB "gmbneural" ALIAS "ofx_nn_gru_forward__nat" (h AS LONG, x() AS DOUBLE, T AS LONG, y() AS DOUBLE) AS LONG
DECLARE NATIVE FUNCTION GRUBACKWARD LIB "gmbneural" ALIAS "ofx_nn_gru_backward__nat" (h AS LONG, gy() AS DOUBLE, T AS LONG) AS LONG

' ── BiRNN (kind: 0=RNN, 1=LSTM, 2=GRU) ─────────────────────────────────────
DECLARE FUNCTION BIRNNINIT LIB "gmbneural" ALIAS "ofx_nn_birnn_init" (kind AS LONG, in_ AS LONG, hidden AS LONG, out_ AS LONG, seed AS LONG) AS LONG
DECLARE NATIVE FUNCTION BIRNNFORWARD LIB "gmbneural" ALIAS "ofx_nn_birnn_forward__nat" (h AS LONG, x() AS DOUBLE, T AS LONG, y() AS DOUBLE) AS LONG
DECLARE NATIVE FUNCTION BIRNNBACKWARD LIB "gmbneural" ALIAS "ofx_nn_birnn_backward__nat" (h AS LONG, gy() AS DOUBLE, T AS LONG) AS LONG

' ── comuni a tutte le famiglie ──────────────────────────────────────────────
' Azzera i gradienti accumulati.
DECLARE FUNCTION ZEROGRAD LIB "gmbneural" ALIAS "ofx_nn_zerograd" (h AS LONG) AS LONG
' Applica l'ottimizzatore (0 SGD, 1 momentum, 2 Adam).
DECLARE FUNCTION STEP LIB "gmbneural" ALIAS "ofx_nn_step" (h AS LONG, kind AS LONG, lr AS DOUBLE) AS LONG
' Libera l'handle (e i buffer dell'ottimizzatore).
DECLARE FUNCTION FREE LIB "gmbneural" ALIAS "ofx_nn_free" (h AS LONG) AS LONG

' ── PBP (Probabilistic Backpropagation, momenti ReLU) ──────────────────────
' out(0)=m_z, out(1)=v_z dei momenti ReLU su N(m_a, v_a).
DECLARE FUNCTION PBPMOMENTS LIB "gmbneural" ALIAS "ofx_nn_pbp_moments" (m_a AS DOUBLE, v_a AS DOUBLE, out_() AS DOUBLE) AS LONG
' Strato denso PBP senza handle: m_w/v_w (out×in) e i momenti di stato sono parametri.
' relu <> 0 applica i momenti ReLU, altrimenti il layer resta lineare.
DECLARE NATIVE FUNCTION PBPLAYER LIB "gmbneural" ALIAS "ofx_nn_pbp_player__nat" (m_w() AS DOUBLE, v_w() AS DOUBLE, m_prev() AS DOUBLE, v_prev() AS DOUBLE, m_z() AS DOUBLE, v_z() AS DOUBLE, in_ AS LONG, out_ AS LONG, scaling AS DOUBLE, relu AS LONG) AS LONG
' ── CNN, pooling, FusedMM: chiamate dirette (FFI fino a 64 argomenti) ──────
' Layout tensori: CHW (canale, altezza, larghezza), DOUBLE row-major.
' Dimensione di uscita: floor((dim + 2*pad - k)/stride) + 1.
DECLARE FUNCTION OUTSIZE LIB "gmbneural" ALIAS "ofx_nn_outsize" (dim AS LONG, k AS LONG, stride AS LONG, pad AS LONG) AS LONG

' Convoluzione 2D: out (oc×oh×ow) da in (c×h×w) e weight (oc×c×kh×kw).
' dil = 1 → standard, dil > 1 → dilatata. CONV2DB aggiunge il bias (un valore per canale).
DECLARE NATIVE FUNCTION CONV2D LIB "gmbneural" ALIAS "ofx_nn_conv2d__nat" (out_() AS DOUBLE, in_() AS DOUBLE, weight() AS DOUBLE, c AS LONG, h AS LONG, w AS LONG, oc AS LONG, kh AS LONG, kw AS LONG, stride AS LONG, pad AS LONG, dil AS LONG) AS LONG
DECLARE NATIVE FUNCTION CONV2DB LIB "gmbneural" ALIAS "ofx_nn_conv2db__nat" (out_() AS DOUBLE, in_() AS DOUBLE, weight() AS DOUBLE, bias() AS DOUBLE, c AS LONG, h AS LONG, w AS LONG, oc AS LONG, kh AS LONG, kw AS LONG, stride AS LONG, pad AS LONG, dil AS LONG) AS LONG

' Deconvoluzione (transposed conv): out = (h-1)*stride - 2*pad + kh per lato.
DECLARE NATIVE FUNCTION DECONV2D LIB "gmbneural" ALIAS "ofx_nn_deconv2d__nat" (out_() AS DOUBLE, in_() AS DOUBLE, weight() AS DOUBLE, c AS LONG, h AS LONG, w AS LONG, oc AS LONG, kh AS LONG, kw AS LONG, stride AS LONG, pad AS LONG) AS LONG
DECLARE NATIVE FUNCTION DECONV2DB LIB "gmbneural" ALIAS "ofx_nn_deconv2db__nat" (out_() AS DOUBLE, in_() AS DOUBLE, weight() AS DOUBLE, bias() AS DOUBLE, c AS LONG, h AS LONG, w AS LONG, oc AS LONG, kh AS LONG, kw AS LONG, stride AS LONG, pad AS LONG) AS LONG

' Winograd F(m×m, 3×3): stride 1, pad 1; tile 2 o 4 (h,w multipli di tile).
DECLARE NATIVE FUNCTION WINOGRAD LIB "gmbneural" ALIAS "ofx_nn_winograd__nat" (out_() AS DOUBLE, in_() AS DOUBLE, weight() AS DOUBLE, c AS LONG, h AS LONG, w AS LONG, oc AS LONG, tile AS LONG) AS LONG
DECLARE NATIVE FUNCTION WINOGRADB LIB "gmbneural" ALIAS "ofx_nn_winogradb__nat" (out_() AS DOUBLE, in_() AS DOUBLE, weight() AS DOUBLE, bias() AS DOUBLE, c AS LONG, h AS LONG, w AS LONG, oc AS LONG, tile AS LONG) AS LONG

' im2col / col2im: col = (c*kh*kw) × (oh*ow).
DECLARE NATIVE FUNCTION IM2COL LIB "gmbneural" ALIAS "ofx_nn_im2col__nat" (col() AS DOUBLE, in_() AS DOUBLE, c AS LONG, h AS LONG, w AS LONG, kh AS LONG, kw AS LONG, stride AS LONG, pad AS LONG) AS LONG
DECLARE NATIVE FUNCTION COL2IM LIB "gmbneural" ALIAS "ofx_nn_col2im__nat" (out_() AS DOUBLE, col() AS DOUBLE, c AS LONG, h AS LONG, w AS LONG, kh AS LONG, kw AS LONG, stride AS LONG, pad AS LONG) AS LONG

' Local Response Normalization.
DECLARE NATIVE FUNCTION LRN LIB "gmbneural" ALIAS "ofx_nn_lrn__nat" (out_() AS DOUBLE, in_() AS DOUBLE, c AS LONG, h AS LONG, w AS LONG, size AS LONG, alpha AS DOUBLE, beta AS DOUBLE, biasv AS DOUBLE) AS LONG

' Resize: kind 0 = nearest, 1 = bilinear.
DECLARE NATIVE FUNCTION RESIZE LIB "gmbneural" ALIAS "ofx_nn_resize__nat" (out_() AS DOUBLE, in_() AS DOUBLE, c AS LONG, h AS LONG, w AS LONG, oh AS LONG, ow AS LONG, kind AS LONG) AS LONG

' Pooling 2D (size/stride); MAXPOOLARG riceve l'indice ky*size+kx di ogni output.
DECLARE NATIVE FUNCTION MAXPOOL LIB "gmbneural" ALIAS "ofx_nn_maxpool__nat" (out_() AS DOUBLE, in_() AS DOUBLE, c AS LONG, h AS LONG, w AS LONG, size AS LONG, stride AS LONG) AS LONG
DECLARE NATIVE FUNCTION AVGPOOL LIB "gmbneural" ALIAS "ofx_nn_avgpool__nat" (out_() AS DOUBLE, in_() AS DOUBLE, c AS LONG, h AS LONG, w AS LONG, size AS LONG, stride AS LONG) AS LONG
DECLARE NATIVE FUNCTION MAXPOOLARG LIB "gmbneural" ALIAS "ofx_nn_maxpool_arg__nat" (out_() AS DOUBLE, in_() AS DOUBLE, argmax() AS LONG, c AS LONG, h AS LONG, w AS LONG, size AS LONG, stride AS LONG) AS LONG
DECLARE NATIVE FUNCTION UNPOOL LIB "gmbneural" ALIAS "ofx_nn_unpool__nat" (out_() AS DOUBLE, in_() AS DOUBLE, argmax() AS LONG, c AS LONG, h AS LONG, w AS LONG, size AS LONG, stride AS LONG, fill AS DOUBLE) AS LONG

' FusedMM / aggregazione su matrice CSR (GAT/GraphSAGE).
DECLARE NATIVE FUNCTION FUSEDMM LIB "gmbneural" ALIAS "ofx_nn_fusedmm__nat" (out_() AS DOUBLE, rowptr() AS LONG, colidx() AS LONG, val() AS DOUBLE, X() AS DOUBLE, Y() AS DOUBLE, rows AS LONG, cols AS LONG, k AS LONG, nnz AS LONG, alpha AS DOUBLE, beta AS DOUBLE) AS LONG
DECLARE NATIVE FUNCTION AGGREGATE LIB "gmbneural" ALIAS "ofx_nn_aggregate__nat" (out_() AS DOUBLE, rowptr() AS LONG, colidx() AS LONG, val() AS DOUBLE, Y() AS DOUBLE, rows AS LONG, cols AS LONG, k AS LONG, nnz AS LONG, alpha AS DOUBLE, beta AS DOUBLE) AS LONG
' ── Pruning: 2:4 strutturato e channel pruning (da lightNet) ────────────────
' SPARSIFY24 azzera, in ogni finestra di `window` pesi consecutivi, tutti tranne i
' `nonzeros` di |w| maggiore; ritorna il numero di pesi azzerati (window=4,nonzeros=2 = 2:4).
DECLARE NATIVE FUNCTION SPARSIFY24 LIB "gmbneural" ALIAS "ofx_nn_sparsify24__nat" (w() AS DOUBLE, n AS LONG, window AS LONG, nonzeros AS LONG) AS LONG
' Norma L1 dei pesi di ogni canale di uscita: w out_c×per_ch -> l1(out_c).
DECLARE NATIVE FUNCTION PRUNECHANNELL1 LIB "gmbneural" ALIAS "ofx_nn_prune_channel_l1__nat" (w() AS DOUBLE, out_c AS LONG, per_ch AS LONG, l1() AS DOUBLE) AS LONG
' Maschera 1/0 (LONG) dei canali tenuti: top `keep_frac` (0..1) degli score L1; ritorna quanti.
DECLARE NATIVE FUNCTION PRUNECHANNELMASK LIB "gmbneural" ALIAS "ofx_nn_prune_channels_mask__nat" (l1() AS DOUBLE, out_c AS LONG, keep_frac AS DOUBLE, mask() AS LONG) AS LONG

' ── Quantizzazione affine INT8/UINT8 (XNNPACK/QNNPACK/NNoM) ──────────────────
' Gli array a byte viaggiano come LONG (il marshalling FFI conosce DOUBLE e LONG).
' x -> int8: q = round(x/scale) + zero_point, clamp [-128,127].
DECLARE NATIVE FUNCTION QUANTI8 LIB "gmbneural" ALIAS "ofx_nn_quant_i8__nat" (x() AS DOUBLE, n AS LONG, scale AS DOUBLE, zero_point AS LONG, out() AS LONG) AS LONG
' int8 -> double: x = (q - zero_point) * scale.
DECLARE NATIVE FUNCTION DEQUANTI8 LIB "gmbneural" ALIAS "ofx_nn_dequant_i8__nat" (q() AS LONG, n AS LONG, scale AS DOUBLE, zero_point AS LONG, out() AS DOUBLE) AS LONG
' Requantizzazione di un accumulatore int32 tra due griglie int8.
DECLARE NATIVE FUNCTION REQUANTI8 LIB "gmbneural" ALIAS "ofx_nn_requant_i8__nat" (acc() AS LONG, n AS LONG, in_scale AS DOUBLE, in_zp AS LONG, out_scale AS DOUBLE, out_zp AS LONG, out() AS LONG) AS LONG
' out(0) = scale, out(1) = zero_point per la griglia int8 dai min/max di x.
DECLARE NATIVE FUNCTION QUANTPARAMS LIB "gmbneural" ALIAS "ofx_nn_quant_params__nat" (x() AS DOUBLE, n AS LONG, out() AS DOUBLE) AS LONG
' GEMM uint8: out(m×n) = A(m×k)·B(k×n), accumulo int32, zero-point, bias int32(n), requant.
DECLARE NATIVE FUNCTION QUANTGEMMU8 LIB "gmbneural" ALIAS "ofx_nn_quant_gemm_u8__nat" (out() AS LONG, a() AS LONG, b() AS LONG, bias() AS LONG, m AS LONG, k AS LONG, n AS LONG, scale_a AS DOUBLE, zp_a AS LONG, scale_b AS DOUBLE, zp_b AS LONG, scale_out AS DOUBLE, zp_out AS LONG) AS LONG
' Requantizzazione a shift (CMSIS-NN): out = clamp((acc + (1<<(rshift-1))) >> rshift).
DECLARE NATIVE FUNCTION QUANTREQSHIFTI8 LIB "gmbneural" ALIAS "ofx_nn_quant_requant_shift_i8__nat" (acc() AS LONG, n AS LONG, rshift AS LONG, out() AS LONG) AS LONG
' Conv2D quantizzata uint8->int8: in c×h×w, weight out_c×c×kh×kw, bias int32(out_c).
DECLARE NATIVE FUNCTION QUANTCONV2DI8 LIB "gmbneural" ALIAS "ofx_nn_quant_conv2d_i8__nat" (out() AS LONG, in_() AS LONG, weight() AS LONG, bias() AS LONG, c AS LONG, h AS LONG, w AS LONG, out_c AS LONG, kh AS LONG, kw AS LONG, stride AS LONG, pad AS LONG, bias_shift AS LONG, out_shift AS LONG) AS LONG
' Clamp di un tensore uint8 in [lo,hi] (in place).
DECLARE NATIVE FUNCTION QUANTCLAMPU8 LIB "gmbneural" ALIAS "ofx_nn_quant_clamp_u8__nat" (x() AS LONG, n AS LONG, lo AS LONG, hi AS LONG) AS LONG
' Per-canale: scale(out_c) e zero-point(out_c) sulla griglia uint8 [0,255].
DECLARE NATIVE FUNCTION QUANTPARAMSCHANNEL LIB "gmbneural" ALIAS "ofx_nn_quant_params_channel__nat" (w() AS DOUBLE, out_c AS LONG, per_ch AS LONG, scale() AS DOUBLE, zp() AS LONG) AS LONG
' Conv2D uint8 con scala di uscita per-canale: M(out_c) = scale_in*scale_w/scale_out.
DECLARE NATIVE FUNCTION QUANTCONV2DI8CHANNEL LIB "gmbneural" ALIAS "ofx_nn_quant_conv2d_i8_channel__nat" (out() AS LONG, in_() AS LONG, weight() AS LONG, bias() AS LONG, M() AS DOUBLE, c AS LONG, h AS LONG, w AS LONG, out_c AS LONG, kh AS LONG, kw AS LONG, stride AS LONG, pad AS LONG, zp_in AS LONG, zp_out AS LONG) AS LONG

' ── Ternario (1.58-bit, bit-slicing) — handle 2001+ ─────────────────────────
' Quantizza W (rows×cols) a {-1,0,+1}: scale = media(|W|), bitplane w_pos/w_neg.
DECLARE NATIVE FUNCTION TERNARYINIT LIB "gmbneural" ALIAS "ofx_nn_ternary_init__nat" (w() AS DOUBLE, rows AS LONG, cols AS LONG) AS LONG
DECLARE FUNCTION TERNARYFREE LIB "gmbneural" ALIAS "ofx_nn_ternary_free" (h AS LONG) AS LONG
' Byte per il vettore di attivazione binaria di `dim` bit (multiplo di 8).
DECLARE FUNCTION TERNARYACTBYTES LIB "gmbneural" ALIAS "ofx_nn_ternary_act_bytes" (dim AS LONG) AS LONG
' Prodotto scalare bit-sliced della riga: a() = byte dell'attivazione in LONG.
DECLARE NATIVE FUNCTION TERNARYDOTROW LIB "gmbneural" ALIAS "ofx_nn_ternary_dotrow__nat" (h AS LONG, row AS LONG, a() AS LONG, nbits AS LONG) AS LONG
' forward: x (batch×cols) -> out (batch×rows) = scale·(W ⊙ sign(x)).
DECLARE NATIVE FUNCTION TERNARYFORWARD LIB "gmbneural" ALIAS "ofx_nn_ternary_forward__nat" (h AS LONG, x() AS DOUBLE, batch AS LONG, out() AS DOUBLE) AS LONG
' Dequantizza W in DOUBLE row-major (rows×cols).
DECLARE NATIVE FUNCTION TERNARYTOMAT LIB "gmbneural" ALIAS "ofx_nn_ternary_tomat__nat" (h AS LONG, w() AS DOUBLE) AS LONG

' ── Alberi CART, Random Forest, AdaBoost — handle 3001+/3101+/3201+ ─────────
' Albero di regressione/classificazione (split su feature/soglia). I parametri 0
' prendono il default C: depth 32, min_split 2, min_leaf 1.
DECLARE FUNCTION TREEINIT LIB "gmbneural" ALIAS "ofx_nn_tree_init" (max_depth AS LONG, min_split AS LONG, min_leaf AS LONG) AS LONG
' Fit regressione: X n×d row-major, y (n). Ritorna 0/-1.
DECLARE NATIVE FUNCTION TREEFITREG LIB "gmbneural" ALIAS "ofx_nn_tree_fit_reg__nat" (h AS LONG, X() AS DOUBLE, y() AS DOUBLE, n AS LONG, d AS LONG) AS LONG
' Fit classificazione: y in 0..n_classes-1 (array LONG).
DECLARE NATIVE FUNCTION TREEFITCLS LIB "gmbneural" ALIAS "ofx_nn_tree_fit_cls__nat" (h AS LONG, X() AS DOUBLE, y() AS LONG, n AS LONG, d AS LONG, n_classes AS LONG) AS LONG
' Predizione su una riga: regressione = valore, classificazione = classe argmax.
DECLARE NATIVE FUNCTION TREEPREDICT LIB "gmbneural" ALIAS "ofx_nn_tree_predict__nat" (h AS LONG, x() AS DOUBLE) AS DOUBLE
DECLARE FUNCTION TREEFREE LIB "gmbneural" ALIAS "ofx_nn_tree_free" (h AS LONG) AS LONG

' Random Forest: max_features 0 = sqrt(d) in classificazione / d/3+1 in regressione;
' seed 0 = 12345. Bootstrap deterministico.
DECLARE FUNCTION FORESTINIT LIB "gmbneural" ALIAS "ofx_nn_forest_init" (n_estimators AS LONG, max_depth AS LONG, min_split AS LONG, max_features AS LONG, seed AS LONG) AS LONG
DECLARE NATIVE FUNCTION FORESTFITREG LIB "gmbneural" ALIAS "ofx_nn_forest_fit_reg__nat" (h AS LONG, X() AS DOUBLE, y() AS DOUBLE, n AS LONG, d AS LONG) AS LONG
DECLARE NATIVE FUNCTION FORESTFITCLS LIB "gmbneural" ALIAS "ofx_nn_forest_fit_cls__nat" (h AS LONG, X() AS DOUBLE, y() AS LONG, n AS LONG, d AS LONG, n_classes AS LONG) AS LONG
' Regressione: media dei tree; classificazione: voto di maggioranza.
DECLARE NATIVE FUNCTION FORESTPREDICT LIB "gmbneural" ALIAS "ofx_nn_forest_predict__nat" (h AS LONG, x() AS DOUBLE) AS DOUBLE
DECLARE FUNCTION FORESTFREE LIB "gmbneural" ALIAS "ofx_nn_forest_free" (h AS LONG) AS LONG
' AUC di Mann-Whitney U (rank con pareggi): actual 0/1, score continuo.
DECLARE NATIVE FUNCTION AUC LIB "gmbneural" ALIAS "ofx_nn_auc__nat" (actual() AS DOUBLE, score() AS DOUBLE, n AS LONG) AS DOUBLE

' AdaBoost SAMME binario (stump): y in {0,1}; 0 = default (50 stimatori, lr 1.0).
DECLARE FUNCTION ADABOOSTINIT LIB "gmbneural" ALIAS "ofx_nn_adaboost_init" (n_estimators AS LONG, learning_rate AS DOUBLE) AS LONG
DECLARE NATIVE FUNCTION ADABOOSTFIT LIB "gmbneural" ALIAS "ofx_nn_adaboost_fit__nat" (h AS LONG, X() AS DOUBLE, y() AS LONG, n AS LONG, d AS LONG) AS LONG
' Predizione: +1 / -1 (classe 1 / classe 0).
DECLARE NATIVE FUNCTION ADABOOSTPREDICT LIB "gmbneural" ALIAS "ofx_nn_adaboost_predict__nat" (h AS LONG, x() AS DOUBLE) AS LONG
DECLARE FUNCTION ADABOOSTFREE LIB "gmbneural" ALIAS "ofx_nn_adaboost_free" (h AS LONG) AS LONG

' ── SNN (surrogate gradient) e LUT ─────────────────────────────────────────
DECLARE FUNCTION SNNSURROGATE LIB "gmbneural" ALIAS "ofx_nn_snn_surrogate" (x AS DOUBLE) AS DOUBLE
DECLARE FUNCTION SNNSURROGATEGRAD LIB "gmbneural" ALIAS "ofx_nn_snn_surrogate_grad" (x AS DOUBLE) AS DOUBLE
' Indice LUT: bit r = (x[a(r)] - x[b(r)] > 0), n_c <= 30.
DECLARE NATIVE FUNCTION SNNLUTINDEX LIB "gmbneural" ALIAS "ofx_nn_snn_lut_index__nat" (x() AS DOUBLE, a() AS LONG, b() AS LONG, n_c AS LONG) AS LONG

' ── BNN 1-bit: pacchetti da 64 bit in array LONG ───────────────────────────
DECLARE FUNCTION BNNBYTES LIB "gmbneural" ALIAS "ofx_nn_bnn_bytes" (n AS LONG) AS LONG
DECLARE FUNCTION BNNWORDS LIB "gmbneural" ALIAS "ofx_nn_bnn_words" (n AS LONG) AS LONG
' Binarizza x (negativo -> 1) in out() di BNNWORDS(n) parole.
DECLARE NATIVE FUNCTION BNNPACK LIB "gmbneural" ALIAS "ofx_nn_bnn_pack__nat" (out() AS LONG, x() AS DOUBLE, n AS LONG) AS LONG
DECLARE FUNCTION BNNTHRESH LIB "gmbneural" ALIAS "ofx_nn_bnn_thresh" (k AS DOUBLE, h AS DOUBLE) AS DOUBLE
' XNOR-popcount e prodotto scalare ±1 su n bit.
DECLARE NATIVE FUNCTION BNNXNOR LIB "gmbneural" ALIAS "ofx_nn_bnn_xnor__nat" (a() AS LONG, b() AS LONG, n AS LONG) AS LONG
DECLARE NATIVE FUNCTION BNNDOT LIB "gmbneural" ALIAS "ofx_nn_bnn_dot__nat" (a() AS LONG, b() AS LONG, n AS LONG) AS LONG

' ── PINN: residui fisici (errore quadratico) ──────────────────────────────
DECLARE FUNCTION PINNHEAT LIB "gmbneural" ALIAS "ofx_nn_pinn_heat" (u_t AS DOUBLE, u_xx AS DOUBLE, alpha AS DOUBLE) AS DOUBLE
DECLARE FUNCTION PINNWAVE LIB "gmbneural" ALIAS "ofx_nn_pinn_wave" (u_tt AS DOUBLE, u_xx AS DOUBLE, c AS DOUBLE) AS DOUBLE
DECLARE FUNCTION PINNLAPLACE LIB "gmbneural" ALIAS "ofx_nn_pinn_laplace" (u_xx AS DOUBLE, u_yy AS DOUBLE, f AS DOUBLE) AS DOUBLE
DECLARE FUNCTION PINNADVECTION LIB "gmbneural" ALIAS "ofx_nn_pinn_advection" (u_t AS DOUBLE, u_x AS DOUBLE, beta AS DOUBLE) AS DOUBLE
DECLARE FUNCTION PINNBURGERS LIB "gmbneural" ALIAS "ofx_nn_pinn_burgers" (u_t AS DOUBLE, u_x AS DOUBLE, u_xx AS DOUBLE, u AS DOUBLE, nu AS DOUBLE) AS DOUBLE
DECLARE FUNCTION PINNBOUNDARY LIB "gmbneural" ALIAS "ofx_nn_pinn_boundary" (u AS DOUBLE, u_bc AS DOUBLE) AS DOUBLE
DECLARE FUNCTION PINNINITIAL LIB "gmbneural" ALIAS "ofx_nn_pinn_initial" (u AS DOUBLE, u0 AS DOUBLE) AS DOUBLE
' Fattore di normalizzazione = max(|loss|) con pavimento 1e-8.
DECLARE NATIVE FUNCTION PINNADAPTIVENORM LIB "gmbneural" ALIAS "ofx_nn_pinn_adaptive_norm__nat" (losses() AS DOUBLE, n AS LONG) AS DOUBLE

' ── Detection: IoU, NMS, decode YOLO (bbox = x,y,w,h) ─────────────────────
DECLARE FUNCTION IOU LIB "gmbneural" ALIAS "ofx_nn_iou" (a() AS DOUBLE, b() AS DOUBLE) AS DOUBLE
' NMS: keep() riceve gli indici tenuti; ritorna quanti.
DECLARE NATIVE FUNCTION NMS LIB "gmbneural" ALIAS "ofx_nn_nms__nat" (boxes() AS DOUBLE, scores() AS DOUBLE, n AS LONG, iou_thresh AS DOUBLE, keep() AS LONG) AS LONG
' Decode di una cella YOLO: out(0..3) = bx,by,bw,bh normalizzati. Anchor e cella
' sono parametri espliciti (nessun handle).
DECLARE NATIVE FUNCTION YOLOBOX LIB "gmbneural" ALIAS "ofx_nn_yolo_box__nat" (out_() AS DOUBLE, tx AS DOUBLE, ty AS DOUBLE, tw AS DOUBLE, th AS DOUBLE, anchor_w AS DOUBLE, anchor_h AS DOUBLE, cx AS LONG, cy AS LONG, grid_w AS LONG, grid_h AS LONG) AS LONG
' NMS multi-classe: azzera in place le probabilita' soppresse di probs(n×classes).
DECLARE NATIVE FUNCTION NMSMULTICLASS LIB "gmbneural" ALIAS "ofx_nn_nms_multiclass__nat" (boxes() AS DOUBLE, probs() AS DOUBLE, n AS LONG, classes AS LONG, thresh AS DOUBLE) AS LONG

' ── KNN: training set passato a ogni chiamata (Xtr come matrice ntr×d) ─────
DECLARE NATIVE FUNCTION KNNCLASSIFY LIB "gmbneural" ALIAS "ofx_nn_knn_classify__nat" (Xtr() AS DOUBLE, ytr() AS LONG, ntr AS LONG, d AS LONG, n_classes AS LONG, k AS LONG, x() AS DOUBLE) AS LONG
DECLARE NATIVE FUNCTION KNNACCURACY LIB "gmbneural" ALIAS "ofx_nn_knn_accuracy__nat" (Xtr() AS DOUBLE, ytr() AS LONG, ntr AS LONG, Xte() AS DOUBLE, yte() AS LONG, nte AS LONG, d AS LONG, n_classes AS LONG, k AS LONG) AS DOUBLE

' ── Metriche di classificazione e regressione ─────────────────────────────
' Matrice di confusione k×k (riga = vera, colonna = predetta).
DECLARE NATIVE FUNCTION CONFUSION LIB "gmbneural" ALIAS "ofx_nn_confusion__nat" (y_true() AS LONG, y_pred() AS LONG, n AS LONG, k AS LONG, cm() AS LONG) AS LONG
DECLARE NATIVE FUNCTION ACCURACY LIB "gmbneural" ALIAS "ofx_nn_accuracy__nat" (y_true() AS LONG, y_pred() AS LONG, n AS LONG) AS DOUBLE
DECLARE NATIVE FUNCTION ACCURACYCM LIB "gmbneural" ALIAS "ofx_nn_accuracy_cm__nat" (cm() AS LONG, k AS LONG) AS DOUBLE
' PRECISION/RECALL/F1: per-classe in out(k), macro come valore di ritorno.
DECLARE NATIVE FUNCTION PRECISION LIB "gmbneural" ALIAS "ofx_nn_precision__nat" (cm() AS LONG, k AS LONG, out() AS DOUBLE) AS DOUBLE
DECLARE NATIVE FUNCTION RECALL LIB "gmbneural" ALIAS "ofx_nn_recall__nat" (cm() AS LONG, k AS LONG, out() AS DOUBLE) AS DOUBLE
DECLARE NATIVE FUNCTION F1 LIB "gmbneural" ALIAS "ofx_nn_f1__nat" (cm() AS LONG, k AS LONG, out() AS DOUBLE) AS DOUBLE
DECLARE NATIVE FUNCTION MSE LIB "gmbneural" ALIAS "ofx_nn_mse__nat" (y_true() AS DOUBLE, y_pred() AS DOUBLE, n AS LONG) AS DOUBLE
DECLARE NATIVE FUNCTION MAE LIB "gmbneural" ALIAS "ofx_nn_mae__nat" (y_true() AS DOUBLE, y_pred() AS DOUBLE, n AS LONG) AS DOUBLE
DECLARE NATIVE FUNCTION R2 LIB "gmbneural" ALIAS "ofx_nn_r2__nat" (y_true() AS DOUBLE, y_pred() AS DOUBLE, n AS LONG) AS DOUBLE

' ── Pianificazione di memoria (lifetime dei buffer) ───────────────────────
' Array di UDT: il layout C di MemTensor e' double + int32 + int32 (16 byte), lo
' stesso di gn_memplan_tensor. offsets(n) riceve l'offset di ogni tensore; il
' valore di ritorno e' la dimensione totale del buffer.
TYPE MemTensor
  Size AS DOUBLE
  Birth AS LONG
  Death AS LONG
END TYPE
DECLARE NATIVE FUNCTION MEMPLAN LIB "gmbneural" ALIAS "ofx_nn_memplan__nat" (t() AS MemTensor, n AS LONG, offsets() AS DOUBLE) AS DOUBLE

' ── Attivazioni e gradienti (gn_act) ──────────────────────────────────────
' I valori di RELU/SIGMOID/TANH/GELU sono gia' in TORCH: qui stanno i gradienti
' (argomento y = attivazione gia' calcolata dove previsto) e le attivazioni non
' coperte (silu, mish, selu, elu, prelu, celu, hard*, crelu, ...).
DECLARE FUNCTION SIGMOIDGRAD LIB "gmbneural" ALIAS "ofx_nn_act_sigmoid_grad" (x AS DOUBLE) AS DOUBLE
DECLARE FUNCTION TANHGRAD LIB "gmbneural" ALIAS "ofx_nn_act_tanh_grad" (x AS DOUBLE) AS DOUBLE
DECLARE FUNCTION RELUGRAD LIB "gmbneural" ALIAS "ofx_nn_act_relu_grad" (x AS DOUBLE) AS DOUBLE
DECLARE FUNCTION GELUGRAD LIB "gmbneural" ALIAS "ofx_nn_act_gelu_grad" (x AS DOUBLE) AS DOUBLE
DECLARE FUNCTION LEAKYRELU LIB "gmbneural" ALIAS "ofx_nn_act_leaky_relu" (x AS DOUBLE) AS DOUBLE
DECLARE FUNCTION LEAKYRELUGRAD LIB "gmbneural" ALIAS "ofx_nn_act_leaky_relu_grad" (x AS DOUBLE) AS DOUBLE
DECLARE FUNCTION ELU LIB "gmbneural" ALIAS "ofx_nn_act_elu" (x AS DOUBLE) AS DOUBLE
DECLARE FUNCTION ELUGRAD LIB "gmbneural" ALIAS "ofx_nn_act_elu_grad" (x AS DOUBLE) AS DOUBLE
DECLARE FUNCTION IDENTITY LIB "gmbneural" ALIAS "ofx_nn_act_identity" (x AS DOUBLE) AS DOUBLE
DECLARE FUNCTION IDENTITYGRAD LIB "gmbneural" ALIAS "ofx_nn_act_identity_grad" (x AS DOUBLE) AS DOUBLE
DECLARE FUNCTION THRESHOLD LIB "gmbneural" ALIAS "ofx_nn_act_threshold" (x AS DOUBLE) AS DOUBLE
DECLARE FUNCTION THRESHOLDGRAD LIB "gmbneural" ALIAS "ofx_nn_act_threshold_grad" (x AS DOUBLE) AS DOUBLE
DECLARE FUNCTION HARDSIGMOID LIB "gmbneural" ALIAS "ofx_nn_act_hardsigmoid" (x AS DOUBLE) AS DOUBLE
DECLARE FUNCTION HARDSIGMOIDGRAD LIB "gmbneural" ALIAS "ofx_nn_act_hardsigmoid_grad" (x AS DOUBLE) AS DOUBLE
DECLARE FUNCTION HARDSWISH LIB "gmbneural" ALIAS "ofx_nn_act_hardswish" (x AS DOUBLE) AS DOUBLE
DECLARE FUNCTION HARDSWISHGRAD LIB "gmbneural" ALIAS "ofx_nn_act_hardswish_grad" (x AS DOUBLE) AS DOUBLE
DECLARE FUNCTION SILU LIB "gmbneural" ALIAS "ofx_nn_act_silu" (x AS DOUBLE) AS DOUBLE
DECLARE FUNCTION SILUGRAD LIB "gmbneural" ALIAS "ofx_nn_act_silu_grad" (x AS DOUBLE) AS DOUBLE
DECLARE FUNCTION MISH LIB "gmbneural" ALIAS "ofx_nn_act_mish" (x AS DOUBLE) AS DOUBLE
DECLARE FUNCTION MISHGRAD LIB "gmbneural" ALIAS "ofx_nn_act_mish_grad" (x AS DOUBLE) AS DOUBLE
DECLARE FUNCTION SELU LIB "gmbneural" ALIAS "ofx_nn_act_selu" (x AS DOUBLE) AS DOUBLE
DECLARE FUNCTION SELUGRAD LIB "gmbneural" ALIAS "ofx_nn_act_selu_grad" (x AS DOUBLE) AS DOUBLE
DECLARE FUNCTION PLSE LIB "gmbneural" ALIAS "ofx_nn_act_plse" (x AS DOUBLE) AS DOUBLE
DECLARE FUNCTION PLSEGRAD LIB "gmbneural" ALIAS "ofx_nn_act_plse_grad" (x AS DOUBLE) AS DOUBLE
DECLARE FUNCTION LOGGY LIB "gmbneural" ALIAS "ofx_nn_act_loggy" (x AS DOUBLE) AS DOUBLE
DECLARE FUNCTION LOGGYGRAD LIB "gmbneural" ALIAS "ofx_nn_act_loggy_grad" (x AS DOUBLE) AS DOUBLE
DECLARE FUNCTION HARDTAN LIB "gmbneural" ALIAS "ofx_nn_act_hardtan" (x AS DOUBLE) AS DOUBLE
DECLARE FUNCTION HARDTANGRAD LIB "gmbneural" ALIAS "ofx_nn_act_hardtan_grad" (x AS DOUBLE) AS DOUBLE
DECLARE FUNCTION LHTAN LIB "gmbneural" ALIAS "ofx_nn_act_lhtan" (x AS DOUBLE) AS DOUBLE
DECLARE FUNCTION LHTANGRAD LIB "gmbneural" ALIAS "ofx_nn_act_lhtan_grad" (x AS DOUBLE) AS DOUBLE
DECLARE FUNCTION RELIE LIB "gmbneural" ALIAS "ofx_nn_act_relie" (x AS DOUBLE) AS DOUBLE
DECLARE FUNCTION RELIEGRAD LIB "gmbneural" ALIAS "ofx_nn_act_relie_grad" (x AS DOUBLE) AS DOUBLE
DECLARE FUNCTION RAMP LIB "gmbneural" ALIAS "ofx_nn_act_ramp" (x AS DOUBLE) AS DOUBLE
DECLARE FUNCTION RAMPGRAD LIB "gmbneural" ALIAS "ofx_nn_act_ramp_grad" (x AS DOUBLE) AS DOUBLE
DECLARE FUNCTION STAIR LIB "gmbneural" ALIAS "ofx_nn_act_stair" (x AS DOUBLE) AS DOUBLE
DECLARE FUNCTION STAIRGRAD LIB "gmbneural" ALIAS "ofx_nn_act_stair_grad" (x AS DOUBLE) AS DOUBLE
DECLARE FUNCTION CELU LIB "gmbneural" ALIAS "ofx_nn_act_celu" (x AS DOUBLE, alpha AS DOUBLE) AS DOUBLE
DECLARE FUNCTION CELUGRAD LIB "gmbneural" ALIAS "ofx_nn_act_celu_grad" (x AS DOUBLE, alpha AS DOUBLE) AS DOUBLE
DECLARE FUNCTION PRELU LIB "gmbneural" ALIAS "ofx_nn_act_prelu" (x AS DOUBLE, alpha AS DOUBLE) AS DOUBLE
DECLARE FUNCTION PRELUGRAD LIB "gmbneural" ALIAS "ofx_nn_act_prelu_grad" (x AS DOUBLE, alpha AS DOUBLE) AS DOUBLE
' CReLU: out(0..n-1) = max(in,0), out(n..2n-1) = max(-in,0).
DECLARE NATIVE FUNCTION CRELU LIB "gmbneural" ALIAS "ofx_nn_act_crelu__nat" (out_() AS DOUBLE, in_() AS DOUBLE, n AS LONG) AS LONG
DECLARE NATIVE FUNCTION CRELUGRAD LIB "gmbneural" ALIAS "ofx_nn_act_crelu_grad__nat" (grad_in() AS DOUBLE, grad_out() AS DOUBLE, in_() AS DOUBLE, n AS LONG) AS LONG

' ── Strato denso addestrabile (gn_dense) — handle 5001+ ──────────────────
' W: out×in, b: out. FORWARD memorizza x per il BACKWARD; ZEROGRAD/STEP come le RNN.
DECLARE FUNCTION DENSEINIT LIB "gmbneural" ALIAS "ofx_nn_dense_init" (in_ AS LONG, out_ AS LONG, seed AS LONG) AS LONG
DECLARE NATIVE FUNCTION DENSEFORWARD LIB "gmbneural" ALIAS "ofx_nn_dense_forward" (h AS LONG, x() AS DOUBLE, batch AS LONG, OUT y() AS DOUBLE) AS LONG
DECLARE NATIVE FUNCTION DENSEBACKWARD LIB "gmbneural" ALIAS "ofx_nn_dense_backward" (h AS LONG, grad_y() AS DOUBLE, batch AS LONG, OUT grad_x() AS DOUBLE) AS LONG
' Lettura/scrittura dei parametri (W out×in, b out).
DECLARE NATIVE FUNCTION DENSEPARAMS LIB "gmbneural" ALIAS "ofx_nn_dense_params" (h AS LONG, W() AS DOUBLE, b() AS DOUBLE) AS LONG
DECLARE NATIVE FUNCTION DENSESETPARAMS LIB "gmbneural" ALIAS "ofx_nn_dense_setparams" (h AS LONG, W() AS DOUBLE, b() AS DOUBLE) AS LONG
DECLARE FUNCTION DENSEZEROGRAD LIB "gmbneural" ALIAS "ofx_nn_dense_zerograd" (h AS LONG) AS LONG
' kind: 0 SGD, 1 momentum, 2 Adam.
DECLARE FUNCTION DENSESTEP LIB "gmbneural" ALIAS "ofx_nn_dense_step" (h AS LONG, kind AS LONG, lr AS DOUBLE) AS LONG
DECLARE FUNCTION DENSEFREE LIB "gmbneural" ALIAS "ofx_nn_dense_free" (h AS LONG) AS LONG

' ── Perdite con gradiente (gn_loss): y, t, grad sono rows×cols ────────────
' LOSS* scrive il gradiente in grad() e il valore della perdita in loss() (indice 0).
DECLARE NATIVE FUNCTION LOSSMSE LIB "gmbneural" ALIAS "ofx_nn_loss_mse" (y() AS DOUBLE, t() AS DOUBLE, rows AS LONG, cols AS LONG, OUT grad() AS DOUBLE, OUT loss() AS DOUBLE) AS LONG
DECLARE NATIVE FUNCTION LOSSMAE LIB "gmbneural" ALIAS "ofx_nn_loss_mae__nat" (y() AS DOUBLE, t() AS DOUBLE, rows AS LONG, cols AS LONG, OUT grad() AS DOUBLE, OUT loss() AS DOUBLE) AS LONG
DECLARE NATIVE FUNCTION LOSSCROSSENTROPY LIB "gmbneural" ALIAS "ofx_nn_loss_cross_entropy__nat" (y() AS DOUBLE, t() AS DOUBLE, rows AS LONG, cols AS LONG, OUT grad() AS DOUBLE, OUT loss() AS DOUBLE) AS LONG
DECLARE NATIVE FUNCTION LOSSBINARYCROSSENTROPY LIB "gmbneural" ALIAS "ofx_nn_loss_binary_cross_entropy__nat" (y() AS DOUBLE, t() AS DOUBLE, rows AS LONG, cols AS LONG, OUT grad() AS DOUBLE, OUT loss() AS DOUBLE) AS LONG

' ── Dropout — handle 6001+ (maschera interna, rigenerata dal FORWARD in train) ──
DECLARE FUNCTION DROPOUTINIT LIB "gmbneural" ALIAS "ofx_nn_dropout_init" (n AS LONG, rate AS DOUBLE) AS LONG
' train <> 0: campiona con seed e scala 1/(1-rate); train = 0: copia e maschera tutta 1.
DECLARE NATIVE FUNCTION DROPOUTFORWARD LIB "gmbneural" ALIAS "ofx_nn_dropout_forward__nat" (h AS LONG, x() AS DOUBLE, out_() AS DOUBLE, train AS LONG, seed AS LONG) AS LONG
' Riusa la maschera dell'ultimo FORWARD.
DECLARE NATIVE FUNCTION DROPOUTBACKWARD LIB "gmbneural" ALIAS "ofx_nn_dropout_backward__nat" (h AS LONG, grad_out() AS DOUBLE, grad_x() AS DOUBLE) AS LONG
DECLARE FUNCTION DROPOUTFREE LIB "gmbneural" ALIAS "ofx_nn_dropout_free" (h AS LONG) AS LONG

' ── BatchNorm — handle 6101+ (features colonne, x batch×features) ──────────
' train <> 0 aggiorna rmean/rvar con momentum (default 0.1); 0 usa le statistiche correnti.
DECLARE FUNCTION BNINIT LIB "gmbneural" ALIAS "ofx_nn_bn_init" (features AS LONG, eps AS DOUBLE, momentum AS DOUBLE) AS LONG
DECLARE NATIVE FUNCTION BNFORWARD LIB "gmbneural" ALIAS "ofx_nn_bn_forward__nat" (h AS LONG, x() AS DOUBLE, batch AS LONG, y() AS DOUBLE, train AS LONG) AS LONG
' Accumula ggamma/gbeta; senza STEP i gradienti si azzerano con BNZEROGRAD.
DECLARE NATIVE FUNCTION BNBACKWARD LIB "gmbneural" ALIAS "ofx_nn_bn_backward__nat" (h AS LONG, grad_y() AS DOUBLE, batch AS LONG, grad_x() AS DOUBLE) AS LONG
DECLARE NATIVE FUNCTION BNPARAMS LIB "gmbneural" ALIAS "ofx_nn_bn_params__nat" (h AS LONG, gamma() AS DOUBLE, beta() AS DOUBLE) AS LONG
DECLARE NATIVE FUNCTION BNSETPARAMS LIB "gmbneural" ALIAS "ofx_nn_bn_setparams__nat" (h AS LONG, gamma() AS DOUBLE, beta() AS DOUBLE) AS LONG
DECLARE FUNCTION BNZEROGRAD LIB "gmbneural" ALIAS "ofx_nn_bn_zerograd" (h AS LONG) AS LONG
DECLARE FUNCTION BNFREE LIB "gmbneural" ALIAS "ofx_nn_bn_free" (h AS LONG) AS LONG

' ── LayerNorm — handle 6201+ (normalizza per riga) ─────────────────────────
DECLARE FUNCTION LNINIT LIB "gmbneural" ALIAS "ofx_nn_ln_init" (features AS LONG, eps AS DOUBLE) AS LONG
DECLARE NATIVE FUNCTION LNFORWARD LIB "gmbneural" ALIAS "ofx_nn_ln_forward__nat" (h AS LONG, x() AS DOUBLE, batch AS LONG, y() AS DOUBLE) AS LONG
DECLARE NATIVE FUNCTION LNBACKWARD LIB "gmbneural" ALIAS "ofx_nn_ln_backward__nat" (h AS LONG, grad_y() AS DOUBLE, batch AS LONG, grad_x() AS DOUBLE) AS LONG
DECLARE NATIVE FUNCTION LNPARAMS LIB "gmbneural" ALIAS "ofx_nn_ln_params__nat" (h AS LONG, gamma() AS DOUBLE, beta() AS DOUBLE) AS LONG
DECLARE NATIVE FUNCTION LNSETPARAMS LIB "gmbneural" ALIAS "ofx_nn_ln_setparams__nat" (h AS LONG, gamma() AS DOUBLE, beta() AS DOUBLE) AS LONG
DECLARE FUNCTION LNZEROGRAD LIB "gmbneural" ALIAS "ofx_nn_ln_zerograd" (h AS LONG) AS LONG
DECLARE FUNCTION LNFREE LIB "gmbneural" ALIAS "ofx_nn_ln_free" (h AS LONG) AS LONG

' ── Chuck (Adam con dampening, iniezione di rumore, freeze) — handle 6301+ ──
' Un handle = un parametro (matrice rows×cols) con il suo stato m/v e pstate.
DECLARE FUNCTION CHUCKINIT LIB "gmbneural" ALIAS "ofx_nn_chuck_init" () AS LONG
DECLARE FUNCTION CHUCKLOSS LIB "gmbneural" ALIAS "ofx_nn_chuck_loss" (h AS LONG, loss AS DOUBLE) AS LONG
DECLARE NATIVE FUNCTION CHUCKSTEP LIB "gmbneural" ALIAS "ofx_nn_chuck_step__nat" (h AS LONG, param() AS DOUBLE, grad() AS DOUBLE, rows AS LONG, cols AS LONG, lr AS DOUBLE, beta1 AS DOUBLE, beta2 AS DOUBLE, eps AS DOUBLE) AS LONG
' out(0)=dampen out(1)=loss_ema out(2)=noise out(3)=lr_scale out(4)=global_step out(5)=frozen
DECLARE NATIVE FUNCTION CHUCKSTATE LIB "gmbneural" ALIAS "ofx_nn_chuck_state__nat" (h AS LONG, out() AS DOUBLE) AS LONG
DECLARE FUNCTION CHUCKREST LIB "gmbneural" ALIAS "ofx_nn_chuck_reset" (h AS LONG) AS LONG
DECLARE FUNCTION CHUCKFREE LIB "gmbneural" ALIAS "ofx_nn_chuck_free" (h AS LONG) AS LONG

' ── Autodiff scalare (micrograd) — handle 7001+ ────────────────────────────
' Ogni nodo e' un handle: le operazioni ne creano uno nuovo che referenzia i figli.
' BACKWARD propaga i gradienti; FREE libera il nodo (i figli restano dei loro handle).
DECLARE FUNCTION AUTOVALUE LIB "gmbneural" ALIAS "ofx_nn_auto_value" (x AS DOUBLE) AS LONG
DECLARE FUNCTION AUTOADD LIB "gmbneural" ALIAS "ofx_nn_auto_add" (a AS LONG, b AS LONG) AS LONG
DECLARE FUNCTION AUTOSUB LIB "gmbneural" ALIAS "ofx_nn_auto_sub" (a AS LONG, b AS LONG) AS LONG
DECLARE FUNCTION AUTOMUL LIB "gmbneural" ALIAS "ofx_nn_auto_mul" (a AS LONG, b AS LONG) AS LONG
DECLARE FUNCTION AUTODIV LIB "gmbneural" ALIAS "ofx_nn_auto_div" (a AS LONG, b AS LONG) AS LONG
DECLARE FUNCTION AUTOPOW LIB "gmbneural" ALIAS "ofx_nn_auto_pow" (a AS LONG, p AS DOUBLE) AS LONG
DECLARE FUNCTION AUTONEG LIB "gmbneural" ALIAS "ofx_nn_auto_neg" (a AS LONG) AS LONG
DECLARE FUNCTION AUTORELU LIB "gmbneural" ALIAS "ofx_nn_auto_relu" (a AS LONG) AS LONG
DECLARE FUNCTION AUTOTANH LIB "gmbneural" ALIAS "ofx_nn_auto_tanh" (a AS LONG) AS LONG
DECLARE FUNCTION AUTOEXP LIB "gmbneural" ALIAS "ofx_nn_auto_exp" (a AS LONG) AS LONG
DECLARE FUNCTION AUTODATA LIB "gmbneural" ALIAS "ofx_nn_auto_data" (h AS LONG) AS DOUBLE
DECLARE FUNCTION AUTOGRAD LIB "gmbneural" ALIAS "ofx_nn_auto_grad" (h AS LONG) AS DOUBLE
DECLARE FUNCTION AUTOBACKWARD LIB "gmbneural" ALIAS "ofx_nn_auto_backward" (h AS LONG) AS LONG
DECLARE FUNCTION AUTOFREE LIB "gmbneural" ALIAS "ofx_nn_auto_free" (h AS LONG) AS LONG

' ── Programmazione genetica (gn_gp) — handle 7101+ ────────────────────────
' Alfabeto: codes(i) con arity(i) (0 = terminale, 1/2 = funzione). La fitness e'
' una FUNCTION(code() AS DOUBLE) AS DOUBLE che riceve il genoma in ordine prefisso.
DECLARE NATIVE FUNCTION GPINIT LIB "gmbneural" ALIAS "ofx_nn_gp_init__nat" (codes() AS LONG, arity() AS LONG, n AS LONG) AS LONG
' Ritorna la lunghezza del genoma migliore; best() riceve i codici, fit(0) la fitness.
' seed 0 = default del motore; max_depth viene limitato a [1,5].
DECLARE NATIVE FUNCTION GPEVOLVE LIB "gmbneural" ALIAS "ofx_nn_gp_evolve__nat" (h AS LONG, fitness AS FUNCTION(code() AS DOUBLE) AS DOUBLE, best() AS LONG, fit() AS DOUBLE, pop AS LONG, gens AS LONG, seed AS LONG, max_depth AS LONG) AS LONG
' Fitness di un genoma esplicito (codes, n) con la stessa callback.
DECLARE NATIVE FUNCTION GPEVAL LIB "gmbneural" ALIAS "ofx_nn_gp_eval__nat" (h AS LONG, codes() AS LONG, n AS LONG, fitness AS FUNCTION(code() AS DOUBLE) AS DOUBLE) AS DOUBLE
DECLARE FUNCTION GPFREE LIB "gmbneural" ALIAS "ofx_nn_gp_free" (h AS LONG) AS LONG

' ── Normalizzazione online (gn_norm) — handle 7201+ ───────────────────────
' OSSERVA i campioni (max-abs + media/varianza di Welford), FINALIZZA, poi APPLICA /
' DENORM / ZSCORE con le statistiche addestrate. E' il caso "scaler riusabile" che
' TORCH.NORMALIZE/STANDARDIZE (una-shot) non copre.
DECLARE FUNCTION NORMINIT LIB "gmbneural" ALIAS "ofx_nn_norm_init" (dim AS LONG) AS LONG
DECLARE NATIVE FUNCTION NORMOBSERVE LIB "gmbneural" ALIAS "ofx_nn_norm_observe__nat" (h AS LONG, x() AS DOUBLE) AS LONG
' margin <= 0 = 1.0; allarga il fattore max-abs (es. 1.1 per un margine di sicurezza).
DECLARE FUNCTION NORMFINALIZE LIB "gmbneural" ALIAS "ofx_nn_norm_finalize" (h AS LONG, margin AS DOUBLE) AS LONG
' x/scale, x*scale, (x-media)/sd.
DECLARE NATIVE FUNCTION NORMAPPLY LIB "gmbneural" ALIAS "ofx_nn_norm_apply__nat" (h AS LONG, x() AS DOUBLE, out_() AS DOUBLE) AS LONG
DECLARE NATIVE FUNCTION NORMDENORM LIB "gmbneural" ALIAS "ofx_nn_norm_denormalize__nat" (h AS LONG, x() AS DOUBLE, out_() AS DOUBLE) AS LONG
DECLARE NATIVE FUNCTION NORMZSCORE LIB "gmbneural" ALIAS "ofx_nn_norm_zscore__nat" (h AS LONG, x() AS DOUBLE, out_() AS DOUBLE) AS LONG
' out: dim scale, poi dim medie, poi dim deviazioni standard.
DECLARE NATIVE FUNCTION NORMSTATS LIB "gmbneural" ALIAS "ofx_nn_norm_stats__nat" (h AS LONG, out_() AS DOUBLE) AS LONG
DECLARE FUNCTION NORMCOUNT LIB "gmbneural" ALIAS "ofx_nn_norm_count" (h AS LONG) AS LONG
DECLARE FUNCTION NORMFREE LIB "gmbneural" ALIAS "ofx_nn_norm_free" (h AS LONG) AS LONG

' ── Righe/matrici non coperte da TORCH (gn_ops, gn_core) ──────────────────
' GATHERROWS: righe scelte per indice (TORCH.SLICE prende solo un intervallo).
DECLARE NATIVE FUNCTION GATHERROWS LIB "gmbneural" ALIAS "ofx_nn_gather_rows__nat" (out_() AS DOUBLE, data() AS DOUBLE, rows AS LONG, cols AS LONG, indices() AS LONG, n_idx AS LONG) AS LONG
DECLARE NATIVE FUNCTION SLICEROWS LIB "gmbneural" ALIAS "ofx_nn_slice_rows__nat" (out_() AS DOUBLE, data() AS DOUBLE, rows AS LONG, cols AS LONG, start AS LONG, count AS LONG) AS LONG
' Sotto-matrice [r0..r0+nr) × [c0..c0+nc).
DECLARE NATIVE FUNCTION COPYBLOCK LIB "gmbneural" ALIAS "ofx_nn_copy_block__nat" (out_() AS DOUBLE, data() AS DOUBLE, rows AS LONG, cols AS LONG, r0 AS LONG, c0 AS LONG, nr AS LONG, nc AS LONG) AS LONG
' Applica una funzione BASIC a ogni elemento; ABSMAX = max(|x|).
DECLARE NATIVE FUNCTION MATAPPLY LIB "gmbneural" ALIAS "ofx_nn_mat_apply__nat" (out_() AS DOUBLE, in_() AS DOUBLE, n AS LONG, f AS FUNCTION(x AS DOUBLE) AS DOUBLE) AS LONG
DECLARE NATIVE FUNCTION MATABSMAX LIB "gmbneural" ALIAS "ofx_nn_mat_absmax__nat" (in_() AS DOUBLE, n AS LONG) AS DOUBLE

' ── Complementi: softmax con temperatura e LUT SNN ────────────────────────
' TORCH.SOFTMAX copre la temperatura 1; qui la variante con temperatura.
DECLARE NATIVE FUNCTION SOFTMAXTEMP LIB "gmbneural" ALIAS "ofx_nn_softmax_temp__nat" (out_() AS DOUBLE, z() AS DOUBLE, rows AS LONG, cols AS LONG, temp AS DOUBLE) AS LONG
' LUT SNN: s() = stack contiguo dei tempi (n_t blocchi di n_tab tabelle da y_dim);
' per ogni t somma la tabella j(t) in y() (accumula, y va azzerato dal chiamante).
DECLARE NATIVE FUNCTION SNNLUTFORWARD LIB "gmbneural" ALIAS "ofx_nn_snn_lut_forward__nat" (s() AS DOUBLE, n_tab AS LONG, j() AS LONG, n_t AS LONG, y_dim AS LONG, y() AS DOUBLE) AS LONG
