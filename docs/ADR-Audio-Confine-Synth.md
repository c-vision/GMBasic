# ADR — Confine del synth a 8 voci: device, mixer, voce → canale

> **Stato**: accettato il 2026-11-28, **rev. 2026-11-28** (D-1/D-2 in due fasi: in fase 1 il device è
> quello del mixer). **Solo decisioni e documentazione: nessuna riga di codice.**
> **Ambito**: chi possiede il device audio, se il synth assorbe il mixer `dlopen` o resta separato,
> e la semantica voce → canale che deve restare compatibile con l'audio posizionale **P6**
> (`M-SND*`, `docs/3D-Specifica-Comportamenti.md` §3.2).
> **Specifica di comportamento**: `docs/Audio-Specifica-Comportamenti.md` (ID `M-AUD-*`). Questo ADR
> chiude le sei domande aperte del suo §11 e ne cambia tre punti (`M-AUD-DLOPEN`, §6, `M-AUD-POS`).
> **D-9** chiude la coabitazione delle 8 voci fra audio posizionale e comandi temporali.
> **Piano**: `docs/Piano-Motore-3D.md` §4.3, riga «Synth 8 canali + MML», fase P6.

---

## 1. Contesto (fatti verificati nel codice, non dedotti)

1. **Oggi il motore C fa due cose in un file solo.** `gmb_runtime_c/src/gmb_audio.c` (776 righe,
   lista `GMB_ALL_SRCS` → **core**) contiene:
   - il synth PCM **puro** (`gmb_audio_synth`) che rende **una nota intera** in un buffer;
   - il playback tramite **`dlopen` di SDL2_mixer** (`g_mix`, 11 simboli: `OpenAudio`,
     `AllocateChannels`, `QuickLoadRAW`, `PlayChannel`, `Playing`, `FreeChunk`, `LoadWAV`,
     `HaltChannel`, `Pause`, `Resume`, `Volume`), con `Mix_OpenAudio(44100, S16, 2, 1024)` e
     16 canali del mixer.
   Il canale lo sceglieva il mixer (`Mix_PlayChannel(-1, …)` = primo canale libero): oggi il
   posizionale **non** prende un canale occupato, e se non ce n'è uno libero il tono non parte in
   silenzio. Con le voci fisse di D-4 quella ricerca non esiste più: chi vince lo dice D-9.
2. **Il core non linka SDL, di proposito.** `CMakeLists.txt` 1058-1061: «Audio SDL_mixer — CARICATA
   A RUNTIME (dlopen), NON LINKATA: linkarla faceva ereditare a ogni dylib la catena del mixer
   (FLAC/ogg/opus/vorbis/sndfile/fluidsynth… ~18 lib e ~15 MB)». La variante minima del runtime
   (`gmb_runtime_min`) deve compilare e linkare **senza SDL**.
3. **P6 suona su un canale del mixer.** `gmb_3d_audio.c` chiama `gmb_audio_sound_panned(freq, dur,
   gain, pan)` **una volta**, al `SCENE SOUND`: il gain e il pan sono quelli dell'istante del comando;
   il `RENDER` successivo ricalcola i valori **per le due letture** (`SOUNDGAIN`/`SOUNDPAN`) ma **non**
   tocca il suono già avviato. Già oggi il limite dichiarato è «8 toni sovrapposti» (`M-SND-NOWAIT`),
   non 16: gli slot sono 16, i toni contemporanei 8.
4. **I file WAV sono usati pochissimo.** `SOUND LOAD` / `SOUND PLAY #h` / `SOUND VOLUME` compaiono in
   **2 sample** (`shared/samples/Audio/AudioControls.bas`, `AudioFileLoading.bas`) per **7 righe** in
   tutto. Il loro parse esiste solo in C# (`ParseSoundLoadStatement`, `ParseSoundPlayFileStatement`,
   `ParseSoundVolumeStatement`); sulle gambe C quei comandi sono oggi **saltati**: il parser C99 li
   riduce a `REM` (`gmb_parser_stmt_audio.c`), mentre il runtime C ha già `gmb_audio_load_sound`,
   `gmb_audio_play_sound` e `gmb_audio_set_volume` sul mixer e VM/C ha i codici
   `GMB_SC_SOUND_LOAD`/`PLAY_FILE`/`VOLUME`.
