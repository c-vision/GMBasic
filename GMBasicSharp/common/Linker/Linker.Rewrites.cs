// FILE: Interpreter.Linker.Rewrites.cs

using System.Collections.Immutable;
using System.Collections.Generic;

// ReSharper disable RedundantArgumentDefaultValue

namespace Basic;

public static partial class Linker
{
    /// <summary>§4.10(f) — hook INIETTATO dal layer che vede il grafo (`GMBasic.SemanticGraph`).
    /// Il Linker NON referenzia il grafo (ciclo SemanticGraph→Compilation): il folder e' iniettato.
    /// Ritorna la stringa-costante per AI.SYMBOL/IMPACT/CALLMAP, o null se non gestito.
    /// args: (program, TokenKind, name, depth).</summary>
    public static System.Func<BasicProgram, TokenKind, string, int, string?>? SemanticQueryFolder;

    private const bool RewriteLog = false;

    /// <summary>
    /// Riscrive un Target, arricchendolo con il suo tipo risolto e risolvendo i SymbolId.
    /// QUESTA VERSIONE È ARCHITETTURALMENTE CORRETTA: utilizza FindSymbolInScope() senza parametri
    /// e RegisterSymbol() per la creazione implicita, fidandosi dello scope corrente.
    /// Include la logica per risolvere i campi DBF come target di assegnazione (es. REPLACE)
    /// e la validazione dei campi dei tipi built-in tramite BuiltinTypeSchemas.
    /// 
    /// NOTA PER L'EXECUTOR:
    /// Quando un campo appartiene a un tipo built-in (es. HttpOptions.Method), non esiste un
    /// SymbolId corrispondente nella SymbolTable. In questi casi, il metodo imposta correttamente
    /// il 'ResolvedType' ma lascia 'FieldSymbolId' a -1.
    /// L'Executor DEVE gestire il caso FieldSymbolId == -1 eseguendo un accesso per nome (runtime binding/reflection)
    /// invece che un accesso indicizzato.
    /// </summary>
    private static Target RewriteTarget(Target target, AstRewriteContext context, BasicProgram program)
    {
        // In un metodo di classe, un VariableTarget il cui simbolo E' un CAMPO (scalare) della
        // classe corrente va riscritto come `SELF.<campo>`: il binder lo risolve già al simbolo
        // del campo, ma un VariableTarget nudo finisce in un LOCAL lato backend (bug: `v = 5`
        // non persisteva nel campo). Qui in testa, per QUALUNQUE SymbolId.
        if (target is VariableTarget vtPre && context.CurrentClass is { } ccPre && vtPre.SymbolId >= 0)
        {
            var (fmPreAny, _) = FindFieldInHierarchy(ccPre, vtPre.NameCleaned.Lexeme);
            if (fmPreAny is FieldMember fmPre && vtPre.SymbolId == fmPre.Declaration.SymbolId)
            {
                var selfTokPre = new Token(TokenKind.Self, "SELF", null, vtPre.Position);
                var selfIdPre = FindSymbolIdInScopeChain("SELF", vtPre.ScopeId, vtPre.Position, program);
                var selfTargetPre = new VariableTarget(selfIdPre, selfTokPre, selfTokPre)
                    { ResolvedType = new ClassType(ccPre) };
                return new FieldTarget(selfTargetPre, fmPre.Declaration.SymbolId, vtPre.NameCleaned)
                    { ResolvedType = fmPre.Declaration.DataType };
            }
        }
        // Dentro un metodo di classe, `arr(i) = v` su un nome NUDO che è un campo-array della
        // classe corrente → `SELF.arr(i) = v`. Il parser produce ArrayElementTarget (non conosce
        // i campi) e il Resolver può averlo risolto a un simbolo globale/dinamico scorrelato: la
        // riscrittura va fatta QUI, prima dello switch, per qualunque SymbolId.
        if (target is ArrayElementTarget aetM && context.CurrentClass is { } ccM)
        {
            var (bareM, _) = SplitNameAndSuffix(aetM.NameCleaned.Lexeme);
            var (memberM, _) = FindFieldInHierarchy(ccM, bareM);
            if (memberM is FieldMember fmM && !fmM.Declaration.Dimensions.IsEmpty)
            {
                var selfTokM = new Token(TokenKind.Self, "SELF", null, aetM.Position);
                // SELF risolto (come il ramo scalare sopra), non UnresolvedSymbolId: un SELF
                // non risolto faceva divergere il codegen IL per l'array-campo MULTI-DIM
                // (`m(i,j)` nudo → InvalidProgramException; `SELF.m(i,j)` esplicito andava).
                var selfIdM = FindSymbolIdInScopeChain("SELF", aetM.ScopeId, aetM.Position, program);
                var selfVarM = new VariableExpr(selfIdM, selfTokM, selfTokM)
                    { ScopeId = aetM.ScopeId };
                var fieldAccessM = new FieldAccessExpr(selfVarM, aetM.NameCleaned) { ScopeId = aetM.ScopeId };
                return RewriteTarget(new IndexedTarget(fieldAccessM, aetM.Indices) { ScopeId = aetM.ScopeId }, context, program);
            }
        }

        // Passaggio 1: Risolvi il SymbolId del target di base e applica le trasformazioni semantiche.
        Target idResolvedTarget;
        switch (target)
        {
            case VariableTarget { SymbolId: < 0 } vt:
            {
                var variableName = vt.NameCleaned.Lexeme;
                if (vt.NameCleaned.Kind is TokenKind.Self or TokenKind.Base ||
                    vt.NameCleaned.Lexeme.Equals("SELF", StringComparison.OrdinalIgnoreCase) ||
                    vt.NameCleaned.Lexeme.Equals("ME", StringComparison.OrdinalIgnoreCase) ||
                    vt.NameCleaned.Lexeme.Equals("BASE", StringComparison.OrdinalIgnoreCase))
                {
                    if (context.CurrentClass != null)
                    {
                        var selfSymbolId = vt.ScopeId >= 0
                            ? FindSymbolIdInScopeChain(vt.NameCleaned.Lexeme, vt.ScopeId, vt.Position, program)
                            : -1;
                        if (selfSymbolId < 0)
                        {
                            var allSym = program.Symbols.FindSymbolsByName(vt.NameCleaned.Lexeme)?.ToImmutableArray() ?? ImmutableArray<SymbolTable.Symbol>.Empty;
                            selfSymbolId = allSym.FirstOrDefault(s => s.DataType is ClassType)?.Id ?? -1;
                        }
                        return vt with { SymbolId = selfSymbolId, ResolvedType = new ClassType(context.CurrentClass) };
                    }

                    // SELF/ME in un metodo/costruttore di TYPE (UDT): il tipo è l'UDT corrente.
                    // Senza, SELF resta irrisolto (SymbolId -1) e il fallback generico
                    // (FindSymbolInScope, sotto) trova il SELF di una CLASSE precedente nel file:
                    // `SELF.<campo>` veniva risolto contro quella classe → bug "campo non trovato"
                    // in un TYPE preceduto da una CLASS (es. samples/Classes/ClassesDestructor.bas).
                    if (context.CurrentUdtScopeId.HasValue &&
                        program.Symbols.GetSymbol(context.CurrentUdtScopeId.Value).DeclarationNode is UserDefinedType udtSelfType)
                    {
                        var udtSelfId = FindSymbolIdInScopeChain(variableName, vt.ScopeId, vt.Position, program);
                        return vt with
                        {
                            SymbolId = udtSelfId >= 0 ? udtSelfId : vt.SymbolId,
                            ResolvedType = udtSelfType
                        };
                    }
                }

                if (context.CurrentUdtScopeId.HasValue)
                {
                    var udtSymbol = program.Symbols.GetSymbol(context.CurrentUdtScopeId.Value);
                    if (udtSymbol.DeclarationNode is UserDefinedType udt)
                    {
                        var field = udt.Fields.FirstOrDefault(f =>
                            f.Name.Lexeme.Equals(variableName, StringComparison.OrdinalIgnoreCase));
                        if (field != null)
                        {
                            var dbToken = new Token(TokenKind.Identifier, "_dbRecord", null, vt.Position);
                            var baseRecordTarget =
                                new VariableTarget(SymbolTable.DbfRecordBufferSymbolId, dbToken, dbToken);
                            idResolvedTarget = new FieldTarget(baseRecordTarget, field.SymbolId, vt.NameCleaned)
                                { ResolvedType = field.DataType };
                            goto SkipDefaultResolution;
                        }
                    }
                }

                var symbolIds = program.Symbols.FindSymbolInScope(variableName);
                // 2026-09-24 (overload): con omonimi, preferire la procedura contenitrice
                // (return-slot) — mai il primo per posizione. Vedi binder.
                var ownProcId = program.Symbols.PreferEnclosingProcedureSymbol(variableName, symbolIds ?? []);
                var resolvedId = ownProcId >= 0 ? ownProcId
                    : symbolIds is { Count: > 0 } ? symbolIds[0] : -1;
                if (resolvedId < 0)
                {
                    var allMatches = program.Symbols.FindSymbolsByName(variableName);
                    // Preferisci una vera DICHIARAZIONE a un simbolo la cui DeclarationNode è un
                    // VariableTarget (un target registrato come simbolo — es. il target di
                    // `++`/`--` in un metodo di classe): senza, `param--` risolveva al
                    // target-simbolo invece del parametro reale (IL lo trattava come locale).
                    var match = allMatches.FirstOrDefault(s => s.DeclarationNode is not VariableTarget)
                                ?? allMatches.FirstOrDefault(s => s.DeclarationNode is VariableTarget);
                    if (match != null)
                    {
                        resolvedId = match.Id;
                    }
                    else if (vt.ScopeId >= 0)
                    {
                        resolvedId = FindSymbolIdInScopeChain(variableName, vt.ScopeId, vt.Position, program);
                    }
                }

                // In un metodo di classe, un nome NUDO che è un CAMPO SCALARE della classe
                // corrente (e NON un parametro/locale che lo ombreggia) va indirizzato come
                // `SELF.<campo>`: un VariableTarget nudo veniva trattato dal backend come un
                // LOCAL (bug: `v = 5` non persisteva nel campo, `SELF.v` restava 0). Gli array
                // sono gestiti dall'altro ramo (ArrayElementTarget) e da RewriteArrayAccess.
                if (context.CurrentClass is { } ccBareT)
                {
                    var (fmBareT, _) = FindFieldInHierarchy(ccBareT, variableName);
                    if (fmBareT is FieldMember fieldMemBare)
                    {
                        // Ombreggiato se risolto a un simbolo DIVERSO dal campo (param/locale/globale).
                        var shadowedByLocal = resolvedId >= 0 && resolvedId != fieldMemBare.Declaration.SymbolId;
                        if (!shadowedByLocal)
                        {
                            var selfTokBare = new Token(TokenKind.Self, "SELF", null, vt.Position);
                            var selfIdBare = FindSymbolIdInScopeChain("SELF", vt.ScopeId, vt.Position, program);
                            var selfTargetBare = new VariableTarget(selfIdBare, selfTokBare, selfTokBare)
                                { ResolvedType = new ClassType(ccBareT) };
                            return new FieldTarget(selfTargetBare, fieldMemBare.Declaration.SymbolId, vt.NameCleaned)
                                { ResolvedType = fieldMemBare.Declaration.DataType };
                        }
                    }
                }

                if (resolvedId >= 0)
                {
                    idResolvedTarget = vt with { SymbolId = resolvedId };
                }
                else if (context.IsInClipperMode)
                {
                    idResolvedTarget = vt; // Late-binding DBF field target (REPLACE, etc.)
                }
                else if (program.Symbols.FindSymbolsByName(variableName)
                             .FirstOrDefault(s => s.DeclarationNode is ClassDefinition) is { } classNameSym)
                {
                    // "Forma" in `Forma._nextId = ...` — non è una variabile, è il nome di una
                    // CLASS usato per accedere a un membro STATIC. Segna il target col SymbolId
                    // della classe stessa (mai un target valido da solo): serve solo da marcatore
                    // per il ramo FieldTarget/ClassType più sotto, che lo riconosce e risolve
                    // l'intero accesso su un VariableTarget del campo STATIC condiviso.
                    idResolvedTarget = vt with { SymbolId = classNameSym.Id };
                }
                else
                {
                    // Inferisci il tipo dal suffisso ($=string, %=integer, &=long, !=single, #=double)
                    var lastChar = vt.NameSuffixed.Lexeme.Length > 0 ? vt.NameSuffixed.Lexeme[^1] : '\0';
                    var inferredType = (DataType)(lastChar switch
                    {
                        '$' => new StringType(),
                        '%' => new IntegerType(),
                        '&' => new LongType(),
                        '!' => new SingleType(),
                        '#' => new DoubleType(),
                        _ => null!
                    }) ?? new InferredType();
                    var registeredId = program.Symbols.RegisterSymbol(vt.NameCleaned, SymbolFlags.IsLocal,
                        inferredType, vt);
                    idResolvedTarget = vt with { SymbolId = registeredId };
                }

                break;
            }
            case ArrayElementTarget { SymbolId: < 0 } aet:
            {
                // Dentro un metodo di classe, un accesso indicizzato a un nome NUDO che è un
                // campo-array della classe corrente (es. `arr(i) = v`) va trattato come
                // `SELF.arr(i)`: il parser non conosce i campi e produce ArrayElementTarget.
                // Senza questa riscrittura il backend cerca un array inesistente ("accesso a un
                // array non DIMensionato" su IL; accesso a scalare su VM-C).
                // Current-scope lookup first (matches VariableTarget logic);
                // then global name scan; then scope-chain if node carries a valid ScopeId.
                int arrayId = -1;
                var localIds = program.Symbols.FindSymbolInScope(aet.NameCleaned.Lexeme);
                if (localIds is { Count: > 0 })
                {
                    // 2026-09-24 (overload): come sopra per VariableTarget.
                    var ownProcId = program.Symbols.PreferEnclosingProcedureSymbol(aet.NameCleaned.Lexeme, localIds);
                    arrayId = ownProcId >= 0 ? ownProcId : localIds[0];
                }
                else
                {
                    var allMatches = program.Symbols.FindSymbolsByName(aet.NameCleaned.Lexeme);
                    var match = allMatches.FirstOrDefault(s =>
                    {
                        if (s.DeclarationNode is VariableDeclaration vd)
                            return vd.DataType is ArrayType;
                        return s.DeclarationNode is ArrayElementTarget;
                    });
                    if (match != null)
                    {
                        arrayId = match.Id;
                    }
                    else if (aet.ScopeId >= 0)
                    {
                        arrayId = FindSymbolIdInScopeChain(aet.NameCleaned.Lexeme, aet.ScopeId, aet.Position, program);
                    }
                    else if (context.IsInClipperMode)
                    {
                        arrayId = -1; // late-binding
                    }
                    else
                    {
                        arrayId = program.Symbols.RegisterSymbol(aet.NameCleaned, SymbolFlags.IsLocal,
                            new InferredType(), aet);
                    }
                }
                idResolvedTarget = aet with { SymbolId = arrayId };
                break;
            }
            default:
                idResolvedTarget = target;
                break;
        }

        SkipDefaultResolution:

        // Post-resolution: per VariableTarget, controlla se è nel closureSymbolMap
        // (heap-promotion). Necessario per target con SymbolId già risolto (>= 0).
        if (idResolvedTarget is VariableTarget vt2 && context.SymbolMap != null && vt2.SymbolId >= 0)
        {
            Expr? mappedExpr2 = null;
            if (context.SymbolMap.TryGetValue(vt2.SymbolId, out var me2))
                mappedExpr2 = me2;
            else {
                foreach (var kv in context.SymbolMap)
                    if (kv.Value is FieldAccessExpr fae2 && fae2.Field.Lexeme.Equals(vt2.NameCleaned.Lexeme, StringComparison.OrdinalIgnoreCase))
                    { mappedExpr2 = kv.Value; break; }
            }
            if (mappedExpr2 is FieldAccessExpr fae3 && fae3.Structure is VariableExpr vx3)
                idResolvedTarget = new FieldTarget(
                    new VariableTarget(vx3.SymbolId, vx3.NameSuffixed, vx3.NameCleaned),
                    fae3.FieldSymbolId,
                    fae3.Field
                ) { ResolvedType = fae3.ResolvedType };
        }

        // Passaggio 2: Riscrivi ricorsivamente le sotto-espressioni e applica la trasformazione semantica.
        var rewrittenTarget = idResolvedTarget switch
        {
            ArrayElementTarget aet => RewriteArrayElementTarget(aet, context, program),
            FieldTarget ft when ft.Structure is null => ft, // WITH block implicit member
            FieldTarget ft => ft with { Structure = RewriteTarget(ft.Structure!, context, program) },
            IndexedTarget it => it with
            {
                Base = RewriteExpression(it.Base, context, program),
                Indices = [..it.Indices.Select(i => RewriteExpression(i, context, program))]
            },
            MacroTarget mt => mt with { Expression = (MacroExpr)RewriteExpression(mt.Expression, context, program) },
            _ => idResolvedTarget
        };

        // Passaggio 3: Risolvi i SymbolId dei campi e inferisci il tipo finale del target.
        DataType? finalType = null;
        if (rewrittenTarget is FieldTarget { FieldSymbolId: < 0, Structure: VariableTarget { SymbolId: >= 0 } classNameTarget } classQualifiedFieldTarget &&
            program.Symbols.GetSymbol(classNameTarget.SymbolId).DeclarationNode is ClassDefinition staticOwnerClass)
        {
            // `Forma._nextId = ...`: Structure e' il marcatore creato in Passaggio 1 (SymbolId
            // della CLASS, non di una variabile). Risolve l'assegnazione direttamente su un
            // VariableTarget del campo STATIC — stessa identità (SymbolId) usata per SELF.Campo,
            // così entrambi i percorsi di scrittura condividono la stessa storage (vedi
            // StaticFieldValues in Executor.cs / lo stesso meccanismo lato P-CODE).
            var (staticFoundMember, _) = FindFieldInHierarchy(staticOwnerClass, classQualifiedFieldTarget.OriginalToken.Lexeme);
            var (staticFieldId, staticFieldType) = staticFoundMember switch
            {
                FieldMember { Declaration.Flags: var flags } fm when flags.HasFlag(SymbolFlags.IsStatic)
                    => (fm.Declaration.SymbolId, fm.Declaration.DataType),
                PropertyMember { Property.Flags: var flags } pm when flags.HasFlag(SymbolFlags.IsStatic)
                    => (pm.Property.SymbolId, pm.Property.Type),
                _ => (-1, null)
            };
            if (staticFieldId < 0)
            {
                throw CreateError(classQualifiedFieldTarget.OriginalToken,
                    $"Nessun campo o proprietà STATIC '{classQualifiedFieldTarget.OriginalToken.Lexeme}' trovato nella classe '{staticOwnerClass.Name.Lexeme}'.");
            }

            return new VariableTarget(staticFieldId, classQualifiedFieldTarget.OriginalToken, classQualifiedFieldTarget.OriginalToken)
            {
                ResolvedType = staticFieldType
            };
        }

        if (rewrittenTarget is FieldTarget { FieldSymbolId: < 0 } fieldTarget)
        {
            var structureType = InferTargetType(fieldTarget.Structure!, context, program);
            var resolvedStructureType =
                ResolveDataType(structureType ?? new InferredType(), program, context.ModulePath);
            var fieldName = fieldTarget.OriginalToken.Lexeme;
            var hasSchema =
                TypeSchemaRegistry.TryGetValue(resolvedStructureType.GetType(), out var schema);

            if (resolvedStructureType is InterfaceType interfaceTypeResolving)
            {
                var member = interfaceTypeResolving.Definition.Members.FirstOrDefault(m =>
                    m.Name.Lexeme.Equals(fieldName, StringComparison.OrdinalIgnoreCase));
                if (member is not PropertyMember)
                {
                    throw CreateError(fieldTarget.OriginalToken,
                        $"Il membro '{fieldName}' non è una proprietà valida dell'interfaccia '{interfaceTypeResolving.Definition.Name.Lexeme}'.");
                }
            }

            if (hasSchema)
            {
                if (schema!.Fields.TryGetValue(fieldName, out var fieldType))
                {
                    finalType = fieldType;
                    rewrittenTarget = fieldTarget with { ResolvedType = finalType };
                    goto EndFieldResolution;
                }

                throw CreateError(fieldTarget.OriginalToken,
                    $"Campo '{fieldName}' non trovato nel tipo built-in '{resolvedStructureType}'.");
            }

            switch (resolvedStructureType)
            {
                case UserDefinedType udt:
                {
                    var fieldSymbol = udt.Fields.FirstOrDefault(f =>
                        f.Name.Lexeme.Equals(fieldName, StringComparison.OrdinalIgnoreCase));
                    var propertySymbol = udt.Properties.FirstOrDefault(p =>
                        p.Name.Lexeme.Equals(fieldName, StringComparison.OrdinalIgnoreCase));
                    if (fieldSymbol != null)
                    {
                        finalType = fieldSymbol.Dimensions.Any()
                            ? new ArrayType(fieldSymbol.DataType)
                            : fieldSymbol.DataType;
                        rewrittenTarget = fieldTarget with { FieldSymbolId = fieldSymbol.SymbolId };
                    }
                    else if (propertySymbol != null)
                    {
                        finalType = propertySymbol.Type;
                        rewrittenTarget = fieldTarget with { FieldSymbolId = propertySymbol.SymbolId };
                    }
                    else
                    {
                        throw CreateError(fieldTarget.OriginalToken,
                            $"Nessun campo o proprietà '{fieldName}' trovato nel tipo UDT '{udt.Name.Lexeme}'.");
                    }

                    break;
                }
                case InterfaceType interfaceType:
                {
                    var member = interfaceType.Definition.Members.FirstOrDefault(m =>
                        m is PropertyMember && m.Name.Lexeme.Equals(fieldName, StringComparison.OrdinalIgnoreCase));
                    if (member is not PropertyMember)
                    {
                        throw CreateError(fieldTarget.OriginalToken,
                            $"Il membro '{fieldName}' non è una proprietà assegnabile dell'interfaccia '{interfaceType.Definition.Name.Lexeme}'.");
                    }

                    break;
                }
                case ClassType classType:
                {
                    var memberName = fieldTarget.OriginalToken.Lexeme;
                    var (foundMember, _) = FindFieldInHierarchy(classType.Definition, memberName);
                    if (foundMember != null)
                    {
                        // Declaration.DataType di un campo di CLASSE è già il tipo pienamente
                        // risolto (RegisterFieldMember/SymbolTable lo aggiornano a un vero
                        // ArrayType per un campo con Dimensions non vuote) — non ri-wrapparlo
                        // qui, altrimenti produce un doppio ArrayType (stesso bug scoperto e
                        // corretto in LinkerInferExpressionType, vedi Linker.InferTypes.cs).
                        var (resolvedId, memberType) = foundMember switch
                        {
                            FieldMember fm => (fm.Declaration.SymbolId, fm.Declaration.DataType),
                            PropertyMember pm => (pm.Property.SymbolId, pm.Property.Type),
                            _ => (-1, null)
                        };
                        if (memberType != null)
                        {
                            finalType = memberType;
                            rewrittenTarget = fieldTarget with { FieldSymbolId = resolvedId };
                        }
                    }
                    else
                    {
                        throw CreateError(fieldTarget.OriginalToken,
                            $"Nessun campo o proprietà '{fieldName}' trovato nella classe '{classType.Definition.Name.Lexeme}'.");
                    }

                    break;
                }
            }
        }
        else
        {
            finalType = InferTargetType(rewrittenTarget, context, program);
        }

        EndFieldResolution:

        // Passaggio 4: Arricchisci il nodo con il tipo finale trovato.
        if (finalType is not null and not InferredType)
        {
            rewrittenTarget = rewrittenTarget switch
            {
                VariableTarget vt => vt with { ResolvedType = finalType },
                ArrayElementTarget aet => aet with { ResolvedType = finalType },
                IndexedTarget it => it with { ResolvedType = finalType },
                FieldTarget ft => ft with { ResolvedType = finalType },
                _ => rewrittenTarget
            };
        }

        return rewrittenTarget;
    }

    /// <summary>
    /// [NUOVO HELPER] Riscrive un ArrayElementTarget, gestendo la trasformazione semantica di `Array()`
    /// in un riferimento a un intero array.
    /// </summary>
    private static Target RewriteArrayElementTarget(ArrayElementTarget aet, AstRewriteContext context,
        BasicProgram program)
    {
        var arrayId = aet.SymbolId;
        if (arrayId < 0 && !context.IsInClipperMode)
        {
            throw CreateError(aet.NameSuffixed, $"Array non definito: '{aet.NameCleaned.Lexeme}'.",
                ErrorCode.UndefinedVariable);
        }

        // Se l'ID è ancora -1 (per Clipper), non possiamo fare molto, ma riscriviamo gli indici.
        if (arrayId < 0)
        {
            var unresolvedIndices = aet.Indices.Select(i => RewriteExpression(i, context, program)).ToImmutableArray();
            return aet with { Indices = unresolvedIndices };
        }

        var rewrittenIndices = aet.Indices.Select(i => RewriteExpression(i, context, program)).ToImmutableArray();

        // --- LA CORREZIONE CHIAVE È QUI ---
        // Se non ci sono indici, la sintassi `Nome()` è un riferimento all'intero array.
        // Lo trasformiamo nel nodo semanticamente corretto: un VariableTarget.
        if (rewrittenIndices.IsEmpty)
        {
            var symbol = program.Symbols.GetSymbol(arrayId);
            return new VariableTarget(arrayId, aet.NameSuffixed, aet.NameCleaned)
            {
                ResolvedType = symbol.DataType // Propaghiamo il tipo dell'array
            };
        }

        // Se ci sono indici, è un normale accesso a un elemento.
        return aet with { Indices = rewrittenIndices };
    }

    /// <summary>
    /// Riscrive un'istruzione LET. Inferisce il tipo di dato dall'espressione a destra
    /// per le dichiarazioni implicite, sincronizza il tipo con la SymbolTable, e
    /// valida la compatibilità dei tipi per le assegnazioni usando la logica di coercizione.
    /// Preserva tutta la logica esistente di inferenza e promozione.
    /// </summary>
    /// <param name="stmt">L'istruzione LetStmt originale.</param>
    /// <param name="context">Il contesto di riscrittura.</param>
    /// <param name="program">L'istanza del programma.</param>
    /// <returns>Il nodo LetStmt riscritto.</returns>
    private static LetStmt RewriteLetStmt(LetStmt stmt, AstRewriteContext context, BasicProgram program)
    {
        var newAssignments = ImmutableArray.CreateBuilder<Assignment>();
        const bool log = false;

        foreach (var assignment in stmt.Assignments)
        {
            // --- FASE 1: Riscrivi il Target e determina il tipo atteso ---
            var rewrittenTarget = RewriteTarget(assignment.Target, context, program);
            var targetType = InferTargetType(rewrittenTarget, context, program) ?? new InferredType();
            Log($"[REWRITE-LET] Target: '{rewrittenTarget}', Tipo Atteso: {targetType}", log);

            // --- FASE 1.5: LOGICA OPTION STRICT PER CREAZIONE IMPLICITA ---
            Log(
                $"[REWRITE-LET-STRICT] Controllo per '{rewrittenTarget}'. IsInStrictContext = {context.IsInStrictContext}",
                RewriteLog);
            if (context.IsInStrictContext)
            {
                var targetSymbolId = GetSymbolId(rewrittenTarget);
                // Se il simbolo non è stato risolto (non esiste), è un errore in modalità strict.
                if (targetSymbolId < 0 && rewrittenTarget is VariableTarget vt)
                {
                    Log(
                        $"[REWRITE-LET-STRICT] ERRORE: Creazione implicita della variabile '{vt.NameSuffixed.Lexeme}' non permessa in OPTION STRICT ON.",
                        true);
                    throw CreateError(vt.NameSuffixed,
                        $"OPTION STRICT ON: La variabile '{vt.NameSuffixed.Lexeme}' deve essere dichiarata " +
                        $"esplicitamente con DIM, LOCAL, GLOBAL, o STATIC prima di essere utilizzata.",
                        ErrorCode.UndefinedVariable);
                }
            }

            // --- FASE 1.75: MATRICE — M = {{...}} / M = [[...]] su array 2D dichiarato ---
            // Un letterale matrice annidato (grado 2) non è assegnabile direttamente a un array
            // multi-dimensionale: il linker lo appiattisce a grado 1 e il rank check fallisce
            // ("array con 1 dimensione ... ne richiede 2"). Espandiamo l'assegnazione in
            // assegnazioni elemento-per-elemento (ArrayElementTarget): riusa il percorso già
            // supportato da tutti e 5 i backend, senza toccare il codegen.
            var matrixSymbolId = GetSymbolId(rewrittenTarget);
            if (matrixSymbolId >= 0
                && program.Symbols.GetSymbol(matrixSymbolId).DeclarationNode is VariableDeclaration matrixDecl
                && matrixDecl.Dimensions.Length == 2
                && rewrittenTarget is VariableTarget matrixVt
                && TryGetRectangularMatrixLiteral(assignment.Value, out var matrixRows))
            {
                var elementType = (GetResolvedType(rewrittenTarget) as ArrayType)?.ElementType
                                  ?? (program.Symbols.GetSymbol(matrixSymbolId).DataType as ArrayType)?.ElementType
                                  ?? new DoubleType();
                var elementContext = context with { ExpectedType = elementType };
                var basePosition = assignment.Target.Position;
                for (var rowIndex = 0; rowIndex < matrixRows.Length; rowIndex++)
                {
                    var row = matrixRows[rowIndex];
                    for (var colIndex = 0; colIndex < row.Length; colIndex++)
                    {
                        var elementValue = RewriteExpression(row[colIndex], elementContext, program);
                        var rowToken = new Token(TokenKind.Number, rowIndex.ToString(), (double)rowIndex, basePosition);
                        var colToken = new Token(TokenKind.Number, colIndex.ToString(), (double)colIndex, basePosition);
                        var elementTarget = new ArrayElementTarget(matrixSymbolId, matrixVt.NameSuffixed,
                            matrixVt.NameCleaned,
                            [new NumberExpr(rowIndex, rowToken), new NumberExpr(colIndex, colToken)])
                        {
                            ResolvedType = elementType
                        };
                        newAssignments.Add(new Assignment(elementTarget, elementValue));
                    }
                }

                continue;
            }

            // --- FASE 2: Riscrivi il Valore con il Contesto Corretto ---
            var valueContext = context with { ExpectedType = targetType };
            var rewrittenValue = RewriteExpression(assignment.Value, valueContext, program);

            // Gestione speciale per i riferimenti a funzione dopo la riscrittura dell'ID.
            if (rewrittenValue is VariableExpr { SymbolId: >= 0 } funcRefExpr &&
                program.Symbols.GetSymbol(funcRefExpr.SymbolId).DeclarationNode is ProcedureDefinition)
            {
                rewrittenValue = new FunctionReferenceExpr(funcRefExpr.SymbolId, funcRefExpr.NameCleaned);
            }

            // --- FASE 3: Inferenza e Aggiornamento dei Tipi ---
            var inferredType = LinkerInferExpressionType(rewrittenValue, context, program);
            var finalTarget = rewrittenTarget;

            if (inferredType is not InferredType && GetResolvedType(rewrittenTarget) is InferredType or null)
            {
                context.OnTypeInferred?.Invoke();
            }

            if (inferredType is not InferredType)
            {
                var targetSymbolId = GetSymbolId(rewrittenTarget);

                if (targetSymbolId >= 0)
                {
                    var symbol = program.Symbols.GetSymbol(targetSymbolId);
                    Log(
                        $"[REWRITE-LET] Assegnazione a '{symbol.OriginalName}' (ID {targetSymbolId}). Tipo corrente: {symbol.DataType}, Tipo inferito valore: {inferredType}",
                        log);

                    /* Suffisso tipo sulla scrittura (`S$ = …` su una variabile numerica,
                     * `n% = …` su una stringa): il suffisso non fa parte del nome, quindi la
                     * scrittura sarebbe ambigua → errore esplicito, PRIMA che la promozione a
                     * variante (ANY) la renda silenziosa. */
                    switch (rewrittenTarget)
                    {
                        case VariableTarget vtSfx:
                            CheckSuffixWriteCompatibility(program, vtSfx.NameSuffixed, targetSymbolId);
                            break;
                        case ArrayElementTarget aetSfx:
                            CheckSuffixWriteCompatibility(program, aetSfx.NameSuffixed, targetSymbolId);
                            break;
                    }

                    if (symbol.Has(SymbolFlags.IsParameter | SymbolFlags.IsByRef) ||
                        symbol.Has(SymbolFlags.IsConst))
                    {
                        Log($"[REWRITE-LET] Skipping type inference for BYREF parameter/CONST '{symbol.OriginalName}'.",
                            log);
                    }
                    else
                    {
                        switch (symbol.DataType)
                        {
                            case InferredType when finalTarget is VariableTarget vt:
                                program.Symbols.UpdateSymbolType(targetSymbolId, inferredType);
                                finalTarget = vt with { ResolvedType = inferredType };
                                Log(
                                    $"[REWRITE-LET] Variabile '{symbol.OriginalName}' inferita come {inferredType}. SymbolTable aggiornata.",
                                    log);
                                break;
                            case ArrayType { ElementType: InferredType } arrayType when
                                finalTarget is ArrayElementTarget aet:
                            {
                                var newArrayType = arrayType with { ElementType = inferredType };
                                program.Symbols.UpdateSymbolType(targetSymbolId, newArrayType);
                                if (symbol.DeclarationNode is VariableDeclaration decl)
                                {
                                    UpdateDeclarationStatementInModule(program, decl.NameCleaned, newArrayType);
                                }

                                finalTarget = aet with { ResolvedType = inferredType };
                                Log(
                                    $"[REWRITE-LET] Tipo base inferito '{inferredType}' per l'array '{symbol.OriginalName}'.",
                                    log);
                                break;
                            }
                            default:
                            {
                                // Logica di promozione per variabili a tipizzazione dinamica/implicita.
                                // Se una variabile numerica (default Double) riceve una Lambda o un Array, la promuoviamo.
                                bool isPromotable =
                                    symbol.DataType is DoubleType or IntegerType or LongType or InferredType;
                                bool isComplex = inferredType is FunctionPointerType or ArrayType or ListType;

                                if (isPromotable && isComplex)
                                {
                                    program.Symbols.UpdateSymbolType(targetSymbolId, inferredType);
                                    // Aggiorna il flag nella SymbolTable: permette al Resolver di accettare la sintassi 'Var()' o 'Var(i)'
                                    program.Symbols.UpdateSymbolFlags(targetSymbolId, SymbolFlags.IsArray);

                                    finalTarget = rewrittenTarget switch
                                    {
                                        VariableTarget v => v with { ResolvedType = inferredType },
                                        _ => rewrittenTarget
                                    };

                                    Log(
                                        $"[REWRITE-LET] PROMOZIONE: '{symbol.OriginalName}' (ID {targetSymbolId}) -> {inferredType}.",
                                        log);
                                    context.OnTypeInferred?.Invoke();
                                    break;
                                }

                                // Variabile a tipo CONCRETO che riceve un valore di FAMIGLIA diversa
                                // (string <-> numero <-> bool): diventa una VARIANTE (AnyType).
                                // Caso reale: `Q = CHR(34)` (stringa) e poi `q = INSTR(...)` (numero)
                                // sono la STESSA variabile (BASIC case-insensitive) → restava StringType
                                // e l'assegnazione numerica era coercita a stringa → `string - long`
                                // → IL invalido (bug Compiler-InvalidOperandTypes-BinaryInst). Le
                                // famiglie NUMERICHE tra loro restano compatibili (widening/narrowing).
                                static int Family(DataType t) => t switch
                                {
                                    ByteType or SByteType or IntegerType or UIntegerType or LongType
                                        or ULongType or QuadType or UQuadType or SingleType or DoubleType
                                        or CurrencyType => 1,
                                    StringType => 2,
                                    BitType => 3,
                                    _ => 0,
                                };
                                int famCur = Family(symbol.DataType);
                                int famNew = Family(inferredType);
                                if (symbol.DataType is not InferredType && famCur != 0 && famNew != 0 && famCur != famNew)
                                {
                                    program.Symbols.UpdateSymbolType(targetSymbolId, new AnyType());
                                    finalTarget = rewrittenTarget switch
                                    {
                                        VariableTarget v => v with { ResolvedType = new AnyType() },
                                        _ => rewrittenTarget
                                    };
                                    Log($"[REWRITE-LET] VARIANTE: '{symbol.OriginalName}' -> ANY (famiglia {famCur} vs {famNew}).", log);
                                    context.OnTypeInferred?.Invoke();
                                    break;
                                }

                                if (GetResolvedType(rewrittenTarget) is null or InferredType)
                                {
                                    finalTarget = rewrittenTarget switch
                                    {
                                        VariableTarget v => v with { ResolvedType = inferredType },
                                        ArrayElementTarget aett => aett with { ResolvedType = inferredType },
                                        FieldTarget ft => ft with { ResolvedType = inferredType },
                                        IndexedTarget it => it with { ResolvedType = inferredType },
                                        _ => rewrittenTarget
                                    };
                                }

                                break;
                            }
                        }
                    }
                }
            }

            var finalTargetType = InferTargetType(finalTarget, context, program) ?? new InferredType();

            rewrittenValue = RewriteUserDefinedCTypeConversion(
                rewrittenValue, inferredType, finalTargetType, context, program);
            inferredType = LinkerInferExpressionType(rewrittenValue, context, program);

            // --- INIZIO BLOCCO DI CONTROLLO RANK ---
            if (finalTargetType is ArrayType && inferredType is ArrayType)
            {
                var targetRank = GetMatrixRankFromTarget(finalTarget, program);
                var sourceRank = GetMatrixRankFromExpression(rewrittenValue, program);

                // Controlliamo solo se entrambi i rank sono noti (> 0)
                if (targetRank > 0 && sourceRank > 0 && targetRank != sourceRank)
                {
                    throw CreateError(rewrittenValue,
                        $"Impossibile assegnare un array con {sourceRank} dimensioni a una variabile array che ne richiede {targetRank}.",
                        ErrorCode.TypeMismatch);
                }
            }

            // --- FASE 4: Validazione Finale (Type Checking) ---
            if (inferredType is not InferredType && finalTargetType is not InferredType)
            {
                if (!CanCoerce(inferredType, finalTargetType, context, program))
                {
                    var targetName = finalTarget switch
                    {
                        VariableTarget vt => vt.NameCleaned.Lexeme,
                        ArrayElementTarget aet => aet.NameCleaned.Lexeme,
                        FieldTarget ft => ft.OriginalToken.Lexeme,
                        _ => null
                    };
                    var named = string.IsNullOrEmpty(targetName) ? "" : $" nella variabile '{targetName}'";
                    throw CreateError(rewrittenValue,
                        $"Impossibile assegnare un valore di tipo '{inferredType}'{named} (tipo dichiarato '{finalTargetType}'). Tipizzare la variabile (es. AS JSON/AS STRING) o usare il suffisso '$'.",
                        ErrorCode.TypeMismatch);
                }
            }

            newAssignments.Add(new Assignment(finalTarget, rewrittenValue));
        }

        return new LetStmt(newAssignments.ToImmutable(), stmt.LetToken) { ScopeId = stmt.ScopeId };
    }

