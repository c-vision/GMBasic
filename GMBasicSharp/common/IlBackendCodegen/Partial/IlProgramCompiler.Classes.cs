// FILE: IlProgramCompiler.Classes.cs
// PARTIAL — dominio CLASSI/UDT/CONSTRUCTOR (ordine classi, gerarchia, costruttori UDT).
// Migrato da IlProgramCompiler.cs (2026-08-25 — protocollo 3-step).

using System.Collections.Immutable;
using System.Linq;
using System.Reflection;
using System.Reflection.Emit;
using CommonLib;
using System.Reflection.Metadata;
using System.Reflection.Metadata.Ecma335;
using System.Reflection.PortableExecutable;
using Basic.IlBackend.Gpm;
using Basic.Runtime.Gpm;
using Basic;

namespace Basic.IlBackend;

/// <summary>Partial del transpiler IL: dominio CLASSI/UDT/CONSTRUCTOR.</summary>
public sealed partial class IlProgramCompiler
{

    private static Type MapProcedureType(DataType type, IlTypeContext types, IReadOnlyDictionary<int, TypeBuilder> classTypes)
        => DataTypeMapper.Map(type, types, classTypes);

    /// <summary>Risolve il tipo di ritorno di un OPERATOR UDT quando il Linker lo lascia come
    /// UnresolvedQualifiedType (es. "Vector2D" — il nome qualificato non è ancora legato al
    /// UserDefinedType reale): lo cerca per nome tra gli UDT. Senza, il MethodBuilder
    /// ritornerebbe object e il corpo dell'operatore (arithmetica sulla struct) produrrebbe
    /// IL invalido ("Invalid operand types in BinaryInst" in ILOptim).</summary>
    private static DataType ResolveOperatorReturnType(DataType returnType, BasicProgram program)
    {
        if (returnType is not UnresolvedQualifiedType uqt) return returnType;
        var name = uqt.QualifiedName.FullName;
        var udt = program.GetAllUserDefinedTypes()
            .FirstOrDefault(u => u.OriginalName.Lexeme.Equals(name, StringComparison.OrdinalIgnoreCase));
        return udt ?? returnType;
    }

    /// <summary>
    /// Ordina le classi in modo che ogni base preceda sempre le sue derivate — necessario
    /// perché TypeBuilder.DefineType richiede il TypeBuilder del parent già esistente
    /// (vedi CompileToAssembly, Pass 0). Le classi formano un DAG in un programma valido
    /// (nessuna ereditarietà circolare); un ciclo qui indica un bug altrove nel Linker,
    /// non un caso legittimo da gestire silenziosamente.
    /// </summary>
    /// <summary>
    /// Risolve QUALE costruttore usare tra gli overload di una classe in base al numero
    /// di argomenti (unicità già garantita in fase di dichiarazione — vedi
    /// CompileToAssembly, Pass 1) — stessa strategia usata dal p-code VM
    /// (OP_NEW_CLASS_INSTANCE), decisa qui a compile-time invece che a runtime.
    /// </summary>
    private static ClassConstructorInfo ResolveConstructorOverload(IReadOnlyList<ClassConstructorInfo> candidates, int argCount, string classNameForError)
    {
        foreach (var candidate in candidates)
            if (candidate.Parameters.Length == argCount) return candidate;

        throw new NotSupportedException(
            $"Backend IL: nessun costruttore di '{classNameForError}' accetta {argCount} argomenti.");
    }

    private static ImmutableArray<ClassDefinition> OrderClassesBaseFirst(ImmutableArray<ClassDefinition> classDefinitions)
    {
        var bySymbolId = classDefinitions.ToDictionary(c => c.SymbolId);
        var ordered = ImmutableArray.CreateBuilder<ClassDefinition>(classDefinitions.Length);
        var visited = new HashSet<int>();

        void Visit(ClassDefinition classDef)
        {
            if (!visited.Add(classDef.SymbolId)) return;
            if (classDef.BaseClassSymbolId is int baseId)
            {
                if (!bySymbolId.TryGetValue(baseId, out var baseDef))
                    throw new NotSupportedException(
                        $"Backend IL: classe base di '{classDef.OriginalName.Lexeme}' non trovata nel modulo principale (ereditarietà cross-modulo non ancora supportata).");
                Visit(baseDef);
            }
            ordered.Add(classDef);
        }

        foreach (var classDef in classDefinitions) Visit(classDef);
        return ordered.MoveToImmutable();
    }

