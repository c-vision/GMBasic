using CommonLib;
// FILE: Interpreter.AudioManager.cs
/*
Documentazione, Comandi BASIC Suggeriti ed Esempi
Ecco un'analisi di ogni funzionalità offerta dalla nuova classe, con la documentazione, il comando BASIC che possiamo implementare e un esempio di utilizzo.

1. Riproduzione di Suoni Singoli
Metodo C#: Sound(double frequency, double duration, int volume)
Descrizione: Riproduce un singolo tono alla frequenza e per la durata specificate. La chiamata è sincrona e blocca l'esecuzione finché il suono non è terminato.
Comando BASIC Suggerito: SOUND frequenza, durata [, volume]
Esempio di Utilizzo BASIC:
code
Basic
10 REM Emette un LA a 440 Hz per mezzo secondo
20 SOUND 440, 0.5

30 REM Emette un suono più acuto a volume ridotto
40 SOUND 880, 0.2, 50

2. Riproduzione di Melodie (Bloccante)
Metodo C#: Play(string musicString)
Descrizione: Interpreta ed esegue una stringa in Music Macro Language (MML). La chiamata è sincrona e l'interprete 
attende la fine della melodia prima di procedere.
Comando BASIC Suggerito: PLAY musicString$
Esempio di Utilizzo BASIC:

10 REM Esegue una semplice melodia e attende la sua fine
20 PLAY "T180 L8 O4 C D E F G A B > C"
30 PRINT "Melodia terminata."

3. Gestione di Canali Audio Nominati
Metodi C#: PlayNamedAsync(string name, ...) e StopNamedAsync(string name)
Descrizione: Permettono di avviare e fermare una sequenza musicale su un "canale" identificato da un nome. Questo è 
perfetto per gestire separatamente musica di sottofondo ed effetti sonori.
Comandi BASIC Suggeriti:
PLAY musicString$ ON canal_name$
SOUND STOP canal_name$
Esempio di Utilizzo BASIC:

10 REM Fa partire una musica di sottofondo sul canale "bgm"
20 PLAY "T120 O3 C G >C" ON "bgm"

30 REM ...il gioco continua...
40 PRINT "Premi un tasto per un effetto sonoro"
50 WAITKEY

60 REM Esegue un effetto sonoro senza interrompere la musica
70 SOUND 1500, 0.1

80 REM Ferma la musica di sottofondo
90 SOUND STOP "bgm"

4. Riproduzione Concorrente
Metodo C#: PlayConcurrentAsync(params string[] musicStrings)
Descrizione: Riproduce più sequenze musicali contemporaneamente, avviandole nello stesso momento.
Comando BASIC Suggerito: PLAY CONCURRENT musicString1$, musicString2$, ...
Esempio di Utilizzo BASIC:

10 REM Esegue un accordo di DO maggiore suonando 3 note insieme
20 PLAY CONCURRENT "O4 C", "O4 E", "O4 G"

5. Riproduzione in Loop
Metodo C#: PlayLoopAsync(string name, string musicString, int repeatCount)
Descrizione: Riproduce una sequenza musicale su un canale nominato ripetutamente. La ripetizione può essere infinita (-1) 
o per un numero fisso di volte.
Comando BASIC Suggerito: PLAY LOOP musicString$ ON canal_name$ [TIMES numero_ripetizioni]
Esempio di Utilizzo BASIC:

10 REM Fa partire una musica di sottofondo in loop infinito
20 PLAY LOOP "L4 C G A G" ON "theme"

30 REM ...il gioco prosegue con la musica in sottofondo...

40 REM Esempio con 5 ripetizioni
50 PLAY LOOP "L8 C G" ON "jingle" TIMES 5
6. Coda Musicale (Music Queue)
Metodi C#: QueuePlay(string musicString), ClearQueue(), GetQueueLength()
Descrizione: Permette di accodare più sequenze musicali che verranno eseguite una dopo l'altra, senza interruzioni.
Comandi BASIC Suggeriti:
PLAY QUEUE musicString$
PLAY CLEAR QUEUE
SOUND.QUEUELEN() (come funzione)
Esempio di Utilizzo BASIC:

10 PRINT "Accodamento di tre jingle..."
20 PLAY QUEUE "T200 L8 C E G"  ' Jingle 1
30 PLAY QUEUE "T200 L8 G E C"  ' Jingle 2
40 PLAY QUEUE "T200 L4 >C"     ' Jingle 3
50 PRINT "Elementi in coda:"; SOUND.QUEUELEN() ' Stampa 3
60 REM L'AudioManager eseguirà i tre jingle in sequenza

7. Controllo Globale della Riproduzione
Metodi C#: PauseAll(), ResumeAll(), StopAllAsync()
Descrizione: Forniscono un controllo master su tutto l'audio in esecuzione.
Comandi BASIC Suggeriti:
SOUND PAUSE
SOUND RESUME
SOUND STOP ALL
Esempio di Utilizzo BASIC:

10 PLAY LOOP "C D E" ON "music"
20 PRINT "Musica in esecuzione. Premi P per pausare/riprendere."
30 K$ = INKEY$
40 IF K$ = "p" THEN SOUND PAUSE
50 SLEEP 2000
60 IF K$ = "p" THEN SOUND RESUME
70 SOUND STOP ALL

8. Funzioni di Stato
Metodi C#: IsPlaying, IsNamedTaskActive(string name), GetActiveTaskCount(), GetActiveTaskNames()
Descrizione: Permettono al programma BASIC di interrogare lo stato del motore audio.
Funzioni BASIC Suggerite:
SOUND.PLAYING (variabile di sistema, 0 o -1)
SOUND.ISACTIVE(canal_name$)
SOUND.TASKCOUNT()
SOUND.TASKS$() (restituisce un array di stringhe)
Esempio di Utilizzo BASIC:

10 PLAY "C D E" ON "melody"
20 WHILE SOUND.ISACTIVE("melody")
30   PRINT "La melodia sta ancora suonando..."
40   SLEEP 100
50 WEND
60 PRINT "Melodia 'melody' terminata.

 * AudioManager per l'Interprete BASIC
 * 
 * Questa classe è un motore audio avanzato per l'interprete, che va ben oltre la semplice
 * riproduzione di suoni. Sfrutta SDL2_mixer per fornire un audio multicanale, asincrono
 * e di alta qualità, permettendo l'integrazione di comandi musicali potenti e flessibili
 * direttamente nel linguaggio BASIC.
 * 
 * L'architettura è basata su Task asincroni di .NET, il che significa che la musica e i suoni
 * possono essere eseguiti in background senza bloccare l'esecuzione dei programmi BASIC,
 * una caratteristica fondamentale per un interprete con supporto alla concorrenza (goroutines).
 *
 * --- Funzionalità Chiave Implementate ---
 *
 * 1.  Qualità Sonora Superiore:
 *     Invece di onde sinusoidali pure, il generatore di suoni implementa un envelope ADSR
 *     (Attack, Decay, Sustain, Release) e armoniche aggiuntive. Questo produce toni
 *     più naturali e gradevoli, simili a quelli di uno strumento musicale.
 * 
 * 2.  Efficienza tramite Caching:
 *     I toni generati vengono salvati in una cache. Se la stessa nota (stessa frequenza
 *     e durata) viene richiesta di nuovo, viene recuperata istantaneamente dalla memoria,
 *     riducendo drasticamente il carico sulla CPU durante l'esecuzione di melodie complesse.
 *
 * 3.  Riproduzione Sincrona e Asincrona:
 *     - SOUND/PLAY (sincrono): Per un comportamento fedele ai BASIC classici, dove
 *       l'interprete attende la fine del suono/melodia.
 *     - SOUND.ASYNC/PLAY.ASYNC (asincrono): Comandi estesi che avviano la riproduzione
 *       e restituiscono immediatamente il controllo al programma BASIC.
 *
 * --- 🎵 Nuove Funzionalità Concorrenti e Comandi BASIC Suggeriti ---
 * 
 * 1. Riproduzione Nominata (Canali Virtuali):
 *    Permette di gestire più tracce audio (es. musica di sottofondo ed effetti sonori)
 *    in modo indipendente, identificandole con un nome.
 *    - C#: `PlayNamedAsync("background", "C D E")`, `StopNamedAsync("background")`
 *    - BASIC Suggerito:
 *      `PLAY "C D E" ON "background"`
 *      `SOUND STOP "background"`
 *
 * 2. Sistema di Code (Playlist):
 *    Aggiunge sequenze musicali a una coda che le eseguirà una dopo l'altra.
 *    - C#: `QueuePlay("C D E")`, `ClearQueue()`, `GetQueueLength()`
 *    - BASIC Suggerito:
 *      `PLAY QUEUE "C D E"`
 *      `PLAY QUEUE "F G A"`
 *      `PLAY CLEAR QUEUE`
 *      `PRINT SOUND.QUEUELEN`
 *
 * 3. Riproduzione Concorrente:
 *    Riproduce più sequenze musicali contemporaneamente per creare armonie e accordi.
 *    - C#: `PlayConcurrentAsync("C E G", "E G B")`
 *    - BASIC Suggerito: `PLAY CONCURRENT "C E G", "E G B"`
 *
 * 4. Loop e Ripetizioni:
 *    Esegue una melodia ripetutamente, in modo infinito o per un numero fisso di volte.
 *    - C#: `PlayLoopAsync("drums", "C P C P", -1)` (infinito), `PlayLoopAsync("melody", "C D E", 3)` (3 volte)
 *    - BASIC Suggerito:
 *      `PLAY LOOP "C P" ON "drums"` (infinito)
 *      `PLAY LOOP "C D E" ON "melody" TIMES 3`
 *
 * 5. Controlli Avanzati e Funzioni di Stato:
 *    Fornisce comandi per un controllo globale (pausa, riprendi, stop totale) e funzioni
 *    per interrogare lo stato del motore audio.
 *    - C#: `PauseAll()`, `ResumeAll()`, `StopAllAsync()`, `IsNamedTaskActive("bgm")`
 *    - BASIC Suggerito:
 *      `SOUND PAUSE`
 *      `SOUND RESUME`
 *      `SOUND STOP ALL`
 *      `IF SOUND.ISACTIVE("bgm") THEN ...`
 *
 * Questa classe trasforma l'interprete da un semplice esecutore di codice a una piattaforma
 * multimediale capace di gestire scenari audio complessi, ideale per giochi o applicazioni
 * interattive. 🚀🎶
 */

