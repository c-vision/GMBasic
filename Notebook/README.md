# Notebook — notebook server di GMBASIC (C)

Server HTTP **locale** (`127.0.0.1`) che esegue celle BASIC in una **sessione con stato**:
le variabili definite in una cella restano disponibili nelle successive. È il ripristino in
**C** del `NotebookServer` che viveva nella REPL C# (eliminata in F6b), con lo stesso
protocollo JSON.

## Build ed esecuzione

```bash
cmake -S Notebook -B Notebook/build
cmake --build Notebook/build --target gmb_notebook
Notebook/build/gmb_notebook                 # 127.0.0.1:8030
Notebook/build/gmb_notebook --port 8040 --autosave /tmp/nb-vars.json
```

Dipende solo dal runtime C condiviso (`GMBasic/native/gmb_runtime_c`): nessuna dipendenza
dal mondo C#.

## API

| endpoint | corpo | risposta |
|---|---|---|
| `GET /health` | — | `{"ok":true}` |
| `POST /execute` | `{"cellId":"c1","source":"x = 2\nPRINT x * 3"}` | `{"ok":true,"output":"6","value":null,"compileMs":0,"executeMs":0.02,"fromCache":false}` |
| `GET\|POST /vars` | — | `{"vars":[{"name":"x","value":2}]}` |
| `GET /cells` | — | `{"cells":[{"cellId","source","output","ok"}]}` (celle in ordine di prima esecuzione) |
| `GET /order` | — | `{"order":["c1","c2"]}` |
| `POST /runall` | — | azzera lo scope e riesegue le celle in ordine: `{"results":[...]}` |
| `GET\|POST /reset` | — | `{"ok":true}` (azzera le variabili, non le celle) |
| `POST /shutdown` | — | `{"ok":true}` e il server si ferma |

Errore: `{"ok":false,"error":"..."}` con status 200 (come il server C#), 400 per richiesta
malformata, 404 per endpoint sconosciuto, 413 oltre 768 KB di corpo, 501 per le funzioni non
supportate (`/ai`).

**Celle magiche**: `%md testo` è passthrough (output = testo). `%sql`, `%ai`, `%vdb`, `%chart`
rispondono con un **errore esplicito** (non supportate nel notebook C — vedi sotto).

### Differenze dalla vecchia versione C# (nessuna persa in silenzio)

| funzione (C#) | stato nel notebook C |
|---|---|
| `/execute`, `/vars`, `/reset`, `/health`, `/shutdown` | **presenti** |
| `/cells`, `/order`, `/runall` | **presenti** (in C) |
| `%md` | **presente** |
| compilazione per-cell P-Code + cache per cella | **presente**: ogni cella è compilata in chunk A1/GMBC dal compilatore C (`gmb_compile_snippet`) ed eseguita dalla VM/C, con cache del frammento |
| `%sql` (SQLite in-memory), `%vdb` (VectorDB) | **non supportate** → errore esplicito |
| `%ai` e `POST /ai` (SSE) | **non supportati** → errore esplicito (usare AI.* in BASIC o il server del mondo C#) |
| `%chart` | **non supportato** → errore esplicito |
| `GET /` (frontend web) | **non incluso** → 404 esplicito |
| `NOTEBOOK SAVE/LOAD/EXPORT/AUTOSAVE` (.gmnb) | il notebook ora è un processo: `/cells` espone le celle, `--autosave` salva lo **snapshot variabili**; nessun `.gmnb` |
| frontend/SSE, multi-kernel | fuori perimetro (un kernel = un processo) |

Esempio:

```bash
curl -s -X POST 127.0.0.1:8030/execute -d '{"cellId":"1","source":"x = 6 * 7"}'
curl -s -X POST 127.0.0.1:8030/execute -d '{"cellId":"2","source":"PRINT \"x =\"; x"}'
curl -s 127.0.0.1:8030/vars
```

## Cosa esegue una cella

P-CODE: il sorgente della cella passa dal **compilatore C** (`gmb_compile_snippet`, dentro
`libgmb_eval`) e diventa un chunk A1/GMBC eseguito dalla **VM/C**
(`gmb_vm_eval_text_standalone`) — la stessa pipeline di `gmb`, della REPL e di `gmb_vm_host`,
non un interprete di statement separato. Lo stato è il dict della sessione: il frammento
legge e scrive i nomi con i callback dell'ambiente (`nb_env_get`/`nb_env_set`, BYREF
write-through), quindi le celle successive vedono i valori precedenti e `GET /vars` li elenca.
Una cella-espressione restituisce anche `value` (valore tipizzato, compilato come espressione).
Le richieste sono servite **in serie** da un solo kernel (una sessione = un processo),
come nel server C#.

## Autosave

Con `--autosave <file.json>`, a ogni cella eseguita viene scritto `<file.json>` con lo
snapshot delle variabili (`{"vars":[...]}`, ricaricabile come stato iniziale in una fase
successiva).
