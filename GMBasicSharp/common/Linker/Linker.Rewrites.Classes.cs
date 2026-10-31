// FILE: Interpreter.Linker.Rewrites.Classes.cs

using System.Collections.Immutable;

// ReSharper disable RedundantArgumentDefaultValue

namespace Basic;

public static partial class Linker
{
    /// <summary>
    /// [FASE 3] Riscrive una definizione di classe completa, scendendo ricorsivamente in tutti i suoi membri
    /// per riscriverne i corpi e le espressioni. Questo è il punto di ingresso per la riscrittura
    /// semantica delle classi.
    /// </summary>
    /// <param name="classDef">La definizione della classe da riscrivere.</param>
    /// <param name="context">Il contesto di riscrittura corrente.</param>
    /// <param name="program">L'istanza del programma.</param>
    /// <returns>Una nuova istanza di ClassDefinition con tutti i membri riscritti.</returns>
    private static ClassDefinition RewriteClassDefinition(ClassDefinition classDef, AstRewriteContext context, BasicProgram program)
    {
        // Una CLASS generica dichiarata (es. `CLASS Box<T>`) è un template: i suoi membri
        // referenziano `T` come tipo, mai risolvibile a un tipo reale (non è un simbolo
        // registrato). Riscriverli qui fallirebbe con "il tipo T non è stato trovato" — stesso
        // motivo per cui i Fields di un TYPE generico non vengono MAI riscritti in questo passo.
        // Il template non viene mai compilato direttamente: solo le sue istanziazioni concrete
        // (monomorfizzate da ResolveGenericClassInstantiation, con TypeParameters vuoto) lo
        // sono, e quelle hanno già i tipi sostituiti nella firma — non serve passare di qui.
        if (!classDef.TypeParameters.IsDefaultOrEmpty)
        {
            return classDef;
        }

        // Crea un nuovo contesto che imposta la classe corrente. Questo contesto verrà passato a tutti i membri.
        var classContext = context with { CurrentClass = classDef };

        program.Symbols.EnterScope(classDef.Name.Lexeme, SymbolTable.ScopeType.Class, classDef.SymbolId);
        try {
            var rewrittenMembers = classDef.Members
                // Passa il nuovo contesto ai membri
                .Select(member => RewriteClassMember(member, classContext, program)) 
                .ToImmutableArray();
             
            var finalClass = classDef with { Members = rewrittenMembers };
            finalClass.ResolvedBaseClass = classDef.ResolvedBaseClass;
            
            if (finalClass.SymbolId >= 0) program.Symbols.UpdateSymbolDeclaration(finalClass.SymbolId, finalClass);
            return finalClass;
        } finally {
            program.Symbols.ExitScope();
        }
    }

    /// <summary>
    /// [FASE 3] Dispatcher per la riscrittura di un singolo membro di classe.
    /// Identifica il tipo di membro e delega la riscrittura all'helper appropriato.
    /// </summary>
    /// <param name="member">Il membro della classe da riscrivere.</param>
    /// <param name="context">Il contesto di riscrittura corrente.</param>
    /// <param name="program">L'istanza del programma.</param>
    /// <returns>La nuova istanza del membro di classe riscritto.</returns>
    private static ClassMember RewriteClassMember(ClassMember member, AstRewriteContext context, BasicProgram program)
    {
        return member switch
        {
            FieldMember fm => fm with
            {
                Declaration = RewriteVariableDeclarationWithInference(fm.Declaration, context, program)
            },
            MethodMember mm => mm with { Procedure = RewriteProcedure(mm.Procedure, context, program) },
            PropertyMember pm => RewritePropertyMember(pm, context, program),
            ConstructorMember cm => RewriteConstructorMember(cm, context, program),
            DestructorMember dm => RewriteDestructorMember(dm, context, program),
            OperatorMember om => RewriteOperatorMember(om, context, program),
            EventMember em => RewriteEventMember(em, context, program),
            _ => member
        };
    }