// FILE: Interpreter.AudioManager.cs

using System.Collections.Concurrent;
using System.Text.RegularExpressions;
using SDL2;
using static SDL2.SDL_mixer;

namespace CommonGraphics;

public class AudioManager : IDisposable
{
    private bool _isAudioInitialized;
    private readonly Lock _lock = new();
    private readonly Dictionary<int, IntPtr> _toneCache = new();

    /// <summary>
    /// Traccia tutti i task di riproduzione asincroni generici (non nominati),
    /// come quelli avviati da PlayAsync e PlayConcurrentAsync.
    /// Viene periodicamente pulito dai task completati.
    /// </summary>
    private readonly List<Task> _activeTasks = new();
    
    /// <summary>
    /// Mappa i nomi dei "canali" virtuali ai loro CancellationTokenSource.
    /// Questo permette di controllare (es. fermare) individualmente le tracce audio
    /// nominate avviate tramite PlayNamedAsync o PlayLoopAsync.
    /// L'uso di ConcurrentDictionary garantisce la thread-safety.
    /// </summary>
    private readonly ConcurrentDictionary<string, CancellationTokenSource> _namedTasks = new();
    
    /// <summary>
    /// Coda in stile FIFO (First-In, First-Out) per i comandi musicali da eseguire
    /// in sequenza tramite il comando QueuePlay.
    /// </summary>
    private readonly Queue<PlayCommand> _playQueue = new();
    
