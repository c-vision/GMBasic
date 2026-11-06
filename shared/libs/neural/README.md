# gmbneural — reti ricorrenti (namespace BASIC `NEURAL`)

**Prefisso C:** `gn_` · **Dipendenze:** C99 + libm (self-contained: `gc_gemm`/`gc_array`
copiati da `shared/libs/math`)

Slice 1 del port di **gmbneural** via FFI: copre **solo ciò che non esiste in GMBasic**.
`TORCH` (libtorch) copre MLP/dense/attivazioni/loss; `MULTIVARIATE` copre KMeans/Pca/SVM.
Questi moduli restano fuori dal pacchetto per non duplicare la superficie BASIC.

## Superficie (`shared/include/neural.inc`, `IMPORT NEURAL`)

| famiglia | funzioni | note |
|---|---|---|
| RNN | `RNNINIT/FORWARD/BACKWARD` | tanh, BPTT |
| LSTM | `LSTMINIT/FORWARD/BACKWARD` | 4 gate, BPTT |
| GRU | `GRUINIT/FORWARD/BACKWARD` | z/r/n, BPTT |
| BiRNN | `BIRNNINIT/FORWARD/BACKWARD` | `kind` 0=RNN 1=LSTM 2=GRU, due direzioni |
| comuni | `ZEROGRAD`, `STEP`, `FREE` | handle; `STEP(h, 0|1|2, lr)` = SGD/momentum/Adam |
| PBP | `PBPMOMENTS`, `PBPLAYERINIT`, `PBPLAYER` | momenti ReLU + forward denso (handle) |
| ~~ML classico~~ | — | **non qui**: KMeans/PCA/SVM sono già FFI nel namespace `MULTIVARIATE` (`multivariate.inc`, libgmbstats), dopo il ritiro dei builtin (2026-11-05) |
| CNN | `CONVINIT/CONVSETDIL/CONVRUN[R B]`, `DECONV*`, `WINO*`, `IM2COL*`, `COL2IM*`, `LRN*`, `RESIZE*` | layout CHW; configurazione su handle (i parametri non si ripetono a ogni forward) |
| Pooling | `MAXPOOLINIT`, `AVGPOOLINIT`, `POOLRUN`, `POOLRUNARG`, `UNPOOLINIT`, `UNPOOLRUN` | argmax LONG per il backward |
| FusedMM | `FUSEDMMINIT/RUN`, `AGGINIT/RUN` | CSR (GAT/GraphSAGE) |
| Pruning | `SPARSIFY24`, `PRUNECHANNELL1`, `PRUNECHANNELMASK` | 2:4 strutturato, channel pruning |
| Quantizzazione | `QUANTI8`, `DEQUANTI8`, `REQUANTI8`, `QUANTPARAMS`, `QUANTGEMMU8`, `QUANTREQSHIFTI8`, `QUANTCONV2DI8`, `QUANTCLAMPU8`, `QUANTPARAMSCHANNEL`, `QUANTCONV2DI8CHANNEL` | array a byte come `LONG`; firme naturali a 13/14/15 argomenti (la FFI ne regge 20) |
| Ternario | `TERNARYINIT/FREE/ACTBYTES/DOTROW/FORWARD/TOMAT` | bit-slicing 1.58-bit, handle 2001+ |
| Alberi | `TREEINIT/TREEFITREG/TREEFITCLS/TREEPREDICT/TREEFREE` | CART, handle 3001+ |
| Random Forest | `FORESTINIT/FORESTFITREG/FORESTFITCLS/FORESTPREDICT/FORESTFREE`, `AUC` | bagging deterministico (seed), handle 3101+ |
| AdaBoost | `ADABOOSTINIT/ADABOOSTFIT/ADABOOSTPREDICT/ADABOOSTFREE` | SAMME binario, handle 3201+ |
| SNN / BNN | `SNNSURROGATE[GRAD]`, `SNNLUTINDEX`; `BNNBYTES/WORDS/PACK/THRESH/XNOR/DOT` | BNN: pacchetti 64-bit in array `LONG` |
| PINN | `PINNHEAT/WAVE/LAPLACE/ADVECTION/BURGERS/BOUNDARY/INITIAL/ADAPTIVENORM` | residui quadratici |
| Detection | `IOU`, `NMS`, `YOLOBOXCFG/YOLOBOX/YOLOBOXFREE`, `NMSMULTICLASS` | bbox x,y,w,h; cfg handle 4001+ |
| KNN | `KNNINIT/KNNCLASSIFY/KNNACCURACY/KNNFREE` | handle 4101+, copia il training set |
| Metriche | `CONFUSION`, `ACCURACY[CM]`, `PRECISION/RECALL/F1`, `MSE/MAE/R2` | macro come ritorno, per-classe in `out()` |
| Memplan | `MEMPLAN` | offset per lifetime, ritorna la dimensione totale |
| Attivazioni | `*GRAD` (sigmoid/tanh/relu/gelu), `LEAKYRELU`, `ELU`, `SELU`, `SILU`, `MISH`, `PLSE`, `LOGGY`, `HARDTAN`, `LHTAN`, `RELIE`, `RAMP`, `STAIR`, `IDENTITY`, `THRESHOLD`, `HARD*`, `CELU`, `PRELU`, `CRELU` (+ `*GRAD`) | valori RELU/SIGMOID/TANH/GELU già in TORCH: qui solo i gradienti |
| Strato denso | `DENSEINIT/FORWARD/BACKWARD/PARAMS/SETPARAMS/ZEROGRAD/STEP/FREE` | handle 5001+; SGD/momentum/Adam |
| Perdite | `LOSSMSE/LOSSMAE/LOSSCROSSENTROPY/LOSSBINARYCROSSENTROPY` | valore in `loss(0)`, gradiente in `grad()` |
| Dropout | `DROPOUTINIT/FORWARD/BACKWARD/FREE` | handle 6001+, maschera interna |
| BatchNorm / LayerNorm | `BNINIT/FORWARD/BACKWARD/PARAMS/SETPARAMS/ZEROGRAD/FREE`, `LN*` | handle 6101+/6201+, gamma/beta e statistiche correnti |
| Chuck | `CHUCKINIT/LOSS/STEP/STATE/REST/FREE` | Adam con dampening, rumore su stallo, freeze — handle 6301+ |
| Autodiff | `AUTOVALUE`, `AUTO{ADD,SUB,MUL,DIV,POW,NEG,RELU,TANH,EXP}`, `AUTODATA/GRAD/BACKWARD/FREE` | grafo scalare (micrograd), handle 7001+ |
| GP | `GPINIT/GPEVOLVE/GPEVAL/GPFREE` | alfabeto codes/arity + fitness `FUNCTION(code() AS DOUBLE) AS DOUBLE`, handle 7101+ |
| Normalizzazione online | `NORMINIT/OBSERVE/FINALIZE/APPLY/DENORM/ZSCORE/STATS/COUNT/FREE` | scaler addestrato e riusabile, handle 7201+ |
| Righe/matrici | `GATHERROWS`, `SLICEROWS`, `COPYBLOCK`, `MATAPPLY`, `MATABSMAX` | il resto (concat, matmul...) resta in TORCH |

