// FILE: Basic.IlBackend.PCode.PCodeProcessor.Builtins.Vault.cs
// P-CODE VM: dominio VAULT (Second Brain L1) — delegazione a IlRuntime (unica
// implementazione), come MEMORY/AIVECTOR. Refactor per dominio (2026-08-30).

using Basic.Runtime;
using Basic.Runtime.Gpm;
using Compiler;

namespace Basic.IlBackend.PCode;

internal sealed partial class PCodeProcessor
{
    /// <summary>VAULT: delegazione dominio — case contigui estratti
    /// da PCodeProcessor.Builtins.cs (refactor B, de-mescolamento per dominio).</summary>
    private static readonly HashSet<BuiltInFunction> VAULTBuiltInFunctions = new HashSet<BuiltInFunction>
    {
        BuiltInFunction.AIVAULTCREATE,
        BuiltInFunction.AIVAULTINDEX,
        BuiltInFunction.AIVAULTINFO,
        BuiltInFunction.AIVAULTFOLDERS,
        BuiltInFunction.AIVAULTCOUNT,
        BuiltInFunction.AIVAULTLIST,
        BuiltInFunction.AIVAULTWATCH,
        BuiltInFunction.AIVAULTPOLICY,
        BuiltInFunction.AIVAULTADDFOLDER,
        BuiltInFunction.AIVAULTREMOVEFOLDER,
        BuiltInFunction.AIVAULTSETFOLDERPROPS,
        BuiltInFunction.AINOTELINK,
        BuiltInFunction.AINOTEUNLINK,
        BuiltInFunction.AINOTESETALIAS,
        BuiltInFunction.AISYNCFOLDER,
        BuiltInFunction.AIINGESTWEB,
        BuiltInFunction.AIBRAINUSE,
        BuiltInFunction.AIBRAINDETACH,
        BuiltInFunction.AIBRAINUNINSTALL,
        BuiltInFunction.AIBRAININSTALL,
        BuiltInFunction.AIBRAINLIST,
        BuiltInFunction.AIBRAININFO,
        BuiltInFunction.AIBRAINUSED,
        BuiltInFunction.AISEARCHMULTI,
        BuiltInFunction.AIRAGCONTEXTMULTI,
        BuiltInFunction.AIASKMULTI,
        BuiltInFunction.GRAPHQUERYMULTI,
        BuiltInFunction.GRAPHNEIGHBORS,
        BuiltInFunction.GRAPHPATH,
        BuiltInFunction.GRAPHPAGERANK,
        BuiltInFunction.GRAPHCONNECTED,
        BuiltInFunction.GRAPHSTATS,
        BuiltInFunction.GRAPHSTRONGLYCONNECTED,
        BuiltInFunction.GRAPHTOPOSORT,
        BuiltInFunction.GRAPHBIPARTITE,
        BuiltInFunction.GRAPHBETWEENNESS,
        BuiltInFunction.GRAPHCLUSTERING,
        BuiltInFunction.GRAPHDIAMETER,
        BuiltInFunction.GRAPHMST,
        BuiltInFunction.GRAPHLOUVAIN,
        BuiltInFunction.GRAPHMAXFLOW,
        BuiltInFunction.GRAPHDIJKSTRA,
        BuiltInFunction.GRAPHHASCYCLE,
        BuiltInFunction.GRAPHECCENTRICITY,
        BuiltInFunction.GRAPHDFS,
        BuiltInFunction.GRAPHBFS,
        BuiltInFunction.GRAPHBUILD,
        BuiltInFunction.GRAPHUPDATE,
        BuiltInFunction.GRAPHFINDPATH,
        BuiltInFunction.GRAPHRDD,
        BuiltInFunction.AIENRICH,
        BuiltInFunction.AIQAANSWER,
        BuiltInFunction.AISUGGESTTAGS,
        BuiltInFunction.AISIGNNOTE,
        BuiltInFunction.AIVERIFYNOTE,
        BuiltInFunction.AISHAREENCRYPT,
        BuiltInFunction.AISHAREDECRYPT,
        BuiltInFunction.AIBRAINVAULTBUILD,
        BuiltInFunction.AIBRAINVAULTQUERY,
        BuiltInFunction.AIBRAINVAULTWRITE,
        BuiltInFunction.GRAPHDOT,
        BuiltInFunction.GRAPHSUBGRAPH,
        BuiltInFunction.GRAPHBYTAG,
        BuiltInFunction.GRAPHBYFOLDER,
        BuiltInFunction.GRAPHISVALID,
        BuiltInFunction.NOTECREATE,
        BuiltInFunction.NOTEUPDATE,
        BuiltInFunction.NOTEAPPEND,
        BuiltInFunction.NOTEDELETE,
        BuiltInFunction.NOTEMOVE,
        BuiltInFunction.NOTEGET,
        BuiltInFunction.NOTETITLE,
        BuiltInFunction.NOTETAGS,
        BuiltInFunction.NOTEWORDCOUNT,
        BuiltInFunction.NOTEEXISTS,
        BuiltInFunction.NOTEFOLDER,
        BuiltInFunction.LINKSOUT,
        BuiltInFunction.BACKLINKS,
        BuiltInFunction.LINKDANGLING,
        BuiltInFunction.LINKJSON,
        BuiltInFunction.INGESTFILE,
        BuiltInFunction.INGESTFOLDER,
    };

