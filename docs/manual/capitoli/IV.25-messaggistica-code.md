# IV.25 — Messaggistica e code

## Cosa ottieni da questo capitolo

Un canale fra parti del sistema che non si conoscono: pubblicare un evento su un topic e riceverlo
in un handler. Il codice è corto; le decisioni che contano sono due — il topic e l'endpoint.

## 1. Il ciclo completo

```basic
IMPORT MESSAGE

SUB OnOrders(id AS STRING, topic AS STRING, payload AS STRING)
  PRINT "[orders:"; id; "] "; payload
END SUB

SUB Main()
  DIM bus AS LONG
  MESSAGE CONNECT "local" AS bus

  MESSAGE SUBSCRIBE bus TOPIC("orders") AS "o"
  MESSAGE SUBSCRIBE bus TOPIC("trades") AS "t"
  ON MESSAGE "o" CALL OnOrders

  MESSAGE PUBLISH bus TOPIC("orders") DATA("buy 100 AAPL")
  SLEEP 120

  MESSAGE UNSUBSCRIBE "t"
  MESSAGE PUBLISH bus TOPIC("trades") DATA("ignored")
  SLEEP 120

  MESSAGE CLOSE bus
  PRINT "done"
END SUB
```

| Comando | Effetto |
|---|---|
| `MESSAGE CONNECT <endpoint> AS <handle>` | apre il canale (`"local"` = bus in-process) |
| `MESSAGE SUBSCRIBE <handle> TOPIC("x") AS "id"` | iscrive il topic con un identificatore |
| `ON MESSAGE "id" CALL <SUB>` | collega l'handler all'identificatore |
| `MESSAGE PUBLISH <handle> TOPIC("x") DATA("...")` | pubblica |
| `MESSAGE UNSUBSCRIBE "id"` | smette di consegnare |
| `MESSAGE PING <handle>` | verifica che il canale sia vivo |
| `MESSAGE CLOSE <handle>` | chiude |

L'handler riceve **tre** argomenti: identificatore, topic, payload.

## 2. Eseguito

(`shared/samples/Messaging/PubSubDemo.bas`, backend C99Native)

```
[orders:o] buy 100 AAPL
[trades:t] fill 100 AAPL @ 200
[orders:o] sell 50 AAPL
done
```

La riga `ignored` **non** appare: dopo `MESSAGE UNSUBSCRIBE "t"` il topic `trades` non consegna
più. È l'unica asserzione del sample, ed è quella che conta — un bus che continua a consegnare
dopo il distacco è un bug di memoria, non una gentilezza.

L'ordine invece non è garantito dal bus in-process: i due `SLEEP 120` danno il tempo agli handler
di girare, ma il contratto è "consegna", non "consegna in ordine". Se l'ordine conta, mettilo nel
payload o usa un topic per sequenza.

## 3. L'endpoint decide il mondo

| Endpoint | Uso | Quando |
|---|---|---|
| `"local"` | bus **in-process** | test, eventi interni all'applicazione |
| `tcp://…` | broker TCP | più processi sulla stessa rete |
| `amqp://…` | RabbitMQ e compatibili | code di lavoro, routing |
| `mqtt://…` | broker MQTT | dispositivi, telemetria |
| `nats://…` | NATS | messaggistica leggera ad alta frequenza |
| `kafka://…` | Kafka | flussi di eventi con storico |
| `redis://…` | Redis pub/sub e stream | cache e notifiche |

Il codice BASIC **non cambia** fra `local` e Kafka: cambia la stringa di connessione. Le guide
`AMQP-Guida-Utente`, `Kafka-Guida-Utente`, `MQTT-Guida-Utente`, `NATS-Guida-Utente`,
`Redis-Guida-Utente` descrivono le configurazioni specifiche.

## 4. Handler e GPM

L'handler gira come **task GPM**: non è il thread del socket a eseguire il tuo codice, e questo è
ciò che permette di avere `SLEEP`, chiamate al database o lavoro lungo dentro l'handler senza
fermare il canale. Vale la regola di III.13: mai chiamare BASIC da un thread esterno.

Conseguenza: l'handler è concorrente rispetto al `Main`. Se scrive su una variabile condivisa,
usa le primitive atomiche (`ATOMIC`/`LOCK`), non un `LET`.

## 5. gRPC e Protobuf

Quando il canale deve avere uno schema e non solo un payload di stringhe, si usa Protobuf per la
definizione dei messaggi e gRPC per le chiamate (`Protobuf-Guida-Utente`, `gRPC-Guida-Utente`).
La differenza pratica rispetto al pub/sub: qui c'è un contratto tipizzato, e un campo aggiunto in
modo compatibile non rompe i client vecchi.

