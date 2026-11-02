// FILE: Interpreter.Compiler.Compiler.UserTypes.cs

using System.Collections.Immutable;

namespace Compiler;

public sealed partial class Compiler
{
    /// <summary>
    /// Nome VTable convenzionale per un operatore sovraccaricato (es. "__ADD__", "__EQUAL__").
    /// CTYPE è l'unico operatore per cui una CLASS/TYPE dichiara tipicamente PIÙ overload
    /// (uno per ogni tipo di destinazione, es. "Punto → STRING" e "Punto → INTEGER") — dato
    /// che la VTable è un semplice dizionario nome→indirizzo, usare il nome bare "__CTYPE__"
    /// per tutti farebbe collidere gli overload (l'ultimo registrato sovrascriverebbe i
    /// precedenti). Per CTYPE il nome include quindi anche il tipo di ritorno.
    /// </summary>
    private static string OperatorVTableName(TokenKind opKind, DataType returnType)
    {
        var baseName = $"__{opKind.ToString().ToUpperInvariant()}__";
        if (opKind != TokenKind.CType) return baseName;
        var typeSuffix = returnType.ToString()!.ToUpperInvariant().Replace(" ", "").Replace("*", "");
        return baseName + typeSuffix;
    }

    private static (ClassMember? Member, ClassDefinition? Owner) FindFieldInHierarchy(ClassDefinition startingClass, string memberName)
    {
        var current = startingClass;
        while (current != null)
        {
            var member = current.Members.FirstOrDefault(m =>
                m.Name.Lexeme.Equals(memberName, StringComparison.OrdinalIgnoreCase));
            if (member != null)
                return (member, current);
            current = current.ResolvedBaseClass;
        }
        return (null, null);
    }

    /// <summary>
    /// Gestisce l'assegnazione a un campo di una struttura (UDT/Classe) o a una proprietà
    /// di un oggetto built-in (es. Collezione).
    /// </summary>
    private void AssignField(FieldTarget target)
    {
        // Determine base type before LoadVariable, so we can emit ADJUST_STRING
        // for fixed-length string fields BEFORE pushing the object
        var baseType = target.Structure is not null ? InferExpressionTypeFromTarget(target.Structure) : null;

        // Campo STATIC di CLASS: nessuna istanza da pushare (condiviso da tutte, vedi
        // OP_STORE_STATIC_FIELD). Va controllato PRIMA di pushare target.Structure sotto — sia che
        // l'accesso sia SELF.Campo = ... sia NomeClasse.Campo = ... (Structure in quel caso e' un
        // riferimento alla classe stessa: InferExpressionTypeFromTarget non riesce a risolverlo a
        // ClassType — il Compiler non ha accesso alla SymbolTable — quindi il controllo si basa
        // direttamente su target.FieldSymbolId, già risolto correttamente dal Linker
        // indipendentemente dalla forma di Structure, esattamente come per la lettura in
        // VisitFieldAccessExpression).
        if (target.FieldSymbolId >= 0 && _staticFieldSymbolIds.Contains(target.FieldSymbolId))
        {
            EmitByte((byte)OpCode.OP_STORE_STATIC_FIELD);
            EmitShort(target.FieldSymbolId);
            return;
        }

        // Emit ADJUST_STRING for fixed-length string fields.
        // NB: si usa un tipo SEPARATO (adjType) — MAI baseType — perché baseType governa più
        // sotto la SCELTA dell'opcode di scrittura (OP_STORE_MEMBER per-offset vs
        // OP_STORE_FIELD per-nome): sovrascriverlo per SELF cambierebbe quel percorso (rotto
        // il campo LIST, count=0). adjType serve SOLO a trovare la lunghezza fissa.
        // Per SELF/ME InferExpressionTypeFromTarget può non risolvere il ClassType (il local
        // SELF non è sempre in _locals → DeduceTypeFromSuffix → DoubleType): in un
        // costruttore/metodo d'istanza il tipo è noto da _currentMethodClassType.
        var adjType = baseType;
        if (adjType is not (UserDefinedType or ClassType) && _currentMethodClassType is not null
            && target.Structure is VariableTarget selfVt
            && (selfVt.NameCleaned.Lexeme.Equals("SELF", StringComparison.OrdinalIgnoreCase)
                || selfVt.NameCleaned.Lexeme.Equals("ME", StringComparison.OrdinalIgnoreCase)))
        {
            adjType = _currentMethodClassType;
        }
        int fixedLen = -1;
        if (adjType is UserDefinedType udtAdj)
        {
            var fieldDecl = udtAdj.Fields.FirstOrDefault(f => f.Name.Lexeme.Equals(target.OriginalToken.Lexeme, StringComparison.OrdinalIgnoreCase));
            if (fieldDecl?.DataType is StringType { FixedLength: int fl } && fl > 0) fixedLen = fl;
        }
        else if (adjType is ClassType ctCheck)
        {
            var fieldMem = ctCheck.Definition.Members.OfType<FieldMember>().FirstOrDefault(m => m.Declaration.NameCleaned.Lexeme.Equals(target.OriginalToken.Lexeme, StringComparison.OrdinalIgnoreCase));
            if (fieldMem?.Declaration?.DataType is StringType { FixedLength: int fl } && fl > 0) fixedLen = fl;
        }
        if (fixedLen > 0)
        {
            EmitByte((byte)OpCode.OP_ADJUST_STRING);
            EmitShort((ushort)fixedLen);
        }

        // Now push the structure object (the value is already on the stack)
        if (target.Structure is null)
        {
            if (_globals.TryGetValue("__with_obj", out var withGlobalIdx))
            {
                EmitByte((byte)OpCode.OP_LOAD_GLOBAL);
                EmitShort(withGlobalIdx);
            }
            else
            {
                throw new Exception("Compiler error: implicit WITH field access outside of WITH block.");
            }
        }
        else if (target.Structure is ArrayElementTarget aet && !aet.Indices.IsEmpty)
        {
            // `Arr(i).Campo = valore`: la struttura intermedia è un elemento di array — usa
            // OP_LOAD_ELEMENT_REF invece del normale OP_LOAD_ELEMENT (emesso da LoadVariable)
            // per dare al backend VM in C un punto distinto dove evitare la copia superficiale
            // che applica di norma a un elemento GMB_OBJ_DICT/GMB_OBJ_LIST (semantica "UDT è un
            // tipo valore", corretta per un RVALUE ma non per un target di scrittura — vedi
            // OP_LOAD_ELEMENT_REF in OpCode.cs). Stessa identica sequenza di caricamento indici
            // del caso ArrayElementTarget in LoadVariable (Compiler.Variables.cs), solo
            // l'opcode finale cambia.
            LoadVariable(new VariableTarget(-1, aet.NameSuffixed, aet.NameCleaned));
            foreach (var index in aet.Indices)
            {
                VisitExpression(index);
            }
            EmitBytes((byte)OpCode.OP_LOAD_ELEMENT_REF, (byte)aet.Indices.Length);
        }
        else if (target.Structure is IndexedTarget itx && !itx.Indices.IsEmpty)
        {
            // `SELF.arr(i).campo = v` / `arr(i).campo = v` (campo-array di CLASSE): la struttura
            // è un IndexedAccessExpr su un campo-array → stesso trattamento _REF di
            // ArrayElementTarget (senza, l'elemento dict è copiato dal VM-C e la scrittura del
            // campo va persa: `arr(i).campo` restava 0 su VM-C).
            VisitExpression(itx.Base);
            foreach (var index in itx.Indices)
            {
                VisitExpression(index);
            }
            EmitBytes((byte)OpCode.OP_LOAD_ELEMENT_REF, (byte)itx.Indices.Length);
        }
        else
        {
            LoadVariable(target.Structure);
        }

        if (baseType is UserDefinedType udt || baseType is ClassType)
        {
            var typeName = baseType is UserDefinedType u ? u.Name.Lexeme : ((ClassType)baseType).Definition.Name.Lexeme;
            var fieldName = target.OriginalToken.Lexeme;
            var fieldKey = $"{typeName}.{fieldName}";
            if (!_fieldOffsets.TryGetValue(fieldKey, out var offset))
            {
                bool isProperty = false;
                ClassMember? foundMember = null;
                ClassDefinition? ownerClass = null;

                if (baseType is UserDefinedType udtCheck)
                {
                    isProperty = udtCheck.Properties.Any(p =>
                        p.Name.Lexeme.Equals(fieldName, StringComparison.OrdinalIgnoreCase));
                }
                else if (baseType is ClassType classTypeCheck)
                {
                    (foundMember, ownerClass) = FindFieldInHierarchy(classTypeCheck.Definition, fieldName);
                    if (foundMember is PropertyMember { Property.Setter: not null } propMember && ownerClass is not null)
                    {
                        // Chiama il setter compilato da VisitPropertyMember (convenzione
                        // ClassName.Set_PropertyName) invece di scrivere direttamente su un
                        // campo dinamico (OP_STORE_FIELD): quest'ultimo bypasserebbe del tutto
                        // la logica del setter (es. validazione dell'input), che pero' non
                        // viene mai eseguita finche' non esiste un P-CODE VM runtime (vedi
                        // 02-Knowledge/GMBasic-GPM-State-Machine-Patterns.md nel vault) — qui
                        // e' comunque la scelta piu' corretta per coerenza futura.
                        // Stack: [..., istanza, valore]. Setter compilato con locali
                        // [0]=SELF, [1]=VALUE — stessa convenzione posizionale di BASE.NEW
                        // (VisitConstructorMember): argomenti reali, poi SELF, poi conteggio.
                        EmitConstant(2.0); // argomenti: SELF, VALUE
                        var setterName = $"{ownerClass.Name.Lexeme}.Set_{propMember.Name.Lexeme}";
                        var callLocation = EmitJump((byte)OpCode.OP_CALL_CTOR);
                        _jumpsToPatch.Add((callLocation,
                            new Token(TokenKind.Identifier, setterName, null, target.OriginalToken.Position)));
                        // L'assegnazione a proprietà è uno STATEMENT: il valore di ritorno del
                        // setter (spinto da OP_RETURN) va scartato, altrimenti resta sullo stack
                        // del chiamante e OP_RETURN lo scambia per il valore di ritorno della
                        // procedura corrente (stack leak: `b = NEW B(...)` tornava NIL).
                        EmitByte((byte)OpCode.OP_POP);
                        return;
                    }
                    isProperty = foundMember is PropertyMember;
                }

                if (isProperty)
                {
                    var fieldNameIndex = AddConstant(fieldName);
                    EmitByte((byte)OpCode.OP_STORE_FIELD);
                    EmitShort(fieldNameIndex);
                    return;
                }

                // Campo ereditato da una base: il membro è stato trovato nella gerarchia ma
                // _fieldOffsets non ha la chiave "TipoDerivato.Campo" (il campo è registrato
                // sotto la classe che lo DICHIARA). Usa l'offset dell'OWNER (es. "Base.Req").
                if (foundMember is FieldMember inheritedField && ownerClass != null)
                {
                    var ownerKey = $"{ownerClass.Name.Lexeme}.{fieldName}";
                    if (_fieldOffsets.TryGetValue(ownerKey, out var inheritedOffset))
                    {
                        EmitByte((byte)OpCode.OP_STORE_MEMBER);
                        EmitShort(inheritedOffset);
                        return;
                    }
                }

                throw new Exception(
                    $"Compiler error: Field '{target.OriginalToken.Lexeme}' not found for assignment in resolved type '{typeName}'.");
            }

            EmitByte((byte)OpCode.OP_STORE_MEMBER);
            EmitShort(offset);
        }
        else
        {
            var fieldNameIndex = AddConstant(target.OriginalToken.Lexeme);
            EmitByte((byte)OpCode.OP_STORE_FIELD);
            EmitShort(fieldNameIndex);
        }
    }