    /// <summary>
    /// [FASE 3] Riscrive i corpi (GET/SET) di una PropertyMember.
    /// </summary>
    private static PropertyMember RewritePropertyMember(PropertyMember pm, AstRewriteContext context,
        BasicProgram program)
    {
        var prop = pm.Property;

        PropertyGetter? rewrittenGetter = null;
        if (prop.Getter != null)
        {
            // Passa il contesto alla riscrittura del corpo
            var (getterBody, _) = RewriteStatementList(prop.Getter.Body, context, null, program);
            rewrittenGetter = new PropertyGetter(Body: getterBody);
        }

        PropertySetter? rewrittenSetter = null;
        if (prop.Setter != null)
        {
            // Passa il contesto alla riscrittura del corpo
            var (setterBody, _) = RewriteStatementList(prop.Setter.Body, context, null, program);
            rewrittenSetter = prop.Setter with { Body = setterBody };
        }

        return pm with { Property = prop with { Getter = rewrittenGetter, Setter = rewrittenSetter } };
    }

    /// <summary>
    /// [FASE 3] Riscrive il corpo di un ConstructorMember.
    /// </summary>
    private static ConstructorMember RewriteConstructorMember(ConstructorMember cm, AstRewriteContext context,
        BasicProgram program)
    {
        var pos = cm.Constructor.Parameters.FirstOrDefault()?.Position ?? cm.Name.Position;
        program.Symbols.EnterScope($"NEW_{pos.Line}_{pos.Column}", SymbolTable.ScopeType.Procedure);
        try
        {
            var (rewrittenBody, _) = RewriteStatementList(cm.Constructor.Body, context, null, program);
            return cm with { Constructor = cm.Constructor with { Body = rewrittenBody } };
        }
        finally
        {
            program.Symbols.ExitScope();
        }
    }

    /// <summary>
    /// [FASE 3] Riscrive il corpo di un DestructorMember.
    /// </summary>
    private static DestructorMember RewriteDestructorMember(DestructorMember dm, AstRewriteContext context,
        BasicProgram program)
    {
        var (rewrittenBody, _) = RewriteStatementList(dm.Destructor.Body, context, null, program);
        return dm with { Destructor = dm.Destructor with { Body = rewrittenBody } };
    }

    /// <summary>
    /// Item 13 (audit rewrite non uniforme): un EVENT i cui parametri usano un tipo
    /// classe/UDT utente restava con un UnresolvedQualifiedType — mai risolto, perché
    /// EventMember veniva restituito invariato ("gli eventi non hanno corpi da
    /// riscrivere" è vero, ma i Parameters hanno comunque un DataType da risolvere,
    /// esattamente come per OperatorMember/costruttori sotto).
    /// </summary>
    private static EventMember RewriteEventMember(EventMember em, AstRewriteContext context, BasicProgram program)
    {
        var ev = em.Event;
        var rewrittenParameters = ev.Parameters
            .Select(parameter => RewriteParameter(parameter, context, program))
            .ToImmutableArray();
        return em with { Event = ev with { Parameters = rewrittenParameters } };
    }

    /// <summary>
    /// [FASE 3] Riscrive il corpo di un OperatorMember.
    /// Questa versione corretta riscrive il corpo direttamente, senza tentare
    /// un cast illegale o accedere a proprietà inesistenti.
    /// </summary>
    private static OperatorMember RewriteOperatorMember(OperatorMember om, AstRewriteContext context, BasicProgram program)
    {
        var op = om.Operator;
        var modulePath = program.FileIdToPathMap.TryGetValue(op.OperatorToken.Position.FileId, out var path)
            ? path
            : string.Empty;
        // OperatorDefinition non passa da RewriteProcedure: risolviamo qui la firma
        // per non lasciare UnresolvedQualifiedType al backend IL.
        var rewrittenParameters = op.Parameters
            .Select(parameter => RewriteParameter(parameter, context, program))
            .ToImmutableArray();
        var rewrittenReturnType = ResolveDataType(op.ReturnType, program, modulePath);
        
        // Creiamo un contesto fittizio per l'operatore.
        // NOTA: Poiché siamo dentro lo scope della classe, i parametri verranno risolti correttamente.
        // Se volessimo essere precisi, potremmo cercare il SymbolId dell'operatore, ma non è strettamente necessario
        // per il funzionamento del corpo, dato che RewriteFieldAccess usa ScopeId.
        var dummyProcContext = new FunctionDefinition(-1, op.OperatorToken, rewrittenParameters,
            rewrittenReturnType, op.Body, op.Flags);
        var opContext = context with { CurrentProcedure = dummyProcContext };
        
        var (rewrittenBody, _) = RewriteStatementList(op.Body, opContext, dummyProcContext, program);
        var rewrittenOp = op with
        {
            Parameters = rewrittenParameters,
            ReturnType = rewrittenReturnType,
            Body = rewrittenBody
        };

        return om with { Operator = rewrittenOp };
    }
}
