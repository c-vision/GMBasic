// FILE: Interpreter.Compiler.Compiler.Variables.cs



namespace Compiler;

public sealed partial class Compiler
{
    /// <summary>
    /// Rappresenta le informazioni su una variabile locale o un parametro,
    /// utilizzate durante la compilazione per la gestione degli scope.
    /// </summary>
    private sealed class Local
    {
        /// <summary>
        /// Il token che rappresenta il nome della variabile locale o del parametro.
        /// </summary>
        public required Token Name { get; set; }

        /// <summary>
        /// La profondità dello scope in cui la variabile è stata definita (0 per lo scope più esterno).
        /// Utilizzata per determinare la visibilità e per la pulizia dello stack alla fine di uno scope.
        /// Set: il contatore FOR senza DIM esterna viene "promosso" a uno scope esterno a fine
        /// ciclo (Compiler.Loops), così sopravvive al NEXT come su IL (canone 2026-09-30).
        /// </summary>
        public required int Depth { get; set; }

        /// <summary>
        /// Il tipo di dato della variabile locale, essenziale per la type inference.
        /// </summary>
        public required DataType Type { get; init; }

        /// <summary>
        /// True se questo local è già stato finalizzato (DESTRUCTOR) esplicitamente PRIMA
        /// dell'uscita naturale dallo scope — es. la risorsa di uno `USING var = ... END USING`
        /// (BUG-8), finalizzata a END USING. Evita la doppia finalizzazione quando lo stesso
        /// local viene ANCHE raggiunto da <see cref="EndScope"/> (blocco annidato) o da
        /// <see cref="EmitFinalizeBodyLocals"/> (fine SUB/FUNCTION, BUG-6).
        /// </summary>
        public bool AlreadyFinalized { get; set; }

        /// <summary>
        /// True per una variabile IMPLICITA (prima assegnazione senza DIM) creata dentro un blocco annidato (IF/FOR/WHILE/...) e
        /// "promossa" subito allo scope della procedura: sopravvive all'uscita dal blocco come in IL/BASIC (<see cref="EndScope"/> la
        /// salta e continua a chiudere le locali di blocco che restano sotto di lei).
        /// </summary>
        public bool Promoted { get; set; }

        /// <summary>Default dello slot: true = 0 numerico, false = null (stringa/riferimento).
        /// null = deducilo dal tipo. Serve alle variabili IMPLICITE, dove il tipo è dedotto dal
        /// nome SENZA suffisso: `S$ = "x"` sarebbe Double e la VM inizializzerebbe 0 invece di
        /// null (parità IL).</summary>
        public bool? NumericDefault { get; set; }
    }

    /// <summary>
    /// Rappresenta le informazioni su una variabile globale.
    /// </summary>
    private sealed class GlobalInfo
    {
        /// <summary>
        /// Il token che rappresenta il nome della variabile globale o del parametro.
        /// </summary>
        public required string Name { get; init; }

        /// <summary>
        /// Il tipo di dato della variabile globale, essenziale per la type inference.
        /// </summary>
        public required DataType Type { get; init; }
    }

    /// <summary>
    /// Normalizza il nome di un identificatore rimuovendo i suffissi di tipo legacy ($, %, #, !, &amp;, @).
    /// </summary>
    /// <param name="name">Il lexeme del token da normalizzare.</param>
    /// <returns>Il nome normalizzato.</returns>
    private static string NormalizeIdentifierName(string name) =>
        CompiledChunk.NormalizeLocalName(name);

