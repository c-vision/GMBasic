// FILE: Interpreter.Linker.Classes.cs

using System.Collections.Immutable;

namespace Basic;

/// <summary>
/// Contiene i metodi del Linker per l'aggregazione e la gestione dei moduli e dei loro simboli.
/// </summary>
public static partial class Linker
{
    /// <summary>
    /// Esegue una passata di linking per risolvere l'ereditarietà delle classi,
    /// rilevare le dipendenze circolari e popolare la proprietà `ResolvedBaseClass`.
    /// Al termine, invoca la validazione delle implementazioni astratte.
    /// </summary>
    /// <param name="program">L'istanza del programma contenente la SymbolTable con tutte le classi registrate.</param>
    public static void ResolveAndValidateClassInheritance(BasicProgram program)
    {
        var resolvedClasses = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        var allClasses = program.GetAllClasses().ToList();

        // Itera su ogni classe definita nel programma.
        foreach (var classDef in allClasses)
        {
            var symbol = program.Symbols.GetSymbol(classDef.SymbolId);
            var nsScope = program.Symbols.FindParentScopeOfType(symbol.ScopeId, SymbolTable.ScopeType.Namespace);
            var currentNamespace = nsScope?.Name ?? SymbolTable.GlobalScopeName;
            
            var qualifiedName = currentNamespace == SymbolTable.GlobalScopeName
                ? classDef.Name.Lexeme
                : $"{currentNamespace}.{classDef.Name.Lexeme}";

            if (!resolvedClasses.Contains(qualifiedName))
            {
                ResolveSingleClass(classDef, qualifiedName,
                    new HashSet<string>(StringComparer.OrdinalIgnoreCase), resolvedClasses, program);
            }
        }
        
        // Dopo che l'ereditarietà è completamente risolta, risolviamo e validiamo le interfacce.
        ResolveAndValidateInterfaceImplementations(allClasses, program);
        
        // Dopo che tutta la gerarchia è stata risolta, valida le implementazioni astratte.
        ValidateAbstractImplementations(allClasses, program);
    }
    
    /// <summary>
    /// [FASE 3] Esegue la risoluzione e la validazione dell'implementazione delle interfacce per tutte le classi e gli UDT.
    /// Questo metodo viene eseguito dopo la risoluzione dell'ereditarietà.
    /// </summary>
    private static void ResolveAndValidateInterfaceImplementations(IEnumerable<ClassDefinition> allClasses, BasicProgram program)
    {
        foreach (var classDef in allClasses)
        {
            if (classDef.ImplementedInterfaceNames.IsEmpty) continue;

            var resolvedInterfaces = ImmutableArray.CreateBuilder<InterfaceDefinition>();
            var modulePath = program.FileIdToPathMap.GetValueOrDefault(classDef.Position.FileId, string.Empty);

            // Risolve ogni nome di interfaccia in una InterfaceDefinition
            foreach (var interfaceNameToken in classDef.ImplementedInterfaceNames)
            {
                var resolvedType = ResolveDataType(new UnresolvedQualifiedType(new QualifiedNameExpr([interfaceNameToken])), program, modulePath);
                if (resolvedType is InterfaceType interfaceType)
                {
                    resolvedInterfaces.Add(interfaceType.Definition);
                }
                else
                {
                    throw CreateError(interfaceNameToken, $"Il tipo '{interfaceNameToken.Lexeme}' non è un'interfaccia valida.");
                }
            }
            
            // Aggiorna il nodo ClassDefinition con le interfacce risolte
            var updatedClassDef = classDef with { ResolvedInterfaces = resolvedInterfaces.ToImmutable() };
            program.Symbols.UpdateSymbolDeclaration(classDef.SymbolId, updatedClassDef);
            
            // Se la classe non è astratta, valida che implementi tutti i membri richiesti.
            if (!classDef.Flags.HasFlag(SymbolFlags.IsAbstract))
            {
                ValidateInterfaceMembers(updatedClassDef, program);
            }
        }
        
        // --- NUOVO: Risoluzione e validazione interfacce per UDT (TYPE) ---
        var allUdts = program.GetAllUserDefinedTypes().ToList();
        foreach (var udtDef in allUdts)
        {
            if (udtDef.ImplementedInterfaceNames.IsEmpty) continue;

            var resolvedInterfaces = ImmutableArray.CreateBuilder<InterfaceDefinition>();
            var modulePath = program.FileIdToPathMap.GetValueOrDefault(udtDef.Position.FileId, string.Empty);

            foreach (var interfaceNameToken in udtDef.ImplementedInterfaceNames)
            {
                var resolvedType = ResolveDataType(new UnresolvedQualifiedType(new QualifiedNameExpr([interfaceNameToken])), program, modulePath);
                if (resolvedType is InterfaceType interfaceType)
                {
                    resolvedInterfaces.Add(interfaceType.Definition);
                }
                else
                {
                    throw CreateError(interfaceNameToken, $"Il tipo '{interfaceNameToken.Lexeme}' non è un'interfaccia valida.");
                }
            }

            var updatedUdt = udtDef with { ResolvedInterfaces = resolvedInterfaces.ToImmutable() };
            program.Symbols.UpdateSymbolDeclaration(udtDef.SymbolId, updatedUdt);

            ValidateInterfaceMembersForUdt(updatedUdt, program);
        }
    }

