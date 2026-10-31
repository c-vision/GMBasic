// FILE: IlRuntime.Message.cs — namespace MESSAGE lato runtime IL.
// Trasporti: "local" (bus in-process) e "tcp://host:port" (client verso un broker,
// line protocol SUB/PUB/MSG). La ricezione avviene su un THREAD dedicato che dispatcha
// gli handler come TASK GPM (DispatchHandlerAsGpmOrDirect) — mai sul thread esterno (pattern IoWatch).
using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Net.Sockets;
using System.Reflection;
using System.Threading;

namespace Basic.IlBackend;

public static partial class IlRuntime
{
    private sealed class MBus
    {
        public string Uri = "";
        public List<string> Subs = new();
        public TcpClient? Tcp;
        public Thread? Recv;
        public volatile bool Stop;
        public int Port;
        public string Host = "";
        /// <summary>Il socket è scritto da due thread (PUB dal thread BASIC, ACK/NACK dal
        /// thread di ricezione): senza lock due write possono interlacciarsi sul NetworkStream.</summary>
        public readonly object WriteLock = new();
        /// <summary>CONTATORI DI CONSEGNA (MESSAGE.DELIVERY(bus)): per bus, monotoni, azzerati
        /// alla CONNECT. Scritti dal thread BASIC (sent) e dal thread di ricezione (recv/ack/
        /// nack/dup), che è l'unico a toccare quelli di ricezione → nessun lock necessario.</summary>
        public long Sent, Received, Acked, Nacked, Dup, MaxSeq;
    }
    private sealed class MSub
    {
        public string Id = "";
        public string Topic = "";
        public int Bus;
        public int HandlerSymbolId = -1;
        public MethodInfo? Handler;
    }

    private static readonly Dictionary<int, MBus> _mbus = new();
    private static readonly Dictionary<string, MSub> _msub = new(StringComparer.OrdinalIgnoreCase);
    private static int _mbusSeq = 1, _msubSeq = 1;

    /// <summary>MESSAGE CONNECT "&lt;uri&gt;" AS bus → handle del bus ("local" o "tcp://host:port").</summary>
    public static long MessageConnect(string uri)
    {
        var id = _mbusSeq++;
        var b = new MBus { Uri = uri ?? "" };
        if (!string.IsNullOrEmpty(uri) && uri.StartsWith("tcp://", StringComparison.OrdinalIgnoreCase))
        {
            var hp = uri.Substring(6);
            var colon = hp.LastIndexOf(':');
            if (colon > 0) { b.Host = hp.Substring(0, colon); int.TryParse(hp.Substring(colon + 1), out b.Port); }
            try
            {
                b.Tcp = new TcpClient();
                b.Tcp.Connect(b.Host, b.Port);
                var stream = b.Tcp.GetStream();
                b.Recv = new Thread(() => RecvLoop(id, stream)) { IsBackground = true };
                b.Recv.Start();
            }
            catch
            {
                /* Connessione fallita: il bus NON è TCP. Lasciare `Tcp` non-null (client creato ma
                 * non connesso) faceva prendere a PUBLISH il ramo tcp → SendTcp su stream morto,
                 * eccezione ingoiata, messaggio MAI consegnato: il test `messaging/message_tcp_parity`
                 * vedeva «done» senza la callback, mentre le gambe C (VM/C, C99Native) consegnano
                 * in-process (il loro connect fallito lascia il bus non-tcp). Qui si allinea:
                 * connect fallita ⇒ bus LOCALE. */
                try { b.Tcp?.Close(); } catch { }
                b.Tcp = null;
            }
        }
        _mbus[id] = b;
        return id;
    }