    /// <summary>
    /// Cerca, risalendo la catena di ereditarietà a partire da <paramref name="baseClass"/>,
    /// il MethodBuilder di un metodo VIRTUAL/OVERRIDE con lo stesso nome BASIC — usato da
    /// DefineMethodOverride (Pass 1.5) per collegare un OVERRIDE al metodo che ridefinisce.
    /// Confronto per nome (case-insensitive), non per firma completa: overload di metodi
    /// virtuali non ancora distinti (stesso limite già noto per le funzioni builtin, es.
    /// INSTR — vedi B7).
    /// </summary>
    private static MethodBuilder? FindVirtualMethodInHierarchy(ClassDefinition? baseClass, string methodName, IReadOnlyDictionary<int, MethodBuilder> procedureMethods)
    {
        // Ritorna il MethodBuilder della DICHIARAZIONE VIRTUALE PIÙ IN ALTO (root dello slot),
        // NON quello dell'override intermedio più vicino. Con DefineMethodOverride(ovr, nearest)
        // una catena a 3 livelli (Furgone→Auto→Veicolo, cross-file) NON propaga transitivamente
        // nello slot del CLR: `v AS Veicolo: v.Tipo()` su un Furgone risolveva ad Auto.Tipo
        // (output corrotto, ClassiInheritsCrossFileTransitive). Mappando OGNI override allo
        // STESSO slot root (Veicolo.Tipo) il dispatch dinamico sceglie la derivata più profonda.
        MethodBuilder? root = null;
        for (var current = baseClass; current is not null; current = current.ResolvedBaseClass)
        {
            foreach (var member in current.Members)
            {
                if (member is MethodMember { Procedure: FunctionDefinition fd } &&
                    (fd.Flags.HasFlag(SymbolFlags.IsVirtual) || fd.Flags.HasFlag(SymbolFlags.IsOverride)) &&
                    fd.OriginalName.Lexeme.Equals(methodName, StringComparison.OrdinalIgnoreCase))
                    root = procedureMethods[fd.SymbolId];   // continua a salire: tieni il più in alto
            }
        }
        return root;
    }

    /// <summary>
    /// Cerca, a partire da <paramref name="classDef"/> e risalendo la catena di
    /// ereditarietà, il MethodBuilder di UN metodo (virtuale o no) con lo stesso nome
    /// BASIC — usato per collegare l'implementazione CONCRETA di un membro d'interfaccia
    /// (Pass 1.5b), che non richiede VIRTUAL/OVERRIDE come FindVirtualMethodInHierarchy.
    /// </summary>
    private static MethodBuilder? FindMethodInHierarchy(ClassDefinition classDef, string methodName, IReadOnlyDictionary<int, MethodBuilder> procedureMethods)
    {
        for (var current = classDef; current is not null; current = current.ResolvedBaseClass)
        {
            foreach (var member in current.Members)
            {
                if (member is MethodMember { Procedure: FunctionDefinition fd } &&
                    fd.OriginalName.Lexeme.Equals(methodName, StringComparison.OrdinalIgnoreCase))
                    return procedureMethods[fd.SymbolId];
            }
        }
        return null;
    }