    /// <summary>
    /// [NUOVO] Valida che un UDT implementi tutti i membri richiesti dalle interfacce che dichiara.
    /// </summary>
    private static void ValidateInterfaceMembersForUdt(UserDefinedType udtDef, BasicProgram program)
    {
        foreach (var implementedInterface in udtDef.ResolvedInterfaces)
        {
            foreach (var requiredMember in implementedInterface.Members)
            {
                var found = false;

                switch (requiredMember)
                {
                    case MethodMember requiredMethod:
                    {
                        var matchingMethod = udtDef.Methods.FirstOrDefault(m =>
                            m.OriginalName.Lexeme.Equals(requiredMethod.Procedure.OriginalName.Lexeme, StringComparison.OrdinalIgnoreCase) &&
                            m.Signature.Equals(requiredMethod.Procedure.Signature));
                        if (matchingMethod != null)
                        {
                            if (!matchingMethod.Flags.HasFlag(SymbolFlags.IsPublic))
                                throw CreateError(matchingMethod.OriginalName, $"Il metodo '{matchingMethod.OriginalName.Lexeme}' deve essere PUBLIC per implementare l'interfaccia '{implementedInterface.Name.Lexeme}'.");
                            found = true;
                        }
                        break;
                    }
                    case PropertyMember requiredProperty:
                    {
                        var matchingProp = udtDef.Properties.FirstOrDefault(p =>
                            p.OriginalName.Lexeme.Equals(requiredProperty.Property.OriginalName.Lexeme, StringComparison.OrdinalIgnoreCase));
                        if (matchingProp != null)
                        {
                            var dummyContext = new AstRewriteContext(
                                program.FileIdToPathMap.GetValueOrDefault(udtDef.Position.FileId, string.Empty),
                                null, new DbfContext(), false, false, udtDef.ScopeId);
                            if (!AreSameType(matchingProp.Type, requiredProperty.Property.Type, dummyContext, program))
                                throw CreateError(matchingProp.OriginalName, $"Il tipo della proprietà '{matchingProp.OriginalName.Lexeme}' non corrisponde al tipo richiesto dall'interfaccia '{implementedInterface.Name.Lexeme}'.");
                            found = true;
                        }
                        break;
                    }
                }

                if (!found)
                {
                    throw CreateError(udtDef.OriginalName, $"Il tipo '{udtDef.OriginalName.Lexeme}' non implementa il membro di interfaccia '{requiredMember.Name.Lexeme}' da '{implementedInterface.Name.Lexeme}'.");
                }
            }
        }
    }