    // --- Compilazione di TYPE...END TYPE ---

    /// <summary>
    /// Compila un TYPE...END TYPE. Genera un descrittore di tipo in fase di compilazione e,
    /// se il TYPE ha costruttori/metodi/operatori/distruttori (i TYPE in GMBasic possono
    /// averne, non sono semplici struct), compila anche il loro P-Code — senza, un TYPE con
    /// costruttore usato tramite <c>NEW MioTipo(args)</c> non avrebbe MAI un descrittore in
    /// <c>_compiledClassDescriptors</c> (quel dizionario era popolato solo da
    /// VisitClassDefinition), causando "UDT descriptor not found" in VisitNewExpression —
    /// vedi 02-Knowledge/GMBasic_Issues.md sessione 2026-07-14 (sessione 4) nel vault.
    /// I TYPE non supportano INHERITS: nessuna V-Table/classe base da gestire, a differenza
    /// di VisitClassDefinition.
    /// </summary>
    private void VisitUserDefinedType(UserDefinedType udt)
    {
        // Campi indicizzati per SymbolId, non per byte-offset cumulativo — deve combaciare
        // con VisitFieldAccessExpression (Compiler.Expressions.cs), che per QUALUNQUE campo
        // (CLASS o UDT) emette SEMPRE expr.FieldSymbolId come operando di OP_LOAD_MEMBER: lo
        // stesso schema già usato correttamente da VisitClassDefinition (_fieldOffsets e
        // FieldNames chiavate per SymbolId). Prima di questo fix, un TYPE (a differenza di
        // una CLASS) usava qui un offset a byte cumulativo (0, poi +size del campo
        // precedente, ecc.) sia in _fieldOffsets (letto da AssignField per OP_STORE_MEMBER)
        // sia nel descrittore FieldNames — un numero completamente scorrelato dal SymbolId
        // globale che il lato LETTURA emette sempre, indipendentemente da CLASS/UDT: `DIM v
        // AS T: v.X = 42: PRINT v.X` scriveva in dict["X"] (via FieldNames[0] = "X", offset
        // corretto per il primo/unico campo) ma leggeva usando l'ID globale di X come
        // "offset" (es. 47) — fuori dalle voci di un FieldNames con una manciata di campi, la
        // lookup falliva silenziosamente restituendo null/0 (vedi 02-Knowledge/
        // GMBasic-PCODE-UDT-Field-Access-Bug.md nel vault).
        var fieldOffsetsInThisUdt = new Dictionary<string, ushort>(StringComparer.OrdinalIgnoreCase);

        foreach (var field in udt.Fields)
        {
            var fieldKey = $"{udt.Name.Lexeme}.{field.Name.Lexeme}";
            _fieldOffsets[fieldKey] = (ushort)field.SymbolId;
            fieldOffsetsInThisUdt[field.Name.Lexeme] = (ushort)field.SymbolId;
        }

        // NOTA: il descrittore viene registrato SEMPRE, anche per un TYPE senza costruttori
        // espliciti — NEW MioTipo() con parentesi vuote (costruzione di default) passa
        // comunque da VisitNewExpression "Caso 2" (ConstructorArgs.HasValue e' true anche per
        // una lista vuota) e richiede un descrittore in _compiledClassDescriptors. Un early
        // return qui per i TYPE "puri dati" causava una regressione ("UDT descriptor for
        // 'Contatto' not found" su Types/TypesNullConditional.bas, che usa `NEW Contatto()`
        // su un TYPE senza SUB NEW).
        var classDescriptor = new Dictionary<string, object?>(StringComparer.OrdinalIgnoreCase);
        classDescriptor["Name"] = udt.Name.Lexeme;
        classDescriptor["BaseClass"] = null;
        classDescriptor["FieldOffsets"] = fieldOffsetsInThisUdt;
        // FieldNames chiavate per SymbolId (non byte offset) — vedi commento sopra: deve
        // combaciare con expr.FieldSymbolId emesso da VisitFieldAccessExpression, stesso
        // schema già usato da VisitClassDefinition per le CLASS.
        classDescriptor["FieldNames"] = udt.Fields.ToDictionary(f => f.SymbolId, f => f.Name.Lexeme);
        // Tipo dichiarato di ogni campo (SymbolId -> DataType), usato dalla VM per spingere un
        // valore di default TIPIZZATO (0.0 per i numerici, "" per le stringhe, ecc.) invece di
        // "" incondizionato per ogni campo non ancora assegnato — vedi OP_NEW_UDT_INSTANCE e
        // MakeDefaultUdtInstance in PCodeProcessor.cs.
        classDescriptor["FieldTypes"] = udt.Fields.ToDictionary(f => f.SymbolId, f => (DataType)f.DataType);
        // Descriptor dell'ELEMENTO per i campi ARRAY/LIST OF <ElemUDT> (SymbolId -> descriptor
        // {FieldNames,FieldTypes}): il VM-C/PCode lo usa per agganciare il __class__ agli
        // elementi — campi LIST OF popolati da JSON.TOOBJECT (gli elementi arrivano come dict
        // SENZA __class__) e array-campo UDT. Parità col path CLASS (VisitClassDefinition).
        {
            var fieldArrElemDesc = new Dictionary<int, object?>();
            foreach (var field in udt.Fields)
            {
                UserDefinedType? udtElem = null;
                if (!field.Dimensions.IsEmpty)
                {
                    if (field.DataType is ArrayType faAt && faAt.ElementType is UserDefinedType ue1) udtElem = ue1;
                }
                else if (field.DataType is ListType ltFld && ltFld.ElementType is UserDefinedType ue2)
                {
                    udtElem = ue2;
                }
                if (udtElem != null)
                {
                    fieldArrElemDesc[field.SymbolId] = new Dictionary<string, object?>
                    {
                        ["FieldNames"] = udtElem.Fields.ToDictionary(f => (object)f.SymbolId, f => (object?)f.Name.Lexeme),
                        ["FieldTypes"] = udtElem.Fields.ToDictionary(f => (object)f.SymbolId, f => (object?)f.DataType),
                    };
                }
            }
            classDescriptor["FieldArrayElemDesc"] = fieldArrElemDesc;
        }
        // Dimensione in BYTE del layout (PACKED TYPE = somma dei campi, altrimenti
        // allineamento naturale), calcolata a compile-time dall'unica sorgente del canone
        // TypeLayout. La chiave "Size" è ciò che SYSTEM.SIZEOF legge a runtime su PCodeVM e
        // VM/C (rispettivamente PCodeProcessor.Builtins.cs e gmb_bf_594 in gmb_builtin.c):
        // quei due backend eseguono lo STESSO numero degli altri tre, che lo calcolano
        // direttamente dall'AST, senza replicare la tabella delle larghezze.
        classDescriptor["Size"] = (double)TypeLayout.UdtByteSize(udt);
        classDescriptor["Constructors"] = udt.Constructors.Select(_ => $"{udt.Name.Lexeme}#{udt.SymbolId}.__new").ToList();
        // Bug preesistente corretto: controllava udt.Constructors invece di udt.Destructors,
        // quindi qualunque TYPE con un costruttore ma senza distruttore avrebbe comunque
        // registrato "Nome.__delete" come distruttore (mai compilato, indirizzo inesistente).
        classDescriptor["Destructor"] = udt.Destructors.Length > 0 ? $"{udt.Name.Lexeme}.__delete" : null;
        classDescriptor["VTable"] = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
        var vtable = (Dictionary<string, string>)classDescriptor["VTable"]!;
        foreach (var method in udt.Methods)
            vtable[method.OriginalName.Lexeme] = $"{udt.Name.Lexeme}.{method.OriginalName.Lexeme}";
        foreach (var op in udt.Operators)
        {
            var vOpName = OperatorVTableName(op.OperatorToken.Kind, op.ReturnType);
            vtable[vOpName] = $"{udt.Name.Lexeme}.{vOpName}";
        }

        _compiledClassDescriptors[udt.Name.Lexeme + "#" + udt.SymbolId] = classDescriptor;

        // Compila i corpi di costruttori/metodi/operatori/distruttori — assenti nella
        // versione precedente (solo il descrittore veniva registrato, mai il P-Code dei
        // membri: NEW MioTipo(...) avrebbe chiamato un indirizzo mai popolato in
        // _procedureAddresses).
        BeginScope();
        AddLocal(new Token(TokenKind.Identifier, "SELF", null, SourcePosition.Unknown)); // SELF è il primo locale.

        foreach (var ctor in udt.Constructors) VisitUdtConstructor(ctor, udt);
        foreach (var method in udt.Methods) VisitUdtMethod(method, udt);
        foreach (var op in udt.Operators) VisitUdtOperator(op, udt);
        foreach (var dtor in udt.Destructors) VisitUdtDestructor(dtor, udt);

        EndScope();

        // Compila proprieta' DOPO costruttori (evita collisione indirizzi)
        if (udt.Properties.Length > 0)
        {
            var propGetters = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
            var propSetters = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
            foreach (var prop in udt.Properties)
            {
                var g=prop.Getter; if(g!=null&&g.Body.Length>0){var getterBody=g.Body;
                {
                    var getterName = $"{udt.Name.Lexeme}.get_{prop.Name.Lexeme}";
                    _procedureAddresses[getterName] = _chunk.Code.Count;
                    BeginScope();
                    AddLocal(new Token(TokenKind.Identifier, "SELF", null, SourcePosition.Unknown));
                    _isReachable = true;
                    PreScanWholeBodyNativeFusions(getterBody);
                    foreach (var stmt in getterBody) VisitStatement(stmt);
                    EmitByte((byte)OpCode.OP_RETURN);
                    EndScope();
                    propGetters[prop.Name.Lexeme] = getterName;
                }}
                var s=prop.Setter; if(s!=null&&s.Body.Length>0){var setterBody=s.Body;
                {
                    var setterName = $"{udt.Name.Lexeme}.set_{prop.Name.Lexeme}";
                    _procedureAddresses[setterName] = _chunk.Code.Count;
                    BeginScope();
                    AddLocal(new Token(TokenKind.Identifier, "SELF", null, SourcePosition.Unknown));
                    AddLocal(new Token(TokenKind.Identifier, "VALUE", null, SourcePosition.Unknown));
                    _isReachable = true;
                    PreScanWholeBodyNativeFusions(setterBody);
                    foreach (var stmt in setterBody) VisitStatement(stmt);
                    EmitByte((byte)OpCode.OP_RETURN);
                    EndScope();
                    propSetters[prop.Name.Lexeme] = setterName;
                }}
            }
            classDescriptor["PropertyGetters"] = propGetters;
            classDescriptor["PropertySetters"] = propSetters;
        }
    }