    private static void RecvLoop(int bus, NetworkStream stream)
    {
        try
        {
            using var reader = new StreamReader(stream);
            string? line;
            while ((line = reader.ReadLine()) is not null)
            {
                if (line.StartsWith("MSG ", StringComparison.Ordinal))
                {
                    // Due formati: "MSG <seq> <topic> <data>" (broker con conferma di consegna)
                    // e il legacy "MSG <topic> <data>". L'id è il primo token se è numerico.
                    var rest = line.Substring(4);
                    int sp = rest.IndexOf(' ');
                    var first = sp >= 0 ? rest.Substring(0, sp) : rest;
                    var tail = sp >= 0 ? rest.Substring(sp + 1) : "";
                    string topic, data;
                    long seq = 0;
                    var hasSeq = long.TryParse(first, NumberStyles.None, CultureInfo.InvariantCulture, out seq);
                    if (hasSeq)
                    {
                        int sp2 = tail.IndexOf(' ');
                        topic = sp2 >= 0 ? tail.Substring(0, sp2) : tail;
                        data = sp2 >= 0 ? tail.Substring(sp2 + 1) : "";
                    }
                    else { topic = first; data = tail; }
                    var hits = DispatchMessage(bus, topic, data);
                    // Conferma: ACK se un handler ha preso in carico il messaggio, NACK se è
                    // rimasto orfano (il broker lo ri-consegna: semantica competing-consumer).
                    // L'ACK conferma la CONSEGNA al dispatcher (task GPM accodato), non la fine
                    // dell'esecuzione dell'handler: chi ha bisogno dell'effetto deve attenderlo
                    // su una condizione propria, non sull'ordine di stampa.
                    if (_mbus.TryGetValue(bus, out var cb))
                    {
                        cb.Received++;
                        // Ripetizione ⇔ l'id non supera il massimo già visto (il broker numera
                        // in modo monotono): è la ri-consegna che il client ha osservato.
                        if (hasSeq)
                        {
                            if (seq <= cb.MaxSeq) cb.Dup++; else cb.MaxSeq = seq;
                            if (hits > 0) cb.Acked++; else cb.Nacked++;
                        }
                    }
                    if (hasSeq)
                        SendTcp(bus, (hits > 0 ? "ACK " : "NACK ") + seq.ToString(CultureInfo.InvariantCulture));
                }
                else if (line.StartsWith("ACK ", StringComparison.Ordinal))
                {
                    // "ACK <seq>" = conferma del broker alla nostra PUB: il broker l'ha già scritta
                    // PRIMA delle MSG, quindi non richiede stato qui (la conferma di consegna che
                    // conta è quella del consumatore, sopra).
                }
            }
        }
        catch { /* chiuso */ }
    }

    /// <summary>Dispatcha ai sottoscrittori e ritorna quanti handler sono stati attivati:
    /// zero significa messaggio orfano → il client risponde NACK al broker.</summary>
    private static int DispatchMessage(int bus, string topic, string data)
    {
        var hits = 0;
        foreach (var kv in new List<KeyValuePair<string, MSub>>(_msub))
        {
            var s = kv.Value;
            if (s.Bus != bus || s.HandlerSymbolId < 0 || s.Handler is null) continue;
            if (!string.IsNullOrEmpty(s.Topic) && !string.Equals(s.Topic, topic, StringComparison.OrdinalIgnoreCase)) continue;
            DispatchHandlerAsGpmOrDirect(s.HandlerSymbolId, s.Handler, new object?[] { s.Id, topic, data });
            hits++;
        }
        return hits;
    }

    private static bool SendTcp(int bus, string line)
    {
        if (!_mbus.TryGetValue(bus, out var b) || b.Tcp is null) return false;
        try
        {
            var bytes = System.Text.Encoding.UTF8.GetBytes(line + "\n");
            lock (b.WriteLock) b.Tcp.GetStream().Write(bytes, 0, bytes.Length);
            return true;
        }
        catch { return false; }
    }

