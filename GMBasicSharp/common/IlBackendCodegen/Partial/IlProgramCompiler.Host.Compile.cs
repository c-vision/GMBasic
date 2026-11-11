// FILE: IlBackendCodegen/Partial/IlProgramCompiler.Host.Compile.cs
// PARTIAL — dominio HOST/COMPILE: CompileToAssembly (il metodo di compilazione
// assembly, pass -1..N). Migrato da IlProgramCompiler.Host.cs (2026-08-30 — split
// 6.0.1, file <= 1200 righe).

using System.Collections.Immutable;
using System.Linq;
using System.Reflection;
using System.Reflection.Emit;
using System.Reflection.Metadata;
using System.Reflection.Metadata.Ecma335;
using System.Reflection.PortableExecutable;
using Basic.IlBackend.Gpm;
using Basic.Runtime.Gpm;
using CommonLib;
using Basic;

namespace Basic.IlBackend;

/// <summary>Partial del transpiler IL: CompileToAssembly.</summary>
public sealed partial class IlProgramCompiler
{
    public static PersistedAssemblyBuilder CompileToAssembly(
        BasicProgram program, string assemblyName, out int entryPointToken, IlTypeContext? types = null,
        bool skipEntryInitialization = false)
    {
        if (program.MainModule is null)
            throw new InvalidOperationException(
                "BasicProgram.MainModule non è impostato: il programma non ha superato Linker.HasEntryPoint.");

        // Le dichiarazioni arrivano da TUTTI i moduli caricati (MainModule incluso in
        // LoadedModules): TYPE/CLASS/INTERFACE/SUB/FUNCTION definiti in un modulo importato
        // (IMPORT ... WITH ALIAS / MyLibrary.inc) vivono nel BasicModule del file, NON nel
        // MainModule. Usare solo MainModule.Declarations lasciava i tipi/procedure dei moduli
        // fuori → "UDT 'X' non registrato" / "Simbolo non definito: 'Add'" nel backend IL
        // (il P-Code li risolve perché il Linker ha un indice SymbolTable globale).
        static ImmutableArray<AstNode> AllModuleDeclarations(BasicProgram p) =>
            p.LoadedModules.Values.SelectMany(m => m.Declarations).ToImmutableArray();

        types ??= IlTypeContext.ForRuntime();

        var asmName = new AssemblyName(assemblyName);
        var assemblyBuilder = new PersistedAssemblyBuilder(asmName, types.Object.Assembly);
        var moduleBuilder = assemblyBuilder.DefineDynamicModule(assemblyName);
        // Delegate con arità > 16 (limite del BCL): il tipo va definito in QUESTO modulo,
        // altrimenti l'IL emesso lo referenzia in un assembly dinamico "Run" che il .dll
        // salvato non può risolvere ("Could not load file or assembly 'GmbDynamicDelegates'").
        types = types.WithModule(moduleBuilder);

        // Il parent va passato ESPLICITAMENTE: a differenza del vecchio
        // AssemblyBuilderAccess.RunAndSave, PersistedAssemblyBuilder non lo assume di default,
        // e senza un token esplicito per la classe base il file .dll salvato risulta
        // incompleto — un consumer esterno (es. un altro progetto che referenzia il .dll,
        // come nella pipeline dotnet publish -p:PublishAot=true) fallisce con
        // "CS0012: Il tipo 'Object' è definito in un assembly di cui manca il riferimento".
        var programType = moduleBuilder.DefineType(
            "GeneratedProgram",
            TypeAttributes.Public | TypeAttributes.Class | TypeAttributes.Abstract | TypeAttributes.Sealed,
            types.Object);
        // Registra il TypeBuilder per i thunk dei callback (CreateCallbackThunk): la stessa
        // BasicProgram può essere ricompilata, quindi si sostituisce il valore precedente.
        _programTypeBuilders.Remove(program);
        _programTypeBuilders.Add(program, programType);

        // --- Pass -1: dichiara il TIPO (TypeBuilder) di ogni INTERFACE, prima di tutto il
        // resto — un'interfaccia utente è un vero TypeBuilder CLR con
        // TypeAttributes.Interface (nessun parent, a differenza delle classi che vogliono
        // sempre System.Object o un'altra classe esplicita — DefineType(nome, attrs) senza
        // terzo argomento). Condivide lo stesso dizionario classTypes delle classi (stesso
        // spazio di SymbolId, nessuna collisione — vedi DataTypeMapper).
        var interfaceDefinitions = AllModuleDeclarations(program).OfType<InterfaceDefinition>().ToImmutableArray();
        var classTypes = new Dictionary<int, TypeBuilder>();
        // Nomi BASIC dei tipi UDT (SymbolId → "Person"): registrati in IlRuntime dopo CreateType()
        // così TYPEINFO(udt) ritorna il nome utente e non "Udt_544".
        var udtBasicNames = new Dictionary<int, string>();
        // Numero di campi per UDT (SymbolId → n): registrato in IlRuntime per SYSTEM.UDTPACKED (O(1)).
        var udtFieldCounts = new Dictionary<int, int>();
        foreach (var ifaceDef in interfaceDefinitions)
        {
            if (!ifaceDef.TypeParameters.IsDefaultOrEmpty)
                throw new NotSupportedException($"Backend IL: interfacce generiche non ancora supportate ('{ifaceDef.OriginalName.Lexeme}').");
            classTypes[ifaceDef.SymbolId] = moduleBuilder.DefineType(
                "Interface_" + ifaceDef.SymbolId,
                TypeAttributes.Public | TypeAttributes.Interface | TypeAttributes.Abstract);
        }

        // --- Pass -0.5: dichiara le firme (ASTRATTE, nessun corpo) di ogni metodo/
        // proprietà d'interfaccia — MethodAttributes.Abstract|Virtual|NewSlot, condivide
        // procedureMethods/classProperties con le classi (stesso spazio di SymbolId).
        var procedureMethods = new Dictionary<int, MethodBuilder>();
        var procedureDefinitionsBySymbol = new Dictionary<int, FunctionDefinition>();
        var classProperties = new Dictionary<int, ClassPropertyInfo>();
        foreach (var ifaceDef in interfaceDefinitions)
        {
            var ifaceTypeBuilder = classTypes[ifaceDef.SymbolId];
            foreach (var member in ifaceDef.Members)
            {
                switch (member)
                {
                    case MethodMember { Procedure: FunctionDefinition ifaceMethodDef }:
                        var ifaceMethodReturnType = ifaceMethodDef.ReturnType is VoidType ? types.Void : MapProcedureType(ifaceMethodDef.ReturnType, types, classTypes);
                        var ifaceMethodParamTypes = ifaceMethodDef.Parameters.Select(p =>
                        {
                            var mapped = MapProcedureType(p.DataType, types, classTypes);
                            return p.Flags.HasFlag(SymbolFlags.IsByRef) ? mapped.MakeByRefType() : mapped;
                        }).ToArray();
                        var ifaceMethodBuilder = ifaceTypeBuilder.DefineMethod(
                            "Proc_" + ifaceMethodDef.SymbolId,
                            MethodAttributes.Public | MethodAttributes.Abstract | MethodAttributes.Virtual | MethodAttributes.NewSlot,
                            ifaceMethodReturnType, ifaceMethodParamTypes);
                        procedureMethods[ifaceMethodDef.SymbolId] = ifaceMethodBuilder;
                        procedureDefinitionsBySymbol[ifaceMethodDef.SymbolId] = ifaceMethodDef;
                        break;

                    case PropertyMember ifacePropMember:
                        var ifacePropClrType = MapProcedureType(ifacePropMember.Property.Type, types, classTypes);
                        MethodBuilder? ifaceGetter = null;
                        MethodBuilder? ifaceSetter = null;
                        const MethodAttributes abstractAccessorAttrs = MethodAttributes.Public | MethodAttributes.Abstract | MethodAttributes.Virtual | MethodAttributes.NewSlot | MethodAttributes.SpecialName;
                        if (ifacePropMember.Property.Getter is not null)
                            ifaceGetter = ifaceTypeBuilder.DefineMethod("PropGet_" + ifacePropMember.Property.SymbolId, abstractAccessorAttrs, ifacePropClrType, Type.EmptyTypes);
                        if (ifacePropMember.Property.Setter is not null)
                            ifaceSetter = ifaceTypeBuilder.DefineMethod("PropSet_" + ifacePropMember.Property.SymbolId, abstractAccessorAttrs, types.Void, [ifacePropClrType]);
                        classProperties[ifacePropMember.Property.SymbolId] = new ClassPropertyInfo(ifaceGetter, ifaceSetter);
                        break;

                    default:
                        throw new NotSupportedException($"Backend IL: membro di interfaccia '{member.GetType().Name}' non ancora supportato (solo metodi e proprietà).");
                }
            }
        }

        // --- Pass -0.8: TYPE...END TYPE (UDT) — un vero VALUE TYPE CLR (TypeBuilder che
        // eredita da System.ValueType, mai System.Object come le classi), copiato per
        // valore nativamente da Stloc/Ldloc/Starg — mai un dizionario dinamico di campi.
        // Condivide classTypes/classFields/procedureMethods/classConstructors con
        // classi/interfacce (stesso spazio globale di SymbolId, nessuna collisione).
        // Supporta campi, METODI e COSTRUTTORI (stesso schema delle classi: un metodo
        // d'istanza è un vero MethodBuilder non-static, SELF risolto per nome via arg 0
        // esattamente come per una CLASS — vedi EmitMethodCallCore per l'unica vera
        // differenza, l'istanza va caricata per INDIRIZZO non per valore, requisito CLR
        // per una chiamata su un value type). PROPERTY, OPERATOR, distruttori, generici,
        // PACKED e campi array restano un gap esplicito (stesso approccio incrementale
        // già usato per le classi).
        var classFields = new Dictionary<int, FieldBuilder>();
        var classConstructors = new Dictionary<int, List<ClassConstructorInfo>>();
        // I TYPE generici dichiarati (es. `TYPE Box<T>...END TYPE`) sono template: non
        // hanno un tipo CLR proprio e non vengono mai compilati direttamente (T non è
        // risolvibile). Il Linker li monomorfizza per ogni istanziazione concreta usata
        // nel programma (es. `Box<DOUBLE>`); queste monomorfizzazioni compaiono già in
        // program.MainModule.Declarations con TypeParameters vuoto (il Linker ricostruisce
        // quella lista dall'indice UDT della SymbolTable dopo il rewrite, che include anche
        // i simboli registrati per le istanziazioni) — basta escludere solo i template.
        var udtDefinitions = AllModuleDeclarations(program).OfType<UserDefinedType>()
            .Where(u => u.TypeParameters.IsDefaultOrEmpty)
            .ToImmutableArray();
        // TYPE dichiarati NEL CORPO del codice (non nelle Declarations dei moduli — es.
        // ParserStress `TYPE MyUdt` in mezzo agli statement): il Linker li registra nella
        // SymbolTable con DeclarationNode = UserDefinedType, ma AllModuleDeclarations non
        // li vede. Senza, classTypes non li contiene → "UDT 'X' non ancora registrato".
        {
            var seenUdtIds = new HashSet<int>(udtDefinitions.Select(u => u.SymbolId));
            var extraUdts = new List<UserDefinedType>();
            var symCount = program.Symbols.Count;
            for (var si = 0; si < symCount; si++)
            {
                var sym = program.Symbols.GetSymbol(si);
                if (sym.DeclarationNode is UserDefinedType bodyUdt
                    && (bodyUdt.TypeParameters.IsDefaultOrEmpty)
                    && seenUdtIds.Add(bodyUdt.SymbolId))
                    extraUdts.Add(bodyUdt);
            }
            udtDefinitions = udtDefinitions.AddRange(extraUdts);
        }
        // TYPE (value type) con SUB DESTRUCTOR → metodo pubblico `__Destructor` sulla struct
        // (nessun finalizer CLR per i value type: la finalizzazione è solo esplicita).
        var udtFinalizers = new Dictionary<int, MethodBuilder>();
        foreach (var udtDef in udtDefinitions)
        {
            if (!udtDef.Destructors.IsDefaultOrEmpty)
            {
                // TYPE (value type CLR) con SUB DESTRUCTOR: si dichiara un metodo PUBBLICO
                // `__Destructor` sulla struct (il corpo del DELETE), chiamabile dal codegen
                // a fine procedura (finalizzazione deterministica, parità PCode). NON c'è
                // un finalizer CLR per i value type (il GC non li ha nel heap) — la sola
                // finalizzazione è quella esplicita. Registrato per la chiamata (`Ldloca`
                // + call) in EmitFinalizeClassLocals. Il TypeBuilder esiste già (creato
                // prima di questo blocco / riusato sotto per i metodi).
                var udtDtorTypeBuilder = classTypes.TryGetValue(udtDef.SymbolId, out var existingTb)
                    ? existingTb : moduleBuilder.DefineType("Udt_" + udtDef.SymbolId,
                        TypeAttributes.Public | (udtDef.IsUnion ? TypeAttributes.ExplicitLayout : TypeAttributes.SequentialLayout) | TypeAttributes.Sealed, types.ValueType);
                classTypes[udtDef.SymbolId] = udtDtorTypeBuilder;
                var udtDestructorBuilder = udtDtorTypeBuilder.DefineMethod(
                    "__Destructor",
                    MethodAttributes.Public | MethodAttributes.HideBySig,
                    types.Void, Type.EmptyTypes);
                udtFinalizers[udtDef.SymbolId] = udtDestructorBuilder;
            }

            var udtBuilder = classTypes.TryGetValue(udtDef.SymbolId, out var preUdtTb)
                ? preUdtTb
                : moduleBuilder.DefineType(
                    "Udt_" + udtDef.SymbolId,
                    TypeAttributes.Public | (udtDef.IsUnion ? TypeAttributes.ExplicitLayout : TypeAttributes.SequentialLayout) | TypeAttributes.Sealed,
                    types.ValueType);
            // PACKED TYPE (2026-11-11): la rappresentazione CLR resta NATURALE — il layout
            // packed serve SOLO alla frontiera FFI, dove viaggia come forma/offset espliciti
            // (UdtShapeString). Dichiarare Pack=1 sul tipo del dialetto rendeva i campi
            // disallineati, e la reflection su di essi (GetValue/SetValue del marshalling UDT)
            // fa crashare il runtime in modo intermittente (FieldDesc::*InstanceField, SIGBUS
            // su ARM64, ~50% delle esecuzioni). Il layout C è comunque rispettato: gli UDT
            // packed sono ammessi solo BYREF/array (il per-valore è rifiutato a compile-time).
            classTypes[udtDef.SymbolId] = udtBuilder;
            udtBasicNames[udtDef.SymbolId] = udtDef.OriginalName.Lexeme;
            udtFieldCounts[udtDef.SymbolId] = udtDef.Fields.Length;
        }

        // --- Pass 0: dichiara il TIPO (TypeBuilder) di ogni CLASS, prima di qualunque campo/
        // metodo/costruttore — necessario per campi/parametri/return type di tipo classe
        // che si riferiscono in avanti ad altre classi. Un'istanza di classe è un vero
        // reference type CLR (eredita da System.Object O da un'altra classe utente —
        // vedi ereditarietà sotto): niente dizionario dinamico di campi come farebbe un
        // runtime interpretato.
        //
        // Le classi vanno dichiarate in ordine topologico (base PRIMA di derivata):
        // TypeBuilder.DefineType richiede il TypeBuilder del parent già esistente quando
        // la classe eredita da un'altra classe utente (a differenza di quando il parent è
        // semplicemente System.Object, sempre già disponibile).
        // Stessa storia dei TYPE generici sopra: una CLASS generica dichiarata (es. `CLASS Box<T>`)
        // è un template mai compilato direttamente — il Linker la monomorfizza per ogni
        // istanziazione concreta (`DIM b AS Box<DOUBLE>`), che compare già in
        // program.MainModule.Declarations con TypeParameters vuoto. Basta escludere i template;
        // il controllo sotto resta come difesa in profondità (non dovrebbe mai scattare).
        // A differenza dei TYPE generici (la cui istanziazione monomorfizzata finisce, per una
        // coincidenza di timing nel rewrite del Linker, dentro program.MainModule.Declarations),
        // un'istanziazione di CLASS generica (es. `DIM b AS Box<DOUBLE>`) viene risolta durante
        // il rewrite degli STATEMENT del modulo — DOPO che la lista Declarations di quel modulo
        // è già stata costruita nella stessa passata — quindi non vi compare mai. Va quindi presa
        // esplicitamente da program.GenericClassInstantiations (la cache di
        // ResolveGenericClassInstantiation), non solo da MainModule.Declarations.
        var classDefinitionsInSourceOrder = AllModuleDeclarations(program).OfType<ClassDefinition>()
            .Where(c => c.TypeParameters.IsDefaultOrEmpty)
            .Concat(program.GenericClassInstantiations.Values)
            .ToImmutableArray();
        // Classi di CONTESTO sintetiche (heap promotion del Linker per FUNCTION locali con
        // catture: __ClosureCtx_<FUNCTION>_<id>, registrate nella SymbolTable, MAI nelle
        // Declarations dei moduli) — senza, PreScanRedefinedTypes lancia 'classe
        // __ClosureCtx_FUNCTION_xxx non ancora registrata' al primo uso di una closure.
        {
            var seenClassIds = new HashSet<int>(classDefinitionsInSourceOrder.Select(c => c.SymbolId));
            var extraClasses = new List<ClassDefinition>();
            var clsCount = program.Symbols.Count;
            for (var ci = 0; ci < clsCount; ci++)
            {
                var sym = program.Symbols.GetSymbol(ci);
                if (sym.DeclarationNode is ClassDefinition synthClass
                    && (sym.OriginalName.StartsWith("__ClosureCtx", StringComparison.Ordinal)
                        || (sym.Flags.HasFlag(SymbolFlags.IsTypeAlias) && synthClass.Members.IsDefaultOrEmpty
                            && synthClass.BaseClassSymbolId is null))
                    && synthClass.TypeParameters.IsDefaultOrEmpty
                    && seenClassIds.Add(synthClass.SymbolId))
                    extraClasses.Add(synthClass);
            }
            classDefinitionsInSourceOrder = classDefinitionsInSourceOrder.AddRange(extraClasses);
        }
        var classDefinitions = OrderClassesBaseFirst(classDefinitionsInSourceOrder);
        foreach (var classDef in classDefinitions)
        {
            if (!classDef.TypeParameters.IsDefaultOrEmpty)
                throw new NotSupportedException($"Backend IL: classi generiche non ancora supportate (classe '{classDef.OriginalName.Lexeme}').");

            // OrderClassesBaseFirst garantisce che, se BaseClassSymbolId è impostato, il
            // TypeBuilder della base sia già in classTypes a questo punto.
            var parentType = classDef.BaseClassSymbolId is int baseClassId ? classTypes[baseClassId] : types.Object;
            classTypes[classDef.SymbolId] = moduleBuilder.DefineType(
                "Class_" + classDef.SymbolId,
                TypeAttributes.Public | TypeAttributes.Class,
                parentType);
        }

        // --- Pass 0.2: campi degli UDT — DOPO le classi (Pass 0), non prima: un campo di
        // UDT può riferirsi a un tipo CLASSE (es. TYPE Scatola: Rif AS Contenitore), quindi
        // il TypeBuilder della classe deve già esistere in classTypes. Il TypeBuilder VUOTO
        // di ogni UDT è già stato dichiarato sopra (Pass -0.8), PRIMA delle classi — così
        // vale anche il percorso inverso (un campo di CLASSE di tipo UDT).
        // classFieldArrays è dichiarato QUI (non più insieme al resto dei dizionari CLASS
        // più sotto) perché lo popola ANCHE questo pass UDT — condiviso con CLASS, keyed
        // per SymbolId del campo (univoco globalmente, nessuna collisione possibile).
        var classFieldArrays = new Dictionary<int, FieldArrayInfo>();
        var udtFieldArrayDeclarations = new Dictionary<int, List<TypeField>>();
        var udtConstructorDeclarations = new Dictionary<int, List<ConstructorDefinition>>();
        var classOperators = new Dictionary<(int, TokenKind, int, string), MethodBuilder>();
        foreach (var udtDef in udtDefinitions)
        {
            var udtTypeBuilder = classTypes[udtDef.SymbolId];
            var udtFieldArrays = new List<TypeField>();
            udtFieldArrayDeclarations[udtDef.SymbolId] = udtFieldArrays;
            foreach (var field in udtDef.Fields)
            {
                if (!field.Dimensions.IsEmpty)
                {
                    // Un campo array di UDT non ha MAI un lower bound esplicito (TypeField,
                    // a differenza di VariableDeclaration usato dai campi CLASS, non porta
                    // LowerBounds — la sintassi dei campi TYPE non lo permette): sempre
                    // 0-based. A differenza di un campo CLASS/una DIM locale, per un
                    // TypeField il DataType (sia sul nodo AST sia nella SymbolTable) è
                    // SEMPRE quello dell'ELEMENTO diretto (es. INTEGER per "Ports(1) AS
                    // INTEGER"), MAI avvolto in ArrayType — "array-ità" è segnalata SOLO da
                    // Dimensions non vuoto, verificato sopra. L'allocazione vera avviene in
                    // ogni "costruttore" (EmitUdtConstructorBody per NEW,
                    // EmitScalarDeclaration per un DIM senza NEW — l'idioma UDT classico,
                    // mai un vero SUB NEW).
                    var udtFieldArrayRank = field.Dimensions.Length;
                    var udtFieldArrayElementDataType = field.DataType;
                    var udtFieldArrayElementClrType = DataTypeMapper.Map(udtFieldArrayElementDataType, types, classTypes);
                    /* Campo MATRICE rank-2 DOUBLE (`DIM m(N,M) AS DOUBLE` dentro un TYPE):
                     * descrittore MatrixValue (come un DIM locale rank-2, Stage D), non un
                     * double[,]: così `DIM x = t.m` condivide lo storage (semantica di
                     * riferimento, parità PCode/VM-C/C99) e le funzioni MATRIX con parametri
                     * MATRIX OF ricevono il descrittore senza conversioni. */
                    var udtFieldArrayClrType = udtFieldArrayRank == 2
                                               && udtFieldArrayElementDataType is DoubleType
                        ? typeof(MatrixValue)
                        : udtFieldArrayRank == 1 ? udtFieldArrayElementClrType.MakeArrayType() : udtFieldArrayElementClrType.MakeArrayType(udtFieldArrayRank);
                    var udtFieldArrayBuilder = udtTypeBuilder.DefineField(field.Name.Lexeme, udtFieldArrayClrType, FieldAttributes.Public);
                    classFieldArrays[field.SymbolId] = new FieldArrayInfo(udtFieldArrayBuilder, udtFieldArrayClrType, udtFieldArrayElementDataType, udtFieldArrayRank, new int[udtFieldArrayRank], udtDef.SymbolId, field.Name.Lexeme);
                    udtFieldArrays.Add(field);
                    continue;
                }
                var udtFieldClrType = MapProcedureType(field.DataType, types, classTypes);
                classFields[field.SymbolId] = udtTypeBuilder.DefineField(field.Name.Lexeme, udtFieldClrType, FieldAttributes.Public);
                // UNION (2026-11-11): tutti i campi a offset 0 — il CLR gestisce l'aliasing
                // (scrivere I e leggere F restituisce la reinterpretazione dei byte, come in C).
                if (udtDef.IsUnion)
                    classFields[field.SymbolId].SetCustomAttribute(new CustomAttributeBuilder(
                        typeof(System.Runtime.InteropServices.FieldOffsetAttribute).GetConstructor([typeof(int)])!,
                        new object[] { 0 }));
            }

            // PROPERTY di UDT (es. `PROPERTY Magnitude AS DOUBLE` con GET): getter/setter
            // come veri metodi d'istanza sulla struct (arg 0 = SELF per indirizzo), registrati
            // in classProperties — lo stesso dizionario usato da EmitPropertyAccess/Assignment
            // (call sul getter/setter). Il valore è un value type: mai callvirt, solo call.
            foreach (var udtProp in udtDef.Properties)
            {
                var udtPropClrType = MapProcedureType(udtProp.Type, types, classTypes);
                MethodBuilder? udtPropGetter = null;
                MethodBuilder? udtPropSetter = null;
                if (udtProp.Getter is not null)
                    udtPropGetter = udtTypeBuilder.DefineMethod(
                        "PropGet_" + udtProp.SymbolId, MethodAttributes.Public, udtPropClrType, Type.EmptyTypes);
                if (udtProp.Setter is not null)
                    udtPropSetter = udtTypeBuilder.DefineMethod(
                        "PropSet_" + udtProp.SymbolId, MethodAttributes.Public, types.Void, [udtPropClrType]);
                classProperties[udtProp.SymbolId] = new ClassPropertyInfo(udtPropGetter, udtPropSetter);
            }

            // Un metodo d'istanza di UDT è un vero MethodBuilder non-static, esattamente
            // come un metodo di CLASS — mai virtual (un value type è sempre sealed, nessuna
            // ereditarietà possibile, VIRTUAL/OVERRIDE non hanno senso qui).
            foreach (var procedureMethod in udtDef.Methods)
            {
                if (procedureMethod is not FunctionDefinition method)
                    throw new NotSupportedException($"Backend IL: metodo UDT di tipo '{procedureMethod.GetType().Name}' non ancora supportato (solo SUB/FUNCTION).");
                var udtMethodReturnClrType = method.ReturnType is VoidType ? types.Void : MapProcedureType(method.ReturnType, types, classTypes);
                var udtMethodParamClrTypes = method.Parameters.Select(p =>
                {
                    var mapped = MapProcedureType(p.DataType, types, classTypes);
                    return p.Flags.HasFlag(SymbolFlags.IsByRef) ? mapped.MakeByRefType() : mapped;
                }).ToArray();
                // Un metodo UDT che implementa un membro d'interfaccia (TYPE X IMPLEMENTS
                // IFoo) DEVE essere Virtual — requisito CLR: senza, DefineMethodOverride
                // fallisce con TypeLoadException ("must be virtual to implement a method on
                // an interface"). Stesso criterio delle classi (metodo PUBLIC normale con lo
                // stesso nome BASIC di un metodo d'interfaccia).
                var udtImplementsInterfaceMethod = udtDef.ResolvedInterfaces.Any(iface =>
                    iface.Members.OfType<MethodMember>().Any(m => m.Procedure.OriginalName.Lexeme.Equals(method.OriginalName.Lexeme, StringComparison.OrdinalIgnoreCase)));
                var udtMethodAttrs = MethodAttributes.Public;
                if (udtImplementsInterfaceMethod)
                    udtMethodAttrs |= MethodAttributes.Virtual | MethodAttributes.HideBySig;
                var udtMethodBuilder = udtTypeBuilder.DefineMethod(
                    "Proc_" + method.SymbolId, udtMethodAttrs, udtMethodReturnClrType, udtMethodParamClrTypes);
                procedureMethods[method.SymbolId] = udtMethodBuilder;
                procedureDefinitionsBySymbol[method.SymbolId] = method;
            }

            foreach (var op in udtDef.Operators)
            {
                var parameterTypes = op.Parameters.Select((parameter, index) =>
                    index == 0 ? classTypes[udtDef.SymbolId] : MapProcedureType(ResolveOperatorReturnType(parameter.DataType, program), types, classTypes)).ToArray();
                var opReturnType = ResolveOperatorReturnType(op.ReturnType, program);
                var returnType = MapProcedureType(opReturnType, types, classTypes);
                var builder = udtTypeBuilder.DefineMethod(
                    $"Op_{udtDef.SymbolId}_{op.OperatorToken.Kind}_{OperatorReturnKey(opReturnType)}",
                    MethodAttributes.Public | MethodAttributes.Static, returnType, parameterTypes);
                classOperators[(udtDef.SymbolId, op.OperatorToken.Kind, op.Parameters.Length,
                    OperatorReturnKey(opReturnType))] = builder;
            }

            var udtDeclaredConstructors = new List<ClassConstructorInfo>();
            var udtCtorDefinitions = new List<ConstructorDefinition>();
            classConstructors[udtDef.SymbolId] = udtDeclaredConstructors;
            udtConstructorDeclarations[udtDef.SymbolId] = udtCtorDefinitions;
            foreach (var ctor in udtDef.Constructors)
            {
                if (udtDeclaredConstructors.Any(c => c.Parameters.Length == ctor.Parameters.Length))
                    throw new NotSupportedException(
                        $"Backend IL: due costruttori con lo stesso numero di argomenti ({ctor.Parameters.Length}) nell'UDT '{udtDef.OriginalName.Lexeme}' non sono ancora distinguibili (overload risolti solo per numero di argomenti).");
                var udtCtorParamClrTypes = ctor.Parameters.Select(p =>
                {
                    var mapped = MapProcedureType(p.DataType, types, classTypes);
                    return p.Flags.HasFlag(SymbolFlags.IsByRef) ? mapped.MakeByRefType() : mapped;
                }).ToArray();
                var udtCtorBuilder = udtTypeBuilder.DefineConstructor(MethodAttributes.Public, CallingConventions.Standard, udtCtorParamClrTypes);
                udtDeclaredConstructors.Add(new ClassConstructorInfo(udtCtorBuilder, ctor.Parameters));
                udtCtorDefinitions.Add(ctor);
            }
            // UDT (TYPE) senza costruttori espliciti: definisce un ctor implicito
            // parameterless — come le CLASS sotto (senza, un array di UDT ha elementi
            // null e `arr(0).Campo = ...` dà NullReferenceException; il VM li crea).
            if (udtDeclaredConstructors.Count == 0)
            {
                var udtImplicitCtor = udtTypeBuilder.DefineConstructor(
                    MethodAttributes.Public, CallingConventions.Standard, Type.EmptyTypes);
                var udtImplicitIl = udtImplicitCtor.GetILGenerator();
                // Un VALUE TYPE NON chiama mai un costruttore base (System.ValueType non ha
                // un ctor paramless invocabile, e `call Object::.ctor` su una struct è IL
                // ILLEGALE — "found address of 'Udt' expected ref object", BadImageFormat).
                // Il ctor implicito azzera i campi con `initobj` (definite-assignment del
                // verifier) e ritorna: è esattamente il default di una struct.
                udtImplicitIl.Emit(OpCodes.Ldarg_0);
                udtImplicitIl.Emit(OpCodes.Initobj, udtTypeBuilder);
                // Campi ARRAY del UDT (S3b, 2026-11-06): initobj li azzera a NULL — vanno
                // allocati qui, come in EmitUdtConstructorBody (che però NON copre questo
                // ctor implicito: è emesso qui con un ILGenerator grezzo). Senza,
                // `n = NEW Arr4 : n.A(1) = 3` andava in NullReferenceException (le altre
                // gambe allocavano i campi).
                foreach (var farr in udtFieldArrayDeclarations[udtDef.SymbolId])
                    EmitFieldArrayAllocationCode(udtImplicitIl, types, classFieldArrays[farr.SymbolId],
                        farr.Dimensions, () => udtImplicitIl.Emit(OpCodes.Ldarg_0));
                udtImplicitIl.Emit(OpCodes.Ret);
                udtDeclaredConstructors.Add(new ClassConstructorInfo(udtImplicitCtor, ImmutableArray<Parameter>.Empty));
            }
        }

        // Pre-passata: dichiara la FIRMA di ogni SUB/FUNCTION del modulo principale PRIMA
        // di compilare qualunque corpo — stesso pattern a due passate delle label (vedi
        // EmitEntryPoint), necessario per riferimenti in avanti/ricorsione diretta o mutua.
        // FUNCTION generiche (template con TypeParameters): NON compilate direttamente —
        // il Linker le SPECIALIZZA per ogni chiamata con typeArgs concreti
        // (ResolvedSpecializations, SpecializeGenericFunction) e l'istanziazione va
        // compilata al posto del template (come GenericClassInstantiations per le CLASS).
        // Procedure LOCALI (LOCAL FUNCTION/SUB dichiarate DENTRO un corpo — LocalProcedureStmt):
        // non sono nei moduli — raccolte qui e compilate come metodi normali (le chiamate le
        // risolvono per SymbolId, come le top-level).
        var localProcedures = AllModuleDeclarations(program).OfType<FunctionDefinition>()
            .SelectMany(fd => fd.Body.OfType<LocalProcedureStmt>())
            .Select(lps => lps.Procedure)
            .OfType<FunctionDefinition>()
            .Where(d => d.TypeParameters.IsDefaultOrEmpty)
            .ToImmutableArray();
        var localProcedureSymbolIds = AllModuleDeclarations(program).OfType<FunctionDefinition>()
            .SelectMany(fd => fd.Body.OfType<LocalProcedureStmt>())
            .Select(lps => lps.Procedure.SymbolId)
            .ToHashSet();
        var procedureDefinitions = AllModuleDeclarations(program).OfType<FunctionDefinition>()
            .Where(d => d.TypeParameters.IsDefaultOrEmpty)
            .Concat(program.ResolvedSpecializations.Values)
            .Concat(localProcedures)
            // GENERATOR ... YIELD: compilato come metodo statico `Proc_<id>(params)` void con il CORPO
            // del generatore; YIELD (IlRuntime.GeneratorYield) passa i valori al chiamante, il call site
            // (EmitFunctionCall) crea l'iteratore (IlRuntime.GeneratorCreate) e NEXT lo riprende.
            .Concat(AllModuleDeclarations(program).OfType<GeneratorDefinition>()
                .Select(g => new FunctionDefinition(g.SymbolId, g.Name, g.Parameters, new VoidType(), g.Body, g.Flags)))
            .ToImmutableArray();
        // Handler evento con target = LABEL (locale o di modulo): metodo sintetico, così
        // `ON WINDOW … GOSUB FinestraChiusa` funziona anche su IL (parità con C99/VM-C/PCode).
        procedureDefinitions = AddEventLabelHandlerDefinitions(program, procedureDefinitions);
        foreach (var definition in procedureDefinitions)
        {
            var returnClrType = definition.ReturnType is VoidType ? types.Void : MapProcedureType(definition.ReturnType, types, classTypes);
            // BYREF: il parametro CIL è un vero T& (MakeByRefType) — leggerlo/scriverlo nel
            // corpo della procedura richiede Ldind/Stind, non il semplice Ldarg/Starg di un
            // BYVAL (vedi EmitLoadVariable/EmitStoreVariable), e al call site va passato
            // l'INDIRIZZO dell'argomento, non il suo valore (vedi EmitAddressOf).
            var parameterClrTypes = definition.Parameters.Select((p, pIdx) =>
                MapParamClrTypeConsideringBox(p, pIdx, definition.SymbolId, program, types, classTypes)).ToArray();

            // Nome CIL mangled sul SymbolId, non sul nome BASIC: evita collisioni con
            // l'overloading per firma che BASIC supporta (ProcedureSignature) e rende
            // superfluo un controllo di unicità qui — le chiamate risolvono comunque per
            // SymbolId (vedi EmitFunctionCall), mai per nome.
            var methodBuilder = programType.DefineMethod(
                "Proc_" + definition.SymbolId,
                MethodAttributes.Public | MethodAttributes.Static,
                returnClrType,
                parameterClrTypes);
            procedureMethods[definition.SymbolId] = methodBuilder;
            procedureDefinitionsBySymbol[definition.SymbolId] = definition;
        }

        // --- Pass 1: dichiara campi + firme di metodo + firma del costruttore di ogni
        // classe. Un metodo d'istanza è un vero MethodBuilder non-static: il CLR gestisce
        // `this` come argomento 0 implicito da sé, mai esplicito nella firma. VIRTUAL/
        // OVERRIDE ottengono MethodAttributes.Virtual (il vtable NATIVO del CLR, mai uno
        // fatto a mano — vedi TODO Obsidian, "Domande aperte" #5) e vengono chiamati con
        // `callvirt`; ogni altro metodo resta `call` diretta, senza indirezione.
        var classFieldArrayDeclarations = new Dictionary<int, List<VariableDeclaration>>();
        var classFinalizers = new Dictionary<int, MethodBuilder>();
        var staticFieldInitializers = new List<(int ClassSymbolId, FieldBuilder Field, Expr InitialValue)>();
        foreach (var classDef in classDefinitions)
        {
            var classTypeBuilder = classTypes[classDef.SymbolId];
            var declaredConstructors = new List<ClassConstructorInfo>();
            classConstructors[classDef.SymbolId] = declaredConstructors;
            var fieldArrayDeclarations = new List<VariableDeclaration>();
            classFieldArrayDeclarations[classDef.SymbolId] = fieldArrayDeclarations;

            foreach (var member in classDef.Members)
            {
                switch (member)
                {
                    case FieldMember field when !field.Declaration.Dimensions.IsEmpty:
                        // Un vero array CLR PER-ISTANZA (mai per-tipo, a differenza di
                        // FieldAttributes.Static — vedi caso sotto): il DataType del campo
                        // stesso (non del symbol, che qui coincide comunque) è quello
                        // dell'ELEMENTO, il rank va ricavato da Dimensions.Length, stesso
                        // schema di EmitArrayDeclaration per un DIM locale. L'allocazione
                        // vera e propria (newarr/newobj) avviene in OGNI costruttore della
                        // classe (vedi EmitConstructorBody), non qui — qui solo la firma
                        // del campo CLR.
                        if (program.Symbols.GetSymbol(field.Declaration.SymbolId).DataType is not ArrayType fieldArrayType)
                            throw new InvalidOperationException($"Backend IL: tipo array atteso per il campo '{field.Declaration.NameCleaned.Lexeme}' in '{classDef.OriginalName.Lexeme}'.");
                        var fieldArrayRank = field.Declaration.Dimensions.Length;
                        var fieldArrayElementDataType = fieldArrayType.ElementType;
                        var fieldArrayElementClrType = DataTypeMapper.Map(fieldArrayElementDataType, types, classTypes);
                        // Rank > 1 con elemento CLASS/UDT (TypeBuilder non finalizzato): array
                        // JAGGED (T[][]) — il rettangolare T[,...] non è risolvibile in
                        // Reflection.Emit. I tipi nativi restano rettangolari.
                        Type fieldArrayClrType;
                        if (fieldArrayRank == 2 && fieldArrayElementDataType is DoubleType)
                        {
                            // Campo matrice rank-2 DOUBLE: descrittore (come un DIM locale
                            // rank-2, Stage D) — riferimento condiviso, niente double[,].
                            fieldArrayClrType = typeof(MatrixValue);
                        }
                        else if (fieldArrayRank == 1)
                            fieldArrayClrType = fieldArrayElementClrType.MakeArrayType();
                        else if (fieldArrayElementClrType is TypeBuilder)
                        {
                            fieldArrayClrType = fieldArrayElementClrType;
                            for (var i = 0; i < fieldArrayRank; i++) fieldArrayClrType = fieldArrayClrType.MakeArrayType();
                        }
                        else
                            fieldArrayClrType = fieldArrayElementClrType.MakeArrayType(fieldArrayRank);
                        var fieldArrayLowerBounds = new int[fieldArrayRank];
                        for (var axis = 0; axis < fieldArrayRank; axis++)
                            fieldArrayLowerBounds[axis] = field.Declaration.LowerBounds.Length > axis ? ExtractCompileTimeInt32(field.Declaration.LowerBounds[axis]) : 0;
                        var classFieldArrayBuilder = classTypeBuilder.DefineField(field.Declaration.NameCleaned.Lexeme, fieldArrayClrType, FieldAttributes.Public);
                        classFieldArrays[field.Declaration.SymbolId] = new FieldArrayInfo(classFieldArrayBuilder, fieldArrayClrType, fieldArrayElementDataType, fieldArrayRank, fieldArrayLowerBounds, classDef.SymbolId, field.Declaration.NameCleaned.Lexeme);
                        fieldArrayDeclarations.Add(field.Declaration);
                        break;

                    case FieldMember field:
                        var fieldClrType = MapProcedureType(field.Declaration.DataType!, types, classTypes);
                        // Fase 4 (Closure-ByRef-Box-Linker-Definitivo): un campo di una
                        // __ClosureCtx sintetica (mai una classe utente — il prefisso è
                        // riservato, vedi PerformHeapPromotion) il cui nome corrisponde a un
                        // parametro BYREF catturato da una closure locale (in QUALUNQUE
                        // procedura del programma — vedi GetByRefCapturedParamNames) è
                        // rappresentato come ByRefBox<T>, mai T nudo: riceve il BOX già
                        // posseduto dal parametro (MakeByRefExpr, seeding del Linker), mai una
                        // copia del valore — letture/scritture instradate su .Value (vedi
                        // EmitLoadVariable/EmitStoreVariable, ramo _capturedFields).
                        if (classDef.OriginalName.Lexeme.StartsWith("__ClosureCtx", StringComparison.OrdinalIgnoreCase)
                            && GetByRefCapturedParamNames(program).Contains(field.Declaration.NameCleaned.Lexeme))
                            fieldClrType = typeof(ByRefBox<>).MakeGenericType(fieldClrType);
                        // STATIC: FieldAttributes.Static — un vero campo statico CLR (uno per
                        // TIPO, non per istanza, il CLR lo gestisce nativamente), mai un
                        // dizionario globale fatto a mano. L'inizializzatore (se presente) va
                        // eseguito UNA VOLTA nel costruttore di tipo (.cctor, Pass 1.5 sotto),
                        // non nel costruttore d'istanza.
                        var isStaticField = field.Declaration.Flags.HasFlag(SymbolFlags.IsStatic);
                        var fieldAttrs = isStaticField ? FieldAttributes.Public | FieldAttributes.Static : FieldAttributes.Public;
                        var classFieldBuilder = classTypeBuilder.DefineField(field.Declaration.NameCleaned.Lexeme, fieldClrType, fieldAttrs);
                        classFields[field.Declaration.SymbolId] = classFieldBuilder;
                        if (isStaticField && field.Declaration.InitialValue is not null)
                            staticFieldInitializers.Add((classDef.SymbolId, classFieldBuilder, field.Declaration.InitialValue));
                        break;

                    case MethodMember { Procedure: FunctionDefinition methodDef }:
                        var methodReturnClrType = methodDef.ReturnType is VoidType ? types.Void : MapProcedureType(methodDef.ReturnType, types, classTypes);
                        var methodParamClrTypes = methodDef.Parameters.Select(p =>
                        {
                            var mapped = MapProcedureType(p.DataType, types, classTypes);
                            return p.Flags.HasFlag(SymbolFlags.IsByRef) ? mapped.MakeByRefType() : mapped;
                        }).ToArray();
                        // VIRTUAL/OVERRIDE: MethodAttributes.Virtual attiva il vtable NATIVO
                        // del CLR (mai un vtable fatto a mano — vedi TODO Obsidian, "Domande
                        // aperte" #5). Il collegamento OVERRIDE→VIRTUAL della base avviene
                        // esplicitamente più sotto (Pass 1.5, DefineMethodOverride) — i nomi
                        // CIL sono mangled sul SymbolId (mai sul nome BASIC), quindi il
                        // collegamento implicito per nome+firma che farebbe il compilatore
                        // C# non scatterebbe da solo. Un metodo PUBLIC "normale" (né VIRTUAL
                        // né OVERRIDE in BASIC) che implementa un membro d'interfaccia deve
                        // ANCH'ESSO essere Virtual — requisito del CLR, non una scelta: senza
                        // Virtual, DefineMethodOverride (Pass 1.5b) fallisce a runtime con
                        // TypeLoadException ("must be virtual to implement a method on an
                        // interface"), scoperto testando questa feature.
                        // STATIC: nessun `this`, mai virtual (un metodo statico non può
                        // partecipare al vtable — non ha un'istanza su cui essere
                        // dispatchato), quindi non può implementare un membro d'interfaccia.
                        var isStaticMethod = methodDef.Flags.HasFlag(SymbolFlags.IsStatic);
                        var implementsInterfaceMethod = !isStaticMethod && classDef.ResolvedInterfaces.Any(iface =>
                            iface.Members.OfType<MethodMember>().Any(m => m.Procedure.OriginalName.Lexeme.Equals(methodDef.OriginalName.Lexeme, StringComparison.OrdinalIgnoreCase)));
                        var methodAttrs = MethodAttributes.Public;
                        if (isStaticMethod) methodAttrs |= MethodAttributes.Static;
                        if (methodDef.Flags.HasFlag(SymbolFlags.IsVirtual) || methodDef.Flags.HasFlag(SymbolFlags.IsOverride) || implementsInterfaceMethod)
                            methodAttrs |= MethodAttributes.Virtual | MethodAttributes.HideBySig;
                        var classMethodBuilder = classTypeBuilder.DefineMethod(
                            "Proc_" + methodDef.SymbolId, methodAttrs, methodReturnClrType, methodParamClrTypes);
                        procedureMethods[methodDef.SymbolId] = classMethodBuilder;
                        procedureDefinitionsBySymbol[methodDef.SymbolId] = methodDef;
                        break;

                    case ConstructorMember ctorMember:
                        if (declaredConstructors.Any(c => c.Parameters.Length == ctorMember.Constructor.Parameters.Length))
                            throw new NotSupportedException(
                                $"Backend IL: due costruttori con lo stesso numero di argomenti ({ctorMember.Constructor.Parameters.Length}) nella classe '{classDef.OriginalName.Lexeme}' non sono ancora distinguibili (overload risolti solo per numero di argomenti).");
                        var ctorParamClrTypes = ctorMember.Constructor.Parameters.Select(p =>
                        {
                            var mapped = MapProcedureType(p.DataType, types, classTypes);
                            return p.Flags.HasFlag(SymbolFlags.IsByRef) ? mapped.MakeByRefType() : mapped;
                        }).ToArray();
                        var classCtorBuilder = classTypeBuilder.DefineConstructor(
                            MethodAttributes.Public, CallingConventions.Standard, ctorParamClrTypes);
                        declaredConstructors.Add(new ClassConstructorInfo(classCtorBuilder, ctorMember.Constructor.Parameters));
                        break;

                    case PropertyMember propMember:
                        // Get/Set sono metodi d'istanza NON-static veri, esattamente come un
                        // metodo normale — PropertyBuilder è solo un wrapper di metadati .NET
                        // "pulito" (utile per l'interoperabilità con altri assembly .NET, non
                        // strettamente necessario per la correttezza del codice generato qui,
                        // che chiama get_/set_ direttamente — vedi EmitPropertyAccess/
                        // EmitFieldAssignment). Nessun supporto a PROPERTY VIRTUAL in questo
                        // MVP, ma se implementa una PROPERTY d'interfaccia gli accessor devono
                        // comunque essere Virtual — stesso requisito del CLR dei metodi normali
                        // (vedi il case MethodMember sopra).
                        var propImplementsInterfaceMember = classDef.ResolvedInterfaces.Any(iface =>
                            iface.Members.OfType<PropertyMember>().Any(m => m.Property.OriginalName.Lexeme.Equals(propMember.Property.OriginalName.Lexeme, StringComparison.OrdinalIgnoreCase)));
                        var propAccessorAttrs = MethodAttributes.Public;
                        if (propImplementsInterfaceMember) propAccessorAttrs |= MethodAttributes.Virtual | MethodAttributes.HideBySig;

                        var propClrType = MapProcedureType(propMember.Property.Type, types, classTypes);
                        MethodBuilder? getterBuilder = null;
                        MethodBuilder? setterBuilder = null;
                        if (propMember.Property.Getter is not null)
                            getterBuilder = classTypeBuilder.DefineMethod(
                                "PropGet_" + propMember.Property.SymbolId, propAccessorAttrs, propClrType, Type.EmptyTypes);
                        if (propMember.Property.Setter is not null)
                            setterBuilder = classTypeBuilder.DefineMethod(
                                "PropSet_" + propMember.Property.SymbolId, propAccessorAttrs, types.Void, [propClrType]);
                        classProperties[propMember.Property.SymbolId] = new ClassPropertyInfo(getterBuilder, setterBuilder);

                        var propertyBuilder = classTypeBuilder.DefineProperty(
                            propMember.Property.OriginalName.Lexeme, PropertyAttributes.None, propClrType, Type.EmptyTypes);
                        if (getterBuilder is not null) propertyBuilder.SetGetMethod(getterBuilder);
                        if (setterBuilder is not null) propertyBuilder.SetSetMethod(setterBuilder);
                        break;

                    case OperatorMember opMember:
                        // Come un metodo STATIC: entrambi gli operandi sono parametri
                        // espliciti (indice 0-based), nessun SELF — confermato con
                        // --dump-ast, il corpo di un OPERATOR non referenzia mai SELF
                        // (vedi nota su _classOperators). Un `BinaryExpr` con operandi
                        // ClassType risolve staticamente a questo MethodBuilder in
                        // EmitBinaryExpression (mai un vero MethodCallExpr — il Linker
                        // lascia il BinaryExpr generico, dispatch dinamico a runtime nel
                        // p-code Compiler/Executor, statico qui).
                        var opParamClrTypes = opMember.Operator.Parameters.Select(p => MapProcedureType(p.DataType, types, classTypes)).ToArray();
                        var opReturnClrType = MapProcedureType(opMember.Operator.ReturnType, types, classTypes);
                        var opMethodBuilder = classTypeBuilder.DefineMethod(
                            "Op_" + classDef.SymbolId + "_" + opMember.Operator.OperatorToken.Kind,
                            MethodAttributes.Public | MethodAttributes.Static, opReturnClrType, opParamClrTypes);
                        classOperators[(classDef.SymbolId, opMember.Operator.OperatorToken.Kind,
                            opMember.Operator.Parameters.Length, OperatorReturnKey(opMember.Operator.ReturnType))] = opMethodBuilder;
                        break;

                    case EventMember:
                        // EVENT name(args) come membro di classe: la sottoscrizione/dispatch
                        // vive nel runtime (EventSubscribe/RaiseEvent keyed per tipo+evento)
                        // — nessun campo CLR di supporto necessario. Accettato, nessun membro.
                        break;

                    case DestructorMember:
                        // DELETE...END DELETE → DUE metodi: (1) `__Destructor` PUBBLICO
                        // (corpo del DELETE) — chiamabile ESPLICITAMENTE dal codegen a fine
                        // scope/procedura per la finalizzazione DETERMINISTICA (parità PCode
                        // OP_FINALIZE_SCOPE/GLOBAL); (2) `Finalize` (Family virtual, il
                        // finalizer CLR riconosciuto per nome+firma) il cui corpo chiama
                        // `__Destructor` per il percorso GC. `classFinalizers` punta al
                        // metodo PUBBLICO (l'unico chiamabile da fuori la classe).
                        var destructorBuilder = classTypeBuilder.DefineMethod(
                            "__Destructor",
                            MethodAttributes.Public | MethodAttributes.HideBySig,
                            types.Void, Type.EmptyTypes);
                        classFinalizers[classDef.SymbolId] = destructorBuilder;
                        var finalizerBuilder = classTypeBuilder.DefineMethod(
                            "Finalize",
                            MethodAttributes.Family | MethodAttributes.Virtual | MethodAttributes.HideBySig,
                            types.Void, Type.EmptyTypes);
                        {
                            var fil = finalizerBuilder.GetILGenerator();
                            fil.Emit(OpCodes.Ldarg_0);
                            fil.Emit(OpCodes.Call, destructorBuilder);
                            fil.Emit(OpCodes.Ret);
                        }
                        break;

                    default:
                        throw new NotSupportedException(
                            $"Backend IL: membro di classe '{member.GetType().Name}' non ancora supportato (solo campi, metodi, costruttori, PROPERTY, OPERATOR, DELETE).");
                }
            }

            // Nessun SUB NEW dichiarato: sintetizza un costruttore pubblico parameterless
            // implicito (stile C#) che si limita a invocare quello della base — copre le
            // classi "solo STATIC" (utility, mai istanziate via NEW) che altrimenti non
            // avrebbero alcun costruttore CLR (a differenza di C#, TypeBuilder non lo
            // aggiunge da sé). Se la base non ha un costruttore parameterless risolvibile,
            // resta un errore esplicito: nessun modo corretto di sintetizzarne uno.
            if (declaredConstructors.Count == 0)
            {
                ConstructorInfo implicitBaseCtor;
                if (classDef.BaseClassSymbolId is int baseClassId)
                {
                    implicitBaseCtor = classConstructors[baseClassId]
                        .SingleOrDefault(c => c.Parameters.Length == 0)?.Builder
                        ?? throw new NotSupportedException(
                            $"Backend IL: classe '{classDef.OriginalName.Lexeme}' senza costruttore esplicito (SUB NEW) e la base '{classDef.ResolvedBaseClass?.OriginalName.Lexeme}' non ne ha uno parameterless.");
                }
                else
                {
                    implicitBaseCtor = types.Object.GetConstructor(Type.EmptyTypes)
                        ?? throw new InvalidOperationException("Backend IL: costruttore parameterless di System.Object non trovato.");
                }
                var implicitCtorBuilder = classTypeBuilder.DefineConstructor(
                    MethodAttributes.Public, CallingConventions.Standard, Type.EmptyTypes);
                var implicitCtorIl = implicitCtorBuilder.GetILGenerator();
                implicitCtorIl.Emit(OpCodes.Ldarg_0);
                implicitCtorIl.Emit(OpCodes.Call, implicitBaseCtor);
                foreach (var arrayFieldDecl in fieldArrayDeclarations)
                    EmitFieldArrayAllocationCode(implicitCtorIl, types, classFieldArrays[arrayFieldDecl.SymbolId], arrayFieldDecl.Dimensions, () => implicitCtorIl.Emit(OpCodes.Ldarg_0));
                implicitCtorIl.Emit(OpCodes.Ret);
                declaredConstructors.Add(new ClassConstructorInfo(implicitCtorBuilder, ImmutableArray<Parameter>.Empty));
            }
        }

        // --- Pass 1.5: collega ogni metodo OVERRIDE al metodo VIRTUAL della classe base
        // che ridefinisce — esplicito (DefineMethodOverride), non implicito per nome+firma
        // (i nomi CIL sono mangled sul SymbolId, non sul nome BASIC, quindi il matching
        // implicito del CLR non troverebbe mai la coppia giusta da solo). Richiede che
        // TUTTI i metodi (base e derivata) abbiano già una firma dichiarata — fatto sopra.
        foreach (var classDef in classDefinitions)
        {
            foreach (var member in classDef.Members)
            {
                if (member is not MethodMember { Procedure: FunctionDefinition methodDef } || !methodDef.Flags.HasFlag(SymbolFlags.IsOverride))
                    continue;

                var baseMethodBuilder = FindVirtualMethodInHierarchy(classDef.ResolvedBaseClass, methodDef.OriginalName.Lexeme, procedureMethods)
                    ?? throw new NotSupportedException(
                        $"Backend IL: OVERRIDE '{methodDef.OriginalName.Lexeme}' in '{classDef.OriginalName.Lexeme}' senza un metodo VIRTUAL corrispondente nella gerarchia base.");
                classTypes[classDef.SymbolId].DefineMethodOverride(procedureMethods[methodDef.SymbolId], baseMethodBuilder);
            }
        }

        // --- Pass 1.5b: implementazione di interfacce — AddInterfaceImplementation
        // dichiara il "contratto" a livello di metadati CLR; DefineMethodOverride collega
        // OGNI membro d'interfaccia (metodo o accessor get_/set_ di una PROPERTY) al
        // metodo CONCRETO della classe con lo stesso nome BASIC (ricerca anche nelle
        // classi base — un'implementazione ereditata è comunque valida), esplicito per lo
        // stesso motivo di Pass 1.5 (nomi CIL mangled sul SymbolId, il matching implicito
        // per nome+firma del CLR non troverebbe mai la coppia). Un membro d'interfaccia
        // senza implementazione corrispondente lancia esplicito — il Linker dovrebbe già
        // validare questo requisito semanticamente, ma qui serve comunque risolvere il
        // MethodBuilder concreto per il collegamento fisico.
        foreach (var classDef in classDefinitions)
        {
            foreach (var ifaceDef in classDef.ResolvedInterfaces)
            {
                classTypes[classDef.SymbolId].AddInterfaceImplementation(classTypes[ifaceDef.SymbolId]);
                foreach (var ifaceMember in ifaceDef.Members)
                {
                    switch (ifaceMember)
                    {
                        case MethodMember { Procedure: FunctionDefinition ifaceMethodDef }:
                            var implementingMethod = FindMethodInHierarchy(classDef, ifaceMethodDef.OriginalName.Lexeme, procedureMethods)
                                ?? throw new NotSupportedException(
                                    $"Backend IL: la classe '{classDef.OriginalName.Lexeme}' non implementa il metodo '{ifaceMethodDef.OriginalName.Lexeme}' richiesto dall'interfaccia '{ifaceDef.OriginalName.Lexeme}'.");
                            classTypes[classDef.SymbolId].DefineMethodOverride(implementingMethod, procedureMethods[ifaceMethodDef.SymbolId]);
                            break;

                        case PropertyMember ifacePropMember:
                            var ifacePropInfo = classProperties[ifacePropMember.Property.SymbolId];
                            var implementingPropSymbolId = ResolveMemberSymbolIdByName(classDef, ifacePropMember.Property.OriginalName.Lexeme);
                            var implementingPropInfo = classProperties[implementingPropSymbolId];
                            if (ifacePropInfo.Getter is not null)
                                classTypes[classDef.SymbolId].DefineMethodOverride(
                                    implementingPropInfo.Getter ?? throw new NotSupportedException($"Backend IL: la classe '{classDef.OriginalName.Lexeme}' non implementa il GET di '{ifacePropMember.Property.OriginalName.Lexeme}' richiesto dall'interfaccia '{ifaceDef.OriginalName.Lexeme}'."),
                                    ifacePropInfo.Getter);
                            if (ifacePropInfo.Setter is not null)
                                classTypes[classDef.SymbolId].DefineMethodOverride(
                                    implementingPropInfo.Setter ?? throw new NotSupportedException($"Backend IL: la classe '{classDef.OriginalName.Lexeme}' non implementa il SET di '{ifacePropMember.Property.OriginalName.Lexeme}' richiesto dall'interfaccia '{ifaceDef.OriginalName.Lexeme}'."),
                                    ifacePropInfo.Setter);
                            break;
                    }
                }
            }
        }

        // --- Pass 1.5c: implementazione di interfacce da parte di un UDT (TYPE X IMPLEMENTS
        // IFoo): AddInterfaceImplementation + DefineMethodOverride sugli stessi MethodBuilder
        // (i metodi del TYPE sono già in procedureMethods come Proc_<id>). Identico al
        // percorso delle CLASSI (Pass 1.5b), qui per un value type.
        foreach (var udtDef in udtDefinitions)
        {
            foreach (var ifaceDef in udtDef.ResolvedInterfaces)
            {
                classTypes[udtDef.SymbolId].AddInterfaceImplementation(classTypes[ifaceDef.SymbolId]);
                foreach (var ifaceMember in ifaceDef.Members)
                {
                    if (ifaceMember is not MethodMember { Procedure: FunctionDefinition ifaceMethodDef }) continue;
                    var udtImplementingMethod = udtDef.Methods
                        .OfType<FunctionDefinition>()
                        .FirstOrDefault(m => m.OriginalName.Lexeme.Equals(ifaceMethodDef.OriginalName.Lexeme, StringComparison.OrdinalIgnoreCase));
                    if (udtImplementingMethod is null)
                        throw new NotSupportedException(
                            $"Backend IL: l'UDT '{udtDef.OriginalName.Lexeme}' non implementa il metodo '{ifaceMethodDef.OriginalName.Lexeme}' richiesto dall'interfaccia '{ifaceDef.OriginalName.Lexeme}'.");
                    classTypes[udtDef.SymbolId].DefineMethodOverride(
                        procedureMethods[udtImplementingMethod.SymbolId],
                        procedureMethods[ifaceMethodDef.SymbolId]);
                }
            }
        }

        // --- Pass 1.7: scopre e compila ogni LambdaExpr del programma (vedi
        // IlProgramCompiler, sezione CLOSURES/LAMBDA) — DOPO che classi/UDT/procedure sono
        // tutte dichiarate (una lambda può referenziare qualunque cosa nel programma), PRIMA
        // che i corpi che le usano (Pass 1.6 sotto, Main, procedure, Pass 2) vengano
        // compilati. Le eventuali classi di chiusura sintetizzate vanno CreateType()-ate
        // insieme a quelle utente, in fondo (closureTypesToFinalize).
        var closureTypesToFinalize = new List<TypeBuilder>();
        var lambdaInfos = DiscoverAndCompileLambdas(
            program, moduleBuilder, types, classTypes, procedureMethods, procedureDefinitionsBySymbol,
            classFields, classFieldArrays, classConstructors, classProperties, classOperators,
            programType, closureTypesToFinalize);

        // --- Pass 1.6: costruttore di TIPO (.cctor) per ogni classe con almeno un campo
        // STATIC inizializzato — eseguito dal CLR UNA SOLA VOLTA al primo uso del tipo,
        // mai per istanza (a differenza del costruttore normale). Nessun parametro/istanza
        // (usa il costruttore "Main-like", senza _isInstanceMember).
        foreach (var group in staticFieldInitializers.GroupBy(x => x.ClassSymbolId))
        {
            var staticCtorBuilder = classTypes[group.Key].DefineTypeInitializer();
            var staticCtorCompiler = new IlProgramCompiler(program, staticCtorBuilder.GetILGenerator(), types, procedureMethods, procedureDefinitionsBySymbol, classTypes, classFields, classFieldArrays, classConstructors, classProperties, classOperators, lambdaInfos);
            foreach (var (_, field, initialValue) in group)
                staticCtorCompiler.EmitStaticFieldInitializer(field, initialValue);
            staticCtorCompiler.FinishStaticConstructor();
        }

        var mainMethod = programType.DefineMethod(
            "Main",
            MethodAttributes.Public | MethodAttributes.Static,
            types.Void,
            Type.EmptyTypes);

        var mainCompiler = new IlProgramCompiler(program, mainMethod.GetILGenerator(), types, procedureMethods, procedureDefinitionsBySymbol, classTypes, classFields, classFieldArrays, classConstructors, classProperties, classOperators, lambdaInfos, classFinalizers, udtFinalizers);

        // PrepareGraphicsHost DEVE precedere "new HeadlessRuntimeHost(...)": IRuntimeHost.
        // Graphics/Audio sono proprietà get-only lette UNA VOLTA SOLA nel costruttore da
        // HeadlessRuntimeHost.DefaultGraphics/DefaultAudio — chiamarlo dopo (come nel primo
        // tentativo di questo fix) non ha alcun effetto, l'host è già congelato su
        // NullGraphicsService.
        //
        // skipEntryInitialization: le pagine BSP girano IN-PROCESS DENTRO il programma server
        // (stesso processo, stessa ProgramGlobals condivisa keyed-by-symbolId). La entry del
        // server ha già fatto PrepareGraphicsHost + host + ResetProgramState all'avvio; se la
        // entry della pagina li rifacesse: (a) ResetProgramState() svuoterebbe ProgramGlobals →
        // il server perde i suoi globali (es. `pkgs`) e un FETCH della pagina verso lo STESSO
        // server risponde vuoto; (b) `new HeadlessRuntimeHost(Console.Out)` scarterebbe l'host
        // con outputWriter di ServeBspPageIlBackend. Con questo flag l'entry è SOLO il corpo
        // top-level; host/globali li prepara il chiamante (BspDispatch).
        if (!skipEntryInitialization)
        {
            // GMBASIC_HEADLESS=1 (runner headless: test_ilrun/sweep): salta PrepareGraphicsHost
            // così HeadlessRuntimeHost.DefaultGraphics resta null → il host usa NullGraphicsService
            // e i park grafici completano subito. Senza, l'entry cabla la SDL reale che i runner
            // non pompano → i park grafici non si completano mai (hang).
            var skipPrepare = mainCompiler._il.DefineLabel();
            mainCompiler._il.Emit(OpCodes.Ldstr, "GMBASIC_HEADLESS");
            mainCompiler._il.Emit(OpCodes.Call, typeof(System.Environment).GetMethod(nameof(System.Environment.GetEnvironmentVariable), [typeof(string)])!);
            mainCompiler._il.Emit(OpCodes.Ldstr, "1");
            mainCompiler._il.Emit(OpCodes.Call, typeof(string).GetMethod(nameof(string.Equals), [typeof(string), typeof(string)])!);
            mainCompiler._il.Emit(OpCodes.Brtrue, skipPrepare);
            mainCompiler._il.Emit(OpCodes.Call, typeof(IlRuntime).GetMethod(nameof(IlRuntime.PrepareGraphicsHost))!);
            mainCompiler._il.MarkLabel(skipPrepare);
            // Initialize runtime host before executing user code
            mainCompiler._il.Emit(OpCodes.Call, typeof(System.Console).GetProperty(nameof(System.Console.Out))!.GetGetMethod()!);
            mainCompiler._il.Emit(OpCodes.Ldnull);
            mainCompiler._il.Emit(OpCodes.Newobj, typeof(Runtime.HeadlessRuntimeHost).GetConstructor([typeof(TextWriter), typeof(Runtime.RuntimeConfig)])!);
            mainCompiler._il.Emit(OpCodes.Call, typeof(IlRuntime).GetMethod(nameof(IlRuntime.SetCurrent), [typeof(Runtime.IRuntimeHost)])!);
            mainCompiler._il.Emit(OpCodes.Call, typeof(IlRuntime).GetMethod(nameof(IlRuntime.ResetProgramState))!);
            // Scope globali isolato per QUESTA esecuzione (vedi IlRuntime.BeginProgramScope):
            // senza, due Main() compilati in overlap nello stesso processo (harness/test che
            // compilano+invocano più sample IL, es. via CompileToAssembly) condividono l'unico
            // ProgramGlobals statico — il reset dell'uno svuota i globali impliciti dell'altro
            // a metà esecuzione.
            mainCompiler._il.Emit(OpCodes.Call, typeof(IlRuntime).GetMethod(nameof(IlRuntime.BeginProgramScope))!);
            // Feeder console → InkeyQueue (tutti i path, flat+v2): senza, nel percorso FLAT
            // INKEY$ non-bloccante e WAITKEY/GET su console reale non vedevano MAI i tasti
            // (IlRuntime.Inkey() ritorna sempre "" su console — il fallback System.Console
            // esiste solo nei metodi bloccanti). Il feeder è no-op se stdin rediretto o
            // GMBASIC_HEADLESS (test/CI); in esecuzione reale drena la console nella coda
            // che Inkey()/ReadKey()/TryDequeueInkey consumano. GPM-safe (loop dedicato, chiama
            // solo QueueInkey = enqueue + wake; mai codice BASIC). Il wrapper v2
            // (RunV2EntryConsole) ferma il feeder nel finally; il flat lo ferma il runner
            // (REPL --il-module) o termina col processo (eseguibile standalone).
            mainCompiler._il.Emit(OpCodes.Call, typeof(IlConsoleKeyFeeder).GetMethod(nameof(IlConsoleKeyFeeder.Start))!);
        }

        // Statements.Count > 0 da solo NON basta a dire "c'è codice top-level reale": una
        // riga con solo IMPORT/REQUIRE lascia un RemStmt placeholder vuoto nello stream (il
        // namespace viene registrato altrove, il placeholder serve solo alla numerazione di
        // riga del BASIC classico) — con SOLO quello, Statements.Count era 1 anche per un
        // programma "IMPORT X: SUB Main() ... END SUB" senza NESSUN codice top-level reale,
        // e il ramo else sotto (EmitEntryPoint sui top-level statement) compilava un Main()
        // synthetic VUOTO, ignorando silenziosamente il vero SUB Main() dell'utente — che
        // finiva compilato come una procedura normale mai chiamata da nessuno. Exit code 0,
        // zero output, zero eccezioni: bug scoperto con `IMPORT JSON` + `SUB Main()` in
        // ForEachComprehensive.bas (le sezioni 5a/5b/5c usano JSON.PARSE).
        // DIM/GLOBAL/CONST a livello di modulo sono preambolo (inizializzatori), non il
        // programma: senza questo filtro, `GLOBAL Alive = 2` + `SUB Main()` compilava solo
        // gli store GLOBAL e non chiamava mai Main (ILOUT vuoto, exit 0).
        var hasExecutableTopLevel = program.MainModule.Statements.Values
            .SelectMany(stmts => stmts)
            .Any(s => s is not RemStmt and not ImportStmt and not RequireStmt
                      and not DeclarationStmt and not ConstStmt);
        // Se esiste un SUB Main valido (entry point dichiarato), gli statement top-level
        // del modulo sono SOLO preambolo (inizializzatori di globali, es. `DIM x AS INT`
        // seguito da `x = 0` a livello di modulo — un LetStmt di inizializzazione globale,
        // non "programma reale"): il vero entry è Main, come nel C99 (il codice numerato
        // del modulo resta raggiungibile via GOTO/GOSUB, mai eseguito all'avvio). Senza
        // questo, `g_notifiche = 0` a livello di modulo sopprimeva silenziosamente Main
        // (ILOUT vuoto, exit 0) — bug ClassesEvents/EventHandler.
        // SUB Main eventuale (entry point dichiarato: nome "Main", parametri opzionali).
        var mainProc = procedureDefinitions.FirstOrDefault(p =>
            p.OriginalName.Lexeme.Equals("Main", StringComparison.OrdinalIgnoreCase) &&
            p.Parameters.All(par => par.Flags.HasFlag(SymbolFlags.IsOptional)));

        if (hasExecutableTopLevel)
        {
            // Codice top-level ESEGUIBILE: è il programma. Se esiste ANCHE SUB Main, il
            // top-level va eseguito come PREAMBOLO PRIMA di Main (altrimenti è ignorato:
            // bug DIM/assegnazioni top-level + SUB Main → variabili vuote/0). Main GPM v2
            // (may-park) non è catenabile qui → resta l'entry GPM (limite noto).
            var mainIsGpm = mainProc is not null &&
                ShouldRunMainAsGpmV2(mainProc, procedureDefinitionsBySymbol, sym => program.Symbols.GetSymbol(sym).DataType,
                    sym => { var s = program.Symbols.GetSymbol(sym); return s.Flags.HasFlag(SymbolFlags.IsGlobal) || s.Flags.HasFlag(SymbolFlags.IsStaticStorage); });
            if (mainProc is not null && !mainIsGpm)
                mainCompiler._trailingMainCall = (mainProc, procedureMethods[mainProc.SymbolId]);
            if (mainProc is null || !mainIsGpm)
            {
                mainCompiler.EmitEntryPoint(program.MainModule);
            }
            else
            {
                mainCompiler.EmitModulePreamble(program.MainModule);
                mainCompiler.EmitEntryPointGpmV2(mainProc);
            }
        }
        else if (program.MainModule.LibraryName is not null && mainProc is null)
        {
            // LIBRARY: nessun entry point (il prodotto e' una libreria). Le PUBLIC SUB/FUNCTION
            // restano procedure normali (mai avviate); il metodo di entry resta vuoto.
            mainCompiler.EmitLibraryEntry(program.MainModule);
        }
        else
        {
            // Nessun codice top-level ESEGUIBILE: l'altro ramo valido secondo
            // Linker.HasEntryPoint è un SUB Main (nome "Main", tutti i parametri
            // opzionali) nello scope globale del modulo principale — già presente tra
            // procedureDefinitions/procedureMethods come una procedura normale (compilata
            // più sotto, nello stesso loop di ogni altra SUB/FUNCTION). Se HasEntryPoint
            // ha validato il programma, un candidato del genere esiste per costruzione.
            var mainProcDef = mainProc
                ?? throw new NotSupportedException("Backend IL: nessun SUB Main valido trovato (Linker.HasEntryPoint avrebbe dovuto escludere questo programma).");

            mainCompiler.EmitModulePreamble(program.MainModule);

            // GPM (STEP 1b): se Main è may-park FLAT (e perciò ha uno stepper Step_<id>
            // generato da EmitGpmSteppers), l'entry point avvia lo scheduler + il task Main
            // + IlGpmDriver invece di chiamare Proc_<Main> inline. Solo procedure may-park
            // FLAT sono supportate (caso semplice §13-2): per il resto resta il percorso
            // normale. EmitGpmSteppers viene chiamato DOPO (riga sotto il loop procedure),
            // quindi la decisione qui usa lo stesso predicato MayParkAnalysis.
            if (ShouldRunMainAsGpmV2(mainProcDef, procedureDefinitionsBySymbol, sym => program.Symbols.GetSymbol(sym).DataType,
                sym => { var s = program.Symbols.GetSymbol(sym); return s.Flags.HasFlag(SymbolFlags.IsGlobal) || s.Flags.HasFlag(SymbolFlags.IsStaticStorage); }))
            {
                // G7: PONYTAIL RIMOSSO — il driver v2 è l'unico path GPM.
                mainCompiler.EmitEntryPointGpmV2(mainProcDef);
            }
            else
            {
                mainCompiler.EmitEntryPointCallingMain(mainProcDef, procedureMethods[mainProcDef.SymbolId]);
            }
        }

        var mayParkForTrampoline = MayParkAnalysis.ComputeMayPark(
            [.. procedureDefinitions],
            id => procedureDefinitionsBySymbol.TryGetValue(id, out var d) ? d : null,
            sym => program.Symbols.GetSymbol(sym).DataType);

        // Nomi (originale e linkato) → SymbolId + tipo del simbolo: servono al gate
        // IsV2Subset del trampolino grafico — il trampolino si emette solo se esiste lo
        // StepV2_<id> (vedi IsGraphicsGpmTrampoline, fix ricorsione infinita).
        var trampolineNameToId = new Dictionary<string, int>(StringComparer.OrdinalIgnoreCase);
        foreach (var p in procedureDefinitions)
        {
            trampolineNameToId[p.OriginalName.Lexeme] = p.SymbolId;
            trampolineNameToId[p.Name.Lexeme] = p.SymbolId;
        }
        Func<int, Basic.DataType?> trampolineSymbolType = sym => program.Symbols.GetSymbol(sym).DataType;

        /* Runner delle righe NUMERATE legacy raggiunte da GOSUB dentro una procedura
         * (programmi ibridi SUB Main + codice numerato): un salto CIL non attraversa i
         * metodi, quindi ogni riga-target diventa un metodo (parità C99Native
         * lfn_num_<riga>). Da preparare DOPO le lambda (i runner possono contenerne) e
         * PRIMA dei corpi delle procedure (che le chiamano). */
        mainCompiler.PrepareModuleLineRunners(program.MainModule, programType, types,
            procedureMethods, procedureDefinitionsBySymbol, classTypes, classFields,
            classFieldArrays, classConstructors, classProperties, classOperators, lambdaInfos);

        foreach (var definition in procedureDefinitions)
        {
            // Procedure LOCALI con CATTURA (uso di un simbolo di uno scope ANCESTRO): le
            // catture BYVAL sono promosse a __ClosureCtx dal Linker e compilate come
            // MethodMember d'istanza (vedi EmitFunctionCall, ramo !methodBuilder.IsStatic).
            // La cattura BYREF NON è promuovibile (un campo non può essere T&): la closure
            // resterebbe un metodo statico senza legame al puntatore → NRE silenzioso.
            // SKIP esplicito per quel caso (FunctionsAdvanced Test 2); le BYVAL passano.
            if (localProcedureSymbolIds.Contains(definition.SymbolId))
            {
                var localIds = EnumerateDeclarationSymbols(definition.Body).ToHashSet();
                foreach (var vExpr in EnumerateVariableExprs(definition.Body))
                {
                    if (definition.Parameters.Any(p => p.SymbolId == vExpr.SymbolId)) continue;
                    if (localIds.Contains(vExpr.SymbolId)) continue;
                    // La cattura è promossa se il suo scope è una classe __ClosureCtx.
                    if (!IsSymbolInClosureContext(program, vExpr.SymbolId))
                        throw new NotSupportedException(
                            "Backend IL: closure di una procedura locale con cattura BYREF non ancora supportata.");
                }
            }

            var methodBuilder = procedureMethods[definition.SymbolId];
            var procedureCompiler = new IlProgramCompiler(program, methodBuilder.GetILGenerator(), types, procedureMethods, procedureDefinitionsBySymbol, classTypes, classFields, classFieldArrays, classConstructors, classProperties, classOperators, lambdaInfos, definition, classFinalizers, udtFinalizers);
            // ON ERROR dentro una PROCEDURA: il dispatch a chunk (EmitProcedureBodyWithErrorHandling)
            // non era MAI collegato al flusso — le procedure usavano EmitStatementBody lineare
            // (EmitOnErrorStatement → NotSupported "fuori dall'entry point"). Ora il percorso
            // a chunk è attivo anche qui (stesso modello dell'entry point).
            // SUB con LOAD TEXTURE/SPRITE LOAD: Proc_* lineare lancia (GPM obbligatorio).
            // Trampolino verso lo stepper Step_<id> emesso sotto da EmitGpmSteppers.
            if (!definition.Body.Any(StatementUsesOnErrorHandling)
                && IsGraphicsGpmTrampoline(definition, mayParkForTrampoline, trampolineNameToId, trampolineSymbolType,
                    sym => { var s = program.Symbols.GetSymbol(sym); return s.Flags.HasFlag(SymbolFlags.IsGlobal) || s.Flags.HasFlag(SymbolFlags.IsStaticStorage); }))
                procedureCompiler.EmitGpmGraphicsTrampoline(definition);
            else if (definition.Body.Any(StatementUsesOnErrorHandling))
                procedureCompiler.EmitProcedureBodyWithErrorHandling(definition.Body);
            else
                procedureCompiler.EmitProcedureBody(definition);
        }

        // --- Pass GPM-v2 (G2): genera gli StepV2_<symbolId> (IR stackless → CIL, transizioni)
        // per le procedure may-park nel sottoinsieme G2. G7: PONYTAIL RIMOSSO — NON si emette
        // più lo stepper flat Step_<id> (EmitGpmSteppers eliminato); il DRIVER v2 è l'unico
        // path GPM. Le procedure may-park fuori dal sottoinsieme v2 girano sul trampolino
        // grafico/flat se may-suspend, altrimenti lineare.
        // G4: riceve classTypes/classFields/classConstructors + il Program per le procedure
        // locali con CATTURA (classi __ClosureCtx, metodi d'istanza).
        IlCoroutineEmitter.EmitV2Steppers(procedureDefinitionsBySymbol, programType, mayParkForTrampoline,
            classTypes, classFields, classConstructors, program);

        // --- Pass 2: compila i corpi di metodi, costruttori e accessor PROPERTY di classe. ---
        foreach (var classDef in classDefinitions)
        {
            foreach (var member in classDef.Members)
            {
                switch (member)
                {
                    case MethodMember { Procedure: FunctionDefinition methodDef }:
                        var methodBuilder = procedureMethods[methodDef.SymbolId];
                        // STATIC: nessuna istanza implicita — riusa lo stesso costruttore
                        // "procedura top-level" (parametri a partire dall'indice 0,
                        // _isInstanceMember resta false, niente SELF/ME) invece di quello
                        // per metodi d'istanza (parametri da indice 1, SELF/ME risolti per
                        // nome). Un metodo statico è, a tutti gli effetti compilativi, una
                        // procedura top-level con lo scope di una classe.
                        var methodCompiler = methodDef.Flags.HasFlag(SymbolFlags.IsStatic)
                            ? new IlProgramCompiler(
                                program, methodBuilder.GetILGenerator(), types, procedureMethods, procedureDefinitionsBySymbol,
                                classTypes, classFields, classFieldArrays, classConstructors, classProperties, classOperators, lambdaInfos, methodDef)
                            : new IlProgramCompiler(
                                program, methodBuilder.GetILGenerator(), types, procedureMethods, procedureDefinitionsBySymbol,
                                classTypes, classFields, classFieldArrays, classConstructors, classProperties, classOperators, lambdaInfos, methodDef.Parameters, methodDef.ReturnType, classDef.SymbolId, methodDef.SymbolId);
                        methodCompiler.EmitProcedureBody(methodDef);
                        break;

                    case ConstructorMember ctorMember:
                        var ctorInfo = ResolveConstructorOverload(classConstructors[classDef.SymbolId], ctorMember.Constructor.Parameters.Length, classDef.OriginalName.Lexeme);
                        var ctorCompiler = new IlProgramCompiler(
                            program, ctorInfo.Builder.GetILGenerator(), types, procedureMethods, procedureDefinitionsBySymbol,
                            classTypes, classFields, classFieldArrays, classConstructors, classProperties, classOperators, lambdaInfos, ctorMember.Constructor.Parameters, null, classDef.SymbolId);
                        // BASE.NEW(args) esplicita passa args.Length argomenti al costruttore
                        // base; senza BASE.NEW si assume un costruttore base parameterless
                        // implicito (stile C#) — vedi EmitConstructorBody.
                        var baseArgCount = ctorMember.Constructor.Body is [BaseConstructorCallStmt baseCall, ..] ? baseCall.Args.Length : 0;
                        var baseConstructor = classDef.BaseClassSymbolId is int baseClassId
                            ? (ConstructorInfo)ResolveConstructorOverload(classConstructors[baseClassId], baseArgCount, classDef.ResolvedBaseClass!.OriginalName.Lexeme).Builder
                            : types.Object.GetConstructor(Type.EmptyTypes)!;
                        ctorCompiler.EmitConstructorBody(ctorMember.Constructor, baseConstructor, classFieldArrayDeclarations[classDef.SymbolId], classFieldArrays);
                        break;

                    case PropertyMember propMember:
                        var propInfo = classProperties[propMember.Property.SymbolId];
                        if (propInfo.Getter is not null)
                        {
                            var getterCompiler = new IlProgramCompiler(
                                program, propInfo.Getter.GetILGenerator(), types, procedureMethods, procedureDefinitionsBySymbol,
                                classTypes, classFields, classFieldArrays, classConstructors, classProperties, classOperators, lambdaInfos, ImmutableArray<Parameter>.Empty, propMember.Property.Type, classDef.SymbolId);
                            getterCompiler.EmitStatementBody(propMember.Property.Getter!.Body);
                        }
                        if (propInfo.Setter is not null)
                        {
                            var setterCompiler = new IlProgramCompiler(
                                program, propInfo.Setter.GetILGenerator(), types, procedureMethods, procedureDefinitionsBySymbol,
                                classTypes, classFields, classFieldArrays, classConstructors, classProperties, classOperators, lambdaInfos, [propMember.Property.Setter!.ValueParameter], null, classDef.SymbolId);
                            setterCompiler.EmitStatementBody(propMember.Property.Setter!.Body);
                        }
                        break;

                    case OperatorMember opMember:
                        var opBuilder = classOperators[(classDef.SymbolId, opMember.Operator.OperatorToken.Kind,
                            opMember.Operator.Parameters.Length, OperatorReturnKey(opMember.Operator.ReturnType))];
                        // "Procedura top-level" (parametri 0-based, niente SELF/ME) — stesso
                        // schema di un metodo STATIC (vedi _classOperators). OperatorDefinition
                        // non è una FunctionDefinition: sintetizza un wrapper con SymbolId=-1
                        // (mai risolto/usato — solo per riusare EmitProcedureBody), stesso
                        // identico trucco del Linker per riscrivere il corpo di un OPERATOR
                        // (Linker.Rewrites.Classes.cs, RewriteOperatorMember/dummyProcContext).
                        var opProcDef = new FunctionDefinition(-1, opMember.Operator.OperatorToken,
                            opMember.Operator.Parameters, opMember.Operator.ReturnType, opMember.Operator.Body, opMember.Operator.Flags);
                        var opCompiler = new IlProgramCompiler(
                            program, opBuilder.GetILGenerator(), types, procedureMethods, procedureDefinitionsBySymbol,
                            classTypes, classFields, classFieldArrays, classConstructors, classProperties, classOperators, lambdaInfos, opProcDef);
                        opCompiler.EmitProcedureBody(opProcDef);
                        break;

                    case DestructorMember dtorMember:
                        // Corpo del finalizer: membro d'istanza normale (SELF via arg 0),
                        // nessun parametro, nessun tipo di ritorno. NON chiama
                        // base.Finalize(): Object.Finalize è un no-op e l'ereditarietà di
                        // classi con DELETE multipli lungo la gerarchia non è ancora
                        // supportata/testata (una classe derivata con DELETE nasconde
                        // quello della base — gap esplicito, vedi TODO Obsidian).
                        var finalizerCompiler = new IlProgramCompiler(
                            program, classFinalizers[classDef.SymbolId].GetILGenerator(), types, procedureMethods, procedureDefinitionsBySymbol,
                            classTypes, classFields, classFieldArrays, classConstructors, classProperties, classOperators, lambdaInfos,
                            ImmutableArray<Parameter>.Empty, null, classDef.SymbolId);
                        finalizerCompiler.EmitStatementBody(dtorMember.Destructor.Body);
                        break;
                }
            }
        }

        // --- Pass 2.2: compila i corpi di metodi e costruttori di UDT — mai un
        // DestructorMember/PropertyMember/OperatorMember, esplicitamente rifiutati sopra.
        // Il costruttore d'istanza (1-based, SELF via arg 0) è lo STESSO usato per un
        // metodo/costruttore di CLASS: SELF per un value type è semplicemente un T&
        // invece di un T (gestito automaticamente dal CLR in base al DeclaringType, non
        // richiede alcuna logica diversa qui — vedi EmitMethodCallCore per l'unico punto
        // che DEVE distinguere value/reference type, il call site).
        foreach (var udtDef in udtDefinitions)
        {
            foreach (var procedureMethod in udtDef.Methods)
            {
                var method = (FunctionDefinition)procedureMethod;
                var udtMethodBuilder = procedureMethods[method.SymbolId];
                var udtMethodCompiler = method.Flags.HasFlag(SymbolFlags.IsStatic)
                    ? new IlProgramCompiler(
                        program, udtMethodBuilder.GetILGenerator(), types, procedureMethods, procedureDefinitionsBySymbol,
                        classTypes, classFields, classFieldArrays, classConstructors, classProperties, classOperators, lambdaInfos, method)
                    : new IlProgramCompiler(
                        // returnSymbolId = method.SymbolId (prima mancante, default -1): senza
                        // questo, EmitEndProcedureStatement non trova mai un return-slot condiviso
                        // col nome del metodo e la bare-assignment `NomeMetodo = espr` (convenzione
                        // "classica" per il valore di ritorno, oracolo samples/Types/
                        // TypesImplementsInterface.bas) non veniva MAI letta prima del ret — bug
                        // "UDT-Interface-Dispatch-Default-Su-Gambe-CSharp" (nulla a che vedere con
                        // INTERFACE: riproducibile su un TYPE con metodi e ZERO interfacce).
                        program, udtMethodBuilder.GetILGenerator(), types, procedureMethods, procedureDefinitionsBySymbol,
                        classTypes, classFields, classFieldArrays, classConstructors, classProperties, classOperators, lambdaInfos, method.Parameters, method.ReturnType, udtDef.SymbolId, method.SymbolId);
                udtMethodCompiler.EmitProcedureBody(method);
            }

            foreach (var op in udtDef.Operators)
            {
                var opReturnType = ResolveOperatorReturnType(op.ReturnType, program);
                var builder = classOperators[(udtDef.SymbolId, op.OperatorToken.Kind, op.Parameters.Length,
                    OperatorReturnKey(opReturnType))];
                var definition = new FunctionDefinition(-1, op.OperatorToken, op.Parameters,
                    opReturnType, op.Body, op.Flags);
                var compiler = new IlProgramCompiler(
                    program, builder.GetILGenerator(), types, procedureMethods, procedureDefinitionsBySymbol,
                    classTypes, classFields, classFieldArrays, classConstructors, classProperties,
                    classOperators, lambdaInfos, definition);
                compiler.EmitProcedureBody(definition);
            }

            foreach (var (ctorInfo, ctorDef) in classConstructors[udtDef.SymbolId].Zip(udtConstructorDeclarations[udtDef.SymbolId]))
            {
                var udtCtorCompiler = new IlProgramCompiler(
                    program, ctorInfo.Builder.GetILGenerator(), types, procedureMethods, procedureDefinitionsBySymbol,
                    classTypes, classFields, classFieldArrays, classConstructors, classProperties, classOperators, lambdaInfos, ctorDef.Parameters, null, udtDef.SymbolId);
                udtCtorCompiler.EmitUdtConstructorBody(ctorDef, classTypes[udtDef.SymbolId], udtFieldArrayDeclarations[udtDef.SymbolId], classFieldArrays);
            }

            // Corpi degli accessor PROPERTY di UDT (SELF via arg 0, come i metodi d'istanza).
            foreach (var udtProp in udtDef.Properties)
            {
                var udtPropInfo = classProperties[udtProp.SymbolId];
                if (udtPropInfo.Getter is not null && udtProp.Getter is not null)
                {
                    var udtGetterCompiler = new IlProgramCompiler(
                        program, udtPropInfo.Getter.GetILGenerator(), types, procedureMethods, procedureDefinitionsBySymbol,
                        classTypes, classFields, classFieldArrays, classConstructors, classProperties, classOperators, lambdaInfos,
                        ImmutableArray<Parameter>.Empty, NormalizeInferredType(udtProp.Type), udtDef.SymbolId);
                    udtGetterCompiler.EmitStatementBody(udtProp.Getter.Body);
                }
                if (udtPropInfo.Setter is not null && udtProp.Setter is not null)
                {
                    var udtSetterCompiler = new IlProgramCompiler(
                        program, udtPropInfo.Setter.GetILGenerator(), types, procedureMethods, procedureDefinitionsBySymbol,
                        classTypes, classFields, classFieldArrays, classConstructors, classProperties, classOperators, lambdaInfos,
                        [udtProp.Setter.ValueParameter], null, udtDef.SymbolId);
                    udtSetterCompiler.EmitStatementBody(udtProp.Setter.Body);
                }
            }
            // Corpi dei distruttori di UDT (`__Destructor`): membro d'istanza (SELF via arg 0),
            // nessun parametro/ritorno — stesso schema del finalizer di classe.
            foreach (var udtDtor in udtDef.Destructors)
            {
                if (!udtFinalizers.TryGetValue(udtDef.SymbolId, out var udtDtorBuilder)) continue;
                var udtDtorCompiler = new IlProgramCompiler(
                    program, udtDtorBuilder.GetILGenerator(), types, procedureMethods, procedureDefinitionsBySymbol,
                    classTypes, classFields, classFieldArrays, classConstructors, classProperties, classOperators, lambdaInfos,
                    ImmutableArray<Parameter>.Empty, null, udtDef.SymbolId);
                udtDtorCompiler.EmitStatementBody(udtDtor.Body);
            }
        }

        foreach (var kv in classTypes)
        {
            var classClrType = kv.Value.CreateType();
            if (udtBasicNames.TryGetValue(kv.Key, out var udtBasicName))
                Basic.IlBackend.IlRuntime.RegisterUdtTypeName(classClrType, udtBasicName,
                    udtFieldCounts.TryGetValue(kv.Key, out var udtNf) ? udtNf : -1);
        }
        foreach (var closureTypeBuilder in closureTypesToFinalize) closureTypeBuilder.CreateType();
        // Delegate generati per arità > 16 (vedi DataTypeMapper/DelegateTypeBeyondBcl): creati
        // DOPO i tipi utente, che possono comparire nei loro parametri.
        if (types.PendingDelegateTypes != null)
            foreach (var dynDelegate in types.PendingDelegateTypes) dynDelegate.CreateType();
        programType.CreateType();
        entryPointToken = mainMethod.MetadataToken;
        return assemblyBuilder;
    }
}
