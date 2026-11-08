// FILE: Basic.IlBackend.PCode.PCodeProcessor.Builtins.cs
// P-CODE VM: ExecuteBuiltIn — refactored from PCodeProcessor.cs (2026-08-03)

using Basic.Runtime;
using Basic.Runtime.Gpm;
using Compiler;
using Microsoft.Data.Sqlite;
using Basic.AI.Rag;
using Basic.Databases.Rdd;
using Basic.IlBackend.PCode.Http;
using CommonLib.Http;

namespace Basic.IlBackend.PCode;

internal sealed partial class PCodeProcessor
{
    /// <summary>PROVIDER: delegazione dominio — case contigui estratti
    /// da PCodeProcessor.Builtins.cs (refactor B, de-mescolamento per dominio).</summary>
    private static readonly HashSet<BuiltInFunction> PROVIDERBuiltInFunctions = new HashSet<BuiltInFunction>
    {
        BuiltInFunction.PROVIDERREGISTER,
        BuiltInFunction.AIREGISTERSOURCE,
        BuiltInFunction.PROVIDERREGISTERLOCAL,
        BuiltInFunction.PROVIDERCONFIGURE,
        BuiltInFunction.PROVIDERSETACTIVE,
        BuiltInFunction.PROVIDERROTATE,
        BuiltInFunction.PROVIDERACTIVE,
        BuiltInFunction.PROVIDERLIST,
        BuiltInFunction.PROVIDERUNREGISTER,
        BuiltInFunction.PROVIDERSAVE,
        BuiltInFunction.PROVIDERLOAD,
        BuiltInFunction.PROVIDERTEST,
        BuiltInFunction.PROVIDERSETPRIORITY,
        BuiltInFunction.PROVIDERPRIORITY,
        BuiltInFunction.PROVIDERSETWEIGHT,
        BuiltInFunction.PROVIDERWEIGHT,
        BuiltInFunction.PROVIDERDISCOVER,
        BuiltInFunction.PROVIDERSETCREDITS,
        BuiltInFunction.PROVIDERCREDITS,
        BuiltInFunction.PROVIDERFAIL,
        BuiltInFunction.PROVIDERRESETFAILED,
        BuiltInFunction.PROVIDERSETSTATUS,
        BuiltInFunction.PROVIDERSTATUS,
        BuiltInFunction.PROVIDERSTARTHEALTHCHECK,
        BuiltInFunction.PROVIDERSTOPHEALTHCHECK,
        BuiltInFunction.PROVIDERHEALTHCHECKINTERVAL,
        BuiltInFunction.PROVIDERRUNHEALTHCHECK,
    };

