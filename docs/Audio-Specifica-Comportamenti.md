# Specifica di comportamento dell'audio — synth a 8 voci e MML

> **Stato**: specifica di comportamento, **nessun codice**. È il documento che rende verificabile
> il synth a 8 voci prima che nasca `gmb_synth.c` (piano `docs/Piano-Motore-3D.md` §4.3, riga
> «Synth 8 canali + MML», fase **P6**).
> **Metodo**: il comportamento da replicare è quello **osservabile** di `AudioManager.cs` (riferimento
> C# delle gambe gestite) e quello già in `GMBasic/native/gmb_runtime_c/src/gmb_audio.c` alla base
> `401794f0`. Nessuna copia di codice: i numeri sono contratto, l'espressione è nostra.
> **Uso**: ogni voce ha un **ID** (`M-AUD-…`). Nel piano, nei commit e nei MiniTest si cita l'ID,
> non la frase. Un ID senza controllo non è specificato; un controllo senza ID non è un oracolo.
> **Decisioni di confine** (device, mixer, voce → canale, voci condivise): `docs/ADR-Audio-Confine-Synth.md`, D-1..D-9.
> D-1/D-2 sono in **due fasi** (rev. 2026-11-28): in fase 1 il device è quello del mixer via `Mix_HookMusic`
> e i WAV restano chunk; l'assorbimento (decoder, device SDL2 core, rimozione) è la fase 2.
> Chiudono il §11 e governano `M-AUD-DEV`, `M-AUD-NOMIX`, `M-AUD-SOUND-VOL`, `M-AUD-SOUND-WAV`,
> `M-AUD-POS-LIVE` e la superficie di §6.
>
> **Perché `M-AUD-` e non `M-`**: il prefisso `M-SND*` è già occupato dall'audio **posizionale**
> (`SCENE SOUND`, P6) nella specifica 3D. `M-AUD-*` è lo stesso namespace «M» del piano, senza
> collisioni: `M-SND*` resta la sorgente posizionale, `M-AUD-*` è il motore di sintesi.

---

## 0. Come si usa questa specifica

1. **Prima di scrivere una funzione del synth** si cerca qui il suo ID: se manca, la funzione non ha
   oracolo e non si scrive (stessa regola della specifica 3D, §0).
2. **Ogni ID diventa almeno un controllo**: un MiniTest headless con `' EXPECT:` che legge i punti
   di §6, oppure un'asserzione nel selfcheck C. Le letture di §6 esistono **per questo**: senza di
   esse l'unica verifica sarebbe l'ascolto, che non è un test.
3. **Le divergenze sono dichiarate, non silenziose** (§8): dove la specifica dice *diverso da oggi* o
   *diverso dal C#*, il motivo sta accanto.
4. **I numeri sono oracoli**: default, limiti, sentinelle e arrotondamenti qui sotto sono il
   contratto. Cambiarne uno significa aggiornare specifica e test insieme.
5. **Nessun parametro passa da variabili d'ambiente** (`M-AUD-NOENV`): la modalità di prova è una
   chiamata, non un `getenv`.

---

## 1. Base comune

| ID | Comportamento | Oracolo |
|---|---|---|
| `M-AUD-RATE` | Frequenza di campionamento **44100 Hz**, canali **2**, campioni **int16**. È la stessa di oggi (`GMB_AUDIO_RATE`) e del C#. | `AUDIO.STATUS(0)` avanza di n dopo `AUDIO.RENDER(n)`; un blocco di 44100 campioni dura 1,000 s. |
| `M-AUD-AMP` | Ampiezza di **una** voce: `16000` su 32767 (≈ −6,2 dBFS). Il mix somma le voci senza normalizzare (§3): con più voci il segnale **può** saturare ed è contato (`M-AUD-CLIP`). Non c'è limiter. | `AUDIO.VOICE(v, 0)` di una voce a volume 100, onda quadra, sustain 1: **16000/32767 = 0,4883** ± 1e−4. |
| `M-AUD-QUANT` | Dal double al campione: `s = clamp(lround(x * 32767.0), −32768, 32767)`. **Arrotondamento**, non troncamento. | Onda quadra a ampiezza 0,5 → campioni esattamente `16384` e `−16384` (con troncamento darebbe 16383). |
| `M-AUD-BLOCK` | Il rendering è **indipendente dalla dimensione del blocco**: 1×1000 campioni e 10×100 danno **byte identici** (fase e stato dei filtri persistono per voce). Il device chiama il render a blocchi di 512, il test quanto vuole: stesso codice. | Due render della stessa sequenza con blocchi diversi: confronto byte a byte = 0 differenze. |
| `M-AUD-DET` | **Determinismo**: stesso programma, stessa sequenza di comandi, stesso `RESET` → stessi byte. Il rumore (`M-AUD-NOISE`) ha seed fisso per voce, non `rand()` di libc. | Due esecuzioni dello stesso MiniTest danno lo stesso hash dei campioni. |
| `M-AUD-SENT` | Sentinelle delle letture: **−1** = voce vuota, non valida o fuori 0..7 (coerente con `C-SENTINEL` della specifica 3D: −1 = assente). Nessuna lettura alza errori. | Ogni lettura provata su voce vuota, su voce 8 e su voce −1: sempre −1. |
| `M-AUD-MODE` | Due modalità, **una sola alla volta**: **device** (il motore è tirato dal callback del device: in fase 1 quello del mixer via `Mix_HookMusic`, in fase 2 quello di SDL2 core — ADR D-1) e **offline** (il tempo avanza solo con `AUDIO.RENDER`, §6). Default: device al primo comando audio; se il device non si apre (CI, headless, nessuna scheda) si resta **offline**. `AUDIO OFFLINE` forza l'offline **prima** di aprire il device; `AUDIO ONLINE` prova ad aprirlo. In device mode `AUDIO.RENDER` restituisce 0 e non fa nulla. Il corrispondente C è `gmb_audio_set_mode(0\|1)`: il selfcheck lo chiama come prima istruzione, senza `SDL_AUDIODRIVER`. | Programma che chiama `AUDIO OFFLINE` e poi `SOUND`: nessun device aperto, `AUDIO.RENDER` accetta. Senza `AUDIO OFFLINE` e senza scheda audio: stesso risultato. |
| `M-AUD-GPM` | Come per il 3D (`C-GPM`): i comandi audio girano **dentro un task GPM**; l'attesa è **park** del task, mai `gmb_sleep_ms` su un worker. Il thread del device audio **non esegue BASIC** e non tocca strutture del task: pubblica solo stato (§0ter). | `grep gmb_sleep_ms` nel percorso audio = 0; contatore di park > 0 durante un `SOUND` in device mode. |
| `M-AUD-NOENV` | Nessuna variabile d'ambiente nuova, **nemmeno nei test**: la modalità, i livelli, i seed e il device si comandano da BASIC, dal registro opzioni o dalla facade C (`gmb_audio_set_mode`, `M-AUD-MODE`). Il perimetro comprende i file di test: forzare un driver con `setenv` è la stessa violazione che leggerlo. | `grep -n "getenv\|setenv"` su `GMBasic/native/gmb_runtime_c/src/gmb_audio*.c` **e** su `tests/gmb_audio_selfcheck.c` = **0** dalla fase 1; fino ad allora un solo residuo dichiarato (ADR §6) |
| `M-AUD-ERR` | Errori **espliciti** dove il contratto lo chiede (`SOUND` con frequenza negativa, `FILTER` con tipo non intero, voce fuori range nei comandi posizionali già specificati in `M-SND-ERR`, formato audio non PCM in `M-AUD-SOUND-WAV`, fase 2). Dove il riferimento **tace** (frequenza 0, durata 0), vale `M-AUD-SOUND-QUIET`: nessun suono, nessun errore. | Un caso per riga: gli errori alzano il messaggio, i due "quiet" no. |
| `M-AUD-DEV` | **Un solo device per processo** (ADR D-1), e cambia chi lo apre. **Fase 1**: il device è quello di SDL2_mixer (`dlopen`, aperto con `Mix_OpenAudioDevice(44100, S16SYS, 2, 512)`), pull via **`Mix_HookMusic`**, che gira per primo su ogni buffer con il buffer **silenzioso** in ingresso; il formato **ottenuto** si verifica con `Mix_QuerySpec` e se non è 44100/S16/2 si va **offline** (mai render a un rate diverso). **Fase 2**: `SDL_OpenAudioDevice` di SDL2 **core** in `dlopen`, stesso render. In entrambe il render è `gmb_synth_render(stream, n)`, nessun thread nostro, blocco **512** frame (1024 con il `Mix_OpenAudio` legacy). `dlopen` o apertura falliti → offline automatico (`M-AUD-MODE`). | `AUDIO.STATUS(4)` = 1 con scheda audio, 0 in CI; fase 1: `Mix_HookMusic` nella tabella, `Mix_` > 0; fase 2: `grep -c "Mix_"` = 0 |
| `M-AUD-NOMIX` | **Il mixer `dlopen` non è assorbito nella fase 1** (ADR D-2, rev.): resta com'è oggi — tabella `g_mix`, `gmb_mix_api`, 11 simboli **+2** (`Mix_HookMusic`, `Mix_QuerySpec`) — perché è il device e il riproduttore dei WAV. Un solo device e un solo orologio, ma master in due punti e clipping dei WAV non contato (`M-AUD-CLIP`). **Fase 2**: assorbito — niente `gmb_mix_api`, niente `Mix_*`, WAV come voce campione, un solo master e un solo contatore. La promessa di P6 `M-SND-PCM` («la tabella `dlopen` non cambia») vale in fase 1 e a fortiori in fase 2. | fase 1: `grep -c "Mix_" gmb_audio.c` > 0 e solo i due simboli nuovi; fase 2: `grep -c "Mix_" GMBasic/native/gmb_runtime_c/src/gmb_audio.c` = **0** |

---

## 2. Voci

Otto voci logiche **0..7**, indipendenti, ognuna con: frequenza, durata rimanente, fase, inviluppo,
forma d'onda, pulse width, filtro, pan, volume, stato.
Le otto voci sono **un pool unico**, condiviso con l'audio posizionale P6 (`M-SND-VOICE`): **un solo
proprietario per voce** alla volta, senza priorità di classe e senza voci riservate (ADR D-9).

| ID | Comportamento | Oracolo |
|---|---|---|
| `M-AUD-VOICE` | `voice` è **0..7**; fuori range si **clampa** (0 o 7), non è un errore — come `Math.Clamp(voice, 0, MaxVoices-1)` nel C#. La voce **è** il canale (ADR D-3): `ON 3` suona sulla voce 3, sempre — anche se la voce 3 era di un tono posizionale o di un'altra sequenza, che viene interrotta (`M-AUD-RETRIG`, ADR D-9). | `SOUND ON 3, …` poi `AUDIO.VOICE(3, 2)` = 1 e `AUDIO.VOICE(2, 2)` = 0; `ON 9` suona sulla 7; `ON −2` sulla 0; con lo slot 4 (`M-SND-VOICE`: voce 3) in corso, `SOUND ON 3, 440, 0.5` lascia `AUDIO.VOICE(3, 3)` = 440. |
| `M-AUD-ALLOC` | `SOUND` prende la voce indicata (default 0). `PLAY` prende una voce **a rotazione** da un cursore che avanza di uno a ogni sequenza (0,1,…,7,0,…), così otto `PLAY` consecutivi non si rubano la voce. Il cursore **non cerca voci libere**: prende la voce che tocca e sostituisce chi c'era (`M-AUD-RETRIG`, ADR D-9). `PLAY CONCURRENT` dà **una voce per sequenza**, distinta. | Tre `PLAY` consecutivi mentre le note suonano: `AUDIO.VOICE(v, 2)` = 1 sulle voci 0, 1, 2. |
| `M-AUD-RETRIG` | Un nuovo comando sulla stessa voce **interrompe** il proprietario in corso (fase e inviluppo ripartono da zero) e lo sostituisce: nessuna coda della vecchia nota, nessuna allocazione, **nessuna migrazione**. La regola è **uniforme**, senza classi protette: vale fra `SOUND`, sequenza `PLAY`/`CONCURRENT` e tono posizionale, e **nei due sensi** (la voce campione si aggiunge dalla fase 2: in fase 1 i WAV sono canali del mixer, fuori dal pool — ADR D-2/D-9) — `SOUND` che prende la voce di uno slot, `SCENE SOUND` che prende la voce di un `SOUND` (ADR D-9). Il comando interrotto è **cancellato**, non spostato: una sequenza che perde la voce non riparte su un'altra (gli eventi già parsati restano contati da `M-AUD-EVENTS`, non vengono suonati). | `SOUND 0,440,1` seguito subito da `SOUND 0,880,0.1`: dopo 0,1 s la voce è vuota, e `AUDIO.VOICE(0, 0)` nel blocco successivo è 0. `SOUND ON 0, 440, 1` poi `SCENE SOUND 1, 880, 1, 0,0,0, 10, 100`: `AUDIO.VOICE(0, 3)` = 880; l'ordine inverso dà 440. `PLAY "T120 L4 C D E" ON 2` poi `SCENE SOUND 3, …`: nessuna voce torna occupata dopo il tono. |
| `M-AUD-ENV` | ADSR in **frazioni della durata nominale** `N` (campioni): `A = (int)(attack·N)`, `D = (int)(decay·N)`, `R = (int)(release·N)`, `S = max(0, N − A − D − R)`. Inviluppo: attacco lineare `i/A`; decadimento lineare `1 − (1−sustain)·(i−A)/D`; sustain costante `sustain`; rilascio lineare `sustain·(1 − k/R)`. Fuori: 0. La **release sta dentro** la durata: la nota finisce con l'inviluppo a 0, non lo supera. | `A=0,25 D=0 S=1 R=0,25` su 400 campioni: `env(0)=0`, `env(100)=1`, `env(200)=1`, `env(350)=0,5` (sostegno fino a 299, rilascio 300..399), `env(399)=0,01`. |
| `M-AUD-WAVE` | Sei forme, `wave` clampato 0..5, `f = freq`, `ph = frac(f·t)`: **0** `sin(2πft) + 0,1·sin(4πft)`; **1** `sin(2πft) ≥ 0 ? 1 : −1`; **2** `2·(f·t − floor(f·t + 0,5))`; **3** `4·|ph − 0,5| − 1`; **4** rumore (`M-AUD-NOISE`); **5** `ph < pw ? 1 : −1`. Oltre: sine pura. | Per ogni forma, il campione a `t = 1/(4f)` (fase 0,25) con inviluppo 1 e filtro spento: 0 → 1,0 (il secondo armonico è nullo a fase 0,25); 1 → 1; 2 → 0,5; 3 → 0; 5 → −1 con `pw = 0,25`. |
| `M-AUD-PW` | `pulse_width` clampato **0,01..0,99** (frazione del periodo), default **0,25**. Vale solo per `wave = 5`. | `PW = 0` → clampato a 0,01; con `pw = 0,5` l'onda quadra è bilanciata (media 0 su periodo intero). |
| `M-AUD-NOISE` | Rumore **per voce**: xorshift64 con seed **fisso per voce** (costante di base + indice), azzerato da `RESET`. Mai `rand()`, mai seed condiviso fra voci (oggi `gmb_audio_synth` usa un `static uint64_t` unico: dichiarato in §8). | Due voci 0 e 1 con `wave = 4`: sequenze diverse; ripetendo da `RESET`: identiche. |
| `M-AUD-PHASE` | La fase di una voce è un **double in campioni** che continua fra i blocchi: nessun click a ogni blocco (`M-AUD-BLOCK`). Cambio di frequenza su una voce: la fase **continua** (nessuno scatto a 0). | Render a blocchi di 1 campione: nessuna discontinuità alla giunzione (differenza prima/dopo ≤ pendenza massima). |

---

## 3. Mix

| ID | Comportamento | Oracolo |
|---|---|---|
| `M-AUD-MIX` | Somma **in double** delle otto voci (già scalate per volume e pan), poi `× master` (default 1,0), poi `M-AUD-QUANT`. Il mix avviene **per campione**, non per chunk: nessun `Mix_Chunk` e nessuna allocazione nel percorso di render. | Due voci identiche a volume 100, onda quadra: il campione misto è il doppio di una (16000 → 32000, ancora sotto il fondo scala); con **quattro** voci 64000 → saturato e contato in `AUDIO.STATUS(2)`. |
| `M-AUD-MASTER` | Guadagno master **0..1** (0..100 %), default 1,0, applicato **dopo** la somma. Si comanda con `SOUND VOLUME v` **senza canale** (`M-AUD-SOUND-VOL`): la forma del C# è `volume[, channel]`, quindi senza canale è il master. Oggi il valore finisce su `Mix_Volume`; in **fase 1** va **sia** al gain del synth **sia** a `Mix_Volume` per i WAV (stesso numero, due punti), in **fase 2** solo qui (ADR D-2). | `master = 0,5` dimezza il picco del mix; `master = 0` dà silenzio con `STATUS(2)` = 0. |
| `M-AUD-PAN` | Pan per voce, **legge lineare**: `gl = pan > 0 ? 1 − pan : 1`, `gr = pan < 0 ? 1 + pan : 1`, `pan` in −1..+1 (clamp). Centro = **piena ampiezza su entrambi i canali** (nessuna perdita di 3 dB), estremi = un solo canale. Default 0. | `pan = 0`: L == R == campione; `pan = −1`: R == 0, L == campione; `pan = +1`: viceversa. |
| `M-AUD-PAN-POS` | L'audio posizionale (`M-SND*`, P6) **entra nel mix** come una voce qualsiasi: `gain` della sorgente → volume della voce, `pan` della sorgente → `M-AUD-PAN` per campione, invece di un secondo percorso di sintesi. I **numeri** di `SCENE.SOUNDGAIN`/`SOUNDPAN` non cambiano: cambia solo chi li applica ai campioni. | `scene_sound.bas` resta verde con gli stessi valori (`g1= 0.5|p1= 0|…`); `--audiocheck` P6-SND resta `ok=32`. |
| `M-AUD-POS-LIVE` | Una sorgente posizionale che sta già suonando **segue** il listener: `SCENE RENDER` ricalcola gain/pan e il motore li applica alla voce dal **blocco successivo**, senza riavviare il tono (ADR D-4) e **solo se** lo slot ne è ancora il proprietario — un `RENDER` non ruba voci (ADR D-9). Oggi il tono resta congelato ai valori del comando, perché il chunk era unico: è un limite dell'architettura, non un contratto. | Sorgente fissa, camera che si allontana a metà nota: `AUDIO.VOICE(v, 0)` del blocco dopo cala, e il tono **non** riparte (campioni rimanenti continuano). |
| `M-AUD-CLIP` | Saturazione **conta**: ogni campione (L o R) che supera ±1,0 dopo `× master` incrementa un contatore per canale. Nessun soft-knee, nessun limiter, nessuna compressione: il clipping è **dichiarato e misurato**. In **fase 1** il contatore vede solo il synth: la saturazione dei WAV avviene dentro SDL_mixer ed è **invisibile** (ADR D-2); dalla fase 2 conta tutto. | `AUDIO.STATUS(2)` = 0 con una voce a volume 50; > 0 con tre voci a volume 100; `RESET` azzera. |
| `M-AUD-NOCHUNK` | Nessun `gmb_mix_chunk` per nota nel percorso del synth: la memoria è **statica** (8 voci × stato) più il buffer di render. `SOUND LOAD`/`PLAY #h` (file WAV) sono **voci campione** dello stesso motore dalla **fase 2** (`M-AUD-SOUND-WAV`, ADR D-2): in fase 1 restano chunk del mixer, ma sullo stesso device e sullo stesso orologio (D-1). | `AUDIO.RENDER` in un ciclo di 10 000 blocchi: nessuna crescita di RSS misurabile (`grep malloc` nel percorso = 0). |

---

## 4. Comandi

### 4.1 `SOUND`

`SOUND [ON voice,] freq, dur[, vol[, wave[, att, dec, sus, rel[, pw]]]]`

| ID | Comportamento | Oracolo |
|---|---|---|
| `M-AUD-SOUND` | Default: `voice = 0`, `vol = 100`, `wave = 0`, `att = 0,10`, `dec = 0,0`, `sus = 1,0`, `rel = 0,20`, `pw = 0,25` (gli stessi di `AudioManager.SoundOnVoiceAsync`, che usa 10/0/100/20/25 in percento). Parametri in **frazioni della durata** (0..1), non secondi. | `SOUND 440, 1` e `SOUND ON 0, 440, 1, 100, 0, 0.10, 0, 1, 0.20, 0.25` producono **lo stesso PCM** byte a byte. |
| `M-AUD-SOUND-QUIET` | `freq ≤ 0` oppure `dur ≤ 0`: **nessun suono**, nessuna voce occupata, nessun errore (comportamento del C# e di oggi). | `SOUND 0, 1` e `SOUND 440, 0`: dopo un render `AUDIO.VOICE(0, 2)` = 0 e il mix è silenzioso. |
| `M-AUD-SOUND-WAIT` | `SOUND` **attende** la fine della propria nota sul clock del motore: in device mode è un park del task fino all'ultimo campione; in offline mode attende che il clock arrivi a `dur` (cioè che qualcuno renda). Il comando **non** ritorna prima. | Con `AUDIO OFFLINE`, `SOUND 440, 0.1` seguito da `AUDIO.STATUS(0)` = 4410. |
| `M-AUD-SOUND-VOL` | `SOUND VOLUME v[, n]` (forma del C#: `volume[, channel]`): senza canale è il **master** (`M-AUD-MASTER`), con `n` il volume della voce `n` (clamp 0..100), applicato ai campioni successivi. Una stringa al posto di `n` (canale nominato) è accettata e **ignorata** (ADR D-3). In **fase 1** lo stesso numero va al gain del synth e a `Mix_Volume` sui canali WAV (ADR D-2). | `SOUND VOLUME 50` dimezza il picco del mix; `SOUND VOLUME 50, 0` dimezza solo la voce 0. |
| `M-AUD-SOUND-WAV` | **Due fasi** (ADR D-2). **Fase 1**: `SOUND LOAD/PLAY #h/VOLUME` restano quelli di oggi — chunk del mixer, `channel` = **canale del mixer**, i WAV non sono una delle 8 voci, il formato lo valida SDL_mixer e la gamba C99 salta i comandi come oggi (gap dichiarato); il selfcheck C, che oggi forza `SDL_AUDIODRIVER=dummy`, in fase 1 va **offline** e **parcheggia** le asserzioni sui WAV, riprendendole in fase 2. **Fase 2**: `SOUND LOAD "f" AS #h` decodifica un **PCM WAV** (8/16/24/32 bit, mono/stereo, qualunque rate ricampionato a 44100) in memoria una volta, `SOUND PLAY #h [, n] [, LOOPS k]` lo suona su una **voce** (voce `n` se numerico, altrimenti rotazione), formati compressi (MP3/OGG/ADPCM) → **errore esplicito** (`M-AUD-ERR`). | fase 1: i 2 sample suonano come oggi; fase 2: `SOUND PLAY` occupa una voce e `AUDIO.VOICE(v, 4)` conta i campioni residui, un `.mp3` alza l'errore. |

### 4.2 `SET FILTER` / `FILTER`

`FILTER voice, type, cutoff, resonance` (la forma `SET FILTER` è accettata allo stesso modo)

| ID | Comportamento | Oracolo |
|---|---|---|
| `M-AUD-FILTER` | `type`: 0 = off, 1 = low-pass, 2 = high-pass, 3 = band-pass; `cutoff` in Hz clampato **20..20000**; `resonance` in percento clampato **0..95**. Il filtro è **per voce** e vale dalla nota successiva su quella voce; `type = 0` lo spegne. | `FILTER 0,1,1000,0` poi una nota: l'energia sopra 1 kHz cala; `FILTER 0,0,…`: nessun effetto. |
| `M-AUD-FILTER-Q` | **Una sola legge**, biquad RBJ con `w0 = 2π·cutoff/rate`, `q = 0,5 + 0,095·resonance_percento` (equivalente `q = 0,5 + 9,5·res` con `res` 0..0,95), `alpha = sin(w0)/(2q)`. Oggi il codice ha **due** leggi (`apply_biquad` con la Q qui sopra, e un ramo dentro `gmb_audio_synth` con `alpha = sin(w0)·resonance`): la specifica sceglie quella di `apply_biquad`, che è quella che `FILTER` applica davvero. | Con `res = 0` il filtro è a Q 0,5; con `res = 95` il picco alla cutoff supera di ~10 dB il guadagno in banda. |
| `M-AUD-FILTER-LIVE` | Il filtro si applica **ai campioni successivi**, non a un buffer già pronto: cambiare cutoff/resonanza mentre una nota suona si sente dalla giunzione in poi, senza riavviare la nota. È la differenza rispetto ai chunk di oggi (`M-AUD-NOCHUNK`). | Nota lunga + `FILTER` a metà: `AUDIO.VOICE(v, 1)` del blocco dopo il cambio è diverso dal precedente, e la nota **non** riparte (fase continua). |

### 4.3 `PLAY` e controllo

| ID | Comportamento | Oracolo |
|---|---|---|
| `M-AUD-PLAY` | `PLAY "mml"` accoda la sequenza sul clock; `PLAY … ON n` con `n` **numerico** = voce `n` (`M-AUD-PLAY-ON`); `ON "nome"` (canale nominato) si accetta e si **ignora** (le code nominate non esistono in questa fase, §9). | `PLAY "C D E"` produce 3 eventi (`AUDIO.STATUS(3)`), ognuno con il proprio `start` in campioni (`M-AUD-MML-EVENTS`). |
| `M-AUD-PLAY-ON` | `PLAY … ON n` fissa la voce della sequenza (clamp 0..7), come `SOUND ON n`; senza `ON` la voce viene dal cursore di rotazione (`M-AUD-ALLOC`). Con `CONCURRENT` il cursore parte da `n` e prende le voci successive. | `PLAY "L4 C" ON 3`: `AUDIO.VOICE(3, 2)` = 1 e le altre voci libere. |
| `M-AUD-PLAY-CONC` | `PLAY CONCURRENT a$, b$, …`: **una voce per sequenza**, tutte a partire dallo stesso campione di inizio (simultanee), su voci **distinte** prese dal cursore a partire da `ON n` (wrap 0..7) e assegnate **atomicamente** nello stesso blocco. Oltre **8** sequenze: **errore esplicito**, nessuna parte (mai polifonia parziale). Se una voce del gruppo viene poi presa da un altro comando, si cancella **solo quella sequenza** e le altre continuano, senza riallocazione (`M-AUD-RETRIG`, ADR D-9). Oggi sulle gambe C suonano **in sequenza** (dichiarato in §8); il target è questo. | Due sequenze `L1 C` e `L1 E` in CONCURRENT: nel primo blocco `AUDIO.VOICE(v, 2)` = 1 su due voci diverse; 9 sequenze: errore e `AUDIO.VOICE(v, 2)` = 0 su 0..7. |
| `M-AUD-PLAY-LOOP` | `PLAY LOOP "mml" ON canale TIMES n`: la sequenza si ripete **n** volte sul clock, senza gap aggiuntivi (la pausa finale della sequenza resta quella scritta). `TIMES` assente = 1. | `PLAY LOOP "L8 C" ON 0 TIMES 3`: `AUDIO.STATUS(3)` = 3 e l'ultimo evento finisce a `3 × 0,25 s = 33075` campioni. |
| `M-AUD-CTRL` | `PLAY PAUSE\|RESUME\|STOP [canale\|"ALL"]`: agisce sulle **voci** e — in **fase 1** — anche sui canali del mixer per i WAV (`ALL` o argomento assente), così un file si ferma come oggi; dalla fase 2 solo voci (ADR D-2). STOP ferma e libera la voce; PAUSE congela fase e inviluppo (il clock avanza, la voce no); RESUME riprende dallo stesso campione. `"ALL"` o argomento assente = tutte. | PAUSE per 0,1 s: `AUDIO.STATUS(0)` avanza di 4410, la voce resta occupata e il picco resta quello di prima; RESUME prosegue senza ripartire. |
| `M-AUD-BUSY` | Stato di una voce = `AUDIO.VOICE(v, 2)`: 1 se sta suonando (campioni rimanenti > 0), 0 se è libera, −1 se non valida. Non esiste una lettura aggregata: «almeno una voce suona» è un OR di 0..7 fatto dal test (ADR D-6). | Dopo la fine dell'inviluppo: 0; prima: 1; voce 8: −1. |

---

## 5. MML

Grammatica (una sequenza per stringa), identica al C# `AudioManager.ParseMusicString` e a
`gmb_audio_mml_parse` dopo l'allineamento del 2026-11-28:

| ID | Comando | Comportamento |
|---|---|---|
| `M-AUD-MML-O` | `O0..O6` | ottava, clamp 0..6; una cifra |
| `M-AUD-MML-L` | `L1..L64` | lunghezza di default delle note successive, **più cifre** (`L16` = 1/16, non `L1`), clamp 1..64 |
| `M-AUD-MML-T` | `T32..T255` | tempo in BPM, clamp 32..255, default **120** |
| `M-AUD-MML-V` | `V0..V100` | volume delle note successive, clamp 0..100, default **100**, **applicato** (non solo letto) |
| `M-AUD-MML-N` | `N0..N127` | nota **MIDI assoluta** (ignora l'ottava), `f = 440·2^((n−69)/12)`, clamp 0..127 |
| `M-AUD-MML-NOTE` | `C D E F G A B` + `#`/`+` | semitono `×2^(1/12)` sulla base dell'ottava corrente (`gmb_audio_frequency(base, octave) = base·2^(octave−4)`) |
| `M-AUD-MML-OCT` | `>` `<` | ottava su/giù di uno, clamp 0..6 |
| `M-AUD-MML-PAUSE` | `P1..P64` | pausa della lunghezza indicata: evento senza frequenza (`kind = 1`), stessa durata di una nota |
| `M-AUD-MML-DOT` | `… .` | punto **singolo**: durata `× 1,5`. Due punti: il secondo è ignorato (dichiarato, §8) |
| `M-AUD-MML-TIME` | durata | `dur_secondi = 240 / (tempo · lunghezza)`; **in campioni**: `dur_campioni = lround(44100 · dur_secondi)`, minimo 1. Ritmo calcolato **in campioni interi**, mai in millisecondi |
| `M-AUD-MML-EVENTS` | eventi | ogni comando produce **un evento**: `{kind, voice, freq, dur_campioni, vol, start_campione}`, con `start` = fine dell'evento precedente. Nessun accumulo di errore: il cursore è in campioni interi |
| `M-AUD-MML-ERR` | caratteri | i caratteri non riconosciuti sono **saltati** (come oggi e come il C#): nessun errore. Una sequenza vuota produce **0 eventi** e nessuna voce occupata |

---

## 6. Punti di lettura headless (il contratto verificabile)

Sono le **uniche** superfici che rendono il synth testabile senza orecchie. Valgono in **offline
mode** (`M-AUD-MODE`); in device mode `AUDIO.RENDER` resta 0 e le altre letture riportano lo stato
dell'ultimo blocco del callback.
**Sono quattro funzioni, non otto** (ADR D-6): una per famiglia, i campi sono l'argomento `what`. Meno
superficie significa meno guide e meno MiniTest da mantenere, senza perdere una sola asserzione.

| ID | Lettura | Valore | Verifica |
|---|---|---|---|
| `M-AUD-RENDER` | `AUDIO.RENDER(n)` | rende **esattamente n** campioni (n 1..44100) nel buffer interno e fa avanzare il clock di n; ritorna n. Fuori range: 0. In device mode: 0. `n` non influenza il risultato (`M-AUD-BLOCK`) | `AUDIO.STATUS(0)` dopo 3 render da 100 = 300; render 1000 in 1 blocco == 10 blocchi da 100 (byte) |
| `M-AUD-PEAK` | `AUDIO.VOICE(v, 0)` | picco **assoluto** della voce `v` **nell'ultimo blocco reso**, post-inviluppo e post-filtro, pre-mix, normalizzato 0..1 (campione/32767). −1 se la voce è vuota o non valida | Voce a volume 100, onda quadra, sustain: 0,4883 ± 1e−4; voce vuota: −1 |
| `M-AUD-RMS` | `AUDIO.VOICE(v, 1)` | `sqrt(media(x²))` della voce `v` nell'ultimo blocco, stessa normalizzazione e stessa sentinella di `M-AUD-PEAK` | Sine a piena ampiezza: RMS = picco/√2 ± 1e−3; volume 0 su una voce **ancora occupata**: 0 (non −1); voce libera (nota finita): −1 |
| `M-AUD-BUSY-READ` | `AUDIO.VOICE(v, 2)` | 1 se la voce suona (campioni rimanenti > 0), 0 se è libera, −1 se non valida | Prima della fine: 1; dopo: 0; voce 8: −1 |
| `M-AUD-VOICE-READ` | `AUDIO.VOICE(v, 3)` e `AUDIO.VOICE(v, 4)` | frequenza in Hz e campioni rimanenti della voce; −1 se non valida | Nota `440` di 1 s resa a metà: 440 e 22050 |
| `M-AUD-CLOCK` | `AUDIO.STATUS(0)` | campioni resi dall'ultimo `RESET` (il clock del motore) | `AUDIO.RENDER(44100)` → 44100; `RESET` → 0 |
| `M-AUD-MASTER-READ` | `AUDIO.STATUS(1)` | picco assoluto del **mix** (post-master, post-saturazione) nell'ultimo blocco, 0..1 | Una voce onda quadra: 0,4883 × master; quattro voci identiche: 1,0 (satura) e `STATUS(2)` > 0 |
| `M-AUD-CLIPS` | `AUDIO.STATUS(2)` | campioni (L o R, sommati) saturati dall'ultimo `RESET` | 0 con una voce a volume 50; > 0 con tre voci a volume 100 |
| `M-AUD-EVENTS` | `AUDIO.STATUS(3)` | numero di eventi MML prodotti da `PLAY` dall'ultimo `RESET` (l'**oracolo del parser**, indipendente dalla sintesi) | `"T120 L4 C D"` → 2; `""` → 0 |
| `M-AUD-MODE-READ` | `AUDIO.STATUS(4)` | 0 = offline, 1 = device | Dopo `AUDIO OFFLINE`: 0; in CI: 0 |
| `M-AUD-EVT-READ` | `AUDIO.EVENT(i, what)` | campo dell'evento `i` (0-based): `what` **0** = start in campioni, **1** = frequenza Hz, **2** = durata in campioni, **3** = volume 0..100, **4** = tipo (0 nota, 1 pausa), **5** = voce. −1 se `i` o `what` fuori range | `"T120 L4 C D"`: evento 0 `{0, 261.63, 22050, 100, 0, v}` e evento 1 start **22050** |
| `AUDIO OFFLINE` / `AUDIO ONLINE` | modalità | vedi `M-AUD-MODE`: `AUDIO OFFLINE` è il primo comando dei MiniTest che leggono i campioni; è un **comando**, senza valore di ritorno | MiniTest che parte con `AUDIO OFFLINE`: nessun device aperto, letture deterministiche |

Regole delle letture:
1. **Nessuna lettura altera lo stato** (tranne `AUDIO.RENDER`, che avanza il clock): sono funzioni,
   non comandi, e non riavviano note.
2. **Nessuna lettura alza errori**: fuori range o stato impossibile → sentinella `−1` (`M-AUD-SENT`).
3. Le letture sono uguali sulle **quattro gambe** (IL, PCode, VM/C, C99Native): le gambe gestite
   delegano al runtime C con P/Invoke (ADR D-5), non duplicano il motore.
4. Un MiniTest che legge campioni **deve** chiamare `AUDIO OFFLINE` per prima cosa: senza, su una
   macchina con scheda audio il device si apre e il test perde il determinismo. Vale anche per il
   selfcheck C, che chiama `gmb_audio_set_mode(0)` prima di ogni altra cosa.

---

## 7. Stato rispetto al codice di oggi (base `401794f0`)

| Gruppo | Oggi | Cosa manca |
|---|---|---|
| `M-AUD-RATE`, `M-AUD-AMP`, `M-AUD-WAVE`, `M-AUD-PW`, `M-AUD-ENV` | **esistono** come funzioni pure (`gmb_audio_synth`) ma per **chunk interi**: una nota, un buffer, poi il mixer | render a voci per campione (`M-AUD-PHASE`, `M-AUD-BLOCK`) |
| `M-AUD-VOICE`, `M-AUD-RETRIG` | `gmb_audio_sound_voice` (2026-11-28): canale stabile = voce, clamp 0..7, nota che si sovrascrive | la voce deve vivere fra i blocchi (oggi è un chunk) |
| `M-AUD-ALLOC`, `M-AUD-PLAY-ON` | `PLAY` usa la voce 0; `CONCURRENT` suona in sequenza | cursore di rotazione, una voce per sequenza, `ON n` |
| `M-AUD-MIX`, `M-AUD-MASTER`, `M-AUD-CLIP`, `M-AUD-PAN` | somma nel mixer SDL (invisibile e non misurabile); pan solo nel percorso posizionale | mix per campione, master, contatore di saturazione, pan per voce |
| `M-AUD-DEV`, `M-AUD-MODE` | apertura lazy del mixer `dlopen` al primo uso; headless = nessun device | fase 1: pull sul device del mixer (`Mix_HookMusic`) + offline esplicito e clock manuale; device proprio in fase 2 |
| `M-AUD-NOMIX`, `M-AUD-SOUND-VOL`, `M-AUD-SOUND-WAV` | `SOUND LOAD/PLAY/VOLUME` solo in C# (C li salta); volume via `Mix_Volume` | fase 1: WAV invariati, master in due punti; fase 2: mixer assorbito, decoder PCM WAV, volume per voce/master nel motore |
| `M-AUD-FILTER*` | `FILTER` applica `apply_biquad` a una nota intera; **due leggi** di filtro nel file | una legge sola, per campione, viva durante la nota |
| `M-AUD-MML-*` | parser allineato al C# il 2026-11-28 (`N`, `L` multi-cifra, `V` applicato); durate in **secondi** | eventi in **campioni**, `start` esplicito, coda sul clock |
| `M-AUD-CTRL` | no-op su VM/C, parziale sul C# | STOP/PAUSE/RESUME sulle voci |
| `M-AUD-RENDER`…`M-AUD-EVT-READ` | **non esistono**: oggi l'unica lettura è `SCENE.SOUNDGAIN/PAN` (P6) | tutta la superficie di §6 |

---

## 8. Divergenze dichiarate

| ID | Tema | Oggi / C# | Specifica | Perché |
|---|---|---|---|---|
| `M-AUD-DIVERGE-CONC` | `PLAY CONCURRENT` | sulle gambe C le sequenze suonano **in sequenza**; il C# usa voci diverse in parallelo | **simultanee**, una voce per sequenza (`M-AUD-PLAY-CONC`) | senza questo `PLAY CONCURRENT` non è polifonia |
| `M-AUD-DIVERGE-FILTER` | legge del filtro | due leggi nello stesso file (`apply_biquad` e il ramo di `gmb_audio_synth`) | **una** (`M-AUD-FILTER-Q`) | due leggi = due suoni diversi per lo stesso `FILTER` |
| `M-AUD-DIVERGE-NOISE` | rumore | un `static uint64_t` condiviso da tutte le voci | **per voce**, seed fisso, azzerato da `RESET` (`M-AUD-NOISE`) | determinismo e indipendenza delle voci |
| `M-AUD-DIVERGE-QUANT` | conversione a int16 | troncamento (`(int16_t)v`) | arrotondamento `lround` (`M-AUD-QUANT`) | valori riproducibili e simmetrici; cambia i byte, non il suono |
| `M-AUD-DIVERGE-MIXER` | file WAV e device | `SOUND LOAD`/`PLAY #h`/`VOLUME` passano dal mixer `dlopen`: canale del mixer, master via `Mix_Volume` | **fase 1**: stesso device e stesso orologio, WAV ancora sul mixer con master in due punti; **fase 2**: **assorbito** — voce campione nel synth, un master e un contatore di clipping (ADR D-1/D-2) | un device solo, quindi un solo orologio; la fase 2 toglie anche i due punti del master; il costo (2 sample, decoder PCM) è dichiarato |
| `M-AUD-DIVERGE-DOT` | punto doppio | il C# accetta solo un punto, il C ignora il secondo | un punto solo, il secondo ignorato | il riferimento non ha note doppiamente puntate |
| `M-AUD-DIVERGE-TEST` | `PLAY` in offline | non esiste | in offline accoda e **ritorna** (il test possiede il clock), in device parca fino alla fine | un test deterministico non può aspettare un clock che governa lui |

---

## 9. Cosa **non** entra in questa fase (dichiarato, non dimenticato)

1. **Strumenti 4..63** (preset timbrici del riferimento): servono tabelle di timbro che non abbiamo e
   che il C# non ha; senza specifica sarebbero inventate.
2. **Vibrato, tremolo, glissando/portamento, cutoff con glide**: non esistono nemmeno in
   `AudioManager.cs`, quindi non c'è comportamento da replicare: vanno specificati a parte (con i
   loro ID) prima di scriverli.
3. **`playchord`, `syncchans`, BPM globale del motore**: idem (assenti nel riferimento C#).
4. **MML multi-traccia, loop `[ ]` annidati, cambio strumento `@`, legature**: il C# non li ha.
5. **Code nominate** (`PLAY … ON nome`, `SOUND STOP "ch1"`): il canale si accetta e si ignora
   (`M-AUD-PLAY`); le code nominate sono un secondo scheduler, fuori fase.
6. **Reverb, cori, effetti**: nessun effetto oltre al filtro per voce.
7. **Rendering su file / WAV export**, e **streaming** dei WAV: il decoder carica in memoria (un WAV
   da 100 MB occupa 100 MB, limite dichiarato); il buffer di `AUDIO.RENDER` è interno.

---

## 10. Oracoli previsti (quando il codice ci sarà)

| Gruppo | Verifica | File |
|---|---|---|
| `M-AUD-WAVE`, `M-AUD-ENV`, `M-AUD-QUANT`, `M-AUD-PHASE` | selfcheck C sul buffer: valori attesi campione per campione | `GMBasic/native/gmb_runtime_c/tests/gmb_audio_selfcheck.c` (estensione) |
| `M-AUD-VOICE`, `M-AUD-RETRIG`, `M-AUD-ALLOC`, `M-AUD-MIX`, `M-AUD-CLIP` | MiniTest offline: `AUDIO OFFLINE` + `RENDER` + `VOICE`/`STATUS` | `shared/minitests/audio/synth_voices.bas` |
| `M-AUD-FILTER*` | MiniTest offline: energia prima/dopo il filtro, cambio a nota in corso, le due leggi non devono divergere | `shared/minitests/audio/synth_filter.bas` |
| `M-AUD-MML-*`, `M-AUD-PLAY*`, `M-AUD-CTRL` | MiniTest offline: eventi in campioni via `AUDIO.EVENT(i, what)`; nessuna dipendenza dalla sintesi | `shared/minitests/audio/synth_mml.bas` |
| `M-AUD-BLOCK`, `M-AUD-DET` | MiniTest offline: stesso programma in 1 blocco e in 10 blocchi, confronto byte a byte tramite un hash calcolato in BASIC | `shared/minitests/audio/synth_block.bas` |
| `M-AUD-SOUND-WAV` (fase 2) | MiniTest offline: `SOUND LOAD/PLAY` sui 2 sample esistenti; formato compresso → errore | `shared/minitests/audio/synth_wav.bas` |
| `M-AUD-PAN-POS`, `M-AUD-POS-LIVE` | il gate 3D resta verde: `--audiocheck` (`P6-SND ok=32`) e `scene_sound.bas` invariati | `tools/sh/check_3d_oracle.sh` |
| `audio/sound_play.bas` (esistente) | invariato: la superficie compila ed esegue su tutte le gambe | `shared/minitests/audio/sound_play.bas` |

---

## 11. Decisioni chiuse (ADR `docs/ADR-Audio-Confine-Synth.md`)

Le sei domande che questa specifica lasciava aperte hanno ora una risposta vincolante, e D-9 chiude
la coabitazione delle voci fra posizionale e comandi temporali:

| # | Domanda | Decisione | Dove |
|---|---|---|---|
| 1 | Chi possiede il device | **Uno solo**: in fase 1 quello del mixer con pull via `Mix_HookMusic`, in fase 2 SDL2 **core**; offline automatico se manca | D-1 |
| 2 | Il mixer `dlopen` | **Fase 1: resta** (device + WAV, +2 simboli di pull); **fase 2: assorbito** — niente `Mix_*`, WAV = voce campione, un master e un contatore | D-2 |
| 3 | Semantica voce → canale | `voice` **è** il canale 0..7; `SOUND ON n` → voce n; `PLAY` a rotazione; `SOUND VOLUME v` = master, `v, n` = voce n | D-3 |
| 4 | P6 | Slot → voce `(slot−1) mod 8`, 8 sorgenti simultanee (limite già reale), gain/pan **live**, numeri e sentinelle invariati | D-4 |
| 5 | Gambe gestite | C# **delega** via P/Invoke; un solo motore e un solo parser MML | D-5 |
| 6 | Letture e `AUDIO OFFLINE` | **Quattro** funzioni (`RENDER`, `VOICE`, `STATUS`, `EVENT`) + `AUDIO OFFLINE` come comando | D-6 |
| 7 | Voci condivise P6 ↔ `SOUND`/`PLAY` | **Un proprietario per voce, l'ultimo comando vince** nei due sensi; nessuna priorità di classe, nessuna voce riservata; il preempted si cancella e non migra; `CONCURRENT` prenota in blocco voci distinte, > 8 sequenze è errore | D-9 |

Restano **due preferenze** dichiarate nell'ADR, che non bloccano il codice: se il gain/pan di una
sorgente in riproduzione debba seguire il listener (`M-AUD-POS-LIVE`, D-4 — reversibile a costo zero)
e la dimensione del blocco del device (512, 256 se il driver lo consente, D-1).

---

## 12. Invarianti di compatibilità (non negoziabili)

1. **Nessuna env nuova** (`M-AUD-NOENV`).
2. **Un solo device e una sola tabella `dlopen` audio**: in fase 1 è quella del mixer (`M-AUD-NOMIX`,
   +2 simboli di pull), in fase 2 è SDL2 core e quella del mixer non esiste più (`Mix_` = 0,
   `M-AUD-DEV`). La promessa di P6 (`M-SND-PCM`) vale in entrambe.
3. **P6 resta verde**: `--audiocheck` `P6-SND ok=32`, `scene_sound.bas` con gli stessi valori,
   gate 3D `diff=0`.
4. **`audio/sound_play.bas` resta verde** su C99Native e VM/C.
5. **Runtime condiviso** (§0bis): il motore vive in `gmb_runtime_c` (`gmb_synth.c`, `gmb_audio_dev.c`,
   `gmb_audio_wav.c`, `gmb_audio.c` — ognuno sotto le 1200 righe), le gambe chiamano; nessuna copia.
6. **Niente attese bloccanti su un worker** (`M-AUD-GPM`): park, non `gmb_sleep_ms`.
7. **Il thread del device non esegue BASIC e non prende lock** (ADR D-7): coda SPSC dei comandi,
   statistiche in seqlock, render funzione pura.
8. **Una voce ha un solo proprietario** (ADR D-9): `AUDIO.VOICE(v, 2)` **è** la prenotazione;
   l'ultimo comando vince nei due sensi, il preempted non migra, `RENDER` non reclama voci.
