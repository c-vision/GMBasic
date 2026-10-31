// FILE: Basic.Linker.Mvc.cs

using System.Collections.Immutable;

namespace Basic;

/// <summary>
/// Contiene le trasformazioni semantiche del framework MVC.
/// </summary>
public static partial class Linker
{
    private enum MvcActionKind
    {
        None,
        GetAll,
        Get,
        Post,
        Put,
        Delete,
        Patch
    }

    private sealed record MvcActionDescriptor(
        int ControllerClassSymbolId,
        int ActionProcedureSymbolId,
        MvcActionKind ActionKind,
        ProcedureDefinition Procedure);

    private sealed record MvcControllerDescriptor(
        int ControllerClassSymbolId,
        int CanonicalHttpControllerSymbolId,
        string RouteSegment,
        ClassDefinition Definition,
        ImmutableArray<MvcActionDescriptor> Actions);

    /// <summary>
    /// Scopre semanticamente controller e azioni MVC dopo la risoluzione dell'ereditarietà.
    /// </summary>
    /// <param name="program">Programma con simboli e gerarchie di classe già risolti.</param>
    /// <param name="mainFilePath">Percorso del modulo principale (entry-point), per individuare
    /// il module in <see cref="BasicProgram.LoadedModules"/>: <c>program.MainModule</c> viene
    /// impostato SOLO a fine linking (passata 3.6 di Orchestrator), quindi qui è ancora null.</param>
    public static void PrepareMvcBridges(BasicProgram program, string mainFilePath)
    {
        ArgumentNullException.ThrowIfNull(program);

        var httpControllerSymbolId = FindCanonicalHttpControllerSymbolId(program);
        if (httpControllerSymbolId < 0) return;

        var actionKindsBySymbolId = BuildMvcActionKindsBySymbolId(program);
        var controllers = program.Symbols.GetAllClasses()
            .Where(classDefinition =>
                classDefinition.SymbolId != httpControllerSymbolId &&
                InheritsFrom(classDefinition, httpControllerSymbolId))
            .Select(classDefinition => CreateControllerDescriptor(
                program,
                classDefinition,
                httpControllerSymbolId,
                actionKindsBySymbolId))
            .Where(descriptor => descriptor.Actions.Length > 0)
            .ToImmutableArray();

        if (controllers.Length == 0) return;

        GenerateMvcBridges(program, controllers, mainFilePath);
    }

    private static int FindCanonicalHttpControllerSymbolId(BasicProgram program)
    {
        var candidates = program.Symbols.FindSymbolsByName("HttpController")
            .Where(symbol => symbol.DeclarationNode is ClassDefinition)
            .Where(symbol => string.Equals(
                program.Symbols.GetNamespaceNameForSymbol(symbol.Id),
                "MVC",
                StringComparison.OrdinalIgnoreCase))
            .Select(symbol => symbol.Id)
            .ToArray();

        return candidates.Length == 1 ? candidates[0] : SymbolTable.UnresolvedSymbolId;
    }

    private static ImmutableDictionary<int, MvcActionKind> BuildMvcActionKindsBySymbolId(BasicProgram program)
    {
        var builder = ImmutableDictionary.CreateBuilder<int, MvcActionKind>();
        AddActionSymbols(program, builder, "GetAll", MvcActionKind.GetAll);
        AddActionSymbols(program, builder, "Get", MvcActionKind.Get);
        AddActionSymbols(program, builder, "Post", MvcActionKind.Post);
        AddActionSymbols(program, builder, "Put", MvcActionKind.Put);
        AddActionSymbols(program, builder, "Delete", MvcActionKind.Delete);
        AddActionSymbols(program, builder, "Patch", MvcActionKind.Patch);
        return builder.ToImmutable();
    }

    private static void AddActionSymbols(
        BasicProgram program,
        IDictionary<int, MvcActionKind> actionKindsBySymbolId,
        string canonicalName,
        MvcActionKind actionKind)
    {
        foreach (var symbol in program.Symbols.FindSymbolsByName(canonicalName))
        {
            if (symbol.DeclarationNode is ProcedureDefinition)
                actionKindsBySymbolId[symbol.Id] = actionKind;
        }
    }

