# Librerie Windows (win-arm64) — come si producono

Le nostre librerie FFI (`protobuf`, `amqp`, `grpc`, `kafka`, `mqtt`, `nats`) compilano per
**Windows ARM64** con **Zig** dentro una VM Windows (`vmfc` su VMware Fusion), senza MSVC e senza
permessi di amministratore.

## Perché Zig

Sulla VM Windows 11 ARM non ci sono compilatori (`cl`, `cmake`, `clang`: assenti) e l'utente non è
amministratore. Zig è un compilatore C autoportante (un solo `zig.exe`): con
`zig cc -target aarch64-windows-gnu` compila C con headers mingw e linka DLL che dipendono **solo**
da `WS2_32.dll`, `KERNEL32.dll` e dalle `api-ms-win-crt-*` di sistema — nessuna DLL di runtime da
distribuire.

## Strato POSIX→Win32

I sorgenti sono gli stessi di macOS/Linux (nessuna `#if _WIN32`): su Windows si compila con
`-include include/win32/gmb_win32.h` e `-I include/win32`, dove stanno

- `gmb_win32.h`: include `winsock2`/`ws2tcpip`/`windows`, mappa `close`→`closesocket`,
  `read`/`write`→`recv`/`send`, `usleep`/`sleep`→`Sleep`, e offre `gmb_win_net_init()` (`WSAStartup`);
- header-stub (`netdb.h`, `sys/socket.h`, `sys/select.h`, `netinet/in.h`, `arpa/inet.h`,
  `unistd.h`, …) che risolvono le `#include <…>` POSIX verso lo strato sopra.

Su macOS/Linux il file non viene usato: la compilazione resta identica a prima.

## Procedura

```powershell
# dentro Windows, con lo zip dei sorgenti accanto
powershell -ExecutionPolicy Bypass -File build-ffi.ps1 -Zip gmb-ffi-src.tar.gz -Out out
```

Lo zip si crea sul Mac (attenzione: `--no-mac-metadata`, altrimenti macOS infila file `._*` che
rompono la compilazione):

```bash
COPYFILE_DISABLE=1 tar --no-mac-metadata -czf gmb-ffi-src.tar.gz -C staging .
```

## Verifica e pubblicazione

1. **Caricabilità** (dentro Windows): `LoadLibrary` + `GetProcAddress("gmb<lib>_available")` → 1.
2. **Pubblicazione** (dal monorepo, asset per-file):

```bash
tools/sh/libs_publish_files_gh.sh ffi win-arm64 --source <dir-con-le-dll> --tag ffi-v1 --yes
tools/sh/libs_manifest_gh.sh build --rid win-arm64 --groups ffi
tools/sh/libs_index_publish.sh --publish
```

## Stato

| libreria | win-arm64 | nota |
|---|---|---|
| protobuf, amqp, grpc, kafka, mqtt, nats | ✅ compilano, caricano, `available=1` | pubblicate su `ffi-v1` |
| media | ❌ | richiede `dlfcn`/`dirent`/`pwd`/`arm_neon` (port a sé) |
| `support` (SDL/OpenSSL/curl/…) | ❌ | terze parti: servono i loro sorgenti compilati per arm64-windows (vcpkg/MSVC o zig con i rispettivi build) |
| `runtime`/`core` | ❌ | il runtime C usa `dlopen`/`fork`/`pthread`: port POSIX→Win32 del runtime |
