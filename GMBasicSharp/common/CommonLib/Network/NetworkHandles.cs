// FILE: CommonLib.Network.NetworkHandles.cs

using System.Net;
using System.Net.Security;
using System.Net.Sockets;

/*
 * NetworkHandles.cs: definizioni dei tipi di handle di rete (TCP listener, client, UDP) con motori Standard e HighPerformance
 *
 * ENUM/CLASSI:
 * - NetworkEngine: enum (Standard, HighPerformance)
 * - NetworkHandle: classe base astratta con HandleId, Engine, IsSecure
 * - TcpListenerHandle: socket TCP in ascolto (motore Standard)
 * - TcpClientHandle: connessione TCP attiva con supporto SSL (motore Standard)
 * - UdpHandle: socket UDP
 * - AsyncTcpListenerHandle: listener del motore HighPerformance (TcpListener async .NET, non libuv)
 * - AsyncTcpClientHandle: client del motore HighPerformance (TcpClient+Stream async .NET, non libuv)
 */

namespace CommonLib.Network;

/// <summary>
/// Definisce il tipo di motore di rete utilizzato per creare un handle.
/// <see cref="NetworkEngine.Standard"/> utilizza i socket .NET classici (TcpListener, TcpClient, UdpClient),
/// con I/O prevalentemente bloccante/sincrono lato chiamante.
/// <see cref="NetworkEngine.HighPerformance"/> usa GLI STESSI socket .NET ma con I/O interamente
/// asincrono (accept/connect/handshake/receive via Task, nessun thread OS dedicato per connessione
/// idle) — pensato per HTTP SERVER e WebSocket, che devono gestire molte connessioni concorrenti
/// senza bloccare i worker GPM. Non usa più Libuv (rimosso 2026-08-01): lo stesso obiettivo di
/// scalabilità si ottiene con TcpListener/TcpClient/SslStream asincroni puri .NET, che si appoggiano
/// alle stesse primitive del sistema operativo (IOCP/epoll/kqueue) senza l'overhead di marshaling
/// P/Invoke verso codice nativo — stessa scelta fatta da Kestrel (ASP.NET Core) nel 2019.
/// </summary>
public enum NetworkEngine
{
    /// <summary>Motore di rete standard basato su socket .NET (System.Net.Sockets).</summary>
    Standard,
    /// <summary>Motore di rete asincrono basato su socket .NET (TcpListener/TcpClient/SslStream),
    /// pensato per scenari con molte connessioni concorrenti (HTTP SERVER, WebSocket).</summary>
    HighPerformance
}

/// <summary>
/// Classe base astratta per tutte le risorse di rete gestite dall'interprete.
/// Fornisce un identificatore numerico univoco (HandleId) generato in modo thread-safe
/// tramite Interlocked.Increment, e definisce il motore di rete associato.
/// Ogni handle concreto (TCP listener, client, UDP) estende questa classe e ne implementa
/// la logica di rilascio delle risorse tramite Dispose().
/// </summary>
public abstract class NetworkHandle : IDisposable
{
    private static int _nextHandleId = 1;

    /// <summary>
    /// Identificatore numerico univoco per questo handle di rete.
    /// Assegnato dal costruttore in modo atomico e thread-safe.
    /// </summary>
    public int HandleId { get; }

    /// <summary>
    /// Motore di rete (Standard o HighPerformance) utilizzato da questo handle.
    /// Determinato al momento della creazione e immutabile per l'intero ciclo di vita.
    /// </summary>
    public NetworkEngine Engine { get; }

    /// <summary>
    /// Indica se questa connessione è protetta da SSL/TLS.
    /// Se true, le operazioni di I/O devono passare attraverso un SslStream.
    /// Impostata a false per default, viene attivata dopo un handshake SSL riuscito.
    /// </summary>
    public bool IsSecure { get; set; }

    /// <summary>
    /// Inizializza un nuovo handle di rete, assegnandogli un ID univoco.
    /// </summary>
    /// <param name="engine">Il motore di rete da associare a questo handle.</param>
    protected NetworkHandle(NetworkEngine engine)
    {
        HandleId = Interlocked.Increment(ref _nextHandleId);
        Engine = engine;
        IsSecure = false;
    }

    /// <summary>
    /// Rilascia le risorse native e gestite associate all'handle.
    /// Ogni sottoclasse deve implementare questo metodo per chiudere socket,
    /// stream e altre risorse specifiche.
    /// </summary>
    public abstract void Dispose();
}

/// <summary>
/// Rappresenta un socket TCP in ascolto (creato con TCP OPEN).
/// Incapsula un TcpListener .NET standard per accettare connessioni in entrata.
/// Supporta sia il motore Standard che HighPerformance (usa comunque TcpListener
/// per l'accettazione, delegando le connessioni accettate al motore specificato).
/// </summary>
public sealed class TcpListenerHandle : NetworkHandle
{
    /// <summary>
    /// Il listener TCP .NET sottostante. Usato per accettare connessioni client
    /// e ottenere informazioni sul binding locale (porta, indirizzo).
    /// </summary>
    public TcpListener Listener { get; }