    /// <summary>
    /// Compila il corpo di un costruttore di classe (SUB NEW). Chiama SEMPRE PRIMA un
    /// costruttore della base (ogni oggetto CLR deve inizializzare la propria base prima
    /// di eseguire logica propria — regola del CLR, non solo convenzione):
    /// <paramref name="baseConstructor"/> è System.Object.ctor() per una classe senza
    /// INHERITS, o il costruttore (unico, in questo MVP) della classe base altrimenti. Se
    /// il corpo inizia con BASE.NEW(...) esplicita, i suoi argomenti vengono valutati e
    /// passati al costruttore base, poi lo statement viene saltato nel resto del corpo;
    /// altrimenti si assume un costruttore base parameterless (comportamento implicito,
    /// come C#).
    ///
    /// EX-BUG (risolto, non più qui): un'assegnazione <c>SELF.campoArray(i) = valore</c>
    /// dentro un costruttore sembrava produrre IL non valido, ma il vero difetto non
    /// riguardava affatto i campi array né il costruttore in sé — riguardava QUALSIASI
    /// argomento di chiamata a <c>NEW Tipo(argomenti)</c> il cui tipo non coincidesse col
    /// tipo CLR del parametro dichiarato (es. un letterale che compila a Double passato a
    /// un parametro <c>AS INTEGER</c>): il Linker non applicava la coercizione numerica
    /// implicita per gli argomenti di NEW come già faceva per le chiamate a procedura
    /// regolari. Vedi <c>RewriteNewExpr</c> in
    /// Linker/Linker.Rewrites.Expressions.cs (ora usa RewriteSingleArgument con
    /// ExpectedType per-parametro, come RewriteSingleArgument stesso).
    /// </summary>
    private void EmitConstructorBody(
        ConstructorDefinition constructor, ConstructorInfo baseConstructor,
        List<VariableDeclaration> fieldArrayDeclarations, Dictionary<int, FieldArrayInfo> fieldArrays)
    {
        var body = constructor.Body;
        _il.Emit(OpCodes.Ldarg_0);
        if (body.Length > 0 && body[0] is BaseConstructorCallStmt baseCall)
        {
            var bp = baseConstructor.GetParameters();
            for (var ai = 0; ai < baseCall.Args.Length; ai++)
            {
                var arg = baseCall.Args[ai];
                /* Parametro array del ctor base: `T[]&` → indirizzo (come NEW ctor). */
                if (ai < bp.Length && bp[ai].ParameterType.IsByRef)
                {
                    if (arg is VariableExpr) EmitAddressOf(arg);
                    else
                    {
                        var elem = bp[ai].ParameterType.GetElementType()!;
                        var tmp = _il.DeclareLocal(elem);
                        EmitExpression(arg);
                        _il.Emit(OpCodes.Stloc, tmp);
                        _il.Emit(OpCodes.Ldloca, tmp);
                    }
                }
                else EmitExpression(arg);
            }
            body = body.RemoveAt(0);
        }
        _il.Emit(OpCodes.Call, baseConstructor);

        // Ogni campo ARRAY dichiarato DIRETTAMENTE in questa classe (non ereditato — quello
        // della base è già allocato dalla chiamata al costruttore base sopra) va allocato
        // qui, PRIMA del corpo utente: cosi' il corpo del costruttore (che potrebbe già
        // indicizzare SELF.Campo(i)) trova sempre un array non-null.
        foreach (var declaration in fieldArrayDeclarations)
            EmitFieldArrayAllocationCode(_il, _types, fieldArrays[declaration.SymbolId], declaration.Dimensions, () => _il.Emit(OpCodes.Ldarg_0));

        foreach (var stmt in body) PreScanLabels(stmt);
        foreach (var stmt in body) EmitStatement(stmt);
        _il.Emit(OpCodes.Ret);
    }

