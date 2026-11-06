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

Convenzioni: `x()` = T×in, `y()`/`gy()` = T×out, DOUBLE row-major; handle LONG > 0;
`STEP` usa i gradienti accumulati da `BACKWARD` (buffer m/v allocati al primo passo,
Adam con contatore di passo globale). Tabella handle: 32 vivi, uso sequenziale.

### Perché `PBPLAYERINIT` + `PBPLAYER`

`PBPLAYERINIT` copia `m_w/v_w` (out×in) in un handle, `PBPLAYER` riceve solo i
vettori di stato del passo: i buffer non si ricreano a ogni forward.

## Sorgenti (copia autonoma)

| file | origine |
|---|---|
| `src/gn_{core,act,opt,rnn,lstm,gru,birnn,pbp}.c` + header | `gmbsuite/gmbneural` (copia identica) |
| `src/gc_{gemm,array}.c` + header | `shared/libs/math` (gmbcompute) |
| `src/nn_ffi.c` | shim ABI di questo pacchetto (handle, array piatti, STEP) |

## Build e test

```sh
shared/libs/neural/build.sh
GMB_MINI_ONLY="neural/neural_recurrent.bas" tools/sh/test_minitests_c99native
zsh tools/sh/test_minitests_parity.sh "neural/neural_recurrent.bas"
```

## Prossime slice (non ancora qui)

Conv2d/pool/winograd/fusedmm · prune/quant/ternary · tree/forest/adaboost ·
gp/snn/bnn/pinn/detect/knn/metrics/memplan/chuck/autograd/norm.