    /// <summary>
    /// Crea un nuovo listener TCP che ascolta sull'indirizzo e porta specificati.
    /// </summary>
    /// <param name="address">L'indirizzo IP su cui mettersi in ascolto (es. IPAddress.Any).</param>
    /// <param name="port">La porta TCP (1-65535) su cui ascoltare.</param>
    /// <param name="engine">Il motore di rete da associare a questo listener.</param>
    public TcpListenerHandle(IPAddress address, int port, NetworkEngine engine)
        : base(engine)
    {
        Listener = new TcpListener(address, port);
    }

    /// <summary>
    /// Ferma il listener e rilascia le risorse. Gli errori durante la chiusura
    /// vengono silenziosamente ignorati per garantire una terminazione robusta.
    /// </summary>
    public override void Dispose()
    {
        try { Listener.Stop(); }
        catch { }
    }
}

/// <summary>
/// Rappresenta una connessione TCP attiva con un client remoto, creata tramite
/// TCP ACCEPT (connessione in entrata) o TCP CONNECT (connessione in uscita).
/// Incapsula un Socket .NET con supporto SSL/TLS opzionale tramite SslStream.
/// Se <see cref="IsSecure"/> è true, leggere e scrivere attraverso <see cref="SslStream"/>
/// anziché direttamente sul socket.
/// </summary>
public sealed class TcpClientHandle : NetworkHandle
{
    /// <summary>
    /// Il socket TCP .NET sottostante per questa connessione client.
    /// Utilizzato per tutte le operazioni di I/O in chiaro (non SSL).
    /// </summary>
    public Socket ClientSocket { get; }

    /// <summary>
    /// Lo stream SSL/TLS associato a questa connessione, se sicura.
    /// È null per le connessioni in chiaro (non crittografate).
    /// Quando <see cref="NetworkHandle.IsSecure"/> è true, tutte le operazioni
    /// di lettura e scrittura devono passare attraverso questo stream.
    /// </summary>
    public SslStream? SslStream { get; set; }

    /// <summary>
    /// Crea un nuovo handle client associato al socket specificato.
    /// </summary>
    /// <param name="clientSocket">Il socket TCP già connesso al peer remoto.</param>
    /// <param name="engine">Il motore di rete con cui è stato creato questo handle.</param>
    public TcpClientHandle(Socket clientSocket, NetworkEngine engine)
        : base(engine)
    {
        ClientSocket = clientSocket;
    }

    /// <summary>
    /// Rilascia tutte le risorse: chiude l'SslStream se presente, effettua
    /// shutdown bidirezionale del socket, poi lo chiude e lo smaltisce.
    /// L'ordine di chiusura è importante: prima SslStream, poi shutdown, poi close.
    /// </summary>
    public override void Dispose()
    {
        SslStream?.Dispose();

        if (ClientSocket.Connected)
        {
            try { ClientSocket.Shutdown(SocketShutdown.Both); }
            catch { }
        }
        ClientSocket.Close();
        ClientSocket.Dispose();
    }
}

/// <summary>
/// Rappresenta un socket UDP per l'invio e la ricezione di datagrammi senza connessione.
/// Incapsula un Socket .NET configurato come Dgram su UDP. Se la porta specificata
/// è maggiore di zero, il socket viene immediatamente associato (bind) a quella porta
/// per poter ricevere datagrammi.
/// </summary>
public sealed class UdpHandle : NetworkHandle
{
    /// <summary>
    /// Il socket UDP .NET sottostante. Configurato con AddressFamily.InterNetwork,
    /// SocketType.Dgram e ProtocolType.Udp.
    /// </summary>
    public Socket UdpSocket { get; }

    /// <summary>
    /// Crea un nuovo socket UDP, opzionalmente bindandolo a una porta specifica.
    /// </summary>
    /// <param name="port">La porta locale su cui ricevere datagrammi. Usare 0 per una porta assegnata dal sistema.</param>
    /// <param name="engine">Il motore di rete da associare a questo handle.</param>
    public UdpHandle(int port, NetworkEngine engine)
        : base(engine)
    {
        UdpSocket = new Socket(AddressFamily.InterNetwork, SocketType.Dgram, ProtocolType.Udp);
        // Bind sempre (anche porta 0 = porta effimera auto): senza bind, LOCALPORT() resta 0
        // e SendTo da un socket non bound funziona ma la porta locale non è interrogabile.
        UdpSocket.Bind(new IPEndPoint(IPAddress.Any, port));
    }

    /// <summary>
    /// Chiude e smaltisce il socket UDP. Gli errori vengono silenziosamente ignorati.
    /// </summary>
    public override void Dispose()
    {
        UdpSocket.Close();
        UdpSocket.Dispose();
    }
}