    /// <summary>
    /// [FASE 3] Valida che una classe concreta implementi tutti i membri richiesti dalle sue interfacce.
    /// </summary>
    private static void ValidateInterfaceMembers(ClassDefinition classDef, BasicProgram program)
    {
        foreach (var implementedInterface in classDef.ResolvedInterfaces)
        {
            foreach (var requiredMember in implementedInterface.Members)
            {
                // Cerca un membro corrispondente nella classe o nella sua gerarchia.
                var foundImplementation = false;
                var currentClass = classDef;

                while (currentClass != null)
                {
                    var matchingMember = currentClass.Members.FirstOrDefault(m =>
                    {
                        if (m.GetType() != requiredMember.GetType() || !m.Name.Lexeme.Equals(requiredMember.Name.Lexeme, StringComparison.OrdinalIgnoreCase))
                            return false;

                        // Per i metodi, confronta la firma completa
                        if (m is MethodMember methodImpl && requiredMember is MethodMember methodReq)
                        {
                            return methodImpl.Procedure.Signature.Equals(methodReq.Procedure.Signature);
                        }
                        
                        // Per le proprietà, basta il nome e il tipo (la firma è più semplice)
                        if (m is PropertyMember propImpl && requiredMember is PropertyMember propReq)
                        {
                            // Crea un contesto fittizio per la chiamata a AreSameType
                            // Crea un contesto fittizio per la chiamata a AreSameType
                            var dummyContext = new AstRewriteContext(
                                program.FileIdToPathMap.GetValueOrDefault(classDef.Position.FileId, string.Empty),
                                null, new DbfContext(), false, false, classDef.ScopeId);
                            return AreSameType(propImpl.Property.Type, propReq.Property.Type, dummyContext, program);
                        }
                        
                        // Aggiungere logica per eventi se necessario
                        return false;
                    });

                    if (matchingMember != null)
                    {
                        // L'implementazione deve essere pubblica
                        if (matchingMember.Access != AccessModifier.Public)
                        {
                            throw CreateError(matchingMember.Name, $"Il membro '{matchingMember.Name.Lexeme}' deve essere dichiarato PUBLIC per implementare l'interfaccia '{implementedInterface.Name.Lexeme}'.");
                        }
                        foundImplementation = true;
                        break;
                    }
                    currentClass = currentClass.ResolvedBaseClass;
                }

                if (!foundImplementation)
                {
                    throw CreateError(classDef.Name, $"La classe '{classDef.Name.Lexeme}' non implementa il membro di interfaccia '{requiredMember.Name.Lexeme}' da '{implementedInterface.Name.Lexeme}'.");
                }
            }
        }
    }
    
    /// <summary>
    /// [NUOVO METODO] Itera su tutte le classi non astratte e verifica che implementino
    /// tutti i membri astratti ereditati dalla loro gerarchia di classi base.
    /// </summary>
    /// <param name="allClasses">Un elenco di tutte le definizioni di classe nel programma.</param>
    /// <param name="program">L'istanza del programma per l'accesso alla SymbolTable.</param>
    /// <exception cref="BasicException">Lanciata se una classe concreta non implementa un membro astratto richiesto.</exception>
    private static void ValidateAbstractImplementations(IEnumerable<ClassDefinition> allClasses, BasicProgram program)
    {
        foreach (var classDef in allClasses)
        {
            // La validazione si applica solo alle classi CONCRETE (non astratte).
            if (classDef.Flags.HasFlag(SymbolFlags.IsAbstract))
            {
                continue;
            }

            var abstractMethodsToImplement = new Dictionary<ProcedureSignature, MethodMember>();

            // Risali la gerarchia per collezionare tutti i metodi astratti.
            var currentBase = classDef.ResolvedBaseClass;
            while (currentBase != null)
            {
                var abstractMethodsInBase = currentBase.Members.OfType<MethodMember>()
                    .Where(m => m.Procedure.Flags.HasFlag(SymbolFlags.IsAbstract));

                foreach (var abstractMethod in abstractMethodsInBase)
                {
                    // Aggiungi il metodo astratto alla lista di quelli da implementare.
                    // Se una classe più in basso nella gerarchia lo ha già, verrà sovrascritto qui,
                    // che è corretto perché l'implementazione più vicina è quella che conta.
                    abstractMethodsToImplement[abstractMethod.Procedure.Signature] = abstractMethod;
                }
                currentBase = currentBase.ResolvedBaseClass;
            }

            // Ora, rimuovi dalla lista i metodi che sono stati implementati (OVERRIDE)
            // dalla classe corrente o da una delle sue classi base intermedie.
            var currentForOverrideCheck = classDef;
            while (currentForOverrideCheck != null)
            {
                var overriddenMethods = currentForOverrideCheck.Members.OfType<MethodMember>()
                    .Where(m => m.Procedure.Flags.HasFlag(SymbolFlags.IsOverride));

                foreach (var impl in overriddenMethods)
                {
                    // Se questo override implementa uno dei metodi astratti che cercavamo, rimuovilo dalla lista.
                    if (abstractMethodsToImplement.ContainsKey(impl.Procedure.Signature))
                    {
                        abstractMethodsToImplement.Remove(impl.Procedure.Signature);
                    }
                }
                currentForOverrideCheck = currentForOverrideCheck.ResolvedBaseClass;
            }

            // Se alla fine di tutto, la lista non è vuota, significa che la classe
            // non ha implementato tutti i metodi astratti richiesti.
            if (abstractMethodsToImplement.Count > 0)
            {
                var missingMethod = abstractMethodsToImplement.First().Value;
                throw new BasicException(classDef.Name,
                    $"La classe non astratta '{classDef.Name.Lexeme}' non implementa il membro astratto ereditato " +
                    $"'{missingMethod.Procedure.OriginalName.Lexeme}{missingMethod.Procedure.Signature}'.",
                    ErrorCode.SyntaxError);
            }
        }
    }
        