5. **Il parser C# di `SOUND` copre solo il tono**: `SOUND [ON v,] freq, dur, vol, wave, att, dec, sus,
   rel, pw`. `SOUND VOLUME` ha la forma **`volume[, channel]`** (`IAudioService.SetVolume(int volume,
   int? channel)`), quindi `SOUND VOLUME 50` = master, non «voce 50».
6. **Nessuna env, un'implementazione sola**: regole §"ENVIRONMENT VARIABLES" e §0bis di `AGENTS.md`;
   C# può chiamare C via P/Invoke (regola C#→C), il C non chiama C#.
7. **GPM (§0ter)**: il thread del device audio non esegue BASIC e non prende in ostaggio un worker.

---

## 2. Decisioni

### D-1 — Un device **solo**: in fase 1 è quello del mixer (pull via `Mix_HookMusic`), in fase 2 quello di SDL2 core

> **Rev. 2026-11-28 — due fasi.** La prima stesura diceva «il device è del synth, ed è SDL2 core» in un
> colpo solo, e rifiutava «il mixer come device» perché «il mixer non espone un callback che possiamo
> rendere noi»: **premessa falsa**. `Mix_HookMusic` e `Mix_SetPostMix` esistono da SDL_mixer 2.0.0 e sono
> esattamente quel callback. La revisione separa il device dal costo dell'assorbimento dei WAV.

**Decisione.** Un **solo** device audio per processo, in entrambe le fasi. Cambia chi lo apre:

| | **fase 1** (questo lavoro) | **fase 2** (pulizia, dopo P6 verde) |
|---|---|---|
| device | quello di **SDL2_mixer** (`dlopen`, come oggi) | **SDL2 core**, `SDL_OpenAudioDevice` in `dlopen` |
| chi tira il render | **`Mix_HookMusic`**: gira per **primo** su ogni buffer, che è **silenzioso** in ingresso — si scrive, non si somma | il callback del nostro device |
| WAV | chunk del mixer, come oggi | voce campione del synth (`gmb_audio_wav.c`) |
| simboli `Mix_*` | restano, **+2** (`Mix_HookMusic`, `Mix_QuerySpec`) | **0** |

In entrambe `gmb_synth_render(stream, n)` è l'**unica** funzione di render — la stessa che i MiniTest
chiamano offline (`M-AUD-MODE`) — e il file che apre il device è `gmb_audio_dev.c` in `GMB_ALL_SRCS`
(core): nessun thread nostro, nessuna coda di push.

**Formato e blocco (fase 1).** Il hook riceve il buffer nel formato **ottenuto**, non in quello chiesto:
si apre con `Mix_OpenAudioDevice(44100, AUDIO_S16SYS, 2, 512, NULL, 0)` (`allowed_changes` = 0) e si
**verifica** con `Mix_QuerySpec(&freq, &fmt, &ch)`. Simbolo assente, richiesta non onorata o formato
diverso da 44100/S16/2 → **offline automatico**: mai rendere a un rate diverso da quello del device
(cambierebbe l'altezza, non la latenza). Blocco **512** frame ≈ 11,6 ms; con il `Mix_OpenAudio` legacy,
1024 come oggi.

**Perché (fase 1).**
- **Il device resta uno, e l'orologio pure**: il hook è dentro il callback del device, quindi il
  render vede lo stesso tempo, lo stesso master e lo stesso contatore per il synth e per i WAV. Due
  device (synth + mixer) renderebbero master, saturazione e latenza non definibili.
- **Il core continua a non linkare SDL** (fatto 2): la fase 1 dlopena SDL2_mixer (come già fa), la
  fase 2 SDL2 core; in nessuna delle due SDL entra nel link.
- **Pull e non push.** Il render è una **funzione pura** di `(stato, n)`, la stessa che i MiniTest
  chiamano offline: il hook la chiama una volta per blocco. Con `SDL_QueueAudio` servirebbe un secondo
  thread che rende "a tempo", cioè un altro orologio da sincronizzare.
- **Il synth non dipende da un decoder che non esiste**: P6 nasce e diventa verde con i WAV
  esattamente come sono oggi. Il decoder è fase 2 (D-2).

**Conseguenze.**
- `Mix_HookMusic` **sostituisce il music player** di SDL_mixer: nella tabella `g_mix` non c'è **nessun**
  simbolo `Mix_Music*` (i WAV sono `Mix_Chunk`), quindi non si perde nulla — va scritto nel commento del
  file, perché è l'unica cosa che il hook disabilita.
- Il hook **non prende lock, non chiama funzioni del mixer, non esegue BASIC** (D-7, §0ter): scrive nel
  buffer e pubblica lo snapshot. I comandi che chiamano il mixer per i WAV (`Mix_Volume`,
  `Mix_HaltChannel`) prendono il lock audio di SDL e possono attendere **al più un blocco**: bounded, e
  dal lato comandi, non dal lato render.
- Se `dlopen`, l'apertura o il formato falliscono → **modalità offline automatica** (`M-AUD-MODE`):
  nessun errore fatale, nessun device, tutte le letture di `AUDIO.*` restano valide.
- Latenza dichiarata: 512 frame di buffer + il buffer del driver; non è un target "pro audio". Il
  render resta indipendente dal blocco (`M-AUD-BLOCK`).

**Alternative rifiutate.**
1. **Linkare SDL2** nel core: rompe l'invariante del runtime minimo (fatto 2) e trascina SDL in ogni
   processo che vuole solo il synth.
2. ~~**Tenere SDL2_mixer come device**: il mixer non espone un callback che possiamo rendere noi.~~
   **Ritirata: la premessa è falsa.** `Mix_HookMusic` è quel callback (fire su ogni buffer, buffer
   silenzioso in ingresso) e `Mix_SetPostMix` è la variante che gira dopo il mix; entrambe esistono da
   SDL_mixer 2.0.0. Il costo vero di tenere il mixer non è «niente callback», è: la catena `dlopen` dei
   ~18 lib resta, i WAV restano fuori da master e contatore di clipping, un canale del mixer non è una
   delle 8 voci. Sono i costi che paga la **fase 2**, non quelli che giustificano di pagarli **tutti
   adesso**.
3. **Due device** (synth su SDL2 core + mixer per i WAV): due orologi, due buffer, due livelli, master
   e contatore di clipping non definibili; e la fattibilità dipende dalla versione di SDL_mixer
   (`Mix_OpenAudio` storico passa da `SDL_OpenAudio`, documentata «you can only have one device open at
   a time with this function»). È la fase 1 **peggiore**: un device in più e zero semplificazioni.
   Rifiutata in radice.

---

### D-2 — L'assorbimento del mixer è **fase 2**: la fase 1 lo tiene come device e per i WAV

**Decisione.** `g_mix`, `gmb_mix_api` e i simboli `Mix_*` **non** escono dal percorso audio nella fase
1: il mixer è il device (D-1) e il riproduttore dei WAV, come oggi. La fase 2 lo assorbe e lo cancella.

| punto | **fase 1** | **fase 2** |
|---|---|---|
| `SOUND`/`PLAY`/`FILTER`/`SCENE SOUND` | **synth a 8 voci**, nessun chunk | uguale |
| `SOUND LOAD` / `PLAY #h` / `VOLUME` | chunk del mixer, **comportamento di oggi** | voce campione con decoder PCM (`gmb_audio_wav.c`) |
| `g_mix` + 11 simboli | restano, **+2** (`Mix_HookMusic`, `Mix_QuerySpec`) | rimossi, `grep -c "Mix_"` = 0 |
| master (`SOUND VOLUME v`) | **un valore, due punti**: gain del synth e `Mix_Volume` sui canali WAV | un punto |
| clipping | contato sul synth; la saturazione dei WAV la fa SDL_mixer, **invisibile** | contato su tutto |