## 6. Il mock è quasi sempre la scelta giusta

Per un test, `"local"` basta: nessun broker da avviare, nessuna rete, output deterministico —
infatti `PubSubDemo.bas` è un MiniTest e non un sample interattivo. Passa a un broker reale solo
quando devi verificare il broker (persistenza, riconnessione, partizioni).

## 7. Errori frequenti

| Sintomo | Causa | Correzione |
|---|---|---|
| l'handler non viene chiamato | manca `ON MESSAGE` per quell'id | collega l'handler |
| non arriva nulla dopo la riconnessione | l'iscrizione non è stata rifatta | iscriviti dopo `CONNECT` |
| messaggi persi in produzione | broker senza persistenza | configura la durevolezza |
| l'ordine non è quello atteso | bus concorrente | sequenza nel payload o partizione |
| memoria che cresce | `MESSAGE CLOSE` mancante | chiudi i canali a fine programma |

## 8. Dove andare adesso

- **IV.24** — HTTP, quando serve una risposta immediata.
- **III.13** — il GPM e le primitive atomiche.
- **IV.26** — autenticare un canale.














<!-- BEGIN SURFACE (generato da tools/sh/manual_surface.sh — non modificare a mano) -->

## Riferimento completo: comandi, funzioni, parametri

> Sezione generata dalle guide del progetto per questo capitolo: contiene **tutte**
> le superfici documentate, non una selezione. La prosa dei capitoli precedenti le
> spiega; qui stanno i dettagli — parametri, varianti, esempi d'uso.

### Messaging-Guida-Utente

*Fonte: `Messaging-Guida-Utente`*


## Endpoint

## Comandi

### `MESSAGE CONNECT "<uri>" AS handle`

### `MESSAGE SUBSCRIBE handle TOPIC("<topic>") AS "<id>"`

### `ON MESSAGE "<id>" CALL <proc>(id, topic, payload)`

### `MESSAGE PUBLISH handle TOPIC("<topic>") DATA(<payload>)`

### `MESSAGE UNSUBSCRIBE "<id>"`

### `MESSAGE PING handle`

### `MESSAGE CLOSE handle`

## Esempio minimo

IMPORT MESSAGE
SUB OnMsg(id AS STRING, topic AS STRING, payload AS STRING)
  PRINT id, topic, payload
END SUB
SUB Main()
  DIM bus AS LONG
  MESSAGE CONNECT "local" AS bus
  MESSAGE SUBSCRIBE bus TOPIC("t1") AS "s1"
  ON MESSAGE "s1" CALL OnMsg
  MESSAGE PUBLISH bus TOPIC("t1") DATA("hello")
  SLEEP 200
END SUB


## Parità e test

## Note / limiti attuali

### AMQP-Guida-Utente

*Fonte: `AMQP-Guida-Utente`*


## 1. Attivazione

IMPORT AMQP WITH ALIAS        ' .inc auto-inclusa da shared/libs/amqp/; dylib risolta dal loader FFI
PRINT AMQP.AVAILABLE()        ' 1 = libreria disponibile


## 2. Convenzioni FFI

## 3. Membri del namespace AMQP
| Membro | Semantica |
|---|---|
| `AVAILABLE()` | 1 = disponibile |
| `CONNECT(host, port, user, pass)` | handle (>0) o -1 |
| `DECLAREQUEUE(h, queue)` | 0 ok |
| `PUBLISH(h, exchange, rkey, body)` | 0 ok |
| `GET(h, queue, timeout_ms)` | 1 msg, 0 vuota, -1 err |
| `LASTBODY(h)` / `LASTROUTINGKEY(h)` / `LASTSIZE(h)` | ultimo messaggio |
| `DISCONNECT(h)` | 0 ok |

## 4. Esempio

## 5. Test

## 6. Note e limiti

## 7. Riferimenti

### Kafka-Guida-Utente

*Fonte: `Kafka-Guida-Utente`*


## 1. Attivazione

IMPORT KAFKA WITH ALIAS        ' .inc auto-inclusa da shared/libs/kafka/; dylib risolta dal loader FFI
PRINT KAFKA.AVAILABLE()        ' 1 = libreria disponibile


## 2. Convenzioni FFI

## 3. Membri del namespace KAFKA
| Membro | Semantica |
|---|---|
| `AVAILABLE()` | 1 = disponibile |
| `CONNECT(host, port, cid)` | handle (>0) o -1 |
| `PRODUCE(h, topic, partition, value)` | error_code (0 ok) |
| `CONSUME(h, topic, partition, offset, timeout_ms)` | 1 record, 0 nessuno, -1 err |
| `LASTVALUE(h)` / `NEXTOFFSET(h)` | ultimo valore / prossimo offset |
| `DISCONNECT(h)` | 0 ok |