    /// <summary>
    /// Riferimento al task in background che si occupa di processare la _playQueue.
    /// È nullo o completato se la coda è vuota, altrimenti è in esecuzione.
    /// </summary>
    private Task? _queueProcessor;

    // Frequenze base per l'ottava 4 (standard MIDI)
    private static readonly Dictionary<string, double> BaseFrequencies = new()
    {
        {"C", 261.63}, {"C+", 277.18}, {"D", 293.66}, {"D+", 311.13}, 
        {"E", 329.63}, {"F", 349.23}, {"F+", 369.99}, {"G", 392.00}, 
        {"G+", 415.30}, {"A", 440.00}, {"A+", 466.16}, {"B", 493.88}
    };

    private record PlayCommand(CommandType Type, double Frequency = 0, double Duration = 0, 
                              int Volume = 100, int Octave = 4, int NoteLength = 4, int Tempo = 120);

    private enum CommandType { Note, Pause }

    public AudioManager()
    {
        // Inizializzazione posticipata al primo utilizzo (SOUND/PLAY).
        // Cosi headless/test_exec non tentano Mix_OpenAudio e non crashano.
    }

    private void InitializeAudio()
    {
        lock (_lock)
        {
            if (_isAudioInitialized) return;

            // Inizializzato in SdlManager
            // // Inizializza SDL Audio
            // if (SDL.SDL_Init(SDL.SDL_INIT_AUDIO) < 0)
            // {
            //     throw new InvalidOperationException($"Errore inizializzazione SDL Audio: {SDL.SDL_GetError()}");
            // }

            // ponytail: headless/test_exec skips SDL init — audio stays no-op, no throw
            if (Mix_OpenAudio(44100, SDL.AUDIO_S16LSB, 2, 1024) < 0)
            {
                SDL.SDL_QuitSubSystem(SDL.SDL_INIT_AUDIO);
                return;
            }

            // Alloca canali per la riproduzione simultanea
            Mix_AllocateChannels(16);
            
            _isAudioInitialized = true;
            //Console.WriteLine("AudioManager inizializzato correttamente.");
        }
    }

    /// <summary>Lazy init: chiama InitializeAudio() al primo utilizzo. Ritorna true se l'audio è disponibile.</summary>
    private bool TryEnsureInitialized()
    {
        if (!_isAudioInitialized) InitializeAudio();
        return _isAudioInitialized;
    }

    /// <summary>
    /// Riproduce un singolo tono con frequenza, durata, volume, forma d'onda e ADSR.
    // --- Voice System ---
    private const int MaxVoices = 8;
    private readonly int?[] _voiceChannels = new int?[MaxVoices]; // voice ID → SDL channel
    private int _nextPlayVoice; // rotazione voice per i task PLAY (polifonia)
    private readonly VoiceFilter[] _voiceFilters = new VoiceFilter[MaxVoices];

    private struct VoiceFilter
    {
        public int Type; // 0=Off, 1=LowPass, 2=HighPass, 3=BandPass
        public double Cutoff; // Hz
        public double Resonance; // 0-1
    }

    /// <summary>
    /// Riproduce un tono su un voice specifico. Ogni voice è indipendente.
    /// voice: 0-7. Se un voice è già attivo, viene fermato e sovrascritto.
    /// </summary>
    public async Task SoundOnVoiceAsync(int voice, double frequency, double duration,
        int volume = 100, int waveform = 0, double attack = -1, double decay = -1,
        double sustain = -1, double release = -1, int pulseWidth = 25)
    {
        if (!TryEnsureInitialized()) return;
        voice = Math.Clamp(voice, 0, MaxVoices - 1);
        volume = Math.Clamp(volume, 0, 100);
        var waveType = Math.Clamp(waveform, 0, 5);
        pulseWidth = Math.Clamp(pulseWidth, 1, 99);

        if (attack < 0) attack = 10;
        if (decay < 0) decay = 0;
        if (sustain < 0) sustain = 100;
        if (release < 0) release = 20;

        // Stop existing voice
        if (_voiceChannels[voice].HasValue)
        {
            var oldCh = _voiceChannels[voice]!.Value;
            Mix_HaltChannel(oldCh);
        }

        var chunk = CreateWaveformChunk(frequency, duration, waveType,
            attack / 100.0, decay / 100.0, sustain / 100.0, release / 100.0,
            pulseWidth / 100.0, _voiceFilters[voice]);

        if (chunk == IntPtr.Zero) return;

        // Use a dedicated channel per voice (voice index tracks SDL channel)
        var channel = Mix_PlayChannel(voice, chunk, 0);
        if (channel == -1) return;

        _voiceChannels[voice] = channel;
        Mix_Volume(channel, (int)(volume * MIX_MAX_VOLUME / 100.0));

        await Task.Run(() =>
        {
            while (Mix_Playing(channel) != 0)
                SDL.SDL_Delay(10);
            _voiceChannels[voice] = null;
        });
    }