**Perché (e cosa cambia rispetto alla prima stesura).**
- La prima stesura addebitava a questo punto **tre** cose che non gli appartengono: (a) il decoder WAV;
  (b) l'implementazione di `SOUND LOAD/PLAY #h/VOLUME` sulla gamba **C99**, che oggi il parser **salta**
  esplicitamente e che per §4.1 di `AGENTS.md` significa MiniTest + sample + guida; (c) la **delega
  C#**, che però è imposta da D-6 + §4.1 (le letture `AUDIO.*` devono esistere su IL/PCode) e si paga
  in **ogni** opzione. Il costo marginale dell'assorbimento è (a)+(b)+la cancellazione: reale, ma **non**
  un prerequisito del synth.
- Assorbire adesso obbligherebbe a un **PR unico** (vecchio §3: «nessun passo rilasciabile da solo»),
  perché il mixer sparirebbe prima che il decoder esista. Con la fase 1 il PR del synth non tocca né i
  WAV né i simboli.
- Resta vero che la fase 2 **conviene**: una dipendenza in meno (`SDL2_mixer` + catena codec), un solo
  master e un solo contatore, WAV come voce, gate `Mix_` = 0. È pulizia, e si fa quando P6 è verde.

**Conseguenze (fase 1, dichiarate).**
- I due sample WAV continuano a suonare come oggi, tramite il mixer: nessuna regressione e nessuna
  verifica nuova sul loro audio.
- `SOUND PLAY #h, n` usa il **canale del mixer** `n`, non la voce `n`: i WAV non entrano nel pool delle
  8 voci finché non c'è la fase 2 (`M-AUD-SOUND-WAV`, e l'eccezione in D-9).
- `M-AUD-CLIP` non vede la saturazione dei WAV e `M-AUD-SOUND-VOL` ha due punti di applicazione: sono i
  due prezzi della fase 1, scritti nella specifica.
- Il master resta **uno** come valore: `SOUND VOLUME v` scrive il gain del synth **e** `Mix_Volume` sui
  canali del mixer, con lo stesso numero. Non è un secondo master, è lo stesso applicato in due punti.

**Alternative rifiutate.**
1. **Assorbire subito** (prima stesura): paga (a)+(b)+(c) prima che il synth esista e impone il PR
   unico; il guadagno — una dipendenza in meno — non serve a P6.
2. **Synth accanto al mixer con due device**: vietato da D-1 alternativa 3, e non semplifica nulla.
3. **Non assorbire mai**: possibile, ma lascia due punti per il master, un contatore parziale e un
   canale che non è una voce; la fase 2 esiste per questo ed è dichiarata opzionale, senza una data.

---

### D-3 — `voice` **è** il canale: 0..7 stabili, e i tre proprietari delle voci

**Decisione.** La voce (0..7, clamp) è l'identità del canale. Mappatura completa della superficie:

| superficie | voce usata | note |
|---|---|---|
| `SOUND freq, dur, …` | **0** | default del C# (`SoundOnVoiceAsync` senza voce) |
| `SOUND ON n, freq, dur, …` | **n** (clamp 0..7) | il canale è la voce, nessuna tabella di traduzione |
| `SOUND VOLUME v` | — (master) | forma del C#: `volume[, channel]` |
| `SOUND VOLUME v, n` | **n** | volume della voce, applicato ai campioni successivi |
| `SOUND STOP [n\|ALL]` / `PAUSE` / `RESUME` | **n** o tutte | azione sulle voci; le stringhe (canali nominati) sono ignorate, dichiarato |
| `FILTER voice, …` | **n** | già per voce (`M-AUD-FILTER`) |
| `PLAY "mml"` | rotazione 0..7 | `M-AUD-ALLOC`; `ON n` numerico → voce `n`, senza ON → cursore |
| `PLAY CONCURRENT a$, b$` | una per sequenza | voci diverse, partenza simultanea (`M-AUD-PLAY-CONC`) |
| `SOUND LOAD "f" AS #h` | — | handle a un **buffer decodificato**, non una voce |
| `SOUND PLAY #h [, ch] [, LOOPS n]` | **ch** se numerico, altrimenti rotazione | un file può durare più di una nota: la voce si libera a fine buffer |
| `SCENE SOUND n, …` (P6) | **derivata dallo slot**, §D-4 | 16 slot → 8 voci, il posizionale non usa `PLAY` |

**Perché.** È l'unica mappatura che non introduce una seconda numerazione: il canale del mixer era
già un intero, la voce del synth è un intero 0..7, e `SOUND ON n` è già documentato come "sulla voce
n" (`M-AUD-VOICE`). Le voci **non** sono handle con nome: una tabella nome→voce sarebbe la seconda
semantica che `M-AUD-PLAY` dichiara fuori perimetro.

**Conseguenze.**
- `SOUND ON 3` e la voce 3 sono la stessa cosa per sempre: nessun "canale 3" che suona su una voce
  diversa.
- Un solo retrigger per voce (`M-AUD-RETRIG`): due comandi sulla stessa voce non si sovrappongono,
  il secondo sostituisce il primo. È la regola che rende `SOUND ON n` prevedibile e che P6 usa per
  rubare una voce. **Chi** possa essere il secondo — un comando temporale o una sorgente
  posizionale — lo fissa D-9, e la regola vale nei due sensi.

---

### D-4 — P6: slot → voce **derivata**, 8 sorgenti simultanee, gain/pan **live**

**Decisione.**
- **Mappatura:** `voce = (slot − 1) mod 8`, quindi slot 1 → voce 0, …, slot 8 → voce 7, slot 9 →
  voce 0 (ruba), …, slot 16 → voce 7. È **stabile**: lo stesso slot usa sempre la stessa voce finché
  suona, e il test P6 può indirizzarla senza ambiguità. Il furto è **incondizionato**: lo slot non
  chiede permesso a nessun altro proprietario della voce (D-9).
- **Limite dichiarato:** al massimo **8 sorgenti posizionali contemporanee**; la nona ruba la voce
  dello slot che la occupa (retrigger). È **lo stesso limite di oggi** (`M-SND-NOWAIT`: «8 toni
  sovrapposti»), quindi P6 non peggiora: cambia solo che il limite diventa esplicito e verificabile.
- **Gain e pan live:** `SCENE RENDER` ricalcola gain/pan dell'istante e il motore li applica **dalla
  giunzione del blocco successivo** alla voce in riproduzione, senza riavviare il tono
  (`M-AUD-POS-LIVE`). Oggi il tono resta congelato ai valori del comando (`M-SND-SNAP`), perché la
  vecchia architettura rendeva la nota in un chunk unico. Il live tocca la voce **solo se lo slot ne
  è ancora il proprietario**: un `RENDER` non reclama mai una voce (D-9).
- **Invarianti che non cambiano:** le formule di gain/pan (`M-SND-GAIN`, `M-SND-PAN`), le letture
  `SCENE.SOUNDGAIN(n)`/`SCENE.SOUNDPAN(n)` e le loro sentinelle (`−1`/`−2`), l'errore esplicito di
  `M-SND-ERR`, lo svuotamento di `RESET`, il comando non bloccante.
- **Dove il pan si applica:** nel mix del synth, per campione, con legge lineare
  (`M-AUD-PAN`/`M-AUD-PAN-POS`). La vecchia frase di `M-SND-PCM` («la tabella `dlopen` del mixer non
  cambia») resta vera **a fortiori**: dopo D-2 quella tabella non esiste.

**Perché.** P6 non deve rompersi, e non si rompe: si romperebbe se un comando cambiasse significato,
se una lettura cambiasse sentinella o se il tono non partisse più. La derivazione `(slot−1) mod 8`
non tocca nessuno dei tre; il limite a 8 è già quello reale; il gain/pan live è la correzione di un
limite del chunk, non un cambio di contratto (i numeri letti sono gli stessi, e lo erano anche prima
che il suono li seguisse).

