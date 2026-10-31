# GMBasicSharp (C#)

Progetto **solo C#** del monorepo: frontend (Lexer/Parser/Linker/Compiler), backend **IL** e
**PCodeVM**, Executor/REPL, BRT/CommonLib/Graphics. Nessun sorgente C nel progetto.

## Build

```
dotnet build GMBasicSharp.slnx
dotnet build REPL/REPL.csproj
```

Il build non richiede il compilatore C. A runtime il lato C è usato via **P/Invoke** alle dylib
(`gmb_db`, `gmb_gfx`, `gmb_ai_llm`, `gmb_ai_modelio`), la cui ABI è verificata in
`runtime-abi/` (workspace). Il corpus `.bas` è in `shared/`.

## Test

```
dotnet test tests/GMBASIC.IlBackend.Tests/GMBASIC.IlBackend.Tests.csproj
dotnet test tests/AI.Tests/AI.Tests.csproj
```

## Licenza

Apache-2.0 (proposta, vedi piano di split). La reimplementazione C# dei P/Invoke (F5) usa
`GMBasic-Deprecated` come fonte e sarà decisa modulo per modulo.
