// PCodeProcessor.Message.cs — MESSAGE (namespace MESSAGE) lato PCodeVM.
// Trasporti: "local" (bus in-process) e "tcp://host:port" (client verso un broker).
// L'handle del bus è il NOME (stringa, come nel Compiler). Ricezione su THREAD dedicato →
// task GPM (CreateTask+Schedule); mai sul thread esterno (pattern IO watch).
using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Net.Sockets;
using System.Text;
using System.Threading;
using Basic.Runtime.Gpm;

namespace Basic.IlBackend.PCode;

internal sealed partial class PCodeProcessor
{
    private sealed class PBus
    {
        public string Uri = ""; public TcpClient? Tcp; public volatile bool Stop;
        /// <summary>Scritture dal thread BASIC e dal thread di ricezione (ACK/NACK): lock.</summary>
        public readonly object WriteLock = new();
        /// <summary>CONTATORI DI CONSEGNA (MESSAGE.DELIVERY): sent dal thread BASIC, gli altri dal
        /// thread di ricezione. Vedi IlRuntime.Message.cs, stessa semantica.</summary>
        public long Sent, Recv, Ack, Nack, Dup, MaxSeq;
    }
    private sealed class PSub { public string Id = ""; public string Topic = ""; public string Handle = ""; public string Handler = ""; }

    private readonly Dictionary<string, PBus> _msgBus = new(StringComparer.OrdinalIgnoreCase);
    private readonly Dictionary<string, PSub> _msgSub = new(StringComparer.OrdinalIgnoreCase);
    private int _msgSubSeq = 1;

    private void HandleMessageOp(PCodeTask task)
    {
        var subop = PopInt(task);
        var a = PopString(task) ?? "";
        var b = PopString(task) ?? "";
        var c = PopString(task) ?? "";
        switch (subop)
        {
            case 0: /* CONNECT handle, uri */
                {
                    var bus = new PBus { Uri = b };
                    if (b.StartsWith("tcp://", StringComparison.OrdinalIgnoreCase))
                    {
                        var hp = b.Substring(6); var colon = hp.LastIndexOf(':');
                        var host = colon > 0 ? hp.Substring(0, colon) : hp;
                        var port = colon > 0 && int.TryParse(hp.Substring(colon + 1), out var p) ? p : 0;
                        try { bus.Tcp = new TcpClient(); bus.Tcp.Connect(host, port); var st = bus.Tcp.GetStream(); new Thread(() => MsgRecvLoop(bus, st)) { IsBackground = true }.Start(); }
                        catch
                        {
                            /* Connect fallita: NON lasciare il TcpClient non-connesso, altrimenti
                             * PUBLISH prende il ramo tcp e il messaggio non viene consegnato
                             * (parità con VM/C e C99Native, che in questo caso consegnano
                             * in-process). Vedi IlRuntime.Message.cs, stesso fix. */
                            try { bus.Tcp?.Close(); } catch { }
                            bus.Tcp = null;
                        }
                    }
                    _msgBus[a] = bus;
                }
                break;
            case 1: /* SUBSCRIBE handle, topic, alias */
                {
                    var id = string.IsNullOrEmpty(c) ? ("s" + _msgSubSeq++) : c;
                    _msgSub[id] = new PSub { Id = id, Topic = b, Handle = a, Handler = _msgSub.TryGetValue(id, out var e0) ? e0.Handler : "" };
                    if (_msgBus.TryGetValue(a, out var bb) && bb.Tcp is not null) MsgSend(bb, "SUB " + b);
                }
                break;
            case 2: /* UNSUBSCRIBE alias */
                _msgSub.Remove(a);
                break;
            case 3: /* PUBLISH handle, topic, data */
                if (_msgBus.TryGetValue(a, out var b3) && b3.Tcp is not null)
                {
                    /* NESSUNA attesa della conferma del broker: il contratto è lo stesso di IL,
                     * VM/C e C99Native (gmb_msg_publish scrive e ritorna). Attendere qui era una
                     * DIVERGENZA fra backend — con un broker che non conferma la PUBLISH restava
                     * bloccata fino a 2 s solo su PCode. La conferma che conta è quella del
                     * consumatore (ACK/NACK qui sotto); chi deve osservare l'effetto attende una
                     * condizione propria. */
                    if (MsgSend(b3, "PUB " + b + " " + c)) b3.Sent++;
                    break;
                }
                foreach (var kv in new List<KeyValuePair<string, PSub>>(_msgSub))
                {
                    var s = kv.Value;
                    if (!string.Equals(s.Handle, a, StringComparison.OrdinalIgnoreCase) || s.Handler.Length == 0) continue;
                    if (s.Topic.Length > 0 && !string.Equals(s.Topic, b, StringComparison.OrdinalIgnoreCase)) continue;
                    DispatchEventHandler(task, s.Handler, new object?[] { kv.Key, b, c });
                }
                break;
            case 4: break;                 /* PING */
            case 5:                        /* CLOSE handle */
                if (_msgBus.TryGetValue(a, out var b5)) { foreach (var kv in new List<KeyValuePair<string, PSub>>(_msgSub)) if (string.Equals(kv.Value.Handle, a, StringComparison.OrdinalIgnoreCase)) _msgSub.Remove(kv.Key); try { b5.Stop = true; b5.Tcp?.Close(); } catch { } _msgBus.Remove(a); }
                break;
        }
    }