    /// <summary>
    /// Riproduce un singolo tono su voice 0 (backward compat).
    /// </summary>
    public async Task SoundAsync(double frequency, double duration, int volume = 100,
        int waveform = 0, double attack = -1, double decay = -1,
        double sustain = -1, double release = -1, int pulseWidth = 25)
    {
        await SoundOnVoiceAsync(0, frequency, duration, volume, waveform,
            attack, decay, sustain, release, pulseWidth);
    }

    /// <summary>
    /// Imposta parametri filtro per un voice. Il filtro viene applicato al
    /// prossimo SOUND su quel voice (biquad IIR integrato nel wave buffer).
    /// </summary>
    public void SetVoiceFilter(int voice, int type, double cutoffHz, double resonancePct)
    {
        voice = Math.Clamp(voice, 0, MaxVoices - 1);
        _voiceFilters[voice] = new VoiceFilter
        {
            Type = Math.Clamp(type, 0, 3),
            Cutoff = Math.Clamp(cutoffHz, 20, 20000),
            Resonance = Math.Clamp(resonancePct / 100.0, 0.0, 0.95)
        };
    }

    public void StopVoice(int voice)
    {
        voice = Math.Clamp(voice, 0, MaxVoices - 1);
        if (_voiceChannels[voice].HasValue)
        {
            Mix_HaltChannel(_voiceChannels[voice]!.Value);
            _voiceChannels[voice] = null;
        }
    }

    /// <summary>
    /// Crea chunk audio SDL con forma d'onda, ADSR, pulse width e filtro biquad.
    /// </summary>
    private IntPtr CreateWaveformChunk(double frequency, double duration, int waveform,
        double attack, double decay, double sustain, double release,
        double pulseWidth, VoiceFilter filter)
    {
        const int sampleRate = 44100;
        const double amplitude = 16000;
        var samples = (int)(sampleRate * duration);
        if (samples <= 0) return IntPtr.Zero;

        var wave = new byte[samples * 4];

        var attackSamples = (int)(attack * samples);
        var decaySamples = (int)(decay * samples);
        var releaseSamples = (int)(release * samples);
        var sustainSamples = samples - attackSamples - decaySamples - releaseSamples;
        if (sustainSamples < 0) sustainSamples = 0;

        // Biquad filter state
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
        double x1 = 0, x2 = 0, y1 = 0, y2 = 0;

        if (filter.Type != 0 && filter.Cutoff > 0)
        {
            var w0 = 2.0 * Math.PI * filter.Cutoff / sampleRate;
            var alpha = Math.Sin(w0) * filter.Resonance;
            var cosW0 = Math.Cos(w0);
            var a0 = 1.0 + alpha;

            switch (filter.Type)
            {
                case 1: // LowPass
                    b0 = (1.0 - cosW0) / 2.0 / a0;
                    b1 = (1.0 - cosW0) / a0;
                    b2 = (1.0 - cosW0) / 2.0 / a0;
                    a1 = (-2.0 * cosW0) / a0;
                    a2 = (1.0 - alpha) / a0;
                    break;
                case 2: // HighPass
                    b0 = (1.0 + cosW0) / 2.0 / a0;
                    b1 = -(1.0 + cosW0) / a0;
                    b2 = (1.0 + cosW0) / 2.0 / a0;
                    a1 = (-2.0 * cosW0) / a0;
                    a2 = (1.0 - alpha) / a0;
                    break;
                case 3: // BandPass
                    b0 = alpha / a0;
                    b1 = 0;
                    b2 = -alpha / a0;
                    a1 = (-2.0 * cosW0) / a0;
                    a2 = (1.0 - alpha) / a0;
                    break;
            }
        }

        for (var i = 0; i < samples; i++)
        {
            var t = (double)i / sampleRate;
            var envelope = AudioUtils.CalculateEnvelopeADSR(i, attackSamples, decaySamples,
                sustainSamples, releaseSamples, sustain);

            double value;
            switch (waveform)
            {
                case 0: // Sine
                    value = Math.Sin(2 * Math.PI * frequency * t);
                    value += 0.1 * Math.Sin(4 * Math.PI * frequency * t);
                    break;

                case 1: // Square
                    value = Math.Sin(2 * Math.PI * frequency * t) >= 0 ? 1.0 : -1.0;
                    break;

                case 2: // Sawtooth
                    value = 2.0 * (frequency * t - Math.Floor(frequency * t + 0.5));
                    break;

                case 3: // Triangle
                    var phase = frequency * t - Math.Floor(frequency * t);
                    value = 4.0 * Math.Abs(phase - 0.5) - 1.0;
                    break;

                case 4: // Noise
                    value = Random.Shared.NextDouble() * 2.0 - 1.0;
                    break;

                case 5: // Pulse with configurable duty cycle
                    value = (frequency * t - Math.Floor(frequency * t)) < pulseWidth ? 1.0 : -1.0;
                    break;

                default:
                    value = Math.Sin(2 * Math.PI * frequency * t);
                    break;
            }

            // Apply biquad filter
            if (filter.Type != 0)
            {
                var x0 = value;
                var y0 = b0 * x0 + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
                // Clamp to prevent blowup
                y0 = Math.Clamp(y0, -2.0, 2.0);
                x2 = x1; x1 = x0;
                y2 = y1; y1 = y0;
                value = y0;
            }

            var sample = (short)(amplitude * envelope * Math.Clamp(value, -1.0, 1.0));
            wave[i * 4] = (byte)(sample & 0xFF);
            wave[i * 4 + 1] = (byte)((sample >> 8) & 0xFF);
            wave[i * 4 + 2] = (byte)(sample & 0xFF);
            wave[i * 4 + 3] = (byte)((sample >> 8) & 0xFF);
        }

        return Mix_QuickLoad_RAW(wave, (uint)wave.Length);
    }