    /// <summary>
    /// Compila un costruttore (SUB NEW) di un TYPE. Analogo a VisitConstructorMember ma
    /// senza gestione BASE.NEW (i TYPE non supportano INHERITS).
    /// </summary>
    private void VisitUdtConstructor(ConstructorDefinition ctor, UserDefinedType udt)
    {
        var fullCtorName = $"{udt.Name.Lexeme}#{udt.SymbolId}.__new";
        _procedureAddresses[fullCtorName] = _chunk.Code.Count;

        var originalLocalsCount = _locals.Count;
        foreach (var param in ctor.Parameters) AddLocal(param.Name, param.DataType);

        // Fusioni native "tutto il ciclo in un opcode" (vedi Compiler.NativeLoopFusion.cs).
        PreScanWholeBodyNativeFusions(ctor.Body);

        foreach (var stmt in ctor.Body)
        {
            // ConstStmt NON è più skippato: scorre a VisitStatement -> case ConstStmt ->
            // VisitConstStatement (registrazione in _constants). Prima le costanti locali
            // non venivano MAI registrate (bug %NOME condiviso, fix GMBasicAI).
            if (stmt is DataStmt or DeclarationStmt { Keyword.Kind: TokenKind.Global }) continue;
            VisitStatement(stmt);
        }

        while (_locals.Count > originalLocalsCount && _locals[^1].Name.Lexeme != "SELF")
        {
            _locals.RemoveAt(_locals.Count - 1);  // No OP_POP: frame locals, not global stack
        }

        EmitByte((byte)OpCode.OP_RETURN);
    }

