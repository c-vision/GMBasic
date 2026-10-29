# build-ffi.ps1 — compila le nostre librerie FFI per Windows (ARM64) e le lascia in `out\`.
#
# PERCHÉ ESISTE. Il porting Windows si verifica su una macchina Windows vera (VM), non sul Mac:
# qui si usa **Zig** (`zig cc -target aarch64-windows-gnu`), un compilatore C autoportante che non
# richiede MSVC né permessi di amministratore — sulla VM `vmfc` è l'unica via.
#
# USO (dentro Windows, con questo file e lo zip dei sorgenti accanto):
#   powershell -ExecutionPolicy Bypass -File build-ffi.ps1 -Zip <sorgenti.tar.gz> -Out <dir>
#
# Lo zip contiene: `<lib>/src`, `<lib>/include`, `include/win32/` (strato POSIX→Win32) e
# `include/*.inc`. Le DLL prodotte esportano i simboli `gmb<lib>_*` (verificato con
# IlCaricatore di prova: LoadLibrary + GetProcAddress → `available=1`).
#
# Le dipendenze a runtime sono SOLO di sistema (`WS2_32.dll`, `KERNEL32.dll`, `api-ms-win-crt-*`):
# nessuna DLL di runtime mingw da distribuire.
param(
    [Parameter(Mandatory = $true)][string]$Zip,
    [string]$Out = "out",
    [string]$Zig = ""
)
$ErrorActionPreference = "Continue"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
if (-not $Zig) {
    $Zig = (Get-Command zig.exe -ErrorAction SilentlyContinue).Source
    if (-not $Zig) { $Zig = (Get-ChildItem -Path "$env:USERPROFILE\zig" -Recurse -Filter zig.exe -ErrorAction SilentlyContinue | Select-Object -First 1).FullName }
}
if (-not $Zig) { Write-Error "zig.exe non trovato: passa -Zig <path>"; exit 2 }

$root = Join-Path $env:TEMP ("gmbffi_" + [guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Path $root | Out-Null
New-Item -ItemType Directory -Path (Join-Path $root $Out) -Force | Out-Null
tar -xzf $Zip -C $root
$outDir = Join-Path $root $Out

# Le lib di rete; `media` richiede ancora il port di dlfcn/dirent/pwd (esclusa: non compila).
$libs = @("protobuf", "amqp", "grpc", "kafka", "mqtt", "nats")
$ok = 0
foreach ($lib in $libs) {
    $srcs = Get-ChildItem (Join-Path $root "$lib\src\*.c") -ErrorAction SilentlyContinue | ForEach-Object { $_.FullName }
    if (-not $srcs) { Write-Host "$lib : sorgenti assenti"; continue }
    $dll = Join-Path $outDir "libgmb$lib.dll"
    & $Zig cc -shared -target aarch64-windows-gnu -O2 `
        -include (Join-Path $root "include\win32\gmb_win32.h") `
        -I (Join-Path $root "include\win32") `
        -I (Join-Path $root "$lib\include") `
        -I (Join-Path $root "include") `
        $srcs -o $dll -lws2_32 *> (Join-Path $outDir "$lib.err")
    if (Test-Path $dll) { Write-Host ("{0,-10} OK {1} byte" -f $lib, (Get-Item $dll).Length); $ok++ }
    else { Write-Host ("{0,-10} FAIL" -f $lib); Get-Content (Join-Path $outDir "$lib.err") | Select-String "error:" | Select-Object -First 3 }
}
Write-Host "compilate: $ok / $($libs.Count) -> $outDir"