    /// <summary>
    /// Risolve ricorsivamente la catena di ereditarietà per una singola classe.
    /// Questa versione è stata rifattorizzata per interrogare direttamente la SymbolTable,
    /// eliminando la dipendenza da dizionari intermedi.
    /// </summary>
    private static void ResolveSingleClass(
        ClassDefinition classToResolve,
        string qualifiedName,
        HashSet<string> resolutionPath,
        HashSet<string> alreadyResolved,
        BasicProgram program)
    {
        if (alreadyResolved.Contains(qualifiedName)) return;

        if (!resolutionPath.Add(qualifiedName))
            throw new BasicException(
                ErrorCode.SyntaxError,
                classToResolve.Name.Position,
                $"Rilevata dipendenza circolare nell'ereditarietà che coinvolge la classe '{qualifiedName}'.");

        if (classToResolve.OriginalBaseClassName != null)
        {
            var (_, baseClassName, _, sourcePosition) = classToResolve.OriginalBaseClassName;

            // Z3: classe base QUALIFICATA (INHERITS MVC.HttpController) o da altro file —
            // il lexeme puntato ("MVC.HttpController") viene risolto cercando il tipo nel
            // namespace dichiarato; un nome nudo resta sul percorso legacy (FindSymbolsByName).
            ClassDefinition? baseDef = null;
            var baseParts = baseClassName.Split('.');
            if (baseParts.Length > 1)
            {
                var nsName = string.Join(".", baseParts.Take(baseParts.Length - 1));
                var typeName = baseParts[^1];
                var nsSymbols = program.Symbols.GetSymbolIdsByNamespace(nsName);
                if (nsSymbols != null)
                {
                    foreach (var sid in nsSymbols)
                    {
                        var symbol = program.Symbols.GetSymbol(sid);
                        if (symbol.DeclarationNode is ClassDefinition cand &&
                            cand.OriginalName.Lexeme.Equals(typeName, StringComparison.OrdinalIgnoreCase) &&
                            symbol.Flags.HasFlag(SymbolFlags.IsPublic))
                        {
                            baseDef = cand;
                            break;
                        }
                    }
                }
            }
            else
            {
                // Cerca la classe base interrogando la SymbolTable.
                var candidates = program.Symbols.FindSymbolsByName(baseClassName)
                    .Select(s => s.DeclarationNode)
                    .OfType<ClassDefinition>()
                    .ToList();

                switch (candidates.Count)
                {
                    case 0:
                        throw new BasicException(
                            ErrorCode.TypeNotDefined,
                            sourcePosition,
                            $"La classe base '{baseClassName}' non è stata trovata.");
                    case > 1:
                        throw new BasicException(
                            ErrorCode.AmbiguousCall,
                            sourcePosition,
                            $"Il nome della classe base '{baseClassName}' è ambiguo. Trovato in più namespace.");
                }

                baseDef = candidates.First();
            }

            if (baseDef == null)
            {
                throw new BasicException(
                    ErrorCode.TypeNotDefined,
                    sourcePosition,
                    $"La classe base '{baseClassName}' non è stata trovata.");
            }

            // Determina il nome qualificato della classe base per la ricorsione.
            var baseSymbol = program.Symbols.GetSymbol(baseDef.SymbolId);
            var baseNsScope = program.Symbols.FindParentScopeOfType(baseSymbol.ScopeId, SymbolTable.ScopeType.Namespace);
            var baseNamespace = baseNsScope?.Name ?? SymbolTable.GlobalScopeName;
            
            var resolvedBaseQualifiedName = baseNamespace == SymbolTable.GlobalScopeName
                ? baseDef.OriginalName.Lexeme
                : $"{baseNamespace}.{baseDef.OriginalName.Lexeme}";

            // Chiamata ricorsiva con la nuova firma (senza il dizionario).
            ResolveSingleClass(baseDef, resolvedBaseQualifiedName,
                resolutionPath, alreadyResolved, program);

            classToResolve.ResolvedBaseClass = baseDef;

            // Aggiorna il SymbolId della classe base nella definizione.
            var updatedDef = classToResolve with { BaseClassSymbolId = baseDef.SymbolId };
            
            // FIX: Riassegna la proprietà non posizionale persa dal 'with'
            updatedDef.ResolvedBaseClass = baseDef;
            
            program.Symbols.UpdateSymbolDeclaration(classToResolve.SymbolId, updatedDef);

            // --- DIAGNOSTICA 1: Verifica del salvataggio ---
            //Log($"[LINKER-DIAG] ResolveSingleClass SAVED: Class '{updatedDef.Name.Lexeme}' (ID {updatedDef.SymbolId}) -> BaseClassID: {updatedDef.BaseClassSymbolId}", true);
            
            ValidateMethodOverrides(updatedDef, program);
            ValidateBaseConstructorCalls(updatedDef);
        }

        resolutionPath.Remove(qualifiedName);
        alreadyResolved.Add(qualifiedName);
    }