    /// <summary>
    /// Emette il P-Code per caricare il valore di una variabile, che DEVE esistere.
    /// Se non la trova, lancia un errore, perché il caricamento non può creare variabili.
    /// </summary>
    /// <summary>
    /// Un nome con kind di KEYWORD (FORM/QUERY) puo' essere una variabile REALE dichiarata con DIM:
    /// in quel caso vince il simbolo dichiarato, e il ramo "dynamic scope" (convenzione BSP,
    /// vedi VisitExpression) non deve|leaderlo — altrimenti `DIM Query = 9 : PRINT Query`
    /// restituiva silenziosamente il vuoto della keyword invece di 9 (bug: solo P-Code; IL/VM/C
    /// risolvono il simbolo e sono corretti).
    /// Criterio: esiste uno slot REALE (upvalue, locale o globale gia' registrato). Non si usa
    /// <see cref="TryResolveOrRegisterGlobal"/> perche' REGISTRA il nome se assente (effetto
    /// collaterale: renderebbe il predicato sempre vero), e non si consulta
    /// <c>ExternalGlobalProbe</c> perche' quei nomi sono proprio quelli del dynamic scope.
    /// </summary>
    private bool IsRealVariableReference(string lexeme)
    {
        var normalizedName = NormalizeIdentifierName(lexeme);
        return ResolveUpvalueByName(normalizedName) != -1
            || ResolveLocal(normalizedName) != -1
            || GlobalSlots.ContainsKey(normalizedName);
    }

    private void LoadVariable(Target target)
    {
        // --- LOGGING DI DEBUG ---
        //LogDebug($"[LoadVariable] Attempting to load target: {target}");
        // --- FINE LOGGING ---
        if (target.SourceLineIndex >= 0) _currentSourceLineIndex = target.SourceLineIndex;
        switch (target)
        {
            case VariableTarget vt:
                var normalizedName = NormalizeIdentifierName(vt.NameCleaned.Lexeme);
                // SCOPE DINAMICO (DYNAMIC): la lettura va risolta sullo scope dinamico per NOME
                // (catena di chiamate) — vedi Compiler.Scope.Dynamic.cs. Va PRIMA della
                // risoluzione locale/globale: una lettura a cascata in una procedura che non
                // dichiara il nome risolverebbe altrimenti a un globale implicito (registrato
                // on-the-fly) e leggerebbe sempre il default. Non intercetta se un
                // locale/parametro/upvalue/globale genuino omonimo ombreggia il nome.
                if (ShouldUseDynamicScope(normalizedName))
                {
                    EmitDynamicLoad(vt.NameCleaned.Lexeme);
                    return;
                }
                var upvalueIndex = ResolveUpvalueByName(normalizedName);
                if (upvalueIndex != -1)
                {
                    EmitBytes((byte)OpCode.OP_LOAD_UPVALUE, (byte)upvalueIndex);
                    return;
                }

                var localIndex = ResolveLocal(normalizedName); if (localIndex != -1)
                {
                    EmitByte((byte)OpCode.OP_LOAD_LOCAL); EmitShort((ushort)localIndex);
                }
                else if (_staticVars.TryGetValue(normalizedName, out var staticSymbolId))
                {
                    // STATIC della procedura corrente: lo storage persistente è l'area statica
                    // del runtime (stessa degli OP_*_STATIC_FIELD dei campi di classe).
                    EmitByte((byte)OpCode.OP_LOAD_STATIC_FIELD);
                    EmitShort((ushort)staticSymbolId);
                }
                else if (TryResolveOrRegisterGlobal(normalizedName, out var globalIndex))
                {
                    EmitByte((byte)OpCode.OP_LOAD_GLOBAL);
                    EmitShort(globalIndex);
                }
                else
                {
                    // Se siamo in un contesto di classe (SELF è locale[0]), prova ad accedere
                    // alla variabile come campo dell'istanza (es. closure context).
                    if (_locals.Count > 0 && _locals[0].Name.Lexeme.Equals("SELF", StringComparison.OrdinalIgnoreCase))
                    {
                        // Se il nome della variabile è __context o __ClosureCtx_*,
                        // è un alias per SELF (locale[0]) — usa direttamente l'istanza.
                        if (normalizedName.StartsWith("__context") || normalizedName.StartsWith("__ClosureCtx"))
                        {
                            EmitByte((byte)OpCode.OP_LOAD_LOCAL); EmitShort((ushort)0);
                        }
                        else
                        {
                            // Prova a caricare il campo dal'istanza SELF.
                            EmitByte((byte)OpCode.OP_LOAD_LOCAL); EmitShort((ushort)0);
                            var fnIndex = AddConstant(normalizedName);
                            EmitByte((byte)OpCode.OP_LOAD_FIELD);
                            EmitShort(fnIndex);
                        }
                    }
                    else
                    {
                        // Variabile nuda INDEFINITA → 0 numerico (canone IL/C99/C99Native: il
                        // default di una variabile mai assegnata è 0, NON nil). Prima pushava NIL
                        // → `PRINT ANALYSIS.FFTC` (var nuda "FFTC") stampava vuoto mentre il/C99/
                        // C99Native stampavano 0 (AnalysisTests FFTC/FFTINV/FFTINVC).
                        EmitConstant(0.0);
                    }
                }

                break;

            case ArrayElementTarget aet:
                if (aet.Indices.IsEmpty)
                {
                    LoadVariable(new VariableTarget(-1, aet.NameSuffixed, aet.NameCleaned));
                }
                else
                {
                    LoadVariable(new VariableTarget(-1, aet.NameSuffixed, aet.NameCleaned));
                    foreach (var index in aet.Indices)
                    {
                        VisitExpression(index);
                    }

                    EmitBytes((byte)OpCode.OP_LOAD_ELEMENT, (byte)aet.Indices.Length);
                }

                break;

            case FieldTarget ft:
                LoadVariable(ft.Structure);
                var fieldNameIndex = AddConstant(ft.OriginalToken.Lexeme);
                EmitByte((byte)OpCode.OP_LOAD_FIELD);
                EmitShort(fieldNameIndex);
                break;

            case IndexedTarget it:
                // Compila la base dell'espressione (la collezione) e gli indici.
                VisitExpression(it.Base);
                foreach (var index in it.Indices)
                {
                    VisitExpression(index);
                }

                // Emette l'istruzione per caricare l'elemento.
                EmitBytes((byte)OpCode.OP_LOAD_ELEMENT, (byte)it.Indices.Length);
                break;

            default:
                throw new NotImplementedException(
                    $"Loading a target of type '{target.GetType().Name}' not implemented.");
        }
    }