Convenzioni: `x()` = T×in, `y()`/`gy()` = T×out, DOUBLE row-major; handle LONG > 0;
`STEP` usa i gradienti accumulati da `BACKWARD` (buffer m/v allocati al primo passo,
Adam con contatore di passo globale). Tabella handle: 32 vivi, uso sequenziale.

### Perché `PBPLAYERINIT` + `PBPLAYER`

`PBPLAYERINIT` copia `m_w/v_w` (out×in) in un handle, `PBPLAYER` riceve solo i
vettori di stato del passo: i buffer non si ricreano a ogni forward.

## Sorgenti (copia autonoma)

| file | origine |
|---|---|
| `src/gn_*.c` + header (core, act, opt, rnn, lstm, gru, birnn, pbp, conv, fusedmm, prune, quant, ternary, tree, forest, adaboost, snn, bnn, pinn, detect, knn, metrics, memplan, dropout, normlayer, chuck, dense, loss, autograd, gp) | `gmbsuite/gmbneural` (copia identica) |
| `src/gc_{gemm,array}.c` + header | `shared/libs/math` (gmbcompute) |
| `src/nn_ffi.c` | shim ABI di questo pacchetto (handle, array piatti, STEP) |

## Build e test

```sh
shared/libs/neural/build.sh
GMB_MINI_ONLY="neural/neural_recurrent.bas" tools/sh/test_minitests_c99native
zsh tools/sh/test_minitests_parity.sh "neural/neural_recurrent.bas"
```

## Copertura completa di gmbneural

Portati (228 export `ofx_nn_*`): RNN/LSTM/GRU/BiRNN/PBP, CNN (conv/deconv/winograd/
im2col/col2im/LRN/resize), pooling, FusedMM/aggregate, prune, quant, ternary,
CART/forest/adaboost, SNN/BNN/PINN/detection/KNN/metrics/memplan, attivazioni e
gradienti, strato denso, perdite, dropout, batchnorm/layernorm, Chuck, autograd, GP.

NON esposti perché già coperti dalla superficie BASIC (nessun duplicato):

| modulo | coperto da |
|---|---|
| `gn_core` (matmul/add/scale) | `TORCH` MATMUL/ADD/MUL/SUB/DIV — `apply`/`absmax` sono in NEURAL |
| `gn_ops` (concat righe) | `TORCH` CONCAT/STACK — gather/slice/block sono in NEURAL |
| `gn_preprocess` (standardize/minmax/onehot) | `TORCH` STANDARDIZE/NORMALIZE/ONEHOT |
| `gn_net` (MLP con save/load/merge) | `TORCH` MODELCREATE/MODELFORWARD/MODELTRAINSTEP/MODELSAVE |
| `gn_kmeans`/`gn_pca`/`gn_svm` | `MULTIVARIATE` (FFI, libgmbstats) |
| `gn_opt` (SGD/adam grezzi) | esposti tramite `STEP` di RNN/DENSE (stessi parametri m/v) |