**Alternative rifiutate.**
1. **Un pool di voci con furto "più vecchia"** per P6: rende la voce di uno slot **non prevedibile**
   (dipende dalla storia), e i test non possono indirizzare un tono per voce. Scartata.
2. **Slot 1..8 fissi e slot 9..16 muti**: silenzio silenzioso su metà superficie. Scartata.
3. **Dare al posizionale le proprie voci fuori dal pool** (8+8): viola D-1 (un solo mix) e raddoppia
   le voci senza che nessuno le abbia chieste.
4. **Tenere gain/pan congelati** (`M-SND-SNAP` invariato): possibile, ma con il motore a voci il costo
   è zero e la sorgente che non segue la camera è un difetto, non una scelta. Se l'utente preferisce
   la parità stretta, si toglie `M-AUD-POS-LIVE` e resta il resto: **unica decisione reversibile a
   costo zero** di questo ADR.

---

### D-5 — Una sola implementazione, quattro gambe: il C# delega

**Decisione.** Il motore e il parser MML restano **una copia sola**, in C (`gmb_synth.c`,
`gmb_audio.c`). Le gambe gestite (IL, PCodeVM) e il C# chiamano `gmb_audio_*` via **P/Invoke**
(regola C#→C di `AGENTS.md`); `AudioManager.cs` diventa un **wrapper sottile** (parse degli statement →
chiamata C), non un secondo synth. VM/C e C99Native chiamano le stesse funzioni, come già fanno.

**Conseguenze.**
- Il vecchio percorso C# con `SDL2_mixer` (`AudioManager.cs`, ~1100 righe con le sue `Mix_Volume`)
  non è più l'implementazione: sparisce a migrazione fatta, insieme alla tabella `dlopen` del C.
- Finché la migrazione C# non è completa, **il C# non guadagna feature nuove**: il suo MML e i suoi
  default restano quelli allineati il 2026-11-28, senza divergenze introdotte da questa parte.
- Un MiniTest su IL/PCode misura la **stessa** implementazione di C99Native: la parità non è più un
  confronto fra due synth, è la stessa funzione chiamata due volte.

**Alternativa rifiutata.** Tenere il synth in C# per le gambe gestite e in C per C99Native: due
motori, due MML, due semantichè di filtro e di clipping — esattamente ciò che `M-AUD-DIVERGE-FILTER`
ha già dovuto correggere una volta.

---

### D-6 — Superficie di lettura: **4 funzioni**, non 8

**Decisione.** Le letture headless di §6 della specifica si consolidano in **quattro** funzioni (una
per famiglia, i campi sono documentati):

| funzione | campi (`what`) |
|---|---|
| `AUDIO.RENDER(n)` | rende n campioni, ritorna n (0 fuori range o in device mode) |
| `AUDIO.VOICE(v, what)` | 0 = picco, 1 = RMS, 2 = occupata, 3 = frequenza, 4 = campioni rimanenti |
| `AUDIO.STATUS(what)` | 0 = clock, 1 = picco del mix, 2 = campioni saturati, 3 = eventi, 4 = modalità (0 offline, 1 device) |
| `AUDIO.EVENT(i, what)` | 0 = start in campioni, 1 = frequenza, 2 = durata in campioni, 3 = volume, 4 = tipo, 5 = voce |

`AUDIO OFFLINE` / `AUDIO ONLINE` restano **comandi** (nessun valore di ritorno).

**Perché.** `AGENTS.md` §4.1 impone, per **ogni** funzione nuova, MiniTest + sample + guida: quattro
funzioni con campi documentati costano quattro guide invece di otto, senza perdere una sola
asserzione. La separazione per famiglia rende anche impossibile confondere una lettura di voce con
una di motore.

**Conseguenze.** Un MiniTest legge sempre la stessa coppia `AUDIO.VOICE`/`AUDIO.STATUS`; i valori e le
sentinelle (`−1`) sono quelli di §6 della specifica.

---

### D-7 — Concorrenza: comandi in coda, statistiche in seqlock, callback senza lock

**Decisione.**
- Il thread GPM **non tocca** lo stato del motore vivente: accoda **record a dimensione fissa** in una
  coda **SPSC** (produttore = GPM, consumatore = callback del device). Il callback li applica **tutti
  all'inizio del blocco**, mai a metà campione.
- Le statistiche (picco/RMS per voce, picco del mix, contatore di clipping, clock) sono pubblicate dal
  callback in uno **snapshot double-buffer con contatore di sequenza** (seqlock): le letture non
  bloccano il callback e non vedono mai un blocco misto.
- In **offline** non c'è callback: `AUDIO.RENDER` esegue lo stesso `gmb_synth_render` sul task GPM e
  lo snapshot è scritto dallo stesso thread. **Un solo codice di render per le due modalità.**

**Perché.** Un mutex nel callback audio è il modo classico di perdere campioni e di creare inversione
di priorità; una coda SPSC senza lock lo evita e rende il confine GPM/thread esplicito (§0ter: il
callback non esegue BASIC, non alloca, non chiama il runtime).