    /// <summary>
    /// Cerca o definisce una variabile globale.
    /// </summary>
    private int ResolveGlobal(string name)
    {
        var normalizedName = NormalizeIdentifierName(name);
        if (_globals.TryGetValue(normalizedName, out var index))
        {
            return index;
        }

        return DefineGlobal(normalizedName);
    }

    /// <summary>
    /// Cerca una variabile locale. Se non la trova, risale la catena dei compilatori
    /// per vedere se può essere catturata come upvalue.
    /// Questa versione corregge il bug di normalizzazione confrontando i nomi normalizzati.
    /// </summary>
    private int ResolveLocal(string name)
    {
        var normalizedName = NormalizeIdentifierName(name);
        
        // Cerca all'indietro per trovare la dichiarazione più recente (corretta gestione dello shadowing).
        for (var i = _locals.Count - 1; i >= 0; i--)
        {
            var localNormalizedName = NormalizeIdentifierName(_locals[i].Name.Lexeme);
            if (localNormalizedName.Equals(normalizedName, StringComparison.OrdinalIgnoreCase))
            {
                return i;
            }
        }

        return -1;
    }

    /// <summary>
    /// Cerca una variabile come upvalue, risalendo la catena dei compilatori padre.
    /// Se trovata, aggiunge un upvalue e ne restituisce l'indice.
    /// </summary>
    private int ResolveUpvalueByName(string name)
    {
        if (_enclosing == null) return -1;
        
        var normalizedName = NormalizeIdentifierName(name);

        // SHADOWING (2026-11-07): un locale/parametro del compilatore CORRENTE ha la
        // precedenza sull'upvalue omonimo. Senza questa guardia, i call-site che
        // consultano ResolveUpvalueByName PRIMA di ResolveLocal (LoadVariable,
        // StoreVariable... per motivi storici) facevano vincere la variabile esterna:
        // una lambda `FN(u, v) => {u, v, u*v}` con un `DIM v(...)` esterno leggeva
        // l'array esterno invece del parametro (era il ramo upvalue) e il callback FFI
        // falliva con InvalidCastException su PCodeVM. Il controllo è innocuo nella
        // ricorsione transitiva: il chiamante verifica già `_enclosing.ResolveLocal`
        // prima di ricorrere, quindi qui il nome non è un locale del genitore.
        if (ResolveLocal(normalizedName) != -1) return -1;
        
        // Cerca come locale nel genitore
        var localIndex = _enclosing.ResolveLocal(normalizedName);
        if (localIndex != -1) return AddUpvalue((byte)localIndex, true);
        
        // Cerca come upvalue nel genitore (closure annidata)
        var upvalueIndex = _enclosing.ResolveUpvalueByName(name);
        if (upvalueIndex != -1) return AddUpvalue((byte)upvalueIndex, false);
        
        return -1;
    }