    private static bool InheritsFrom(ClassDefinition classDefinition, int baseClassSymbolId)
    {
        for (var current = classDefinition.ResolvedBaseClass; current is not null; current = current.ResolvedBaseClass)
        {
            if (current.SymbolId == baseClassSymbolId) return true;
        }

        return false;
    }

    private static MvcControllerDescriptor CreateControllerDescriptor(
        BasicProgram program,
        ClassDefinition classDefinition,
        int httpControllerSymbolId,
        IReadOnlyDictionary<int, MvcActionKind> actionKindsBySymbolId)
    {
        var actions = classDefinition.Members
            .OfType<MethodMember>()
            .Where(member => member.Access == AccessModifier.Public)
            .Where(member => member.Procedure is FunctionDefinition { ReturnType: VoidType })
            .Where(member => actionKindsBySymbolId.ContainsKey(member.Procedure.SymbolId))
            .Select(member => new MvcActionDescriptor(
                classDefinition.SymbolId,
                member.Procedure.SymbolId,
                actionKindsBySymbolId[member.Procedure.SymbolId],
                member.Procedure))
            .ToImmutableArray();

        return new MvcControllerDescriptor(
            classDefinition.SymbolId,
            httpControllerSymbolId,
            CreateRouteSegment(program.Symbols.GetSymbol(classDefinition.SymbolId).OriginalName),
            classDefinition,
            actions);
    }

    private static string CreateRouteSegment(string canonicalClassName)
    {
        const string suffix = "Controller";
        var routeName = canonicalClassName.EndsWith(suffix, StringComparison.OrdinalIgnoreCase)
            ? canonicalClassName[..^suffix.Length]
            : canonicalClassName;
        return routeName.ToLowerInvariant();
    }

    private static void GenerateMvcBridges(
        BasicProgram program,
        ImmutableArray<MvcControllerDescriptor> controllers,
        string mainFilePath)
    {
        var mainModule = ResolveMainModule(program, mainFilePath);
        var declarations = mainModule.Declarations.ToList();
        var statements = mainModule.Statements.ToDictionary(entry => entry.Key, entry => entry.Value.ToList());
        var nextSyntheticLine = statements.Count == 0 ? -1 : statements.Keys.Min() - 1;

        program.Symbols.ResetScope();
        foreach (var controller in controllers)
        {
            foreach (var action in controller.Actions)
            {
                var bridge = RegisterProcedure(program, CreateBridge(program, controller, action), mainModule.IsStrict);
                declarations.Add(bridge);

                var route = CreateRouteStatement(program, controller, action, bridge);
                statements[nextSyntheticLine--] = [route];
            }
        }

        program.MainModule = new BasicModule(
            mainModule.FilePath,
            mainModule.IsStrict,
            mainModule.SourceMap,
            statements,
            mainModule.Imports,
            declarations,
            mainModule.LibraryName);
        program.LoadedModules[mainModule.FilePath] = program.MainModule;
    }