    /// <summary>MESSAGE.DELIVERY(bus) — contatori di consegna del bus, stesso testo della gamba
    /// IL/VM-C/C99Native: "sent=S recv=R ack=A nack=N dup=D last=L".</summary>
    private string MsgDelivery(string handle)
    {
        if (!_msgBus.TryGetValue(handle, out var b))
            return "sent=0 recv=0 ack=0 nack=0 dup=0 last=0";
        return string.Format(CultureInfo.InvariantCulture,
            "sent={0} recv={1} ack={2} nack={3} dup={4} last={5}", b.Sent, b.Recv, b.Ack, b.Nack, b.Dup, b.MaxSeq);
    }

    private void HandleOnMessage(PCodeTask task)
    {
        var sub = PopString(task) ?? "";
        var handler = PopString(task) ?? "";
        if (_msgSub.TryGetValue(sub, out var e)) _msgSub[sub] = new PSub { Id = e.Id, Topic = e.Topic, Handle = e.Handle, Handler = handler };
    }

    private void MsgRecvLoop(PBus b, NetworkStream stream)
    {
        try
        {
            using var reader = new StreamReader(stream);
            string? line;
            while ((line = reader.ReadLine()) is not null)
            {
                if (line.StartsWith("MSG ", StringComparison.Ordinal))
                {
                    /* Due formati: "MSG <seq> <topic> <data>" (broker con conferma) e legacy
                     * "MSG <topic> <data>". L'id è il primo token se è numerico. */
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
                    var hits = MsgDispatch(b, topic, data);
                    /* ACK se un handler ha preso in carico il messaggio, NACK se è rimasto orfano
                     * (il broker lo ri-consegna: semantica competing-consumer). */
                    b.Recv++;
                    if (hasSeq)
                    {
                        if (seq <= b.MaxSeq) b.Dup++; else b.MaxSeq = seq;
                        if (hits > 0) b.Ack++; else b.Nack++;
                    }
                    if (hasSeq)
                        MsgSend(b, (hits > 0 ? "ACK " : "NACK ") + seq.ToString(CultureInfo.InvariantCulture));
                }
                else if (line.StartsWith("ACK ", StringComparison.Ordinal))
                {
                    /* "ACK <seq>" = conferma del broker alla nostra PUB (scritta PRIMA delle MSG).
                     * Non richiede stato: la PUB non la attende (vedi case 3). */
                }
            }
        }
        catch { }
    }

    /// <summary>Dispatcha e ritorna quanti handler sono stati attivati: zero = messaggio orfano
    /// → il client risponde NACK al broker.</summary>
    private int MsgDispatch(PBus b, string topic, string data)
    {
        if (_scheduler is null) return 0;
        var hits = 0;
        foreach (var kv in new List<KeyValuePair<string, PSub>>(_msgSub))
        {
            var s = kv.Value;
            if (s.Handler.Length == 0) continue;
            if (!_msgBus.TryGetValue(s.Handle, out var bb) || !ReferenceEquals(bb, b)) continue;
            if (s.Topic.Length > 0 && !string.Equals(s.Topic, topic, StringComparison.OrdinalIgnoreCase)) continue;
            if (!_procedureAddresses.TryGetValue(s.Handler, out var addr)) continue;
            var cbTask = _scheduler.CreateTask(addr);
            cbTask.Scheduler = _scheduler;
            cbTask.Globals = _vm?.MainTask.Globals;
            object?[] args = { s.Id, topic, data };
            for (int i = 0; i < args.Length; i++) cbTask.PushRef(args[i]);
            // NIENTE frame sintetico qui: il task è NUOVO e CreateTask(addr) ha già posto
            // Ip = addr. Un frame con ReturnIp = cbTask.Ip riporterebbe il RETURN su addr
            // (l'ingresso dell'handler), che verrebbe rieseguito con lo stack già consumato —
            // la seconda "RX" con argomenti vuoti di messaging/message_tcp_parity.bas. Senza
            // frame, OP_RETURN chiude il task (ramo Frame == null) e il prologo scritto dal
            // compilatore lega i parametri in task.GetLocals(): stesso pattern di
            // SpawnHttpHandlerTask (handler HTTP/WebSocket), che non installa frame.
            _scheduler.Schedule(cbTask);
            hits++;
        }
        return hits;
    }

    private static bool MsgSend(PBus b, string line)
    {
        if (b.Tcp is null) return false;
        try
        {
            var bytes = Encoding.UTF8.GetBytes(line + "\n");
            lock (b.WriteLock) b.Tcp.GetStream().Write(bytes, 0, bytes.Length);
            return true;
        }
        catch { return false; }
    }
}