    /// <summary>
    /// Costruttore di un UDT (value type) — a differenza di EmitConstructorBody (CLASS),
    /// nessuna chiamata a un costruttore base (un value type non ha ereditarietà). Emette
    /// SEMPRE <c>Initobj</c> come PRIMA istruzione: azzera esplicitamente TUTTI i campi
    /// prima del corpo utente, garantendo il "definite assignment" richiesto dal
    /// verificatore CLR per un costruttore di struct (che a differenza di uno di classe
    /// NON eredita uno stato già inizializzato da un `base()` implicito) anche se il corpo
    /// BASIC non assegna esplicitamente ogni campo. Ogni campo ARRAY del UDT va allocato
    /// SUBITO DOPO Initobj (che li azzererebbe a null altrimenti) — stesso schema di
    /// EmitConstructorBody (CLASS), qui rilevante solo per un UDT costruito via un vero
    /// SUB NEW esplicito (il caso più comune, un semplice <c>DIM x AS TipoUdt</c> senza
    /// NEW, passa invece da EmitScalarDeclaration, che non tocca mai questo metodo).
    ///
    /// Vedi nota EX-BUG su EmitConstructorBody (CLASS): la stessa causa (coercizione
    /// numerica mancante per gli argomenti di NEW) si riproduceva identica qui.
    /// </summary>
    private void EmitUdtConstructorBody(ConstructorDefinition constructor, Type udtType, List<TypeField> fieldArrayDeclarations, Dictionary<int, FieldArrayInfo> fieldArrays)
    {
        _il.Emit(OpCodes.Ldarg_0);
        _il.Emit(OpCodes.Initobj, udtType);
        foreach (var fieldDecl in fieldArrayDeclarations)
            EmitFieldArrayAllocationCode(_il, _types, fieldArrays[fieldDecl.SymbolId], fieldDecl.Dimensions, () => _il.Emit(OpCodes.Ldarg_0));

        foreach (var stmt in constructor.Body) PreScanLabels(stmt);
        foreach (var stmt in constructor.Body) EmitStatement(stmt);
        _il.Emit(OpCodes.Ret);
    }

    /// <summary>
    /// Emette <c>newarr</c> (rank 1) o <c>newobj</c> sul costruttore N-int dell'array
    /// rettangolare (rank N) e lo memorizza nel campo istanza — stesso schema di
    /// EmitArrayDeclaration per un DIM locale, ma con Stfld sull'istanza invece di Stloc su
    /// un LocalBuilder. <paramref name="emitInstanceRef"/> emette il riferimento/indirizzo
    /// su cui fare Stfld — <c>Ldarg_0</c> dentro un costruttore (CLASS o UDT: per un value
    /// type "this" all'arg 0 è già un puntatore, stessa convenzione di Initobj in
    /// EmitUdtConstructorBody), oppure <c>Ldloca</c> sul locale quando si allocano i campi
    /// array di un UDT dichiarato con un semplice DIM senza NEW (vedi EmitScalarDeclaration
    /// — un DIM di UDT non passa da NESSUN costruttore, quindi i suoi campi array vanno
    /// allocati lì direttamente). Le dimensioni restano costanti a compile-time (stesso
    /// limite del DIM top-level — vedi ExtractCompileTimeInt32). Statico (niente `_il`/
    /// `_types` d'istanza) perché va invocato anche dal costruttore implicito sintetizzato
    /// in Pass 1, emesso con un ILGenerator "grezzo" prima che esista un'istanza di
    /// IlProgramCompiler per quella classe.
    /// </summary>
    private static void EmitFieldArrayAllocationCode(ILGenerator il, IlTypeContext types, FieldArrayInfo info, ImmutableArray<Expr> dimensions, Action emitInstanceRef)
    {
        var lengths = new int[info.Rank];
        for (var axis = 0; axis < info.Rank; axis++)
        {
            var upperBound = ExtractCompileTimeInt32(dimensions[axis]);
            lengths[axis] = upperBound - info.LowerBounds[axis] + 1;
        }

        emitInstanceRef();
        if (info.Rank == 1)
        {
            il.Emit(OpCodes.Ldc_I4, lengths[0]);
            il.Emit(OpCodes.Newarr, info.ClrArrayType.GetElementType()!);
        }
        else if (info.ClrArrayType == typeof(MatrixValue))
        {
            // Campo matrice rank-2 DOUBLE: descrittore (Stage D), come un DIM locale.
            il.Emit(OpCodes.Ldc_I4, lengths[0]);
            il.Emit(OpCodes.Ldc_I4, lengths[1]);
            il.Emit(OpCodes.Call, typeof(MatrixValue).GetMethod(nameof(MatrixValue.Zeros),
                [types.Int32, types.Int32])!);
        }
        else if (info.ClrArrayType.GetElementType() is { IsArray: true })
        {
            // Elemento CLASS/UDT: array JAGGED (type elemento = T[] dell'array esterno)
            // creato a runtime da IlRuntime.MakeJagged (sotto-array freschi per slot),
            // stesso schema del DIM locale UDT multi-dim. Il rettangolare T[,...] non è
            // risolvibile in Reflection.Emit con elemento TypeBuilder.
            var elemClr = info.ClrArrayType;
            for (var i = 0; i < info.Rank; i++) elemClr = elemClr.GetElementType()!;
            il.Emit(OpCodes.Ldtoken, elemClr);
            il.Emit(OpCodes.Call, typeof(Type).GetMethod(nameof(Type.GetTypeFromHandle))!);
            il.Emit(OpCodes.Ldc_I4, lengths.Length);
            il.Emit(OpCodes.Newarr, types.Int32);
            for (var a = 0; a < lengths.Length; a++)
            {
                il.Emit(OpCodes.Dup);
                il.Emit(OpCodes.Ldc_I4, a);
                il.Emit(OpCodes.Ldc_I4, lengths[a]);
                il.Emit(OpCodes.Stelem_I4);
            }
            il.Emit(OpCodes.Call, typeof(IlRuntime).GetMethod(nameof(IlRuntime.MakeJagged), new[] { typeof(Type), typeof(int[]) })!);
            il.Emit(OpCodes.Castclass, info.ClrArrayType);
        }
        else
        {
            var ctor = info.ClrArrayType.GetConstructor(Enumerable.Repeat(types.Int32, info.Rank).ToArray())
                ?? throw new InvalidOperationException($"Backend IL: costruttore non trovato per l'array rettangolare di campo '{info.ClrArrayType}'.");
            foreach (var length in lengths) il.Emit(OpCodes.Ldc_I4, length);
            il.Emit(OpCodes.Newobj, ctor);
        }
        il.Emit(OpCodes.Stfld, info.Field);
    }