    /// <summary>Provider: dispatch per BuiltInFunction a TABELLA (Dictionary), non switch (refactor §18).</summary>
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>>? _providerDispatch;
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>> PROVIDERDispatch
        => _providerDispatch ??= BuildProviderDispatch();
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>> BuildProviderDispatch() => new()
    {
        [BuiltInFunction.PROVIDERREGISTER] = (task, args) =>
        {
                {
                    // Runtime comune (IlRuntime): stesso dispatch per firma del backend IL
                    // (role canonico via RegisterFromJson; retro-compat "embedding"/"reranker" via
                    // RegisterSource) — unica implementazione.
                    return Basic.IlBackend.IlRuntime.ProviderRegister(
                        ArgOr(args, 0)?.ToString() ?? "",
                        ArgOr(args, 1)?.ToString() ?? "",
                        ArgOr(args, 2)?.ToString() ?? "",
                        args.Length > 3 ? ArgOr(args, 3)?.ToString() ?? "" : "");
                }
        },
        [BuiltInFunction.AIREGISTERSOURCE] = (task, args) =>
        {
                    // AI REGISTER alias, capability, source [, param] (statement, alias-first —
                    // vedi "Nuova firma" in Parser.Stmt.Ai.cs). NON e' PROVIDERREGISTER
                    // (role-first, chiamata diretta PROVIDER.REGISTER): stesso runtime del
                    // backend IL (EmitAiRegisterCall -> IlRuntime.AiRegisterSource) — bug L17,
                    // prima condivideva PROVIDERREGISTER e l'alias finiva nel parametro role.
                    return Basic.IlBackend.IlRuntime.AiRegisterSource(
                        ArgOr(args, 0)?.ToString() ?? "",
                        ArgOr(args, 1)?.ToString() ?? "",
                        ArgOr(args, 2)?.ToString() ?? "",
                        args.Length > 3 ? ArgOr(args, 3)?.ToString() : null);
        },
        [BuiltInFunction.PROVIDERREGISTERLOCAL] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.ProviderRegisterLocal(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.PROVIDERCONFIGURE] = (task, args) =>
        {
                    // Runtime comune (IlRuntime): stessa logica del backend IL — unica implementazione.
                    return Basic.IlBackend.IlRuntime.ProviderConfigure(
                        ArgOr(args, 0)?.ToString() ?? "",
                        ArgOr(args, 1)?.ToString() ?? "",
                        ArgOr(args, 2)?.ToString() ?? "");
        },
        [BuiltInFunction.PROVIDERSETACTIVE] = (task, args) =>
        {
                    // Runtime comune (IlRuntime): normalizza il ruolo (NormalizeRole) — unica implementazione.
                    return Basic.IlBackend.IlRuntime.ProviderSetActive(
                        ArgOr(args, 0)?.ToString() ?? "",
                        ArgOr(args, 1)?.ToString() ?? "");
        },
        [BuiltInFunction.PROVIDERROTATE] = (task, args) =>
        {
                    // Runtime comune (IlRuntime): normalizza il ruolo — unica implementazione.
                    return Basic.IlBackend.IlRuntime.ProviderRotate(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.PROVIDERACTIVE] = (task, args) =>
        {
                    // Piano unificato: ACTIVE(alias [, capability]) SETTA l'attivo per capability
                    // (dedotta se omessa); ACTIVE(capability) senza alias LEGGE l'attivo di quella
                    // capability. capability: %CHAT/%OCR/%AUDIO.
                    if (args.Length >= 2)
                        return Basic.Providers.ProviderServer.Instance.Activate(ArgOr(args, 0)?.ToString() ?? "",
                            ArgOr(args, 1)?.ToString());
                    // Runtime comune (IlRuntime) per la lettura: normalizza la capability.
                    return Basic.IlBackend.IlRuntime.ProviderActive(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.PROVIDERLIST] = (task, args) =>
        {
                    // Runtime comune (IlRuntime): normalizza il ruolo — unica implementazione.
                    return Basic.IlBackend.IlRuntime.ProviderList(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.PROVIDERUNREGISTER] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.ProviderUnregister(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.PROVIDERSAVE] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.ProviderSave(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.PROVIDERLOAD] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.ProviderLoad(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.PROVIDERTEST] = (task, args) =>
        {
                    // Runtime comune (IlRuntime): normalizza il ruolo — unica implementazione.
                    return Basic.IlBackend.IlRuntime.ProviderTest(
                        ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "");
        },
        [BuiltInFunction.PROVIDERSETPRIORITY] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.ProviderSetPriority(
                        ArgOr(args, 0)?.ToString() ?? "", SafeDouble(ArgOr(args, 1)));
        },
        [BuiltInFunction.PROVIDERPRIORITY] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.ProviderPriority(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.PROVIDERSETWEIGHT] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.ProviderSetWeight(
                        ArgOr(args, 0)?.ToString() ?? "", SafeDouble(ArgOr(args, 1)));
        },
        [BuiltInFunction.PROVIDERWEIGHT] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.ProviderWeight(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.PROVIDERDISCOVER] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.ProviderDiscover(
                        ArgOr(args, 0)?.ToString() ?? "",
                        args.Length > 1 ? (ArgOr(args, 1)?.ToString())! : null,
                        args.Length > 2 ? ArgOr(args, 2)?.ToString() ?? "" : "",
                        args.Length > 3 ? ArgOr(args, 3)?.ToString() ?? "" : "",
                        args.Length > 4 ? SafeDouble(ArgOr(args, 4)) : 0);
        },
        [BuiltInFunction.PROVIDERSETCREDITS] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.ProviderSetCredits(
                        ArgOr(args, 0)?.ToString() ?? "", SafeDouble(ArgOr(args, 1)));
        },
        [BuiltInFunction.PROVIDERCREDITS] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.ProviderCredits(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.PROVIDERFAIL] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.ProviderMarkFailed(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.PROVIDERRESETFAILED] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.ProviderResetFailed(
                        args.Length > 0 ? ArgOr(args, 0)?.ToString() ?? "" : "");
        },
        [BuiltInFunction.PROVIDERSETSTATUS] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.ProviderSetStatus(
                        ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "");
        },
        [BuiltInFunction.PROVIDERSTATUS] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.ProviderStatus(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.PROVIDERSTARTHEALTHCHECK] = (task, args) =>
        {
                    // Runtime comune (IlRuntime): stessa logica — unica implementazione.
                    return Basic.IlBackend.IlRuntime.ProviderStartHealthCheck(
                        SafeDouble(ArgOr(args, 0)));
        },
        [BuiltInFunction.PROVIDERSTOPHEALTHCHECK] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.ProviderStopHealthCheck();
        },
        [BuiltInFunction.PROVIDERHEALTHCHECKINTERVAL] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.ProviderHealthCheckInterval();
        },
        [BuiltInFunction.PROVIDERRUNHEALTHCHECK] = (task, args) =>
        {
                    return Basic.IlBackend.IlRuntime.ProviderRunHealthCheck();
        },
    };

    private object? ExecuteBuiltInProvider(PCodeTask task, BuiltInFunction fn, object?[] args)
        => PROVIDERDispatch.TryGetValue(fn, out var handler)
            ? handler(task, args)
            : throw new InvalidOperationException(
                $"OP_CALL_BUILTIN: funcCode {(int)fn} non gestito da ExecuteBuiltInProvider (fn={fn}).");

}