    public static string MessageSubscribe(long bus, string topic, string queue, string alias)
    {
        var id = string.IsNullOrEmpty(alias) ? ("s" + _msubSeq++) : alias;
        var s = new MSub { Id = id, Topic = string.IsNullOrEmpty(topic) ? (queue ?? "") : topic, Bus = (int)bus };
        _msub[id] = s;
        if (_mbus.TryGetValue((int)bus, out var b)) { b.Subs.Add(id); if (b.Tcp is not null) SendTcp((int)bus, "SUB " + s.Topic); }
        return id;
    }

    public static void MessageUnsubscribe(string sub)
    {
        if (string.IsNullOrEmpty(sub)) { _msub.Clear(); foreach (var b in _mbus.Values) b.Subs.Clear(); return; }
        if (_msub.TryGetValue(sub, out var s)) { if (_mbus.TryGetValue(s.Bus, out var b)) { b.Subs.Remove(sub); if (b.Tcp is not null) SendTcp(s.Bus, "UNSUB " + s.Topic); } _msub.Remove(sub); }
    }

    public static void MessagePublish(long bus, string topic, string data)
    {
        if (_mbus.TryGetValue((int)bus, out var b) && b.Tcp is not null)
        {
            if (SendTcp((int)bus, "PUB " + (topic ?? "") + " " + (data ?? ""))) b.Sent++;
            return;
        }
        foreach (var kv in new List<KeyValuePair<string, MSub>>(_msub))
        {
            var s = kv.Value;
            if (s.Bus != (int)bus || s.HandlerSymbolId < 0 || s.Handler is null) continue;
            if (!string.IsNullOrEmpty(s.Topic) && !string.Equals(s.Topic, topic, StringComparison.OrdinalIgnoreCase)) continue;
            DispatchHandlerAsGpmOrDirect(s.HandlerSymbolId, s.Handler, new object?[] { s.Id, topic, data });
        }
    }

    public static void MessagePing(long bus)
    {
        if (_mbus.TryGetValue((int)bus, out var b) && b.Tcp is not null) SendTcp((int)bus, "PING");
    }

    public static void MessageClose(long bus)
    {
        if (_mbus.TryGetValue((int)bus, out var b))
        {
            foreach (var sid in b.Subs) _msub.Remove(sid);
            try { b.Stop = true; b.Tcp?.Close(); } catch { }
            _mbus.Remove((int)bus);
        }
    }

    /// <summary>MESSAGE.DELIVERY(bus) → "sent=S recv=R ack=A nack=N dup=D last=L".
    /// Contatori di consegna del bus (vedi MBus): sent = PUB scritte al broker, recv = MSG
    /// ricevute, ack/nack = conferme inviate (nack = messaggio orfano → il broker ri-consegna),
    /// dup = MSG con id già visto (ri-consegne osservate), last = massimo id visto. Il bus
    /// locale risponde con sent/ack/nack/dup = 0 (consegna sincrona, nessuna conferma).</summary>
    public static string MessageDelivery(long bus)
    {
        if (!_mbus.TryGetValue((int)bus, out var b))
            return "sent=0 recv=0 ack=0 nack=0 dup=0 last=0";
        return string.Format(CultureInfo.InvariantCulture,
            "sent={0} recv={1} ack={2} nack={3} dup={4} last={5}", b.Sent, b.Received, b.Acked, b.Nacked, b.Dup, b.MaxSeq);
    }

    /// <summary>ON MESSAGE &lt;sub&gt; CALL/GOSUB &lt;handler&gt;: risolve e registra il symbolId/metodo.</summary>
    public static void RegisterMessageHandlerBySymbol(string sub, int symbolId)
    {
        if (!string.IsNullOrEmpty(sub) && _msub.TryGetValue(sub, out var s))
        {
            s.HandlerSymbolId = symbolId;
            s.Handler = symbolId >= 0 ? FindProcedureMethodIn(Assembly.GetCallingAssembly(), symbolId, paramTypes: null) : null;
        }
    }
}