    /// <summary>
    /// Compila un metodo (SUB/FUNCTION) definito in un TYPE. Analogo a VisitMethodMember ma
    /// per ProcedureDefinition nuda (i metodi di TYPE non sono avvolti in un MethodMember).
    /// </summary>
    private void VisitUdtMethod(ProcedureDefinition method, UserDefinedType udt)
    {
        var fullMethodName = $"{udt.Name.Lexeme}.{method.OriginalName.Lexeme}";
        _procedureAddresses[fullMethodName] = _chunk.Code.Count;

        var originalLocalsCount = _locals.Count;
        _isReachable = true;

        // Stesso fix "Z1" già applicato a VisitMethodMember (metodi di CLASS), mai portato qui:
        // senza _currentFunctionReturnType/_currentProcedureName, END FUNCTION dentro un
        // metodo di TYPE compila via VisitEndProcedureStatement con contesto nullo → emette
        // OP_PUSH_NIL prima di OP_RETURN invece di OP_LOAD_LOCAL(return-slot) → ogni FUNCTION
        // di un UDT restituiva sempre NIL. _currentProcedureName serve anche a ResolveLocal per
        // trovare il return-slot: la bare-assignment `NomeMetodo = ...` (convenzione oracolo
        // samples/Types/TypesImplementsInterface.bas) crea il local implicito col
        // nome del metodo dopo SELF+parametri — senza il nome giusto in contesto, quel local
        // non viene mai trovato in lettura. Bug "UDT-Interface-Dispatch-Default-Su-Gambe-
        // CSharp" (nulla a che vedere con INTERFACE — riproducibile su un TYPE senza alcuna
        // interfaccia).
        var previousFunctionReturnType = _currentFunctionReturnType;
        var previousProcedureName = _currentProcedureName;
        _currentFunctionReturnType = (method as FunctionDefinition)?.ReturnType;
        _currentProcedureName = method.OriginalName.Lexeme;

        foreach (var param in method.Parameters) AddLocal(param.Name, param.DataType);

        // Fusioni native "tutto il ciclo in un opcode" (vedi Compiler.NativeLoopFusion.cs).
        PreScanWholeBodyNativeFusions(method.Body);

        foreach (var stmt in method.Body)
        {
            // ConstStmt NON è più skippato (fix %NOME, twin-port GMBasicAI).
            if (stmt is DataStmt or DeclarationStmt { Keyword.Kind: TokenKind.Global }) continue;
            VisitStatement(stmt);
        }

        while (_locals.Count > originalLocalsCount && _locals[^1].Name.Lexeme != "SELF")
        {
            _locals.RemoveAt(_locals.Count - 1);  // No OP_POP: frame locals, not global stack
        }

        _currentFunctionReturnType = previousFunctionReturnType;
        _currentProcedureName = previousProcedureName;

        EmitByte((byte)OpCode.OP_RETURN);
    }

    /// <summary>
    /// Compila un operatore sovraccaricato definito in un TYPE. Analogo a
    /// VisitOperatorMember — a differenza di VisitUdtConstructor/VisitUdtMethod, si avvolge
    /// nel proprio scope (BeginScope/EndScope) invece di riusare il SELF esterno: senza, i
    /// parametri dell'operatore (es. 'a', 'b') resterebbero in _locals oltre la fine di
    /// questo operatore, disallineando gli indici locali del membro compilato successivamente
    /// (bug osservato con 2+ operatori nello stesso TYPE).
    /// </summary>
    private void VisitUdtOperator(OperatorDefinition op, UserDefinedType udt)
    {
        var opName = OperatorVTableName(op.OperatorToken.Kind, op.ReturnType);
        var fullOpName = $"{udt.Name.Lexeme}.{opName}";
        _procedureAddresses[fullOpName] = _chunk.Code.Count;

        BeginScope();
        AddLocal(new Token(TokenKind.Identifier, "SELF", null, SourcePosition.Unknown));
        foreach (var param in op.Parameters) AddLocal(param.Name, param.DataType);
        _isReachable = true;
        PreScanWholeBodyNativeFusions(op.Body);
        foreach (var stmt in op.Body) VisitStatement(stmt);
        EmitByte((byte)OpCode.OP_RETURN);
        EndScope();
    }

