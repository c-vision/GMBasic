// FILE: Basic.Runtime.IRuntimeHost.cs

namespace Basic.Runtime;

/// <summary>
/// Host del BASIC Runtime. Contiene tutti i servizi che un programma BASIC
/// compilato può utilizzare. Backend-agnostic (AST, P-CODE, IL, LLVM, Wasm).
/// </summary>
public interface IRuntimeHost
{
    // === Core Services ===
    IConsoleService Console { get; }
    IKeyboardService Keyboard { get; }
    IClipboardService Clipboard { get; }
    IFileService File { get; }
    ITimeService Time { get; }

    // === Network & HTTP ===
    INetworkService Network { get; }
    IHttpService Http { get; }

    /// <summary>Client HTTP ricco condiviso (CommonLib.Http), usato da FETCH nella P-CODE VM —
    /// stesso client/opzioni/cookie-jar già usato dall'AST executor (vedi BasicRuntime.HttpClientHelper),
    /// invece del minimale <see cref="IHttpService"/> (solo GET testuale, bloccante).</summary>
    CommonLib.Http.HttpClientHelper HttpClientHelper { get; }

    // === Database ===
    IDatabaseService Database { get; }
    /// <summary>RDD DataSession condiviso tra GPM e P-Code VM.</summary>
    Basic.Databases.Rdd.DataSession SqlDataSession { get; }

    // === Crypto & Memory ===
    ICryptoService Crypto { get; }
    IMemoryService Memory { get; }
    IFfiService Ffi { get; }
    System.Random Rng { get; }

    // === Sandbox (futuro) ===
    ISandboxService? Sandbox { get; }

    // === Graphics & Multimedia ===
    IGraphicsService Graphics { get; }
    IAudioService Audio { get; }

    // === AI Agent & Orchestration ===
    IAgentService Agent { get; }
    ITeamService Team { get; }
    IWorkflowService Workflow { get; }

    // === Type System ===
    object CoerceValue(object? value, DataType targetType);
    DataType DeduceTypeFromSuffix(string name);
    object GetDefaultValue(DataType type);

    // === Error Handling ===
    BasicException CreateError(string message, ErrorCode code);

    // === Config ===
    RuntimeConfig Config { get; }
}
