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
| `GET\|POST /reset` | — | `{"ok":true}` |
| `POST /shutdown` | — | `{"ok":true}` e il server si ferma |

Errore: `{"ok":false,"error":"..."}` con status 200 (come il server C#), 400 per richiesta
malformata, 404 per endpoint sconosciuto, 413 oltre 768 KB di corpo.

Esempio:

```bash
curl -s -X POST 127.0.0.1:8030/execute -d '{"cellId":"1","source":"x = 6 * 7"}'
curl -s -X POST 127.0.0.1:8030/execute -d '{"cellId":"2","source":"PRINT \"x =\"; x"}'
curl -s 127.0.0.1:8030/vars
```

## Cosa esegue una cella

Il valutatore BASIC del runtime C (`gmb_expr_exec_capture`, lo stesso delle macro `&`/`EVAL`):
assegnazioni, `PRINT`, `LET`, `IF/THEN/ELSE`, `FOR/NEXT`, `WHILE/WEND`, espressioni e
funzioni del runtime. Lo stato è il dict della sessione (`gmb_expr_dict_get/set`).

**Limite dichiarato (Fase 1)**: una cella non può **definire** `SUB`/`FUNCTION` (il
valutatore interpreta statement, non compila moduli). Per le celle-espressione viene
restituito anche `value` (valutato una seconda volta: nessun effetto collaterale).
Le richieste sono servite **in serie** da un solo kernel (una sessione = un processo),
come nel server C#.

## Autosave

Con `--autosave <file.json>`, a ogni cella eseguita viene scritto `<file.json>` con lo
snapshot delle variabili (`{"vars":[...]}`, ricaricabile come stato iniziale in una fase
successiva).