    /// <summary>
    /// Compila un distruttore (SUB DESTRUCTOR) di un TYPE.
    /// </summary>
    private void VisitUdtDestructor(DestructorDefinition dtor, UserDefinedType udt)
    {
        var fullDtorName = $"{udt.Name.Lexeme}.__delete";
        _procedureAddresses[fullDtorName] = _chunk.Code.Count;

        _isReachable = true;
        PreScanWholeBodyNativeFusions(dtor.Body);
        foreach (var stmt in dtor.Body) VisitStatement(stmt);
        EmitByte((byte)OpCode.OP_RETURN);
    }

    // --- Compilazione di CLASS...END CLASS ---

    /// <summary>
    /// Compila una definizione di CLASSE.
    /// Emette il descrittore della classe e compila tutti i membri.
    /// ORA COMPILA CORRETTAMENTE LA V-TABLE EREDITANDO DALLA CLASSE BASE.
    /// </summary>
    private void VisitClassDefinition(ClassDefinition classDef)
    {
        // 1. Fase Preliminare: Calcola gli offset dei campi per questa classe.
        ushort currentOffset = 0;
        var fieldOffsetsInThisClass = new Dictionary<string, ushort>(StringComparer.OrdinalIgnoreCase);

        foreach (var member in classDef.Members.OfType<FieldMember>())
        {
            var fieldKey = member.Name.Lexeme;
            fieldOffsetsInThisClass[fieldKey] = currentOffset;
            currentOffset += GetDataTypeByteSize(member.Declaration.DataType!);
        }

        // Registra gli offset dei campi nel dizionario globale per AssignField/LoadVariable
        foreach (var member in classDef.Members.OfType<FieldMember>())
        {
            _fieldOffsets[$"{classDef.Name.Lexeme}.{member.Name.Lexeme}"] = (ushort)member.Declaration.SymbolId;
            if (member.Declaration.Flags.HasFlag(SymbolFlags.IsStatic))
            {
                _staticFieldSymbolIds.Add(member.Declaration.SymbolId);
            }
        }

        // Register empty descriptor early (needed for self-referencing NEW in constructors)
        var classDescriptor = new Dictionary<string, object?>(StringComparer.OrdinalIgnoreCase);
        classDescriptor["Name"] = classDef.Name.Lexeme;
        classDescriptor["BaseClass"] = classDef.ResolvedBaseClass?.Name.Lexeme;
        classDescriptor["FieldOffsets"] = fieldOffsetsInThisClass;
        // Mappa SymbolId → nome/tipo dei campi. Si costruisce a mano e NON con
        // .ToDictionary(symbolId, ...): una CLASSE BUILTIN (es. "KEY", StdLib) espone membri
        // con SymbolId -1 (non assegnato) e il dizionario esplodeva con
        // "An item with the same key has already been added. Key: -1" appena un programma
        // usava `KEY.MAKE(...)` (bug console/make.bas, gamba PCode). I membri senza SymbolId
        // valido non servono a nessun offset (la classe builtin non ha campi d'istanza).
        var fieldNamesById = new Dictionary<int, string>();
        var fieldTypesById = new Dictionary<int, DataType>();
        foreach (var fm in classDef.Members.OfType<FieldMember>())
        {
            var sidField = fm.Declaration.SymbolId;
            if (sidField < 0) continue;
            fieldNamesById[sidField] = fm.Name.Lexeme;
            if (fm.Declaration.DataType is not null) fieldTypesById[sidField] = fm.Declaration.DataType;
        }
        classDescriptor["FieldNames"] = fieldNamesById;
        // Vedi commento in VisitUserDefinedType: stesso scopo, default tipizzato per campo.
        classDescriptor["FieldTypes"] = fieldTypesById;
        // Dimensioni degli array-campo (SymbolId -> double[] con coppie [lower0,upper0,...]):
        // il runtime le usa per ALLOCARE l'array per-istanza all'instanziazione
        // (OP_NEW_CLASS_INSTANCE). Senza, un `PUBLIC DIM arr(3)` restava un default scalare
        // (0.0/"") e `SELF.arr(i)`/`arr(i)` falliva ("indexing a numeric scalar"). Solo campi
        // con dimensioni costanti note (il caso idiomatico `DIM arr(N) AS ...`).
        {
            var fieldArrDims = new Dictionary<int, double[]>();
            foreach (var fm in classDef.Members.OfType<FieldMember>())
            {
                var dims = fm.Declaration.Dimensions;
                if (dims.IsEmpty) continue;
                var pairs = new double[dims.Length * 2];
                var ok = true;
                for (var d = 0; d < dims.Length; d++)
                {
                    if (dims[d] is not NumberExpr up) { ok = false; break; }
                    double lo = 0;
                    if (d < fm.Declaration.LowerBounds.Length)
                    {
                        if (fm.Declaration.LowerBounds[d] is NumberExpr le) lo = le.Value;
                        else { ok = false; break; }
                    }
                    pairs[d * 2] = lo;
                    pairs[d * 2 + 1] = up.Value;
                }
                if (ok) fieldArrDims[fm.Declaration.SymbolId] = pairs;
            }
            classDescriptor["FieldArrayDimensions"] = fieldArrDims;
            // Descriptor dell'ELEMENTO per gli array-campo di UDT (SymbolId -> descriptor
            // {FieldNames,FieldTypes}): il VM-C ne crea un'istanza per elemento (con __class__)
            // così `arr(i).campo` risolve per nome/offset come per un campo UDT diretto.
            // (PCode usa il tipo DataType in-process e non ne ha bisogno.)
            var fieldArrElemDesc = new Dictionary<int, object?>();
            foreach (var fm in classDef.Members.OfType<FieldMember>())
            {
                // Array-campo UDT (dims fisse) OPPURE campo LIST OF <ElemUDT>: in entrambi i casi
                // l'elemento è un UDT e va dotato del descriptor (__class__/FieldNames) così
                // `arr(i).campo` / `lst(i).campo` risolvono per nome/offset. Per LIST, gli elementi
                // arrivano dal JSON come dict SENZA __class__ → serve agganciarlo (JSON.TOOBJECT).
                UserDefinedType? udtElem = null;
                if (!fm.Declaration.Dimensions.IsEmpty)
                {
                    if (fm.Declaration.DataType is ArrayType faAt && faAt.ElementType is UserDefinedType ue1) udtElem = ue1;
                }
                else if (fm.Declaration.DataType is ListType ltFld && ltFld.ElementType is UserDefinedType ue2)
                {
                    udtElem = ue2;
                }
                if (udtElem != null)
                {
                    fieldArrElemDesc[fm.Declaration.SymbolId] = new Dictionary<string, object?>
                    {
                        ["FieldNames"] = udtElem.Fields.ToDictionary(f => (object)f.SymbolId, f => (object?)f.Name.Lexeme),
                        ["FieldTypes"] = udtElem.Fields.ToDictionary(f => (object)f.SymbolId, f => (object?)f.DataType),
                    };
                }
            }
            classDescriptor["FieldArrayElemDesc"] = fieldArrElemDesc;
        }
        // Una CLASS è un oggetto su heap (riferimento): NON ha layout a byte statico, quindi
        // SIZEOF vale -1 su tutti e cinque i backend (stesso valore che dà IL/C99 risolvendo
        // il tipo statico). Vedi la chiave "Size" in VisitUserDefinedType.
        classDescriptor["Size"] = (double)Basic.TypeLayout.NoStaticSize;

        classDescriptor["Constructors"] = classDef.Members.OfType<ConstructorMember>().Select(_ => $"{classDef.Name.Lexeme}#{classDef.SymbolId}.__new").ToList();
        classDescriptor["Destructor"] = classDef.Members.OfType<DestructorMember>().Any() ? $"{classDef.Name.Lexeme}.__delete" : null;
        classDescriptor["VTable"] = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
        var vtable = (Dictionary<string, string>)classDescriptor["VTable"]!;
        foreach (var m in classDef.Members.OfType<MethodMember>())
            vtable[m.Name.Lexeme] = m.Name.Lexeme;
        _compiledClassDescriptors[classDef.Name.Lexeme + "#" + classDef.SymbolId] = classDescriptor;

        // 2. Compila tutti i metodi, costruttori e distruttori.
        var clsPropGetters = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
        var clsPropSetters = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
        
        // Force-reset scope to prevent depth leaks from previous classes
        _locals.Clear();
        _scopeDepth = 0;
        
        BeginScope();
        AddLocal(new Token(TokenKind.Identifier, "SELF", null,
            SourcePosition.Unknown)); // SELF è il primo locale (indice 0).

        foreach (var member in classDef.Members)
        {
            switch (member)
            {
                case MethodMember mm: VisitMethodMember(mm, classDef); break;
                case ConstructorMember cm: VisitConstructorMember(cm, classDef); break;
                case DestructorMember dm: VisitDestructorMember(dm, classDef); break;
                case PropertyMember pm:
                    VisitPropertyMember(pm, classDef);
                    if (pm.Property.Getter is not null)
                        clsPropGetters[pm.Name.Lexeme] = $"{classDef.Name.Lexeme}.Get_{pm.Name.Lexeme}";
                    if (pm.Property.Setter is not null)
                        clsPropSetters[pm.Name.Lexeme] = $"{classDef.Name.Lexeme}.Set_{pm.Name.Lexeme}";
                    break;
                case OperatorMember om: VisitOperatorMember(om, classDef); break;
            }
        }

        EndScope(); // Chiude lo scope dei membri della classe.

        // --- POPOLAZIONE DESCRITTORE DI CLASSE (completa V-Table) ---
        // classDescriptor was already created and registered before member compilation

        // --- COMPILAZIONE COMPLETA DELLA V-TABLE ---
        var vTable = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);