**Conseguenze.** La granularità dei comandi è il **blocco** (≤ 11,6 ms): `SOUND START` che arriva a
metà blocco parte al blocco successivo. È dichiarato e non si vede in un test offline, dove il clock
lo governa il test.

---

### D-8 — La facade resta: gli chiamanti non cambiano

**Decisione.** Le firme esistenti in `gmb_audio.h` restano valide e vengono **reimplementate** sopra
il synth, senza cambiarne il nome: `gmb_audio_sound`, `gmb_audio_play_sound`, `gmb_audio_load_sound`,
`gmb_audio_set_volume`, `gmb_audio_control`, `gmb_audio_is_playing`, `gmb_audio_sound_voice`,
`gmb_audio_sound_panned`, `gmb_audio_play_mml`, `gmb_audio_set_filter`.

**Perché.** C99Native, VM/C e P6 chiamano quelle funzioni: cambiare firma significa toccare tre leg e
la scena 3D nello stesso passo della migrazione del motore. Reimplementare dietro la stessa facade
tiene il confine **sostituibile** e i diff leggibili.

**Conseguenze.** `gmb_audio_sound` e `gmb_audio_sound_voice` diventano sinonimi (voce 0 vs voce n) e
la documentazione lo dice; `gmb_audio_is_playing` legge `AUDIO.VOICE(v, 2)`.

---

### D-9 — Otto voci, **un proprietario per voce**: retrigger uniforme, nessuna priorità, nessuna prenotazione

**Decisione.** Le 8 voci sono **un solo pool** condiviso dai quattro proprietari (tono posizionale,
`SOUND`, sequenza `PLAY`/`CONCURRENT`, voce campione di `SOUND PLAY #h` — quest'ultima solo dalla
**fase 2**: in fase 1 i WAV sono canali del mixer e restano fuori dal pool, D-2). La regola è una sola,
senza eccezioni di classe:

1. **Un proprietario per voce.** La voce è occupata esattamente quando `AUDIO.VOICE(v, 2) = 1`
   (campioni rimanenti > 0): la prenotazione **non è uno stato nuovo**, è il busy che esiste già. Il
   comando che parte prende possesso; la voce torna libera alla fine dell'inviluppo (o del buffer) o
   allo `STOP`.
2. **L'ultimo comando vince, nei due sensi.** `SCENE SOUND` ruba la voce anche a un `SOUND`/`PLAY` in
   corso; `SOUND ON n`, `PLAY … ON n` e `SOUND PLAY #h` sulla voce di un tono posizionale fanno lo
   stesso. Nessuna priorità di classe, nessuna voce riservata al 3D, nessuna coda d'attesa: chi arriva
   dopo prende.
3. **Il preempted è cancellato, non migrato.** La nota o la sequenza che perde la voce **finisce
   lì**: nessuna riassegnazione a un'altra voce, nessuna ripresa, nessuna nota in sospeso. La
   cancellazione non riavvolge il parser: gli eventi MML restano quelli prodotti (`M-AUD-EVENTS`), è
   la voce a non suonarli più.
4. **Il posizionale non reclama a ogni frame.** `SCENE RENDER` ricalcola gain/pan per le **letture**
   (`M-SND-READ`) e applica il live **solo se lo slot è ancora proprietario** della voce (D-4): un
   `RENDER` non ruba mai una voce. Persa la voce, lo slot resta `used` e le letture restano quelle
   della sorgente, senza toccare i campioni di nessun altro.
5. **`CONCURRENT` prenota in blocco, con voci distinte.** N sequenze prendono N voci **distinte** dal
   cursore di rotazione (wrap 0..7), tutte nello stesso blocco (atomico). Oltre **8** sequenze:
   errore esplicito, nessuna parte (mai polifonia parziale). Se una voce del gruppo viene poi presa,
   si cancella **solo quella sequenza**: le altre continuano e il gruppo non si rialloca.

**Perché.**
- Le due invarianti scritte sono letterali e non ammettono eccezioni: `SOUND ON n` suona **sempre**
  sulla voce `n` (D-3, `M-AUD-VOICE`) e uno slot usa **sempre** `(slot − 1) mod 8` (D-4,
  `M-SND-VOICE`). Una priorità di classe ne romperebbe per forza una: se il posizionale non è
  cedibile, `ON n` non è più `n`; se `ON n` è intoccabile, il posizionale non è più stabile proprio
  quando la scena è affollata. La regola uniforme è l'unica che tiene entrambe.
- **Una prenotazione separata non è disponibile**: le voci sono 8 (D-1, D-4). Riservarne alcune al 3D
  toglierebbe polifonia a chi non usa `SCENE` (terza alternativa di D-4) o creerebbe un pool 8+8
  (vietato da D-1).
- **Costa zero**: nessun flag, nessun contatore, nessuna lettura nuova. `AUDIO.VOICE(v, 2)` è già il
  registro di proprietà, `AUDIO.VOICE(v, 3)` dice chi possiede la voce **ora**, e una preemption si
  vede dal fatto che la frequenza precedente è sparita.

