# Glossario

> Termini usati nel manuale, nella forma in cui il libro li usa. Dove una parola ha due sensi
> (per esempio *token*) il manuale la qualifica sempre. Le forme elencate come **Non** sono
> errori da correggere, non varianti stilistiche.

## Linguaggio

**Comando** — Istruzione autonoma che compie un'azione e non restituisce un valore. Si scrive nella forma a spazio, senza parentesi e senza punto: `DBF USE "work/clienti.dbf" ALIAS C`. Un comando non ha forma qualificata. **Non:** `DBF.USE(...)`, "funzione DBF.USE".

**Funzione** — Operazione che restituisce un valore e si usa dentro un'espressione, sempre con le parentesi: `n = DBF.LASTREC()`.

**Metodo** — Operazione legata a un oggetto, invocata sull'oggetto stesso (`DICT.ADD`). Può condividere il nome con funzioni di altri namespace, ma è un simbolo distinto.

**Namespace** — Dominio del linguaggio che raccoglie comandi, funzioni, tipi e costanti (`DBF`, `ISAM`, `AI`, `TORCH`, `TRAIN`). È la prima parola di un comando o il prefisso di una funzione.

**IMPORT** — Direttiva di modulo, non un comando: rende visibili i simboli pubblici di un namespace, con alias di namespace, selezione di membri o alias di membro.

**NAMESPACE** — Dichiarazione che apre un dominio e ne raccoglie i simboli; `PUBLIC` li esporta, il resto resta privato al modulo.

**TokenKind** — Identificatore canonico e univoco di un simbolo, risolto dal lexer. È la chiave di ogni dispatch: la grafia sorgente non è mai la chiave.

**Lessema** — Testo esatto con cui un simbolo compare nel sorgente. Può divergere dal nome canonico: `ADD` è il lessema di `TorchAdd`, di `MatrixAddColVector` e del metodo `DICT.ADD`.

**Token** — Sempre qualificato. *Token lessicale*: elemento prodotto dal lexer, con un TokenKind e un lessema. *Token LLM*: unità di testo di un modello linguistico. Mai "token" da solo.

**Lexer** — Prima fase della compilazione: trasforma il testo in token e associa a ogni parola il suo TokenKind.

**Parser** — Seconda fase: costruisce l'albero sintattico e sceglie il parser di comando o di funzione in base al TokenKind.

**Linker** — Fase che risolve i simboli (variabili, UDF, builtin) e produce il programma che il backend esegue.

**Simbolo** — Entità nominata visibile al programma: variabile, parametro, SUB, FUNCTION, builtin, costante.

**Keyword** — Parola riservata del linguaggio. Alcune sono fabbricate per una coppia (namespace, funzione) e non coincidono con la grafia sorgente.

**Variabile** — Nome che contiene un valore; tipizzata o dinamica secondo la dichiarazione.

**Scope** — Visibilità di un simbolo: blocco, procedura, modulo o namespace.

**UDF** — Funzione definita dall'utente, contrapposta alle funzioni di libreria.

**SUB** — Procedura che non restituisce un valore e si invoca come istruzione autonoma.

**FUNCTION** — Procedura che restituisce un valore e si usa in un'espressione.

**Costante di linguaggio** — Valore simbolico scritto con il prefisso `%` (`%CHAT`, `%OCR`, `%AUDIO`), usato da comandi e funzioni.

**Operatore** — Simbolo che combina valori (`+`, `*`, `=`, `AND`). La precedenza è quella documentata nella guida del linguaggio.

**Polimorfismo** — Comportamento di un nome che dipende dal contesto risolto: un comando può avere più registrazioni (`CLOSE`, `GET`) e l'oggetto può cambiare l'implementazione del metodo.

**Macro** — Espansione testuale o riferimento indiretto a un valore (`&nome`). Non è una funzione.

**Codeblock** — Blocco di codice trattabile come valore e valutabile con `EVAL`.

**EVAL** — Valutazione dinamica di un'espressione o di un blocco a runtime, con le regole del backend in uso.

**xBase e Clipper** — Dialetto storico da cui GMBasic eredita tabelle DBF, campi nudi, macro, codeblock e parte della sintassi dei comandi.

**Preprocessore** — Fase che applica direttive e definizioni testuali prima della compilazione.

## Esecuzione

**Runtime** — Codice che esegue un programma compilato: scheduler, memoria, dispatch dei builtin, servizi di sistema.

**GPM** — Goroutine-Processor-Machine: l'unico esecutore del codice BASIC. Ogni handler, callback o evento diventa un task GPM con contesto esplicito.