    /// <summary>
    /// Valida le chiamate ai costruttori base (`BASE.NEW`) in una classe derivata.
    /// VERSIONE CORRETTA: Cerca la prima istruzione eseguibile, ignorando commenti o etichette,
    /// per trovare la chiamata al costruttore base e prevenire falsi negativi.
    /// </summary>
    private static void ValidateBaseConstructorCalls(ClassDefinition derivedClass)
    {
        var baseClass = derivedClass.ResolvedBaseClass;
        if (baseClass == null) return;

        var baseConstructors =
            baseClass.Members.OfType<ConstructorMember>().Select(c => c.Constructor).ToImmutableArray();
        var hasDefaultBaseConstructor = !baseConstructors.Any() || baseConstructors.Any(c => c.Parameters.IsEmpty);

        foreach (var constructorMember in derivedClass.Members.OfType<ConstructorMember>())
        {
            var derivedConstructor = constructorMember.Constructor;

            // Cerca la prima istruzione *eseguibile*, ignorando commenti (RemStmt) o etichette (LabelStmt).
            // Questo rende la validazione robusta contro nodi AST non eseguibili che potrebbero essere inseriti dal parser.
            var firstExecutableStmt = derivedConstructor.Body.FirstOrDefault(s => s is not RemStmt and not LabelStmt);

            if (firstExecutableStmt is BaseConstructorCallStmt baseCallStmt)
            {
                var argCount = baseCallStmt.Args.Length;

                var foundMatch = baseConstructors.Any(bc =>
                {
                    var dummyProcDef = new FunctionDefinition(
                        -1,
                        new Token(TokenKind.New, "NEW", null, SourcePosition.Unknown),
                        bc.Parameters,
                        new VoidType(),
                        ImmutableArray<Stmt>.Empty,
                        SymbolFlags.IsConstructor
                    );
                    return SymbolTable.IsProcedureSignatureCompatible(dummyProcDef, argCount);
                });

                if (!foundMatch)
                    throw new BasicException(baseCallStmt.BaseToken,
                        $"Nessun costruttore accessibile nella classe base '{baseClass.Name.Lexeme}' accetta {argCount} argomenti.",
                        ErrorCode.ArgumentCountMismatch);
            }
            else
            {
                if (!hasDefaultBaseConstructor)
                    throw new BasicException(derivedClass.Name,
                        $"Il costruttore della classe base '{baseClass.Name.Lexeme}' richiede argomenti, ma non è " +
                        $"stata fornita nessuna chiamata esplicita a 'BASE.NEW(...)' nel costruttore di '{derivedClass.Name.Lexeme}'.");
            }
        }
    }