    /// <summary>Vault: dispatch per BuiltInFunction a TABELLA (Dictionary), non switch (refactor §18).</summary>
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>>? _vaultDispatch;
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>> VAULTDispatch
        => _vaultDispatch ??= BuildVaultDispatch();
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>> BuildVaultDispatch() => new()
    {
        [BuiltInFunction.AIVAULTCREATE] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.VaultCreate(
                    ArgOr(args, 0)?.ToString() ?? "",
                    ArgOr(args, 1)?.ToString() ?? "",
                    args.Length > 2 ? ArgOr(args, 2)?.ToString() : null);
        },
        [BuiltInFunction.AIVAULTINDEX] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.VaultIndex(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.AIVAULTINFO] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.VaultInfo(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.AIVAULTFOLDERS] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.VaultFolders(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.AIVAULTCOUNT] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.VaultCount(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.AIVAULTLIST] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.VaultList();
        },
        [BuiltInFunction.AIVAULTWATCH] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.VaultWatch(
                    ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "");
        },
        [BuiltInFunction.AIVAULTPOLICY] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.VaultPolicy(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.AIVAULTADDFOLDER] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.VaultAddFolder(
                    ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "",
                    args.Length > 2 ? ArgOr(args, 2)?.ToString() : null);
        },
        [BuiltInFunction.AIVAULTREMOVEFOLDER] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.VaultRemoveFolder(
                    ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "");
        },
        [BuiltInFunction.AIVAULTSETFOLDERPROPS] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.VaultSetFolderProps(
                    ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "",
                    args.Length > 2 ? ArgOr(args, 2)?.ToString() : null, null);
        },
        [BuiltInFunction.AINOTELINK] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.NoteLink(
                    ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "",
                    ArgOr(args, 2)?.ToString() ?? "", args.Length > 3 ? ArgOr(args, 3)?.ToString() : null);
        },
        [BuiltInFunction.AINOTEUNLINK] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.NoteUnlink(
                    ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "", ArgOr(args, 2)?.ToString() ?? "");
        },
        [BuiltInFunction.AINOTESETALIAS] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.NoteSetAlias(
                    ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "",
                    ArgOr(args, 2)?.ToString() ?? "", ArgOr(args, 3)?.ToString() ?? "");
        },
        [BuiltInFunction.AISYNCFOLDER] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.SyncFolder(
                    ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "",
                    args.Length > 2 ? ArgOr(args, 2)?.ToString() : null,
                    args.Length > 3 ? ArgOr(args, 3)?.ToString() : null);
        },
        [BuiltInFunction.AIINGESTWEB] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.IngestWeb(
                    ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "",
                    args.Length > 2 ? ArgOr(args, 2)?.ToString() : null);
        },
        [BuiltInFunction.AIBRAINUSE] = (task, args) =>
        {
                {
                    var names = new System.Collections.Generic.List<string>();
                    foreach (var a in args ?? [])
                        names.Add(a?.ToString() ?? "");
                    return Basic.IlBackend.IlRuntime.BrainUse(string.Join(",", names));
                }
        },
        [BuiltInFunction.AIBRAINDETACH] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.BrainDetach(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.AIBRAINUNINSTALL] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.BrainUninstall(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.AIBRAININSTALL] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.BrainInstall(
                    ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "");
        },
        [BuiltInFunction.AIBRAINLIST] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.BrainList();
        },
        [BuiltInFunction.AIBRAININFO] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.BrainInfo(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.AIBRAINUSED] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.BrainUsed();
        },
        [BuiltInFunction.AISEARCHMULTI] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.AiSearchMulti(
                    ArgOr(args, 0)?.ToString() ?? "",
                    args.Length > 1 ? SafeDouble(ArgOr(args, 1)) : 5,
                    args.Length > 2 ? ArgOr(args, 2)?.ToString() : null,
                    args.Length > 3 ? ArgOr(args, 3)?.ToString() : null,
                    args.Length > 4 ? SafeDouble(ArgOr(args, 4)) : 1,
                    args.Length > 5 ? SafeDouble(ArgOr(args, 5)) : double.NaN,
                    args.Length > 6 ? SafeDouble(ArgOr(args, 6)) : 0);
        },
        [BuiltInFunction.AIRAGCONTEXTMULTI] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.AiRagContextMulti(
                    ArgOr(args, 0)?.ToString() ?? "",
                    args.Length > 1 ? SafeDouble(ArgOr(args, 1)) : 5,
                    args.Length > 2 ? SafeDouble(ArgOr(args, 2)) : 1,
                    args.Length > 3 ? SafeDouble(ArgOr(args, 3)) : double.NaN);
        },
        [BuiltInFunction.AIASKMULTI] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.AiAskMulti(
                    ArgOr(args, 0)?.ToString() ?? "",
                    args.Length > 1 ? SafeDouble(ArgOr(args, 1)) : 5,
                    args.Length > 2 ? SafeDouble(ArgOr(args, 2)) : 1,
                    args.Length > 3 ? SafeDouble(ArgOr(args, 3)) : double.NaN);
        },
        [BuiltInFunction.GRAPHQUERYMULTI] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.GraphQueryMulti(
                    ArgOr(args, 0)?.ToString() ?? "",
                    args.Length > 1 ? ArgOr(args, 1)?.ToString() : null);
        },
        [BuiltInFunction.GRAPHNEIGHBORS] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.GraphNeighbors(
                    ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "");
        },
        [BuiltInFunction.GRAPHPATH] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.GraphPath(
                    ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "", ArgOr(args, 2)?.ToString() ?? "");
        },
        [BuiltInFunction.GRAPHPAGERANK] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.GraphPageRank(
                    ArgOr(args, 0)?.ToString() ?? "", args.Length > 1 ? SafeDouble(ArgOr(args, 1)) : 10);
        },
        [BuiltInFunction.GRAPHCONNECTED] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.GraphConnected(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.GRAPHSTATS] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.GraphStats(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.GRAPHSTRONGLYCONNECTED] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.GraphStronglyConnected(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.GRAPHTOPOSORT] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.GraphTopoSort(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.GRAPHBIPARTITE] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.GraphBipartite(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.GRAPHBETWEENNESS] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.GraphBetweenness(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.GRAPHCLUSTERING] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.GraphClustering(
                    ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "");
        },
        [BuiltInFunction.GRAPHDIAMETER] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.GraphDiameter(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.GRAPHMST] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.GraphMst(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.GRAPHLOUVAIN] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.GraphLouvain(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.GRAPHMAXFLOW] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.GraphMaxFlow(
                    ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "", ArgOr(args, 2)?.ToString() ?? "");
        },
        [BuiltInFunction.GRAPHDIJKSTRA] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.GraphDijkstra(
                    ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "", ArgOr(args, 2)?.ToString() ?? "");
        },
        [BuiltInFunction.GRAPHHASCYCLE] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.GraphHasCycle(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.GRAPHECCENTRICITY] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.GraphEccentricity(
                    ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "");
        },
        [BuiltInFunction.GRAPHDFS] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.GraphDfs(ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "");
        },
        [BuiltInFunction.GRAPHBFS] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.GraphBfs(ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "");
        },
        [BuiltInFunction.GRAPHBUILD] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.GraphBuild(
                    ArgOr(args, 0)?.ToString() ?? "", args.Length > 1 ? ArgOr(args, 1)?.ToString() : null);
        },
        [BuiltInFunction.GRAPHUPDATE] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.GraphBuild(
                    ArgOr(args, 0)?.ToString() ?? "", args.Length > 1 ? ArgOr(args, 1)?.ToString() : null);
        },
        [BuiltInFunction.GRAPHFINDPATH] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.GraphFindPath(
                    ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "");
        },
        [BuiltInFunction.GRAPHRDD] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.GraphRdd(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.AIENRICH] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.Enrich(
                    ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "");
        },
        [BuiltInFunction.AIQAANSWER] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.QaAnswer(
                    ArgOr(args, 0)?.ToString() ?? "",
                    ArgOr(args, 1)?.ToString() ?? "",
                    args.Length > 2 ? SafeDouble(ArgOr(args, 2)) : 3);
        },
        [BuiltInFunction.AISUGGESTTAGS] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.SuggestTags(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.AISIGNNOTE] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.SignNote(
                    ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "");
        },
        [BuiltInFunction.AIVERIFYNOTE] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.VerifyNote(
                    ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "", ArgOr(args, 2)?.ToString() ?? "");
        },
        [BuiltInFunction.AISHAREENCRYPT] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.ShareEncrypt(
                    ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "");
        },
        [BuiltInFunction.AISHAREDECRYPT] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.ShareDecrypt(
                    ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "");
        },
        [BuiltInFunction.AIBRAINVAULTBUILD] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.BrainVaultBuild(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.AIBRAINVAULTQUERY] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.BrainVaultQuery(
                    ArgOr(args, 0)?.ToString() ?? "", args.Length > 1 ? SafeDouble(ArgOr(args, 1)) : 3);
        },
        [BuiltInFunction.AIBRAINVAULTWRITE] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.BrainVaultWrite(
                    ArgOr(args, 0)?.ToString() ?? "", args.Length > 1 ? SafeDouble(ArgOr(args, 1)) : 3);
        },
        [BuiltInFunction.GRAPHDOT] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.GraphDot(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.GRAPHSUBGRAPH] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.GraphSubgraph(
                    ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "",
                    args.Length > 2 ? SafeDouble(ArgOr(args, 2)) : 1);
        },
        [BuiltInFunction.GRAPHBYTAG] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.GraphByTag(
                    ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "");
        },
        [BuiltInFunction.GRAPHBYFOLDER] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.GraphByFolder(
                    ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "");
        },
        [BuiltInFunction.GRAPHISVALID] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.GraphIsValid(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.NOTECREATE] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.NoteCreate(
                    "",
                    ArgOr(args, 0)?.ToString() ?? "",
                    args.Length > 1 ? ArgOr(args, 1)?.ToString() : null,
                    args.Length > 2 ? ArgOr(args, 2)?.ToString() : null);
        },
        [BuiltInFunction.NOTEUPDATE] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.NoteUpdate(
                    "", ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "");
        },
        [BuiltInFunction.NOTEAPPEND] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.NoteAppend(
                    "", ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "");
        },
        [BuiltInFunction.NOTEDELETE] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.NoteDelete("", ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.NOTEMOVE] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.NoteMove(
                    "", ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "");
        },
        [BuiltInFunction.NOTEGET] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.NoteGet(ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "");
        },
        [BuiltInFunction.NOTETITLE] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.NoteTitle(ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "");
        },
        [BuiltInFunction.NOTETAGS] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.NoteTags(ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "");
        },
        [BuiltInFunction.NOTEWORDCOUNT] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.NoteWordCount(ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "");
        },
        [BuiltInFunction.NOTEEXISTS] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.NoteExists(ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "");
        },
        [BuiltInFunction.NOTEFOLDER] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.NoteFolder(ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "");
        },
        [BuiltInFunction.LINKSOUT] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.LinksOut(ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "");
        },
        [BuiltInFunction.BACKLINKS] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.Backlinks(ArgOr(args, 0)?.ToString() ?? "", ArgOr(args, 1)?.ToString() ?? "");
        },
        [BuiltInFunction.LINKDANGLING] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.LinkDangling(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.LINKJSON] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.LinkJson(ArgOr(args, 0)?.ToString() ?? "");
        },
        [BuiltInFunction.INGESTFILE] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.IngestFile(
                    "",
                    ArgOr(args, 0)?.ToString() ?? "",
                    args.Length > 1 ? ArgOr(args, 1)?.ToString() : null);
        },
        [BuiltInFunction.INGESTFOLDER] = (task, args) =>
        {
                return Basic.IlBackend.IlRuntime.IngestFolder(
                    "",
                    ArgOr(args, 0)?.ToString() ?? "",
                    args.Length > 1 ? SafeDouble(ArgOr(args, 1)) : 1);
        },
    };

    private object? ExecuteBuiltInVault(PCodeTask task, BuiltInFunction fn, object?[] args)
        => VAULTDispatch.TryGetValue(fn, out var handler)
            ? handler(task, args)
            : throw new InvalidOperationException(
                $"OP_CALL_BUILTIN: funcCode {(int)fn} non gestito da ExecuteBuiltInVault (fn={fn}).");

}