    /// <summary>
    /// Trasforma una coercizione definita dall'utente nella corrispondente chiamata esplicita a CTYPE.
    /// </summary>
    /// <param name="value">Espressione sorgente già riscritta.</param>
    /// <param name="sourceType">Tipo statico dell'espressione sorgente.</param>
    /// <param name="targetType">Tipo richiesto dalla destinazione.</param>
    /// <param name="context">Contesto di riscrittura e inferenza corrente.</param>
    /// <param name="program">Programma contenente simboli e dichiarazioni canoniche.</param>
    /// <returns>La chiamata CTYPE sintetica quando esiste un overload compatibile; altrimenti il valore originale.</returns>
    private static Expr RewriteUserDefinedCTypeConversion(Expr value, DataType sourceType, DataType targetType,
        AstRewriteContext context, BasicProgram program)
    {
        if (sourceType is InferredType || targetType is InferredType ||
            AreSameType(sourceType, targetType, context, program))
            return value;

        IEnumerable<OperatorDefinition> operators = sourceType switch
        {
            UserDefinedType udt => udt.Operators,
            ClassType classType => classType.Definition.Members.OfType<OperatorMember>().Select(member => member.Operator),
            _ => []
        };
        var conversion = operators.FirstOrDefault(op =>
            op.OperatorToken.Kind == TokenKind.CType &&
            op.Parameters.Length == 1 &&
            AreSameType(op.ReturnType, targetType, context, program));
        if (conversion is null) return value;

        var ctypeToken = new Token(TokenKind.CType, "CTYPE", null, value.Position);
        var namespaceToken = new Token(TokenKind.Identifier, "CONVERSIONS", null, value.Position);
        var targetName = new StringExpr(conversion.ReturnType.ToString() ?? targetType.ToString() ?? string.Empty,
            ctypeToken);
        return new BuiltinFunctionCallExpr(namespaceToken, ctypeToken, [value, targetName], conversion.ReturnType);
    }

    /// <summary>
    /// Riscrive un singolo parametro, risolvendo il suo tipo di dato e l'espressione del valore di default,
    /// e sincronizzando il tipo aggiornato con la SymbolTable.
    /// QUESTA VERSIONE È ARCHITETTURALMENTE CORRETTA: utilizza il SymbolId per aggiornamenti O(1).
    /// </summary>
    private static Parameter RewriteParameter(Parameter param, AstRewriteContext context, BasicProgram program)
    {
        // Se l'ID è negativo (-1, un placeholder) o se è fuori dal range della SymbolTable
        // a causa di un oggetto fittizio mal costruito (es. un UDT temporaneo), saltiamo l'accesso a GetSymbol.
        if (param.SymbolId < 0 || param.SymbolId >= program.Symbols.Count)
        {
            var rewrittenDefaultValueNeg = param.DefaultValue != null
                ? RewriteExpression(param.DefaultValue, context, program)
                : null;
            return param with
            {
                DataType = param.DataType,
                DefaultValue = rewrittenDefaultValueNeg
            };
        }

        var resolvedDataType = ResolveDataType(param.DataType, program, context.ModulePath);
        var rewrittenDefaultValue =
            param.DefaultValue != null ? RewriteExpression(param.DefaultValue, context, program) : null;

        // Sincronizza la SymbolTable se il tipo è stato risolto/modificato.
        if (resolvedDataType != param.DataType)
        {
            program.Symbols.UpdateSymbolType(param.SymbolId, resolvedDataType);
        }

        return param with
        {
            DataType = resolvedDataType,
            DefaultValue = rewrittenDefaultValue
        };
    }

    /// <summary>
    /// Riscrive un nodo EnumDefinition, processando le espressioni di valore dei suoi membri.
    /// Questa fase è cruciale per il constant folding all'interno delle definizioni di enumerazione.
    /// Sebbene i valori siano già stati calcolati in `PerformStaticAnalysisAndValidation`,
    /// questa passata garantisce che l'AST finale rifletta le espressioni risolte, se necessario,
    /// e che i tipi siano coerenti.
    /// </summary>
    /// <param name="enumDef">La definizione dell'enumerazione da riscrivere.</param>
    /// <param name="context">Il contesto di riscrittura corrente.</param>
    /// <param name="program">L'istanza del programma per l'accesso alla SymbolTable.</param>
    /// <returns>Una nuova istanza di `EnumDefinition` con i membri e i loro valori riscritti.</returns>
    private static EnumDefinition RewriteEnumDefinition(EnumDefinition enumDef, AstRewriteContext context,
        BasicProgram program)
    {
        var rewrittenMembers = enumDef.Members.Select(member =>
        {
            if (member.ValueExpr == null)
            {
                // Se non c'è un'espressione esplicita, non c'è nulla da riscrivere.
                return member;
            }

            // Riscrive l'espressione del valore. Questo risolve eventuali riferimenti
            // ad altre costanti o membri di enum.
            var rewrittenValueExpr = RewriteExpression(member.ValueExpr, context, program);

            // Per coerenza, ci assicuriamo che l'espressione riscritta sia ancora una costante.
            // Il risultato del constant folding dovrebbe essere un letterale.
            if (rewrittenValueExpr is not (NumberExpr or StringExpr or BooleanExpr))
            {
                // Questa condizione indica un errore se il constant folding non è riuscito
                // a ridurre l'espressione a un valore letterale.
                var errorToken = new Token(TokenKind.Unknown, rewrittenValueExpr.ToString(), null,
                    rewrittenValueExpr.Position);
                throw CreateError(errorToken,
                    $"L'espressione per il membro enum '{member.Name.Lexeme}' deve risolversi in una costante a tempo di compilazione.");
            }

            return member with { ValueExpr = rewrittenValueExpr };
        }).ToImmutableArray();

        // Restituisce la definizione dell'enum con i membri aggiornati.
        return enumDef with { Members = rewrittenMembers };
    }

    /// <summary>
    /// Riscrive una singola definizione di procedura.
    /// Implementa la stabilizzazione dei tipi e la trasformazione "Heap Promotion" per le closure.
    /// QUESTA VERSIONE È COMPLETA E CORRETTA: implementa la creazione della classe contesto e lo spostamento dei membri.
    /// 
    /// NOTA IMPORTANTE PER L'EXECUTOR:
    /// La trasformazione delle closure converte le funzioni locali in metodi di istanza di una classe contesto sintetica.
    /// Quando una funzione locale promossa viene restituita o assegnata a un delegato (es. 'RETURN ContatoreInterno'),
    /// l'AST risultante contiene un 'FunctionReferenceExpr' che punta al simbolo originale, che ora è marcato come 'IsMethod'.
    /// 
    /// L'EXECUTOR DEVE GESTIRE QUESTO CASO:
    /// 1. Quando valuta un 'FunctionReferenceExpr' che punta a un simbolo 'IsMethod':
    /// 2. Deve identificare l'istanza del contesto ('__context') nello scope corrente (che è garantita esistere grazie a questa riscrittura).
    /// 3. Deve creare un 'BoundDelegate' (o una struttura equivalente a runtime) che accoppia il puntatore al metodo con l'istanza del contesto.
    /// 4. Quando il delegato viene invocato successivamente, l'Executor deve usare l'istanza catturata come 'SELF' per la chiamata al metodo.
    /// 
    /// Senza questa gestione a runtime, la chiamata al metodo fallirà o userà un 'SELF' nullo/errato.
    /// </summary>
    private static ProcedureDefinition RewriteProcedure(ProcedureDefinition proc, AstRewriteContext parentContext,
        BasicProgram program)
    {
        const bool rewriteLog = false; // Log forzato per debug

        if (parentContext.IsInStrictContext)
        {
            // 1. Controlla il tipo di ritorno per le FUNCTION
            if (proc is FunctionDefinition { ReturnType: InferredType })
            {
                // La clausola AS è obbligatoria per le FUNCTION in modalità strict.
                // Il parser la garantisce già, ma questo è un controllo di sicurezza aggiuntivo.
                throw CreateError(proc.OriginalName,
                    $"OPTION STRICT ON: La funzione '{proc.OriginalName.Lexeme}' deve avere un tipo di ritorno " +
                    $"esplicito dichiarato con 'AS'.");
            }

            // 2. Controlla i parametri
            foreach (var param in proc.Parameters)
            {
                // --- INIZIO BLOCCO DI CORREZIONE ---
                // Un parametro è implicitamente tipizzato se il parser gli ha assegnato il tipo
                // di default (DoubleType), ma non c'era né una clausola 'AS' né un suffisso di tipo.
                if (param is { DataType: DoubleType, HasAsClause: false } &&
                    SplitNameAndSuffix(param.OriginalName.Lexeme).SuffixType == null)
                {
                    throw CreateError(param.OriginalName,
                        $"OPTION STRICT ON: Il parametro '{param.OriginalName.Lexeme}' della procedura " +
                        $"'{proc.OriginalName.Lexeme}' deve avere un tipo esplicito dichiarato con 'AS' o un suffisso.");
                }
                // --- FINE BLOCCO DI CORREZIONE ---
            }
        }

        if (proc.SymbolId < 0 || proc.SymbolId >= program.Symbols.Count)
        {
            throw new InvalidOperationException($"Internal Compiler Error: Invalid SymbolId {proc.SymbolId}.");
        }

        Log($"[REWRITE-PROC] >>> Inizio riscrittura per '{proc.OriginalName.Lexeme}' (ID: {proc.SymbolId})",
            rewriteLog);

        // Recupera l'ID dello scope reale della procedura dalla SymbolTable
        program.Symbols.EnterScope(proc.OriginalName.Lexeme, SymbolTable.ScopeType.Procedure, proc.SymbolId);
        var procedureBodyScopeId = program.Symbols.GetCurrentScope().Id;
        program.Symbols.ExitScope();

        // Crea un nuovo contesto con CurrentUdtScopeId per UDT methods.
        var procUdt = proc.Parent is UserDefinedType udtP ? udtP.SymbolId : (int?)null;
        var procedureContext = parentContext with { CurrentProcedure = proc, CurrentScopeId = procedureBodyScopeId, CurrentUdtScopeId = procUdt ?? parentContext.CurrentUdtScopeId };

        var modulePath = program.FileIdToPathMap.TryGetValue(proc.Position.FileId, out var mp) ? mp : string.Empty;
        var rewrittenParams =
            proc.Parameters.Select(p => RewriteParameter(p, parentContext, program)).ToImmutableArray();

        // Le procedure esterne (DECLARE) e native non hanno un corpo da analizzare
        // per closure o da riscrivere. Saltiamo direttamente alla fine.
        if (proc is ExternalProcedureDefinition or NativeProcedureDefinition)
        {
            Log($"[REWRITE-PROC]   -> Rilevata procedura senza corpo ({proc.GetType().Name}). Salto analisi del corpo.",
                rewriteLog);
            return proc switch
            {
                // Anche se saltiamo il corpo, dobbiamo comunque risolvere i tipi dei parametri e del ritorno.
                NativeProcedureDefinition npd => npd with
                {
                    Parameters = rewrittenParams, ReturnType = ResolveDataType(npd.ReturnType, program, modulePath)
                },
                ExternalProcedureDefinition epd => epd with
                {
                    Parameters = rewrittenParams,
                    ReturnType = epd.ReturnType != null ? ResolveDataType(epd.ReturnType, program, modulePath) : null
                },
                _ => proc // Non dovrebbe mai accadere
            };
        }

        // --- FASE A: RILEVAMENTO CATTURE ---
        var capturedSymbols = DetectCapturedSymbols(proc, modulePath, parentContext, program);

        // --- FASE B: HEAP PROMOTION (se necessario) ---
        var closureSymbolMap = new Dictionary<int, Expr>();
        var localProceduresToRemove = new HashSet<int>();
        VariableDeclaration? contextVarDecl = null;

        if (capturedSymbols.Count > 0)
        {
            // Entriamo temporaneamente nello scope della procedura per ottenere l'ID corretto
            program.Symbols.EnterScope(proc.OriginalName.Lexeme, SymbolTable.ScopeType.Procedure, proc.SymbolId);
            program.Symbols.ExitScope(); // Usciamo subito, lo useremo dopo nel loop

            (contextVarDecl, closureSymbolMap, localProceduresToRemove) =
                PerformHeapPromotion(proc, capturedSymbols, modulePath, program, procedureBodyScopeId, rewriteLog);
        }

        // --- FASE B.5: RISOLUZIONE TIPO DI RITORNO ANTICIPATA (Z1 — upcast nel return) ---
        // Un'assegnazione bare al return-slot dentro il corpo (`Json = NEW MvcJsonResult()`,
        // ovvero "FUNCTION = <istanza derivata>" per una FUNCTION che ritorna una classe base)
        // viene type-checked DURANTE il rewrite del corpo: RewriteLetStmt legge il DataType del
        // simbolo della procedura (InferTargetType → Symbols.GetSymbol().DataType) e lo confronta
        // col tipo dell'espressione via CanCoerce. Se il DataType resta UnresolvedQualifiedType
        // (era risolto SOLO a fine riscrittura, troppo tardi), il canCoerce(ClassType derivata,
        // UQT base) fallisce → "Impossibile assegnare un valore di tipo 'ClassType' a una
        // variabile di tipo 'X'" (Z1). Risolvendo il ReturnType QUI e propagandolo sia al simbolo
        // sia alla dichiarazione finale, il check vede il ClassType/InterfaceType risolto e
        // CanCoerce (che già cammina la catena ResolvedBaseClass, Linker.Rewrites.Utils.cs) passa.
        DataType? resolvedReturnType = null;
        DataType? resolvedYieldType = null;
        if (proc is FunctionDefinition fdRet && fdRet.ReturnType is not null)
        {
            resolvedReturnType = ResolveDataType(fdRet.ReturnType, program, modulePath);
            program.Symbols.UpdateSymbolType(proc.SymbolId, resolvedReturnType);
        }
        else if (proc is GeneratorDefinition gdRet && gdRet.YieldType is not null)
        {
            resolvedYieldType = ResolveDataType(gdRet.YieldType, program, modulePath);
        }

        // --- FASE C: RISCRITTURA DEL CORPO (Stabilizzazione) ---
        var (finalProcDef, _) = RewriteProcedureBodyLoop(
            proc, rewrittenParams, parentContext, modulePath, program,
            contextVarDecl, closureSymbolMap, localProceduresToRemove, rewriteLog
        );

        // Risincronizza i parametri con la SymbolTable (bug 2026-09-08, BackendIL-GPM
        // InvalidProgramException): `rewrittenParams` è stato costruito PRIMA di processare il
        // corpo (riga sopra), quindi non vede l'inferenza fatta durante il walk del corpo
        // (RewritePreResolvedSymbol in Linker.Rewrites.Variables.cs, es. un parametro
        // implicitamente double passato come argomento string a AI.AGENTRUN dentro il corpo —
        // aggiorna SOLO la SymbolTable, mai questo array). Il backend IL legge il tipo dei
        // parametri da QUESTO array (Parameters), non dalla SymbolTable — senza risincronizzare
        // qui l'inferenza fatta durante il rewrite non arriva mai al codegen. Confronto diretto
        // col tipo del simbolo (non un check "era InferredType": il parser assegna DoubleType,
        // non InferredType, ai parametri impliciti — vedi RewritePreResolvedSymbol) — un
        // disallineamento può nascere SOLO da quella stessa inferenza guardata, quindi adottare
        // sempre il tipo del simbolo quando diverge è sicuro. No-op nel caso comune (nessuna
        // inferenza scattata, simbolo e parametro concordano già).
        {
            var resyncedParams = finalProcDef.Parameters.Select(p =>
            {
                if (p.SymbolId < 0) return p;
                var sym = program.Symbols.GetSymbol(p.SymbolId);
                return sym.DataType.GetType() != p.DataType.GetType() ? p with { DataType = sym.DataType } : p;
            }).ToImmutableArray();
            finalProcDef = finalProcDef with { Parameters = resyncedParams };
        }

        Log("[REWRITE-PROC] <<< Fine riscrittura.", rewriteLog);

        ProcedureDefinition finalProcDefWithResolvedReturn = finalProcDef switch
        {
            // Usa il tipo già risolto in FASE B.5 quando disponibile (evita di ri-risolvere
            // e mantiene coerente la dichiarazione con ciò che il body-check ha visto).
            FunctionDefinition fd => fd with
            {
                ReturnType = resolvedReturnType ?? ResolveDataType(fd.ReturnType, program, modulePath)
            },
            GeneratorDefinition gd => gd with
            {
                YieldType = resolvedYieldType ?? ResolveDataType(gd.YieldType, program, modulePath)
            },
            // I casi Native ed External non vengono più raggiunti qui, ma li lasciamo per sicurezza.
            NativeProcedureDefinition npd => npd with
            {
                Parameters = rewrittenParams, ReturnType = ResolveDataType(npd.ReturnType, program, modulePath)
            },
            ExternalProcedureDefinition epd => epd with
            {
                Parameters = rewrittenParams,
                ReturnType = epd.ReturnType != null ? ResolveDataType(epd.ReturnType, program, modulePath) : null
            },
            _ => throw new InvalidOperationException($"Tipo non gestito: {proc.GetType().Name}")
        };

        program.Symbols.UpdateSymbolDeclaration(proc.SymbolId, finalProcDefWithResolvedReturn);

        return finalProcDefWithResolvedReturn;
    }

    /// <summary>
    /// [HELPER CORRETTO] Esegue un'analisi statica del corpo di una procedura per identificare e marcare
    /// le variabili catturate dalle closure (funzioni locali o lambda).
    /// Questa versione non esegue più una riscrittura "usa e getta", ma analizza direttamente la
    /// SymbolTable per prevenire falsi positivi.
    /// </summary>
    private static List<SymbolTable.Symbol> DetectCapturedSymbols(ProcedureDefinition proc, string modulePath,
        AstRewriteContext parentContext, BasicProgram program)
    {
        var capturedSymbols = new List<SymbolTable.Symbol>();
        var symbolsToUpdate = new HashSet<int>();

        // 1. Trova tutte le procedure e lambda definite all'interno di 'proc'.
        var nestedProcedures = proc.Body.OfType<LocalProcedureStmt>().Select(lps => lps.Procedure).ToList();
        var lambdaExpressions = FindAllNodesOfType<LambdaExpr>(proc.Body);

        // 2. Analizza le procedure locali.
        foreach (var nestedProc in nestedProcedures)
        {
            var usedSymbols = FindAllUsedVariables(nestedProc.Body, program);
            foreach (var usedSymbol in usedSymbols)
            {
                // Un simbolo è "catturato" se è usato nella funzione annidata MA
                // è stato definito in uno scope che è un antenato dello scope della funzione annidata.
                if (IsSymbolCaptured(usedSymbol, nestedProc.SymbolId, proc.SymbolId, program.Symbols))
                {
                    if (symbolsToUpdate.Add(usedSymbol.Id))
                        capturedSymbols.Add(usedSymbol);
                }
            }
        }

        // 3. Analizza le lambda expression.
        foreach (var lambda in lambdaExpressions)
        {
            var bodyNodes = (lambda.BlockBody ?? Enumerable.Empty<AstNode>())
                .Concat(lambda.ExpressionBody != null ? [lambda.ExpressionBody] : []);

            var usedSymbols = FindAllUsedVariables(bodyNodes, program);
            foreach (var usedSymbol in usedSymbols)
            {
                // Per le lambda, non abbiamo un SymbolId della lambda stessa, ma possiamo usare lo
                // ScopeId del suo corpo per determinare il contesto.

                // Gestisce correttamente il tipo nullable dell'array prima di tentare di accedere 
                // a un elemento tramite indice per evitare errori di compilazione.
                AstNode? firstBodyNode;
                if (lambda.BlockBody is { IsEmpty: false })
                {
                    firstBodyNode = lambda.BlockBody.Value[0];
                }
                else
                {
                    firstBodyNode = lambda.ExpressionBody;
                }

                var lambdaBodyScopeId = firstBodyNode?.ScopeId ?? -1;
                if (lambdaBodyScopeId != -1 && IsSymbolCaptured(usedSymbol, lambdaBodyScopeId, proc.SymbolId,
                        program.Symbols, isLambda: true))
                {
                    if (symbolsToUpdate.Add(usedSymbol.Id))
                        capturedSymbols.Add(usedSymbol);
                }
            }
        }

        // 4. Aggiorna i flag nella SymbolTable in un unico batch.
        foreach (var symbolId in symbolsToUpdate)
        {
            program.Symbols.UpdateSymbolFlags(symbolId, SymbolFlags.IsCaptured);
        }

        return capturedSymbols;
    }

    /// <summary>
    /// Helper ricorsivo per trovare tutti i nodi di un certo tipo in una sequenza di statement.
    /// CORRETTO E COMPLETO: Utilizza un visitatore AST basato su stack per attraversare
    /// in modo esaustivo tutti i nodi figli (sia statement che espressioni), garantendo
    /// di trovare i nodi target (es. LambdaExpr) indipendentemente dal loro livello di annidamento.
    /// </summary>
    private static IEnumerable<T> FindAllNodesOfType<T>(IEnumerable<AstNode> nodes) where T : AstNode
    {
        var nodesToVisit = new Stack<AstNode>(nodes);
        var visited = new HashSet<AstNode>();

        while (nodesToVisit.Count > 0)
        {
            var node = nodesToVisit.Pop();

            if (!visited.Add(node))
            {
                continue;
            }

            // Se il nodo corrente è del tipo che stiamo cercando, lo restituiamo.
            if (node is T foundNode)
            {
                yield return foundNode;
            }

            // --- Attraversamento Ricorsivo dei Nodi Figli ---
            var childrenToVisit = node switch
            {
                // Statements
                LetStmt n => n.Assignments.SelectMany(a => new AstNode[] { a.Target, a.Value }),
                ReturnStmt n => [n.Value],
                IfStmt n => n.IfClauses.SelectMany(c => new AstNode[] { c.Condition }.Concat(c.Statements))
                    .Concat(n.ElseClause?.Statements ?? Enumerable.Empty<AstNode>()),
                PrintStmt n => n.Arguments.Where(a => a.Expression != null).Select(a => a.Expression),
                ForStmt n => [n.Variable, n.Start, n.End, n.Step],
                ForEachStmt n => new AstNode[] { n.Collection }.Concat(n.Body),
                ExpressionStmt n => [n.Expression],
                AEvalStmt n => [n.Array, n.ActionFunction],
                ASortStmt n => [n.Array, n.CompareFunction, n.Order],
                ConditionalJumpStmt n => [n.Condition],

                // Expressions
                BinaryExpr n => [n.Left, n.Right],
                UnaryExpr n => [n.Right],
                GroupingExpr n => [n.Expression],
                FunctionExpr n => n.Args,
                MethodCallExpr n => new AstNode[] { n.Instance }.Concat(n.Args),
                BuiltinFunctionCallExpr n => n.Args,
                LambdaExpr n => (n.ExpressionBody != null
                        ? [n.ExpressionBody]
                        : Enumerable.Empty<AstNode>())
                    .Concat(n.BlockBody ?? Enumerable.Empty<AstNode>()),
                FieldAccessExpr n => [n.Structure],
                IndexedAccessExpr n => new AstNode[] { n.Base }.Concat(n.Indices),
                ArrayLiteralExpr n => n.Elements,
                ListLiteralExpr n => n.Elements,
                DictionaryLiteralExpr n => n.Entries.SelectMany(e => new AstNode[] { e.Key, e.Value }),
                ListComprehensionExpr n => new AstNode[] { n.OutputExpression }
                    .Concat(n.Clauses.SelectMany(c =>
                        c.ConditionExpression != null
                            ? new AstNode[] { c.SourceExpression, c.ConditionExpression }
                            : new AstNode[] { c.SourceExpression })),
                GeneratorExpr n => new AstNode[] { n.OutputExpression }
                    .Concat(n.Clauses.SelectMany(c =>
                        c.ConditionExpression != null
                            ? new AstNode[] { c.SourceExpression, c.ConditionExpression }
                            : new AstNode[] { c.SourceExpression })),
                AssignmentExpr n => [n.Target, n.Value],

                // Targets
                FieldTarget n => [n.Structure],
                ArrayElementTarget n => n.Indices,
                IndexedTarget n => new AstNode[] { n.Base }.Concat(n.Indices),

                // Altri nodi contenitore
                TryCatchStmt n => n.TryBody.Concat(n.CatchClauses.SelectMany(c =>
                        (c.WhenCondition != null ? [c.WhenCondition] : Enumerable.Empty<AstNode>())
                        .Concat(c.Body)))
                    .Concat(n.Finally?.Body ?? Enumerable.Empty<AstNode>()),
                SelectStmt n => new AstNode?[] { n.Expression }
                    .Concat(n.CaseClauses.SelectMany(c => c.Tests.Cast<AstNode>().Concat(c.Statements)))
                    .Concat(n.ElseClause?.Statements ?? Enumerable.Empty<AstNode>()),
                CaseRange n => [n.Start, n.End],
                CaseValue n => [n.Value],
                CaseComparison n => [n.Value],

                _ => []
            };

            // Aggiungiamo i figli allo stack solo se non sono null.
            foreach (var child in childrenToVisit.Reverse())
            {
                if (child != null)
                    nodesToVisit.Push(child);
            }
        }
    }

    /// <summary>
    /// Helper ricorsivo per trovare tutti i simboli di variabile usati in una sequenza di nodi AST.
    /// CORRETTO E COMPLETO: Attraversa ricorsivamente l'intero AST, gestendo tutti i tipi di nodi
    /// che possono contenere espressioni (Stmt, Expr, Target) per garantire che nessuna variabile
    /// venga mancata durante il rilevamento delle catture per le closure.
    /// </summary>
    private static IEnumerable<SymbolTable.Symbol> FindAllUsedVariables(IEnumerable<AstNode> nodes,
        BasicProgram program)
    {
        // Usiamo uno stack per evitare ricorsioni profonde che potrebbero causare StackOverflowException
        // su AST molto complessi. Il filtro .Where(n => n != null) è stato rimosso perché il flusso
        // successivo già gestisce i figli null.
        var nodesToVisit = new Stack<AstNode>(nodes);
        var visited = new HashSet<AstNode>();

        while (nodesToVisit.Count > 0)
        {
            var node = nodesToVisit.Pop();

            // Il controllo 'node == null' viene rimosso dal ciclo principale.
            // Il controllo !visited.Add(node) è sufficiente per prevenire cicli e ri-elaborazioni.
            if (!visited.Add(node))
            {
                continue;
            }

            // --- Logica di base: Trova e restituisci il simbolo per un VariableExpr ---
            if (node is VariableExpr ve)
            {
                var symbolId = ve.SymbolId;
                if (symbolId < 0)
                {
                    symbolId = FindSymbolIdInScopeChain(ve.NameCleaned.Lexeme, ve.ScopeId, ve.Position, program);
                }

                if (symbolId >= 0)
                {
                    yield return program.Symbols.GetSymbol(symbolId);
                }
            }

            // --- Attraversamento Ricorsivo dei Nodi Figli ---
            var childrenToVisit = node switch
            {
                // Statements
                LetStmt n => n.Assignments.SelectMany(a => new AstNode[] { a.Target, a.Value }),
                ReturnStmt n => [n.Value],
                IfStmt n => n.IfClauses.SelectMany(c => new AstNode[] { c.Condition }.Concat(c.Statements))
                    .Concat(n.ElseClause?.Statements ?? Enumerable.Empty<AstNode>()),
                PrintStmt n => n.Arguments.Where(a => a.Expression != null).Select(a => a.Expression),
                ForStmt n => [n.Variable, n.Start, n.End, n.Step],
                ForEachStmt n => new AstNode[] { n.Collection }.Concat(n.Body),
                ExpressionStmt n => [n.Expression],
                AEvalStmt n => [n.Array, n.ActionFunction],
                ASortStmt n => [n.Array, n.CompareFunction, n.Order],
                ConditionalJumpStmt n => [n.Condition],

                // Expressions
                BinaryExpr n => [n.Left, n.Right],
                UnaryExpr n => [n.Right],
                GroupingExpr n => [n.Expression],
                FunctionExpr n => n.Args,
                MethodCallExpr n => new AstNode[] { n.Instance }.Concat(n.Args),
                BuiltinFunctionCallExpr n => n.Args,
                LambdaExpr n => (n.ExpressionBody != null
                        ? [n.ExpressionBody]
                        : Enumerable.Empty<AstNode>())
                    .Concat(n.BlockBody ?? Enumerable.Empty<AstNode>()),
                FieldAccessExpr n => [n.Structure],
                IndexedAccessExpr n => new AstNode[] { n.Base }.Concat(n.Indices),
                ListComprehensionExpr n => new AstNode[] { n.OutputExpression }
                    .Concat(n.Clauses.SelectMany(c =>
                        c.ConditionExpression != null
                            ? new AstNode[] { c.SourceExpression, c.ConditionExpression }
                            : new AstNode[] { c.SourceExpression })),
                GeneratorExpr n => new AstNode[] { n.OutputExpression }
                    .Concat(n.Clauses.SelectMany(c =>
                        c.ConditionExpression != null
                            ? new AstNode[] { c.SourceExpression, c.ConditionExpression }
                            : new AstNode[] { c.SourceExpression })),
                AssignmentExpr n => [n.Target, n.Value],

                // Targets
                FieldTarget n => [n.Structure],
                ArrayElementTarget n => n.Indices,
                IndexedTarget n => new AstNode[] { n.Base }.Concat(n.Indices),

                // Altri nodi contenitore
                TryCatchStmt n => n.TryBody.Concat(n.CatchClauses.SelectMany(c =>
                        (c.WhenCondition != null ? [c.WhenCondition] : Enumerable.Empty<AstNode>())
                        .Concat(c.Body)))
                    .Concat(n.Finally?.Body ?? Enumerable.Empty<AstNode>()),
                SelectStmt n => new AstNode?[] { n.Expression }
                    .Concat(n.CaseClauses.SelectMany(c => c.Tests.Cast<AstNode>().Concat(c.Statements)))
                    .Concat(n.ElseClause?.Statements ?? Enumerable.Empty<AstNode>()),
                CaseRange n => [n.Start, n.End],
                CaseValue n => [n.Value],
                CaseComparison n => [n.Value],

                _ => []
            };

            // Aggiungiamo i figli allo stack solo se non sono null.
            foreach (var child in childrenToVisit.Reverse())
            {
                if (child != null)
                    nodesToVisit.Push(child);
            }
        }
    }

    /// <summary>
    /// Determina se un simbolo è "catturato", ovvero se è definito in uno scope esterno
    /// rispetto allo scope di utilizzo.
    /// </summary>
    private static bool IsSymbolCaptured(SymbolTable.Symbol symbol, int usageScopeOwnerId, int parentProcSymbolId,
        SymbolTable symbolTable, bool isLambda = false)
    {
        var symbolScope = symbolTable.GetScope(symbol.ScopeId);

        // Se il simbolo è globale o statico, non è una cattura.
        if (symbol.Has(SymbolFlags.IsGlobal) || symbol.Has(SymbolFlags.IsStaticStorage))
        {
            return false;
        }

        // Se il simbolo è un parametro o una variabile locale della funzione genitore, è una cattura.
        if (symbolScope.Type == SymbolTable.ScopeType.Procedure && symbolScope.OwnerSymbolId == parentProcSymbolId)
        {
            return true;
        }

        // Risaliamo per vedere se lo scope del simbolo è un antenato dello scope della funzione genitore
        // (per catturare variabili da blocchi IF/FOR esterni alla funzione locale).
        var parentProcScope = symbolTable.GetScope(symbolTable.GetSymbol(parentProcSymbolId).ScopeId);
        var current = parentProcScope;
        while (current.ParentId != -1)
        {
            if (current.Id == symbol.ScopeId)
            {
                return true;
            }

            current = symbolTable.GetScope(current.ParentId);
        }

        return false;
    }