**Conseguenze (dichiarate, non nascoste).**
- Un effetto posizionale può **tagliare la musica**, e un `SOUND` può **spegnere un tono 3D**. Non è
  un caso limite: è la regola. Chi vuole musica e effetti insieme li mette su **voci diverse** —
  `PLAY … ON 0` con gli slot 2..16 (voci 1..7) — oppure accetta il retrigger. È una **convenzione
  d'uso**, non un meccanismo: il motore non riserva voci a nessuno.
- Lo slot che perde la voce **non si svuota**: sentinelle e valori di `M-SND-READ` invariati; il tono
  riparte solo con un altro `SCENE SOUND` sullo stesso slot.
- Il limite posizionale resta «8 toni sovrapposti» (`M-SND-NOWAIT`): con 9 sorgenti la nona ruba la
  voce dello slot con lo stesso resto, e il furto ora ha una regola unica invece del comportamento
  del mixer.

**Alternative rifiutate.**
1. **Priorità al posizionale** (`SOUND` su voce prenotata → altra voce, o drop): rompe `ON n` = voce
   `n` e introduce un ripiego silenzioso o una coda, cioè uno scheduler che né il C# né il codice di
   oggi hanno.
2. **Priorità al temporale** (il posizionale non ruba mai): rompe la stabilità slot → voce proprio
   nel caso affollato e richiede la «prima voce libera» (`Mix_PlayChannel(-1, …)`), cioè la mappa
   imprevedibile già rifiutata in D-4.
3. **Voci riservate al 3D** (4+4, 2+8): polifonia dimezzata per chi non usa la scena, oppure due
   pool (vietato da D-1).
4. **Migrare il preempted su un'altra voce**: la voce di un test dipenderebbe dalla storia; è il
   furto «più vecchia» rifiutato in D-4.

**Oracoli** (offline, `AUDIO OFFLINE`).

| Caso | Sequenza | Atteso |
|---|---|---|
| posizionale ruba al temporale | `SOUND ON 0, 440, 1` · `SCENE SOUND 1, 880, 1, 0,0,0, 10, 100` · `RENDER(512)` | `VOICE(0, 3)` = 880, `VOICE(0, 2)` = 1 |
| temporale ruba al posizionale | `SCENE SOUND 1, 880, 1, …` · `SOUND ON 0, 440, 1` · `RENDER(512)` | `VOICE(0, 3)` = 440; `SOUNDPAN(1)` invariato (lettura per slot) |
| preempted cancellato | `PLAY "T120 L4 C D E" ON 2` · `SCENE SOUND 3, 880, 0.1, …` · render oltre la sequenza | durante il tono `VOICE(2, 3)` = 880; finito, `VOICE(v, 2)` = 0 su tutte le voci (le note non migrano) |
| `CONCURRENT` distinto | `PLAY CONCURRENT "L1 C", "L1 E" ON 6` | `VOICE(6, 2)` = `VOICE(7, 2)` = 1, voci distinte |
| `CONCURRENT` oltre 8 | 9 sequenze `ON 0` | errore esplicito, `VOICE(v, 2)` = 0 su 0..7 |

---

## 3. Ordine di migrazione: **due fasi**, la prima rilasciabile da sola

**Fase 1 — il synth, con il device del mixer.** Rilasciabile da sola: non lascia due motori né due
device, e i WAV si comportano come prima.
1. `gmb_synth.c` — voci, mix, inviluppo, forme, filtro, sequencer MML, eventi in campioni (nessuna
   allocazione per campione).
2. `gmb_audio_dev.c` — `dlopen` di SDL2_mixer (già presente), `Mix_OpenAudioDevice` + `Mix_QuerySpec` +
   `Mix_HookMusic`, coda SPSC dei comandi, seqlock, offline.
3. Facade (D-8) e comandi riimplementati sopra il motore; `AUDIO.*` (D-6).
4. P6: `gmb_audio_sound_panned` → voce dello slot (D-4, D-9); `gmb_3d_audio.c` invariato nei numeri.
5. `g_mix` **resta** (11 + 2 simboli); `SOUND LOAD/PLAY #h/VOLUME` invariati: C# li implementa, la gamba
   C99 li salta come oggi — gap dichiarato, non peggiorato.

**Fase 2 — la pulizia, dopo P6 verde.** Non tocca le letture né i numeri di P6.
6. `gmb_audio_wav.c` — decoder PCM WAV + voce campione; `SOUND LOAD/PLAY #h/VOLUME` portati anche sulla
   gamba C99 (parse + emit + wire, con MiniTest, sample e guida §4.1).
7. Device proprio: `SDL_OpenAudioDevice` di SDL2 **core** in `dlopen`, stesso `gmb_synth_render`.
8. Rimozione di `gmb_mix_api`, dell'`dlopen` di SDL2_mixer e dei simboli `Mix_*`; `GMB_GFX_SDL_MIXER`
   esce dal CMake se resta senza consumatori.

La fase 2 è **opzionale e dichiarata**: se non si fa, lo stato resta coerente (un device, un motore, un
master come valore). Non esiste un «metà» fra le due.

---