    /// <summary>Emette l'espressione di inizializzazione di un campo STATIC e la memorizza (Stsfld) — chiamare una volta per ogni campo statico inizializzato di una classe, poi FinishStaticConstructor.</summary>
    private void EmitStaticFieldInitializer(FieldBuilder field, Expr initialValue)
    {
        EmitExpression(initialValue);
        _il.Emit(OpCodes.Stsfld, field);
    }

    /// <summary>Chiude il costruttore di tipo (.cctor) dopo N chiamate a EmitStaticFieldInitializer.</summary>
    private void FinishStaticConstructor() => _il.Emit(OpCodes.Ret);

    /// <summary>
    /// Sintetizza il corpo di Main secondo la stessa logica di
    /// <see cref="Linker.HasEntryPoint"/>: codice top-level a numero di riga,
    /// eseguito nell'ordine di <see cref="BasicModule.ExecutionOrder"/>. Chiamato solo
    /// quando <c>mainModule.Statements.Count > 0</c> — l'altro ramo valido secondo
    /// Linker.HasEntryPoint (SUB Main con parametri opzionali) usa invece
    /// <see cref="EmitEntryPointCallingMain"/>, vedi il call site in
    /// <see cref="CompileToAssembly"/>.
    ///
    /// Due passate: la prima alloca (senza fissare) un'etichetta IL per ogni
    /// riga e per ogni LabelStmt incontrato, cosi' i salti in avanti (GOTO su
    /// una riga successiva, IF che salta oltre il proprio corpo, ecc.) possono
    /// riferirsi a un'etichetta già esistente anche prima che la sua posizione
    /// sia nota — esattamente il pattern a due passate già usato dal Parser
    /// per i salti in avanti verso le etichette (vedi AOT.txt §2). La seconda
    /// passata emette davvero le istruzioni, fissando ogni etichetta quando la
    /// raggiunge.
    /// </summary>
}
