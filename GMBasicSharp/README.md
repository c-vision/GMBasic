# GMBasicSharp (C#)

The **C#-only** project of the monorepo: frontend (Lexer/Parser/Linker/Compiler) plus the **IL** and
**PCodeVM** backends, Executor/REPL, BRT/CommonLib/Graphics. No C source in this project.

## Build

```
dotnet build GMBasicSharp.slnx
dotnet build REPL/REPL.csproj
```

The build does not need the C compiler. At runtime the C side is used through **P/Invoke** to the
dylibs (`gmb_db`, `gmb_gfx`, `gmb_ai_llm`, `gmb_ai_modelio`), whose ABI is verified in
`runtime-abi/` (workspace). The `.bas` corpus is in `shared/`.

## Test

```
dotnet test tests/GMBASIC.IlBackend.Tests/GMBASIC.IlBackend.Tests.csproj
dotnet test tests/AI.Tests/AI.Tests.csproj
```

## License

Apache-2.0 (proposal, see the split plan). The F5 C# reimplementation of the P/Invokes uses
`GMBasic-Deprecated` as the source and will be decided module by module.