    private static FunctionDefinition CreateBridge(
        BasicProgram program,
        MvcControllerDescriptor controller,
        MvcActionDescriptor action)
    {
        var position = action.Procedure.Position;
        var bridgeToken = SyntheticToken(
            $"__Mvc_{controller.ControllerClassSymbolId}_{action.ActionProcedureSymbolId}",
            position);
        // Nomi locali sintetici UNIVOCI per bridge: con più controller/azioni lo stesso nome
        // locale in procedure diverse confonde il compilatore PCode (risolve lo slot del 1°),
        // bloccando la compilazione o chiamando il metodo sull'istanza sbagliata.
        var localPrefix = $"__mvc_{controller.ControllerClassSymbolId}_{action.ActionProcedureSymbolId}";
        var requestToken = SyntheticToken(localPrefix + "_req", position);
        var responseToken = SyntheticToken(localPrefix + "_res", position);
        var controllerToken = SyntheticToken(localPrefix + "_ctl", position);

        var requestParameter = new Parameter(
            SymbolTable.UnresolvedSymbolId,
            requestToken,
            new HttpRequestType(requestToken),
            SymbolFlags.None,
            null,
            true);
        var responseParameter = new Parameter(
            SymbolTable.UnresolvedSymbolId,
            responseToken,
            new HttpResponseType(responseToken),
            SymbolFlags.None,
            null,
            true);

        var controllerDeclaration = new VariableDeclaration(
            SymbolTable.UnresolvedSymbolId,
            controllerToken,
            controllerToken,
            [],
            new ClassType(controller.Definition),
            new NewExpr(new ClassType(controller.Definition), [], null, new Token(TokenKind.New, "NEW", null, position)),
            false,
            true,
            SymbolFlags.IsLocal | SymbolFlags.IsSynthetic);

        var setContextMember = FindRequiredMethod(program, controller.CanonicalHttpControllerSymbolId, "SetContext");
        var setContextProcedure = (ProcedureDefinition)setContextMember.DeclarationNode!;
        var controllerExpression = new VariableExpr(SymbolTable.UnresolvedSymbolId, controllerToken, controllerToken);
        var body = ImmutableArray.CreateBuilder<Stmt>();
        body.Add(new DeclarationStmt(new Token(TokenKind.Dim, "DIM", null, position), [controllerDeclaration]));
        // Iniezione del contesto via SetContext (chiamata a un metodo della base, NON
        // assegnazione diretta ai campi ereditati: questi non sono assegnabili da un'istanza
        // esterna — i campi ereditati si scrivono solo via SELF nella classe che li dichiara).
        var setContextCall = new MethodCallExpr(
            controllerExpression,
            program.Symbols.GetSymbol(setContextMember.Id).OriginalToken,
            [
                new VariableExpr(SymbolTable.UnresolvedSymbolId, requestToken, requestToken),
                new VariableExpr(SymbolTable.UnresolvedSymbolId, responseToken, responseToken)
            ])
        {
            ResolvedDefinition = setContextProcedure
        };
        body.Add(new ExpressionStmt(setContextCall));

        var actionArguments = CreateActionArguments(program, action, requestToken);
        var actionCall = new MethodCallExpr(
            controllerExpression,
            program.Symbols.GetSymbol(action.ActionProcedureSymbolId).OriginalToken,
            actionArguments)
        {
            ResolvedDefinition = action.Procedure
        };
        body.Add(new ExpressionStmt(actionCall));
        // Le procedure PARSATE ricevono sempre un EndProcedureStmt (da END SUB/END FUNCTION),
        // che Compiler.VisitEndProcedureStatement usa per emettere l'OP_RETURN di chiusura. Un
        // corpo sintetico costruito qui in C# non lo aveva mai: senza, il ciclo di compilazione
        // (CompileProcedures) finiva semplicemente di iterare le statement di QUESTA bridge
        // senza emettere alcun RETURN, e l'esecuzione CADEVA (fall-through) nel bytecode della
        // PROSSIMA procedura compilata subito dopo nel chunk — che, per un programma con
        // `HTTP SERVER`, è tipicamente l'inizializzazione delle variabili GLOBALI di modulo
        // (`DIM x = ...`). Risultato osservato: ogni richiesta HTTP ri-eseguiva silenziosamente
        // gli inizializzatori dei globali subito dopo l'azione del controller, azzerandoli
        // (LIMITE-11, "globali re-inizializzate a ogni richiesta").
        body.Add(new EndProcedureStmt(bridgeToken));

        return new FunctionDefinition(
            SymbolTable.UnresolvedSymbolId,
            bridgeToken,
            [requestParameter, responseParameter],
            new VoidType(),
            body.ToImmutable(),
            SymbolFlags.IsPublic | SymbolFlags.IsSynthetic)
        {
            OriginalName = bridgeToken
        };
    }

