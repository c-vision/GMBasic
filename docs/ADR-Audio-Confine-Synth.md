# ADR — Confine del synth a 8 voci: device, mixer, voce → canale

> **Stato**: accettato il 2026-11-28. **Solo decisioni e documentazione: nessuna riga di codice.**
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
   `ParseSoundVolumeStatement`); sulle gambe C quei comandi sono oggi **saltati**.
5. **Il parser C# di `SOUND` copre solo il tono**: `SOUND [ON v,] freq, dur, vol, wave, att, dec, sus,
   rel, pw`. `SOUND VOLUME` ha la forma **`volume[, channel]`** (`IAudioService.SetVolume(int volume,
   int? channel)`), quindi `SOUND VOLUME 50` = master, non «voce 50».
6. **Nessuna env, un'implementazione sola**: regole §"ENVIRONMENT VARIABLES" e §0bis di `AGENTS.md`;
   C# può chiamare C via P/Invoke (regola C#→C), il C non chiama C#.
7. **GPM (§0ter)**: il thread del device audio non esegue BASIC e non prende in ostaggio un worker.

---

## 2. Decisioni

### D-1 — Il device è **uno**, del synth, ed è SDL2 **core** audio in `dlopen`

**Decisione.** Il synth apre **un solo** device audio, tramite `SDL_OpenAudioDevice` **caricato a
runtime** (`dlopen`/`LoadLibrary` di SDL2 **core**, non di SDL2_mixer), in un nuovo file
`gmb_audio_dev.c` che entra in `GMB_ALL_SRCS` (core). Il device è **pull**: il callback di SDL chiama
`gmb_synth_render(stream, n)`; nessuna coda di push, nessun thread nostro.

**Perché.**
- **Il core non deve linkare SDL** (fatto 2): il device poteva stare solo in `dlopen` o nella
  variante UI. `dlopen` di SDL2 core costa una manciata di simboli e **non** trascina la catena dei
  codec, quindi rispetta l'invariante del runtime minimo.
- **Pull e non push.** Il render è una **funzione pura** di `(stato, n)`; è la stessa funzione che i
  MiniTest chiamano offline. Con `SDL_QueueAudio` avremmo bisogno di un secondo thread che rende "a
  tempo", cioè un altro orologio da sincronizzare: esattamente ciò che stiamo eliminando.
- **Un device per processo.** `SDL_InitSubSystem(SDL_INIT_AUDIO)` è già un singleton SDL: due device
  aperti contemporaneamente (synth + mixer) renderebbero impossibile definire master, saturazione e
  latenza comuni.

**Conseguenze.**
- Se `dlopen` o `SDL_OpenAudioDevice` falliscono → **modalità offline automatica** (`M-AUD-MODE`):
  nessun errore fatale, nessun device, tutte le letture di `AUDIO.*` restano valide. Non è un
  ripiego silenzioso: è la modalità con cui girano CI e MiniTest.
- Blocco del device: **512 campioni** (≈ 11,6 ms a 44100 Hz), 1024 solo se il driver lo impone. Il
  render resta indipendente dal blocco (`M-AUD-BLOCK`).
- Latenza dichiarata: 512 campioni di buffer + il buffer del driver; non è un target "pro audio",
  è un target di gioco con test deterministici.

**Alternative rifiutate.**
1. **Linkare SDL2** nel core: rompe l'invariante del runtime minimo (fatto 2) e trascina SDL in ogni
   processo che vuole solo il synth.
2. **Tenere SDL2_mixer come device** e aggiungere le voci sopra: il mixer non espone un callback che
   possiamo rendere noi; si resta su chunk e su `Mix_Volume`, cioè sul modello che vogliamo togliere.
3. **Due device (uno del synth, uno del mixer)**: due orologi, due livelli, master e contatore di
   clipping non definibili. Rifiutato in radice.

---

### D-2 — Il synth **assorbe** il mixer `dlopen`: la tabella `gmb_mix_api` esce dal percorso audio

**Decisione.** Il synth sostituisce il mixer, non ci convive:
- `g_mix`, `gmb_mix_api`, `gmb_mix_open()` e l'`dlopen` di **SDL2_mixer** vengono **rimossi** dal
  percorso audio; a migrazione finita il file non contiene **nessun** simbolo `Mix_*`.
- I WAV diventano una **voce campione**: `SOUND LOAD "f" AS #h` decodifica il file **una volta** in
  memoria (PCM lineare), `SOUND PLAY #h` avvia una voce che legge quel buffer.
- Decoder minimo in `gmb_audio_wav.c`: **RIFF/WAVE PCM** 8/16/24/32 bit, mono/stereo, qualunque
  sample rate con ricampionamento lineare a 44100. Formati compressi (MP3/OGG/ADPCM) → **errore
  esplicito** (`M-AUD-ERR`), mai silenzio.