        // 1. Eredita i metodi virtuali dalla classe base.
        if (classDef.ResolvedBaseClass is not null)
        {
            // Accede correttamente al descrittore della classe base dalla mappa globale.
            if (_compiledClassDescriptors.TryGetValue(classDef.ResolvedBaseClass.Name.Lexeme + "#" + classDef.ResolvedBaseClass.SymbolId,
                    out var baseClassCompiledDescriptor))
            {
                if (baseClassCompiledDescriptor.TryGetValue("VTable", out var baseVTableObj) &&
                    baseVTableObj is Dictionary<string, string> baseVTable)
                {
                    foreach (var entry in baseVTable)
                        vTable[entry.Key] = entry.Value;
                }
                // Inherit PropertyGetters and PropertySetters from base class
                if (baseClassCompiledDescriptor.TryGetValue("PropertyGetters", out var basePg) &&
                    basePg is Dictionary<string, string> basePgDict)
                {
                    foreach (var e in basePgDict)
                        if (!clsPropGetters.ContainsKey(e.Key)) clsPropGetters[e.Key] = e.Value;
                }
                if (baseClassCompiledDescriptor.TryGetValue("PropertySetters", out var basePs) &&
                    basePs is Dictionary<string, string> basePsDict)
                {
                    foreach (var e in basePsDict)
                        if (!clsPropSetters.ContainsKey(e.Key)) clsPropSetters[e.Key] = e.Value;
                }
                // Eredita dal base i dizionari dei campi, chiavati per SymbolId (gli SymbolId dei
                // campi base sono DISTINTI da quelli della derivata -> unione, con precedenza alla
                // derivata in caso di shadowing). Oltre a FieldNames servono anche FieldTypes e
                // FieldArrayDimensions: senza, i campi-array EREDITATI (es. gli array-campo di una
                // classe base grande come DataGrid) non venivano allocati/riconosciuti ->
                // "indexing a numeric scalar" (VM-C) / campi vuoti (PCode) su una SOTTOCLASSE.
                // (Bug: l'ereditarietà copriva solo FieldNames, aggiunta prima di FieldTypes/
                // FieldArrayDimensions.)
                foreach (var fieldDictKey in new[] { "FieldNames", "FieldTypes", "FieldArrayDimensions", "FieldArrayElemDesc", "FieldOffsets" })
                {
                    if (!baseClassCompiledDescriptor.TryGetValue(fieldDictKey, out var baseFieldVal)
                        || baseFieldVal is not System.Collections.IDictionary baseFieldDict) continue;
                    if (classDescriptor[fieldDictKey] is System.Collections.IDictionary curFieldDict)
                    {
                        foreach (var k in baseFieldDict.Keys)
                            if (!curFieldDict.Contains(k)) curFieldDict[k] = baseFieldDict[k];
                    }
                    else
                    {
                        classDescriptor[fieldDictKey] = baseFieldDict;
                    }
                }
            }
        }

        // 2. Sovrascrive o aggiunge i metodi virtuali/override/nuovi virtuali di questa classe.
        foreach (var member in classDef.Members.OfType<MethodMember>())
            vTable[member.Name.Lexeme] = $"{classDef.Name.Lexeme}.{member.Name.Lexeme}";
        foreach (var member in classDef.Members.OfType<OperatorMember>())
        {
            var vOpName = OperatorVTableName(member.Operator.OperatorToken.Kind, member.Operator.ReturnType);
            vTable[vOpName] = $"{classDef.Name.Lexeme}.{vOpName}";
        }

        classDescriptor["VTable"] = vTable;
        if (clsPropGetters.Count > 0) classDescriptor["PropertyGetters"] = clsPropGetters;
        if (clsPropSetters.Count > 0) classDescriptor["PropertySetters"] = clsPropSetters;
        if (clsPropGetters.Count > 0) classDescriptor["PropertyGetters"] = clsPropGetters;
        if (clsPropSetters.Count > 0) classDescriptor["PropertySetters"] = clsPropSetters;

        // Aggiunge il descrittore completo alla constant pool.
        var finalClassDescriptorConstIndex = AddConstant(classDescriptor);

        // Registra per patching post-compilazione (VTable indirizzi)
        _classDescriptorsToPatch.Add((finalClassDescriptorConstIndex, classDef));

