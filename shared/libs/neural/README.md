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
| CNN | `CONVINIT/CONVSETDIL/CONVRUN[R B]`, `DECONV*`, `WINO*`, `IM2COL*`, `COL2IM*`, `LRN*`, `RESIZE*` | layout CHW; configurazione su handle (le funzioni C superano gli 8 argomenti FFI) |
| Pooling | `MAXPOOLINIT`, `AVGPOOLINIT`, `POOLRUN`, `POOLRUNARG`, `UNPOOLINIT`, `UNPOOLRUN` | argmax LONG per il backward |
| FusedMM | `FUSEDMMINIT/RUN`, `AGGINIT/RUN` | CSR (GAT/GraphSAGE) |

Convenzioni: `x()` = T×in, `y()`/`gy()` = T×out, DOUBLE row-major; handle LONG > 0;
`STEP` usa i gradienti accumulati da `BACKWARD` (buffer m/v allocati al primo passo,
Adam con contatore di passo globale). Tabella handle: 32 vivi, uso sequenziale.

### Perché `PBPLAYERINIT` + `PBPLAYER`

La funzione C `gn_pbp_layer_forward` ha 10 argomenti; il marshalling FFI ne supporta 8.
`PBPLAYERINIT` copia `m_w/v_w` (out×in) in un handle, `PBPLAYER` prende i 7 argomenti
rimanenti.

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