    /// <summary>
    /// [HELPER] Esegue la trasformazione "Heap Promotion" creando la classe contesto e preparando le mappe di riscrittura.
    /// </summary>
    private static (VariableDeclaration?, Dictionary<int, Expr>, HashSet<int>) PerformHeapPromotion(
        ProcedureDefinition proc, List<SymbolTable.Symbol> capturedSymbols, string modulePath, BasicProgram program,
        int procedureBodyScopeId, bool log)
    {
        Log($"[REWRITE-PROC]   -> Rilevate {capturedSymbols.Count} variabili catturate. Generazione Classe Contesto.",
            log);

        var contextClassName = $"__ClosureCtx_{proc.Name.Lexeme}_{proc.SymbolId}";
        // Il rewrite puo' girare PIU' DI UNA VOLTA sullo stesso programma (ResolveAndRewriteModuleAsts fa 2 passate quando e' installato
        // SemanticQueryFolder: AI.SYMBOL/IMPACT/CALLMAP) e i simboli sintetici della prima passata restano nella SymbolTable: stesso nome
        // di classe contesto => "Un simbolo con nome '__ClosureCtx_…' e' gia' stato dichiarato" su QUALUNQUE procedura con closure che
        // cattura variabili (AEVAL con codeblock, ecc.). La seconda passata riparte dall'AST originale: serve solo un nome libero.
        {
            var baseCtxName = contextClassName;
            for (var pass = 2; program.Symbols.FindSymbolInScope(contextClassName) != null; pass++)
                contextClassName = $"{baseCtxName}_p{pass}";
        }
        var contextClassToken = new Token(TokenKind.Identifier, contextClassName, null, proc.Name.Position);
        var rootScopeId = program.Symbols.GetScope(0).Id;
        const SymbolFlags ctxClassFlags =
            SymbolFlags.IsClass | SymbolFlags.IsPublic |
            SymbolFlags.IsSynthetic;

        var preliminaryCtxClass = new ClassDefinition(-1, contextClassToken, ctxClassFlags, null, null,
            ImmutableArray<Token>.Empty, ImmutableArray<ClassMember>.Empty)
            { ScopeId = rootScopeId };

        var ctxClassSymbolId =
            program.Symbols.RegisterSymbol(contextClassToken, ctxClassFlags, new InferredType(), preliminaryCtxClass);
        program.Symbols.EnterScope(contextClassName, SymbolTable.ScopeType.Class, ctxClassSymbolId);

        var contextMembers = new List<ClassMember>();
        var fieldMapping = new Dictionary<int, int>();

        foreach (var sym in capturedSymbols)
        {
            // BYREF captured params: creiamo un campo NORMALE come per qualunque altra cattura
            // (piano Closure-ByRef-Box-Linker-Definitivo, Fase 2) — il campo verrà SEMPRE
            // inizializzato col BOX write-through già ricevuto dal parametro (MakeByRefExpr,
            // vedi il loop di seeding sui parametri poco sotto), MAI con una copia del valore.
            // OP_LOAD_MEMBER/OP_STORE_MEMBER (PCodeProcessor.cs e native gmb_vm_c) srotolano
            // il box in lettura/scrittura — il campo si comporta quindi in modo trasparente
            // come il parametro BYREF originale, sia nel genitore sia nella closure.
            var fieldToken = new Token(TokenKind.Identifier, sym.OriginalName, null, sym.OriginalToken.Position);
            var fieldType = ResolveDataType(sym.DataType, program, modulePath);
            var fieldFlags = SymbolFlags.IsField | SymbolFlags.IsPublic;
            var fieldId = program.Symbols.RegisterSymbol(fieldToken, fieldFlags, fieldType, fieldToken);

            var fieldDecl = new VariableDeclaration(fieldId, fieldToken, fieldToken, ImmutableArray<Expr>.Empty,
                fieldType, null,
                false, true, fieldFlags);
            program.Symbols.UpdateSymbolDeclaration(fieldId, fieldDecl);

            contextMembers.Add(new FieldMember(fieldDecl, AccessModifier.Public));
            fieldMapping[sym.Id] = fieldId;
        }

        // Se non c'è alcun campo (nessuna cattura) e nessuna local procedure, non serve heap
        // promotion (dopo la Fase 2, i BYREF catturati producono SEMPRE un campo — vedi sopra).
        var localProcs = proc.Body.OfType<LocalProcedureStmt>().Select(s => s.Procedure).ToList();
        if (fieldMapping.Count == 0 && localProcs.Count == 0)
        {
            program.Symbols.ExitScope();
            return (null, new Dictionary<int, Expr>(), new HashSet<int>());
        }

        // Se ci sono solo lambda (nessuna local procedure), non serve heap promotion.
        // Le variabili catturate rimangono come upvalues direttamente nel ClosureData.
        if (localProcs.Count == 0)
        {
            program.Symbols.ExitScope();
            return (null, new Dictionary<int, Expr>(), new HashSet<int>());
        }

        // Se nessun campo è stato creato, non serve heap promotion.
        if (fieldMapping.Count == 0)
        {
            program.Symbols.ExitScope();
            return (null, new Dictionary<int, Expr>(), new HashSet<int>());
        }

        var localProceduresToRemove = new HashSet<int>();

        // Determina il contesto strict dal modulo che contiene la procedura.
        var isStrict = program.LoadedModules.TryGetValue(modulePath, out var module) && module.IsStrict;

        // Pre-registra il simbolo __context per usarlo nel methodRewriteMap
        var ctxVarToken = new Token(TokenKind.Identifier, "__context", null, proc.Name.Position);
        var ctxVarId = program.Symbols.RegisterSymbol(ctxVarToken,
            SymbolFlags.IsLocal | SymbolFlags.IsSynthetic,
            new InferredType(), ctxVarToken);

        foreach (var localProc in localProcs)
        {
            Log($"[REWRITE-PROC]     -> Promozione funzione locale '{localProc.OriginalName.Lexeme}' a Metodo.", log);
            var methodRewriteMap = new Dictionary<int, Expr>();

            var ctxLocalExpr = new VariableExpr(ctxVarId, ctxVarToken, ctxVarToken);

            foreach (var (oldVarId, newFieldId) in fieldMapping)
            {
                var fieldSymbol = program.Symbols.GetSymbol(newFieldId);
                methodRewriteMap[oldVarId] = new FieldAccessExpr(ctxLocalExpr, fieldSymbol.OriginalToken)
                    { FieldSymbolId = newFieldId, ResolvedType = fieldSymbol.DataType };
            }

            // --- Risoluzione Scope ID per la procedura locale ---
            // Entriamo temporaneamente nello scope della funzione locale per ottenerne l'ID reale.
            program.Symbols.EnterScope(localProc.OriginalName.Lexeme, SymbolTable.ScopeType.Procedure,
                localProc.SymbolId);
            var localProcScopeId = program.Symbols.GetCurrentScope().Id;
            program.Symbols.ExitScope();

            // Crea il contesto per il metodo promovendo lo scope interno come sesto parametro.
            var methodContext = new AstRewriteContext(modulePath, localProc, new DbfContext(), false,
                isStrict, localProcScopeId, null, null, null, methodRewriteMap);
            var rewrittenMethodBody = localProc.Body
                .Select(stmt => RewriteStatement(stmt, methodContext, localProc, program).Rewritten).ToList();

            var methodDef = localProc with
            {
                Body = [..rewrittenMethodBody], Flags = localProc.Flags | SymbolFlags.IsMethod
            };
            contextMembers.Add(new MethodMember(methodDef, AccessModifier.Public));
            localProceduresToRemove.Add(localProc.SymbolId);
            // Aggiorna la SymbolTable: le chiamate a Moltiplica(5) si risolvono
            // tramite ResolvedDefinition. Senza questo, usano la vecchia definizione
            // col corpo NON riscritto (VariableExpr per variabili catturate).
            program.Symbols.UpdateSymbolDeclaration(localProc.SymbolId, methodDef);
        }

        program.Symbols.ExitScope();

        var finalCtxClass =
            new ClassDefinition(ctxClassSymbolId, contextClassToken, ctxClassFlags, null, null,
                    ImmutableArray<Token>.Empty, [..contextMembers])
                { ScopeId = rootScopeId };
        program.Symbols.UpdateSymbolDeclaration(ctxClassSymbolId, finalCtxClass);

        var ctxType = new ClassType(finalCtxClass);
        var newExpr = new NewExpr(ctxType, null, null, new Token(TokenKind.New, "NEW", null, proc.Name.Position));

        // ctxVarId e' stato gia' registrato prima del loop localProc per
        // essere disponibile in methodRewriteMap. Aggiorniamo tipo e dichiarazione.
        program.Symbols.UpdateSymbolType(ctxVarId, ctxType);
        var contextVarDecl = new VariableDeclaration(ctxVarId, ctxVarToken, ctxVarToken, ImmutableArray<Expr>.Empty,
            ctxType,
            newExpr, false, false, SymbolFlags.IsLocal) { ScopeId = procedureBodyScopeId };
        program.Symbols.UpdateSymbolDeclaration(ctxVarId, contextVarDecl);

        var closureSymbolMap = new Dictionary<int, Expr>();
        var contextVarExpr = new VariableExpr(ctxVarId, ctxVarToken, ctxVarToken);
        foreach (var (oldVarId, fieldId) in fieldMapping)
        {
            var fieldSym = program.Symbols.GetSymbol(fieldId);
            closureSymbolMap[oldVarId] = new FieldAccessExpr(contextVarExpr, fieldSym.OriginalToken)
                { FieldSymbolId = fieldId, ResolvedType = fieldSym.DataType };
        }

        return (contextVarDecl, closureSymbolMap, localProceduresToRemove);
    }

    /// <summary>
    /// [HELPER] Esegue il loop di stabilizzazione per la riscrittura del corpo di una procedura.
    /// CORRETTO: Timbra correttamente i nodi sintetici (es. __context) con lo ScopeId
    /// della procedura in cui vengono iniettati.
    /// </summary>
    private static (ProcedureDefinition, AstRewriteContext) RewriteProcedureBodyLoop(
        ProcedureDefinition proc, ImmutableArray<Parameter> rewrittenParams, AstRewriteContext parentContext,
        string modulePath, BasicProgram program,
        VariableDeclaration? contextVarDecl, Dictionary<int, Expr> closureSymbolMap,
        HashSet<int> localProceduresToRemove, bool log)
    {
        var currentBody = proc.Body;
        var currentProcDef = proc;
        var procedureDbfContext = parentContext.DbfState;

        const int maxIterations = 10;
        var iteration = 0;
        bool typesChangedInPass;

        // Recupera l'ID dello scope dal contesto genitore (impostato in RewriteProcedure).
        // Fallback di sicurezza sull'ID del simbolo se per qualche motivo fosse null.
        var procedureBodyScopeId = parentContext.CurrentScopeId ?? program.Symbols.GetSymbol(proc.SymbolId).ScopeId;

        do
        {
            iteration++;
            Log($"[REWRITE-PROC]     --- Inizio Passata #{iteration} ---", log);
            if (iteration > maxIterations) break;

            typesChangedInPass = false;

            // FIX: Propaga CurrentClass dal parentContext per garantire la risoluzione dei membri ereditati.
            var passContext = new AstRewriteContext(modulePath, currentProcDef, procedureDbfContext,
                parentContext.IsInClipperMode, parentContext.IsInStrictContext,
                procedureBodyScopeId, // Trasporta lo scope ID attivo per la risoluzione
                null, () => typesChangedInPass = true, null, closureSymbolMap, null, parentContext.CurrentClass);

            var bodyBuilder = ImmutableArray.CreateBuilder<Stmt>();
            var currentPassContext = passContext;

            // Lo ScopeId corretto è quello della procedura stessa, disponibile nel contesto.
            // Tutti i nodi iniettati devono appartenere a questo scope.
            if (currentProcDef.Body.Any())
            {
                // Otteniamo lo scope dal primo statement del corpo originale,
                // che è garantito essere stato timbrato dal Binder.
                procedureBodyScopeId = currentProcDef.Body[0].ScopeId;
            }
            else if (program.Symbols.GetSymbol(currentProcDef.SymbolId).DeclarationNode is ProcedureDefinition procNode)
            {
                // Fallback nel caso il corpo sia vuoto.
                procedureBodyScopeId = procNode.ScopeId;
            }

            if (iteration == 1 && contextVarDecl != null)
            {
                // Timbra il DeclarationStmt sintetico.
                bodyBuilder.Add(new DeclarationStmt(new Token(TokenKind.Dim, "DIM", null, SourcePosition.Unknown),
                    [contextVarDecl]) { ScopeId = procedureBodyScopeId });

                foreach (var param in proc.Parameters)
                {
                    if (closureSymbolMap.TryGetValue(param.SymbolId, out var fieldExpr))
                    {
                        // BYREF (piano Closure-ByRef-Box-Linker-Definitivo, Fase 2/3): il campo
                        // deve ricevere il BOX write-through già posseduto dal parametro (stessa
                        // identità del box passato dal chiamante via OP_MAKE_BYREF al call-site),
                        // MAI il suo valore corrente — altrimenti il campo sarebbe un semplice
                        // numero e le scritture successive (genitore E closure) non
                        // raggiungerebbero mai la variabile originale del chiamante.
                        var rewrittenParamValue = param.Flags.HasFlag(SymbolFlags.IsByRef)
                            ? new MakeByRefExpr(param.SymbolId, param.OriginalName)
                            : RewriteExpression(new VariableExpr(param.SymbolId, param.OriginalName, param.Name),
                                currentPassContext with { SymbolMap = null },
                                program);
                        // Timbra il LetStmt sintetico.
                        bodyBuilder.Add(
                            new LetStmt([new Assignment(ExpressionToTarget(fieldExpr), rewrittenParamValue)], null)
                                { ScopeId = procedureBodyScopeId });
                    }
                }
            }

            foreach (var stmt in currentBody)
            {
                if (stmt is LocalProcedureStmt lps &&
                    localProceduresToRemove.Contains(lps.Procedure.SymbolId)) continue;

                if (stmt is DeclarationStmt declStmt && closureSymbolMap.Count > 0)
                {
                    var keptDecls = new List<VariableDeclaration>();
                    foreach (var decl in declStmt.Declarations)
                    {
                        if (closureSymbolMap.TryGetValue(decl.SymbolId, out var targetFieldExpr))
                        {
                            if (decl.InitialValue != null)
                            {
                                var rewrittenVal = RewriteExpression(decl.InitialValue, currentPassContext, program);
                                // Timbra il LetStmt sintetico.
                                bodyBuilder.Add(new LetStmt(
                                        [new Assignment(ExpressionToTarget(targetFieldExpr), rewrittenVal)], null)
                                    { ScopeId = procedureBodyScopeId });
                            }
                        }
                        else
                        {
                            keptDecls.Add(decl);
                        }
                    }

                    if (keptDecls.Count > 0)
                    {
                        (var rwStmt, currentPassContext) = RewriteStatement(
                            declStmt with { Declarations = [..keptDecls] }, currentPassContext, currentProcDef,
                            program);
                        bodyBuilder.Add(rwStmt);
                    }

                    continue;
                }

                (var rewrittenStmt, currentPassContext) =
                    RewriteStatement(stmt, currentPassContext, currentProcDef, program);
                bodyBuilder.Add(rewrittenStmt);
            }

            procedureDbfContext = currentPassContext.DbfState;
            currentBody = bodyBuilder.ToImmutable();
            currentProcDef = currentProcDef switch
            {
                FunctionDefinition fd => fd with { Body = currentBody, Parameters = rewrittenParams },
                GeneratorDefinition gd => gd with { Body = currentBody, Parameters = rewrittenParams },
                _ => throw new InvalidOperationException("Tipo di procedura non gestito.")
            };
        } while (typesChangedInPass);

        return (currentProcDef,
            new AstRewriteContext(modulePath, currentProcDef, procedureDbfContext, parentContext.IsInClipperMode,
                parentContext.IsInStrictContext, procedureBodyScopeId));
    }

    /// <summary>
    /// Riscrive un nodo BuiltinFunctionCallExpr, gestendo gli overload, l'inferenza contestuale per le lambda
    /// e specializzando i tipi di ritorno polimorfici.
    /// CORREZIONE 3: Inietta il tipo atteso per i parametri lambda delle funzioni built-in note (AEVAL, AFILTER, ecc.)
    /// specializzando la definizione della procedura PRIMA di chiamare RewriteArgumentsWithContext.
    /// </summary>
    private static void CheckNarrowingOverflow(BuiltinFunctionCallExpr bce, NativeProcedureDefinition? proc)
    {
        if (proc == null) return;
        var (minVal, maxVal) = proc.ReturnType switch
        {
            ByteType => (0.0, 255.0),
            SByteType => (-128.0, 127.0),
            IntegerType => (-2147483648.0, 2147483647.0),
            UIntegerType => (0.0, 4294967295.0),
            LongType => (-9223372036854775808.0, 9223372036854775807.0),
            ULongType => (0.0, 1.8446744073709552E+19),
            _ => (double.MinValue, double.MaxValue)
        };
        if (minVal == double.MinValue) return;

        foreach (var arg in bce.Args)
        {
            if (arg is NumberExpr n && (n.Value < minVal || n.Value > maxVal))
                throw new BasicException(ErrorCode.Overflow, n.Position,
                    "Overflow: il valore " + n.Value + " non rientra nell'intervallo del tipo di destinazione [" + minVal + ", " + maxVal + "].");
        }
    }

    private static Expr RewriteBuiltinFunctionCall(
        BuiltinFunctionCallExpr bce,
        AstRewriteContext context,
        BasicProgram program)
    {
        // §4.10(f) — AI.SYMBOL/IMPACT/CALLMAP: fold a link-time (grafo del programma, hook iniettato).
        if (bce.Name.Kind is TokenKind.AiSymbol or TokenKind.AiImpact or TokenKind.AiCallmap
            or TokenKind.AiRevision or TokenKind.AiContext or TokenKind.AiCheckrev or TokenKind.AiPatch or TokenKind.AiFindsymbol or TokenKind.AiRepomap
            or TokenKind.AiFileoutline
            or TokenKind.LlmVerify or TokenKind.LlmCommit or TokenKind.LlmInspect)
        {
            if (SemanticQueryFolder is null)
            {
                try { System.Reflection.Assembly.Load("SemanticGraph"); } catch { /* assente: nessun fold */ }
            }
            if (SemanticQueryFolder is null) goto skipSemFold;
            var nm = bce.Args.Length > 0 && bce.Args[0] is StringExpr se ? se.Value
                     : (bce.Name.Kind == TokenKind.AiRevision || bce.Name.Kind == TokenKind.AiRepomap
                        || bce.Name.Kind == TokenKind.LlmVerify || bce.Name.Kind == TokenKind.LlmCommit) ? "" : null;
            if (nm is not null)
            {
                var depth = bce.Args.Length > 1 && bce.Args[1] is NumberExpr ne ? (int)ne.Value
                            : bce.Name.Kind == TokenKind.AiContext ? 1 : 32;
                var res = SemanticQueryFolder(program, bce.Name.Kind, nm, depth);
                if (res is not null) return new StringExpr(res, bce.Name);
            }
            skipSemFold: ;
        }
        // 1. Recupera tutti i candidati per il BuiltinKind specificato nel namespace di riferimento.
        //    NOTA: un BuiltinKind Identifier NON è un catch-all: i moduli IL esterni
        //    (IlModuleLoader) registrano tutti i metodi con BuiltinKind=Identifier (il nome di
        //    un metodo C# arbitrario non ha una keyword), quindi il match deve essere per NOME
        //    (OriginalName) o per una keyword reale — mai per Identifier generico, che renderebbe
        //    ogni metodo del modulo candidato per qualunque chiamata → overload resolution
        //    sceglieva il primo per arità (es. Greet("bob") → Double).
        // 2026-10-30 — i verbi-famiglia LLM (TENSOR/POLICY/ARCH/MODULE/KERNEL/KV) sono COMANDI con
        // una versione-FUNZIONE omonima (`LLM.TENSOR(...)`): il match per NOME farebbe risolvere il
        // COMANDO (`LLM TENSOR …`, che deve stampare) sulla funzione omonima, che restituisce. Per
        // questi Kind si risolve SOLO per BuiltinKind: la funzione ha un Kind dedicato (*Out) e ci
        // arriva; il comando non trova procedure con il suo Kind → builtin (stampa).
        static bool IsLlmFamilyVerbKind(TokenKind k) => k is TokenKind.LlmTensor or TokenKind.LlmPolicy
            or TokenKind.LlmArch or TokenKind.LlmModule or TokenKind.LlmKernel or TokenKind.LlmKv;
        List<NativeProcedureDefinition> candidates = program.GetProceduresForNamespace(bce.Namespace.Lexeme)
            .OfType<NativeProcedureDefinition>()
            // IDENTITÀ PER TOKENKIND (AGENTS §12, regola d'oro): il nodo di chiamata porta
            // SEMPRE il Kind REGISTRATO della procedura (il parser usa DedicatedBuiltinToken),
            // quindi rispondono solo le definizioni con lo stesso `BuiltinKind`. Il NOME resta
            // necessario in un solo caso: nodo con Kind `Identifier` (moduli IL esterni, che
            // registrano tutti i metodi con BuiltinKind=Identifier, e i fallback del parser
            // senza Kind canonico). Prima la clausola per nome era SEMPRE attiva e poteva
            // selezionare una funzione diversa con lo stesso nome (es. comando `AI SET ACTIVE`
            // — kind AiSetActive — che trovava la funzione `PROVIDER.SETACTIVE`, kind
            // ProviderSetActive): il dispatch non deve mai partire dal lexeme.
            .Where(p => (p.BuiltinKind == bce.Name.Kind && p.BuiltinKind != TokenKind.Identifier) ||
                        (bce.Name.Kind == TokenKind.Identifier &&
                         !IsLlmFamilyVerbKind(bce.Name.Kind) &&
                         p.OriginalName.Lexeme.Equals(bce.Name.Lexeme, StringComparison.OrdinalIgnoreCase)))
            .ToList();

        // Bypass: if no candidates found, this is a simple VM-level builtin (no overload resolution needed)
        // 2026-09-24 (overload vs builtin): anche quando i nativi esistono ma NESSUNO è
        // compatibile per arità, la chiamata può riferirsi a un overload UTENTE omonimo
        // (es. MATH.SQR(x,y) con solo SQR/1 nativo) — stesso fallback utente qui sotto.
        if (candidates.Count == 0 ||
            !candidates.Any(c => SymbolTable.IsProcedureSignatureCompatible(c, bce.Args.Length)))
        {
            // DECLARE FUNCTION ... LIB (FFI) o UDF importata/globale con nome che collide con
            // una keyword (es. MULTIPLY da gm_cpp_math o da MathUtils.inc): se esiste una
            // ProcedureDefinition reale col nome, è una chiamata a PROCEDURA, NON un builtin —
            // risolvila come FunctionExpr (il compiler la emette via _ffiFunctions o come
            // chiamata UDF). Prima il default dispatch della VM la "gestiva" col fallback
            // numerico (magic-number) che collideva coi TokenKind correnti — fix 2026-08-11
            // (Structural-Bug-Fix-Builtin-Dispatch, allineato GMBasic).
            // 2026-09-24: cerca PRIMA nello scope del namespace qualificato (overload utente
            // dentro un namespace builtin, es. MATH.SQR/2), poi visibilità generale.
            var procCandidate = program.GetProceduresForNamespace(bce.Namespace.Lexeme)
                .Where(p => p is not NativeProcedureDefinition &&
                    p.OriginalName.Lexeme.Equals(bce.Name.Lexeme, StringComparison.OrdinalIgnoreCase) &&
                    SymbolTable.IsProcedureSignatureCompatible(p, bce.Args.Length))
                .FirstOrDefault()
                ?? FindProcedureCandidatesInScope(bce.Name.Lexeme, context, program)
                    .FirstOrDefault(p => p is not NativeProcedureDefinition &&
                        SymbolTable.IsProcedureSignatureCompatible(p, bce.Args.Length));
            if (procCandidate != null)
            {
                var rewrittenProcArgs = ImmutableArray.CreateBuilder<Expr>(bce.Args.Length);
                foreach (var arg in bce.Args)
                    rewrittenProcArgs.Add(arg is null ? arg! : RewriteExpression(arg, context, program));
                var procExpr = new FunctionExpr(procCandidate.SymbolId, bce.Name, rewrittenProcArgs.ToImmutable(),
                    procCandidate is FunctionDefinition fd ? fd.ReturnType : new VoidType()) { ScopeId = bce.ScopeId };
                return RewriteFunctionExpr(procExpr, context, program);
            }
            // 2026-09-24: se esistevano candidati nativi (incompatibili) e nessun overload
            // utente matcha, NON si prende il bypass silenzioso qui sotto — si prosegue
            // all'overload loop che emette l'errore forte "alcun overload" (comportamento
            // precedente a questo fix).
            if (candidates.Count == 0)
            {
                // Anche in bypass gli argomenti vanno riscritti (es. costanti %NOME → letterali via
                // constant folding): senza questo passaggio restano VariableExpr con SymbolId non
                // risolto, e il Compiler li compila come load da una variabile inesistente → null a
                // runtime. Colpiva le funzioni built-in "semplici" senza overload (es. CONFIRM/ALERT).
                if (bce.Args.IsEmpty)
                    return bce;

                var rewrittenBypassArgs = ImmutableArray.CreateBuilder<Expr>(bce.Args.Length);
                // Gli argomenti NON ereditano l'ExpectedType del call-site (che è il tipo di
                // RITORNO): un `DIM fp AS LONG = LINALG.FRACTIONALPOWER(h, 0.5)` propagava
                // LONG a 0.5 → CLng(0.5)=0 → potenza 0 invece di 0.5. Senza formal types
                // (builtin non materializzato nei symbols, es. namespace non importato),
                // l'inferenza naturale del letterale è l'unica scelta corretta.
                // Lambda/Function: conserva l'ExpectedType del call-site (target typing dei
                // parametri). Gli altri argomenti: niente ExpectedType, che è il tipo di
                // RITORNO — `DIM fp AS LONG = LINALG.FRACTIONALPOWER(h, 0.5)` propagava
                // LONG a 0.5 → CLng(0.5)=0 → potenza 0 invece di 0.5.
                foreach (var arg in bce.Args)
                {
                    var argCtx = arg is LambdaExpr or FunctionExpr
                        ? context
                        : context with { ExpectedType = null };
                    rewrittenBypassArgs.Add(arg is null ? arg! : RewriteExpression(arg, argCtx, program));
                }
                return bce with { Args = rewrittenBypassArgs.ToImmutable() };
            }
        }

        // Bypass per i metodi COLLEZIONE mappati dal ramo commonTk (Count/Keys/ToArray/
        // ToJson/UniqueCount su un'istanza collezione): il primo argomento è la collezione
        // tipizzata e l'overload resolution GLOBALE (che cerca firme tipo SYSTEM.COUNT per
        // string/ICollection) fallisce in scope SUB ('overload Count' — il tipo collezione
        // non è riconosciuto come argomento valido). Questi sono dispatch VM (BuiltInFunction
        // con l'istanza), non chiamate globali da risolvere qui.
        if (bce.Args.Length >= 1 && bce.Args[0] is not null)
        {
            var firstType = LinkerInferExpressionType(bce.Args[0], context, program);
            // NOTA: Stringify NON è nel bypass — STRINGIFY ha una firma generica che
            // l'overload resolution risolve correttamente (JsonSerializer.Serialize(obj));
            // includerlo qui lasciava il BuiltinFunctionCallExpr non risolto → ToJSON()
            // su DICT ritornava null/vuoto.
            bool isCollectionMethod = bce.Name.Kind is TokenKind.Count or TokenKind.ToArray or TokenKind.Keys
                or TokenKind.UniqueCount or TokenKind.ToList;
            if (isCollectionMethod && firstType is ListType or DictType or SortedDictType or HashSetType or
                QueueType or StackType or TreeType or AVLTreeType or RedBlackTreeType or LinkedListType or
                DoublyLinkedListType or CircularLinkedListType or BinaryHeapType or BagType or CacheType or
                GraphType or TimelineType or HashRingType or RingBufferType or SparseSetType or BPlusTreeType or
                BinaryIndexedTreeType or TrieType)
            {
                var rewrittenCollArgs = bce.Args.Select(a => a is null ? a! : RewriteExpression(a, context, program))
                    .ToImmutableArray();
                return bce with { Args = rewrittenCollArgs, ReturnType = new InferredType() };
            }
        }

        // --- GESTIONE SPECIALE: AREDUCE ---
        if (bce.Name.Kind == TokenKind.AReduce && bce.Args.Length == 3 && bce.Args[1] is LambdaExpr lambdaRed)
        {
            Expr rewrittenArray = RewriteExpression(bce.Args[0], context, program);
            Expr rewrittenInitial = RewriteExpression(bce.Args[2], context, program);

            DataType elementType = InferElementType(rewrittenArray, context, program) ?? new InferredType();
            DataType returnType = LinkerInferExpressionType(rewrittenInitial, context, program);

            Dictionary<string, DataType> lambdaScope =
                new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase);
            if (lambdaRed.Parameters.Length > 0) lambdaScope[lambdaRed.Parameters[0].OriginalName.Lexeme] = returnType;
            if (lambdaRed.Parameters.Length > 1) lambdaScope[lambdaRed.Parameters[1].OriginalName.Lexeme] = elementType;

            Expr rewrittenLambda = RewriteExpression(lambdaRed, context with { LambdaScope = lambdaScope }, program);
            ImmutableArray<Expr> finalArgs = [rewrittenArray, rewrittenLambda, rewrittenInitial];

            return bce with { Args = finalArgs, ReturnType = returnType };
        }

        // --- LOGICA DI RISOLUZIONE OVERLOAD CON INIEZIONE DI CONTESTO ---
        NativeProcedureDefinition? bestMatchProc = null;
        ImmutableArray<Expr>? bestMatchArgs = null;

        foreach (NativeProcedureDefinition candidate in candidates)
        {
            // Clona la definizione del candidato per specializzare i tipi dei parametri funzionali
            var specializedCandidate = candidate;

            // Se è una funzione di array che prende una callback, inferiamo il tipo della callback
            // basandoci sul tipo dell'array passato come primo argomento.
            if (bce.Args.Length > 0 &&
                (bce.Name.Kind == TokenKind.AEval || bce.Name.Kind == TokenKind.AFilter ||
                 bce.Name.Kind == TokenKind.AMap || bce.Name.Kind == TokenKind.AScan ||
                 bce.Name.Kind == TokenKind.ACount || bce.Name.Kind == TokenKind.ASort))
            {
                // Risolvi preliminarmente il primo argomento per ottenere il tipo elemento
                // Nota: Usiamo una copia del contesto per non influenzare lo stato globale in caso di fallimento
                var tempArrayExpr = RewriteExpression(bce.Args[0], context, program);
                var elementType = InferElementType(tempArrayExpr, context, program);

                if (elementType is not null and not InferredType)
                {
                    var newParams = candidate.Parameters.ToList();

                    // Indice del parametro callback (solitamente 1, ma per ASORT è 1 opzionale)
                    int callbackIdx = 1;

                    // ASCAN dichiara il 2° param (value) come InferredType (accetta sia un
                    // VALORE sia un predicato); quando l'argomento è una lambda/callback va
                    // specializzato a FunctionPointerType come le altre builtin array,
                    // altrimenti il tipo del parametro della lambda resta <inferred> ->
                    // backend IL emette Func<double,...> anche su array di stringhe ->
                    // InvalidProgramException (FunctionsLambdaInference/ASCAN).
                    var isAscanCallback = bce.Name.Kind == TokenKind.AScan
                        && bce.Args.Length > 1
                        && (bce.Args[1] is LambdaExpr or FunctionExpr);
                    if (callbackIdx < newParams.Count
                        && (newParams[callbackIdx].DataType is FunctionPointerType || isAscanCallback))
                    {
                        // Costruisci il tipo specifico della funzione
                        FunctionPointerType specificFuncType;
                        if (bce.Name.Kind == TokenKind.ASort)
                            // ASORT(a, b) -> Integer
                            specificFuncType = new FunctionPointerType([elementType, elementType], new IntegerType());
                        else if (bce.Name.Kind == TokenKind.AMap)
                            // AMAP(a) -> Any
                            specificFuncType = new FunctionPointerType([elementType], new InferredType());
                        else if (bce.Name.Kind == TokenKind.AEval)
                            // AEVAL(a, [idx]) -> Void. Gestiamo il caso con indice opzionale.
                            // Per semplicità qui definiamo il caso base, il rewriter lambda gestirà i parametri extra.
                            specificFuncType = new FunctionPointerType([elementType, new LongType()], new VoidType());
                        else
                            // AFilter, AScan, ACount(a) -> Boolean
                            specificFuncType = new FunctionPointerType([elementType], new BitType());

                        // Sostituisci il parametro generico con quello specializzato
                        newParams[callbackIdx] = newParams[callbackIdx] with { DataType = specificFuncType };

                        specializedCandidate = candidate with { Parameters = [.. newParams] };
                    }
                }
            }

            ImmutableArray<Expr>? mappingResult =
                RewriteArgumentsWithContext(bce.Args, specializedCandidate.Parameters, specializedCandidate, context,
                    program);

            if (mappingResult.HasValue)
            {
                bestMatchProc = specializedCandidate;
                bestMatchArgs = mappingResult.Value;
                break;
            }
        }

        if (bestMatchProc == null || !bestMatchArgs.HasValue)
        {
            throw CreateError(bce.Name,
                $"Gli argomenti forniti non sono validi per alcun overload della funzione built-in '{bce.Name.Lexeme}'.");
        }

        ImmutableArray<Expr> rewrittenArgs = bestMatchArgs.Value;

        // Blocco di controllo per OPTION STRICT su conversioni esplicite
        if (context.IsInStrictContext && rewrittenArgs.Length == 1 && rewrittenArgs[0] is NumberExpr numExpr)
        {
            double value = numExpr.Value;
            Token token = numExpr.Token;
            bool hasFraction = value % 1 != 0;

            switch (bce.Name.Kind)
            {
                case TokenKind.CInt:
                    if (hasFraction) throw CreateError(token, "OPTION STRICT ON: Perdita frazionaria in CINT.");
                    // INTEGER è un intero a 64 bit nel dialetto (parità PCode/VM-C/C99Native):
                    // il limite a 16 bit rifiutava valori legittimi come 70000, che le altre
                    // gambe accettano e stampano. Il limite dichiarato altrove per INTEGER
                    // (CheckNarrowingOverflow) è l'intervallo a 32 bit.
                    if (value < int.MinValue || value > int.MaxValue) throw CreateError(token, "Overflow in CINT.");
                    break;
                case TokenKind.CByte:
                    if (hasFraction) throw CreateError(token, "OPTION STRICT ON: Perdita frazionaria in CBYTE.");
                    if (value < Byte.MinValue || value > Byte.MaxValue) throw CreateError(token, "Overflow in CBYTE.");
                    break;
                case TokenKind.CLng:
                    if (hasFraction) throw CreateError(token, "OPTION STRICT ON: Perdita frazionaria in CLNG.");
                    if (value < Int32.MinValue || value > Int32.MaxValue) throw CreateError(token, "Overflow in CLNG.");
                    break;
            }
        }

        BuiltinFunctionCallExpr bceWithArgs = bce with { Args = rewrittenArgs };

        // Compile-time overflow check for narrowing conversions (CBYTE, CINT, etc.)
        CheckNarrowingOverflow(bceWithArgs, bestMatchProc);

        // ponytail: lexer maps SEARCH/COUNT to STRINGS TokenKinds — restore native BuiltinKind after overload match
        var resolvedName = bestMatchProc is NativeProcedureDefinition npdResolved
            ? bestMatchProc.OriginalName with { Kind = npdResolved.BuiltinKind }
            : bestMatchProc.OriginalName;

        DataType originalReturnType = bce.ReturnType;
        if (originalReturnType is InferredType || originalReturnType is ArrayType { ElementType: InferredType })
        {
            DataType finalReturnType = InferGenericPolymorphicReturnType(bceWithArgs with { Name = resolvedName },
                context, program);

            if (!originalReturnType.Equals(finalReturnType) && finalReturnType is not InferredType)
            {
                context.OnTypeInferred?.Invoke();
            }

            return bceWithArgs with { ReturnType = finalReturnType, Name = resolvedName };
        }