    /// <summary>
    /// Versione sincrona di SOUND per compatibilità
    /// </summary>
    public void Sound(double frequency, double duration, int volume = 100)
    {
        SoundAsync(frequency, duration, volume).Wait();
    }

    /// <summary>
    /// Esegue una stringa di comandi musicali in stile BASIC PLAY.
    /// Versione migliorata con parsing più robusto e supporto per più comandi.
    /// </summary>
    public async Task PlayAsync(string musicString, CancellationToken cancellationToken = default)
    {
        if (!TryEnsureInitialized() || string.IsNullOrWhiteSpace(musicString)) return;

        var playbackCancellation = CancellationTokenSource.CreateLinkedTokenSource(cancellationToken);

        try
        {
            var commands = ParseMusicString(musicString);
            var task = ExecutePlayCommands(commands, playbackCancellation.Token);
            
            lock (_lock)
            {
                _activeTasks.Add(task);
            }
            
            await task;
        }
        finally
        {
            lock (_lock)
            {
                _activeTasks.RemoveAll(t => t.IsCompleted);
            }
            playbackCancellation.Dispose();
        }
    }

    /// <summary>
    /// Esegue una sequenza musicale su un canale nominato per controllo granulare
    /// </summary>
    public async Task PlayNamedAsync(string name, string musicString, CancellationToken cancellationToken = default)
    {
        if (!TryEnsureInitialized() || string.IsNullOrWhiteSpace(musicString) || string.IsNullOrWhiteSpace(name)) 
            return;

        // Ferma eventuale riproduzione precedente con lo stesso nome
        await StopNamedAsync(name);

        var namedCancellation = CancellationTokenSource.CreateLinkedTokenSource(cancellationToken);
        
        lock (_lock)
        {
            _namedTasks[name] = namedCancellation;
        }

        try
        {
            var commands = ParseMusicString(musicString);
            await ExecutePlayCommands(commands, namedCancellation.Token);
        }
        finally
        {
            lock (_lock)
            {
                _namedTasks.TryRemove(name, out _);
            }
            namedCancellation.Dispose();
        }
    }

    /// <summary>
    /// Aggiunge comandi alla coda di riproduzione per esecuzione sequenziale
    /// </summary>
    public void QueuePlay(string musicString)
    {
        if (!TryEnsureInitialized() || string.IsNullOrWhiteSpace(musicString)) return;

        var commands = ParseMusicString(musicString);
        
        lock (_lock)
        {
            foreach (var command in commands)
            {
                _playQueue.Enqueue(command);
            }
            
            // Avvia il processore della coda se non è già attivo
            if (_queueProcessor?.IsCompleted != false)
            {
                _queueProcessor = ProcessQueueAsync();
            }
        }
    }

    /// <summary>
    /// Riproduce più sequenze musicali contemporaneamente
    /// </summary>
    public async Task PlayConcurrentAsync(params string[] musicStrings)
    {
        if (!TryEnsureInitialized() || musicStrings.Length == 0) return;

        var tasks = musicStrings
            .Where(s => !string.IsNullOrWhiteSpace(s))
            .Select(s => PlayAsync(s))
            .ToArray();

        await Task.WhenAll(tasks);
    }

    /// <summary>
    /// Riproduce sequenze musicali in loop su canali nominati
    /// </summary>
    public Task PlayLoopAsync(string name, string musicString, int repeatCount = -1)
    {
        return Task.Run(async () =>
        {
            var count = 0;
            while ((repeatCount == -1 || count < repeatCount) && 
                   IsNamedTaskActive(name))
            {
                await PlayNamedAsync($"{name}_loop_{count}", musicString);
                count++;
                
                if (repeatCount != -1 && count >= repeatCount) break;
            }
        });
    }

