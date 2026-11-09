# FINANCE — implementazione C# archiviata

Il namespace `FINANCE` è migrato a **shared/libs/finance** (lib `libgmbfinance`,
C puro). La superficie BASIC non è più builtin: IL/PCode/VM-C/C99Native chiamano la
libreria via FFI (P/Invoke lato C#).

Qui è conservata l'implementazione C# storica, NON più compilata:
- `CommonLib-Numerics-Finance.cs` — `CommonLib.Numerics.Finance.cs` (formule, era il core managed)
- `StdLib.Runtime.Finance.cs` — forwarder `Basic.StdLibRuntime.FinanceFunctions`
- `StdLib.Finance.cs` — registrazione del namespace builtin (rimossa, insieme alle 23 entry
  `FINANCE.*` nei quattro `BackendSupport/Registry/{IL,PCodeVM,VMC,C99Native}.cs`;
  `tools/sh/check_ffi_surface.sh finance` è verde)

Anche i forwarder `IlRuntime.Pmt/Fv/Pv/...` sono stati rimossi dal runtime C#: la
logica vive solo in C.