    /// <summary>
    /// Aggiunge una upvalue alla lista di questo compilatore (se non già presente) e ne restituisce l'indice.
    /// </summary>
    private int AddUpvalue(byte index, bool isLocal)
    {
        // Kind 3 valori: 1 = locale del frame chiamante, 2 = upvalue del frame corrente
        // (cattura TRANSITIVA lambda-in-lambda). Il vecchio booleano schiacciava entrambe
        // su 0/1 e la cattura transitiva finiva nel caso "globale".
        var kind = (byte)(isLocal ? 1 : 2);
        for (var i = 0; i < _upvalues.Count; i++)
        {
            var upvalue = _upvalues[i];
            if (upvalue.Index == index && upvalue.Kind == kind)
            {
                return i;
            }
        }

        _upvalues.Add(new Upvalue { Index = index, IsLocal = isLocal, Kind = kind });
        return _upvalues.Count - 1;
    }

    /// <summary>
    /// Cerca una variabile negli scope esterni per determinarne l'indice come upvalue.
    /// Gestisce la cattura ricorsiva attraverso più livelli di annidamento.
    /// QUESTA VERSIONE È AGGIORNATA PER UTILIZZARE IL SymbolId.
    /// </summary>
    private int ResolveUpvalue(int symbolId)
    {
        if (_enclosing == null) return -1;
        
        /*// Cerca la variabile direttamente tramite ID nello scope esterno (genitore).
        var local = _enclosing.ResolveLocal(symbolId);
        if (local != -1)
        {
            // Se trovata localmente nel genitore, catturala e crea un upvalue per il genitore.
            return AddUpvalue((byte)local, true);
        }
        
        // Cerca la variabile come upvalue nello scope esterno (già catturata).
        var upvalue = _enclosing.ResolveUpvalue(symbolId);
        if (upvalue != -1) 
        {
            // Se trovata come upvalue nel genitore, crea un upvalue per lo scope corrente che punta a quello del genitore.
            return AddUpvalue((byte)upvalue, false);
        }*/

        return -1;
    }

    /// <summary>
    /// Aggiunge una variabile locale alla lista dello scope corrente, deducendone il tipo.
    /// </summary>
    private void AddLocal(Token name)
    {
        EnsureLocalSlotAvailable(name);
        var type = DeduceTypeFromSuffix(name.Lexeme);
        var local = new Local { Name = name, Depth = _scopeDepth, Type = type };
        _locals.Add(local);
        _maximumLocalCount = Math.Max(_maximumLocalCount, _locals.Count);
    }