    /// <summary>
    /// Processa la coda di riproduzione sequenziale
    /// </summary>
    private async Task ProcessQueueAsync()
    {
        while (true)
        {
            PlayCommand? command;
            
            lock (_lock)
            {
                if (_playQueue.Count == 0) break;
                command = _playQueue.Dequeue();
            }
            try
            {
                switch (command.Type)
                {
                    case CommandType.Note:
                        await SoundAsync(command.Frequency, command.Duration, command.Volume);
                        break;
                    case CommandType.Pause:
                        await Task.Delay((int)(command.Duration * 1000));
                        break;
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Errore processamento coda: {ex.Message}");
            }
        }
    }

    /// <summary>
    /// Ferma una riproduzione nominata specifica
    /// </summary>
    public async Task StopNamedAsync(string name)
    {
        CancellationTokenSource? cancellation;
        
        lock (_lock)
        {
            if (_namedTasks.TryGetValue(name, out cancellation))
            {
                _namedTasks.TryRemove(name, out _);
            }
        }

        if (cancellation != null)
        {
            cancellation.Cancel();
            await Task.Delay(50); // Breve attesa per completamento
            cancellation.Dispose();
        }
    }

    /// <summary>
    /// Ferma tutte le riproduzioni attive
    /// </summary>
    public async Task StopAllAsync()
    {
        // Ferma tutte le riproduzioni nominate
        string[] activeNames;
        lock (_lock)
        {
            activeNames = _namedTasks.Keys.ToArray();
        }

        foreach (var name in activeNames)
        {
            await StopNamedAsync(name);
        }

        // Ferma tutti i task attivi
        Task[] tasks;
        lock (_lock)
        {
            tasks = _activeTasks.Where(t => !t.IsCompleted).ToArray();
        }

        // Ferma l'audio su tutti i canali
        Mix_HaltChannel(-1);

        // Aspetta che tutti i task si completino
        try
        {
            await Task.WhenAll(tasks.Select(t => Task.Run(async () =>
            {
                try { await t; } catch { /* Ignora cancellazioni */ }
            })));
        }
        catch
        {
            // Ignora le eccezioni di cancellazione
        }

        // Pulisce la coda
        lock (_lock)
        {
            _playQueue.Clear();
            _activeTasks.Clear();
        }
    }

    /// <summary>
    /// Mette in pausa tutte le riproduzioni
    /// </summary>
    public void PauseAll()
    {
        Mix_Pause(-1);
    }

    /// <summary>
    /// Riprende tutte le riproduzioni
    /// </summary>
    public void ResumeAll()
    {
        Mix_Resume(-1);
    }

    /// <summary>
    /// Verifica se un task nominato è attivo
    /// </summary>
    public bool IsNamedTaskActive(string name)
    {
        lock (_lock)
        {
            return _namedTasks.ContainsKey(name);
        }
    }

    /// <summary>
    /// Ottiene il numero di riproduzioni attive
    /// </summary>
    public int GetActiveTaskCount()
    {
        lock (_lock)
        {
            return _activeTasks.Count(t => !t.IsCompleted) + _namedTasks.Count;
        }
    }

    /// <summary>
    /// Ottiene i nomi delle riproduzioni attive
    /// </summary>
    public string[] GetActiveTaskNames()
    {
        lock (_lock)
        {
            return _namedTasks.Keys.ToArray();
        }
    }

    /// <summary>
    /// Pulisce la coda di riproduzione
    /// </summary>
    public void ClearQueue()
    {
        lock (_lock)
        {
            _playQueue.Clear();
        }
    }

    /// <summary>
    /// Ottiene il numero di elementi in coda
    /// </summary>
    public int GetQueueLength()
    {
        lock (_lock)
        {
            return _playQueue.Count;
        }
    }

    /// <summary>
    /// Versione sincrona di PLAY per compatibilità
    /// </summary>
    public void Play(string musicString)
    {
        PlayAsync(musicString).Wait();
    }

    /// <summary>
    /// Ferma la riproduzione corrente (per compatibilità)
    /// </summary>
    public async Task StopPlayAsync()
    {
        await StopAllAsync();
    }

    /// <summary>
    /// Parsing migliorato della stringa musicale
    /// </summary>
    private List<PlayCommand> ParseMusicString(string musicString)
    {
        var commands = new List<PlayCommand>();
        var regex = new Regex(@"([CDEFGAB][#+-]?\.?|O[0-6]|L[1-9]\d*|T[1-9]\d*|P[1-9]\d*|V[0-9]\d*|\.|<|>|N[0-9]\d*)", 
                             RegexOptions.IgnoreCase);
        
        var matches = regex.Matches(musicString.ToUpper());
        var currentOctave = 4;
        var currentNoteLength = 4;
        var tempo = 120;
        var volume = 100;

        foreach (Match match in matches)
        {
            var token = match.Value;
            var isDotted = token.EndsWith('.');
            if (isDotted) token = token[..^1];

            switch (token[0])
            {
                case 'O': // Ottava
                    if (int.TryParse(token[1..], out var octave))
                        currentOctave = Math.Clamp(octave, 0, 6);
                    break;

                case 'L': // Lunghezza nota
                    if (int.TryParse(token[1..], out var length))
                        currentNoteLength = Math.Clamp(length, 1, 64);
                    break;

                case 'T': // Tempo
                    if (int.TryParse(token[1..], out var newTempo))
                        tempo = Math.Clamp(newTempo, 32, 255);
                    break;

                case 'V': // Volume
                    if (int.TryParse(token[1..], out var newVolume))
                        volume = Math.Clamp(newVolume, 0, 100);
                    break;

                case 'P': // Pausa
                    if (int.TryParse(token[1..], out var pauseLength))
                    {
                        var duration = AudioUtils.CalculateNoteDuration(pauseLength, tempo);
                        if (isDotted) duration *= 1.5;
                        commands.Add(new PlayCommand(CommandType.Pause, Duration: duration));
                    }
                    break;

                case 'N': // Nota MIDI (0-127)
                    if (int.TryParse(token[1..], out var midiNote))
                    {
                        var frequency = AudioUtils.MidiNoteToFrequency(midiNote);
                        var duration = AudioUtils.CalculateNoteDuration(currentNoteLength, tempo);
                        if (isDotted) duration *= 1.5;
                        commands.Add(new PlayCommand(CommandType.Note, frequency, duration, volume));
                    }
                    break;

                case '>': // Ottava su
                    currentOctave = Math.Min(currentOctave + 1, 6);
                    break;

                case '<': // Ottava giù
                    currentOctave = Math.Max(currentOctave - 1, 0);
                    break;

                default: // Note normali
                    var noteName = token.Replace("#", "+");
                    if (BaseFrequencies.TryGetValue(noteName, out var baseFreq))
                    {
                        var frequency = AudioUtils.CalculateFrequency(baseFreq, currentOctave);
                        var duration = AudioUtils.CalculateNoteDuration(currentNoteLength, tempo);
                        if (isDotted) duration *= 1.5;
                        commands.Add(new PlayCommand(CommandType.Note, frequency, duration, volume));
                    }
                    break;
            }
        }

        return commands;
    }

    /// <summary>
    /// Esegue i comandi musicali. Ogni task PLAY usa un VOICE dedicato (0..7) così più
    /// PLAY (LOOP/CONCURRENT/NAMED) suonano SIMULTANEAMENTE su canali SDL diversi.
    /// Prima usava SoundAsync → voice 0 fisso → ogni nota interrompeva le altre voci
    /// (le 3 voci polifoniche suonavano in sequenza: l'ultima vinceva).
    /// </summary>
    private async Task ExecutePlayCommands(List<PlayCommand> commands, CancellationToken cancellationToken, int voice = -1)
    {
        if (voice < 0)
        {
            lock (_lock) { voice = _nextPlayVoice; _nextPlayVoice = (_nextPlayVoice + 1) % MaxVoices; }
        }
        foreach (var command in commands)
        {
            cancellationToken.ThrowIfCancellationRequested();

            switch (command.Type)
            {
                case CommandType.Note:
                    await SoundOnVoiceAsync(voice, command.Frequency, command.Duration, command.Volume);
                    break;

                case CommandType.Pause:
                    await Task.Delay((int)(command.Duration * 1000), cancellationToken);
                    break;
            }
        }
    }

    /// <summary>
    /// Ottiene o crea un tono dalla cache
    /// </summary>
    private IntPtr GetOrCreateTone(int frequency, double duration)
    {
        var key = HashCode.Combine(frequency, (int)(duration * 1000));
        
        if (_toneCache.TryGetValue(key, out var cachedChunk))
            return cachedChunk;

        var wave = GenerateAdvancedWave(frequency, duration);
        if (wave.Length == 0) return IntPtr.Zero;

        var chunk = Mix_QuickLoad_RAW(wave, (uint)wave.Length);
        if (chunk != IntPtr.Zero && _toneCache.Count < 100) // Limita la cache
        {
            _toneCache[key] = chunk;
        }

        return chunk;
    }

    /// <summary>
    /// Genera un'onda con envelope ADSR per suoni più naturali
    /// </summary>
    private byte[] GenerateAdvancedWave(int frequency, double duration)
    {
        const int sampleRate = 44100;
        const double amplitude = 16000; // Aumentato per migliore qualità
        var samples = (int)(sampleRate * duration);
        var wave = new byte[samples * 4]; // Stereo 16-bit

        // Parametri ADSR (Attack, Decay, Sustain, Release)
        var attackTime = Math.Min(0.01, duration * 0.1);
        var releaseTime = Math.Min(0.05, duration * 0.2);
        var attackSamples = (int)(attackTime * sampleRate);
        var releaseSamples = (int)(releaseTime * sampleRate);

        for (var i = 0; i < samples; i++)
        {
            var time = (double)i / sampleRate;
            var envelope = AudioUtils.CalculateEnvelope(i, samples, attackSamples, releaseSamples);
            
            // Genera onda sinusoidale con leggera distorsione armonica
            var sineWave = Math.Sin(2 * Math.PI * frequency * time);
            var harmonics = 0.1 * Math.Sin(4 * Math.PI * frequency * time); // Seconda armonica
            
            var value = (short)(amplitude * envelope * (sineWave + harmonics));
            
            // Stereo: stesso valore per entrambi i canali
            wave[i * 4] = (byte)(value & 0xFF);          // Left low
            wave[i * 4 + 1] = (byte)((value >> 8) & 0xFF); // Left high
            wave[i * 4 + 2] = (byte)(value & 0xFF);          // Right low
            wave[i * 4 + 3] = (byte)((value >> 8) & 0xFF);   // Right high
        }

        return wave;
    }

    /// <summary>
    /// Pulisce la cache dei toni
    /// </summary>
    public void ClearCache()
    {
        lock (_lock)
        {
            foreach (var chunk in _toneCache.Values)
            {
                Mix_FreeChunk(chunk);
            }
            _toneCache.Clear();
        }
    }

    /// <summary>
    /// Imposta il volume generale per tutti i canali
    /// </summary>
    public void SetMasterVolume(int volume)
    {
        volume = Math.Clamp(volume, 0, 100);
        var sdlVolume = (int)(volume * MIX_MAX_VOLUME / 100.0);
        
        // Imposta il volume per tutti i canali allocati
        for (var channel = 0; channel < Mix_AllocateChannels(-1); channel++)
        {
            Mix_Volume(channel, sdlVolume);
        }
    }

    /// <summary>
    /// Verifica se l'audio è disponibile e inizializzato
    /// </summary>
    public bool IsAudioAvailable => _isAudioInitialized;

    /// <summary>
    /// Verifica se l'audio è in riproduzione
    /// </summary>
    public bool IsPlaying => GetActiveTaskCount() > 0 || Mix_Playing(-1) > 0 || GetQueueLength() > 0;

    private readonly Dictionary<int, IntPtr> _loadedChunks = new();
    private int _nextChunkId = 1;

    public int LoadAudioFile(string filePath)
    {
        // Il mixer DEVE essere aperto prima di Mix_QuickLoad_RAW, altrimenti il chunk
        // è vuoto/silenzioso. Prima mancava TryEnsureInitialized qui → niente audio.
        if (!TryEnsureInitialized()) return -1;
        if (!File.Exists(filePath))
            throw new FileNotFoundException($"File audio non trovato: {filePath}");

        var path = Path.GetFullPath(filePath);
        var ext = Path.GetExtension(path).ToLowerInvariant();

        if (ext != ".wav")
            throw new NotSupportedException($"Formato audio non supportato: {ext}. Usare file .wav.");

        var fileBytes = File.ReadAllBytes(path);
        if (fileBytes.Length < 44 || fileBytes[0] != 'R' || fileBytes[1] != 'I' || fileBytes[2] != 'F' || fileBytes[3] != 'F')
            throw new InvalidDataException("File WAV non valido: magic RIFF mancante.");

        // Parsa i chunk RIFF per trovare il data chunk REALE: alcuni WAV hanno chunk
        // extra tra fmt e data (es. 'FLLR', 'LIST', 'fact', padding) → dataStart NON è
        // sempre 44. Prima assumeva 44 fisso: il PCM risultava spostato → suono
        // impercettibile/distorto (verificato con un WAV 2ch con chunk FLLR).
        var pos = 12; // dopo RIFF....WAVE
        var dataSize = 0;
        var dataStart = -1;
        while (pos + 8 <= fileBytes.Length)
        {
            var cid = System.Text.Encoding.ASCII.GetString(fileBytes, pos, 4);
            var csize = BitConverter.ToInt32(fileBytes, pos + 4);
            if (cid == "data")
            {
                dataSize = csize;
                dataStart = pos + 8;
                break;
            }
            pos += 8 + csize + (csize & 1);
        }
        if (dataStart < 0 || dataSize <= 0 || dataStart + dataSize > fileBytes.Length)
            throw new InvalidDataException("File WAV non valido: chunk data non trovato.");
        var pcmData = new byte[dataSize];
        Buffer.BlockCopy(fileBytes, dataStart, pcmData, 0, dataSize);

        var chunk = Mix_QuickLoad_RAW(pcmData, (uint)pcmData.Length);
        if (chunk == IntPtr.Zero)
            throw new InvalidOperationException($"Impossibile caricare l'audio WAV: {SDL.SDL_GetError()}");

        var id = _nextChunkId++;
        _loadedChunks[id] = chunk;
        return id;
    }

    /// <summary>Riproduce un file audio caricato.</summary>
    public void PlayAudioFile(int handle)
    {
        if (!TryEnsureInitialized()) return;
        if (!_loadedChunks.TryGetValue(handle, out var chunk))
            throw new ArgumentException($"Handle audio {handle} non valido.");

        var channel = Mix_PlayChannel(-1, chunk, 0);
        if (channel == -1)
            throw new InvalidOperationException($"Impossibile riprodurre l'audio: {SDL.SDL_GetError()}");
    }

    /// <summary>Imposta il volume per un canale specifico o per tutti (null).</summary>
    public void SetVolume(int volume, int? channel = null)
    {
        volume = Math.Clamp(volume, 0, 100);
        var sdlVolume = (int)(volume * MIX_MAX_VOLUME / 100.0);

        if (channel.HasValue)
            Mix_Volume(channel.Value, sdlVolume);
        else
            for (var ch = 0; ch < Mix_AllocateChannels(-1); ch++)
                Mix_Volume(ch, sdlVolume);
    }

    public void Dispose()
    {
        lock (_lock)
        {
            if (!TryEnsureInitialized()) return;

            StopAllAsync().Wait();
            ClearCache();
            
            foreach (var chunk in _loadedChunks.Values)
                Mix_FreeChunk(chunk);
            _loadedChunks.Clear();

            Mix_CloseAudio();
            SDL.SDL_QuitSubSystem(SDL.SDL_INIT_AUDIO);
            _isAudioInitialized = false;
            
            Console.WriteLine("AudioManager rilasciato.");
        }
    }
}