## 4. Esempio

## 5. Test

## 6. Note e limiti

## 7. Riferimenti

### MQTT-Guida-Utente

*Fonte: `MQTT-Guida-Utente`*


## 1. Attivazione

IMPORT MQTT WITH ALIAS        ' .inc auto-inclusa da shared/libs/mqtt/; dylib risolta dal loader FFI
PRINT MQTT.AVAILABLE()        ' 1 = libreria disponibile


## 2. Convenzioni FFI

## 3. Membri del namespace MQTT
| Membro | Semantica |
|---|---|
| `AVAILABLE()` | 1 = disponibile |
| `CONNECT(host, port, cid)` | handle (>0) o -1 |
| `SUBSCRIBE(h, topic)` | 0 ok |
| `PUBLISH(h, topic, payload)` | 0 ok |
| `RECEIVE(h, timeout_ms)` | 1 msg, 0 timeout, -1 err |
| `LASTTOPIC(h)` / `LASTPAYLOAD(h)` | ultimo messaggio |
| `DISCONNECT(h)` | 0 ok |

## 4. Esempio

## 5. Test

## 6. Note e limiti

## 7. Riferimenti

### NATS-Guida-Utente

*Fonte: `NATS-Guida-Utente`*


## 1. Attivazione

IMPORT NATS WITH ALIAS        ' .inc auto-inclusa da shared/libs/nats/; dylib risolta dal loader FFI
PRINT NATS.AVAILABLE()        ' 1 = libreria disponibile


## 2. Convenzioni FFI

## 3. Membri del namespace NATS
| Membro | Semantica |
|---|---|
| `AVAILABLE()` | 1 = disponibile |
| `CONNECT(host, port, name)` | handle (>0) o -1 |
| `SUBSCRIBE(h, subject)` | 0 ok |
| `PUBLISH(h, subject, payload)` | 0 ok |
| `RECEIVE(h, timeout_ms)` | 1 msg, 0 timeout, -1 err |
| `LASTSUBJECT(h)` / `LASTPAYLOAD(h)` | ultimo messaggio |
| `DISCONNECT(h)` | 0 ok |

## 4. Esempio

## 5. Test

## 6. Note e limiti

## 7. Riferimenti

### Protobuf-Guida-Utente

*Fonte: `Protobuf-Guida-Utente`*


## 1. Attivazione

IMPORT PROTOBUF WITH ALIAS        ' .inc auto-inclusa da shared/libs/protobuf/; dylib risolta dal loader FFI
PRINT PROTOBUF.AVAILABLE()        ' 1 = libreria disponibile


## 2. Convenzioni FFI

## 3. Membri del namespace PROTOBUF
| Membro | Semantica |
|---|---|
| `AVAILABLE()` | 1 = disponibile |
| `NEWMSG()` | nuovo builder |
| `ADDINT(h, field, value)` / `ADDSTRING(h, field, s)` | aggiunge campo |
| `SERIALIZE(h)` | byte → **hex** |
| `PARSEHEX(hex)` | hex → reader |
| `HASFIELD(h, field)` / `GETINT(h, field)` / `GETSTRING(h, field)` | leggi campo |
| `FREEMSG(h)` | libera |

## 4. Esempio

## 5. Test

## 6. Note e limiti

## 7. Riferimenti

### Redis-Guida-Utente

*Fonte: `Redis-Guida-Utente`*


## 1. Attivazione

REQUIRE "redis"              ' package manager: risolve/installa e include redis.inc
IMPORT REDIS WITH ALIAS      ' -> chiamate REDIS.<Membro>(...)


## 2. Convenzioni FFI