- `SOUND VOLUME v` = master (`M-AUD-MASTER`), `SOUND VOLUME v, n` = volume della voce `n`
  (forma del C#, fatto 5). `SOUND PLAY #h [, channel] [, LOOPS n]`: `channel` numerico = voce,
  stringa = ignorata (code nominate fuori perimetro, `M-AUD-PLAY`).

**Perché.**
- **Due device sono già esclusi da D-1**, quindi «tenere il mixer per i WAV» non è un'opzione
  separata: sarebbe la terza alternativa già rifiutata.
- Il **costo è misurato**: 2 sample, 7 righe, nessuna gamba C che già li esegua (fatto 4). Un decoder
  PCM WAV è ~150 righe di C puro; assorbire costa meno che mantenere due percorsi e due semantichè di
  volume.
- Si guadagna una **dipendenza in meno** (`SDL2_mixer` + la sua catena) e una classe di fallimenti in
  meno in CI (mixer assente → oggi playback disabilitato, domani nessun problema).

**Conseguenze.**
- `SOUND LOAD/PLAY/VOLUME` passano dalle gambe C# **e** C allo stesso motore: prima volta che questi
  comandi hanno un comportamento unico.
- Il gate cambia segno: da «la tabella `dlopen` non cresce» (`M-AUD-DLOPEN`, vecchia promessa di P6) a
  **«la tabella non esiste più»**: `grep -c "Mix_" gmb_audio.c` = **0**.
- Il master e il contatore di clipping (`M-AUD-CLIP`) valgono anche per i WAV e per l'audio
  posizionale: un solo punto in cui si può saturare.

**Alternative rifiutate.**
1. **Synth accanto al mixer** (separato): due device (vietato da D-1), oppure un solo device con due
   percorsi di mix, cioè due master e due contatori. Il confine diventerebbe indefinibile: quale
   percorso risponde a `SOUND VOLUME`?
2. **Assorbire solo il device e lasciare i WAV al mixer**: non è tecnicamente possibile con un device
   solo.
3. **Tenere il mixer e non fare il synth**: fuori richiesta (P6 vuole il synth a 8 voci).

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
`SOUND`, sequenza `PLAY`/`CONCURRENT`, voce campione di `SOUND PLAY #h`). La regola è una sola, senza
eccezioni di classe:

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

## 3. Ordine di migrazione (un solo passo: nessuno stato misto in HEAD)

Due device aperti insieme sono esclusi da D-1, quindi **non esiste un commit intermedio** in cui il
synth apre il proprio device e il mixer il suo. L'ordine interno a un PR è libero; il punto di arrivo
è uno solo:

1. `gmb_synth.c` — voci, mix, inviluppo, forme, filtro, sequencer MML, eventi in campioni (nessun
   device, nessuna allocazione per campione).
2. `gmb_audio_dev.c` — `dlopen` SDL2 core, `SDL_OpenAudioDevice` pull, coda SPSC, seqlock, offline.
3. `gmb_audio_wav.c` — decoder PCM WAV + voce campione.
4. Riimplementazione dei comandi e della facade (D-8) sopra il motore; `AUDIO.*` (D-6).
5. Rimozione di `gmb_mix_api`, dell'`dlopen` di SDL2_mixer e dei simboli `Mix_*`; rimozione di
   `GMB_GFX_SDL_MIXER` dal CMake se resta senza consumatori.
6. P6: `gmb_audio_sound_panned` → voce dello slot (D-4, D-9); `gmb_3d_audio.c` invariato nei numeri.

Nessun passo può essere rilasciato da solo se lascia due motori accesi: si può fare in un PR unico.

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
| WAV | `SOUND LOAD/PLAY/VOLUME` sui 2 sample esistenti | suonano; formato compresso → errore esplicito |
| Mixer rimosso | `grep -c "Mix_" gmb_audio.c` | **0** |
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
| Migrazione grande in un PR unico | confine per file (D-1/D-2/D-3/D-8) e verifica per ID: ogni passo ha il suo oracolo |

---

## 7. Rinviato (dichiarato, non dimenticato)

**Priorità di classe, bus separati, ducking** (D-9): richiederebbero voci riservate o una precedenza
che rompe `ON n` = voce `n`; oggi una voce ha un solo proprietario e vince l'ultimo comando.
Code nominate (`SOUND STOP "bgm"`), loop MML `[ ]`, strumenti 4..63, vibrato/glide/portamento,
effetti (riverbero/coro), export su file, streaming di file grandi (il decoder carica in memoria: un
WAV da 100 MB occupa 100 MB, limite dichiarato).