    /// <summary>
    /// Valida tutti i metodi marcati con OVERRIDE in una classe.
    /// </summary>
    private static void ValidateMethodOverrides(ClassDefinition classDef, BasicProgram program)
    {
        var methodsToValidate = classDef.Members.OfType<MethodMember>()
            .Where(m => m.Procedure.Flags.HasFlag(SymbolFlags.IsOverride));

        foreach (var overridingMethodMember in methodsToValidate)
        {
            var overridingProcedure = overridingMethodMember.Procedure;
            var baseClass = classDef.ResolvedBaseClass;
            var foundMatch = false;

            while (baseClass != null)
            {
                var baseMethodCandidate = baseClass.Members.OfType<MethodMember>()
                    .FirstOrDefault(m => m.Procedure.Signature.Equals(overridingProcedure.Signature));

                if (baseMethodCandidate != null)
                {
                    // Un metodo può essere sovrascritto se è VIRTUAL oppure se è esso stesso un OVERRIDE
                    // (che implica virtualità continuata, a meno di Sealed che non gestiamo ancora qui).
                    var isOverridable = baseMethodCandidate.Procedure.Flags.HasFlag(SymbolFlags.IsVirtual) ||
                                        baseMethodCandidate.Procedure.Flags.HasFlag(SymbolFlags.IsOverride);

                    if (!isOverridable)
                        throw new BasicException(overridingMethodMember.Name,
                            $"Il metodo '{overridingMethodMember.Name.Lexeme}' non può sovrascrivere il membro della " +
                            $"classe base '{baseClass.Name.Lexeme}' perché non è dichiarato VIRTUAL o OVERRIDE.",
                            ErrorCode.NonVirtualMethodOverride);

                    if (baseMethodCandidate.Access == AccessModifier.Private)
                        throw new BasicException(overridingMethodMember.Name,
                            $"Il metodo '{overridingMethodMember.Name.Lexeme}' non può sovrascrivere il membro privato " +
                            $"'{baseMethodCandidate.Name.Lexeme}' della classe base '{baseClass.Name.Lexeme}'.");

                    if (overridingProcedure is FunctionDefinition overridingFunction)
                    {
                        if (baseMethodCandidate.Procedure is not FunctionDefinition baseFunction)
                            throw new BasicException(overridingMethodMember.Name,
                                $"La funzione '{overridingFunction.Name.Lexeme}' non può sovrascrivere " +
                                $"'{baseMethodCandidate.Name.Lexeme}' perché il membro della classe base non è una funzione.",
                                ErrorCode.TypeMismatch);

                        // Crea un contesto fittizio per la chiamata a AreSameType
                        var dummyContext = new AstRewriteContext(
                            program.FileIdToPathMap.GetValueOrDefault(classDef.Position.FileId, string.Empty),
                            null, new DbfContext(), false, false, classDef.ScopeId);
                        if (!AreSameType(overridingFunction.ReturnType, baseFunction.ReturnType, dummyContext, program))
                            throw new BasicException(overridingMethodMember.Name,
                                $"Il tipo di ritorno di '{overridingFunction.Name.Lexeme}' deve essere " +
                                $"'{baseFunction.ReturnType}' per corrispondere al metodo sovrascritto nella classe base " +
                                $"'{baseClass.Name.Lexeme}'.", ErrorCode.ReturnTypeMismatchInOverride);
                    }

                    foundMatch = true;
                    break;
                }

                baseClass = baseClass.ResolvedBaseClass;
            }

            if (!foundMatch)
            {
                throw new BasicException(overridingMethodMember.Name,
                    $"Nessun membro idoneo da sovrascrivere trovato per '{overridingMethodMember.Name.Lexeme}'.");
            }
        }
    }
}