    private static ImmutableArray<Expr> CreateActionArguments(
        BasicProgram program,
        MvcActionDescriptor action,
        Token requestToken)
    {
        if (action.Procedure.Parameters.Length == 0) return [];
        if (action.Procedure.Parameters.Length != 1)
            throw new BasicException(
                ErrorCode.SyntaxError,
                action.Procedure.Position,
                "Le azioni MVC supportano al massimo un parametro path nella prima implementazione.");

        var parameter = action.Procedure.Parameters[0];
        var parameterName = program.Symbols.GetSymbol(parameter.SymbolId).OriginalName;
        var request = new VariableExpr(SymbolTable.UnresolvedSymbolId, requestToken, requestToken);
        var paramsAccess = new FieldAccessExpr(
            request,
            new Token(TokenKind.Identifier, "Params", null, parameter.Position));
        var value = new IndexedAccessExpr(
            paramsAccess,
            [new StringExpr(parameterName, new Token(TokenKind.StringLiteral, parameterName, parameterName, parameter.Position))]);

        if (parameter.DataType is StringType) return [value];
        if (parameter.DataType is not (IntegerType or LongType or SingleType or DoubleType))
            throw new BasicException(
                ErrorCode.TypeMismatch,
                parameter.Position,
                $"Tipo parametro MVC non supportato: {parameter.DataType}.");

        var systemToken = new Token(TokenKind.Identifier, "SYSTEM", null, parameter.Position);
        var valToken = new Token(TokenKind.Val, "VAL", null, parameter.Position);
        return [new BuiltinFunctionCallExpr(systemToken, valToken, [value], new DoubleType())];
    }

    private static HttpRouteStmt CreateRouteStatement(
        BasicProgram program,
        MvcControllerDescriptor controller,
        MvcActionDescriptor action,
        ProcedureDefinition bridge)
    {
        var position = action.Procedure.Position;
        var method = action.ActionKind switch
        {
            MvcActionKind.GetAll or MvcActionKind.Get => "GET",
            MvcActionKind.Post => "POST",
            MvcActionKind.Put => "PUT",
            MvcActionKind.Delete => "DELETE",
            MvcActionKind.Patch => "PATCH",
            _ => throw new InvalidOperationException("Azione MVC non classificata.")
        };
        var path = action.Procedure.Parameters.Length == 0
            ? $"/api/{controller.RouteSegment}"
            : $"/api/{controller.RouteSegment}/{{{program.Symbols.GetSymbol(action.Procedure.Parameters[0].SymbolId).OriginalName}}}";
        var methodToken = new Token(TokenKind.StringLiteral, method, method, position);
        var pathToken = new Token(TokenKind.StringLiteral, path, path, position);
        return new HttpRouteStmt(
            new StringExpr(method, methodToken),
            new StringExpr(path, pathToken),
            bridge.OriginalName);
    }

    private static SymbolTable.Symbol FindRequiredMethod(BasicProgram program, int ownerClassSymbolId, string name)
    {
        var candidates = program.Symbols.FindSymbolsByName(name)
            .Where(symbol => symbol.Flags.HasFlag(SymbolFlags.IsMethod))
            .Where(symbol => program.Symbols.GetScope(symbol.ScopeId).OwnerSymbolId == ownerClassSymbolId)
            .ToArray();
        if (candidates.Length == 1) return candidates[0];

        throw new BasicException(
            ErrorCode.TypeNotDefined,
            SourcePosition.Unknown,
            $"Il metodo MVC richiesto '{name}' non è definito univocamente in HttpController.");
    }

    private static Token SyntheticToken(string value, SourcePosition position) =>
        new(TokenKind.Identifier, value, null, position);

    /// <summary>
    /// Individua il modulo principale in <see cref="BasicProgram.LoadedModules"/> dal path
    /// dell'entry-point (canonico, con fallback al path grezzo). Stesso contratto della passata
    /// 3.6 di Orchestrator: <c>program.MainModule</c> è ancora null durante il linking.
    /// </summary>
    private static BasicModule ResolveMainModule(BasicProgram program, string mainFilePath)
    {
        var canonical = Path.GetFullPath(mainFilePath);
        if (program.LoadedModules.TryGetValue(canonical, out var main))
            return main;
        if (program.LoadedModules.TryGetValue(mainFilePath, out main))
            return main;

        throw new InvalidOperationException(
            $"Internal Linker Error (MVC): main module not found for path '{canonical}'.");
    }
}