        return bceWithArgs with { Name = resolvedName };
    }

    /// <summary>
    /// Riscrive un nodo MethodCallExpr, risolvendo la chiamata a metodo o trasformandola in accesso indicizzato.
    /// </summary>
    private static Expr RewriteMethodCall(MethodCallExpr mce, AstRewriteContext context, BasicProgram program)
    {
        Expr rewrittenInstance = RewriteExpression(mce.Instance, context, program);
        // F1e (regola d'oro): il matching dei rami di rewrite usa la GRAFIA CANONICA
        // dal TokenKind, non il lexeme sorgente — .count()/.Count()/.COUNT() sono lo
        // STESSO metodo (prima: switch case-sensitive divergevano, l.count() →
        // UBOUND mentre .Count() → COUNT; d.set() non riscritto → nil silenzioso).
        // I membri fuori tabella (HTTP/WS/SQL/utente) restano lexeme.
        string memberName = CollMembers.Canonical(mce.MethodName.Kind) ?? mce.MethodName.Lexeme;

        // BASE.method() — resolve directly to base class method (bypass virtual dispatch)
        if (mce.Instance is BaseExpr && context.CurrentClass?.ResolvedBaseClass is { } baseClass)
        {
            var baseMethod = baseClass.Members.OfType<MethodMember>()
                .FirstOrDefault(m => m.Name.Lexeme.Equals(memberName, StringComparison.OrdinalIgnoreCase));
            if (baseMethod != null)
            {
                var rewrittenBaseArgs = mce.Args.Select(a => RewriteExpression(a, context, program)).ToImmutableArray();
                return mce with
                {
                    ResolvedDefinition = baseMethod.Procedure, Instance = rewrittenInstance, Args = rewrittenBaseArgs
                };
            }
        }

        Dictionary<string, string> aliasMap = program.LoadedModules[context.ModulePath].Imports
            .Where(i => i.Mode == ImportMode.Namespace)
            .ToDictionary(i => i.Alias?.Lexeme ?? i.NamespaceName.Lexeme, i => i.NamespaceName.Lexeme,
                StringComparer.OrdinalIgnoreCase);

        if (rewrittenInstance is VariableExpr ve)
        {
            string? nsName;
            if (aliasMap.TryGetValue(ve.NameCleaned.Lexeme, out nsName) ||
                program.Namespaces.Contains(ve.NameCleaned.Lexeme))
            {
                var rewrittenNsArgs = mce.Args.Select(a => RewriteExpression(a, context, program)).ToImmutableArray();
                return ResolveQualifiedCall(nsName ?? ve.NameCleaned.Lexeme, memberName, rewrittenNsArgs, program,
                    mce.MethodName);
            }
        }

        DataType instanceType = LinkerInferExpressionType(rewrittenInstance, context, program);

        // 1. Tentativo Metodi Definiti (Classi/UDT)
        ProcedureDefinition? resolvedMethod = null;
        if (instanceType is ClassType ct)
        {
            (MethodMember? m, _) = FindMethodInHierarchy(ct.Definition, memberName, mce.Args.Length);
            resolvedMethod = m?.Procedure;
        }
        else if (instanceType is InterfaceType it)
        {
            // Per chiamate su variabili di tipo interfaccia, trova il metodo nella definizione dell'interfaccia.
            // La dispatch sarà eseguita a runtime tramite VTable.
            var ifaceMethod = it.Definition.Members
                .OfType<MethodMember>()
                .FirstOrDefault(m =>
                    m.Procedure.OriginalName.Lexeme.Equals(memberName, StringComparison.OrdinalIgnoreCase) &&
                    SymbolTable.IsProcedureSignatureCompatible(m.Procedure, mce.Args.Length));
            resolvedMethod = ifaceMethod?.Procedure;
        }
        else if (instanceType is UserDefinedType udt)
        {
            resolvedMethod = udt.Methods.FirstOrDefault(m =>
                m.OriginalName.Lexeme.Equals(memberName, StringComparison.OrdinalIgnoreCase) &&
                SymbolTable.IsProcedureSignatureCompatible(m, mce.Args.Length));
        }

        if (resolvedMethod != null)
        {
            ImmutableArray<Expr>? mappingResult = RewriteArgumentsWithContext(mce.Args, resolvedMethod.Parameters,
                resolvedMethod, context, program);

            if (!mappingResult.HasValue)
            {
                throw CreateError(mce.MethodName,
                    $"Gli argomenti forniti non sono validi per il metodo '{memberName}' del tipo '{instanceType}'.");
            }

            // For native static methods (like KEY.MAKE), produce a BuiltinFunctionCallExpr
            // to avoid evaluating the class name as a runtime variable.
            if (resolvedMethod is NativeProcedureDefinition npd)
            {
                var nsToken = new Token(TokenKind.Identifier, instanceType.ToString(), null, mce.MethodName.Position);
                return new BuiltinFunctionCallExpr(nsToken, npd.OriginalName, mappingResult.Value, npd.ReturnType);
            }

            return mce with
            {
                Instance = rewrittenInstance,
                Args = mappingResult.Value,
                ResolvedDefinition = resolvedMethod
            };
        }

        // 2a. Array Fluent Methods: arr.Filter(fn) → ARRAYS.AFILTER(arr, fn)
        if (instanceType is ArrayType)
        {
            // .Count() senza argomenti: numero di elementi. Si risolve a SYSTEM.UBOUND(a):
            // il builtin COUNT ha firma (stringa, sottostringa) e su un ARRAY non ha overload
            // ("argomenti non validi per alcun overload della funzione built-in 'Count'").
            // UBOUND accetta l'array e ritorna l'ultimo indice; per un array di n elementi è n-1,
            // quindi la lunghezza è UBOUND+1 (stessa semantica del ramo ListType più sotto).
            if (memberName.Equals("Count", StringComparison.OrdinalIgnoreCase) && mce.Args.Length == 0)
            {
                var ubToken = new Token(TokenKind.UBound, "UBOUND", null, mce.MethodName.Position);
                var nsToken = new Token(TokenKind.Identifier, "SYSTEM", null, mce.MethodName.Position);
                var uboundCall = (Expr)ResolveQualifiedCall("SYSTEM", "UBOUND",
                    ImmutableArray.Create(rewrittenInstance), program, ubToken);
                return new BinaryExpr(uboundCall, new Token(TokenKind.Plus, "+", null, ubToken.Position),
                    new NumberExpr(1, ubToken)) { ScopeId = mce.ScopeId };
            }
            var arrayFuncName = _arrayFluentMethodMap.TryGetValue(memberName, out var mapped)
                ? mapped
                : memberName;
            if (program.GetProceduresForNamespace("ARRAYS").Any(p =>
                    p.OriginalName.Lexeme.Equals(arrayFuncName, StringComparison.OrdinalIgnoreCase)))
            {
                var newArgs = ImmutableArray.Create(rewrittenInstance).AddRange(
                    mce.Args.Select(a => RewriteExpression(a, context, program)));
                var token = new Token(TokenKind.Identifier, memberName, null, mce.MethodName.Position);
                return ResolveQualifiedCall("ARRAYS", arrayFuncName, newArgs, program, token);
            }
        }

        // 2b-prime. Queue/Stack built-in methods (before collection fallback)
        if (instanceType is QueueType || instanceType is StackType)
        {
            var tk = memberName switch
            {
                "Push" => TokenKind.StackPush,
                "Pop" => TokenKind.StackPop,
                _ => (TokenKind?)null
            };
            if (tk.HasValue)
            {
                var nsToken = new Token(TokenKind.Identifier, "SYSTEM", null, mce.MethodName.Position);
                var nameToken = new Token(tk.Value, memberName, null, mce.MethodName.Position);
                return new BuiltinFunctionCallExpr(nsToken, nameToken,
                    ImmutableArray.Create(rewrittenInstance).AddRange(mce.Args.Select(a => RewriteExpression(a, context, program))),
                    new InferredType());
            }
        }

        // 2b-prime-2. Dict/SortedDict built-in methods
        if (instanceType is DictType || instanceType is SortedDictType)
        {
            var tk = memberName switch
            {
                "Keys" => TokenKind.Keys,
                "ToArray" => TokenKind.ToArray,
                "ToList" => TokenKind.ToList,
                _ => (TokenKind?)null
            };
            if (tk.HasValue)
            {
                var nsToken = new Token(TokenKind.Identifier, "SYSTEM", null, mce.MethodName.Position);
                var nameToken = new Token(tk.Value, memberName, null, mce.MethodName.Position);
                return new BuiltinFunctionCallExpr(nsToken, nameToken,
                    ImmutableArray.Create(rewrittenInstance).AddRange(mce.Args.Select(a => RewriteExpression(a, context, program))),
                    new InferredType());
            }
        }

        // 2b-prime-3. Common collection methods (ToArray, ToList) for all collection types
        if (instanceType is ArrayType or ListType or HashSetType or QueueType or StackType or
            DictType or SortedDictType or TreeType or AVLTreeType or RedBlackTreeType or
            LinkedListType or DoublyLinkedListType or CircularLinkedListType or
            BinaryHeapType or BagType or CacheType or GraphType or TimelineType or
            RingBufferType or SparseSetType or BPlusTreeType or BinaryIndexedTreeType or TrieType or
            InferredType)
        {
            var commonTk = memberName switch
            {
                "ToArray" => TokenKind.ToArray,
                "ToList" => TokenKind.ToList,
                "Count" => TokenKind.Count,
                "ToJson" => TokenKind.Stringify,
                "ToJSON" => TokenKind.Stringify,
                "ToDict" => TokenKind.ToDict,
                "UniqueCount" => TokenKind.UniqueCount,
                // NOTA: FindAll/Query/QueryRange/GetInDegree/Dijkstra/NodeCount NON sono nel
                // mapping comune: sono metodi/proprietà PER-TIPO (Trie.FindAllWordsWithPrefix,
                // BIT.Query/QueryRange, Graph.GetInDegree/Dijkstra/NodeCount) che la reflection
                // VM dispatcha direttamente. Mapparli a ToArray/Count qui produceva
                // SYSTEM.TOARRAY(istanza, argomento) che falliva (FindAll('ca') su TRIE = null,
                // BIT.Query(2) vuoto, Graph.Dijkstra null nel sample, Graph.NodeCount=0).
                "Keys" => TokenKind.Keys,
                _ => (TokenKind?)null
            };
            if (commonTk.HasValue)
            {
                var nsToken = new Token(TokenKind.Identifier, "SYSTEM", null, mce.MethodName.Position);
                var nameToken = new Token(commonTk.Value, memberName, null, mce.MethodName.Position);
                return new BuiltinFunctionCallExpr(nsToken, nameToken,
                    ImmutableArray.Create(rewrittenInstance).AddRange(mce.Args.Select(a => RewriteExpression(a, context, program))),
                    new InferredType());
            }
        }

        // 2b. Tentativo Metodi Collezioni Built-in
        if (InferCollectionMethodReturnType(instanceType, memberName) != null)
        {
            DataType? elementType = instanceType switch
            {
                ArrayType at => at.ElementType,
                ListType lt => lt.ElementType,
                _ => null
            };

            ImmutableArray<Expr>.Builder rewrittenArgsBuilder = ImmutableArray.CreateBuilder<Expr>();

            // .Count() senza argomenti su List: restituisci la lunghezza.
            if (memberName.Equals("Count", StringComparison.OrdinalIgnoreCase) && mce.Args.Length == 0 &&
                instanceType is ListType)
            {
                var countArgsList = ImmutableArray.Create(rewrittenInstance);
                return ResolveQualifiedCall("SYSTEM", "UBOUND", countArgsList, program, mce.MethodName);
            }

            // .Max()/.Min()/.Sum() senza argomenti su List: risolvono ai builtin AMAX/AMIN/ASUM
            // direttamente (BuiltinFunctionCallExpr), NON a un qualificato ARRAYS.AMAX che
            // richiederebbe l'import del namespace (il nome qualificato richiede sempre l'import).
            if (memberName.Equals("Max", StringComparison.OrdinalIgnoreCase) && mce.Args.Length == 0 &&
                instanceType is ListType)
            {
                var amaxToken = new Token(TokenKind.AMax, "AMAX", null, mce.MethodName.Position);
                var nsToken = new Token(TokenKind.Identifier, "SYSTEM", null, mce.MethodName.Position);
                return new BuiltinFunctionCallExpr(nsToken, amaxToken, ImmutableArray.Create(rewrittenInstance), new DoubleType());
            }
            if (memberName.Equals("Min", StringComparison.OrdinalIgnoreCase) && mce.Args.Length == 0 &&
                instanceType is ListType)
            {
                var aminToken = new Token(TokenKind.AMin, "AMIN", null, mce.MethodName.Position);
                var nsToken = new Token(TokenKind.Identifier, "SYSTEM", null, mce.MethodName.Position);
                return new BuiltinFunctionCallExpr(nsToken, aminToken, ImmutableArray.Create(rewrittenInstance), new DoubleType());
            }
            if (memberName.Equals("Sum", StringComparison.OrdinalIgnoreCase) && mce.Args.Length == 0 &&
                instanceType is ListType)
            {
                var asumToken = new Token(TokenKind.ASum, "ASUM", null, mce.MethodName.Position);
                var nsToken = new Token(TokenKind.Identifier, "SYSTEM", null, mce.MethodName.Position);
                return new BuiltinFunctionCallExpr(nsToken, asumToken, ImmutableArray.Create(rewrittenInstance), new DoubleType());
            }

            foreach (Expr arg in mce.Args)
            {
                if (arg is LambdaExpr lambda && elementType != null)
                {
                    Dictionary<string, DataType> lambdaScope =
                        new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase);
                    if (lambda.Parameters.Length > 0)
                    {
                        lambdaScope[lambda.Parameters[0].OriginalName.Lexeme] = elementType;
                    }

                    rewrittenArgsBuilder.Add(RewriteExpression(lambda, context with { LambdaScope = lambdaScope },
                        program));
                }
                else
                {
                    rewrittenArgsBuilder.Add(RewriteExpression(arg, context, program));
                }
            }

            return mce with { Instance = rewrittenInstance, Args = rewrittenArgsBuilder.ToImmutable() };
        }

        // 3. Fallback Campo Indicizzabile
        // Base NON risolta (es. `PORTFOLIO.ISLONGONLY(w)` con qualifica PARZIALE): NON è un
        // "campo indicizzabile". Prima si costruiva IndexedAccessExpr su un simbolo
        // placeholder (__dummy, SymbolId<0): il backend IL lanciava NotSupportedException
        // ("impossibile risolvere il tipo per il simbolo '__dummy'") e PCode/VM-C
        // restituivano NIL in silenzio. Errore esplicito QUI = stessa diagnosi ovunque.
        if (rewrittenInstance is VariableExpr { SymbolId: < 0 } unknownBase)
        {
            throw CreateError(mce.MethodName,
                $"Namespace o oggetto '{unknownBase.NameCleaned.Lexeme}' sconosciuto: impossibile risolvere '{unknownBase.NameCleaned.Lexeme}.{memberName}'. Usare la qualifica COMPLETA (es. TRADING.PORTFOLIO.{memberName}) o IMPORT del namespace.",
                ErrorCode.UndefinedSubOrFunction);
        }
        FieldAccessExpr fieldAccessExpr = new FieldAccessExpr(rewrittenInstance, mce.MethodName);
        DataType fieldType = LinkerInferExpressionType(fieldAccessExpr, context, program);
        if (fieldType is ArrayType or ListType or DictType or InferredType or JsonType or HttpHeadersType)
        {
            ImmutableArray<Expr> fallbackArgs = [.. mce.Args.Select(a => RewriteExpression(a, context, program))];
            return new IndexedAccessExpr(fieldAccessExpr, fallbackArgs);
        }

        throw CreateError(mce.MethodName, $"Membro '{memberName}' non trovato o non accessibile su '{instanceType}'.",
            ErrorCode.MemberNotFound);
    }

    /// <summary>
    /// Helper per risolvere e riscrivere una chiamata a funzione qualificata (statica).
    /// </summary>
    private static Expr ResolveQualifiedCall(string namespaceName, string functionName,
        ImmutableArray<Expr> args, BasicProgram program, Token errorToken)
    {
        var allProcsInNs = program.GetProceduresForNamespace(namespaceName).ToList();

        var procCandidates = allProcsInNs
            .Where(p => p.OriginalName.Lexeme.Equals(functionName, StringComparison.OrdinalIgnoreCase) &&
                        SymbolTable.IsProcedureSignatureCompatible(p, args.Length))
            .ToList();

        switch (procCandidates.Count)
        {
            case 1:
            {
                var proc = procCandidates[0];
                var nsToken = new Token(TokenKind.Identifier, namespaceName, null, errorToken.Position);

                // Se è una nativa, usa BuiltinFunctionCallExpr
                if (proc is NativeProcedureDefinition npd)
                {
                    // ponytail: use npd.OriginalName so keyword members (SEARCH, COUNT) keep AiVector* TokenKind
                    return new BuiltinFunctionCallExpr(nsToken, npd.OriginalName, args, npd.ReturnType);
                }

                // Altrimenti usa FunctionExpr standard con ResolvedDefinition
                var funcExpr = new FunctionExpr(proc.SymbolId, errorToken, args,
                    proc is FunctionDefinition fd ? fd.ReturnType : new VoidType());
                funcExpr.ResolvedDefinition = proc;
                return funcExpr;
            }
            case > 1:
                throw CreateError(errorToken,
                    $"La chiamata a '{namespaceName}.{functionName}' è ambigua. Trovati " +
                    $"{procCandidates.Count} overload compatibili nel namespace '{namespaceName}'.");
            default:
                throw CreateError(errorToken,
                    $"Nessuna funzione pubblica '{functionName}' trovata nel namespace '{namespaceName}' " +
                    $"che accetti {args.Length} argomenti.");
        }
    }

    /// <summary>
    /// Appiattisce ricorsivamente un ArrayLiteralExpr annidato in una lista piatta di espressioni.
    /// Usato per supportare l'inizializzazione di matrici multidimensionali con sintassi {{a,b},{c,d}}.
    /// </summary>
    private static ImmutableArray<Expr> TryFlattenArrayLiteral(ArrayLiteralExpr ale)
    {
        var flatBuilder = ImmutableArray.CreateBuilder<Expr>();
        foreach (var element in ale.Elements)
        {
            if (element is ArrayLiteralExpr nestedAle)
                flatBuilder.AddRange(TryFlattenArrayLiteral(nestedAle));
            else
                flatBuilder.Add(element);
        }

        return flatBuilder.ToImmutable();
    }

    /// <summary>
    /// Riconosce un letterale matrice rettangolare a 2 dimensioni, scritto con la sintassi
    /// annidata `{{...}, {...}}` (ArrayLiteralExpr) oppure `[[...], [...]]` (ListLiteralExpr).
    /// Restituisce le righe solo se TUTTE le righe sono letterali e hanno la stessa lunghezza
    /// (rettangolare); altrimenti false e il chiamante ricade sul comportamento precedente.
    /// </summary>
    private static bool TryGetRectangularMatrixLiteral(Expr value,
        out ImmutableArray<ImmutableArray<Expr>> rows)
    {
        rows = default;

        ImmutableArray<Expr> outer;
        switch (value)
        {
            case ArrayLiteralExpr ale:
                outer = ale.Elements;
                break;
            case ListLiteralExpr lle:
                outer = lle.Elements;
                break;
            default:
                return false;
        }

        if (outer.IsEmpty) return false;

        var built = ImmutableArray.CreateBuilder<ImmutableArray<Expr>>(outer.Length);
        int? width = null;
        foreach (var element in outer)
        {
            ImmutableArray<Expr> inner;
            switch (element)
            {
                case ArrayLiteralExpr innerAle:
                    inner = innerAle.Elements;
                    break;
                case ListLiteralExpr innerLle:
                    inner = innerLle.Elements;
                    break;
                default:
                    return false; // Riga non-letterale: non è una matrice di soli valori.
            }

            if (width is null) width = inner.Length;
            else if (width != inner.Length) return false; // Ragged: non rettangolare.

            built.Add(inner);
        }

        rows = built.ToImmutable();
        return true;
    }

    /// <summary>
    /// `A = {{...}}` / `A = [[...]]` su una variabile NON già dichiarata come array
    /// multidimensionale: la trasforma in una dichiarazione implicita
    /// `DIM A(rows-1, cols-1) = {{...}}`, così il letterale matrice diventa un vero array
    /// 2-D (e `A(i,j)` funziona) invece di un array 1-D appiattito o di una LIST OF LIST.
    /// Per i target già dichiarati (es. `DIM M(r,c)` seguito da `M = {{...}}`)
    /// l'assegnazione è gestita in RewriteLetStmt per espansione elemento-per-elemento.
    /// </summary>
    private static Stmt RewriteLetStmtWithImplicitMatrix(LetStmt ls, AstRewriteContext context, BasicProgram program)
    {
        if (ls.Assignments.Length == 1
            && ls.Assignments[0].Target is VariableTarget vt
            && vt.SymbolId >= 0
            // Solo la forma con GRAFFE `{{...}}` (letterale array): `[[...]]` resta una
            // LIST OF LIST (semantica di lista documentata), MAI convertita in matrice.
            && ls.Assignments[0].Value is ArrayLiteralExpr
            && TryGetRectangularMatrixLiteral(ls.Assignments[0].Value, out var rows)
            && rows.Length > 0 && rows[0].Length > 0
            && !(program.Symbols.GetSymbol(vt.SymbolId).DeclarationNode is VariableDeclaration existing
                 && existing.Dimensions.Length >= 2)
            // Solo target scalari/impliciti: una variabile già dichiarata come ARRAY o
            // LISTA/collezione NON va convertita in matrice (un `A = [[...]]` di
            // riassegnazione lista deve restare una lista).
            && program.Symbols.GetSymbol(vt.SymbolId).DataType
                is InferredType or AnyType or DoubleType or IntegerType or LongType
                or SingleType or CurrencyType or BitType or ByteType)
        {
            var pos = vt.NameSuffixed.Position;
            var openToken = new Token(TokenKind.LeftBrace, "{", null, pos);
            var outer = ImmutableArray.CreateBuilder<Expr>(rows.Length);
            foreach (var row in rows)
                outer.Add(new ArrayLiteralExpr(row, openToken));
            var nestedLiteral = new ArrayLiteralExpr(outer.ToImmutable(), openToken);

            Expr rowsMinusOne = new NumberExpr(rows.Length - 1,
                new Token(TokenKind.Number, (rows.Length - 1).ToString(), (double)(rows.Length - 1), pos));
            Expr colsMinusOne = new NumberExpr(rows[0].Length - 1,
                new Token(TokenKind.Number, (rows[0].Length - 1).ToString(), (double)(rows[0].Length - 1), pos));

            var decl = new VariableDeclaration(vt.SymbolId, vt.NameSuffixed, vt.NameCleaned,
                [rowsMinusOne, colsMinusOne], null, nestedLiteral, false, false);
            var rewrittenDecl = RewriteVariableDeclarationWithInference(decl, context, program);
            var dimToken = new Token(TokenKind.Dim, "DIM", null, pos);
            return new DeclarationStmt(dimToken, [rewrittenDecl]) { ScopeId = ls.ScopeId };
        }

        return RewriteLetStmt(ls, context, program);
    }

    /// <summary>
    /// [NUOVO HELPER] Verifica se un tipo di dato è un tipo intero (con o senza segno).
    /// </summary>
    private static bool IsIntegerBasedType(DataType type)
    {
        return type is ByteType or SByteType or IntegerType or UIntegerType or LongType or ULongType or QuadType
            or UQuadType;
    }

    /// <summary>
    /// Estrae il tipo dell'elemento se il DataType fornito è una collezione (Array o List).
    /// Se il tipo è uno scalare, lo restituisce invariato.
    /// </summary>
    private static DataType GetBaseType(DataType type)
    {
        return type switch
        {
            ArrayType at => at.ElementType,
            ListType lt => lt.ElementType,
            _ => type
        };
    }

    /// <summary>
    /// Riscrive una dichiarazione di variabile stabilizzando il tipo e i flag strutturali.
    /// Risolve il problema del double-wrapping dando priorità alla clausola AS e utilizzando
    /// unwrapping controllato.
    /// </summary>
    /// <summary>
    /// Rewrites a catch clause variable declaration preserving its exception type (ERR or Inferred).
    /// Unlike regular variables, catch variables should not have their type inferred to DoubleType.
    /// </summary>
    private static VariableDeclaration RewriteCatchVariableDeclaration(VariableDeclaration decl,
        AstRewriteContext context, BasicProgram program)
    {
        int resolvedSymbolId = decl.SymbolId;
        if (resolvedSymbolId < 0) throw CreateError(decl.NameSuffixed, "Errore interno: SymbolId non risolto.");

        SymbolTable.Symbol originalSymbol = program.Symbols.GetSymbol(resolvedSymbolId);
        
        // If the variable has an explicit type (AS clause), resolve it normally.
        // If it was Inferred (no AS), default to ErrType.
        DataType finalType;
        if (decl.HasAsClause && decl.DataType != null)
        {
            finalType = ResolveDataType(decl.DataType, program, context.ModulePath);
        }
        else
        {
            finalType = new ErrType();
        }

        // Aggiorna il tipo del simbolo nella SymbolTable — UpdateSymbolType, non
        // UpdateSymbolDeclaration: quest'ultima aggiorna SOLO Symbol.DeclarationNode (vedi
        // SymbolTable.cs), MAI Symbol.DataType, che è però esattamente il campo letto da
        // ogni consumer (Symbols.GetSymbol(id).DataType) — il tipo resta quindi
        // silenziosamente non risolto (UnresolvedQualifiedType) per QUALUNQUE lettura
        // successiva basata sul SymbolId, anche se il nodo AST restituito da questo metodo
        // porta già il tipo corretto. Bug scoperto compilando "CATCH ex AS
        // <CLASS utente>" nel backend IL (che legge la SymbolTable per il dispatch),
        // ma il sintomo esiste per QUALUNQUE consumer che risolva "ex" per SymbolId invece
        // che dal nodo CatchClause.Variable direttamente.
        if (!finalType.Equals(originalSymbol.DataType))
            program.Symbols.UpdateSymbolType(resolvedSymbolId, finalType);

        return decl with { DataType = finalType, SymbolId = resolvedSymbolId };
    }

    private static VariableDeclaration RewriteVariableDeclarationWithInference(VariableDeclaration decl,
        AstRewriteContext context, BasicProgram program)
    {
        const bool log = false;

        if (context.IsInStrictContext && !decl.HasAsClause &&
            SplitNameAndSuffix(decl.NameSuffixed.Lexeme).SuffixType == null)
        {
            throw CreateError(decl.NameSuffixed,
                $"OPTION STRICT ON: La variabile '{decl.NameSuffixed.Lexeme}' richiede un tipo esplicito (clausola AS o suffisso).");
        }

        int resolvedSymbolId = decl.SymbolId;
        if (resolvedSymbolId < 0) throw CreateError(decl.NameSuffixed, "Errore interno: SymbolId non risolto.");

        SymbolTable.Symbol originalSymbol = program.Symbols.GetSymbol(resolvedSymbolId);
        (string _, DataType? suffixType) = SplitNameAndSuffix(decl.NameSuffixed.Lexeme);

        Log($"[REWRITE-DECL] Processing var '{decl.NameSuffixed.Lexeme}' (ID: {resolvedSymbolId}). " +
            $"Current Symbol Type: {originalSymbol.DataType}", log);

        // 1. Risoluzione espressioni figlie
        Expr? rewrittenInitialValue = decl.InitialValue is not null
            ? RewriteExpression(decl.InitialValue, context with { ExpectedType = suffixType ?? decl.DataType }, program)
            : null;

        ImmutableArray<Expr> rewrittenDimensions =
            [.. decl.Dimensions.Select(d => RewriteExpression(d, context, program))];
        ImmutableArray<Expr> rewrittenLowerBounds =
            [.. decl.LowerBounds.Select(b => RewriteExpression(b, context, program))];

        // 2. DETERMINAZIONE DEL TIPO
        bool isArrayStructure = !decl.Dimensions.IsEmpty || decl.IsImplicitArray;
        DataType finalType;

        if (decl.HasAsClause && decl.DataType != null)
        {
            // Autorità 1: Clausola AS. Il tipo specificato è la fonte di verità.
            DataType resolvedTypeFromAs = ResolveDataType(decl.DataType, program, context.ModulePath);

            // Se la clausola AS definisce già un Array (es. "AS ARRAY OF BYTE") e la dichiarazione
            // ha una struttura array (es. "DIM arr(10)"), il tipo finale è quello della clausola AS.
            // Le dimensioni servono solo per l'allocazione, non per creare un array di array.
            if (isArrayStructure && resolvedTypeFromAs is ArrayType)
            {
                finalType = resolvedTypeFromAs;
            }
            // Se la clausola AS definisce un tipo scalare ma la struttura è array, wrappiamo.
            else if (isArrayStructure)
            {
                finalType = new ArrayType(resolvedTypeFromAs);
            }
            else
            {
                finalType = resolvedTypeFromAs;
            }
        }
        else
        {
            // Se non c'è AS, usiamo la gerarchia Suffix -> Initializer -> Fallback
            DataType baseType;
            if (suffixType != null)
            {
                baseType = suffixType;
                // DIM r$ = <inizializzatore-array> (es. ARRAYS.AREVERSE(w) su array di stringhe):
                // il suffisso tipizza l'ELEMENTO, non la variabile — il risultato è un array
                // dinamico di stringhe. Senza questo, r$ restava String scalare e l'indicizzazione
                // r$(0) falliva in RewriteArrayAccess ("non è un array").
                if (rewrittenInitialValue != null
                    && LinkerInferExpressionType(rewrittenInitialValue, context, program) is ArrayType or ListType)
                {
                    isArrayStructure = true;
                }
            }
            else if (rewrittenInitialValue != null)
            {
                DataType initType = LinkerInferExpressionType(rewrittenInitialValue, context, program);

                if (isArrayStructure)
                {
                    // DIM X() = <array>: parentesi vuote SENZA dimensioni — il tipo preserva
                    // il rank dell'inizializzatore (es. FFT -> ARRAY OF ARRAY OF DOUBLE).
                    // Solo con dimensioni esplicite (DIM X(5)) si unwrappa all'elemento.
                    baseType = decl.Dimensions.IsEmpty && initType is ArrayType
                        ? initType
                        : GetInnerElementType(initType);
                }
                else
                {
                    baseType = initType;
                    if (initType is ArrayType or ListType) isArrayStructure = true;
                }
            }
            else
            {
                // Fallback (Sticky Types)
                if (originalSymbol.DataType is not InferredType && originalSymbol.DataType is not DoubleType)
                {
                    if (isArrayStructure && originalSymbol.DataType is ArrayType at)
                    {
                        baseType = at.ElementType; // Unwrap per idempotenza
                    }
                    else
                    {
                        baseType = originalSymbol.DataType;
                    }
                }
                else
                {
                    baseType = new DoubleType();
                }
            }

            if (baseType is InferredType) baseType = new AnyType();

            // Ricostruisci il tipo finale
            finalType = isArrayStructure && baseType is not ArrayType && baseType is not ListType
                ? new ArrayType(baseType)
                : baseType;
        }

        // 3. APPIATTIMENTO MATRICI
        if (finalType is ArrayType && !decl.Dimensions.IsEmpty && decl.Dimensions.Length > 1)
        {
            if (rewrittenInitialValue is ArrayLiteralExpr ale)
            {
                rewrittenInitialValue = ale with { Elements = TryFlattenArrayLiteral(ale) };
            }
            else if (rewrittenInitialValue is ListLiteralExpr
                     && TryGetRectangularMatrixLiteral(rewrittenInitialValue, out var listRows))
            {
                // `[[..],[..]]` (ListLiteralExpr) su un DIM di array 2-D: converti le righe in
                // ArrayLiteralExpr e appiattisci (stessa forma piatta consumata dal codegen,
                // come `{{..},{..}}`). Senza, il letterale resta LIST OF LIST e la coercizione
                // verso ARRAY fallisce ("Impossibile convertire implicitamente...").
                var braceTok = new Token(TokenKind.LeftBrace, "{", null, decl.NameSuffixed.Position);
                var nested = new ArrayLiteralExpr(
                    listRows.Select(r => (Expr)new ArrayLiteralExpr(r, braceTok)).ToImmutableArray(), braceTok);
                rewrittenInitialValue = nested with { Elements = TryFlattenArrayLiteral(nested) };
            }
        }

        // 4. AGGIORNAMENTO SYMBOL TABLE
        if (!finalType.Equals(originalSymbol.DataType))
        {
            program.Symbols.UpdateSymbolType(resolvedSymbolId, finalType);
            context.OnTypeInferred?.Invoke();
        }

        if (finalType is ArrayType or ListType or FunctionPointerType || isArrayStructure)
        {
            program.Symbols.UpdateSymbolFlags(resolvedSymbolId, SymbolFlags.IsArray);
        }
        else
        {
            program.Symbols.RemoveSymbolFlags(resolvedSymbolId, SymbolFlags.IsArray);
        }

        if (rewrittenInitialValue != null)
        {
            var initialType = LinkerInferExpressionType(rewrittenInitialValue, context, program);
            rewrittenInitialValue = RewriteUserDefinedCTypeConversion(
                rewrittenInitialValue, initialType, finalType, context, program);
        }

        // 5. VALIDAZIONE
        if (rewrittenInitialValue != null && context.IsInStrictContext)
        {
            DataType valType = LinkerInferExpressionType(rewrittenInitialValue, context, program);
            if (valType is not InferredType && !finalType.Equals(valType) &&
                !CanCoerce(valType, finalType, context, program))
            {
                throw CreateError(rewrittenInitialValue,
                    $"Impossibile assegnare un valore di tipo '{valType}' nella variabile '{decl.NameSuffixed.Lexeme}' (tipo dichiarato '{finalType}'). Tipizzare la variabile (es. AS JSON/AS STRING) o usare il suffisso '$'.",
                    ErrorCode.TypeMismatch);
            }
        }

        // 6. AGGIORNAMENTO AST
        VariableDeclaration finalDecl = decl with
        {
            SymbolId = resolvedSymbolId,
            DataType = finalType,
            InitialValue = rewrittenInitialValue,
            Dimensions = rewrittenDimensions,
            LowerBounds = rewrittenLowerBounds
        };
        program.Symbols.UpdateSymbolDeclaration(resolvedSymbolId, finalDecl);

        return finalDecl;
    }

    /// <summary>
    /// Riscrive un nodo FieldAccessExpr, gestendo l'accesso a `BASE`, validando l'accesso ai campi
    /// e risolvendo gli alias di namespace per disambiguare le chiamate a funzione qualificate.
    /// VERSIONE CORRETTA: Aggiunta la logica di validazione e inferenza per i campi dei tipi built-in.
    /// </summary>
    private static Expr RewriteFieldAccess(FieldAccessExpr fae, AstRewriteContext context, BasicProgram program)
    {
        // §11.1: `istanza.Font.Name` / `istanza.Font.Size` (oggetto Font VB6) →
        // proprietà piatta della base Control (Font.Name / Font.Size).
        if (fae.Structure is FieldAccessExpr fontField &&
            fontField.Structure is VariableExpr fontVar &&
            fontField.Field.Lexeme.Equals("Font", StringComparison.OrdinalIgnoreCase) &&
            program.Symbols.GetSymbol(fontVar.SymbolId)?.DataType is ClassType fontClass &&
            InheritsControlFlexible(fontClass.Definition))
        {
            string? fontProp = fae.Field.Lexeme.ToUpperInvariant() switch
            {
                "NAME" => "Font.Name",
                "SIZE" => "Font.Size",
                _ => null
            };
            if (fontProp != null)
            {
                bool fontIsStr = fontProp == "Font.Name";
                return new BuiltinFunctionCallExpr(
                    new Token(TokenKind.Identifier, "CONTROLS", null, fae.Field.Position),
                    new Token(TokenKind.Identifier, fontIsStr ? "CTRLGETPROPS" : "CTRLGETPROP", null, fae.Field.Position),
                    [new StringExpr(fontVar.NameCleaned.Lexeme, fae.Field), new StringExpr(fontProp, fae.Field)],
                    fontIsStr ? new StringType() : new DoubleType());
            }
        }
        // §11.1: `istanza.Prop` di una classe INHERITS Control → lettura proprietà (CONTROL GET/GETS).
        if (fae.Structure is VariableExpr cpv &&
            program.Symbols.GetSymbol(cpv.SymbolId)?.DataType is ClassType cpc && InheritsControlFlexible(cpc.Definition))
        {
            var pr = ControlPropRuntime(fae.Field.Lexeme);
            if (pr != null)
            {
                bool isStr = ControlPropIsString(fae.Field.Lexeme);
                return new BuiltinFunctionCallExpr(
                    new Token(TokenKind.Identifier, "CONTROLS", null, fae.Field.Position),
                    new Token(TokenKind.Identifier, isStr ? "CTRLGETPROPS" : "CTRLGETPROP", null, fae.Field.Position),
                    [new StringExpr(cpv.NameCleaned.Lexeme, fae.Field), new StringExpr(pr, fae.Field)],
                    isStr ? new StringType() : new DoubleType());
            }
        }
        // Late-binding DBF/ISAM (campo nudo, es. "Codice" dopo `DBF USE ... AS Type`): il
        // Binder produce GIA' un FieldAccessExpr(_dbRecord, "Codice") completo e finale (vedi
        // RewriteVariableExpr, ramo 9 "Late Binding"). Se il Rewriter passa DI NUOVO su questo
        // stesso nodo (già costruito in una fase precedente — Binding gira prima di Rewrite),
        // il dispatch generico lo instrada qui invece che a RewriteVariableExpr, e Fase 1
        // sotto ri-risolveva _dbRecord (il VariableExpr sentinella SENZA alcuna variabile
        // reale dietro) come se fosse un identificatore nudo comune: essendo anch'esso
        // sconosciuto, scattava DI NUOVO il late-binding, producendo un secondo
        // FieldAccessExpr(_dbRecord, "_dbRecord") che rimpiazzava il nome del campo VERO
        // ("Codice") con quello del sentinella stesso — bare field read dopo un `AS Type`
        // sempre vuoto/sbagliato (scoperto isolando "DBF REPLACE ... : PRINT Codice" nello
        // stesso procedimento, senza CLOSE/riapertura di mezzo). Il sentinella non ha
        // struttura da risolvere ulteriormente: se fae.Structure è già _dbRecord, non
        // ricorrere affatto.
        if (fae.Structure is VariableExpr dbRecordSentinel && dbRecordSentinel.SymbolId == SymbolTable.DbfRecordBufferSymbolId)
            return fae;

        // Evidenza da USO: `param.Campo` dove param è un parametro IMPLICITO non tipato (default
        // double, nessun AS né suffisso). Un double non ha campi: l'accesso a un membro è prova
        // concreta che il parametro NON è numerico ma un oggetto/ANY (es. `SUB PrintResult(label$,
        // result)` con `result.Statistic`, dove result riceve il Dictionary di STATS.TTEST). La VM
        // lo tratta già come VARIANT dinamico (pcode: funziona); IL/C99 lo tipizzavano double ->
        // member access su double = InvalidProgramException / errore cc. Promozione a ANY — stesso
        // pattern di RewritePreResolvedSymbol (Linker.Rewrites.Variables.cs, inferenza dal corpo) —
        // così il resync parametri piu' sotto propaga ANY al codegen.
        if (fae.Structure is VariableExpr { SymbolId: >= 0 } structVar)
        {
            var paramSym = program.Symbols.GetSymbol(structVar.SymbolId);
            if (paramSym is { DataType: DoubleType }
                && paramSym.Flags.HasFlag(SymbolFlags.IsParameter)
                && paramSym.DeclarationNode is Parameter { HasAsClause: false } implicitStruct
                && SplitNameAndSuffix(implicitStruct.OriginalName.Lexeme).SuffixType == null)
            {
                program.Symbols.UpdateSymbolType(structVar.SymbolId, new AnyType());
                context.OnTypeInferred?.Invoke();
            }
        }

        // Fase 1: Riscrivi ricorsivamente la struttura base dell'accesso.
        var rewrittenStructure = RewriteExpression(fae.Structure, context, program);

        // --- ALIAS DI NAMESPACE ---
        if (rewrittenStructure is VariableExpr { SymbolId: < 0 } baseVar)
        {
            var aliasName = baseVar.NameCleaned.Lexeme;
            var currentModule = program.LoadedModules[context.ModulePath];
            var import = currentModule.Imports.FirstOrDefault(i =>
                i.Mode == ImportMode.Namespace &&
                (i.Alias?.Lexeme ?? i.NamespaceName.Lexeme).Equals(aliasName, StringComparison.OrdinalIgnoreCase));

            if (import != null)
            {
                var namespaceName = import.NamespaceName.Lexeme;
                var memberName = fae.Field.Lexeme;

                var symbolIdsInNamespace = program.Symbols.GetSymbolIdsByNamespace(namespaceName);
                if (symbolIdsInNamespace != null)
                {
                    var memberCandidates = symbolIdsInNamespace
                        .Select(id => program.Symbols.GetSymbol(id))
                        .Where(s => s.OriginalName.Equals(memberName, StringComparison.OrdinalIgnoreCase) && s.IsPublic)
                        .ToList();

                    if (memberCandidates.Count == 1)
                    {
                        var foundSymbol = memberCandidates[0];
                        // Ridispatch attraverso RewriteExpression (non un semplice "return new
                        // VariableExpr(...)"): una COSTANTE di namespace (es. C.RED con IMPORT
                        // COLOR AS C) deve passare dal Constant Folding di RewriteVariableExpr
                        // per diventare un letterale — altrimenti il nodo resta un riferimento a
                        // variabile "vuoto" a runtime (nessuna variabile C.RED esiste davvero),
                        // stampando/valutando vuoto ovunque tranne che in PRINT a top-level.
                        return RewriteExpression(
                            new VariableExpr(foundSymbol.Id, foundSymbol.OriginalToken, foundSymbol.OriginalToken),
                            context, program);
                    }

                    if (memberCandidates.Count > 1)
                    {
                        throw CreateError(fae.Field,
                            $"Il riferimento al membro '{memberName}' è ambiguo nel namespace '{namespaceName}'.");
                    }
                }

                throw CreateError(fae.Field,
                    $"Il simbolo '{memberName}' non è un membro pubblico del namespace '{namespaceName}'.");
            }
        }

        // --- GESTIONE ALIAS DBF ---
        if (rewrittenStructure is VariableExpr baseVariable)
        {
            var aliasName = baseVariable.NameCleaned.Lexeme;
            if (context.DbfState.AliasToUdtSymbolId.TryGetValue(aliasName, out var udtSymbolId) && udtSymbolId > 0)
            {
                var udtSymbol = program.Symbols.GetSymbol(udtSymbolId);
                if (udtSymbol.DeclarationNode is UserDefinedType udt)
                {
                    var fieldName = fae.Field.Lexeme;
                    var field = udt.Fields.FirstOrDefault(f =>
                        f.Name.Lexeme.Equals(fieldName, StringComparison.OrdinalIgnoreCase));
                    if (field != null)
                    {
                        return fae with { Structure = rewrittenStructure, FieldSymbolId = field.SymbolId };
                    }
                    else
                    {
                        throw CreateError(fae.Field,
                            $"Il campo '{fieldName}' non è stato trovato nel tipo '{udt.Name.Lexeme}' associato all'alias '{aliasName}'.");
                    }
                }
            }
        }

        // NB: `program.Namespaces` contiene SOLO i namespace built-in + DefaultNamespaces + IMPORTati +
        // esterni: un namespace UTENTE dichiarato (NAMESPACE X) e usato qualificato (X.CONST) SENZA import
        // non c'e' -> il gate falliva e X.CONST restava un FieldAccessExpr grezzo (valore 0). Si accetta
        // anche un namespace presente nell'indice del symbol table.
        if (rewrittenStructure is VariableExpr varBase &&
            (program.Namespaces.Contains(varBase.NameCleaned.Lexeme) ||
             program.Symbols.GetSymbolIdsByNamespace(varBase.NameCleaned.Lexeme) is not null))
        {
            // Stesso caso della sezione "ALIAS DI NAMESPACE" sopra, ma qui 'COLOR' ha già un
            // SymbolId >= 0 (riconosciuto direttamente come namespace, non tramite lookup import),
            // quindi il branch precedente (condizionato su SymbolId < 0) non scatta mai per questo
            // caso — che è comunissimo (IMPORT COLOR seguito da COLOR.RED, senza alias). Senza
            // questa risoluzione, COLOR.RED restava un FieldAccessExpr grezzo fino al Compiler, che
            // non ha nessun caso speciale per COLOR (solo KEY.* lo ha, vedi
            // VisitFieldAccessExpression) — a runtime tentava di caricare una variabile "COLOR"
            // mai esistita, risultando vuoto ovunque tranne che in PRINT a top-level.
            var namespaceName = varBase.NameCleaned.Lexeme;
            var memberName = fae.Field.Lexeme;
            var symbolIdsInNamespace = program.Symbols.GetSymbolIdsByNamespace(namespaceName);
            if (symbolIdsInNamespace != null)
            {
                var constCandidates = symbolIdsInNamespace
                    .Select(id => program.Symbols.GetSymbol(id))
                    .Where(s => s.OriginalName.Equals(memberName, StringComparison.OrdinalIgnoreCase) && s.IsPublic &&
                                s.DeclarationNode is ConstDeclaration)
                    .ToList();

                if (constCandidates.Count == 1)
                {
                    var foundSymbol = constCandidates[0];
                    return RewriteExpression(
                        new VariableExpr(foundSymbol.Id, foundSymbol.OriginalToken, foundSymbol.OriginalToken),
                        context, program);
                }

                if (constCandidates.Count > 1)
                {
                    throw CreateError(fae.Field,
                        $"Il riferimento al membro '{memberName}' è ambiguo nel namespace '{namespaceName}'.");
                }
            }

            return fae with { Structure = rewrittenStructure };
        }

        if (rewrittenStructure is NumberExpr or StringExpr or BooleanExpr)
        {
            throw CreateError(fae.Field,
                $"Impossibile accedere al campo '{fae.Field.Lexeme}' perché la base '{fae.Structure}' è un valore costante.",
                ErrorCode.TypeMismatch);
        }

        // --- GESTIONE ENUM ---
        if (rewrittenStructure is VariableExpr { SymbolId: >= 0 } enumVarExpr)
        {
            var symbol = program.Symbols.GetSymbol(enumVarExpr.SymbolId);
            if (symbol.Flags.HasFlag(SymbolFlags.IsEnum))
            {
                var memberName = fae.Field.Lexeme;
                var qualifiedMemberName = $"{symbol.OriginalName}.{memberName}";
                if (program.ResolvedConstants.TryGetValue(qualifiedMemberName, out var constValue))
                {
                    return new NumberExpr(Convert.ToDouble(constValue), fae.Field);
                }

                if (symbol.DeclarationNode is EnumDefinition enumDef)
                {
                    throw CreateError(fae.Field,
                        $"Membro '{memberName}' non trovato nell'enum '{enumDef.Name.Lexeme}'.");
                }

                throw CreateError(fae.Field, $"Membro enum '{memberName}' non risolvibile.");
            }
        }

        // --- GESTIONE CAMPI TUPLE NOMINATI ---
        // t.x → t(0) quando t è una tupla con campo nominato "x"
        if (rewrittenStructure is VariableExpr { SymbolId: >= 0 } tupleVarExpr)
        {
            var tupleSymbol = program.Symbols.GetSymbol(tupleVarExpr.SymbolId);
            if (tupleSymbol.DataType is TupleType tupleType)
            {
                var fieldName = fae.Field.Lexeme;
                var fieldIndex = -1;
                for (var i = 0; i < tupleType.Elements.Length; i++)
                {
                    if (tupleType.Elements[i].Name != null &&
                        tupleType.Elements[i].Name.Equals(fieldName, StringComparison.OrdinalIgnoreCase))
                    {
                        fieldIndex = i;
                        break;
                    }
                }
                if (fieldIndex >= 0)
                {
                    var indexExpr = new NumberExpr(fieldIndex, fae.Field);
                    return new ArrayAccessExpr(
                        tupleVarExpr.SymbolId,
                        tupleVarExpr.NameSuffixed,
                        tupleVarExpr.NameCleaned,
                        [indexExpr]
                    ) { ScopeId = fae.ScopeId };
                }
            }
        }

        var rewrittenFae = fae with { Structure = rewrittenStructure };
        ClassDefinition? currentClass = null;
        var scopeCursor = fae.ScopeId;
        while (scopeCursor != -1)
        {
            var scope = program.Symbols.GetScope(scopeCursor);
            if (scope.Type == SymbolTable.ScopeType.Class)
            {
                var classSymbolId = scope.OwnerSymbolId;
                if (classSymbolId >= 0)
                {
                    currentClass = program.Symbols.GetSymbol(classSymbolId).DeclarationNode as ClassDefinition;
                }

                break;
            }

            scopeCursor = scope.ParentId;
        }

        if (rewrittenStructure is BaseExpr baseExpr)
        {
            if (currentClass == null)
                throw CreateError(baseExpr.Token, "'BASE' può essere usato solo all'interno di un metodo di classe.");
            var baseClass = currentClass.ResolvedBaseClass ?? throw CreateError(baseExpr.Token,
                $"La classe '{currentClass.Name.Lexeme}' non ha una classe base.");
            var (foundMember, ownerClass) = FindFieldInHierarchy(baseClass, fae.Field.Lexeme);
            if (foundMember == null)
                throw CreateError(fae.Field,
                    $"Nessun membro accessibile '{fae.Field.Lexeme}' trovato nella classe base '{baseClass.Name.Lexeme}'.");
            if (foundMember.Access == AccessModifier.Private && ownerClass != currentClass)
                throw CreateError(fae.Field,
                    $"Il membro '{fae.Field.Lexeme}' è privato di '{ownerClass?.Name.Lexeme}' e non è accessibile.",
                    ErrorCode.SyntaxError);
        }

        // --- FASE 3: VALIDAZIONE PER TIPI BUILT-IN E TIPI UTENTE ---
        var structureType = LinkerInferExpressionType(rewrittenStructure, context, program);
        var resolvedStructureType = ResolveDataType(structureType, program, context.ModulePath);

        // CORREZIONE CHIRURGICA: Gestione dei tipi di sistema (es. HTTPPOSTEDDATA)
        if (TypeSchemaRegistry.TryGetValue(resolvedStructureType.GetType(), out var schema))
        {
            if (schema!.Fields.ContainsKey(fae.Field.Lexeme))
            {
                // Il campo esiste nello schema. Il nodo è valido.
                // Lasciamo FieldSymbolId a -1, ma arricchiamo il nodo con il tipo risolto.
                return rewrittenFae with { ResolvedType = schema.Fields[fae.Field.Lexeme] };
            }
            // Se il campo non esiste nello schema, la logica prosegue e lancerà un errore più avanti se necessario.
        }

        if (structureType is InterfaceType interfaceType)
        {
            var member = interfaceType.Definition.Members.FirstOrDefault(m =>
                m.Name.Lexeme.Equals(fae.Field.Lexeme, StringComparison.OrdinalIgnoreCase));
            if (member is PropertyMember) return new PropertyAccessExpr(rewrittenStructure, fae.Field);
            if (member is null)
                throw CreateError(fae.Field,
                    $"Membro '{fae.Field.Lexeme}' non trovato nell'interfaccia '{interfaceType.Definition.Name.Lexeme}'.");
        }

        var isPropertyAccess = false;
        var fieldSymbolId = -1;
        PropertyDefinition? resolvedPropDef = null;

        switch (structureType)
        {
            case ClassType classType:
                var (foundMember, _) = FindFieldInHierarchy(classType.Definition, fae.Field.Lexeme);
                if (foundMember != null)
                {
                    (fieldSymbolId, _) = foundMember switch
                    {
                        FieldMember fm => (fm.Declaration.SymbolId, fm.Declaration.DataType),
                        PropertyMember pm => (pm.Property.SymbolId, pm.Property.Type),
                        _ => (-1, null)
                    };
                    if (foundMember is PropertyMember) { isPropertyAccess = true; resolvedPropDef = ((PropertyMember)foundMember).Property; }
                }
                else
                {
                    throw CreateError(fae.Field,
                        $"Nessun campo o proprietà '{fae.Field.Lexeme}' trovato nella classe '{classType.Definition.Name.Lexeme}'.");
                }

                break;
            case UserDefinedType udt:
                var field = udt.Fields.FirstOrDefault(f =>
                    f.Name.Lexeme.Equals(fae.Field.Lexeme, StringComparison.OrdinalIgnoreCase));
                var prop = udt.Properties.FirstOrDefault(p =>
                    p.Name.Lexeme.Equals(fae.Field.Lexeme, StringComparison.OrdinalIgnoreCase));
                if (prop != null) isPropertyAccess = true;
                else if (field != null) fieldSymbolId = field.SymbolId;
                else
                    throw CreateError(fae.Field,
                        $"Nessun campo o proprietà '{fae.Field.Lexeme}' trovato nel tipo '{udt.Name.Lexeme}'.");
                break;
        }

        if (isPropertyAccess)
        {
            return new PropertyAccessExpr(rewrittenStructure, fae.Field)
            {
                ResolvedDefinition = resolvedPropDef
            };
        }

        // '.VALUE' su una base che non è un UDT/Classe/tipo di sistema (es. `d.VALUE` con
        // `d AS DOUBLE?`) è l'unwrap del nullable, non un campo. Non possiamo distinguerlo da un
        // campo UDT/CLASS letteralmente chiamato "Value" guardando solo il tipo staticamente
        // dichiarato (l'informazione "nullable" di una variabile non sopravvive al binding, che
        // registra il simbolo con il tipo sottostante), quindi la disambiguazione avviene per
        // priorità: se lo switch sopra ha già risolto un campo/proprietà reale (UDT/Classe), quello
        // vince; altrimenti, se il nome del membro è la keyword VALUE, trattalo come nullable unwrap.
        // ValidateFieldAccess (Linker.Visitors.cs) applica la stessa eccezione, quindi se arriviamo
        // qui con `fieldSymbolId == -1` e il campo non è VALUE è già stato intercettato a monte.
        // ECCEZIONE: `.Value` su un tipo DINAMICO (JSON/ANY — es. l'entry di un FOR EACH a una
        // variabile su un DICT, tipata JsonType dal backend) è un CAMPO del dict (JsonFieldGet
        // "Value"), NON l'unwrap di un nullable: riscriverlo a NullableValueExpr emetteva solo la
        // struttura (entry intera) invece del valore del dict (LoopsForEach/AM fe dict).
        if (fieldSymbolId == -1 && fae.Field.Kind == TokenKind.Value)
        {
            var dynBaseType = LinkerInferExpressionType(rewrittenStructure, context, program);
            if (dynBaseType is not JsonType and not AnyType and not InferredType)
            {
                return new NullableValueExpr(rewrittenStructure, fae.Field) { ScopeId = fae.ScopeId };
            }
        }

        return rewrittenFae with { FieldSymbolId = fieldSymbolId };
    }

    /// <summary>
    /// Riscrive un nodo ArrayAccessExpr, gestendo l'ambiguità tra chiamata a funzione e accesso ad array,
    /// la creazione implicita di variabili per parametri BYREF, e la risoluzione dei suffissi di tipo.
    /// Ora supporta anche le chiamate indirette tramite variabili di tipo FunctionPointerType.
    /// </summary>
    private static Expr RewriteArrayAccess(ArrayAccessExpr aae, AstRewriteContext context, BasicProgram program)
    {
        const bool log = false;
        var symbolId = aae.SymbolId;
        var symbolName = aae.NameCleaned.Lexeme;

        Log($"[RewriteArrayAccess] Inizio riscrittura per '{symbolName}(...)', ID iniziale: {symbolId}", log);

        // Dentro un metodo di classe, `arr(i)` su un nome NUDO che è un campo-array della
        // classe corrente va trattato come `SELF.arr(i)` (il parser produce ArrayAccessExpr
        // perché non conosce i campi). Riscritto in IndexedAccessExpr(SELF.campo, indici) e
        // ridispatchato: SELF + campo vengono risolti dal normale RewriteFieldAccess.
        if (context.CurrentClass is { } ccArr)
        {
            var (bareArr, _) = SplitNameAndSuffix(symbolName);
            var (memberArr, _) = FindFieldInHierarchy(ccArr, bareArr);
            if (memberArr is FieldMember fmArr && !fmArr.Declaration.Dimensions.IsEmpty)
            {
                var selfTok = new Token(TokenKind.Self, "SELF", null, aae.Position);
                var selfIdArr = FindSymbolIdInScopeChain("SELF", aae.ScopeId, aae.Position, program);
                var selfVar = new VariableExpr(selfIdArr, selfTok, selfTok) { ScopeId = aae.ScopeId };
                var fieldAccess = new FieldAccessExpr(selfVar, aae.NameCleaned) { ScopeId = aae.ScopeId };
                var indexed = new IndexedAccessExpr(fieldAccess, aae.Indices, aae.IsSafe) { ScopeId = aae.ScopeId };
                return RewriteExpression(indexed, context, program);
            }
        }

        // --- CASO 1: L'ID è già stato risolto dal Resolver a una PROCEDURA ---
        if (symbolId >= 0 &&
            program.Symbols.GetSymbol(symbolId).DeclarationNode is ProcedureDefinition resolvedProcFromId)
        {
            // Bare call a un METODO della PROPRIA classe (es. `B()` dentro `A()`): il parser lo
            // produce come ArrayAccessExpr perché non conosce i metodi. Stessa convenzione di
            // `SELF.B()` → riscrivi in MethodCallExpr(SELF, name, args) così TUTTI i backend
            // passano il receiver (IL lo faceva implicitamente, PCode/VM-C no).
            if (context.CurrentClass is { } ccBare &&
                (resolvedProcFromId.Flags & SymbolFlags.IsMethod) == SymbolFlags.IsMethod &&
                resolvedProcFromId.Parent is ClassDefinition pcBare &&
                string.Equals(pcBare.Name.Lexeme, ccBare.Name.Lexeme, System.StringComparison.OrdinalIgnoreCase))
            {
                var selfTokB = new Token(TokenKind.Self, "SELF", null, aae.Position);
                var selfIdB = FindSymbolIdInScopeChain("SELF", aae.ScopeId, aae.Position, program);
                var selfVarB = new VariableExpr(selfIdB, selfTokB, selfTokB) { ScopeId = aae.ScopeId };
                var mceBare = new MethodCallExpr(selfVarB, aae.NameCleaned, aae.Indices)
                    { ScopeId = aae.ScopeId, ResolvedDefinition = resolvedProcFromId };
                return RewriteExpression(mceBare, context, program);
            }

            Log(
                $"[RewriteArrayAccess] Rilevata chiamata a procedura già risolta (ID {symbolId}). Delega a RewriteFunctionExpr.",
                log);
            var funcExpr = new FunctionExpr(resolvedProcFromId.SymbolId, aae.NameCleaned, aae.Indices,
                    resolvedProcFromId is FunctionDefinition fd ? fd.ReturnType : new VoidType())
                { ScopeId = aae.ScopeId };
            return RewriteFunctionExpr(funcExpr, context, program);
        }

        // --- CASO 2: L'ID non è risolto. Dobbiamo cercare cosa sia. ---
        if (symbolId < 0)
        {
            // SHADOWING PER SCOPE, PRIMA del dispatch ai nomi importati: uno STORAGE
            // dichiarato nello scope (variabile/array/parametro) rende `nome(...)` un
            // ACCESSO ALL'ELEMENTO e vince su una procedura omonima visibile via IMPORT.
            // Il lookup è position-aware e preferisce i simboli-valore (regola "dichiarato
            // prima"): `DIM voci` seguito da `voci(0)` è l'elemento 0 della variabile, non
            // una chiamata alla procedura omonima importata (bug scoperto scrivendo l'app
            // DigUp: `list(0)` restituiva AI.List(0)). Senza questo ordine la procedura
            // importata rispondeva sempre, perché cercata per prima.
            var storageId = FindSymbolIdInScopeChain(symbolName, aae.ScopeId, aae.Position, program);
            if (storageId != -1 &&
                program.Symbols.GetSymbol(storageId).DeclarationNode is not ProcedureDefinition)
            {
                symbolId = storageId;
                Log($"[RewriteArrayAccess] Shadowing: '{symbolName}' è lo storage ID {storageId}.", log);
            }

            var candidates = symbolId < 0
                ? FindSymbolInImportedNamespaces(symbolName, context, program)
                : [];
            var compatibleProcCandidates = candidates
                .Select(s => s.DeclarationNode)
                .OfType<ProcedureDefinition>()
                .Where(p => SymbolTable.IsProcedureSignatureCompatible(p, aae.Indices.Length))
                // IDENTITÀ PER TOKENKIND (§12, regola d'oro): il lexeme è solo la grafia.
                // Se il nome nel sorgente ha un Kind specifico (keyword, es. `LIST` →
                // TokenKind.List) risponde SOLO la procedura registrata con lo stesso Kind:
                // `AI.List` ha BuiltinKind `AiList`, quindi non è `LIST` e non si chiama in
                // forma nuda (serve `AI.LIST(...)`). Per i nomi con Kind Identifier il nome
                // resta l'unica chiave possibile (comportamento invariato).
                .Where(p => aae.NameCleaned.Kind == TokenKind.Identifier
                            || (p is NativeProcedureDefinition npd ? npd.BuiltinKind : p.OriginalName.Kind)
                            == aae.NameCleaned.Kind)
                .ToList();

            if (compatibleProcCandidates.Count > 0)
            {
                // OVERLOAD dello stesso builtin (stesso BuiltinKind, stesso namespace): NON è
                // un'ambiguità da errore — è la stessa funzione con più firme (es. INSTR
                // canonica a 2/3 argomenti e variante Clipper `INSTR(start, haystack, needle)`),
                // e il runtime/backend la risolve dal TIPO degli argomenti. Prendiamo la prima
                // (parità con la risoluzione delle chiamate qualificate nel parser, che fa la
                // stessa scelta per i Kind identici). Prima `INSTR(1, sysText, "{")` faceva
                // fallire la compilazione con "La chiamata a 'INSTR' è ambigua" (rompeva
                // tools/basic/settings_bundle.bas, cioè il bundle settings di OGNI run C99/IL).
                var sameBuiltinOverloads = compatibleProcCandidates.Count > 1
                    && compatibleProcCandidates.All(c => c is NativeProcedureDefinition npd0
                        && compatibleProcCandidates[0] is NativeProcedureDefinition npd1
                        && npd0.BuiltinKind == npd1.BuiltinKind)
                    && compatibleProcCandidates
                        .Select(c => program.Symbols.GetNamespaceNameForSymbol(c.SymbolId))
                        .Distinct(StringComparer.OrdinalIgnoreCase).Count() == 1;
                if (compatibleProcCandidates.Count > 1 && !sameBuiltinOverloads)
                {
                    var namespaces = compatibleProcCandidates
                        .Select(p => program.Symbols.GetNamespaceNameForSymbol(p.SymbolId)).Distinct().ToList();
                    var nsList = string.Join(" e ", namespaces.Select(ns => $"'{ns}'"));
                    throw CreateError(aae.NameSuffixed,
                        $"La chiamata a '{symbolName}' è ambigua. Definizioni compatibili trovate in: {nsList}.",
                        ErrorCode.AmbiguousCall);
                }

                Log(
                    $"[RewriteArrayAccess] Trovata procedura utente '{symbolName}'. Trasformo in FunctionExpr e delego.",
                    log);
                var resolvedProc = compatibleProcCandidates[0];
                var funcExpr = new FunctionExpr(resolvedProc.SymbolId, aae.NameCleaned,
                        aae.Indices,
                        resolvedProc is FunctionDefinition fd ? fd.ReturnType : new VoidType())
                    { ScopeId = aae.ScopeId };
                return RewriteFunctionExpr(funcExpr, context, program);
            }

            Log($"[RewriteArrayAccess] Nessuna UDF trovata. Cerco variabile '{symbolName}'...", log);

            var varId = FindSymbolIdInScopeChain(symbolName, aae.ScopeId, aae.Position, program);
            if (varId != -1)
            {
                symbolId = varId;
                Log($"[RewriteArrayAccess] Trovata variabile con ID: {symbolId}", log);
            }
            else if (TryResolveBuiltinFunction(aae.NameCleaned, aae.Indices, program) is { } resolvedBuiltin)
            {
                Log($"[RewriteArrayAccess] Fallback a TryResolveBuiltinFunction ha avuto successo per '{symbolName}'.",
                    log);
                return RewriteExpression(resolvedBuiltin, context, program);
            }
            else
            {
                var (baseName, suffixType) = SplitNameAndSuffix(symbolName);
                if (suffixType != null)
                {
                    var baseVarId = FindSymbolIdInScopeChain(baseName, aae.ScopeId, aae.Position, program);
                    if (baseVarId != -1)
                    {
                        var symbol = program.Symbols.GetSymbol(baseVarId);
                        var symbolBaseType = symbol.DataType is ArrayType at ? at.ElementType : symbol.DataType;
                        if (AreSameType(symbolBaseType, suffixType, context, program))
                        {
                            symbolId = symbol.Id;
                            Log($"[RewriteArrayAccess] Risolto tramite suffisso. ID: {symbolId}", log);
                        }
                    }
                }
            }

            if (symbolId < 0)
            {
                var tempVarExpr = new VariableExpr(-1, aae.NameSuffixed, aae.NameCleaned) { ScopeId = aae.ScopeId };
                var lateBindingResult = TryResolveLateBinding(tempVarExpr, context, program);
                if (lateBindingResult != null)
                {
                    Log($"[RewriteArrayAccess] Risolto tramite Late Binding come campo DBF: '{symbolName}'.", log);
                    var rewrittenIndicesForLate = aae.Indices.Select(i => RewriteExpression(i, context, program))
                        .ToImmutableArray();
                    return new IndexedAccessExpr(lateBindingResult, rewrittenIndicesForLate, aae.IsSafe) { ScopeId = aae.ScopeId };
                }
            }

            // Builtin bare senza prefisso: COUNT(arr) → SYSTEM.COUNT(arr), stessa
            // riscrittura del metodo fluente `arr.Count()` (RewriteMemberCall). COUNT è
            // una keyword del linguaggio, quindi non può collidere con variabili o UDF.
            if (symbolId < 0 && aae.NameCleaned.Kind == TokenKind.Count && aae.Indices.Length == 1)
            {
                var countToken = new Token(TokenKind.Count, "COUNT", null, aae.NameCleaned.Position);
                var nsToken = new Token(TokenKind.Identifier, "SYSTEM", null, aae.NameCleaned.Position);
                var countArgs = aae.Indices.Select(i => RewriteExpression(i, context, program)).ToImmutableArray();
                return new BuiltinFunctionCallExpr(nsToken, countToken, countArgs, new DoubleType())
                    { ScopeId = aae.ScopeId };
            }

            /* CHIAMATA NUDA a funzione di libreria SENZA argomenti: `LEFT()`, `RIGHT()`, `X()`,
             * `CLICK()`, … Il parser la rappresenta come accesso indicizzato con 0 indici (una
             * keyword non è un identificatore). La leg C risolve questi nomi per ARITÀ (LEFT() a 0
             * argomenti = stato del tasto, LEFT(s,n) = funzione stringa): qui si cerca una
             * NativeProcedureDefinition a ZERO parametri visibile nello scope e la si emette come
             * chiamata al builtin (TokenKind dedicato, es. MouseLeft → IlRuntime.MouseLeft). */
            /* `STRING$(n, ch)` — funzione classica BASIC: n copie del CARATTERE `ch`.
             * Si riscrive in DUP(n, ch), che è la funzione con la STESSA semantica su tutte le
             * gambe:
             *   - `ch` STRINGA → n copie del suo PRIMO carattere ("ab" → "aaa");
             *   - `ch` NUMERO  → n copie di CHR(codice)   (65 → "AAA").
             * Prima si riscriveva in REPEAT(ch, n), che ha un'altra semantica (ripete l'INTERA
             * stringa) e con un numero produceva IL invalido sulla gamba IL
             * (InvalidProgramException) e "" sulle altre. Bug trovato dal MiniTest
             * strings/string_dollar.bas e da http/config_limits.bas (STRING$(n, "x") non
             * compilava sulla gamba C99). */
            if (symbolId < 0 && aae.Indices.Length == 2
                && (aae.NameCleaned.Lexeme.Equals("STRING$", StringComparison.OrdinalIgnoreCase)
                    || aae.NameCleaned.Lexeme.Equals("STRING", StringComparison.OrdinalIgnoreCase)))
            {
                var dupTok = new Token(TokenKind.Dup, "DUP", null, aae.NameCleaned.Position);
                var dupNs = new Token(TokenKind.Identifier, "STRINGS", null, aae.NameCleaned.Position);
                var dupArgs = ImmutableArray.Create(
                    RewriteExpression(aae.Indices[0], context, program),
                    RewriteExpression(aae.Indices[1], context, program));
                return new BuiltinFunctionCallExpr(dupNs, dupTok, dupArgs, new StringType())
                    { ScopeId = aae.ScopeId };
            }

            if (symbolId < 0)
            {
                var builtinByName = program.Symbols.FindSymbolsByName(aae.NameCleaned.Lexeme)
                    .Select(sp => sp.DeclarationNode)
                    .OfType<NativeProcedureDefinition>()
                    .FirstOrDefault(pr => SymbolTable.IsProcedureSignatureCompatible(pr, aae.Indices.Length)
                                          && (pr.OriginalName.Lexeme.Equals(aae.NameCleaned.Lexeme,
                                                  StringComparison.OrdinalIgnoreCase)
                                              || pr.OriginalName.Lexeme.Equals(
                                                  aae.NameCleaned.Lexeme.TrimEnd('$'),
                                                  StringComparison.OrdinalIgnoreCase)));
                if (builtinByName is not null)
                {
                    Log($"[RewriteArrayAccess] '{aae.NameCleaned.Lexeme}(...)' risolto come builtin ({builtinByName.BuiltinKind}).", log);
                    var nsTok = new Token(TokenKind.Identifier, "BUILTIN", null, aae.NameCleaned.Position);
                    var fnTok = new Token(builtinByName.BuiltinKind, builtinByName.OriginalName.Lexeme, null,
                        aae.NameCleaned.Position);
                    var boundIndices = aae.Indices.Select(i => RewriteExpression(i, context, program))
                        .ToImmutableArray();
                    return new BuiltinFunctionCallExpr(nsTok, fnTok, boundIndices,
                        builtinByName.ReturnType) { ScopeId = aae.ScopeId };
                }
            }

            if (symbolId < 0)
            {
                // Nome nudo di una funzione di libreria in namespace NON caricato: hint distinto
                // (a link-time lo scope è completo → nessuno shadowing con FUNCTION/SUB utente,
                // che sarebbero già risolte sopra). MAI il generico "Simbolo non definito".
                var bareNm = aae.NameCleaned.Lexeme;
                if (bareNm.EndsWith('$')) bareNm = bareNm[..^1];
                var notImportedNs = KnownLibraryNotImported(program, bareNm);
                if (notImportedNs is { Count: > 0 })
                    throw CreateError(aae.NameSuffixed,
                        $"Simbolo non trovato: '{bareNm}' è una funzione di libreria in un namespace non importato ({string.Join(", ", notImportedNs)}). " +
                        $"Usa la forma qualificata <NS>.{bareNm}(...) o aggiungi IMPORT <NS>.",
                        ErrorCode.UndefinedVariable);

                Log($"[RewriteArrayAccess] FALLIMENTO FINALE: simbolo '{symbolName}' non definito.", log);
                throw CreateError(aae.NameSuffixed, $"Simbolo non definito: '{symbolName}'.",
                    ErrorCode.UndefinedVariable);
            }
        }

        // --- CASO 3: L'ID è risolto a una VARIABILE (Array, Collection, Function Pointer) ---
        var rewrittenIndices = aae.Indices.Select(i => RewriteExpression(i, context, program)).ToImmutableArray();

        var finalSymbol = program.Symbols.GetSymbol(symbolId);

        // BUG FIX: FunctionPointerType con 0 argomenti deve creare FunctionExpr,
        // non VariableExpr (che non invocherebbe la funzione).
        if (finalSymbol.DataType is FunctionPointerType fpt)
        {
            if (fpt.ParameterTypes.Length != rewrittenIndices.Length)
            {
                throw CreateError(aae.NameSuffixed,
                    $"Numero di argomenti non valido per la chiamata indiretta a '{finalSymbol.OriginalName}'. " +
                    $"Attesi {fpt.ParameterTypes.Length}, forniti {rewrittenIndices.Length}.",
                    ErrorCode.ArgumentCountMismatch);
            }

            return new FunctionExpr(symbolId, aae.NameCleaned, rewrittenIndices, fpt.ReturnType)
                { ResolvedDefinition = null, IsIndirect = true };
        }

        // Se non ci sono indici, la sintassi `Nome()` è un riferimento all'intero array.
        // Lo trasformiamo nel nodo semanticamente corretto: un VariableExpr.
        if (rewrittenIndices.IsEmpty)
        {
            return new VariableExpr(symbolId, aae.NameSuffixed, aae.NameCleaned) { ScopeId = aae.ScopeId };
        }

        if (finalSymbol.DataType is GeneratorType genT)
        {
            return new FunctionExpr(symbolId, aae.NameCleaned, rewrittenIndices, genT)
                { ResolvedDefinition = finalSymbol.DeclarationNode as ProcedureDefinition };
        }

        var isIndexableType = finalSymbol.DataType is ArrayType or TupleType or ListType or DictType or HashSetType or QueueType
            or StackType or SortedDictType or TreeType or AVLTreeType or RedBlackTreeType
            or BinaryIndexedTreeType or TrieType or LinkedListType or CircularLinkedListType
            or BinaryHeapType or BagType or CacheType or GraphType or TimelineType
            or RingBufferType or SparseSetType or BPlusTreeType or JsonType
            or SqlResultSetType
            or FunctionPointerType or InferredType or AnyType or DoubleType;

        if (!isIndexableType && !finalSymbol.Flags.HasFlag(SymbolFlags.IsArray))
        {
            // Check if this is actually a function call shadowed by a variable
            var funcCandidates = program.Symbols.FindSymbolsByName(finalSymbol.OriginalName)
                .Where(s => s.DeclarationNode is ProcedureDefinition && s.Id != finalSymbol.Id)
                .ToList();
            if (funcCandidates.Count > 0)
            {
                var proc = (ProcedureDefinition)funcCandidates[0].DeclarationNode!;
                var fe = new FunctionExpr(funcCandidates[0].Id, aae.NameCleaned, rewrittenIndices,
                    (proc as FunctionDefinition)?.ReturnType ?? new InferredType())
                { ResolvedDefinition = proc, ScopeId = aae.ScopeId };
                return fe;
            }
            throw CreateError(aae.NameSuffixed,
                    $"La variabile '{finalSymbol.OriginalName}' non è un array, una collezione o un puntatore a " +
                    $"funzione e non può essere indicizzata/chiamata.",
                    ErrorCode.ExpectedArray);
        }

        return aae with { SymbolId = symbolId, Indices = rewrittenIndices };
    }

    /// <summary>
    /// Riscrive un nodo UnresolvedCallExpr, tentando di risolvere l'ambiguità
    /// o di trovare una corrispondenza di firma valida.
    /// QUESTA VERSIONE È ARCHITETTURALMENTE CORRETTA: si affida all'helper
    /// FindProcedureCandidatesInScope per la logica di visibilità e risoluzione.
    /// </summary>
    private static Expr RewriteUnresolvedCall(UnresolvedCallExpr uce, AstRewriteContext context, BasicProgram program)
    {
        var functionName = uce.Name.Lexeme;
        var argsCount = uce.Arguments.Length;

        var candidates = FindProcedureCandidatesInScope(functionName, context, program);
        var compatibleCandidates =
            candidates.Where(p => SymbolTable.IsProcedureSignatureCompatible(p, argsCount)).ToList();

        if (compatibleCandidates.Count == 1)
        {
            var resolvedProc = compatibleCandidates[0];

            // Usiamo la logica custom per BYREF impliciti e Lambda Context
            var finalArgsBuilder = ImmutableArray.CreateBuilder<Expr>();
            for (var i = 0; i < argsCount; i++)
            {
                var arg = uce.Arguments[i];
                var param = resolvedProc.Parameters[i];

                if (param.Flags.HasFlag(SymbolFlags.IsByRef) && arg is VariableExpr { SymbolId: < 0 } ve)
                {
                    // Logica BYREF Implicita
                    var existingIds = program.Symbols.FindSymbolInScope(ve.NameCleaned.Lexeme);
                    if (existingIds == null || existingIds.Count == 0)
                    {
                        var newId = program.Symbols.RegisterSymbol(ve.NameCleaned, SymbolFlags.IsLocal,
                            new InferredType(), ve);
                        finalArgsBuilder.Add(ve with { SymbolId = newId });
                        continue;
                    }
                }

                if (arg is LambdaExpr lambda && param.DataType is FunctionPointerType fpt)
                {
                    // Logica Lambda Context
                    var lambdaScope = new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase);
                    for (var k = 0; k < lambda.Parameters.Length && k < fpt.ParameterTypes.Length; k++)
                    {
                        lambdaScope[lambda.Parameters[k].OriginalName.Lexeme] = fpt.ParameterTypes[k];
                    }

                    var lambdaContext = context with { LambdaScope = lambdaScope };
                    finalArgsBuilder.Add(RewriteExpression(lambda, lambdaContext, program));
                }
                else
                {
                    finalArgsBuilder.Add(RewriteExpression(arg, context, program));
                }
            }

            // --- GENERIC SPECIALIZATION ---
            // Specializza solo se i type parameters sono effettivamente usati.
            if (resolvedProc is FunctionDefinition genericFd && !genericFd.TypeParameters.IsDefaultOrEmpty && GenericUsesTypeParameters(genericFd))
            {
                var typeArgs = InferTypeArguments(genericFd, finalArgsBuilder.ToImmutable(), context, program);
                if (typeArgs != null)
                {
                    var specialized = SpecializeGenericFunction(genericFd, typeArgs, program);
                    var specId = program.Symbols.RegisterSymbol(
                        specialized.Name,
                        genericFd.Flags | SymbolFlags.IsFunction,
                        specialized.ReturnType,
                        specialized);
                    specialized = specialized with { SymbolId = specId };
                    // La DeclarationNode del simbolo deve essere la specializzazione CON l'id:
                    // RegisterSymbol registra l'istanza con SymbolId -1 e il rewrite dei simboli
                    // di alto livello (ResolveAndRewriteModuleAsts) la ripesca da lì →
                    // "Invalid SymbolId -1" sui programmi top-level (senza SUB Main, dove il
                    // simbolo specializzato è nello scope globale).
                    program.Symbols.UpdateSymbolDeclaration(specId, specialized);
                    program.ResolvedSpecializations[specId] = specialized;

                    var specExpr = new FunctionExpr(specId, uce.Name, finalArgsBuilder.ToImmutable(), specialized.ReturnType)
                        { ScopeId = uce.ScopeId };
                    return RewriteFunctionExpr(specExpr, context, program);
                }
            }

            var funcExpr = new FunctionExpr(resolvedProc.SymbolId, uce.Name, finalArgsBuilder.ToImmutable(),
                    resolvedProc is FunctionDefinition fd ? fd.ReturnType : new VoidType())
                { ScopeId = uce.ScopeId };

            // Ritorniamo direttamente RewriteFunctionExpr per gestire eventuali logiche residue (es. return type inference)
            // ma con gli argomenti già riscritti e il symbolID settato.
            return RewriteFunctionExpr(funcExpr, context, program);
        }

        // Se non troviamo candidati o sono ambigui, riscriviamo standard.
        var standardRewrittenArgs =
            uce.Arguments.Select(arg => RewriteExpression(arg, context, program)).ToImmutableArray();

        // CHOKE UNICO a fine link: un UnresolvedCallExpr che sopravvive alla risoluzione utente è
        // un ERORE certo (non trovata / ambigua / namespace non importato), MAI un residuo da
        // tollerare in codegen (bug: l'ambigua veniva inghiottita in silenzio nel percorso
        // pipeline/bytecode). Diagnostico distinto per esito (mai un errore generico).
        if (uce.Reason is not UnresolvedReason.FunctionNotFound and not UnresolvedReason.BuiltinNamespaceNotImported
            and not UnresolvedReason.AmbiguousBuiltinFunctions and not UnresolvedReason.AmbiguousUserFunctions
            and not UnresolvedReason.AmbiguousUserAndBuiltinFunction)
            return new UnresolvedCallExpr(uce.Name, standardRewrittenArgs, uce.ReturnType, uce.Reason, uce.Candidates)
                { ScopeId = uce.ScopeId };

        throw CreateError(uce.Name, UnresolvedCallDiagnostics.Build(uce, CandidateNamespaces(uce, program)));
    }

    /// <summary>Namespace dei candidati builtin di un unresolved (per il diagnostico "ambigua").</summary>
    private static IEnumerable<string> CandidateNamespaces(UnresolvedCallExpr uce, BasicProgram program)
        => uce.Candidates
            .OfType<NativeProcedureDefinition>()
            .Select(c => program.Symbols.GetNamespaceNameForSymbol(c.SymbolId))
            .Where(ns => !string.IsNullOrEmpty(ns))
            .Distinct()
            .ToList();

    /// <summary>Se `name` è una funzione di libreria nota (registry) ma i suoi namespace NON sono
    /// caricati (né default né IMPORT), ritorna l'elenco dei namespace per l'hint; altrimenti null.
    /// A link-time lo scope è completo: un'omonima FUNCTION/SUB utente si risolve prima (niente
    /// shadowing — regressione SciPy 'y' evitata).</summary>
    private static List<string>? KnownLibraryNotImported(BasicProgram program, string name)
    {
        var std = global::Basic.StdLib.Instance;
        if (string.IsNullOrEmpty(name) || !std.AllBuiltinFunctionNames.Contains(name)) return null;
        if (!std.FunctionNameToNamespacesMap.TryGetValue(name, out var nsList) || nsList.Count == 0) return null;
        var loaded = nsList.Any(ns =>
            (program.Namespaces?.Any(n => n.Equals(ns, StringComparison.OrdinalIgnoreCase)) ?? false) ||
            (program.Settings?.DefaultNamespaces?.Any(d => d.Equals(ns, StringComparison.OrdinalIgnoreCase)) ?? false));
        return loaded ? null : nsList.ToList();
    }

    /// <summary>
    /// Inferisce i type arguments dai tipi concreti degli argomanti.
    /// Per ogni type parameter T, cerca il primo parametro AS T e usa il tipo dell'argomento corrispondente.
    /// </summary>
    public static ImmutableDictionary<TypeParameter, DataType>? InferTypeArguments(
        FunctionDefinition genericFd, ImmutableArray<Expr> args, AstRewriteContext context, BasicProgram program)
    {
        var substitution = ImmutableDictionary.CreateBuilder<TypeParameter, DataType>();
        for (var i = 0; i < genericFd.Parameters.Length && i < args.Length; i++)
        {
            var paramType = genericFd.Parameters[i].DataType;
            if (paramType is TypeParameter tp && !substitution.ContainsKey(tp))
            {
                var argType = LinkerInferExpressionType(args[i], context, program);
                if (argType is not InferredType)
                {
                    substitution.Add(tp, argType);
                }
            }
        }
        // Verifica che tutti i type parameters siano stati inferiti
        foreach (var tp in genericFd.TypeParameters)
        {
            if (!substitution.ContainsKey(tp))
                return null; // Non è possibile inferire questo parametro
        }
        return substitution.ToImmutable();
    }

    /// <summary>
    /// Crea una copia specializzata di una funzione generica sostituendo i type parameters con i tipi concreti.
    /// </summary>
    private static FunctionDefinition SpecializeGenericFunction(
        FunctionDefinition genericFd, ImmutableDictionary<TypeParameter, DataType> typeArgs, BasicProgram program)
    {
        var newReturnType = SubstituteTypeParameters(genericFd.ReturnType, typeArgs);
        var newParams = genericFd.Parameters.Select(p =>
        {
            var newDataType = SubstituteTypeParameters(p.DataType, typeArgs);
            return p with { DataType = newDataType };
        }).ToImmutableArray();
        var newBody = genericFd.Body.Select(s => (Stmt)SubstituteInStatement(s, typeArgs)).ToImmutableArray();

        var specialized = new FunctionDefinition(
            -1,
            (Token)genericFd.Name.DeepClone(),
            newParams,
            newReturnType,
            newBody,
            genericFd.Flags,
            ImmutableArray<TypeParameter>.Empty
        )
        {
            OriginalName = (Token)genericFd.OriginalName.DeepClone()
        };

        return specialized;
    }

    /// <summary>
    /// Sostituisce i type parameters in un qualsiasi statement.
    /// </summary>
    private static AstNode SubstituteInStatement(Stmt stmt, ImmutableDictionary<TypeParameter, DataType> typeArgs)
    {
        // Per i statement semplici (non contengono DataType), restituisci così com'è
        return stmt;
    }

    /// <summary>
    /// Helper specifico per riscrivere un'istruzione `IfStmt`.
    /// QUESTA VERSIONE È ARCHITETTURALMENTE CORRETTA: Rimuove la dipendenza da `GetScopeForNode`
    /// e si affida al contesto di scope esistente, che è sufficiente per la riscrittura.
    /// </summary>
    private static (Stmt Rewritten, AstRewriteContext NewContext) RewriteIfStatement(IfStmt stmt,
        AstRewriteContext context, ProcedureDefinition? procedureContext, BasicProgram program)
    {
        // Il contesto di output sarà lo stesso di quello di input,
        // poiché lo stato del DBF non deve "fuoriuscire" da un blocco IF.
        var newContext = context;

        // 1. Riscrive le clausole IF e ELSEIF.
        var rewrittenIfClauses = ImmutableArray.CreateBuilder<IfClause>();
        foreach (var clause in stmt.IfClauses)
        {
            var rewrittenCondition = RewriteExpression(clause.Condition, context, program);

            // Riscrivi il corpo di questo ramo. Le modifiche al contesto fatte qui
            // sono temporanee e non influenzano i rami successivi.
            var branchContext = context;
            var bodyBuilder = ImmutableArray.CreateBuilder<Stmt>();
            foreach (var statement in clause.Statements)
            {
                (var rewrittenStmt, branchContext) =
                    RewriteStatement(statement, branchContext, procedureContext, program);
                bodyBuilder.Add(rewrittenStmt);
            }

            rewrittenIfClauses.Add(clause with
            {
                Condition = rewrittenCondition, Statements = bodyBuilder.ToImmutable()
            });
        }

        // 2. Riscrive la clausola ELSE, se esiste.
        ElseClause? rewrittenElseClause = null;
        if (stmt.ElseClause != null)
        {
            // Anche il ramo ELSE parte dal contesto originale.
            var elseContext = context;
            var elseBodyBuilder = ImmutableArray.CreateBuilder<Stmt>();
            foreach (var statement in stmt.ElseClause.Statements)
            {
                (var rewrittenStmt, elseContext) = RewriteStatement(statement, elseContext, procedureContext, program);
                elseBodyBuilder.Add(rewrittenStmt);
            }

            rewrittenElseClause = stmt.ElseClause with { Statements = elseBodyBuilder.ToImmutable() };
        }

        // 3. Restituisce l'IfStmt completamente riscritto.
        var rewrittenIf = stmt with { IfClauses = rewrittenIfClauses.ToImmutable(), ElseClause = rewrittenElseClause };
        return (rewrittenIf, newContext);
    }

    /// <summary>
    /// Helper specifico per riscrivere un'istruzione `GoProcedureStmt`.
    /// QUESTA VERSIONE È ARCHITETTURALMENTE CORRETTA.
    /// </summary>
    private static GoProcedureStmt RewriteGoProcedureStatement(GoProcedureStmt stmt, AstRewriteContext context,
        BasicProgram program)
    {
        // Fase 1: Riscrivi l'espressione di chiamata.
        // Questa chiamata delega a RewriteFunctionExpr (o simili), che risolverà la procedura
        // e popolerà il nodo con il SymbolId e la ResolvedDefinition corretti.
        var rewrittenCall = RewriteExpression(stmt.Call, context, program);

        // Fase 2: Valida il risultato della riscrittura.
        if (rewrittenCall is not FunctionExpr { ResolvedDefinition: not null } callExpr)
        {
            // Se, dopo la riscrittura, il nodo non è una chiamata a funzione risolta,
            // significa che l'espressione non era una chiamata a procedura valida.
            throw CreateError(
                new Token(TokenKind.Unknown, stmt.Call.ToString(), null, stmt.Call.Position),
                "GO statement must be followed by a valid procedure call.");
        }

        // A questo punto, la validazione dell'esistenza, della firma e dell'ambiguità
        // è già stata gestita da RewriteFunctionExpr o RewriteUnresolvedCall.
        // Non è necessario ripeterla.

        // Fase 3: Riscrivi l'espressione del WaitGroup, se presente.
        var rewrittenWaitGroup = stmt.WaitGroup != null
            ? RewriteExpression(stmt.WaitGroup, context, program)
            : null;

        // Fase 4: Restituisci il nodo Stmt finale con i figli riscritti.
        return stmt with
        {
            Call = callExpr,
            WaitGroup = rewrittenWaitGroup
        };
    }

    /// <summary>
    /// Helper specifico per validare un'istruzione `JumpStmt` (GOTO).
    /// QUESTA VERSIONE È ARCHITETTURALMENTE CORRETTA: convalida l'esistenza dell'etichetta
    /// e verifica che il salto non sia illegale tra scope procedurali diversi.
    /// </summary>
    private static JumpStmt RewriteJumpStatement(JumpStmt stmt, AstRewriteContext context,
        ProcedureDefinition? procedureContext, BasicProgram program)
    {
        var targetToken = stmt.TargetLabel;

        switch (targetToken.Kind)
        {
            case TokenKind.Identifier:
            {
                var labelName = targetToken.Lexeme;

                // Costruisce le chiavi per cercare l'etichetta prima nello scope della procedura, poi in quello del modulo.
                var procedureScopeKey =
                    SymbolTable.GetQualifiedLabelName(context.ModulePath, procedureContext, labelName);
                var moduleScopeKey = SymbolTable.GetQualifiedLabelName(context.ModulePath, null, labelName);

                var destination = program.Symbols.FindLabel(procedureScopeKey) ??
                                  program.Symbols.FindLabel(moduleScopeKey);

                if (destination == null)
                {
                    Log($"[REWRITER-ERROR] Label non trovata: '{targetToken.Lexeme}'.", RewriteLog);
                    Log($"[REWRITER-ERROR] Keys cercate: '{procedureScopeKey}', '{moduleScopeKey}'", RewriteLog);
                    throw CreateError(targetToken, $"Etichetta non definita: '{labelName}'.",
                        ErrorCode.UndefinedLine);
                }

                // Validazione del salto tra scope procedurali.
                // Un salto è illegale se la destinazione è in una procedura diversa da quella corrente,
                // oppure se si tenta di saltare da/verso uno scope di modulo a/da uno di procedura.
                var sourceProcId = procedureContext?.SymbolId;
                var destProcId = destination.Procedure?.SymbolId;

                if (sourceProcId == destProcId) return stmt;
                var sourceName = procedureContext?.Name.Lexeme ?? "scope del modulo";
                var destName = destination.Procedure?.Name.Lexeme ?? "scope del modulo";
                throw CreateError(targetToken,
                    $"Salto illegale: non è possibile usare GOTO per saltare tra procedure o tra una procedura e lo scope del modulo. " +
                    $"(da '{sourceName}' a '{destName}')");
            }
            case TokenKind.Number or TokenKind.LineNumber:
            {
                // Il token numerico di riga è stato parsato come NumberExpr con il suo valore letterale corretto.
                var lineNumber = (int)(double)targetToken.Literal!;

                // La validazione ora costruisce la chiave qualificata per il numero di riga
                // e cerca nella SymbolTable, invece di controllare l'AST.
                var key = SymbolTable.GetQualifiedLabelName(context.ModulePath, null, lineNumber.ToString());

                if (program.Symbols.FindLabel(key) == null)
                {
                    throw CreateError(targetToken, $"Numero di riga non definito per GOTO: {lineNumber}.",
                        ErrorCode.UndefinedLine);
                }

                break;
            }
        }

        // Il nodo non necessita di essere riscritto, solo validato.
        return stmt;
    }

    /// <summary>
    /// Helper specifico per validare un'istruzione `GosubStmt` (GOSUB).
    /// </summary>
    private static GosubStmt RewriteGosubStatement(GosubStmt stmt, AstRewriteContext context,
        ProcedureDefinition? procedureContext, BasicProgram program)
    {
        var targetToken = stmt.Target;

        switch (targetToken.Kind)
        {
            case TokenKind.Identifier:
            {
                var labelName = targetToken.Lexeme;

                // Costruisce le chiavi per cercare l'etichetta prima nello scope della procedura, poi in quello del modulo.
                var procedureScopeKey =
                    SymbolTable.GetQualifiedLabelName(context.ModulePath, procedureContext, labelName);
                var moduleScopeKey = SymbolTable.GetQualifiedLabelName(context.ModulePath, null, labelName);

                var destination = program.Symbols.FindLabel(procedureScopeKey) ??
                                  program.Symbols.FindLabel(moduleScopeKey);

                if (destination == null)
                {
                    throw CreateError(targetToken, $"Etichetta non definita per GOSUB: '{labelName}'.",
                        ErrorCode.UndefinedLine);
                }

                // Validazione del salto tra scope procedurali. La logica è identica a GOTO.
                var sourceProcId = procedureContext?.SymbolId;
                var destProcId = destination.Procedure?.SymbolId;

                if (sourceProcId == destProcId) return stmt;
                var sourceName = procedureContext?.Name.Lexeme ?? "scope del modulo";
                var destName = destination.Procedure?.Name.Lexeme ?? "scope del modulo";
                throw CreateError(targetToken,
                    $"Salto illegale: non è possibile usare GOSUB per saltare tra procedure o tra una procedura e lo scope del modulo. " +
                    $"(da '{sourceName}' a '{destName}')");
            }
            case TokenKind.Number or TokenKind.LineNumber:
            {
                var lineNumber = (int)(double)targetToken.Literal!;

                // La validazione non deve più controllare `module.Statements`.
                // Deve costruire la chiave qualificata per il numero di riga e cercare nella SymbolTable.
                var key = SymbolTable.GetQualifiedLabelName(context.ModulePath, null, lineNumber.ToString());

                if (program.Symbols.FindLabel(key) == null)
                {
                    throw CreateError(targetToken, $"Numero di riga non definito per GOSUB: {lineNumber}.",
                        ErrorCode.UndefinedLine);
                }

                break;
            }
        }

        // Il nodo non necessita di essere riscritto, solo validato.
        return stmt;
    }

    /// <summary>
    /// Riscrive un nodo OnJumpStmt (ON ... GOTO/GOSUB), risolvendo e validando tutte le etichette di destinazione.
    /// </summary>
    private static OnJumpStmt RewriteOnJumpStatement(OnJumpStmt stmt, AstRewriteContext context,
        ProcedureDefinition? procedureContext, BasicProgram program)
    {
        // 1. Riscrivi l'espressione indice (ON <indice>).
        var rewrittenIndex = RewriteExpression(stmt.IndexExpression, context, program);

        // 2. Valida il tipo dell'indice: deve essere un tipo intero.
        var indexType = LinkerInferExpressionType(rewrittenIndex, context, program);
        if (indexType is not IntegerType and not LongType and not InferredType)
        {
            throw CreateError(stmt.Token,
                $"L'espressione indice per ON {stmt.Type} deve essere un tipo intero, trovato '{indexType}'.",
                ErrorCode.TypeMismatch);
        }

        // 3. Risolvi e valida tutti i target.
        foreach (var targetToken in stmt.TargetLabels)
        {
            // La logica di risoluzione è la stessa di GOTO/GOSUB
            var targetName = targetToken.Lexeme;
            var isLineNumber = targetToken.Kind is TokenKind.Number or TokenKind.LineNumber;

            // Se è un'etichetta testuale, cerchiamo prima nello scope procedurale, poi nel modulo.
            if (!isLineNumber)
            {
                var procScopeKey =
                    SymbolTable.GetQualifiedLabelName(context.ModulePath, procedureContext, targetName);
                var moduleScopeKey = SymbolTable.GetQualifiedLabelName(context.ModulePath, null, targetName);

                var destination = program.Symbols.FindLabel(procScopeKey) ?? program.Symbols.FindLabel(moduleScopeKey);

                if (destination == null)
                {
                    throw CreateError(targetToken, $"Etichetta non definita per ON {stmt.Type}: '{targetName}'.",
                        ErrorCode.UndefinedLine);
                }

                // La validazione cross-scope è gestita in RewriteJumpStatement, ma qui l'errore è implicito.
                // L'uso di ON GOTO/GOSUB è tipicamente limitato al modulo corrente/routine.
            }
            else // Target numerico
            {
                // La validazione per i numeri di riga è semplice: deve esistere.
                var key = SymbolTable.GetQualifiedLabelName(context.ModulePath, null, targetName);
                if (program.Symbols.FindLabel(key) == null)
                {
                    throw CreateError(targetToken, $"Numero di riga non definito per ON {stmt.Type}: {targetName}.",
                        ErrorCode.UndefinedLine);
                }
            }
        }

        // Il nodo AST è valido; lo restituiamo con l'espressione indice riscritta.
        return stmt with { IndexExpression = rewrittenIndex };
    }

    /// <summary>
    /// Helper specifico per validare un'istruzione `RestoreStmt`.
    /// </summary>
    private static RestoreStmt RewriteRestoreStatement(RestoreStmt stmt, AstRewriteContext context,
        ProcedureDefinition? procedureContext, BasicProgram program)
    {
        var targetToken = stmt.Target;

        // Se non c'è un target (RESTORE semplice), non c'è nulla da validare.
        if (targetToken == null)
        {
            return stmt;
        }

        switch (targetToken.Kind)
        {
            case TokenKind.Identifier:
            {
                var labelName = targetToken.Lexeme;
                var procedureScopeKey =
                    SymbolTable.GetQualifiedLabelName(context.ModulePath, procedureContext, labelName);
                var moduleScopeKey = SymbolTable.GetQualifiedLabelName(context.ModulePath, null, labelName);

                var destination = program.Symbols.FindLabel(procedureScopeKey) ??
                                  program.Symbols.FindLabel(moduleScopeKey);

                if (destination == null)
                {
                    throw CreateError(targetToken, $"Etichetta '{labelName}' non definita per RESTORE.",
                        ErrorCode.UndefinedLine);
                }

                break;
            }
            case TokenKind.Number or TokenKind.LineNumber:
            {
                var lineNumber = (int)(double)targetToken.Literal!;
                var key = SymbolTable.GetQualifiedLabelName(context.ModulePath, null, lineNumber.ToString());
                if (program.Symbols.FindLabel(key) == null)
                {
                    throw CreateError(targetToken, $"Numero di riga '{lineNumber}' non definito per RESTORE.",
                        ErrorCode.UndefinedLine);
                }

                break;
            }
        }

        // Il nodo non necessita di essere riscritto, solo validato.
        return stmt;
    }

    /// <summary>
    /// Riscrive un'istruzione `SelectStmt`, gestendo la propagazione del contesto di riscrittura
    /// in modo isolato per ogni ramo `CASE`.
    /// Aggiornato per supportare la riscrittura dei pattern di tipo e valore (Pattern Matching).
    /// </summary>
    /// <param name="stmt">L'istruzione `SelectStmt` originale.</param>
    /// <param name="context">Il contesto di riscrittura prima di entrare nello statement.</param>
    /// <param name="procedureContext">Il contesto della procedura corrente.</param>
    /// <param name="program">L'istanza del programma.</param>
    /// <returns>Una tupla contenente lo statement riscritto e il contesto di riscrittura invariato.</returns>
    private static (Stmt Rewritten, AstRewriteContext NewContext) RewriteSelectStatement(SelectStmt stmt,
        AstRewriteContext context, ProcedureDefinition? procedureContext, BasicProgram program)
    {
        // Il contesto di output sarà lo stesso di quello di input,
        // poiché lo stato del DBF non deve "fuoriuscire" da un blocco SELECT.
        var newContext = context;

        // 1. Riscrive l'espressione principale del SELECT.
        var rewrittenExpression = RewriteExpression(stmt.Expression, context, program);

        // 2. Riscrive ogni clausola CASE.
        var rewrittenCaseClauses = ImmutableArray.CreateBuilder<CaseClause>();
        foreach (var clause in stmt.CaseClauses)
        {
            var rewrittenTests = clause.Tests.Select(test => test switch
            {
                CaseValue cv => cv with { Value = RewriteExpression(cv.Value, context, program) },
                CaseRange cr => cr with
                {
                    Start = RewriteExpression(cr.Start, context, program),
                    End = RewriteExpression(cr.End, context, program)
                },
                CaseComparison cc => cc with { Value = RewriteExpression(cc.Value, context, program) },
                CaseTypePattern ctp => ctp with
                {
                    TargetType = ResolveDataType(ctp.TargetType, program, context.ModulePath),
                    WhenCondition = ctp.WhenCondition != null
                        ? RewriteExpression(ctp.WhenCondition, context, program)
                        : null
                },
                CaseValuePattern cvp => cvp with
                {
                    PatternValue = RewriteExpression(cvp.PatternValue, context, program),
                    WhenCondition = cvp.WhenCondition != null
                        ? RewriteExpression(cvp.WhenCondition, context, program)
                        : null
                },
                _ => test
            }).ToImmutableArray();

            // Riscrive il corpo del ramo CASE partendo dal contesto originale.
            var branchContext = context;
            var bodyBuilder = ImmutableArray.CreateBuilder<Stmt>();
            foreach (var statement in clause.Statements)
            {
                (var rewrittenStmt, branchContext) =
                    RewriteStatement(statement, branchContext, procedureContext, program);
                bodyBuilder.Add(rewrittenStmt);
            }

            rewrittenCaseClauses.Add(clause with { Tests = rewrittenTests, Statements = bodyBuilder.ToImmutable() });
        }

        // 3. Riscrive la clausola CASE ELSE, se presente.
        CaseElseClause? rewrittenElseClause = null;
        if (stmt.ElseClause != null)
        {
            var elseContext = context;
            var elseBodyBuilder = ImmutableArray.CreateBuilder<Stmt>();
            foreach (var statement in stmt.ElseClause.Statements)
            {
                (var rewrittenStmt, elseContext) = RewriteStatement(statement, elseContext, procedureContext, program);
                elseBodyBuilder.Add(rewrittenStmt);
            }

            rewrittenElseClause = stmt.ElseClause with { Statements = elseBodyBuilder.ToImmutable() };
        }

        // 4. Restituisce lo SelectStmt completamente riscritto.
        var rewrittenSelect = stmt with
        {
            Expression = rewrittenExpression,
            CaseClauses = rewrittenCaseClauses.ToImmutable(),
            ElseClause = rewrittenElseClause
        };

        return (rewrittenSelect, newContext);
    }

    /// <summary>
    /// Riscrive un'istruzione `TryCatchStmt`, gestendo la propagazione del contesto di riscrittura
    /// in modo isolato per ogni blocco (`TRY`, `CATCH`, `FINALLY`).
    /// </summary>
    /// <param name="stmt">L'istruzione `TryCatchStmt` originale.</param>
    /// <param name="context">Il contesto di riscrittura prima di entrare nello statement.</param>
    /// <param name="procedureContext">Il contesto della procedura corrente.</param>
    /// <param name="program">L'istanza del programma.</param>
    /// <returns>Una tupla contenente lo statement riscritto e il contesto di riscrittura invariato.</returns>
    private static (Stmt Rewritten, AstRewriteContext NewContext) RewriteTryCatchStatement(TryCatchStmt stmt,
        AstRewriteContext context, ProcedureDefinition? procedureContext, BasicProgram program)
    {
        // Il contesto di output sarà lo stesso di quello di input.
        var newContext = context;

        // Riscrive il corpo del blocco TRY.
        var tryContext = context;
        var tryBodyBuilder = ImmutableArray.CreateBuilder<Stmt>();
        foreach (var statement in stmt.TryBody)
        {
            (var rewrittenStmt, tryContext) = RewriteStatement(statement, tryContext, procedureContext, program);
            tryBodyBuilder.Add(rewrittenStmt);
        }

        // Riscrive ogni clausola CATCH.
        var rewrittenCatchClauses = ImmutableArray.CreateBuilder<CatchClause>();
        foreach (var clause in stmt.CatchClauses)
        {
            var rewrittenWhen = clause.WhenCondition != null
                ? RewriteExpression(clause.WhenCondition, context, program)
                : null;

            var rewrittenVar = clause.Variable != null
                ? RewriteCatchVariableDeclaration(clause.Variable, context, program)
                : null;

            var catchContext = context;
            var catchBodyBuilder = ImmutableArray.CreateBuilder<Stmt>();
            foreach (var statement in clause.Body)
            {
                (var rewrittenStmt, catchContext) =
                    RewriteStatement(statement, catchContext, procedureContext, program);
                catchBodyBuilder.Add(rewrittenStmt);
            }

            rewrittenCatchClauses.Add(clause with
            {
                Variable = rewrittenVar, WhenCondition = rewrittenWhen, Body = catchBodyBuilder.ToImmutable()
            });
        }

        // Riscrive il blocco FINALLY, se presente.
        FinallyClause? rewrittenFinally = null;
        if (stmt.Finally != null)
        {
            var finallyContext = context;
            var finallyBodyBuilder = ImmutableArray.CreateBuilder<Stmt>();
            foreach (var statement in stmt.Finally.Body)
            {
                (var rewrittenStmt, finallyContext) =
                    RewriteStatement(statement, finallyContext, procedureContext, program);
                finallyBodyBuilder.Add(rewrittenStmt);
            }

            rewrittenFinally = stmt.Finally with { Body = finallyBodyBuilder.ToImmutable() };
        }

        var rewrittenTryCatch = stmt with
        {
            TryBody = tryBodyBuilder.ToImmutable(),
            CatchClauses = rewrittenCatchClauses.ToImmutable(),
            Finally = rewrittenFinally
        };

        return (rewrittenTryCatch, newContext);
    }

    /// <summary>
    /// Riscrive ricorsivamente le espressioni all'interno di un'azione audio.
    /// Questo metodo gestisce i diversi tipi concreti di <see cref="AudioAction"/>,
    /// riscrivendo i SymbolId per ogni espressione (es. stringa musicale, canale, volume).
    /// </summary>
    private static AudioAction RewriteAudioAction(AudioAction action, AstRewriteContext context, BasicProgram program)
    {
        return action switch
        {
            StandardPlayAction a => new StandardPlayAction(
                RewriteExpression(a.MusicString, context, program)
            ),

            ConcurrentPlayAction a => new ConcurrentPlayAction(
                [..a.MusicStrings.Select(e => RewriteExpression(e, context, program))]
            ),

            QueuePlayAction a => new QueuePlayAction(
                RewriteExpression(a.MusicString, context, program)
            ),

            NamedPlayAction a => new NamedPlayAction(
                RewriteExpression(a.MusicString, context, program),
                RewriteExpression(a.Channel, context, program)
            ),

            LoopPlayAction a => new LoopPlayAction(
                RewriteExpression(a.MusicString, context, program),
                RewriteExpression(a.Channel, context, program),
                a.Times != null ? RewriteExpression(a.Times, context, program) : null
            ),

            ControlAudioAction a => new ControlAudioAction(
                a.Command, // Token, non expression
                a.Argument != null ? RewriteExpression(a.Argument, context, program) : null
            ),

            _ => action
        };
    }

    /// <summary>
    /// Helper specifico per riscrivere un'istruzione `LocalTypeStmt`.
    /// Questo metodo scende ricorsivamente nella definizione del `UserDefinedType` per
    /// risolvere i tipi dei campi, riscrivere le espressioni di default e i corpi dei metodi,
    /// costruttori, operatori e proprietà.
    /// </summary>
    private static LocalTypeStmt RewriteLocalTypeStatement(LocalTypeStmt stmt, AstRewriteContext context,
        BasicProgram program)
    {
        var udt = stmt.Definition;

        // Riscrive i valori di default e i tipi dei campi.
        var rewrittenFields = udt.Fields.Select(field =>
        {
            var rewrittenDefaultValue = field.DefaultValue != null
                ? RewriteExpression(field.DefaultValue, context, program)
                : null;
            var resolvedDataType = ResolveDataType(field.DataType, program, context.ModulePath);

            return field with { DataType = resolvedDataType, DefaultValue = rewrittenDefaultValue };
        }).ToImmutableArray();

        // Riscrive i corpi dei metodi (che sono ProcedureDefinition).
        var rewrittenMethods = udt.Methods
            .Select(method => RewriteProcedure(method, context, program))
            .ToImmutableArray();

        // --- INIZIO LOGICA MANCANTE ---

        // Riscrive i corpi degli operatori. Item 14 (audit rewrite non uniforme): a
        // differenza di RewriteOperatorMember (livello classe), qui Parameters/ReturnType
        // non venivano risolti — un operatore di TYPE (UDT) con parametro/ritorno di tipo
        // classe/UDT poteva restare con un UnresolvedQualifiedType.
        var rewrittenOperators = udt.Operators.Select(op =>
        {
            var rewrittenOpParams = op.Parameters
                .Select(parameter => RewriteParameter(parameter, context, program))
                .ToImmutableArray();
            var rewrittenOpReturnType = ResolveDataType(op.ReturnType, program, context.ModulePath);
            // Riscriviamo il corpo direttamente, preservando il tipo OperatorDefinition.
            // Questo evita il cast errato da ProcedureDefinition.
            var (rewrittenBody, _) = RewriteStatementList(op.Body, context, null, program);
            return op with { Parameters = rewrittenOpParams, ReturnType = rewrittenOpReturnType, Body = rewrittenBody };
        }).ToImmutableArray();

        // Riscrive i corpi dei costruttori con contesto UDT. Stesso item 14: Parameters
        // non veniva risolto (i costruttori non hanno ReturnType).
        var ctorCtx = context with { CurrentUdtScopeId = udt.SymbolId >= 0 ? udt.SymbolId : context.CurrentUdtScopeId };
        var rewrittenConstructors = udt.Constructors.Select(ctor =>
        {
            var rewrittenCtorParams = ctor.Parameters
                .Select(parameter => RewriteParameter(parameter, ctorCtx, program))
                .ToImmutableArray();
            var (rewrittenBody, _) = RewriteStatementList(ctor.Body, ctorCtx, null, program);
            return ctor with { Parameters = rewrittenCtorParams, Body = rewrittenBody };
        }).ToImmutableArray();

        // Riscrive i corpi delle proprietà (Getter e Setter).
        var rewrittenProperties = udt.Properties.Select(prop =>
        {
            PropertyGetter? rewrittenGetter = null;
            if (prop.Getter != null)
            {
                var (getterBody, _) = RewriteStatementList(prop.Getter.Body, context, null, program);
                rewrittenGetter = new PropertyGetter(Body: getterBody);
            }

            PropertySetter? rewrittenSetter = null;
            if (prop.Setter != null)
            {
                // Anche il corpo del setter viene riscritto.
                var (setterBody, _) = RewriteStatementList(prop.Setter.Body, context, null, program);
                rewrittenSetter = prop.Setter with { Body = setterBody };
            }

            return prop with { Getter = rewrittenGetter, Setter = rewrittenSetter };
        }).ToImmutableArray();

        // --- FINE LOGICA MANCANTE ---

        var rewrittenUdt = udt with
        {
            Fields = rewrittenFields,
            Methods = rewrittenMethods,
            Operators = rewrittenOperators,
            Constructors = rewrittenConstructors,
            Properties = rewrittenProperties
        };

        // Aggiorna la definizione nella SymbolTable se è cambiata.
        if (udt.SymbolId >= 0)
        {
            program.Symbols.UpdateSymbolDeclaration(udt.SymbolId, rewrittenUdt);
        }

        return stmt with { Definition = rewrittenUdt };
    }

    /// <summary>
    /// Punto di ingresso ricorsivo per la riscrittura di un'istruzione (Stmt).
    /// Ora gestisce e propaga il contesto di riscrittura (`AstRewriteContext`), inclusi
    /// gli aggiornamenti di stato per i comandi di database.
    /// </summary>
    /// <summary>Zucchero metodi Control: `istanza.Move(x,y)` / `SetFocus()` / `ZOrder(f)` /
    /// `Invalidate()` / `Refresh()` su una classe che eredita `Control` → comando `CONTROL ...`
    /// (equivalente ai comandi legacy, già reali). Vedi §11.7 CONTROLS.</summary>
    private static bool TryRewriteControlMethod(Expr expr, BasicProgram program, out Stmt? stmt)
    {
        stmt = null;
        if (expr is not MethodCallExpr { Instance: VariableExpr ve } mce) return false;
        var instSym = program.Symbols.GetSymbol(ve.SymbolId);
        var oic = StringComparison.OrdinalIgnoreCase;
        Expr Num(double v) => new NumberExpr(v, mce.MethodName);
        Expr Arg(int i) => i < mce.Args.Length ? mce.Args[i] : Num(0);

        // (b) zucchero GraphicsContext: `g.Clear/SetColor/Line/Rect/FillRect/Circle/Text/Plot`
        //     su una variabile ANY (il `g` del metodo `Paint`) → `CONTROL DRAW g, op, …`.
        if (instSym?.DataType is AnyType)
        {
            string m = mce.MethodName.Lexeme;
            int op = -1; var nums = new List<Expr>(); string sarg = "";
            if (m.Equals("Clear", oic)) op = 0;
            else if (m.Equals("SetColor", oic)) { op = 1; nums.Add(Arg(0)); }
            else if (m.Equals("Line", oic)) { op = 2; nums.Add(Arg(0)); nums.Add(Arg(1)); nums.Add(Arg(2)); nums.Add(Arg(3)); }
            else if (m.Equals("Rect", oic)) { op = 3; nums.Add(Arg(0)); nums.Add(Arg(1)); nums.Add(Arg(2)); nums.Add(Arg(3)); }
            else if (m.Equals("FillRect", oic)) { op = 4; nums.Add(Arg(0)); nums.Add(Arg(1)); nums.Add(Arg(2)); nums.Add(Arg(3)); }
            else if (m.Equals("Circle", oic)) { op = 5; nums.Add(Arg(0)); nums.Add(Arg(1)); nums.Add(Arg(2)); }
            else if (m.Equals("FillCircle", oic)) { op = 6; nums.Add(Arg(0)); nums.Add(Arg(1)); nums.Add(Arg(2)); }
            else if (m.Equals("Plot", oic)) { op = 8; nums.Add(Arg(0)); nums.Add(Arg(1)); }
            else if (m.Equals("Text", oic)) { op = 7; nums.Add(Arg(1)); nums.Add(Arg(2)); if (mce.Args.Length >= 1 && mce.Args[0] is StringExpr ts) sarg = ts.Value ?? ""; }
            if (op < 0) return false;
            var dargs = new List<Expr> { ve, Num(op) };
            for (int i = 0; i < 4; i++) dargs.Add(i < nums.Count ? nums[i] : Num(0));
            dargs.Add(new StringExpr(sarg, mce.MethodName));
            stmt = new ControlMethodStmt("DRAW", dargs, null, mce.MethodName);
            return true;
        }

        // (a) metodi Control (istanza che eredita Control) → comando CONTROL ...
        string sub = mce.MethodName.Lexeme.ToUpperInvariant() switch
        {
            "MOVE" => "MOVE",
            "SETFOCUS" => "FOCUS",
            "ZORDER" => "ZORDER",
            "INVALIDATE" => "INVALIDATE",
            "REFRESH" => "REFRESH",
            _ => ""
        };
        if (sub.Length == 0) return false;
        if (instSym?.DataType is not ClassType ct || !InheritsControl(ct.Definition)) return false;
        var idExpr = new StringExpr(ve.NameCleaned.Lexeme, mce.MethodName);
        var args = new List<Expr> { idExpr };
        args.AddRange(mce.Args);
        stmt = new ControlMethodStmt(sub, args, null, mce.MethodName);
        return true;
    }

    /* §11.1: scrittura proprietà controllo — `istanza.Prop = v` (singola assegnazione) → CONTROL SET. */
    private static bool TryControlPropWrite(LetStmt ls, AstRewriteContext context, BasicProgram program, out Stmt? stmt)
    {
        stmt = null;
        if (ls.Assignments.Length != 1) return false;
        var asg = ls.Assignments[0];
        if (asg.Target is not FieldTarget ft) return false;
        VariableTarget? vt = ft.Structure as VariableTarget;
        string? prop = null;
        string propLexeme = ft.OriginalToken.Lexeme;
        if (vt == null && ft.Structure is FieldTarget fontTarget &&
            fontTarget.Structure is VariableTarget fontVar &&
            fontTarget.OriginalToken.Lexeme.Equals("Font", StringComparison.OrdinalIgnoreCase) &&
            program.Symbols.GetSymbol(fontVar.SymbolId)?.DataType is ClassType fontClass &&
            InheritsControlFlexible(fontClass.Definition))
        {
            vt = fontVar;
            propLexeme = "Font." + ft.OriginalToken.Lexeme;
            prop = ControlPropRuntime(propLexeme);
        }
        else
        {
            prop = ControlPropRuntime(ft.OriginalToken.Lexeme);
        }
        if (vt == null) return false;
        if (prop == null) return false;
        if (program.Symbols.GetSymbol(vt.SymbolId)?.DataType is not ClassType ct || !InheritsControlFlexible(ct.Definition)) return false;
        var nameStr = new StringExpr(vt.NameCleaned.Lexeme, ft.OriginalToken);
        var propStr = new StringExpr(prop, ft.OriginalToken);
        var val = RewriteExpression(asg.Value, context, program);
        // Prop STRINGA (Name/Tag/Tooltip/ForeColor/BackColor): il valore va nel 4° arg (str),
        // non nel 3° (num) → CONTROL SET name, prop, 0, val.
        stmt = ControlPropIsString(propLexeme)
            ? new ControlMethodStmt("SET", [nameStr, propStr, new NumberExpr(0.0, ft.OriginalToken), val], null, ft.OriginalToken)
            : new ControlMethodStmt("SET", [nameStr, propStr, val], null, ft.OriginalToken);
        return true;
    }

    private static bool InheritsControl(ClassDefinition? cls)
    {
        for (var c = cls; c != null; c = c.ResolvedBaseClass)
            if (c.OriginalName.Lexeme.Equals("Control", StringComparison.OrdinalIgnoreCase)) return true;
        return false;
    }

    /* Come InheritsControl ma tolerante: durante le prime fasi di Rewrite la catena
     * ResolvedBaseClass può non essere ancora risolta → si usa OriginalBaseClassName. */
    private static bool InheritsControlFlexible(ClassDefinition? cls)
    {
        for (int g = 0; cls != null && g < 16; g++)
        {
            if (cls.OriginalName.Lexeme.Equals("Control", StringComparison.OrdinalIgnoreCase)) return true;
            if (cls.ResolvedBaseClass != null) { cls = cls.ResolvedBaseClass; continue; }
            if (cls.OriginalBaseClassName is { } b)
                return b.Lexeme.Equals("Control", StringComparison.OrdinalIgnoreCase)
                    || b.Lexeme.EndsWith(".Control", StringComparison.OrdinalIgnoreCase);
            return false;
        }
        return false;
    }

    /* §11.1: `istanza.Prop` (classe INHERITS Control) → nome-proprietà runtime per CONTROL GET/SET. */
    private static string? ControlPropRuntime(string p) => p.ToUpperInvariant() switch
    {
        "X" => "x", "Y" => "y", "WIDTH" => "width", "HEIGHT" => "height",
        "VISIBLE" => "visible", "ENABLED" => "enabled", "FOCUSED" => "focused",
        "TABINDEX" => "tabindex", "TABSTOP" => "tabstop", "HANDLE" => "handle",
        "NAME" => "name", "TAG" => "tag", "TOOLTIP" => "tooltip", "TOOLTIPTEXT" => "tooltip",
        "FORECOLOR" => "forecolor", "FG" => "forecolor", "FOREGROUND" => "forecolor",
        "BACKCOLOR" => "backcolor", "BG" => "backcolor", "BACKGROUND" => "backcolor",
        "CAPTION" => "caption", "TEXT" => "text",
        "FONT" => "Font.Name", "FONTNAME" => "Font.Name", "FONT.NAME" => "Font.Name",
        "FONTSIZE" => "Font.Size", "FONT.SIZE" => "Font.Size",
        "FONT.BOLD" => "Font.Bold", "FONT.ITALIC" => "Font.Italic",
        "FONT.UNDERLINE" => "Font.Underline", "FONT.STRIKETHROUGH" => "Font.Strikethrough",
        "APPEARANCE" => "Appearance", "MOUSEPOINTER" => "MousePointer",
        "RIGHTTOLEFT" => "RightToLeft", "LOCKED" => "Locked",
        "STARTUPPOSITION" => "StartUpPosition", "SHOWINTASKBAR" => "ShowInTaskbar",
        "WHATSTHISHELPID" => "WhatsThisHelpID", "WORDWRAP" => "WordWrap",
        "AUTOSIZE" => "AutoSize", "BACKSTYLE" => "BackStyle", "BORDERSTYLE" => "BorderStyle",
        "USEMNEMONIC" => "UseMnemonic", "CAUSESVALIDATION" => "CausesValidation",
        "VALUE" => "value", "LIST" => "items", "LISTINDEX" => "listindex", "LISTCOUNT" => "listcount",
        "INTERVAL" => "interval", "ALIGNMENT" => "alignment", "MULTILINE" => "multiline",
        "PASSWORDCHAR" => "passwordchar", "SCROLLBARS" => "scrollbars", "SORTED" => "sorted",
        "INTEGRALHEIGHT" => "integralheight", "STYLE" => "style", "COLUMNS" => "columns",
        "MIN" => "min", "MAX" => "max", "LARGECHANGE" => "largechange", "SMALLCHANGE" => "smallchange",
        "DATASOURCE" => "datasource", "ALLOWADDNEW" => "allowaddnew", "ALLOWDELETE" => "allowdelete",
        "ALLOWUPDATE" => "allowupdate", "COLUMNHEADERS" => "columnheaders", "DEFCOLWIDTH" => "defcolwidth",
        "ROWHEIGHT" => "rowheight", "ALLOWROWSIZING" => "allowrowsizing", "ALLOWCOLSIZING" => "allowcolsizing",
        "ACTION" => "action", "DIALOGTITLE" => "dialogtitle", "FILTER" => "filter",
        "FILTERINDEX" => "filterindex", "FILENAME" => "filename", "FLAGS" => "flags", "COLOR" => "color",
        "CANCELERROR" => "cancelerror", "CONNECT" => "connect", "DATABASENAME" => "databasename",
        "RECORDSOURCE" => "recordsource", "RECORDSETTYPE" => "recordsettype", "BOFACTION" => "bofaction",
        "EOFACTION" => "eofaction", "READONLY" => "readonly", "EXCLUSIVE" => "exclusive",
        "OPTIONS" => "options", "PICTURE" => "picture", "ALIGN" => "align",
        _ => null
    };
    private static bool ControlPropIsString(string p) => p.ToUpperInvariant() is
        "NAME" or "TAG" or "TOOLTIP" or "TOOLTIPTEXT" or "CAPTION" or "TEXT"
        or "FONT" or "FONTNAME" or "FONT.NAME" or "LIST"
        or "PASSWORDCHAR" or "DATASOURCE" or "DIALOGTITLE" or "FILTER" or "FILENAME" or "CONNECT"
        or "DATABASENAME" or "RECORDSOURCE" or "PICTURE"
        or "FORECOLOR" or "FG" or "FOREGROUND" or "BACKCOLOR" or "BG" or "BACKGROUND";

    /* §11.7-3: riscrive CONTROL <sub>; per ADD di una classe INHERITS Control appende agli Args
     * le triple (evName, metodo, symbolId) dei metodi-evento — così i backend senza type info
     * (PCode/VM-C, il cui `Compiler` vede solo l'AST) possono emettere il binding handle,ev.
     * Nessuna sintesi di SUB/global: solo Args extra, nessuna mutazione del modulo. */
    private static ControlMethodStmt RewriteControlMethod(ControlMethodStmt s, AstRewriteContext context, BasicProgram program)
    {
        var args = s.Args.Select(a => RewriteExpression(a, context, program)).ToList();
        var into = s.Into != null ? RewriteTarget(s.Into, context, program) : null;
        if (string.Equals(s.Sub, "ADD", StringComparison.OrdinalIgnoreCase)
            && s.Args.Count > 0 && s.Args[0] is VariableExpr ive
            && program.Symbols.GetSymbol(ive.SymbolId)?.DataType is ClassType ct
            && InheritsControl(ct.Definition)
            && !s.Args.Any(x => x is StringExpr se0 && se0.Value == "__UCBIND__"))
        {
            var tok0 = ive.NameCleaned;
            args.Add(new StringExpr("__UCBIND__", tok0));   /* marker: inizio delle triple evento */
            foreach (var mem in ct.Definition!.Members)
            {
                if (mem is not MethodMember mm) continue;
                var evName = UserControlEventName(mm.Procedure.OriginalName.Lexeme);
                if (evName == null) continue;
                var tok = mm.Procedure.OriginalName;
                args.Add(new StringExpr(evName, tok));
                args.Add(new StringExpr(tok.Lexeme, tok));
                args.Add(new NumberExpr(mm.Procedure.SymbolId, tok));
            }
        }
        return s with { Args = args, Into = into, ScopeId = s.ScopeId };
    }

    private static string? UserControlEventName(string m) => m.ToUpperInvariant() switch
    {
        "CLICK" => "CLICK", "DBLCLICK" => "DBLCLICK", "CHANGE" => "CHANGE",
        "GOTFOCUS" => "GOTFOCUS", "LOSTFOCUS" => "LOSTFOCUS",
        "MOUSEDOWN" => "MOUSEDOWN", "MOUSEUP" => "MOUSEUP", "MOUSEMOVE" => "MOUSEMOVE",
        "KEYDOWN" => "KEYDOWN", "KEYPRESS" => "KEYPRESS", "KEYUP" => "KEYUP",
        "DRAGDROP" => "DRAGDROP", "DRAGOVER" => "DRAGOVER", "VALIDATE" => "VALIDATE",
        _ => null
    };

    private static (Stmt Rewritten, AstRewriteContext NewContext) RewriteStatement(Stmt stmt, AstRewriteContext context,
        ProcedureDefinition? procedureContext, BasicProgram program)
    {
        // La maggior parte degli statement non modifica il contesto, quindi il nuovo contesto è uguale al vecchio per default.
        var newContext = context;
        Stmt rewrittenStmt;

        switch (stmt)
        {
            // --- CASI CHE CONTENGONO BLOCCHI E NECESSITANO GESTIONE ISOLATA DEL CONTESTO ---
            case IfStmt s:
                (rewrittenStmt, newContext) = RewriteIfStatement(s, context, procedureContext, program);
                break;
            case SelectStmt s:
                (rewrittenStmt, newContext) = RewriteSelectStatement(s, context, procedureContext, program);
                break;
            case WithStmt s:
                (rewrittenStmt, newContext) = RewriteWithStatement(s, context, procedureContext, program);
                break;
            case UsingStmt s:
                (rewrittenStmt, newContext) = RewriteUsingStatement(s, context, procedureContext, program);
                break;
            case TryCatchStmt s:
                (rewrittenStmt, newContext) = RewriteTryCatchStatement(s, context, procedureContext, program);
                break;
            case HttpServerStmt s:
                (rewrittenStmt, newContext) = RewriteHttpServerStatement(s, context, program);
                break;
            // Per tutti gli altri statement, chiamiamo un helper che non modifica il contesto.
            default:
                rewrittenStmt = RewriteSimpleStatement(stmt, context, procedureContext, program);
                break;
        }

        return (rewrittenStmt, newContext);
    }

    /// <summary>
    /// Punto di ingresso ricorsivo per la riscrittura di un'istruzione (Stmt).
    /// Corretto per risolvere i tipi di dato e invocare correttamente i sotto-riscrittori.
    /// </summary>
    /// <summary>
    /// Punto di ingresso ricorsivo per la riscrittura di un'istruzione (Stmt).
    /// Corretto e completato per risolvere i tipi di dato in tutte le dichiarazioni e istruzioni.
    /// </summary>
    /// <summary>
    /// Punto di ingresso ricorsivo per la riscrittura di un'istruzione (Stmt).
    /// Corretto e completato per risolvere i tipi di dato e validare i salti.
    /// </summary>
    /// <summary>
    /// Punto di ingresso ricorsivo per la riscrittura di un'istruzione (Stmt).
    /// Modificato per delegare la riscrittura di DeclarationStmt e per rimuovere ConstStmt.
    /// </summary>
    private static (Stmt Rewritten, AstRewriteContext NewContext) RewriteWithStatement(
        WithStmt stmt, AstRewriteContext context, ProcedureDefinition? procedureContext, BasicProgram program)
    {
        // WITH body is rewritten normally; the executor handles implicit member access.
        var rewrittenBody = stmt.Body
            .Select(s => RewriteStatement(s, context, procedureContext, program).Rewritten)
            .ToImmutableArray();
        return (stmt with { Body = rewrittenBody }, context);
    }

    /// <summary>
    /// USING mancava da questo dispatcher (cadeva nel default -> RewriteSimpleStatement
    /// -> passthrough invariato): ResourceInit non veniva mai riscritta e il Body non
    /// veniva mai attraversato, quindi DIM/LET dentro un blocco USING perdevano la
    /// riscrittura con inferenza di tipo applicata a tutti gli altri blocchi (IF/WITH/ecc).
    /// Porting da GMBasic (stesso gap, stesso codice pre-fork).
    /// </summary>
    private static (Stmt Rewritten, AstRewriteContext NewContext) RewriteUsingStatement(
        UsingStmt stmt, AstRewriteContext context, ProcedureDefinition? procedureContext, BasicProgram program)
    {
        // OPEN/FOPEN/FCREATE usati come `USING F = OPEN(file, OUTPUT)` vengono
        // compilati dal parser come ArrayAccessExpr (non BuiltinFunctionCallExpr).
        // RewriteArrayAccess non li riconosce e lancerebbe "Simbolo non definito:
        // 'OPEN'" a compile time. ExecuteUsingStep (Gpm.Branches.cs) gestisce gia'
        // esplicitamente questa forma ArrayAccessExpr non riscritta via
        // OpenFileFromAae — va quindi lasciata invariata qui, esattamente come
        // funzionava prima che RewriteUsingStatement iniziasse a toccare ResourceInit.
        var isUnrewritableOpenCall = stmt.ResourceInit is ArrayAccessExpr aae &&
            (aae.NameCleaned.Lexeme.Equals("OPEN", StringComparison.OrdinalIgnoreCase) ||
             aae.NameCleaned.Lexeme.Equals("FOPEN", StringComparison.OrdinalIgnoreCase) ||
             aae.NameCleaned.Lexeme.Equals("FCREATE", StringComparison.OrdinalIgnoreCase));

        var rewrittenInit = isUnrewritableOpenCall
            ? stmt.ResourceInit
            : RewriteExpression(stmt.ResourceInit, context, program);

        // BindUsingStatement registra ResourceTarget con DataType = InferredType() (nessuna
        // AS esplicita in `USING var = expr`) e nulla lo corregge in seguito — a differenza di
        // un normale `DIM x = expr`/LET, che passa dallo stesso meccanismo di inferenza qui
        // sotto (vedi RewriteLetStatement). Sul PCodeVM (locali dinamicamente tipizzati,
        // object?[]) questo non causa danni visibili, ma il backend IL mappa InferredType su
        // CLR double (DataTypeMapper.Map) — per una risorsa NON numerica (es. un'istanza di
        // CLASS creata con NEW) il local IL dichiarato è del tipo SBAGLIATO: `r.Nome` dentro il
        // corpo leggeva un valore spazzatura invece dei campi dell'istanza. Stesso schema già
        // usato da RewriteLetStatement per una DIM/LET senza AS esplicita: infierisce il tipo
        // da ResourceInit e corregge la SymbolTable con UpdateSymbolType.
        var finalTarget = stmt.ResourceTarget;
        if (finalTarget is VariableTarget usingVt && usingVt.SymbolId >= 0)
        {
            var usingSymbol = program.Symbols.GetSymbol(usingVt.SymbolId);
            if (usingSymbol.DataType is InferredType)
            {
                var initType = isUnrewritableOpenCall
                    ? new LongType()
                    : LinkerInferExpressionType(rewrittenInit, context, program);
                if (initType is not InferredType)
                {
                    program.Symbols.UpdateSymbolType(usingVt.SymbolId, initType);
                    finalTarget = usingVt with { ResolvedType = initType };
                }
            }
        }

        var rewrittenBody = stmt.Body
            .Select(s => RewriteStatement(s, context, procedureContext, program).Rewritten)
            .ToImmutableArray();
        return (stmt with { ResourceTarget = finalTarget, ResourceInit = rewrittenInit, Body = rewrittenBody }, context);
    }

    /// <summary>
    /// Riscrive una clausola CASE di MATCH (Fase D, twin-port GMBasic 5a583dc): ogni
    /// pattern è un'Espressione che deve passare da RewriteExpression — senza, le chiamate
    /// a FUNCTION utente nei pattern restavano ArrayAccessExpr non risolti → OP_PUSH_NIL.
    /// </summary>
    private static MatchCaseClause RewriteMatchCaseClause(MatchCaseClause c, AstRewriteContext context, ProcedureDefinition? procedureContext, BasicProgram program)
    {
        return c with
        {
            NullPattern = c.NullPattern != null ? RewriteExpression(c.NullPattern, context, program) : null,
            TypePattern = c.TypePattern != null ? c.TypePattern with
            {
                WhenCondition = c.TypePattern.WhenCondition != null
                    ? RewriteExpression(c.TypePattern.WhenCondition, context, program)
                    : null
            } : null,
            ValuePattern = c.ValuePattern != null ? c.ValuePattern with
            {
                PatternValue = RewriteExpression(c.ValuePattern.PatternValue, context, program),
                WhenCondition = c.ValuePattern.WhenCondition != null
                    ? RewriteExpression(c.ValuePattern.WhenCondition, context, program)
                    : null
            } : null,
            ValueTest = c.ValueTest != null ? new CaseValue(RewriteExpression(c.ValueTest.Value, context, program)) : null,
            RangeTest = c.RangeTest != null ? new CaseRange(
                RewriteExpression(c.RangeTest.Start, context, program),
                RewriteExpression(c.RangeTest.End, context, program)) : null,
            ComparisonTest = c.ComparisonTest != null ? new CaseComparison(
                c.ComparisonTest.Operator,
                RewriteExpression(c.ComparisonTest.Value, context, program)) : null,
            // Usa il dispatcher completo (non RewriteSimpleStatement): un IF/SELECT/WITH/USING/
            // TRY-CATCH annidato dentro una CASE di MATCH deve ricevere la stessa gestione del
            // blocco/contesto degli altri statement, non il passthrough "senza contesto".
            Statements = [..c.Statements.Select(st => RewriteStatement(st, context, procedureContext, program).Rewritten)]
        };
    }

    private static Stmt RewriteSimpleStatement(Stmt stmt, AstRewriteContext context,
        ProcedureDefinition? procedureContext, BasicProgram program)
    {
        // La logica di dispatch è stata appiattita per maggiore chiarezza.
        return stmt switch
        {
            // --- Istruzioni con logica di riscrittura complessa delegate a helper ---
            LetStmt lw when TryControlPropWrite(lw, context, program, out var cwS) && cwS is not null => cwS,
            LetStmt ls => RewriteLetStmtWithImplicitMatrix(ls, context, program),
            GoProcedureStmt s => RewriteGoProcedureStatement(s, context, program),
            JumpStmt s => RewriteJumpStatement(s, context, procedureContext, program),
            GosubStmt s => RewriteGosubStatement(s, context, procedureContext, program),
            OnJumpStmt s => RewriteOnJumpStatement(s, context, procedureContext, program),
            RestoreStmt s => RewriteRestoreStatement(s, context, procedureContext, program),

            // --- Istruzioni con riscrittura di dichiarazioni ---
            DeclarationStmt s => s with
            {
                Declarations =
                [..s.Declarations.Select(decl => RewriteVariableDeclarationWithInference(decl, context, program))]
            },
            // --- Istruzioni con riscrittura di espressioni o target interni ---
            ReadStmt s => s with
            {
                Variables = [..s.Variables.Select(v => RewriteTarget(v, context, program))]
            },
            // Clipper @...GET: riscrive TUTTI i figli Expr (Row/Col/Variable/Picture/Valid/
            // RangeLo/RangeHi/Color/Items/ControlWidth/ControlHeight/Caption/StepValue/
            // MinValue/MaxValue) — prima solo Variable (twin-port GMBasic 5a583dc).
            AtGetStmt s => s with
            {
                Row = RewriteExpression(s.Row, context, program),
                Col = RewriteExpression(s.Col, context, program),
                Variable = RewriteTarget(s.Variable, context, program),
                Picture = s.Picture != null ? RewriteExpression(s.Picture, context, program) : null,
                Valid = s.Valid != null ? RewriteExpression(s.Valid, context, program) : null,
                RangeLo = s.RangeLo != null ? RewriteExpression(s.RangeLo, context, program) : null,
                RangeHi = s.RangeHi != null ? RewriteExpression(s.RangeHi, context, program) : null,
                Color = s.Color != null ? RewriteExpression(s.Color, context, program) : null,
                Items = s.Items != null ? RewriteExpression(s.Items, context, program) : null,
                ControlWidth = s.ControlWidth != null ? RewriteExpression(s.ControlWidth, context, program) : null,
                ControlHeight = s.ControlHeight != null ? RewriteExpression(s.ControlHeight, context, program) : null,
                Caption = s.Caption != null ? RewriteExpression(s.Caption, context, program) : null,
                StepValue = s.StepValue != null ? RewriteExpression(s.StepValue, context, program) : null,
                MinValue = s.MinValue != null ? RewriteExpression(s.MinValue, context, program) : null,
                MaxValue = s.MaxValue != null ? RewriteExpression(s.MaxValue, context, program) : null,
                ScopeId = s.ScopeId
            },
            // Clipper @...SAY: riscrive le espressioni interne — senza, le chiamate builtin
            // con nome Identifier-kind (TIME$()/DATE$() inline) restavano ArrayAccessExpr
            // non risolti → OP_PUSH_NIL a runtime (allineato GMBasic).
            AtSayStmt s => s with
            {
                Row = RewriteExpression(s.Row, context, program),
                Col = RewriteExpression(s.Col, context, program),
                Expression = RewriteExpression(s.Expression, context, program),
                Picture = s.Picture != null ? RewriteExpression(s.Picture, context, program) : null,
                ScopeId = s.ScopeId
            },
            // --- Fase D (audit rewrite non uniforme, twin-port GMBasic 5a583dc): statement
            // con figli Expr che prima cascavano in `_ => stmt` senza riscrivere le espressioni
            // interne. Conseguenze: costanti %NOME non piegate (ora coperte anche dal fallback
            // compiler, ma il folding resta necessario) e chiamate a FUNCTION utente
            // (ArrayAccessExpr GetPair#N()) non convertite a FunctionExpr → OP_PUSH_NIL a
            // runtime. Riscrive gli stessi campi che il Resolver risolve.
            MatchStmt s => s with
            {
                Expression = RewriteExpression(s.Expression, context, program),
                Cases = [..s.Cases.Select(c => RewriteMatchCaseClause(c, context, procedureContext, program))],
                ScopeId = s.ScopeId
            },
            // CONTROL <sub> args (DRAW/SET/GET/MOVE...): senza questo caso gli argomenti cascavano in `_ => stmt` e restavano NON riscritti: un campo-array di classe
            // (`mQty(i)`) restava un accesso generico emesso come array LOCALE (`ldloc` non inizializzato + CollectionIndexGet -> InvalidProgram in IL).
            ControlMethodStmt s => RewriteControlMethod(s, context, program),
            // CHART SET/GET/GETS: riscrive handle/prop/num/str e (GET/GETS) il target INTO,
            // altrimenti la variabile INTO non viene registrata e il valore è costante 0.
            ChartConfigStmt s => s with
            {
                Handle = RewriteExpression(s.Handle, context, program),
                Prop = RewriteExpression(s.Prop, context, program),
                Num = s.Num != null ? RewriteExpression(s.Num, context, program) : null,
                Str = s.Str != null ? RewriteExpression(s.Str, context, program) : null,
                Into = s.Into != null ? RewriteTarget(s.Into, context, program) : null,
                ScopeId = s.ScopeId
            },
            ChartDrawStmt s => s with
            {
                Handle = RewriteExpression(s.Handle, context, program),
                Op = RewriteExpression(s.Op, context, program),
                Args = [..s.Args.Select(a => RewriteExpression(a, context, program))],
                Into = s.Into != null ? RewriteTarget(s.Into, context, program) : null,
                ScopeId = s.ScopeId
            },
            TupleAssignmentStmt s => s with { TupleExpression = RewriteExpression(s.TupleExpression, context, program), ScopeId = s.ScopeId },
            TupleDestructuringStmt s => RewriteTupleDestructuring(s, context, program),
            WindowSetTitleStmt s => s with { Title = RewriteExpression(s.Title, context, program), ScopeId = s.ScopeId },
            WindowAppendLineStmt s => s with { Text = RewriteExpression(s.Text, context, program), ScopeId = s.ScopeId },
            WindowSetContentStmt s => s with { Lines = RewriteExpression(s.Lines, context, program), ScopeId = s.ScopeId },
            WindowScrollStmt s => s with { Delta = RewriteExpression(s.Delta, context, program), ScopeId = s.ScopeId },
            WindowSelectStmt s => s with { Target = RewriteExpression(s.Target, context, program), ScopeId = s.ScopeId },
            WindowThemeStmt s => s with { ThemeName = RewriteExpression(s.ThemeName, context, program), ScopeId = s.ScopeId },
            WindowDesktopStmt s => s with { Desktop = RewriteExpression(s.Desktop, context, program), ScopeId = s.ScopeId },
            WindowMoveDesktopStmt s => s with { Desktop = RewriteExpression(s.Desktop, context, program), ScopeId = s.ScopeId },
            SetDateFormatStmt s => s with { Format = RewriteExpression(s.Format, context, program), ScopeId = s.ScopeId },
            RddSetDefaultStmt s => s with { DriverName = RewriteExpression(s.DriverName, context, program), ScopeId = s.ScopeId },
            RddUseStmt s => s with
            {
                FileName = RewriteExpression(s.FileName, context, program),
                Driver = RewriteExpression(s.Driver, context, program),
                ScopeId = s.ScopeId
            },
            RddSetFieldStmt s => s with
            {
                Area = RewriteExpression(s.Area, context, program),
                Field = RewriteExpression(s.Field, context, program),
                Value = RewriteExpression(s.Value, context, program),
                ScopeId = s.ScopeId
            },
            RddSetReprocessStmt s => s with { Ms = RewriteExpression(s.Ms, context, program), ScopeId = s.ScopeId },
            RddSetOrderStmt s => s with
            {
                Area = RewriteExpression(s.Area, context, program),
                Order = RewriteExpression(s.Order, context, program),
                ScopeId = s.ScopeId
            },
            RddSetLockTimeoutStmt s => s with { Ms = RewriteExpression(s.Ms, context, program), ScopeId = s.ScopeId },
            RddOnEventStmt s => s with
            {
                EventName = RewriteExpression(s.EventName, context, program),
                Callback = s.Callback != null ? RewriteExpression(s.Callback, context, program) : null,
                ScopeId = s.ScopeId
            },
            RddOffEventStmt s => s with { EventName = RewriteExpression(s.EventName, context, program), ScopeId = s.ScopeId },
            RddCreateIndexStmt s => s with
            {
                Area = RewriteExpression(s.Area, context, program),
                Name = RewriteExpression(s.Name, context, program),
                Key = RewriteExpression(s.Key, context, program),
                ScopeId = s.ScopeId
            },
            RddCloseIndexStmt s => s with
            {
                Area = RewriteExpression(s.Area, context, program),
                Name = RewriteExpression(s.Name, context, program),
                ScopeId = s.ScopeId
            },
            RddCopyToStmt s => s with
            {
                Area = RewriteExpression(s.Area, context, program),
                FileName = RewriteExpression(s.FileName, context, program),
                OutDriver = s.OutDriver != null ? RewriteExpression(s.OutDriver, context, program) : null,
                ScopeId = s.ScopeId
            },
            RddSortToStmt s => s with
            {
                Area = RewriteExpression(s.Area, context, program),
                FileName = RewriteExpression(s.FileName, context, program),
                Fields = s.Fields != null ? RewriteExpression(s.Fields, context, program) : null,
                ScopeId = s.ScopeId
            },
            RddTotalOnStmt s => s with
            {
                Area = RewriteExpression(s.Area, context, program),
                Key = RewriteExpression(s.Key, context, program),
                ScopeId = s.ScopeId
            },
            LocalProcedureStmt s => s with { Procedure = RewriteProcedure(s.Procedure, context, program) },
            // DRAW TEXT: i suoi figli Expr (text/x/y/rotation/color) cadevano in `_ => stmt`
            // senza riscrittura (stessa famiglia della "Fase D"/CONTROL): una chiamata a
            // FUNCTION utente dentro il testo (`DRAW TEXT "a " + NumS(1), x, y`) restava un
            // ArrayAccessExpr. Il backend IL la emetteva come accesso a indice: la PRIMA
            // occorrenza cadeva nel ramo dinamico (CollectionIndexGet) e registrava un local
            // per il simbolo della procedura, la SECONDA nello stesso SUB trovava quel local
            // e finiva su GetArrayInfo -> "accesso a un array non DIMensionato".
            DrawTextStmt s => s with
            {
                Text = RewriteExpression(s.Text, context, program),
                X = RewriteExpression(s.X, context, program),
                Y = RewriteExpression(s.Y, context, program),
                Rotation = s.Rotation != null ? RewriteExpression(s.Rotation, context, program) : null,
                Color = s.Color != null ? RewriteExpression(s.Color, context, program) : null,
                ScopeId = s.ScopeId
            },
            PrintStmt s => s with
            {
                Arguments =
                [
                    ..s.Arguments.Select(a => a.Expression != null
                        ? a with { Expression = RewriteExpression(a.Expression, context, program) }
                        : a)
                ]
            },
            ReturnStmt s => s with
            {
                // `RETURN { |x| ... }` in una FUNCTION dichiarata AS FUNCTION(...) AS T:
                // tipizza la lambda col FunctionPointerType di ritorno (altrimenti resta
                // Inferred → firma gmb_int_t mentre il call-site casta gmb_double_t,
                // ABI rotta sull'emitter C#: risultato 1 invece di 6). Solo per LambdaExpr.
                Value = s.Value != null
                    ? RewriteExpression(s.Value,
                        s.Value is LambdaExpr && context.CurrentProcedure is FunctionDefinition { ReturnType: FunctionPointerType rfp }
                            ? context with { ExpectedType = rfp }
                            : context,
                        program)
                    : null
            },
            ForStmt s => s with
            {
                Start = RewriteExpression(s.Start, context, program),
                End = RewriteExpression(s.End, context, program),
                Step = s.Step != null ? RewriteExpression(s.Step, context, program) : null
            },
            ForEachInitStmt s => s with { CollectionExpr = RewriteExpression(s.CollectionExpr, context, program) },
            ConditionalJumpStmt s => s with
            {
                Condition = RewriteExpression(s.Condition, context, program)
            },
            ErrorStmt s => s with
            {
                ErrorCode = RewriteExpression(s.ErrorCode, context, program),
                Description = s.Description != null ? RewriteExpression(s.Description, context, program) : null
            },
            RedimStmt s => s with
            {
                ArrayTarget = RewriteTarget(s.ArrayTarget, context, program),
                Dimensions = [..s.Dimensions.Select(d => RewriteExpression(d, context, program))],
                Initializer = s.Initializer != null
                    ? RewriteExpression(s.Initializer, context, program) switch
                    {
                        ArrayLiteralExpr ale => ale with { Elements = TryFlattenArrayLiteral(ale) },
                        var expr => expr
                    }
                    : null
            },
            SwapStmt s => s with
            {
                Left = RewriteTarget(s.Left, context, program),
                Right = RewriteTarget(s.Right, context, program)
            },
            OnKeyStmt s => s with
            {
                KeyExpression = s.KeyExpression != null ? RewriteExpression(s.KeyExpression, context, program) : null,
                Action = RewriteOnKeyAction(s.Action, context, program)
            },
            OnMessageStmt s => s with
            {
                Action = RewriteMessageAction(s.Action, context, program)
            },
            OnSpriteCollisionStmt s => s,
            OnMouseStmt s => s with
            {
                Action = RewriteOnKeyAction(s.Action, context, program)
            },
            OnWindowStmt s => s with
            {
                Action = RewriteOnKeyAction(s.Action, context, program)
            },
            RaiseEventStmt s => new RaiseEventStmt(
                s.EventName,
                [.. s.Arguments.Select(a => RewriteExpression(a, context, program))],
                s.Token
            ) { ScopeId = s.ScopeId },
            // --- GESTIONE ERRORI E PROPAGAZIONE ---
            // Riscrive l'istruzione ON ERROR, risolvendo l'eventuale chiamata a procedura 
            // indipendente (handler procedurale) nel contesto semantico corrente.
            OnErrorStmt s => s with
            {
                ProcedureCall = s.ProcedureCall != null ? RewriteExpression(s.ProcedureCall, context, program) : null
            },
            // L'istruzione Rethrow non contiene espressioni o target figli da risolvere; 
            // viene restituita l'istanza originale timbrata.
            RethrowStmt => stmt,
            ThrowStmt s => s with { Value = RewriteExpression(s.Value, context, program) },
            AddHandlerStmt s => new AddHandlerStmt(
                RewriteExpression(s.EventAccess, context, program),
                RewriteExpression(s.HandlerExpr, context, program),
                s.Token
            ) { ScopeId = s.ScopeId },

            RemoveHandlerStmt s => new RemoveHandlerStmt(
                RewriteExpression(s.EventAccess, context, program),
                RewriteExpression(s.HandlerExpr, context, program),
                s.Token
            ) { ScopeId = s.ScopeId },
            RandomizeStmt s => s with
            {
                Seed = s.Seed != null ? RewriteExpression(s.Seed, context, program) : null
            },
            RunStmt s => s with { Command = RewriteExpression(s.Command, context, program) },
            KeyGenStmt s => s with
            {
                Algorithm = RewriteExpression(s.Algorithm, context, program),
                PrivateKeyHandle = RewriteTarget(s.PrivateKeyHandle, context, program),
                PublicKeyHandle = RewriteTarget(s.PublicKeyHandle, context, program),
                KeySize = s.KeySize != null ? RewriteExpression(s.KeySize, context, program) : null
            },
            KeyCloseStmt s => s with { KeyHandle = RewriteExpression(s.KeyHandle, context, program) },
            MacroStmt s => s with { Expression = RewriteExpression(s.Expression, context, program) },
            YieldStmt s => s with { ValueToYield = RewriteExpression(s.ValueToYield, context, program) },
            DataStmt s => s with
            {
                Values = [..s.Values.Select(v => RewriteExpression(v, context, program))]
            },
            ExpressionStmt { Expression: MethodCallExpr } mcs
                when TryRewriteControlMethod(mcs.Expression, program, out var ctlS) && ctlS is not null => ctlS,
            ExpressionStmt s => s with
            {
                Expression = RewriteExpression(s.Expression, context, program)
            },

            BaseConstructorCallStmt s => s with
            {
                Args = [..s.Args.Select(a => RewriteExpression(a, context, program))]
            },
            LocalTypeStmt s => RewriteLocalTypeStatement(s, context, program),

            SoundStmt s => s with
            {
                Frequency = RewriteExpression(s.Frequency, context, program),
                Duration = RewriteExpression(s.Duration, context, program),
                Volume = s.Volume != null ? RewriteExpression(s.Volume, context, program) : null
            },

            WaitStmt s => s with { WaitGroup = RewriteExpression(s.WaitGroup, context, program) },
            SleepStmt s => s with { Duration = RewriteExpression(s.Duration, context, program) },
            SendStmt s => s with
            {
                Value = RewriteExpression(s.Value, context, program),
                Channel = RewriteExpression(s.Channel, context, program)
            },
            ReceiveStmt s => s with
            {
                Destination = RewriteTarget(s.Destination, context, program),
                Channel = RewriteExpression(s.Channel, context, program)
            },
            DoneStmt s => s with { WaitGroup = RewriteExpression(s.WaitGroup, context, program) },

            GoSelectStmt s => s with
            {
                Cases =
                [
                    ..s.Cases.Select(c => new GoSelectCase(
                        CommunicationStmt: RewriteSimpleStatement(c.CommunicationStmt, context, procedureContext,
                            program),
                        Body:
                        [
                            ..c.Body.Select(stmtInBody =>
                                RewriteStatement(stmtInBody, context, procedureContext, program).Rewritten)
                        ]
                    ))
                ],
                DefaultCase = s.DefaultCase != null
                    ?
                    [
                        ..s.DefaultCase.Value.Select(stmtInDef =>
                            RewriteStatement(stmtInDef, context, procedureContext, program).Rewritten)
                    ]
                    : null
            },

            // --- INPUT & KEYBOARD ---
            InputStmt s => s with
            {
                Variables = [..s.Variables.Select(v => RewriteTarget(v, context, program))],
                Prompt = s.Prompt != null ? RewriteExpression(s.Prompt, context, program) : null,
                MaskExpr = s.MaskExpr != null ? RewriteExpression(s.MaskExpr, context, program) : null
            },
            LineInputStmt s => s with
            {
                Variable = RewriteTarget(s.Variable, context, program),
                Prompt = s.Prompt != null ? RewriteExpression(s.Prompt, context, program) : null,
                MaskExpr = s.MaskExpr != null ? RewriteExpression(s.MaskExpr, context, program) : null
            },
            InkeyStmt s => s with
            {
                Variable = RewriteTarget(s.Variable, context, program),
                TimeoutExpr = s.TimeoutExpr != null ? RewriteExpression(s.TimeoutExpr, context, program) : null
            },
            GetKeyboardStmt s => s with { Variable = RewriteTarget(s.Variable, context, program) },

            // --- GRAFICA & CONSOLE (INTEGRAZIONE) ---
            LocateStmt s => s with
            {
                LineExpr = RewriteExpression(s.LineExpr, context, program),
                ColumnExpr = RewriteExpression(s.ColumnExpr, context, program)
            },
            ScreenStmt s => s with
            {
                Mode = RewriteExpression(s.Mode, context, program),
                ActivePage = s.ActivePage != null ? RewriteExpression(s.ActivePage, context, program) : null,
                VisiblePage = s.VisiblePage != null ? RewriteExpression(s.VisiblePage, context, program) : null,
                CloseRepl = s.CloseRepl != null ? RewriteExpression(s.CloseRepl, context, program) : null
            },
            TextModeStmt tms => tms with { Mode = RewriteExpression(tms.Mode, context, program) },
            ClsStmt s => s with { Color = s.Color != null ? RewriteExpression(s.Color, context, program) : null },
            ColorStmt s => s with
            {
                Foreground = s.Foreground != null ? RewriteExpression(s.Foreground, context, program) : null,
                Background = s.Background != null ? RewriteExpression(s.Background, context, program) : null
            },
            ClipStmt s => s with
            {
                Argument = s.Argument != null ? RewriteExpression(s.Argument, context, program) : null,
                Destination = s.Destination != null ? RewriteTarget(s.Destination, context, program) : null
            },
            CursorShapeStmt s => s with { Shape = RewriteExpression(s.Shape, context, program) },
            ClearRectStmt s => s with
            {
                X1 = RewriteExpression(s.X1, context, program),
                Y1 = RewriteExpression(s.Y1, context, program),
                X2 = RewriteExpression(s.X2, context, program),
                Y2 = RewriteExpression(s.Y2, context, program)
            },
            AudioControlStmt s => s with
            {
                Argument = s.Argument != null ? RewriteExpression(s.Argument, context, program) : null
            },
            PlayStmt s => s with { Action = RewriteAudioAction(s.Action, context, program) },

            // Per Palette, SaveScreen, RestoreScreen
            PaletteStmt s => s with
            {
                Index = RewriteExpression(s.Index, context, program), R = RewriteExpression(s.R, context, program),
                G = RewriteExpression(s.G, context, program), B = RewriteExpression(s.B, context, program)
            },
            PaletteUseStmt s => s with { Name = RewriteExpression(s.Name, context, program) },
            PaletteDefineStmt s => s with
            {
                Name = RewriteExpression(s.Name, context, program),
                Colors = RewriteExpression(s.Colors, context, program)
            },
            PaletteReplaceStmt s => s with
            {
                Name = RewriteExpression(s.Name, context, program),
                Colors = RewriteExpression(s.Colors, context, program)
            },
            PaletteSaveStmt s => s with { FileName = RewriteExpression(s.FileName, context, program) },
            PaletteLoadStmt s => s with { FileName = RewriteExpression(s.FileName, context, program) },
            SaveScreenStmt s => s with { Name = RewriteExpression(s.Name, context, program) },
            RestoreScreenStmt s => s with { Name = RewriteExpression(s.Name, context, program) },
            ViewPortStmt s => s with
            {
                X1 = RewriteExpression(s.X1, context, program),
                Y1 = RewriteExpression(s.Y1, context, program),
                X2 = RewriteExpression(s.X2, context, program),
                Y2 = RewriteExpression(s.Y2, context, program)
            },
            VectorCommandStmt s => s with
            {
                Arguments = [..s.Arguments.Select(a => RewriteExpression(a, context, program))]
            },
            MatrixCommandStmt s => s with
            {
                Arguments = [..s.Arguments.Select(a => RewriteExpression(a, context, program))]
            },

            // --- FILE I/O ---
            OpenStmt s => s with
            {
                FileName = RewriteExpression(s.FileName, context, program),
                FileNumber = RewriteExpression(s.FileNumber, context, program),
                RecordLength = s.RecordLength != null ? RewriteExpression(s.RecordLength, context, program) : null,
                EncodingExpr = s.EncodingExpr != null ? RewriteExpression(s.EncodingExpr, context, program) : null
            },
            CloseStmt s => s with
            {
                FileNumbers = [..s.FileNumbers.Select(f => RewriteExpression(f, context, program))]
            },
            PrintFileStmt s => s with
            {
                FileNumber = RewriteExpression(s.FileNumber, context, program),
                Arguments =
                [
                    ..s.Arguments.Select(a =>
                        a.Expression != null
                            ? a with { Expression = RewriteExpression(a.Expression, context, program) }
                            : a)
                ]
            },
            WriteFileStmt s => s with
            {
                FileNumber = RewriteExpression(s.FileNumber, context, program),
                Arguments = [..s.Arguments.Select(a => RewriteExpression(a, context, program))]
            },
            InputFileStmt s => s with
            {
                FileNumber = RewriteExpression(s.FileNumber, context, program),
                Variables = [..s.Variables.Select(v => RewriteTarget(v, context, program))]
            },
            LineInputFileStmt s => s with
            {
                FileNumber = RewriteExpression(s.FileNumber, context, program),
                Variable = RewriteTarget(s.Variable, context, program)
            },
            GetFileStmt s => s with
            {
                FileNumber = RewriteExpression(s.FileNumber, context, program),
                RecordNumber = s.RecordNumber != null ? RewriteExpression(s.RecordNumber, context, program) : null,
                Destination = RewriteTarget(s.Destination, context, program)
            },
            PutFileStmt s => s with
            {
                FileNumber = RewriteExpression(s.FileNumber, context, program),
                RecordNumber = s.RecordNumber != null ? RewriteExpression(s.RecordNumber, context, program) : null,
                Source = RewriteExpression(s.Source, context, program)
            },
            SeekStmt s => s with
            {
                FileNumber = RewriteExpression(s.FileNumber, context, program),
                PositionExpr = RewriteExpression(s.PositionExpr, context, program)
            },
            LockStmt s => s with
            {
                FileNumber = RewriteExpression(s.FileNumber, context, program),
                RecordNumber = s.RecordNumber != null ? RewriteExpression(s.RecordNumber, context, program) : null
            },
            UnlockStmt s => s with
            {
                FileNumber = RewriteExpression(s.FileNumber, context, program),
                RecordNumber = s.RecordNumber != null ? RewriteExpression(s.RecordNumber, context, program) : null
            },
            IoWatchStmt s => s with
            {
                Path = RewriteExpression(s.Path, context, program),
                EventsMask = RewriteExpression(s.EventsMask, context, program)
            },
            IoUnwatchStmt s => s with { Path = RewriteExpression(s.Path, context, program) },
            KillStmt s => s with { FileName = RewriteExpression(s.FileName, context, program) },
            RenameStmt s => s with
            {
                OldFileName = RewriteExpression(s.OldFileName, context, program),
                NewFileName = RewriteExpression(s.NewFileName, context, program)
            },
            MkDirStmt s => s with { DirectoryPath = RewriteExpression(s.DirectoryPath, context, program) },
            RmDirStmt s => s with
            {
                DirectoryPath = RewriteExpression(s.DirectoryPath, context, program),
                RecursiveExpr = s.RecursiveExpr != null ? RewriteExpression(s.RecursiveExpr, context, program) : null
            },
            ChDirStmt s => s with { DirectoryPath = RewriteExpression(s.DirectoryPath, context, program) },
            CopyStmt s => s with
            {
                SourceFile = RewriteExpression(s.SourceFile, context, program),
                DestinationFile = RewriteExpression(s.DestinationFile, context, program)
            },
            MoveStmt s => s with
            {
                SourceFile = RewriteExpression(s.SourceFile, context, program),
                DestinationFile = RewriteExpression(s.DestinationFile, context, program)
            },
            XCopyStmt s => s with
            {
                SourceDirectory = RewriteExpression(s.SourceDirectory, context, program),
                DestinationDirectory = RewriteExpression(s.DestinationDirectory, context, program),
                RecursiveExpr = s.RecursiveExpr != null ? RewriteExpression(s.RecursiveExpr, context, program) : null,
                OverwriteExpr = s.OverwriteExpr != null ? RewriteExpression(s.OverwriteExpr, context, program) : null
            },
            // --- MEMORY MAP ---
            MemoryMapStmt s =>
                s with
                {
                    FilePath = RewriteExpression(s.FilePath, context, program),
                    Handle = RewriteTarget(s.Handle, context, program)
                },
            MemoryMapPokeStmt s => RewriteMemoryMapPokeStatement(s, context, program),
            MemoryMapWriteBytesStmt s => RewriteMemoryMapWriteBytesStatement(s, context, program),
            // --- ARRAYS & COLLECTIONS ---
            AAddStmt s => s with
            {
                Array = RewriteTarget(s.Array, context, program),
                Value = RewriteExpression(s.Value, context, program)
            },
            AInsStmt s => s with
            {
                Array = RewriteTarget(s.Array, context, program),
                PositionExpr = RewriteExpression(s.PositionExpr, context, program)
            },
            ADelStmt s => s with
            {
                Array = RewriteTarget(s.Array, context, program),
                PositionExpr = RewriteExpression(s.PositionExpr, context, program)
            },
            ASortStmt s => RewriteASortStatement(s, context, program),
            AEvalStmt s => RewriteAevalStatement(s, context, program),
            AFillStmt s => s with
            {
                Array = RewriteTarget(s.Array, context, program),
                Value = RewriteExpression(s.Value, context, program),
                Start = s.Start != null ? RewriteExpression(s.Start, context, program) : null,
                Count = s.Count != null ? RewriteExpression(s.Count, context, program) : null
            },
            ACopyStmt s => s with
            {
                Source = RewriteTarget(s.Source, context, program),
                Destination = RewriteTarget(s.Destination, context, program),
                SourceStart = s.SourceStart != null ? RewriteExpression(s.SourceStart, context, program) : null,
                Count = s.Count != null ? RewriteExpression(s.Count, context, program) : null,
                DestStart = s.DestStart != null ? RewriteExpression(s.DestStart, context, program) : null
            },
            AShuffleStmt s => s with
            {
                Array = RewriteTarget(s.Array, context, program)
            },

            // --- JSON ---
            JsonWriteValueStmt s => s with
            {
                Key = RewriteExpression(s.Key, context, program),
                Value = RewriteExpression(s.Value, context, program),
                Section = s.Section != null ? RewriteExpression(s.Section, context, program) : null
            },
            JsonWriteSectionStmt s => s with
            {
                Source = RewriteExpression(s.Source, context, program),
                Section = s.Section != null ? RewriteExpression(s.Section, context, program) : null
            },
            JsonSaveStmt s => s with
            {
                Source = RewriteExpression(s.Source, context, program),
                FilePath = RewriteExpression(s.FilePath, context, program)
            },

            ExportToFileStmt s => s with
            {
                Area = RewriteExpression(s.Area, context, program),
                FilePath = RewriteExpression(s.FilePath, context, program)
            },
            ImportFromFileStmt s => s with
            {
                Area = RewriteExpression(s.Area, context, program),
                FilePath = RewriteExpression(s.FilePath, context, program)
            },
            // --- NETWORKING & HTTP  ---
            SslOpenStmt s => s with
            {
                Port = RewriteExpression(s.Port, context, program),
                Handle = RewriteTarget(s.Handle, context, program),
                CertificatePath = RewriteExpression(s.CertificatePath, context, program),
                CertificatePassword = s.CertificatePassword != null
                    ? RewriteExpression(s.CertificatePassword, context, program)
                    : null
            },
            SslConnectStmt s => s with
            {
                Host = RewriteExpression(s.Host, context, program),
                Port = s.Port != null ? RewriteExpression(s.Port, context, program) : null,
                Handle = RewriteTarget(s.Handle, context, program),
                Timeout = s.Timeout != null ? RewriteExpression(s.Timeout, context, program) : null
            },
            UdpOpenStmt s => s with
            {
                Port = s.Port != null ? RewriteExpression(s.Port, context, program) : null,
                Handle = RewriteTarget(s.Handle, context, program),
                Engine = s.Engine != null ? RewriteExpression(s.Engine, context, program) : null
            },
            UdpSendStmt s => s with
            {
                Handle = RewriteExpression(s.Handle, context, program),
                Host = RewriteExpression(s.Host, context, program),
                Port = RewriteExpression(s.Port, context, program),
                Data = RewriteExpression(s.Data, context, program)
            },
            UdpRecvStmt s => s with
            {
                Handle = RewriteExpression(s.Handle, context, program),
                Buffer = RewriteTarget(s.Buffer, context, program),
                RemoteIp = RewriteTarget(s.RemoteIp, context, program),
                RemotePort = RewriteTarget(s.RemotePort, context, program),
                Timeout = s.Timeout != null ? RewriteExpression(s.Timeout, context, program) : null
            },
            UdpCloseStmt s => s with { Handle = RewriteExpression(s.Handle, context, program) },
            ShutdownStmt s => s with
            {
                Handle = RewriteExpression(s.Handle, context, program),
                Mode = RewriteExpression(s.Mode, context, program)
            },
            // WebSocketConnectStmt: prima mancava da questo dispatcher → il Path/Timeout
            // (ArrayAccessExpr come `CdpPath(port)` — il parser non sa a parse-time se è una
            // funzione) NON venivano mai riscritti a FunctionExpr → l'emitter IL li trattava
            // come indice di collezione (CollectionIndexGet(0, port)) → path vuoto/InvalidProgram.
            WebSocketConnectStmt s => s with
            {
                Host = RewriteExpression(s.Host, context, program),
                Port = RewriteExpression(s.Port, context, program),
                Handle = RewriteTarget(s.Handle, context, program),
                Path = s.Path != null ? RewriteExpression(s.Path, context, program) : null,
                Timeout = s.Timeout != null ? RewriteExpression(s.Timeout, context, program) : null
            },
            // Tcp*: stesso gap di WebSocketConnectStmt — gli argomenti (handle/host/port/data)
            // non venivano riscritti: una call utente come argomento (es. TcpConnect h, MyHost())
            // restava ArrayAccessExpr → indice di collezione invece di chiamata (parità WS).
            TcpConnectStmt s => s with
            {
                Host = RewriteExpression(s.Host, context, program),
                Port = RewriteExpression(s.Port, context, program),
                Handle = RewriteTarget(s.Handle, context, program),
                Engine = s.Engine != null ? RewriteExpression(s.Engine, context, program) : null,
                Timeout = s.Timeout != null ? RewriteExpression(s.Timeout, context, program) : null
            },
            TcpAcceptStmt s => s with
            {
                ServerHandle = RewriteExpression(s.ServerHandle, context, program),
                ClientHandle = RewriteTarget(s.ClientHandle, context, program),
                Timeout = s.Timeout != null ? RewriteExpression(s.Timeout, context, program) : null
            },
            TcpRecvStmt s => s with
            {
                ClientHandle = RewriteExpression(s.ClientHandle, context, program),
                Buffer = RewriteTarget(s.Buffer, context, program),
                BytesReceived = RewriteTarget(s.BytesReceived, context, program),
                Timeout = s.Timeout != null ? RewriteExpression(s.Timeout, context, program) : null
            },
            TcpSendStmt s => s with
            {
                ClientHandle = RewriteExpression(s.ClientHandle, context, program),
                Data = RewriteExpression(s.Data, context, program)
            },
            TcpCloseStmt s => s with { Handle = RewriteExpression(s.Handle, context, program) },
            SetOptionStmt s => s with
            {
                Handle = RewriteExpression(s.Handle, context, program),
                Option = RewriteExpression(s.Option, context, program),
                Value = RewriteExpression(s.Value, context, program)
            },
            HttpFetchStmt s => s with
            {
                Url = RewriteExpression(s.Url, context, program),
                Options = s.Options != null ? RewriteExpression(s.Options, context, program) : null
            },
            HttpServerStmt s => s with
            {
                Port = s.Port != null ? RewriteExpression(s.Port, context, program) : null,
                CertificatePath = s.CertificatePath != null
                    ? RewriteExpression(s.CertificatePath, context, program)
                    : null,
                CertificatePassword = s.CertificatePassword != null
                    ? RewriteExpression(s.CertificatePassword, context, program)
                    : null
            },
            HttpRouteStmt s => s with
            {
                Method = RewriteExpression(s.Method, context, program),
                Path = RewriteExpression(s.Path, context, program)
            },
            HttpUseStmt s => s with { Path = s.Path != null ? RewriteExpression(s.Path, context, program) : null },
            HttpUseNextStmt s => s with { NextHandle = RewriteExpression(s.NextHandle, context, program) },
            HttpConfigStmt s => s with { ConfigObject = RewriteExpression(s.ConfigObject, context, program) },
            ViewPrintStmt s => s with
            {
                TopLine = s.TopLine != null ? RewriteExpression(s.TopLine, context, program) : null,
                BottomLine = s.BottomLine != null ? RewriteExpression(s.BottomLine, context, program) : null
            },
            // --- Istruzioni che non necessitano di riscrittura ---
            _ => stmt
        };
    }

    /// <summary>
    /// [HELPER] Riscrive un'istruzione AEvalStmt, eseguendo l'inferenza contestuale dei tipi
    /// sui parametri della lambda (code block) in base al tipo dell'array.
    /// </summary>
    private static AEvalStmt RewriteAevalStatement(AEvalStmt stmt, AstRewriteContext context, BasicProgram program)
    {
        // 1. Riscrivi e inferisci il tipo dell'array
        var rewrittenArray = RewriteTarget(stmt.Array, context, program);
        var arrayType = InferTargetType(rewrittenArray, context, program);

        var elementType = arrayType switch
        {
            ArrayType at => at.ElementType,
            ListType lt => lt.ElementType,
            _ => null
        };

        Expr rewrittenAction;

        // 2. Se l'azione è una lambda e abbiamo identificato il tipo elemento, iniettiamo il contesto
        if (stmt.ActionFunction is LambdaExpr lambda && elementType != null)
        {
            var lambdaScope = new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase);

            // Parametro 1: Elemento (assume il tipo dell'array)
            if (lambda.Parameters.Length > 0)
            {
                lambdaScope[lambda.Parameters[0].OriginalName.Lexeme] = elementType;
            }

            // Parametro 2: Indice (è sempre Long)
            if (lambda.Parameters.Length > 1)
            {
                lambdaScope[lambda.Parameters[1].OriginalName.Lexeme] = new LongType();
            }

            // Riscrivi la lambda con il nuovo scope iniettato
            var lambdaContext = context with { LambdaScope = lambdaScope };
            rewrittenAction = RewriteExpression(lambda, lambdaContext, program);
        }
        else
        {
            rewrittenAction = RewriteExpression(stmt.ActionFunction, context, program);
        }

        return stmt with { Array = rewrittenArray, ActionFunction = rewrittenAction };
    }

    /// <summary>
    /// [HELPER] Riscrive un'istruzione ASortStmt con inferenza contestuale per la funzione di comparazione.
    /// </summary>
    private static ASortStmt RewriteASortStatement(ASortStmt stmt, AstRewriteContext context, BasicProgram program)
    {
        var rewrittenArray = RewriteTarget(stmt.Array, context, program);
        var arrayType = InferTargetType(rewrittenArray, context, program);

        var elementType = arrayType switch
        {
            ArrayType at => at.ElementType,
            ListType lt => lt.ElementType,
            _ => null
        };

        var rewrittenCompare = stmt.CompareFunction;

        if (stmt.CompareFunction is LambdaExpr lambda && elementType != null)
        {
            var lambdaScope = new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase);
            // ASORT passa due elementi da confrontare, entrambi dello stesso tipo dell'array
            if (lambda.Parameters.Length > 0) lambdaScope[lambda.Parameters[0].OriginalName.Lexeme] = elementType;
            if (lambda.Parameters.Length > 1) lambdaScope[lambda.Parameters[1].OriginalName.Lexeme] = elementType;

            var lambdaContext = context with { LambdaScope = lambdaScope };
            rewrittenCompare = RewriteExpression(lambda, lambdaContext, program);
        }
        else if (stmt.CompareFunction != null)
        {
            rewrittenCompare = RewriteExpression(stmt.CompareFunction, context, program);
        }

        var rewrittenOrder = stmt.Order != null ? RewriteExpression(stmt.Order, context, program) : null;

        return stmt with
        {
            Array = rewrittenArray,
            CompareFunction = rewrittenCompare,
            Order = rewrittenOrder
        };
    }

    /// <summary>
    /// Riscrive un nodo MemoryMapPokeStmt, validando i tipi degli argomenti.
    /// Questo metodo incapsula la logica di tipo per l'handle, l'offset e il valore del byte.
    /// Assicura che il contesto di inferenza (con `ExpectedType`) usato per la validazione finale
    /// sia lo stesso usato per riscrivere l'espressione, risolvendo il problema dell'inferenza errata.
    /// Aggiunti log dettagliati per il debug.
    /// </summary>
    /// <param name="s">Lo statement MemoryMapPokeStmt originale.</param>
    /// <param name="context">Il contesto di riscrittura corrente.</param>
    /// <param name="program">L'istanza del programma.</param>
    /// <returns>Il nodo MemoryMapPokeStmt riscritto con le espressioni interne risolte.</returns>
    /// <exception cref="BasicException">Lanciata se i tipi degli argomenti non sono compatibili.</exception>
    private static MemoryMapPokeStmt RewriteMemoryMapPokeStatement(MemoryMapPokeStmt s, AstRewriteContext context,
        BasicProgram program)
    {
        const bool log = false; // Log forzato per il debug.
        Log($"[REWRITE-MMAPPOKE] Inizio riscrittura e validazione per POKE alla riga {s.Token.Line}.", log);

        // --- 1. Riscrivi gli argomenti con il ExpectedType corretto ---
        // Handle (LONG)
        var handleContext = context with { ExpectedType = new LongType() };
        var rewrittenHandle = RewriteExpression(s.Handle, handleContext, program);
        Log($"[REWRITE-MMAPPOKE]   -> Handle riscritto: '{rewrittenHandle}'.", log);

        // Offset (LONG)
        var offsetContext = context with { ExpectedType = new LongType() };
        var rewrittenOffset = RewriteExpression(s.Offset, offsetContext, program);
        Log($"[REWRITE-MMAPPOKE]   -> Offset riscritto: '{rewrittenOffset}'.", log);

        // Value (BYTE)
        var valueContext = context with { ExpectedType = new ByteType() };
        var rewrittenValue = RewriteExpression(s.Value, valueContext, program);
        Log($"[REWRITE-MMAPPOKE]   -> Value riscritto: '{rewrittenValue}'.", log);

        // --- 2. Validazione finale dei tipi usando i contesti specifici ---
        // Chiamiamo LinkerInferExpressionType con il contesto che ha l'ExpectedType corretto
        // per assicurarci che l'inferenza sia guidata anche per i letterali.
        var handleType = LinkerInferExpressionType(rewrittenHandle, handleContext, program);
        var offsetType = LinkerInferExpressionType(rewrittenOffset, offsetContext, program);
        var valueType = LinkerInferExpressionType(rewrittenValue, valueContext, program);

        Log($"[REWRITE-MMAPPOKE]   -> Handle Type (inferred): {handleType}. Expected: LONG.", log);
        Log($"[REWRITE-MMAPPOKE]   -> Offset Type (inferred): {offsetType}. Expected: LONG.", log);
        Log($"[REWRITE-MMAPPOKE]   -> Value Type (inferred): {valueType}. Expected: BYTE.", log);


        if (handleType is not LongType && handleType is not InferredType)
            throw CreateError(rewrittenHandle,
                $"L'handle per MEMORYMAP.MMAPPOKE deve essere di tipo LONG, trovato '{handleType}'.",
                ErrorCode.TypeMismatch);
        if (offsetType is not LongType && offsetType is not InferredType)
            throw CreateError(rewrittenOffset,
                $"L'offset per MEMORYMAP.MMAPPOKE deve essere di tipo LONG, trovato '{offsetType}'.",
                ErrorCode.TypeMismatch);
        if (valueType is not ByteType && valueType is not InferredType)
            throw CreateError(rewrittenValue,
                $"Il valore per MEMORYMAP.MMAPPOKE deve essere di tipo BYTE, trovato '{valueType}'.",
                ErrorCode.TypeMismatch);

        Log($"[REWRITE-MMAPPOKE] Fine validazione per POKE. Tutti i tipi OK.", log);
        return s with { Handle = rewrittenHandle, Offset = rewrittenOffset, Value = rewrittenValue };
    }

    /// <summary>
    /// Riscrive un nodo MemoryMapWriteBytesStmt, validando i tipi degli argomenti.
    /// Questo metodo incapsula la logica di tipo per l'handle, l'offset, l'array sorgente e il conteggio opzionale.
    /// Assicura che il contesto di inferenza (con `ExpectedType`) usato per la validazione finale
    /// sia lo stesso usato per riscrivere l'espressione, risolvendo il problema dell'inferenza errata.
    /// Aggiunti log dettagliati per il debug.
    /// </summary>
    /// <param name="s">Lo statement MemoryMapWriteBytesStmt originale.</param>
    /// <param name="context">Il contesto di riscrittura corrente.</param>
    /// <param name="program">L'istanza del programma.</param>
    /// <returns>Il nodo MemoryMapWriteBytesStmt riscritto con le espressioni interne risolte.</returns>
    /// <exception cref="BasicException">Lanciata se i tipi degli argomenti non sono compatibili.</exception>
    private static MemoryMapWriteBytesStmt RewriteMemoryMapWriteBytesStatement(MemoryMapWriteBytesStmt s,
        AstRewriteContext context, BasicProgram program)
    {
        const bool log = false; // Log forzato per il debug.
        Log($"[REWRITE-MMAPWRITEBYTES] Inizio riscrittura e validazione per WRITEBYTES alla riga {s.Token.Line}.", log);

        // --- 1. Riscrivi gli argomenti con il ExpectedType corretto ---
        // Handle (LONG)
        var handleContext = context with { ExpectedType = new LongType() };
        var rewrittenHandle = RewriteExpression(s.Handle, handleContext, program);
        Log($"[REWRITE-MMAPWRITEBYTES]   -> Handle riscritto: '{rewrittenHandle}'.", log);

        // Offset (LONG)
        var offsetContext = context with { ExpectedType = new LongType() };
        var rewrittenOffset = RewriteExpression(s.Offset, offsetContext, program);
        Log($"[REWRITE-MMAPWRITEBYTES]   -> Offset riscritto: '{rewrittenOffset}'.", log);

        // SourceArray (BYTE())
        var sourceArrayContext = context with { ExpectedType = new ArrayType(new ByteType()) };
        var rewrittenSourceArray = RewriteExpression(s.SourceArray, sourceArrayContext, program);
        Log($"[REWRITE-MMAPWRITEBYTES]   -> SourceArray riscritto: '{rewrittenSourceArray}'.", log);

        // Count (LONG, opzionale)
        Expr? rewrittenCount = null;
        AstRewriteContext countContext = context; // Inizializza con il contesto generale
        if (s.Count != null)
        {
            countContext = context with { ExpectedType = new LongType() };
            rewrittenCount = RewriteExpression(s.Count, countContext, program);
            Log($"[REWRITE-MMAPWRITEBYTES]   -> Count riscritto: '{rewrittenCount}'.", log);
        }
        else
        {
            Log($"[REWRITE-MMAPWRITEBYTES]   -> Count è nullo (opzionale).", log);
        }

        // --- 2. Validazione finale dei tipi usando i contesti specifici ---
        var handleType = LinkerInferExpressionType(rewrittenHandle, handleContext, program);
        var offsetType = LinkerInferExpressionType(rewrittenOffset, offsetContext, program);
        var sourceArrayType = LinkerInferExpressionType(rewrittenSourceArray, sourceArrayContext, program);
        DataType? countType = null;
        if (rewrittenCount is not null)
        {
            countType = LinkerInferExpressionType(rewrittenCount, countContext, program);
        }

        Log($"[REWRITE-MMAPWRITEBYTES]   -> Handle Type (inferred): {handleType}. Expected: LONG.", log);
        Log($"[REWRITE-MMAPWRITEBYTES]   -> Offset Type (inferred): {offsetType}. Expected: LONG.", log);
        Log($"[REWRITE-MMAPWRITEBYTES]   -> SourceArray Type (inferred): {sourceArrayType}. Expected: ARRAY OF BYTE.",
            log);
        if (rewrittenCount is not null)
            Log($"[REWRITE-MMAPWRITEBYTES]   -> Count Type (inferred): {countType}. Expected: LONG.", log);


        if (handleType is not LongType && handleType is not InferredType)
            throw CreateError(rewrittenHandle,
                $"L'handle per MEMORYMAP.MMAPWRITEBYTES deve essere di tipo LONG, trovato '{handleType}'.",
                ErrorCode.TypeMismatch);
        if (offsetType is not LongType && offsetType is not InferredType)
            throw CreateError(rewrittenOffset,
                $"L'offset per MEMORYMAP.MMAPWRITEBYTES deve essere di tipo LONG, trovato '{offsetType}'.",
                ErrorCode.TypeMismatch);
        if (sourceArrayType is not ArrayType { ElementType: ByteType } && sourceArrayType is not InferredType)
            throw CreateError(rewrittenSourceArray,
                $"L'array sorgente per MEMORYMAP.MMAPWRITEBYTES deve essere di tipo BYTE(), trovato '{sourceArrayType}'.",
                ErrorCode.TypeMismatch);

        // Aggiunto controllo esplicito per rewrittenCount
        if (rewrittenCount is not null && countType is not null && countType is not LongType &&
            countType is not InferredType)
            throw CreateError(rewrittenCount,
                $"Il conteggio per MEMORYMAP.MMAPWRITEBYTES deve essere di tipo LONG, trovato '{countType}'.",
                ErrorCode.TypeMismatch);

        Log($"[REWRITE-MMAPWRITEBYTES] Fine validazione per WRITEBYTES. Tutti i tipi OK.", log);
        return s with
        {
            Handle = rewrittenHandle, Offset = rewrittenOffset, SourceArray = rewrittenSourceArray,
            Count = rewrittenCount
        };
    }

    /// <summary>
    /// [FASE 3] Riscrive e valida un'istruzione `HttpServerStmt`.
    /// Questo metodo è il punto di controllo semantico per il comando `HTTP SERVER`.
    /// La sua responsabilità principale è validare che la subroutine fornita come gestore
    /// rispetti il contratto richiesto dal server HTTP.
    /// </summary>
    /// <remarks>
    /// Il contratto del gestore è: `SUB Nome(req AS HTTPREQUEST, res AS HTTPRESPONSE)`.
    ///
    /// Logica di Validazione:
    /// 1.  Risoluzione del Simbolo: Cerca la subroutine specificata nella clausola `HANDLER`
    ///     all'interno della SymbolTable.
    /// 2.  Validazione del Tipo: Si assicura che il simbolo trovato sia una `ProcedureDefinition`
    ///     e, più specificamente, una `SUB` (con tipo di ritorno `VOID`).
    /// 3.  Validazione della Firma (Arity Check): Controlla che la subroutine accetti esattamente due parametri.
    /// 4.  Validazione dei Tipi dei Parametri: Verifica che il primo parametro sia di tipo `HTTPREQUEST`
    ///     e il secondo di tipo `HTTPRESPONSE`.
    /// 5.  Riscrittura Espressioni: Riscrive le altre espressioni presenti nello statement (porta, certificato).
    ///
    /// Se una qualsiasi di queste validazioni fallisce, viene lanciata una `BasicException` con un
    /// messaggio di errore chiaro, interrompendo la compilazione.
    /// </remarks>
    /// <param name="stmt">Il nodo `HttpServerStmt` da riscrivere e validare.</param>
    /// <param name="context">Il contesto di riscrittura corrente.</param>
    /// <param name="program">L'istanza del programma per l'accesso alla SymbolTable.</param>
    /// <returns>Una tupla contenente il nodo `HttpServerStmt` validato e il contesto invariato.</returns>
    private static (Stmt Rewritten, AstRewriteContext NewContext) RewriteHttpServerStatement(HttpServerStmt stmt,
        AstRewriteContext context, BasicProgram program)
    {
        // Se è un comando STOP o un avvio senza handler specificato, non c'è nulla da validare.
        if (stmt.IsStopCommand || stmt.Callback == null)
        {
            var rewrittenPort = stmt.Port != null ? RewriteExpression(stmt.Port, context, program) : null;
            var rewrittenCertPath = stmt.CertificatePath != null
                ? RewriteExpression(stmt.CertificatePath, context, program)
                : null;
            var rewrittenCertPass = stmt.CertificatePassword != null
                ? RewriteExpression(stmt.CertificatePassword, context, program)
                : null;

            var rewrittenStmtStop = stmt with
            {
                Port = rewrittenPort, CertificatePath = rewrittenCertPath, CertificatePassword = rewrittenCertPass
            };
            return (rewrittenStmtStop, context);
        }

        // 1. Risoluzione del Simbolo del Gestore
        var handlerName = stmt.Callback.Lexeme;
        var handlerSymbolId = FindSymbolIdInScopeChain(handlerName, stmt.ScopeId, stmt.Callback.Position, program);

        if (handlerSymbolId < 0)
        {
            throw CreateError(stmt.Callback,
                $"La subroutine gestore '{handlerName}' specificata per HTTP SERVER non è stata trovata.",
                ErrorCode.UndefinedSubOrFunction);
        }

        var handlerSymbol = program.Symbols.GetSymbol(handlerSymbolId);

        // 2. Validazione del Tipo di Simbolo
        if (handlerSymbol.DeclarationNode is not ProcedureDefinition procDef)
        {
            throw CreateError(stmt.Callback,
                $"Il simbolo '{handlerName}' non è una procedura valida (SUB) per essere usata come gestore HTTP.");
        }

        // Deve essere una SUB, non una FUNCTION.
        if (procDef is FunctionDefinition { ReturnType: not VoidType })
        {
            throw CreateError(stmt.Callback,
                $"Il gestore HTTP '{handlerName}' deve essere una SUB (senza valore di ritorno), non una FUNCTION.");
        }

        // 3. Validazione della Firma (Arity Check)
        if (procDef.Parameters.Length != 2)
        {
            throw CreateError(stmt.Callback,
                $"La subroutine gestore '{handlerName}' deve accettare esattamente 2 parametri (req AS HTTPREQUEST, res AS HTTPRESPONSE), ma ne accetta {procDef.Parameters.Length}.");
        }

        // 4. Validazione dei Tipi dei Parametri
        var reqParamType = ResolveDataType(procDef.Parameters[0].DataType, program, context.ModulePath);
        var resParamType = ResolveDataType(procDef.Parameters[1].DataType, program, context.ModulePath);

        if (reqParamType is not HttpRequestType)
        {
            throw CreateError(stmt.Callback,
                $"Il primo parametro del gestore '{handlerName}' deve essere di tipo HTTPREQUEST, ma è di tipo '{reqParamType}'.");
        }

        if (resParamType is not HttpResponseType)
        {
            throw CreateError(stmt.Callback,
                $"Il secondo parametro del gestore '{handlerName}' deve essere di tipo HTTPRESPONSE, ma è di tipo '{resParamType}'.");
        }

        // 5. Riscrittura delle altre espressioni
        var rewrittenPortFinal = stmt.Port != null ? RewriteExpression(stmt.Port, context, program) : null;
        var rewrittenCertPathFinal = stmt.CertificatePath != null
            ? RewriteExpression(stmt.CertificatePath, context, program)
            : null;
        var rewrittenCertPassFinal = stmt.CertificatePassword != null
            ? RewriteExpression(stmt.CertificatePassword, context, program)
            : null;

        var finalStmt = stmt with
        {
            Port = rewrittenPortFinal, CertificatePath = rewrittenCertPathFinal,
            CertificatePassword = rewrittenCertPassFinal
        };

        return (finalStmt, context);
    }

    /// <summary>
    /// Helper per riscrivere un'azione evento (usato da OnKey, OnMouse, OnWindow).
    /// </summary>
    private static OnKeyAction RewriteOnKeyAction(OnKeyAction action, AstRewriteContext context, BasicProgram program)
    {
        return action switch
        {
            CallAction ca => new CallAction(RewriteExpression(ca.Call, context, program)),
            _ => action // Goto, Gosub, Stop non hanno espressioni da riscrivere
        };
    }

    private static MessageAction RewriteMessageAction(MessageAction action, AstRewriteContext context, BasicProgram program)
    {
        return action switch
        {
            MessageCallAction ca => ca,
            _ => action
        };
    }

    /// <summary>
    /// Mappa nomi di metodi fluenti per array alle corrispondenti funzioni ARRAYS.*.
    /// arr.Filter(fn) → ARRAYS.AFILTER(arr, fn)
    /// </summary>
    private static readonly Dictionary<string, string> _arrayFluentMethodMap = new(StringComparer.OrdinalIgnoreCase)
    {
        { "Filter", "AFILTER" },
        { "Map", "AMAP" },
        { "Reduce", "AREDUCE" },
        { "Sort", "ASORT" },
        // NIENTE "Reverse" -> "AREVERSE": AREVERSE CLONA l'array (funzione standalone che
        // restituisce l'inverso), mentre il metodo fluente arr.Reverse() è una MUTAZIONE in
        // place (come Sort/Shuffle). Mappandolo ad AREVERSE il backend emette AClone+Reverse
        // su una COPIA scartata -> l'array originale NON viene mai invertito (TypesLinq:
        // "Reverse:" e "Doubled:" restavano non invertiti). Il MethodCallExpr .Reverse()
        // resta e finisce in EmitNativeMethodCall -> IlRuntime.CollectionReverse (swap in
        // place su IList).
        { "Flat", "AFLAT" },
        { "Unique", "AUNIQUE" },
        { "Count", "ACOUNT" },
        { "Join", "AJOIN" },
        { "Shuffle", "ASHUFFLE" },
        { "Clone", "ACLONE" },
        { "Scan", "ASCAN" },
        { "Chunk", "ACHUNK" },
        { "BinarySearch", "ABINARYSEARCH" },
        { "Max", "AMAX" },
        { "Min", "AMIN" },
        { "Sum", "ASUM" },
    };

    /// <summary>
    /// Riscrive l'espressione di una distrutturazione di tupla (`DIM (a, b) = expr`) e propaga
    /// ai simboli destrutturati il tipo del campo corrispondente quando è una STRINGA: senza
    /// questa propagazione i target restano InferredType (→ double nei backend) e un campo
    /// STRING verrebbe interpretato come double (valori spazzatura). I campi numerici restano
    /// volutamente InferredType (→ double), coerente con la convenzione "i numeri sono double".
    /// </summary>
    private static TupleDestructuringStmt RewriteTupleDestructuring(
        TupleDestructuringStmt stmt, AstRewriteContext context, BasicProgram program)
    {
        var rewritten = RewriteExpression(stmt.TupleExpression, context, program);
        if (LinkerInferExpressionType(rewritten, context, program) is TupleType tupleType)
        {
            for (var i = 0; i < stmt.VariableNames.Length && i < tupleType.Elements.Length; i++)
            {
                if (NormalizeInferredTypeForTuple(tupleType.Elements[i].DataType) is not StringType) continue;
                var symbol = program.Symbols.FindSymbolsByName(stmt.VariableNames[i].Lexeme)
                    .FirstOrDefault(s => s.ScopeId == stmt.ScopeId);
                if (symbol is not null && symbol.DataType is InferredType)
                    program.Symbols.UpdateSymbolType(symbol.Id, new StringType());
            }
        }

        return stmt with { TupleExpression = rewritten, ScopeId = stmt.ScopeId };
    }

    private static DataType NormalizeInferredTypeForTuple(DataType type) => type is InferredType ? new DoubleType() : type;
}