## 3. Membri del namespace REDIS
| Membro | Parametri | Ritorno | Cosa |
|---|---|---|---|
| `AVAILABLE()` | — | 1/0 | 1 se la lib è presente |
| `CONNECT(host, port)` | `host` = stringa (es. `"127.0.0.1"`), `port` = intero | handle (>0) / 0 | apre la connessione TCP |
| `CLOSE(h)` | — | 0 | chiude la connessione |
| `ALIVE(h)` | — | 1/0 | 1 se la connessione è viva |
| `ERR(h)` | — | stringa | ultimo errore (`""` se nessuno) |
| `AUTH(h, password)` | `password` = stringa | 1 / −1 | autentica |
| `SELECT(h, db)` | `db` = indice intero | 1 / −1 | seleziona il database |
| `PING(h)` | — | 1 / −1 | ping al server |
| `SET(h, key, val)` | `key`, `val` = stringhe | 1 / −1 | scrive una stringa |
| `SETEX(h, key, secs, val)` | + `secs` = TTL in secondi | 1 / −1 | scrive con scadenza |
| `GET(h, key)` | `key` = stringa | stringa | valore (`""` se nil/errore) |
| `DEL(h, key)` | — | n (≥0) / −1 | cancella; n. cancellate |
| `EXISTS(h, key)` | — | 0/1 / −1 | esiste? |
| `INCR(h, key, by)` | `by` = intero | nuovo valore / −1 | incrementa di `by` |
| `EXPIRE(h, key, secs)` | `secs` = secondi | 0/1 / −1 | imposta la scadenza |
| `TTL(h, key)` | — | secondi / −1 | TTL residuo |
| `STRLEN(h, key)` | — | len / −1 | lunghezza della stringa |
| `HSET(h, key, field, val)` | `field`, `val` = stringhe | 1 / −1 | scrive un campo hash |
| `HGET(h, key, field)` | — | stringa | valore del campo (`""` se assente) |
| `HDEL(h, key, field)` | — | n / −1 | cancella il campo |
| `HEXISTS(h, key, field)` | — | 0/1 / −1 | il campo esiste? |
| `HLEN(h, key)` | — | n / −1 | n. campi dell'hash |
| `LPUSH(h, key, val)` | — | n / −1 | inserisce in testa alla lista |
| `LLEN(h, key)` | — | n / −1 | lunghezza lista |
| `LPOP(h, key)` | — | stringa | estrae dalla testa (`""` se vuota) |
| `SADD(h, key, member)` | — | n / −1 | aggiunge al set |
| `SISMEMBER(h, key, member)` | — | 0/1 / −1 | è membro del set? |
| `SCARD(h, key)` | — | n / −1 | cardinalità del set |
| `PUBLISH(h, channel, msg)` | — | n / −1 | pubblica; n. riceventi |
| `DBSIZE(h)` | — | n / −1 | n. chiavi |
| `FLUSHDB(h)` | — | 1 / −1 | svuota il DB corrente |
| `DO(h, cmdline)` | `cmdline` = comando completo, es. `"INCRBY n 5"` | 1 / −1 | comando generico (argv separati da spazio) |
| `REPLYTYPE(h)` | — | 0..6 | tipo ultima risposta (0=none 1=status 2=error 3=int 4=bulk 5=nil 6=array) |
| `REPLYSTR(h)` | — | stringa | ultima risposta come stringa |
| `REPLYINT(h)` | — | n | ultima risposta come intero |
| `REPLYCOUNT(h)` | — | n | n. elementi dell'ultima risposta (array) |

## 4. Esempio

REQUIRE "redis"
IMPORT REDIS WITH ALIAS

SUB Main()
  DIM h AS LONG
  h = REDIS.CONNECT("127.0.0.1", 6379)
  IF h = 0 THEN PRINT "redis non disponibile" : EXIT SUB
  REDIS.SET(h, "name", "hello")
  PRINT "name = "; REDIS.GET(h, "name")          ' hello
  PRINT "incr = "; REDIS.INCR(h, "counter", 1)   ' 1
  REDIS.HSET(h, "user", "city", "Rome")
  PRINT "city = "; REDIS.HGET(h, "user", "city")
  REDIS.CLOSE(h)
END SUB


## 5. Test

## 6. Note e limiti

## 7. Riferimenti

### gRPC-Guida-Utente

*Fonte: `gRPC-Guida-Utente`*


## 1. Attivazione

IMPORT GRPC WITH ALIAS        ' .inc auto-inclusa da shared/libs/grpc/; dylib risolta dal loader FFI
PRINT GRPC.AVAILABLE()        ' 1 = libreria disponibile


## 2. Convenzioni FFI

## 3. Membri del namespace GRPC
| Membro | Semantica |
|---|---|
| `AVAILABLE()` | 1 = disponibile |
| `INVOKE(host, port, path, req_hex)` | grpc-status (0=OK), -1 err trasporto |
| `LASTRESPONSE()` | messaggio di risposta (hex) |
| `LASTSTATUS()` | ultimo grpc-status |

## 4. Esempio

## 5. Test

## 6. Note e limiti

## 7. Riferimenti

<!-- END SURFACE -->


<!-- BEGIN DOMAIN TABLE -->
## Tavola sinottica del dominio

Ogni riga riporta la firma canonica (dal registry) e il primo uso reale trovato
nel corpus di test e sample `.bas`/`.inc`: dove il corpus non usa il simbolo è dichiarato.

- **MESSAGE.DELIVERY(x1)** — `before = MESSAGE.DELIVERY(bus)` (shared/samples/Messaging/MessageDeliveryMonitor.bas:39)
<!-- END DOMAIN TABLE -->