        _compiledClassDescriptors[classDef.Name.Lexeme + "#" + classDef.SymbolId] = classDescriptor;
    }

    /// <summary>
    /// Compila un metodo definito all'interno di una classe.
    /// Gestisce l'aggiunta di 'SELF' come primo parametro implicito e la compilazione del corpo.
    /// </summary>
    private void VisitMethodMember(MethodMember member, ClassDefinition classDef)
    {
        // Convenzione per i nomi dei metodi: NomeClasse.NomeMetodo
        var fullMethodName = $"{classDef.Name.Lexeme}.{member.Name.Lexeme}";
        _procedureAddresses[fullMethodName] = _chunk.Code.Count;
        var originalLocalsCount = _locals.Count;
        _isReachable = true; // Salva lo stato dei locali prima dei parametri reali.

        var previousMethodClassType = _currentMethodClassType;
        _currentMethodClassType = new ClassType(classDef);

        // Z1 (dispatch/return polimorfico): imposta il contesto di funzione per un METODO
        // FUNCTION, come fa il loop top-level di CompileProcedures per le procedure standalone.
        // Senza, `END FUNCTION` dentro un metodo di classe compilava via VisitEndProcedureStatement
        // con _currentFunctionReturnType/_currentProcedureName null → emetteva OP_PUSH_NIL prima
        // di OP_RETURN invece di OP_LOAD_LOCAL(return-slot) → ogni FUNCTION method restituiva
        // sempre NIL (PRINT r.GetStatusCode() stampava vuoto, non 200). _currentProcedureName
        // serve anche a ResolveLocal per trovare il return-slot: la bare-assignment
        // `GetX = ...` crea il local implicito col nome del metodo dopo SELF+parametri.
        var previousFunctionReturnType = _currentFunctionReturnType;
        var previousProcedureName = _currentProcedureName;
        _currentFunctionReturnType = (member.Procedure as FunctionDefinition)?.ReturnType;
        _currentProcedureName = member.Name.Lexeme;

        // Aggiunge i parametri della procedura come locali, dopo 'SELF'.
        foreach (var param in member.Procedure.Parameters)
        {
            AddLocal(param.Name, param.DataType);
        }

        // Fusioni native "tutto il ciclo in un opcode" (vedi Compiler.NativeLoopFusion.cs).
        PreScanWholeBodyNativeFusions(member.Procedure.Body);

        foreach (var stmt in member.Procedure.Body)
        {
            // Le dichiarazioni GLOBAL sono già state processate.
            // Le dichiarazioni STATIC/LOCAL creano variabili nello scope corrente.
            // Le dichiarazioni CONST vengono gestite dal linker.
            // ConstStmt NON è più skippato (fix %NOME, twin-port GMBasicAI).
            if (stmt is DataStmt or DeclarationStmt { Keyword.Kind: TokenKind.Global }) continue;
            VisitStatement(stmt);
        }

        // Dopo il corpo del metodo, pulisce i locali specifici del metodo.
        while (_locals.Count > originalLocalsCount && _locals[^1].Name.Lexeme != "SELF")
        {
            _locals.RemoveAt(_locals.Count - 1);  // No OP_POP: frame locals, not global stack
        }

        _currentMethodClassType = previousMethodClassType;

        // Ripristina il contesto di funzione della procedura precedente.
        _currentFunctionReturnType = previousFunctionReturnType;
        _currentProcedureName = previousProcedureName;

        // Ogni metodo termina con un RETURN implicito.
        EmitByte((byte)OpCode.OP_RETURN);
    }

    /// <summary>
    /// Compila un costruttore (SUB NEW) di una classe.
    /// </summary>
    private void VisitConstructorMember(ConstructorMember member, ClassDefinition classDef)
    {
        // Convenzione per i costruttori: NomeClasse#SymbolId.__new
        var fullCtorName = $"{classDef.Name.Lexeme}#{classDef.SymbolId}.__new";
        _procedureAddresses[fullCtorName] = _chunk.Code.Count;
        var originalLocalsCount = _locals.Count;
        _isReachable = true;

        // I parametri del costruttore vengono aggiunti dopo 'SELF'.
        foreach (var param in member.Constructor.Parameters)
        {
            AddLocal(param.Name, param.DataType);
        }

        var previousIsCompilingConstructor = _isCompilingConstructor;
        _isCompilingConstructor = true;
        // Come VisitMethodMember: imposta il tipo CLASSE corrente così che
        // CompilerInferExpressionType(SELF) lo risolva (AssignField applica ADJUST_STRING
        // ai campi STRING*N anche dentro il costruttore — bug ClassesAbstract).
        var previousCtorMethodClassType = _currentMethodClassType;
        _currentMethodClassType = new ClassType(classDef);
        var fieldInitsEmitted = false;
        // Fusioni native "tutto il ciclo in un opcode" (vedi Compiler.NativeLoopFusion.cs).
        PreScanWholeBodyNativeFusions(member.Constructor.Body);
        foreach (var stmt in member.Constructor.Body)
        {
            // Gestione speciale per BASE.NEW()
            if (stmt is BaseConstructorCallStmt baseCall)
            {
                // Compila gli argomenti per BASE.NEW().
                foreach (var arg in baseCall.Args) VisitExpression(arg);

                // Spinge SELF sulla stack come istanza implicita (per il costruttore base).
                var selfIndex =
                    ResolveLocal(new Token(TokenKind.Identifier, "SELF", null, SourcePosition.Unknown).Lexeme);
                if (selfIndex == -1) throw new Exception("Compiler Error: SELF not found for BASE.NEW call.");
                EmitByte((byte)OpCode.OP_LOAD_LOCAL); EmitShort((ushort)selfIndex);

                EmitConstant((double)(baseCall.Args.Length + 1)); // Numero di argomenti reali + SELF

                var callLocation = EmitJump((byte)OpCode.OP_CALL_CTOR);
                var baseCtorName = $"{classDef.ResolvedBaseClass!.Name.Lexeme}#{classDef.ResolvedBaseClass!.SymbolId}.__new";
                _jumpsToPatch.Add((callLocation,
                    new Token(TokenKind.Identifier, baseCtorName, null, SourcePosition.Unknown)));
                // BASE.NEW è uno STATEMENT: il valore di ritorno del costruttore base (spinto da
                // OP_RETURN) va scartato. Senza, resta sullo stack della procedura derivata e
                // OP_RETURN lo usa come proprio valore di ritorno: `DIM b AS B = NEW B("x")`
                // assegnava NIL (ogni campo ereditato poi letto come vuoto — fe_inherit/PCode).
                EmitByte((byte)OpCode.OP_POP);

                // Gli inizializzatori inline dei campi DI QUESTA classe vanno emessi subito dopo
                // BASE.NEW() (che ha appena inizializzato i campi ereditati) e prima del resto del
                // corpo del costruttore, cosi' che eventuali assegnazioni esplicite nel corpo
                // possano sovrascriverli (stessa semantica di un normale linguaggio OOP).
                EmitFieldInitializers(classDef);
                fieldInitsEmitted = true;
            }
            else
            {
                if (!fieldInitsEmitted)
                {
                    EmitFieldInitializers(classDef);
                    fieldInitsEmitted = true;
                }
                VisitStatement(stmt);
            }
        }
        if (!fieldInitsEmitted) EmitFieldInitializers(classDef);
        _isCompilingConstructor = previousIsCompilingConstructor;
        _currentMethodClassType = previousCtorMethodClassType;

        // Pulizia locali (rimuove solo da _locals, non emette POP perche'
        // i costruttori usano frameLocals, non lo stack globale)
        while (_locals.Count > originalLocalsCount && _locals[^1].Name.Lexeme != "SELF")
        {
            _locals.RemoveAt(_locals.Count - 1);
        }

        // Ogni costruttore termina con un RETURN. Lo stack e' pulito (nessun OP_PUSH_NIL
        // emesso da VisitEndProcedureStatement per l'EndProcedureStmt finale del corpo,
        // vedi _isCompilingConstructor), quindi PCodeProcessor.OP_RETURN ricade
        // correttamente sul fallback ConstructorInstance invece di restituire NIL.
        EmitByte((byte)OpCode.OP_RETURN);
    }

    /// <summary>
    /// Emette il P-Code per applicare gli inizializzatori inline dei campi di una CLASS
    /// (es. <c>PUBLIC DIM Count AS LONG = 42</c>) prima che il resto del corpo del costruttore
    /// venga eseguito. Prima di questo fix nessun campo con inizializzatore inline riceveva mai
    /// il proprio valore di default: il compilatore compilava SOLO le assegnazioni esplicite nel
    /// corpo del costruttore, ignorando silenziosamente `Declaration.InitialValue`.
    /// I campi STATIC sono esclusi qui: condividono un'unica istanza tra tutti gli oggetti della
    /// classe, quindi vanno inizializzati una sola volta (gestito altrove), non ad ogni NEW.
    /// </summary>
    private void EmitFieldInitializers(ClassDefinition classDef)
    {
        var selfIndex = ResolveLocal(new Token(TokenKind.Identifier, "SELF", null, SourcePosition.Unknown).Lexeme);
        if (selfIndex == -1) throw new Exception("Compiler Error: SELF not found for field initializer emission.");

        foreach (var fieldMember in classDef.Members.OfType<FieldMember>())
        {
            var decl = fieldMember.Declaration;
            if (decl.InitialValue is null) continue;
            if (decl.Flags.HasFlag(SymbolFlags.IsStatic)) continue;

            VisitExpression(decl.InitialValue);
            EmitByte((byte)OpCode.OP_LOAD_LOCAL); EmitShort((ushort)selfIndex);
            EmitByte((byte)OpCode.OP_STORE_MEMBER);
            EmitShort((ushort)decl.SymbolId);
        }
    }

    /// <summary>
    /// Compila un distruttore (SUB DESTRUCTOR) di una classe.
    /// </summary>
    private void VisitDestructorMember(DestructorMember member, ClassDefinition classDef)
    {
        // Convenzione per i distruttori: NomeClasse.__delete
        var fullDtorName = $"{classDef.Name.Lexeme}.__delete";
        _procedureAddresses[fullDtorName] = _chunk.Code.Count;
        var originalLocalsCount = _locals.Count;
        _isReachable = true;

        // Fusioni native "tutto il ciclo in un opcode" (vedi Compiler.NativeLoopFusion.cs).
        PreScanWholeBodyNativeFusions(member.Destructor.Body);

        foreach (var stmt in member.Destructor.Body)
        {
            // ConstStmt NON è più skippato (fix %NOME, twin-port GMBasicAI).
            if (stmt is DataStmt or DeclarationStmt { Keyword.Kind: TokenKind.Global }) continue;
            VisitStatement(stmt);
        }

        // Pulizia locali
        while (_locals.Count > originalLocalsCount && _locals[^1].Name.Lexeme != "SELF")
        {
            _locals.RemoveAt(_locals.Count - 1);  // No OP_POP: frame locals, not global stack
        }

        // Ogni distruttore termina con un RETURN.
        EmitByte((byte)OpCode.OP_RETURN);
    }

    /// <summary>
    /// Compila un'istruzione PROPERTY...END PROPERTY.
    /// I getter e i setter vengono compilati come metodi separati con nomi convenzionali.
    /// </summary>
    private void VisitPropertyMember(PropertyMember member, ClassDefinition classDef)
    {
        // Compilazione del getter
        if (member.Property.Getter is not null)
        {
            var getterName = $"{classDef.Name.Lexeme}.Get_{member.Name.Lexeme}";
            _procedureAddresses[getterName] = _chunk.Code.Count;
            // Non chiamare BeginScope: usa lo SELF della classe (slot 0) come ConstructorMember
            var origLocals = _locals.Count;
            _isReachable = true;
            PreScanWholeBodyNativeFusions(member.Property.Getter.Body);
            foreach (var stmt in member.Property.Getter.Body) VisitStatement(stmt);
            while (_locals.Count > origLocals) { EmitByte((byte)OpCode.OP_POP); _locals.RemoveAt(_locals.Count - 1); }
            EmitByte((byte)OpCode.OP_RETURN);
        }

        // Compilazione del setter
        if (member.Property.Setter is not null)
        {
            var setterName = $"{classDef.Name.Lexeme}.Set_{member.Name.Lexeme}";
            _procedureAddresses[setterName] = _chunk.Code.Count;
            // Non chiamare BeginScope: usa SELF classe (slot 0), VALUE (slot 1)
            var origLocals = _locals.Count;
            AddLocal(member.Property.Setter.ValueParameter.Name); // Slot successivo a SELF
            _isReachable = true;
            PreScanWholeBodyNativeFusions(member.Property.Setter.Body);
            foreach (var stmt in member.Property.Setter.Body) VisitStatement(stmt);
            // Non emettere POP: i locali sono nei frameLocals
            while (_locals.Count > origLocals) { _locals.RemoveAt(_locals.Count - 1); }
            EmitByte((byte)OpCode.OP_RETURN);
        }
    }

    /// <summary>
    /// Compila un operatore sovraccaricato.
    /// </summary>
    private void VisitOperatorMember(OperatorMember member, ClassDefinition classDef)
    {
        // Nomi convenzionali per gli operatori (es. __ADD__, __EQUAL__).
        var opName = OperatorVTableName(member.Operator.OperatorToken.Kind, member.Operator.ReturnType);
        var fullOpName = $"{classDef.Name.Lexeme}.{opName}";
        _procedureAddresses[fullOpName] = _chunk.Code.Count;

        BeginScope();
        AddLocal(new Token(TokenKind.Identifier, "SELF", null, SourcePosition.Unknown));
        foreach (var param in member.Operator.Parameters) AddLocal(param.Name, param.DataType);
        _isReachable = true;
        PreScanWholeBodyNativeFusions(member.Operator.Body);
        foreach (var stmt in member.Operator.Body) VisitStatement(stmt);
        EmitByte((byte)OpCode.OP_RETURN);
        EndScope();
    }

    // --- Metodi Helper ---

    /// <summary>
    /// Calcola la dimensione in byte di un DataType.
    /// Necessaria per determinare gli offset dei campi UDT/Class.
    /// </summary>
    private ushort GetDataTypeByteSize(DataType dataType)
    {
        // L'Executor gestisce direttamente le dimensioni dei tipi complessi.
        // Qui ci basta una stima o un default per i tipi noti.
        return dataType switch
        {
            StringType { FixedLength: { } len } => (ushort)len,
            IntegerType or UIntegerType => 2,
            LongType or ULongType or SingleType or DateType or TimeType or DateTimeType or TimeSpanType => 4,
            DoubleType or CurrencyType or QuadType or UQuadType => 8,
            BitType or ByteType or SByteType => 1,
            _ => 16 // Default arbitrario per tipi complessi (handle o riferimento)
        };
    }
}