**Goroutine** — Unità di esecuzione cooperativa del GPM: leggera, schedulata dal runtime e non dal sistema operativo.

**Task GPM** — Lavoro accodato al GPM ed eseguito su un worker con host e scope già impostati. Un thread esterno non invoca mai una SUB: accoda un task.

**Scheduler** — Componente che sceglie quale goroutine eseguire e quando riprenderla dopo un park.

**Worker** — Thread del pool che esegue i task GPM. Non blocca mai: se l'operazione è bloccante, il task si parcheggia.

**Park** — Sospensione cooperativa di un task in attesa di un evento (I/O, canale, attesa). Il worker resta libero e il task riprende quando l'evento è pronto.

**Canale** — Coda tipizzata per scambiare valori fra goroutine, con semantica di blocco e chiusura.

**WAITGROUP** — Sincronizzatore che attende il completamento di un gruppo di goroutine.

**Isola** — Contesto di esecuzione isolato del GPM, con stato e ciclo di vita propri.

**Frame** — Record di attivazione di una chiamata: parametri, variabili locali, punto di ritorno. La catena dei frame è ciò che un park deve conservare.

**Chunk A1/GMBC** — Formato binario in cui il compilatore C serializza il programma; è l'unità eseguita dalla VM/C e scambiata con il processo esterno.

**gmb_vm_host** — Processo esterno che esegue i chunk A1/GMBC per conto della VM/C.

**gmb_runtime_c** — Runtime C condiviso da VM/C e C99Native: sede unica delle funzioni comuni (matematica, collezioni, formattazione, crittografia).

**Compilatore C nativo** — `gmb_compiler_c`: produce chunk A1/GMBC, non eseguibili.

**Emitter** — Componente che traduce l'albero del programma in codice di destinazione. In C99Native è `gmb_c99_emit*.c` e produce C.

**Transpiler** — Emitter che produce un altro linguaggio sorgente (`.bas` in C per C99Native), poi compilato dal compilatore di sistema.

**Handle** — Riferimento opaco a una risorsa gestita (matrice, collezione, documento), restituito da una funzione e passato alle successive.

**Superficie canonica** — Insieme delle funzioni registrate da StdLib come terne (namespace, nome, TokenKind): **1.540**, coperte al 100% dai quattro backend.

**StdLib** — Registro centrale che dichiara comandi, funzioni, tipi e costanti e li rende disponibili al compilatore.

**Dispatch** — Scelta dell'implementazione da eseguire, fatta sul TokenKind risolto.

**Registro delle opzioni** — Unica sede dei parametri del motore (sezione `engine` delle impostazioni, `AI SET OPTION`). Il comportamento non si configura con variabili d'ambiente.

## Backend

**Backend** — Uno dei quattro esecutori del linguaggio: IL, PCodeVM, VM/C, C99Native. Sempre quattro, sempre in quest'ordine.

**Gamba** — Sinonimo di backend nel contesto della parità ("gamba di parità"). Nel corpo del manuale si dice backend.

**IL** — Backend gestito: compila in IL .NET ed è distribuito con NativeAOT.

**PCodeVM** — Backend gestito che esegue p-code. È gamba di parità; è runtime interno di IL solo per EVAL, macro e BSP.

**VM/C** — Backend C che esegue p-code embedded, anche tramite il processo esterno `gmb_vm_host`.

**C99Native** — Backend nativo: frontend C, emitter C e `cc`. Produce un eseguibile senza passare da C#.

**NativeAOT** — Compilazione anticipata di .NET, usata per distribuire i backend gestiti come eseguibili nativi.

**Parità** — Uguaglianza di comportamento fra backend sullo stesso programma: stessi valori, stessi effetti, stesso exit code.

**parity4** — Strumento che esegue lo stesso programma sulle quattro gambe e confronta l'output normalizzato; la reference è C99Native.

## Dati

**DBF** — Formato di tabella xBase, con record a campi fissi, e il namespace che lo governa.

**ISAM** — Archivio indicizzato del namespace `ISAM`, con indici e filtri.

**RDD** — Strato di accesso ai dati che fa da interfaccia unica verso i driver (DBF, ISAM, SQL).

**SPEED** — Namespace delle operazioni veloci su tabelle e buffer.

**LEDGER** — Namespace della contabilità in partita doppia.

**SQL** — Namespace per l'accesso a database relazionali tramite driver.

**VECTOR** — Namespace degli indici vettoriali usati da RAG e ricerca semantica.