    /// <summary>
    /// Aggiunge una variabile locale alla lista dello scope corrente con un tipo esplicito.
    /// </summary>
    /// <summary>
    /// Crea la locale di una variabile IMPLICITA. In BASIC vive per tutta la procedura: se la prima assegnazione cade in un blocco
    /// annidato (profondita' &gt; 1) la si crea subito a profondita' di procedura, cosi' `IF c THEN r = 2` poi `PRINT r` la vede.
    /// </summary>
    private void AddImplicitLocal(Token name, Token? suffixed = null)
    {
        AddLocal(name);
        if (suffixed is not null)
        {
            /* Il suffisso non fa parte del nome (S e S$ sono la stessa variabile) ma la PRIMA
             * scrittura ne fissa la famiglia: `$` = stringa (default null), altrimenti numerico. */
            var lex = suffixed.Lexeme ?? string.Empty;
            _locals[^1].NumericDefault = lex.Length == 0 || !lex.EndsWith('$');
        }
        if (_scopeDepth > 1)
        {
            var l = _locals[^1];
            l.Depth = 1;
            l.Promoted = true;
        }
    }

    private void AddLocal(Token name, DataType type)
    {
        EnsureLocalSlotAvailable(name);
        var local = new Local { Name = name, Depth = _scopeDepth, Type = type };
        _locals.Add(local);
        _maximumLocalCount = Math.Max(_maximumLocalCount, _locals.Count);
    }

    private void EnsureLocalSlotAvailable(Token name)
    {
        if (_locals.Count >= ushort.MaxValue)
            throw new InvalidOperationException(
                $"La procedura supera il limite P-Code di 65535 variabili locali aggiungendo '{name.Lexeme}'.");
    }

    /// <summary>
    /// Aggiunge una nuova variabile globale alla tabella dei simboli con il suo tipo,
    /// o restituisce l'indice se già esistente.
    /// </summary>
    private int DefineGlobal(string name, DataType type)
    {
        var normalizedName = NormalizeIdentifierName(name);
        if (_globals.TryGetValue(normalizedName, out var existingIndex))
        {
            return existingIndex;
        }
        
        var newIndex = _globalInfos.Count;
        _globals[normalizedName] = newIndex;
        _globalInfos.Add(new GlobalInfo { Name = normalizedName, Type = type });
        return newIndex;
    }

    /// <summary>
    /// Sovraccarico per la creazione implicita di globali, che deduce il tipo.
    /// </summary>
    private int DefineGlobal(string name)
    {
        return DefineGlobal(name, DeduceTypeFromSuffix(name));
    }

    /// <summary>
    /// Metodo helper per estrarre il Token del nome da un Target.
    /// </summary>
    private Token GetTargetToken(Target target)
    {
        return target switch
        {
            VariableTarget vt => vt.NameCleaned,
            ArrayElementTarget aet => aet.NameCleaned,
            FieldTarget ft => GetTargetToken(ft.Structure), // Ricorsivo per p.x.y
            IndexedTarget it => GetTargetTokenFromExpr(it.Base),
            _ => throw new NotImplementedException($"Cannot get name token for target type {target.GetType().Name}")
        };
    }

    private Token GetTargetTokenFromExpr(Expr expr)
    {
        return expr switch
        {
            VariableExpr ve => ve.NameCleaned,
            ArrayAccessExpr aae => aae.NameCleaned,
            FieldAccessExpr fae => GetTargetTokenFromExpr(fae.Structure), // Ricorsivo
            _ => throw new NotImplementedException($"Cannot get name token from expression type {expr.GetType().Name}")
        };
    }

    /// <summary>
    /// Metodo helper per risolvere l'indice di una variabile, sia essa locale o globale.
    /// </summary>
    private int ResolveVariableIndex(Target target)
    {
        var token = GetTargetToken(target);
        var localIndex = ResolveLocal(token.Lexeme);
        if (localIndex != -1) return localIndex;
        return ResolveGlobal(token.Lexeme);
    }
}
