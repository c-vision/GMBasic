# netlb — load balancing (soluzione `gmbnet`)

**Parte di:** [gmbsuite](../../../README.md) · **Soluzione:** `gmbnet` (networking/infra) · **Prefisso:** `netlb_`
**Dipendenze:** nessuna (C99 + libm). Libreria **pura**: nessun thread/socket/I/O.

Libreria di **load balancing** per gmbsuite: un **pool di backend** con **policy
selezionabili**, contabilità eventi (begin/end), salute, slow-start, retry budget, draining.
L'host (GPM/GMBasic) possiede I/O e tempo; netlb **scegle** solo il backend ed è **deterministica**
(hash stabile + RNG seedabile).

## Algoritmi
| Policy | Cosa | Note |
|---|---|---|
| `NETLB_RR` | Round Robin | O(1), backend omogenei |
| `NETLB_WRR` | Smooth Weighted RR (nginx) | pesi statici/slow-start |
| `NETLB_RANDOM` | Random | base |
| `NETLB_LC` / `NETLB_WLC` | Least / Weighted Least Connections | durate variabili |
| `NETLB_LRT` | Least Response Time (EWMA) | esplora i non misurati |
| `NETLB_P2C` | Power of Two Choices + EWMA | **default moderno** (Envoy/Finagle) |
| `NETLB_CHASH` | Consistent Hashing + vnodes | affinità persistente |
| `NETLB_HRW` | Rendezvous / Highest-Random-Weight | affinità **stateless**; skeleton gerarchico O(log n) opz. (`netlb_set_hrw_skeleton`) |
| `NETLB_MAGLEV` | Maglev (tabella O(1)) | affinità ad alta scala (Google) |
| `NETLB_JIQ` | Join-Idle-Queue | idle-first O(1), fallback P2C (Lu NSDI'16) |

Contorno: **health** attivo (`netlb_probe`) + passivo (eiezione dopo 3 fallimenti),
**slow-start** (`netlb_set_slow_start`), **retry budget** (`netlb_should_retry`), **draining**
(`netlb_remove_backend`), **limite pending** (`cap` per backend, modificabile a runtime con
`netlb_set_cap`: oltre il numero di richieste in corso il backend è saltato), **sticky soft**
(`netlb_set_sticky`: affinità cookie→backend con TTL e fallback trasparente se il backend decade).
**HRW gerarchico** (`netlb_set_hrw_skeleton(m,f)`): skeleton a cluster di `m` e fanout `f` → pick in
O(log n) con disruption quasi-1/n (cluster per **id stabile**); per N molto grande.

## Uso
```c
#include "netlb.h"
netlb_pool* p = netlb_pool_new(NETLB_P2C, /*seed*/42);
netlb_backend_spec s; memset(&s, 0, sizeof s); strcpy(s.name, "10.0.0.1:80"); s.weight = 1; s.healthy = 1;
netlb_backend_id a = netlb_add_backend(p, &s);
netlb_backend_id id = netlb_pick(p, NULL, 0);   // key!=NULL per CHASH/HRW/MAGLEV
netlb_begin(p, id);
/* ... usa il backend ... */
netlb_end(p, id, /*ok*/1, /*lat_ms*/12.5);
netlb_free(p);
```

## Test
```sh
make test     # compila out/libnetlb.a + out/run_tests ed esegue
```
Copre (55 check): uniformità RR/WRR/random (χ²), LC, P2C+EWMA (evita il lento), **minimal disruption**
di CHASH/HRW/Maglev (~1/N), salute/eiezione, slow-start, draining, retry budget, **JIQ** (idle-first +
simulazione di coda vs P2C), **sticky soft** (affinità/fallback/TTL), **limite pending** (cap),
**HRW skeleton** (O(log n): 23 hash/pick su N=400, disruption 0.20%).

## Riferimenti
- Analisi: `AiWiki/01-Projects/GMBasic/Knowledge/Load-Balancing-Algoritmi.md`.
- Piano: `AiWiki/01-Projects/GMBasic/Todo/(open) - GMBSuite-Load-Balancing-Lib-C.md`.
- Paper: Mitzenmacher (P2C), Karger (consistent hashing), Thaler-Ravishankar (HRW), Eisenbud (Maglev NSDI'16), Lu et al. (JIQ, NSDI'16).