**Record** — Riga di una tabella. Il record corrente è quello su cui operano i campi nudi.

**Campo** — Colonna di una tabella, letta o scritta per nome o come variabile nuda quando il record è corrente.

**Alias** — Nome logico con cui un'area di lavoro è riferita nel programma (`ALIAS C`).

**Area di lavoro** — Slot che ospita una tabella aperta, con record corrente, indice e filtri.

**Indice** — Struttura ordinata per chiave che accelera la ricerca e impone l'ordine di scansione.

**Filtro** — Condizione che limita i record visibili in un'area di lavoro.

**SEEK** — Posizionamento sul record con chiave uguale o successiva a quella cercata, tramite l'indice attivo.

## Interfacce

**Console** — Output a caratteri su terminale, senza finestre.

**TUI** — Interfaccia a caratteri a schermo intero, con pannelli, controlli e temi.

**GUI** — Interfaccia grafica a finestre.

**SCREEN** — Modo grafico a schermo: il comando `SCREEN` porta l'esecuzione sul target grafico.

**TEXTMODE** — Modo testo: stessa logica applicativa, resa a caratteri, senza dipendere dalla grafica.

**Controllo** — Elemento di interfaccia (pulsante, lista, campo, pannello) gestito dal motore dei controlli.

**Tema** — Insieme di colori e stili applicato a console, TUI e controlli.

**Sprite** — Immagine bitmap posizionata nel piano di gioco.

**Texture** — Immagine caricata sulla GPU e usata da sprite o superfici.

**Evento** — Notifica di input o di sistema (tasto, mouse, finestra). Viene accodata e dispatchata come task GPM, mai eseguita sul thread che l'ha letta.

**Documento** — File strutturato aperto dal motore documenti e manipolato per parti.

**Numero di file** — Intero che identifica un file aperto nelle istruzioni di I/O sequenziale.

## Rete

**HTTP client** — Comandi e funzioni per richieste HTTP(S) in uscita.

**HTTP server** — Server HTTP integrato che instrada le richieste verso handler BASIC.

**WebSocket** — Connessione bidirezionale persistente; i messaggi arrivano come eventi dispatchati nel GPM.

**MVC** — Organizzazione di un'applicazione web in modello, vista e controller, supportata dal framework HTTP.

**Coda** — Canale di messaggistica con consegna asincrona (MQTT, NATS, AMQP, Kafka), usato tramite i namespace delle librerie.

**Endpoint** — Indirizzo e metodo a cui il server risponde.

## AI

**Provider** — Servizio esterno di un'API (per esempio un fornitore di modelli linguistici) registrato nel namespace `AI`.

**Modello** — Rete neurale addestrata caricata dal motore, locale o remota, identificata da un nome registrato.

**Embedding** — Vettore numerico che rappresenta un testo, usato per ricerca semantica e RAG.

**RAG** — Retrieval-Augmented Generation: recupero di documenti pertinenti e loro inserimento nel contesto del modello.

**Vector DB** — Archivio di vettori con ricerca per similarità, esposto dal namespace `VECTOR`.

**Agente** — Programma che usa un modello per decidere azioni e chiamare strumenti, in un ciclo di passi.

**Tool** — Funzione esposta a un agente, con nome, descrizione e parametri.

**MCP** — Protocollo che collega un agente a server di strumenti e dati esterni.

**Second Brain** — Archivio personale indicizzato (note, documenti) interrogabile con RAG.

**LLM locale** — Modello eseguito sulla macchina dell'utente dal motore embedded, senza servizi esterni.

**Quantizzazione** — Riduzione della precisione dei pesi per contenere memoria e banda, con perdita di qualità controllata.

**gmbllm** — Parte del progetto che produce e trasforma i pesi (compilazione dei modelli, quantizzazione, training, LoRA), sotto licenza PUL.

**LLMEnv** — Motore di inferenza Apache-2.0 che carica ed esegue i modelli.

**FastLLM** — Libreria di inferenza veloce del perimetro Apache-2.0.

**TORCH** — Namespace del runtime tensoriale (dispositivi, tensori, metriche, dati), trasversale e non solo LLM.

**TRAIN** — Namespace della produzione e alterazione dei pesi: finetune, LoRA, distillazione, valutazione.

**CUA** — Computer use: runtime che pilota interfacce tramite azioni osservabili.

**Browser automation** — Controllo di un browser reale (navigazione, click, digitazione) da un programma BASIC.