/// <summary>
/// Rappresenta un listener TCP gestito dal motore HighPerformance (async .NET puro, non libuv —
/// vedi <see cref="NetworkEngine.HighPerformance"/>). Incapsula un <see cref="System.Net.Sockets.TcpListener"/>
/// standard, ma pensato per essere usato SOLO con i metodi asincroni di <c>HighPerformance</c>
/// (<c>BeginAcceptTcpClient</c>), mai con l'accept sincrono/bloccante del motore Standard.
/// </summary>
public sealed class AsyncTcpListenerHandle : NetworkHandle
{
    /// <summary>Il listener TCP .NET sottostante, già in ascolto (<c>Start()</c> già chiamato).</summary>
    public System.Net.Sockets.TcpListener Listener { get; }

    private int _disposed;

    /// <summary>Crea un nuovo handle per un listener già creato e avviato dal HighPerformance manager.</summary>
    /// <param name="listener">Il listener .NET già in ascolto (<c>Listener.Start()</c> già chiamato).</param>
    /// <param name="engine">Il motore di rete (deve essere HighPerformance).</param>
    public AsyncTcpListenerHandle(System.Net.Sockets.TcpListener listener, NetworkEngine engine)
        : base(engine)
    {
        Listener = listener;
    }

    /// <summary>Ferma il listener. Idempotente: HTTP SERVER STOP e il check di cancellazione del
    /// listener loop (Gpm.Http.Server.cs, ExecuteHttpListenerLoop) possono entrambi tentare di
    /// rilasciare lo stesso handle.</summary>
    public override void Dispose()
    {
        if (Interlocked.Exchange(ref _disposed, 1) != 0) return;
        try { Listener.Stop(); } catch { /* ignore */ }
    }
}

/// <summary>
/// Rappresenta una connessione client TCP gestita dal motore HighPerformance (async .NET puro).
/// Incapsula un <see cref="System.Net.Sockets.TcpClient"/> e lo <see cref="System.IO.Stream"/>
/// attivo per l'I/O (il <see cref="System.Net.Sockets.NetworkStream"/> di base, sostituito da un
/// <see cref="System.Net.Security.SslStream"/> dopo un handshake TLS riuscito — vedi
/// <see cref="NetworkHandle.IsSecure"/>). Usato esclusivamente con <see cref="NetworkEngine.HighPerformance"/>.
/// </summary>
public sealed class AsyncTcpClientHandle : NetworkHandle
{
    /// <summary>Il client TCP .NET sottostante.</summary>
    public System.Net.Sockets.TcpClient Client { get; }

    /// <summary>Lo stream attivo per lettura/scrittura: <see cref="System.Net.Sockets.NetworkStream"/>
    /// in chiaro, o un <see cref="System.Net.Security.SslStream"/> dopo l'handshake TLS.</summary>
    public System.IO.Stream Stream { get; set; }

    private int _disposed;

    /// <summary>True se <see cref="Dispose"/> è già stato eseguito (per decidere se questo handle
    /// va ancora contato come connessione attiva nelle metriche — vedi HighPerformance.CloseHandle).</summary>
    public bool IsDisposed => _disposed != 0;

    /// <summary>L'endpoint remoto del peer connesso, o null se non disponibile.</summary>
    public System.Net.IPEndPoint? RemoteEndPoint => Client.Client?.RemoteEndPoint as System.Net.IPEndPoint;

    /// <summary>L'endpoint locale di questa connessione, o null se non disponibile.</summary>
    public System.Net.IPEndPoint? LocalEndPoint => Client.Client?.LocalEndPoint as System.Net.IPEndPoint;

    /// <summary>Crea un nuovo handle per una connessione client già stabilita.</summary>
    /// <param name="client">Il client TCP .NET già connesso al peer remoto.</param>
    /// <param name="engine">Il motore di rete (deve essere HighPerformance).</param>
    public AsyncTcpClientHandle(System.Net.Sockets.TcpClient client, NetworkEngine engine)
        : base(engine)
    {
        Client = client;
        Stream = client.GetStream();
    }

    /// <summary>Segna l'handle come disposto. Restituisce true solo la prima volta che viene
    /// chiamato (transizione "ancora attivo" → "chiuso") — usato da HighPerformance.CloseHandle
    /// per decrementare le metriche di connessioni attive una sola volta, anche se Dispose viene
    /// chiamato più volte sullo stesso handle (percorsi di chiusura concorrenti/idempotenti).</summary>
    public bool MarkDisposed() => Interlocked.Exchange(ref _disposed, 1) == 0;

    public override void Dispose()
    {
        if (!MarkDisposed()) return;
        try { Stream.Dispose(); } catch { /* ignore */ }
        try { Client.Close(); } catch { /* ignore */ }
        Client.Dispose();
    }
}
