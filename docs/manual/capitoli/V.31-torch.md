# V.31 — TORCH

## Cosa ottieni da questo capitolo

Tensori: creare, sommare, moltiplicare, applicare funzioni, passare a un dataset. TORCH è il
framework tensoriale del progetto e il substrato su cui girano i capitoli V.32-V.35.

## 1. Device e loader

```
== TORCH: device e loader ==
device  =CPU
loaded  = 1
libpath =.../libs/ml/osx-arm64/libgmb_torch.dylib
```

Eseguito (`shared/samples/Torch/TorchBasics.bas`, backend C99Native). `loaded= 1` dice che la
libreria nativa è stata caricata; `libpath` è il percorso reale, dalla cache `~/.gmbasic/libs/`.
Il device è `CPU` (su macOS esiste anche il backend MLX per l'accelerazione).

Se `loaded` è `0`, il problema non è il tuo codice: è la libreria — la stessa catena di build che
il progetto garantisce con `tools/sh/gmb_ensure_ai_libs.sh`.

## 2. Costruttori

```
zeros sum = 0   ones sum = 4   eye sum = 2
== TORCH: FROM e aritmetica ==
a        = 1  2  3  4
a+b      = 6  8  10  12
a-b      =-4 -4 -4 -4
a*b      = 5  12  21  32
a/b      = 0.2  0.333333333333333  0.428571428571429  0.5
sum/mean = 10 / 2.5
max/min  = 4 / 1
```

`zeros`, `ones`, `eye` (identità), `FROM` per costruire da valori. L'aritmetica è **elemento per
elemento**: `a+b` con `a=1,2,3,4` e `b=5,6,7,8` dà `6,8,10,12`. Riduzioni: `sum`, `mean`, `max`, `min`.

## 3. Matmul

```
== TORCH: matmul 2x2 ==
tr*tr    = 7  10  15  22
```

`tr` è la matrice `1 2 / 3 4`; `tr*tr` (prodotto matriciale) dà `7 10 15 22`, il risultato corretto
(`1*1+2*3=7`, `1*2+2*4=10`, `3*1+4*3=15`, `3*2+4*4=22`). Verifica a mano fatta: questa parte è
affidabile.

## 4. Attivazioni: due cose da sapere

```
== TORCH: attivazioni ==
relu(-a) =NaN NaN NaN NaN
sigmoid  = 3.54644357346823
tanh     = 3.72000578945738
gelu     = 9.79166810310996
sqrt     = 6.14626436994197
```

Qui l'output reale mostra **due anomalie** che il libro non nasconde:

1. `relu(-a)` su valori negativi dovrebbe dare `0 0 0 0`, e dà `NaN`. Una `relu` che produce `NaN`
   inquina qualunque rete a valle;
2. `sigmoid` di un vettore di quattro elementi restituisce **un numero** (`3.546…`), non un
   vettore fra 0 e 1: sembra una riduzione applicata per errore.

Non sono errori di chi legge il libro, e non sono sicuramente errori di questo capitolo: sono
difetti (o divergenze fra backend) del campione, osservati sul backend nativo. Vanno verificati su
IL/PCodeVM e, finché non lo sono, `relu` e `sigmoid` **non vanno usate in un modello reale** senza
un controllo a mano.

## 5. Accesso e confronto

```
get(0,0) = 1
allclose =-1
```

`get(riga, colonna)` legge un elemento; `allclose` confronta due tensori con tolleranza e ritorna
vero (`-1`). È il modo corretto di confrontare risultati float: mai `=`, sempre tolleranza.

## 6. Il resto del catalogo

| Sample | Cosa copre |
|---|---|
| `TorchArithmetic.bas` | operatori e riduzioni |
| `TorchAll.bas`, `TorchCommands.bas` | superficie completa dei comandi |
| `TorchDatasetChar.bas` | dataset per carattere (tokenizzazione) |
| `TorchLoRA.bas`, `TorchDpoToy.bas`, `TorchDistill.bas` | training, LoRA, distillazione |
| `TorchEvalBleuRouge.bas` | metriche di valutazione |
| `TextMiniLM.bas` | modello testuale piccolo |

Attenzione al confine di licenza: `TORCH` **usa** i tensori, ma tutto ciò che **produce o
trasforma pesi** (training, LoRA, quantizzazione) sta nel namespace `TRAIN` e nell'implementazione
`gmbllm` (licenza PUL). Il manuale lo ripete in V.35.

## 7. Errori frequenti

| Sintomo | Causa | Correzione |
|---|---|---|
| `loaded= 0` | libreria nativa assente | ricostruisci le lib AI |
| `NaN` dopo un'operazione | dominio non valido o difetto del backend | verifica l'operazione su un input noto |
| confronto fra tensori fallisce | uguaglianza esatta | usa `allclose` |
| shape mismatch | dimensioni incompatibili | stampa le shape prima dell'operazione |
| memoria esaurita | tensore troppo grande per la RAM | usa blocchi o il device giusto |

## 8. Dove andare adesso

- **V.32** — i modelli che usano questi tensori.
- **V.35** — compilare, quantizzare e caricare un LLM.
- **V.27** — l'algebra lineare che sta sotto `matmul`.














<!-- BEGIN SURFACE (generato da tools/sh/manual_surface.sh — non modificare a mano) -->

## Riferimento completo: comandi, funzioni, parametri

> Sezione generata dalle guide del progetto per questo capitolo: contiene **tutte**
> le superfici documentate, non una selezione. La prosa dei capitoli precedenti le
> spiega; qui stanno i dettagli — parametri, varianti, esempi d'uso.

### Torch-Manual

*Fonte: `Torch-Manual`*


## 1. Prerequisiti e setup

gmb setup TORCH         # CLI, con progress bar (CPU per default)
# oppure nel REPL:     SETUP TORCH
# stato:               gmb ml status / ML STATUS
# rimozione:           gmb ml rm   / ML RM

| Var | Valori | Effetto |
|---|---|---|
| `GMB_ML_DEVICE` | `cpu` \| `cuda` \| `rocm` \| `mlx` \| `metal` | Device attivo al load (default auto-detect) |
| `GMB_TORCH_PATH` | percorso dir | Override directory delle librerie (equivalente a `TORCH SET LIB PATH`) |
| `GMBASIC_VM_HOST` | percorso `gmb_vm_host` | Usa il backend VM/C |

gmb shared/samples/Torch/TorchBasics.bas              # backend VM/C (default)
gmb --pcode shared/samples/Torch/TorchBasics.bas      # PCodeVM managed (C#)


## 2. Regola sintattica: comandi vs funzioni
| | COMANDO (scrive, NON ritorna valori) | FUNZIONE (legge, ritorna sempre un valore) |
|---|---|---|
| Sintassi | `TORCH VERBO ...` (spazio, MAI parentesi) | `TORCH.XXX(...)` (parentesi obbligatorie) |
| Esempio | `TORCH SET DEVICE "cpu"` | `dev$ = TORCH.DEVICE()` |

## 3. Comandi legacy (sintassi a spazio, nessun valore di ritorno)

### `TORCH INFO`

IMPORT TORCH
SUB Main()
  TORCH INFO
END SUB


TORCH INFO
  device attivo : 0
  disponibili   : cpu,metal
  lib path      : /Users/<utente>/.gmbasic/ml/osx-arm64/libgmb_torch.dylib
  caricata      : si


### `TORCH LOAD LIB`

TORCH LOAD LIB
PRINT "caricata ="; TORCH.LOADED()   ' -> 1


### `TORCH UNLOAD LIB`

TORCH UNLOAD LIB
PRINT "caricata ="; TORCH.LOADED()   ' -> 0


### `TORCH SET DEVICE "nome"`

TORCH SET DEVICE "cpu"
PRINT "device ="; TORCH.DEVICE()   ' -> cpu


### `TORCH SET LIB PATH "dir"`

TORCH SET LIB PATH "/opt/libtorch"


## 4. Funzioni — device e loader
| Funzione | Sintassi | Ritorna |
|---|---|---|
| `DEVICE` | `TORCH.DEVICE()` | String — device attivo (`cpu`/`cuda`/`rocm`/`metal`) |
| `LIBPATH` | `TORCH.LIBPATH()` | String — path assoluto della lib caricata |
| `LOADED` | `TORCH.LOADED()` | Long — `1` se la lib è caricata, `0` altrimenti (senza forzare il load) |
| `AVAILABLE` | `TORCH.AVAILABLE()` | String — lista device disponibili separata da virgola (`cpu,metal`) |

IMPORT TORCH
SUB Main()
  PRINT TORCH.DEVICE()          ' cpu
  PRINT TORCH.LIBPATH()         ' ~/.gmbasic/libs/ml/osx-arm64/libgmb_torch.dylib
  PRINT TORCH.LOADED()          ' 1
  PRINT TORCH.AVAILABLE()       ' cpu,metal
END SUB


## 5. Funzioni — tensori

### 5.1 Costruttori
| Funzione | Sintassi | Descrizione |
|---|---|---|
| `CREATE` | `TORCH.CREATE(rows, cols)` | Tensore allocato (valori non inizializzati) |
| `FROM` | `TORCH.FROM(array)` | Tensore `1×N` da un array `DOUBLE[]` |
| `ZEROS` | `TORCH.ZEROS(rows, cols)` | Tensore di zeri |
| `ONES` | `TORCH.ONES(rows, cols)` | Tensore di uni |
| `EYE` | `TORCH.EYE(n)` | Matrice identità `n×n` |
| `RANDN` | `TORCH.RANDN(rows, cols)` | Tensore gaussiano (standard normal) |

IMPORT TORCH
SUB Main()
  z = TORCH.ZEROS(2, 2)
  o = TORCH.ONES(2, 2)
  e = TORCH.EYE(3)
  r = TORCH.RANDN(2, 2)
  PRINT TORCH.SUM(z); TORCH.SUM(o); TORCH.SUM(e); TORCH.SUM(r)
END SUB


### 5.2 Aritmetica
| Funzione | Sintassi | Descrizione |
|---|---|---|
| `ADD` | `TORCH.ADD(a, b)` | Somma element-wise |
| `SUB` | `TORCH.SUB(a, b)` | Differenza element-wise |
| `MUL` | `TORCH.MUL(a, b)` | Prodotto element-wise |
| `DIV` | `TORCH.DIV(a, b)` | Divisione element-wise |
| `MATMUL` | `TORCH.MATMUL(a, b)` | Prodotto matriciale |

IMPORT TORCH
SUB Main()
  DIM a(3) AS DOUBLE = {1, 2, 3, 4}
  DIM b(3) AS DOUBLE = {5, 6, 7, 8}
  ta = TORCH.FROM(a) : tb = TORCH.FROM(b)
  DIM s() = TORCH.TOARRAY(TORCH.ADD(ta, tb))   ' 6  8 10 12
  DIM p() = TORCH.TOARRAY(TORCH.MUL(ta, tb))   ' 5 12 21 32
  tr = TORCH.RESHAPE(ta, 2, 2)
  mm = TORCH.MATMUL(tr, tr)                     ' 1x2 per 2x2
END SUB


### 5.3 Attivazioni
| Funzione | Sintassi | Descrizione |
|---|---|---|
| `RELU` | `TORCH.RELU(h)` | max(0, x) |
| `SIGMOID` | `TORCH.SIGMOID(h)` | 1/(1+e^-x) |
| `TANH` | `TORCH.TANH(h)` | Tangente iperbolica |
| `SOFTMAX` | `TORCH.SOFTMAX(h)` | Softmax |
| `GELU` | `TORCH.GELU(h)` | GELU (errf approx) |

### 5.4 Funzioni puntuali
| Funzione | Sintassi | Descrizione |
|---|---|---|
| `EXP` | `TORCH.EXP(h)` | Esponenziale |
| `LOG` | `TORCH.LOG(h)` | Logaritmo naturale |
| `SQRT` | `TORCH.SQRT(h)` | Radice quadrata |
| `POW` | `TORCH.POW(h, p)` | Potenza (scalare) |
| `ABS` | `TORCH.ABS(h)` | Valore assoluto |

IMPORT TORCH
SUB Main()
  DIM v(3) AS DOUBLE = {1, -2, 3, -4}
  t = TORCH.FROM(v)
  PRINT TORCH.SUM(TORCH.RELU(t))      ' 4 (1+0+3+0)
  PRINT TORCH.SUM(TORCH.ABS(t))       ' 10
  PRINT TORCH.SUM(TORCH.POW(t, 2))    ' 30
  PRINT TORCH.SUM(TORCH.LOG(TORCH.EXP(t)))  ' log(exp)=identità
END SUB


### 5.5 Shape ops
| Funzione | Sintassi | Descrizione |
|---|---|---|
| `RESHAPE` | `TORCH.RESHAPE(h, rows, cols)` | Ridimensiona (niente copy) |
| `TRANSPOSE` | `TORCH.TRANSPOSE(h)` | Trasposta |
| `PERMUTE` | `TORCH.PERMUTE(h, d0, d1)` | Permuta le due dimensioni |
| `SLICE` | `TORCH.SLICE(h, dim, start, end)` | Sotto-tensore lungo una dim |
| `CONCAT` | `TORCH.CONCAT(a, b)` | Concatena lungo la dim 0 |
| `STACK` | `TORCH.STACK(a, b)` | Stack (nuova dim) |
| `SPLIT` | `TORCH.SPLIT(h, index, n)` | Estrae l'`index`-esima parte di `n` |

### 5.6 Riduzioni
| Funzione | Sintassi | Ritorna |
|---|---|---|
| `SUM` | `TORCH.SUM(h)` | Double — somma di tutti gli elementi |
| `MEAN` | `TORCH.MEAN(h)` | Double — media |
| `MAX` | `TORCH.MAX(h)` | Double — massimo |
| `MIN` | `TORCH.MIN(h)` | Double — minimo |

### 5.7 Accesso, conversione, confronto, memoria
| Funzione | Sintassi | Descrizione |
|---|---|---|
| `GET` | `TORCH.GET(h, row, col)` | Double — elemento |
| `SET` | `TORCH.SET(h, row, col, value)` | Imposta un elemento |
| `TOARRAY` | `TORCH.TOARRAY(h)` | `DOUBLE[]` — copia piatta del tensore |
| `SHAPE` | `TORCH.SHAPE(h)` | Array — `[rows, cols]` |
| `ALLCLOSE` | `TORCH.ALLCLOSE(a, b, tol)` | Long — `1` se identici entro tolleranza |
| `FREE` | `TORCH.FREE(h)` | Libera il tensore |

IMPORT TORCH
SUB Main()
  DIM v(3) AS DOUBLE = {1, 2, 3, 4}
  t = TORCH.FROM(v)
  PRINT TORCH.GET(t, 0, 2)          ' 3
  TORCH.SET(t, 0, 0, 99)
  PRINT TORCH.GET(t, 0, 0)          ' 99
  DIM arr() = TORCH.TOARRAY(t)
  DIM sh() = TORCH.SHAPE(t)         ' [1, 4]
  PRINT TORCH.ALLCLOSE(t, t, 0.000000001)  ' 1
  TORCH.FREE(t)
END SUB


### 5.8 Info tensore
| Funzione | Sintassi | Ritorna |
|---|---|---|
| `DTYPE` | `TORCH.DTYPE(h)` | String — tipo elementi (`double`) |
| `DEVICEOF` | `TORCH.DEVICEOF(h)` | String — device del tensore (`cpu`) |

## 6. Funzioni — modelli e addestramento

### `MODELCREATE(arch, cfgJson)` → Long

m = TORCH.MODELCREATE("linear", "{""in"":2,""out"":1}")   ' linear 2 -> 1


### `MODELCREATE("mlp", cfgJson)` → Long — MLP con attivazioni per strato

| Chiave | Tipo | Default | Significato |
|---|---|---|---|
| `in` | numero | — | dimensione d'ingresso |
| `hidden` | lista | `[8]` | dimensioni degli strati nascosti |
| `out` | numero | — | dimensione d'uscita |
| `acts` | stringa | `"relu"` | attivazione degli strati nascosti: una per tutti (`"tanh"`) o lista (`"tanh,relu"`); valori `relu`, `tanh`, `sigmoid`, `leaky`, `linear`/`none` |

```basic
m = TORCH.MODELCREATE("mlp", "{""in"":4,""hidden"":[8,6],""out"":2,""acts"":""tanh,relu""}")
' 4->8 + 8->6 + 6->2 = 108 parametri
```

`acts` copre la scelta per-layer di `gmbneural` (`gn_net`) senza una seconda API di modelli:
copy/merge dei pesi sono composizione di `DENSEPARAMS`/`DENSESETPARAMS`, save/load sono
`MODELSAVE`/`MODELLOAD`.


### `MODELCREATE("lora", cfgJson)` → Long — iperparametri LoRA (L1)
| Chiave | Tipo | Default | Significato |
|---|---|---|---|
| `alpha` | numero | `rank*2` | Scala del contributo adattatori: `alpha/rank` (`linear`) o `alpha/√rank` (`rs`) |
| `dropout` | numero | `0` | Dropout sugli adattatori, solo in training; deve stare in `[0,1)` |
| `target` | stringa | `"all"` | Quali proiezioni adattare: sottoinsieme di `q,k,v,o,ffn` (`ffn` = `f1`+`f2`) separato da virgole, oppure `"all"` |
| `scaling` | stringa | `"linear"` | `"linear"` (alpha/rank) o `"rs"` (rsLoRA: alpha/√rank) |
| `lr_ratio` | numero | `1.0` | L3a LoRA+: lr(B) = lr(A)×ratio (2 gruppi ottimizzatore); ≥0 |
| `dora` | numero | `0` | L3b DoRA (1 = magnitudine+direzione); il SAVE fonde tutto, ckpt invariato |
| `pissa` | numero | `0` | L3c PiSSA (1 = init A/B da SVD di W, solo TORCH) |
| `ranks` | lista | — | L11: rank LoRA **per-layer** (es. `[4,8,16]`); sostituisce il `rank` scalare, `alpha` resta quello |
| `rank_path` | stringa | — | L11: rank per-layer **dall'imatrix** (con `rank_min`/`rank_max`); ignorato se `ranks` è presente |

l = TORCH.MODELCREATE("lora", "{""vocab"":8,""d_model"":32,""rank"":4,""alpha"":16,""target"":""q,k,v,o""}")


### `MODELCREATE("freeze", cfgJson)` → Long — freeze tuning (L4)

f = TORCH.MODELCREATE("freeze", "{""vocab"":8,""d_model"":16,""n_layers"":2,""freeze_layers"":1}")


### `MODELTRAINSTEP(model, x, y, optimizer, loss, lr)`

### `MODELTRAINSTEP(model, x, y, optimizer, loss, lr [, opts$])` — scheduler e dintorni (L2)
| Chiave | Default | Significato |
|---|---|---|
| `scheduler` | `"const"` | `"const"` (lr fisso) \| `"linear"` (decadimento lineare) \| `"cosine"` (warmup poi coseno). Valore ignoto = errore esplicito |
| `warmup_steps` | `0` | Rampa lineare 0→lr nei primi N optimizer-step |
| `decay_steps` | `200` | Orizzonte del decadimento (solo linear/cosine) |
| `accum_steps` | `1` | Accumula i gradienti su N micro-batch prima dello step (1 = come prima) |
| `clip_norm` | `0` | Clip del gradiente (0 = disattivo) |
| `resume_ckpt` | `""` | Path di un file `{"sched_step":N}`: lo schedule riparte da lì (solo contatore, i momenti ripartono — limite documentato); salvato a ogni step |

opts$ = "{""scheduler"":""cosine"",""warmup_steps"":20,""decay_steps"":200}"
FOR i = 1 TO 200
  TORCH.MODELTRAINSTEP(m, xa, ya, "adamw", "crossentropy", 0.05, opts$)
NEXT


### `MODELDPOSTEP(mStudent, mRef, x, chosen, rejected [, opts$])` → Double — preference (L5)

FOR i = 1 TO 80
  TORCH.MODELDPOSTEP(ms, mr, tx, tc, tr, "{""beta"":0.5}")
NEXT


### `MODELLOSS(model, x, y)` → Double

### `MODELFORWARD(model, input)` → Long

### `MODELPARAMETERS(model)` → Long

### `MODELSTATE(model)` → String

### `MODELFREE(model)`

### `MODELSAVE(model, path)` / `MODELLOAD(model, path)` / `MODELRESET(model)`

Il salvataggio è un **tensore flat** con la sequenza dei parametri; non contiene l'architettura.
`MODELLOAD` ricopia quel tensore nei parametri di un modello creato con la stessa architettura
e le stesse dimensioni, quindi il flusso è sempre `MODELCREATE` → `MODELLOAD` (non esiste un
"apri modello dal file"). Ritorna `0` se il caricamento riesce, un valore diverso da `0` se
l'handle non è valido, il file non è leggibile o il numero di parametri non coincide. Dopo il
caricamento il forward è identico a quello del modello di origine; `MODELRESET` azzera
ottimizzatore ed epochs.

```basic
m1 = TORCH.MODELCREATE("linear", "{""in"":2,""out"":1}")
' ... training ...
TORCH.MODELSAVE(m1, "/tmp/ckpt.pt")
m2 = TORCH.MODELCREATE("linear", "{""in"":2,""out"":1}")
PRINT TORCH.MODELLOAD(m2, "/tmp/ckpt.pt")      ' 0
```

### `MODELINPUT(model)` / `MODELOUTPUT(model)` → Long

### Esempio completo (training 2→1, Adam/MSE)

IMPORT TORCH

SUB Main()
  ' modello lineare 2 -> 1
  m = TORCH.MODELCREATE("linear", "{""in"":2,""out"":1}")
  PRINT "params ="; TORCH.MODELPARAMETERS(m)
  PRINT "in/out ="; TORCH.MODELINPUT(m); "/"; TORCH.MODELOUTPUT(m)

  ' dati: x = [[0,0],[1,1],[2,2],[3,3]], y = [0,1,2,3]
  DIM xd(7) AS DOUBLE = {0,0, 1,1, 2,2, 3,3}
  DIM yd(3) AS DOUBLE = {0, 1, 2, 3}
  x = TORCH.RESHAPE(TORCH.FROM(xd), 4, 2)
  y = TORCH.RESHAPE(TORCH.FROM(yd), 4, 1)

  ' training: 500 passi Adam/MSE, lr 0.05
  PRINT "loss iniziale ="; TORCH.MODELLOSS(m, x, y)


## 7. Funzioni — loss e accuratezza
| Funzione | Sintassi | Ritorna |
|---|---|---|
| `LOSSMSE` | `TORCH.LOSSMSE(y, yhat)` | Double — errore quadratico medio |
| `LOSSCROSSENTROPY` | `TORCH.LOSSCROSSENTROPY(y, yhat)` | Double — cross-entropy |
| `LOSSBINARY` | `TORCH.LOSSBINARY(y, yhat)` | Double — loss binaria |
| `ACCURACY` | `TORCH.ACCURACY(y, yhat)` | Double — accuratezza (0..1) |

IMPORT TORCH
SUB Main()
  DIM yv(3) AS DOUBLE = {0, 1, 2, 3}
  DIM pv(3) AS DOUBLE = {0, 1, 2, 3}
  y = TORCH.RESHAPE(TORCH.FROM(yv), 4, 1)
  p = TORCH.RESHAPE(TORCH.FROM(pv), 4, 1)
  PRINT "mse     ="; TORCH.LOSSMSE(y, p)           ' 0 (identici)
  PRINT "acc     ="; TORCH.ACCURACY(y, p)          ' 1
END SUB


## 8. Funzioni bonus (piano ML §3.6)
| Funzione | Sintassi | Descrizione |
|---|---|---|
| `TOPK` | `TORCH.TOPK(h, k)` | I `k` valori più grandi → `DOUBLE[]` |
| `ARGSORT` | `TORCH.ARGSORT(h)` | Indici che ordinano il tensore → `DOUBLE[]` |
| `ONEHOT` | `TORCH.ONEHOT(idx, n)` | Vettore one-hot di lunghezza `n` → `Long` (tensore) |
| `CLIP` | `TORCH.CLIP(h, lo, hi)` | Limita i valori in `[lo, hi]` → `Long` |
| `NORMALIZE` | `TORCH.NORMALIZE(h)` | Min-max in `[0,1]` → `Long` |
| `STANDARDIZE` | `TORCH.STANDARDIZE(h)` | Z-score → `Long` |
| `SIMILARITY` | `TORCH.SIMILARITY(a, b, metric)` | Similarità → Double (`metric` ∈ `"cosine"` `"euclid"`) |
| `BLEU` | `TORCH.BLEU(ref$, hyp$)` | BLEU-4 → Double (0..1): precisione n-grammi con brevity penalty |
| `ROUGE` | `TORCH.ROUGE(ref$, hyp$)` | ROUGE-L → Double (0..1): F-measure su longest common subsequence |
| `TEXTENCODE` | `TORCH.TEXTENCODE(text$)` | Testo → tensore `[1,n]` di id (char-level toy, byte 0..255) |
| `TEXTDECODE` | `TORCH.TEXTDECODE(h)` | Tensore di id → stringa |
| `DATASETLOAD` | `TORCH.DATASETLOAD(path$ [, opts$])` | File di testo (una riga = sample) → tensore `[n_sample, cutoff_len]`; opts: `cutoff_len` (def 64), `max_samples` (def 0 = tutte) |
| `MODELEXPORT` | `TORCH.MODELEXPORT(h, path$ [, format$])` | Esporta (LoRA: **fonde** gli adattatori) → `0` ok / ≠0 errore; `format` solo `"flat"` (default), altro = errore |
| `IMATRIXLOAD` | `TORCH.IMATRIXLOAD(path$)` | Legge un file imatrix → tensore `[n_layer, 1]` di energia media per-layer (sensibilità) |
| `IMATRIXRANKS` | `TORCH.IMATRIXRANKS(path$, min_rank, max_rank)` | Mappa l'energia per-layer in rank LoRA, normalizzato in `[min_rank, max_rank]` → tensore `[n_layer, 1]` |

IMPORT TORCH
SUB Main()
  PRINT TORCH.BLEU("the cat sat on the mat", "the cat sat on the mat")   ' 1 (identici)
  PRINT TORCH.BLEU("a b c d e f", "a b c d")                             ' 0.60653 (brevity)
  PRINT TORCH.ROUGE("a b c d e f", "a b c d")                            ' 0.8
END SUB


IMPORT TORCH
SUB Main()
  DIM v(4) AS DOUBLE = {3, 1, 4, 1, 5}
  t = TORCH.FROM(v)
  DIM tk() = TORCH.TOPK(t, 3)        ' 5 4 3
  DIM so() = TORCH.ARGSORT(t)        ' indici ordinati
  DIM oh() = TORCH.ONEHOT(2, 4)      ' 0 0 1 0
  DIM cl() = TORCH.CLIP(t, 2, 4)     ' 3 2 4 2 4
  DIM no() = TORCH.NORMALIZE(t)      ' min-max
  DIM st() = TORCH.STANDARDIZE(t)    ' z-score
  PRINT TORCH.SIMILARITY(t, t, "cosine")   ' 1
END SUB


## 9. Gestione degli errori

[P-CODE ERROR] ... TORCH non caricato: esegui 'gmb ml setup' (CLI) o 'ML SETUP' (REPL)
                per scaricare le librerie ML (~/.gmbasic/libs o ~/.gmbasic/libs/ml)
[VM ERROR]      TORCH non caricato: esegui 'gmb ml setup' o 'ML SETUP'
[GMBASIC]       ERROR 1: TORCH non caricato: esegui 'gmb ml setup' o 'ML SETUP'   (C99)
[TORCH]         TORCH SET DEVICE: device 'cuda' non disponibile o sconosciuto (auto|cpu|cuda|rocm|mlx)


## 10. Esempio completo autonomo

' ============================================================
' TorchCommands.bas — comandi legacy + verifica stato
' ============================================================
IMPORT TORCH

SUB Main()
  PRINT "== TORCH: comandi legacy =="
  TORCH INFO

  TORCH LOAD LIB
  PRINT "loaded dopo LOAD        ="; TORCH.LOADED()

  TORCH SET DEVICE "cpu"
  PRINT "device dopo SET DEVICE  ="; TORCH.DEVICE()

  TORCH UNLOAD LIB


## 11. Limitazioni note

## 12. Riferimenti

### Torch-Guida-Utente

*Fonte: `Torch-Guida-Utente`*


## 0. Prerequisiti

IMPORT TORCH
SUB Main()
    IF NOT TORCH.LOADED() THEN PRINT "esegui SETUP TORCH"
END SUB


## 1. Tensori — costruttori
| Funzione | Descrizione |
|---|---|
| `TORCH.CREATE(rows, cols)` | tensore di zeri (azzerato) |
| `TORCH.ZEROS(rows, cols)` | tensore di zeri |
| `TORCH.ONES(rows, cols)` | tensore di uni |
| `TORCH.EYE(n)` | matrice identità n×n |
| `TORCH.RANDN(rows, cols)` | valori casuali normali |
| `TORCH.FROM(data())` | tensore da un array di double (1×N) |

a = TORCH.ZEROS(2, 3)
b = TORCH.ONES(2, 3)
id = TORCH.EYE(3)


## 2. Accesso e informazione
| Funzione | Descrizione |
|---|---|
| `TORCH.SET(h, i, j, v)` | scrive l'elemento (i,j) |
| `TORCH.GET(h, i, j)` | legge l'elemento (i,j) |
| `TORCH.SHAPE(h)` | `LONG[]` `{rows, cols}` |
| `TORCH.TOARRAY(h)` | `DOUBLE[]` con gli elementi in row-major |
| `TORCH.DTYPE(h)` | tipo ("double", "float", …) |
| `TORCH.DEVICEOF(h)` | device del tensore ("cpu"/"cuda"/"metal") |
| `TORCH.FREE(h)` | libera l'handle |

a = TORCH.ZEROS(2, 2)
TORCH.SET(a, 0, 0, 1.5)
PRINT TORCH.GET(a, 0, 0)          ' 1.5
DIM sh() = TORCH.SHAPE(a)         ' 2  2
DIM flat() = TORCH.TOARRAY(a)


## 3. Aritmetica e attivazioni
| Funzione | Descrizione |
|---|---|
| `TORCH.ADD(a, b)` / `SUB` / `MUL` / `DIV` | element-wise |
| `TORCH.MATMUL(a, b)` | prodotto matriciale |
| `TORCH.RELU(a)` | max(0, x) |
| `TORCH.SIGMOID(a)` / `TANH(a)` / `GELU(a)` | attivazioni |
| `TORCH.SOFTMAX(a)` | softmax |
| `TORCH.EXP(a)` / `LOG(a)` / `SQRT(a)` / `ABS(a)` | puntuali |
| `TORCH.POW(a, p)` | x^p |
| `TORCH.TRANSPOSE(a)` | trasposta |
| `TORCH.RESHAPE(a, rows, cols)` | reshape |

c = TORCH.ADD(a, b)
d = TORCH.MATMUL(a, b)
r = TORCH.RELU(a)


## 4. Riduzioni, confronto e shape ops
| Funzione | Descrizione |
|---|---|
| `TORCH.SUM(h)` / `MEAN(h)` / `MAX(h)` / `MIN(h)` | riduzioni a scalare |
| `TORCH.ALLCLOSE(a, b, tol)` | BIT: confronto entro tolleranza |
| `TORCH.ARGSORT(a)` | `DOUBLE[]` indici ordinati per valore |
| `TORCH.TOPK(a, k)` | `DOUBLE[]` i k valori più grandi |
| `TORCH.ARGMAX(h)` | indice del massimo (vedi §9) |
| `TORCH.SIMILARITY(a, b, metric)` | distanza ("euclidean", "cosine", …) |

## 5. Normalizzazione e pre-processing
| Funzione | Descrizione |
|---|---|
| `TORCH.NORMALIZE(a)` | `DOUBLE[]` min-max in [0,1] |
| `TORCH.STANDARDIZE(a)` | `DOUBLE[]` z-score |
| `TORCH.CLIP(a, lo, hi)` | `DOUBLE[]` valori limitati a [lo,hi] |
| `TORCH.ONEHOT(idx, n)` | `DOUBLE[]` one-hot di lunghezza n |

## 6. Loss e accuracy
| Funzione | Descrizione |
|---|---|
| `TORCH.LOSSMSE(y, yhat)` | errore quadratico medio |
| `TORCH.LOSSCROSSENTROPY(y, yhat)` | cross-entropy |
| `TORCH.LOSSBINARY(y, yhat)` | binary cross-entropy |
| `TORCH.ACCURACY(y, yhat)` | accuratezza (0..1) |

## 7. Modelli
| Funzione | Descrizione |
|---|---|
| `TORCH.MODELCREATE(arch, cfg)` | crea il modello (`"linear"`, `"mlp"`, `"transformer"`, `"lora"`) |
| `TORCH.MODELINPUT(m)` / `MODELOUTPUT(m)` | dimensioni ingresso/uscita |
| `TORCH.MODELPARAMETERS(m)` | numero di parametri |
| `TORCH.MODELFORWARD(m, x)` | forward su un tensore d'ingresso |
| `TORCH.MODELLOSS(m, x, y)` | loss corrente |
| `TORCH.MODELTRAINSTEP(m, x, y, opt, loss, lr)` | un passo di training |
| `TORCH.MODELSTATE(m)` | stato JSON (epochs, loss, device, opt) |
| `TORCH.MODELSAVE(m, path)` / `MODELLOAD(m, path)` / `MODELRESET(m)` | salva i pesi / li ricarica in un modello con la stessa architettura / resetta |
| `TORCH.MODELFREE(m)` | libera l'handle |

IMPORT TORCH
SUB Main()
    m = TORCH.MODELCREATE("linear", "{""in"":2,""out"":1}")
    PRINT TORCH.MODELPARAMETERS(m)      ' 3
    x = TORCH.ZEROS(1, 2)
    TORCH.SET(x, 0, 0, 1.0) : TORCH.SET(x, 0, 1, 2.0)
    y = TORCH.ZEROS(1, 1)
    TORCH.SET(y, 0, 0, 1.0)
    TORCH.MODELTRAINSTEP(m, x, y, "adam", "mse", 0.01)
    TORCH.MODELFREE(m)
END SUB


## 8. Esempi completi
| Sample | Copre |
|---|---|
| `TorchTensors.bas` | costruttori, accesso, aritmetica base, riduzioni, trasposta, normalizzazione, modello |
| `TorchArithmetic.bas` | `SUB/MUL/DIV/POW/ABS/EXP/LOG/SQRT/RELU/SIGMOID/TANH/GELU/SOFTMAX` |
| `TorchShapeAndData.bas` | `EYE/FROM/CREATE/RESHAPE/PERMUTE/SLICE/CONCAT/STACK/SPLIT/ARGSORT/TOPK/ARGMAX/ALLCLOSE/NORMALIZE/STANDARDIZE/CLIP/ONEHOT/RANDN` |
| `TorchModels.bas` | `LOSSMSE/LOSSCROSSENTROPY/LOSSBINARY/ACCURACY`, info e ciclo di vita del modello |

## 9. Stato di copertura

<!-- END SURFACE -->







<!-- BEGIN DOMAIN TABLE -->
## Tavola sinottica del dominio

Ogni riga riporta la firma canonica (dal registry) e il primo uso reale trovato
nel corpus di test e sample `.bas`/`.inc`: dove il corpus non usa il simbolo è dichiarato.

- **TORCH.ABS(x1)** — `PRINT "ABS     ="; TORCH.SUM(TORCH.ABS(TORCH.FROM(neg)))` (shared/samples/Torch/TorchAll.bas:52)
- **TORCH.ACCURACY(x1, x2)** — `PRINT "accuracy="; ROUND(TORCH.ACCURACY(y, p), 6)` (shared/samples/Torch/TorchModels.bas:13)
- **TORCH.ADD(x1, x2)** — `PRINT "ADD     ="; TORCH.SUM(TORCH.ADD(ta, tb))` (shared/samples/Torch/TorchAll.bas:30)
- **TORCH.ALLCLOSE(x1, x2, x3)** — `PRINT "ALLCLOSE  ="; TORCH.ALLCLOSE(ta, ta, 0.000000001)` (shared/samples/Torch/TorchAll.bas:70)
- **TORCH.ARGMAX(x1)** — `PRINT "argmax="; TORCH.ARGMAX(a)` (shared/samples/Torch/TorchShapeAndData.bas:42)
- **TORCH.ARGSORT(x1)** — `DIM as() = TORCH.ARGSORT(a)` (shared/samples/Torch/TorchShapeAndData.bas:38)
- **TORCH.AVAILABLE()** — `PRINT "available="; TORCH.AVAILABLE()` (shared/samples/Torch/TorchModels.bas:19)
- **TORCH.BLEU(x1, x2)** — `PRINT "BLEU  base ="; ROUND(TORCH.BLEU(ref$, pb$), 4)` (shared/samples/Torch/TorchEvalBleuRouge.bas:71)
- **TORCH.CLIP(x1, x2, x3)** — `DIM cl() = TORCH.CLIP(a, 2.0, 4.0)` (shared/samples/Torch/TorchShapeAndData.bas:47)
- **TORCH.CONCAT(x1, x2)** — `PRINT "CONCAT    ="; TORCH.SUM(TORCH.CONCAT(ta, ta))` (shared/samples/Torch/TorchAll.bas:59)
- **TORCH.CREATE(x1, x2)** — `PRINT "CREATE  ="; TORCH.SUM(TORCH.CREATE(2, 2))` (shared/samples/Torch/TorchAll.bas:19)
- **TORCH.DATASETLOAD(x1)** — `DIM ds AS LONG = TORCH.DATASETLOAD(p, "{""cutoff_len"":16}")` (shared/samples/Torch/TorchDatasetChar.bas:18)
- **TORCH.DEVICE()** — `PRINT "DEVICE()  ="; TORCH.DEVICE()` (shared/samples/Torch/TorchAll.bas:13)
- **TORCH.DEVICEOF(x1)** — `PRINT "deviceof="; TORCH.DEVICEOF(t)` (shared/samples/Torch/TorchModels.bas:21)
- **TORCH.DIV(x1, x2)** — `PRINT "DIV     ="; TORCH.SUM(TORCH.DIV(ta, tb))` (shared/samples/Torch/TorchAll.bas:33)
- **TORCH.DTYPE(x1)** — `PRINT "dtype="; TORCH.DTYPE(t)` (shared/samples/Torch/TorchModels.bas:20)
- **TORCH.EXP(x1)** — `PRINT "EXP     ="; TORCH.SUM(TORCH.EXP(ta))` (shared/samples/Torch/TorchAll.bas:47)
- **TORCH.EYE(x1)** — `PRINT "EYE     ="; TORCH.SUM(TORCH.EYE(3))` (shared/samples/Torch/TorchAll.bas:23)
- **TORCH.FREE(x1)** — `TORCH.FREE(x) : TORCH.FREE(y) : TORCH.FREE(f1) : TORCH.FREE(f2)` (shared/samples/Torch/TorchSaveLoad.bas:54)
- **TORCH.FROM(x1)** — `PRINT "FROM    ="; TORCH.SUM(TORCH.FROM({1, 2, 3, 4}))` (shared/samples/Torch/TorchAll.bas:20)
- **TORCH.GELU(x1)** — `PRINT "GELU    ="; TORCH.SUM(TORCH.GELU(ta))` (shared/samples/Torch/TorchAll.bas:43)
- **TORCH.GET(x1, x2, x3)** — `PRINT "GET(0,0)  ="; TORCH.GET(ta, 0, 0)` (shared/samples/Torch/TorchAll.bas:67)
- **TORCH.IMATRIXLOAD(x1)** — `h = TORCH.IMATRIXLOAD(imatrix)` (shared/samples/AiLLM/TorchArtifacts.bas:26)
- **TORCH.IMATRIXRANKS(x1, x2, x3)** — `h = TORCH.IMATRIXRANKS(imatrix, 4, 64)` (shared/samples/AiLLM/TorchArtifacts.bas:27)
- **TORCH.LIBPATH()** — `PRINT "LIBPATH() ="; TORCH.LIBPATH()` (shared/samples/Torch/TorchAll.bas:15)
- **TORCH.LOADED()** — `PRINT "LOADED()  ="; TORCH.LOADED()` (shared/samples/Torch/TorchAll.bas:14)
- **TORCH.LOG(x1)** — `PRINT "LOG     ="; TORCH.SUM(TORCH.LOG(TORCH.EXP(ta)))` (shared/samples/Torch/TorchAll.bas:48)
- **TORCH.LOSSBINARY(x1, x2)** — `PRINT "binary="; ROUND(TORCH.LOSSBINARY(y, p), 6)` (shared/samples/Torch/TorchModels.bas:12)
- **TORCH.LOSSCROSSENTROPY(x1, x2)** — `PRINT "crossentropy="; ROUND(TORCH.LOSSCROSSENTROPY(y, p), 6)` (shared/samples/Torch/TorchModels.bas:11)
- **TORCH.LOSSMSE(x1, x2)** — `PRINT "mse="; ROUND(TORCH.LOSSMSE(y, p), 6)` (shared/samples/Torch/TorchModels.bas:10)
- **TORCH.MATMUL(x1, x2)** — `PRINT "MATMUL  ="; TORCH.SUM(TORCH.MATMUL(m2, m2))` (shared/samples/Torch/TorchAll.bas:35)
- **TORCH.MAX(x1)** — `PRINT "MAX/MIN   ="; TORCH.MAX(ta); "/"; TORCH.MIN(ta)` (shared/samples/Torch/TorchAll.bas:66)
- **TORCH.MEAN(x1)** — `PRINT "SUM/MEAN  ="; TORCH.SUM(ta); "/"; TORCH.MEAN(ta)` (shared/samples/Torch/TorchAll.bas:65)
- **TORCH.MIN(x1)** — `PRINT "MAX/MIN   ="; TORCH.MAX(ta); "/"; TORCH.MIN(ta)` (shared/samples/Torch/TorchAll.bas:66)
- **TORCH.MODELCREATE(x1, x2)** — `mm = TORCH.MODELCREATE("linear", "{""in"":2,""out"":1}")` (shared/samples/Torch/TorchAll.bas:82)
- **TORCH.MODELDPOSTEP(x1, x2, x3, x4, x5)** — `TORCH.MODELDPOSTEP(ms, mr, tx, tc, tr, "{""beta"":0.5}")` (shared/samples/Torch/TorchDpoToy.bas:33)
- **TORCH.MODELEXPORT(x1, x2)** — `h = TORCH.MODELEXPORT(h, "work/example.gmbllm", "gguf")` (shared/samples/AiLLM/TorchArtifacts.bas:28)
- **TORCH.MODELFORWARD(x1, x2)** — `fw = TORCH.MODELFORWARD(mm, x)` (shared/samples/Torch/TorchAll.bas:88)
- **TORCH.MODELFREE(x1)** — `TORCH.MODELFREE(mm)` (shared/samples/Torch/TorchAll.bas:91)
- **TORCH.MODELINPUT(x1)** — `PRINT "in/out/params="; TORCH.MODELINPUT(m); TORCH.MODELOUTPUT(m); TORCH.MODELPARAMETERS(m)` (shared/samples/Torch/TorchModels.bas:26)
- **TORCH.MODELLOAD(x1, x2)** — `PRINT "load="; TORCH.MODELLOAD(m2, p)` (shared/samples/Torch/TorchSaveLoad.bas:32)
- **TORCH.MODELLOSS(x1, x2, x3)** — `PRINT "LOSS(prima)="; TORCH.MODELLOSS(mm, x, y)` (shared/samples/Torch/TorchAll.bas:84)
- **TORCH.MODELOUTPUT(x1)** — `PRINT "in/out/params="; TORCH.MODELINPUT(m); TORCH.MODELOUTPUT(m); TORCH.MODELPARAMETERS(m)` (shared/samples/Torch/TorchModels.bas:26)
- **TORCH.MODELPARAMETERS(x1)** — `PRINT "PARAMETERS ="; TORCH.MODELPARAMETERS(mm)` (shared/samples/Torch/TorchAll.bas:83)
- **TORCH.MODELRESET(x1)** — `PRINT "reset="; TORCH.MODELRESET(m)` (shared/samples/Torch/TorchModels.bas:36)
- **TORCH.MODELSAVE(x1, x2)** — `PRINT "save="; TORCH.MODELSAVE(m1, p)` (shared/samples/Torch/TorchSaveLoad.bas:28)
- **TORCH.MODELSTATE(x1)** — `PRINT "STATE      ="; TORCH.MODELSTATE(mm)` (shared/samples/Torch/TorchAll.bas:90)
- **TORCH.MODELTRAINSTEP(x1, x2, x3, x4, x5, x6)** — `TORCH.MODELTRAINSTEP(mm, x, y, "adam", "mse", 0.05)` (shared/samples/Torch/TorchAll.bas:85)
- **TORCH.MUL(x1, x2)** — `PRINT "MUL     ="; TORCH.SUM(TORCH.MUL(ta, tb))` (shared/samples/Torch/TorchAll.bas:32)
- **TORCH.NORMALIZE(x1)** — `DIM n() = TORCH.NORMALIZE(a)` (shared/samples/Torch/TorchShapeAndData.bas:45)
- **TORCH.ONEHOT(x1, x2)** — `DIM oh() = TORCH.ONEHOT(1, 3)` (shared/samples/Torch/TorchShapeAndData.bas:48)
- **TORCH.ONES(x1, x2)** — `PRINT "ONES    ="; TORCH.SUM(TORCH.ONES(2, 2))` (shared/samples/Torch/TorchAll.bas:22)
- **TORCH.PERMUTE(x1, x2, x3)** — `PRINT "PERMUTE   ="; TORCH.SUM(TORCH.PERMUTE(m2, 1, 0))` (shared/samples/Torch/TorchAll.bas:57)
- **TORCH.POW(x1, x2)** — `PRINT "POW     ="; TORCH.SUM(TORCH.POW(ta, 2))` (shared/samples/Torch/TorchAll.bas:50)
- **TORCH.RANDN(x1, x2)** — `PRINT "RANDN   ="; TORCH.SUM(TORCH.RANDN(2, 2))` (shared/samples/Torch/TorchAll.bas:24)
- **TORCH.RELU(x1)** — `PRINT "RELU    ="; TORCH.SUM(TORCH.RELU(ta))` (shared/samples/Torch/TorchAll.bas:39)
- **TORCH.RESHAPE(x1, x2, x3)** — `m2 = TORCH.RESHAPE(ta, 2, 2)` (shared/samples/Torch/TorchAll.bas:34)
- **TORCH.ROUGE(x1, x2)** — `PRINT "ROUGE base ="; ROUND(TORCH.ROUGE(ref$, pb$), 4)` (shared/samples/Torch/TorchEvalBleuRouge.bas:74)
- **TORCH.SET(x1, x2, x3, x4)** — `TORCH.SET(ta, 0, 0, 99)` (shared/samples/Torch/TorchAll.bas:68)
- **TORCH.SHAPE(x1)** — `DIM sh() = TORCH.SHAPE(m2)` (shared/samples/Torch/TorchAll.bas:71)
- **TORCH.SIGMOID(x1)** — `PRINT "SIGMOID ="; TORCH.SUM(TORCH.SIGMOID(ta))` (shared/samples/Torch/TorchAll.bas:40)
- **TORCH.SIMILARITY(x1, x2, x3)** — `PRINT "similarity(a,a)="; TORCH.SIMILARITY(a, a, "euclidean")` (shared/samples/Torch/TorchTensors.bas:34)
- **TORCH.SLICE(x1, x2, x3, x4)** — `PRINT "SLICE     ="; TORCH.SUM(TORCH.SLICE(m2, 0, 0, 1))` (shared/samples/Torch/TorchAll.bas:58)
- **TORCH.SOFTMAX(x1)** — `PRINT "SOFTMAX ="; TORCH.SUM(TORCH.SOFTMAX(ta))` (shared/samples/Torch/TorchAll.bas:42)
- **TORCH.SPLIT(x1, x2, x3)** — `PRINT "SPLIT     ="; TORCH.SUM(TORCH.SPLIT(ta, 0, 2))` (shared/samples/Torch/TorchAll.bas:61)
- **TORCH.SQRT(x1)** — `PRINT "SQRT    ="; TORCH.SUM(TORCH.SQRT(ta))` (shared/samples/Torch/TorchAll.bas:49)
- **TORCH.STACK(x1, x2)** — `PRINT "STACK     ="; TORCH.SUM(TORCH.STACK(ta, ta))` (shared/samples/Torch/TorchAll.bas:60)
- **TORCH.STANDARDIZE(x1)** — `DIM sd() = TORCH.STANDARDIZE(a)` (shared/samples/Torch/TorchShapeAndData.bas:46)
- **TORCH.SUB(x1, x2)** — `PRINT "SUB     ="; TORCH.SUM(TORCH.SUB(ta, tb))` (shared/samples/Torch/TorchAll.bas:31)
- **TORCH.SUM(x1)** — `PRINT "CREATE  ="; TORCH.SUM(TORCH.CREATE(2, 2))` (shared/samples/Torch/TorchAll.bas:19)
- **TORCH.TANH(x1)** — `PRINT "TANH    ="; TORCH.SUM(TORCH.TANH(ta))` (shared/samples/Torch/TorchAll.bas:41)
- **TORCH.TEXTDECODE(x1)** — `PRINT "decodifica: [" + TORCH.TEXTDECODE(e) + "]"` (shared/samples/Torch/TorchDatasetChar.bas:29)
- **TORCH.TEXTENCODE(x1)** — `DIM e AS LONG = TORCH.TEXTENCODE("il gatto")` (shared/samples/Torch/TorchDatasetChar.bas:28)
- **TORCH.TOARRAY(x1)** — `DIM arr() = TORCH.TOARRAY(ta)` (shared/samples/Torch/TorchAll.bas:73)
- **TORCH.TOPK(x1, x2)** — `DIM tk() = TORCH.TOPK(a, 2)` (shared/samples/Torch/TorchShapeAndData.bas:40)
- **TORCH.TRANSPOSE(x1)** — `PRINT "TRANSPOSE ="; TORCH.SUM(TORCH.TRANSPOSE(m2))` (shared/samples/Torch/TorchAll.bas:56)
- **TORCH.ZEROS(x1, x2)** — `PRINT "ZEROS   ="; TORCH.SUM(TORCH.ZEROS(2, 2))` (shared/samples/Torch/TorchAll.bas:21)
<!-- END DOMAIN TABLE -->