**Evidenza** — Risultato di `AI DECIDE`: opzione, probabilità e confidenza. Non autorizza side effect e non scrive memoria.

**DECISION** — Tipo del risultato di una decisione (alias JSON), con `OPTION`, `VALUE`, `PROBABILITY`, `CONFIDENCE`, `CALIBRATED`, `KIND`. `DECISION<T>` documenta il dominio atteso.

**Dominio** — Insieme delle opzioni ammesse da una decisione: `NOUL`, `CHOICE`, `SCORE` o un dominio libero `CHOICE:a|b|…`.

**Provider di decisione** — Provider registrato sul ruolo `%DECIDE` e usato da `AI DECIDE` quando il comando non ne specifica uno.

**Calibrazione** — Artefatto `scoring_head.json` (bias per slot e temperature) che corregge il readout; senza artefatto `CALIBRATED` resta vuoto.

**Adapter (decisione)** — Provider che decide facendo generare il modello e riconoscendo l'opzione nella risposta, per endpoint che non espongono i logit.

**Policy** — Soglia applicativa scritta nel programma (`IF d.PROBABILITY >= …`), mai dentro il provider.

**Open-Jev** — Corpus sintetico (licenza MIT) usato per valutare il motore di decisione su domande a opzioni multiple.

**Verbalizer** — Mappatura delle opzioni sugli slot `A`..`P`, necessaria quando le etichette sono multi-parola e il readout usa un token singolo.

## Strumenti

**REPL** — Ciclo leggi-valuta-stampa: esecuzione interattiva di istruzioni ed espressioni.

**IDE** — Ambiente integrato con editor, esecuzione, debugger e pannelli.

**Notebook** — Documento a celle eseguibili con output inline.

**Debugger** — Strumento di ispezione e controllo dell'esecuzione: breakpoint, passo, variabili, stack.

**MiniTest** — Programma di test `.bas` autoconsistente, con oracolo e output deterministico.

**Oracolo** — Output atteso di un MiniTest, scritto nel file come righe `' EXPECT:` e confrontato dal runner.

**Gate** — Controllo automatico che fallisce, con exit diverso da zero, quando una regola del progetto è violata.

**helps** — Raccolta JSON delle descrizioni dei simboli (sintassi, parametri, esempio, output atteso) usata da REPL e IDE.

**COMMANDS.TXT** — Catalogo testuale dei comandi, generato dal registro e verificato.

**Registro della superficie** — File `surface.csv` e `canonical.registry.json` che fissano il contratto dei simboli.

**Audit** — Strumento che confronta la superficie dichiarata con quella implementata nei backend.

## Distribuzione

**Pacchetto** — Modulo distribuito con manifesto, export e sorgenti, installabile dal package manager.

**Package manager** — Strumento che risolve, scarica e collega i pacchetti.

**Libreria** — Codice C nostro, self-contained, esposto via FFI con export `.inc` e header.

**FFI** — Interfaccia per chiamare funzioni di librerie native da BASIC tramite `DECLARE`.

**Export `.inc`** — File che dichiara namespace e firme di una libreria, rendendola visibile a BASIC.

**Vendor** — Cartella del progetto che fissa una versione di pacchetto per build offline e riproducibili.

**Cache** — Archivio locale dei pacchetti e delle librerie scaricate, sotto `~/.gmbasic`.

**Installazione locale** — Installazione di un pacchetto nella cartella dell'eseguibile invece che nella cache.

**Manifesto** — File `gmb-package.json` con nome, versione, dipendenze ed export del pacchetto.

## Licenza

**PUL** — Personal Use License v1.1: licenza del progetto, non OSI. Consente l'uso personale alle condizioni del file `LICENSE`.

**Apache-2.0** — Licenza del perimetro di inferenza: LLMEnv e FastLLM.

**Open source** — Termine che non si applica a GMBasic, distribuito sotto PUL. Nel manuale non si usa mai per descrivere il prodotto.

**Confine di licenza** — Regola che separa ciò che esegue i modelli (Apache) da ciò che li produce o trasforma (PUL).

## Editoria

**Vol I** — Primo volume: Parti I-IV e appendice delle costanti.

**Vol II** — Secondo volume: Parti V-VI, reference e glossario.

**Reference** — Appendice A1 generata dal registro: 2.069 voci.

**Voce** — Singola riga della reference: una funzione, un comando, un tipo o una costante.

**Fonte primaria** — Nota del vault da cui un capitolo deriva. Ogni capitolo ne ha almeno una.

**Capitolo** — Unità del libro, con numero, parte, volume e stima di pagine.