## 4. Verifica (quando il codice ci sarà)

| Cosa | Come | Criterio |
|---|---|---|
| Voci, inviluppo, forme, quantizzazione | selfcheck C sul buffer (`gmb_audio_selfcheck.c`) | valore atteso campione per campione, `M-AUD-*` |
| Determinismo e indipendenza dal blocco | MiniTest offline: 1×1000 vs 10×100 | byte identici (`M-AUD-BLOCK`, `M-AUD-DET`) |
| Voci/mix/clip | MiniTest offline (`AUDIO OFFLINE`, `RENDER`, `VOICE`, `STATUS`) | `shared/minitests/audio/synth_voices.bas` |
| MML in campioni | `AUDIO.EVENT(i, what)` | `synth_mml.bas` |
| Filtro | energia prima/dopo, cambio a nota in corso | `synth_filter.bas` |
| **P6 non rotto** | `--audiocheck` e `scene_sound.bas` | `P6-SND ok=32`, stessi valori di gain/pan |
| **8 voci condivise** | MiniTest offline `synth_race.bas`: `SOUND`/`PLAY`/`SCENE SOUND` sulla stessa voce | vince l'ultimo comando, `VOICE(v, 3)` è il nuovo, il preempted non migra (D-9) |
| WAV invariati (fase 1) | i 2 sample esistenti (`AudioControls.bas`, `AudioFileLoading.bas`) | suonano come oggi via mixer; nessuna verifica audio nuova |
| Formato del device (fase 1) | `Mix_QuerySpec` all'apertura | 44100/S16/2, altrimenti `STATUS(4)` = 0 (offline); mai render a un rate diverso |
| Hook senza lock (fase 1) | ispezione del corpo del hook | nessuna chiamata `Mix_*`, nessun `SDL_LockAudio` |
| WAV assorbito (fase 2) | `SOUND LOAD/PLAY/VOLUME` sui 2 sample | voce campione; formato compresso → errore esplicito |
| Mixer rimosso (fase 2) | `grep -c "Mix_" gmb_audio.c` | **0** |
| Niente env | `grep -n getenv` sui file audio | **0** |
| A/B C | `cc` e `tools/sh/zcc` sul MiniTest toccato | stesso esito |
| Dimensioni | `bash tools/sh/check_file_size.sh --quiet` | nessun file > 1200 righe |

---

## 5. Cosa **non** cambia

1. Le formule e le sentinelle di P6 (`M-SND-GAIN`, `M-SND-PAN`, `M-SND-READ`, `M-SND-ERR`).
2. I default di `SOUND` (10 % attacco, 0 decadimento, 100 % sostegno, 20 % rilascio, pw 25 %) e la
   grammatica MML allineata al C# il 2026-11-28.
3. La regola **niente env** e la regola **una sola implementazione** (D-5).
4. I nomi dei file `gmb_audio.c`/`gmb_audio.h` e le firme della facade (D-8).
5. Il comando `SCENE SOUND` non bloccante e il fatto che in headless non si senta nulla: gain e pan si
   calcolano lo stesso.

---

## 6. Rischi e mitigazioni

| Rischio | Mitigazione |
|---|---|
| Decoder WAV meno capace del mixer (niente MP3/OGG) | dichiarato: **errore esplicito** per i formati compressi; i 2 sample sono WAV PCM |
| Latenza 512 campioni avvertibile in giochi ritmici | il blocco è un parametro del device; 256 se il driver lo consente, senza toccare il motore |
| Nona sorgente posizionale che ruba una voce | limite **già reale** oggi (8 toni), ora dichiarato e verificabile; P6 non peggiora |
| Un effetto posizionale taglia la musica (e un `SOUND` spegne un tono 3D) | **dichiarato**: una voce, un proprietario, l'ultimo comando vince (D-9); per tenerle separate si usano voci diverse (convenzione); priorità di classe e bus sono rinviati (§7) |
| Test che aprono il device su una macchina con scheda audio | `AUDIO OFFLINE` come primo comando dei MiniTest, **documentato** come obbligatorio |
| `dlopen` di SDL2 assente | offline automatico; il synth resta testabile e nessun comando fallisce |
| Fase 1: il mixer resta (`dlopen` dei ~18 lib, due punti per il master, clipping dei WAV non contato) | **dichiarato** (D-2); la fase 2 è opzionale e non ha una data |
| Le due fasi si allontanano | la fase 1 è un punto d'arrivo valido: un device, un motore, un master come **valore**; non esiste uno stato «metà» |

---

## 7. Rinviato (dichiarato, non dimenticato)

**Priorità di classe, bus separati, ducking** (D-9): richiederebbero voci riservate o una precedenza
che rompe `ON n` = voce `n`; oggi una voce ha un solo proprietario e vince l'ultimo comando.
Code nominate (`SOUND STOP "bgm"`), loop MML `[ ]`, strumenti 4..63, vibrato/glide/portamento,
effetti (riverbero/coro), export su file, streaming di file grandi (il decoder carica in memoria: un
WAV da 100 MB occupa 100 MB, limite dichiarato).
