// GmBasic.Jinja — motore Jinja (subset per i chat-template LLM locali). Versione C#.
// Mirror C: LLMEnv/LLMJinja. Contratto: Render(template, vars) con semantica Jinja2; costrutto
// non supportato -> JinjaError esplicita (fail-loud, mai output approssimato). Niente stub.
//
// Supportati i costrutti dei template reali (es. Llama-3.2-Instruct): whitespace control
// {{- -}} {%- -%} {# #}; {{ expr }}; {% if/elif/else/endif %}; {% for x in xs / endfor %};
// {% set x = expr %}; letterali str/int/bool/none; dict/list; .attr/['k']/[0]/slice [a:];
// + - ~ (concat/aritmetica), ==, !=, < > <= >=, in / not in, not, and/or; condizionale
// `val if cond else alt`; tuple (a, b); test is (defined, undefined, none, mapping, iterable,
// string, boolean, true, false, number, sequence) con not;
// filtri |trim |tojson |length |default |join |items |list |map |dictsort |replace |upper |lower
// |string |safe |reject (argomenti posizionali e keyword tipo attribute=); funzioni namespace(),
// range(), strftime_now(fmt); metodi stringa .split/.strip/.lstrip/.rstrip/.startswith/.endswith
// e dict .get/.items/.keys/.values; blocchi {% macro %}...{% endmacro %} e {% set x %}...{% endset %};
// loop.index/index0/revindex/first/last/length/previtem/nextitem nel for;
// raise_exception(...) quando eseguita -> JinjaError.
using System.Globalization;
using System.Text;
using System.Text.Json;

namespace GmBasic.Jinja;

public sealed class JinjaError : Exception
{
    public JinjaError(string msg) : base(msg) { }
}

public static class Jinja
{
    public static string Render(string template, IReadOnlyDictionary<string, object?> vars)
    {
        var nodes = Parse(Tokenize(template));
        var sb = new StringBuilder();
        Eval(nodes, new Env(vars), sb);
        string r = sb.ToString();
        // Jinja2 default keep_trailing_newline=false: se il template termina con newline,
        // quell'ultimo newline non viene emesso.
        if (template.EndsWith('\n') && r.EndsWith('\n') && r.Length > 0) r = r[..^1];
        return r;
    }

    // ---------------- values ----------------
    // Sentinel per valore non definito (variabile assente o attributo/chiave mancante):
    // rende "" se stampato, e' falsy e soddisfa gli operatori di guardia `is defined` /
    // `| default`, come l'Undefined di Jinja2.
    sealed class Undefined { }
    static readonly Undefined Undef = new();
    // Namespace mutabile per riferimento (`{% set ns = namespace(...) %}`): essendo un
    // Dictionary<string,object?> e' letto da Attr/DictGet e mutato da {% set ns.x = ... %}.
    sealed class Namespace : Dictionary<string, object?>
    {
        public Namespace(Dictionary<string, object?> init) : base(init, StringComparer.Ordinal) { }
    }
    static bool IsDict(object? v) => v is IReadOnlyDictionary<string, object?> || v is IDictionary<string, object?>;
    static bool IsList(object? v) => v is IReadOnlyList<object?> || v is System.Collections.IList;
    static object? DictGet(object d, string key)
    {
        if (d is IReadOnlyDictionary<string, object?> r) return r.TryGetValue(key, out var a) ? a : Undef;
        if (d is IDictionary<string, object?> m) return m.TryGetValue(key, out var b) ? b : Undef;
        if (d is System.Collections.IDictionary id) return id.Contains(key) ? id[key] : Undef;
        throw new JinjaError($"attributo '{key}' su valore non-mapping");
    }
    static bool DictHas(object d, string key)
    {
        if (d is IReadOnlyDictionary<string, object?> r) return r.ContainsKey(key);
        if (d is IDictionary<string, object?> m) return m.ContainsKey(key);
        if (d is System.Collections.IDictionary id) return id.Contains(key);
        return false;
    }
    static int Len(object? v) => v switch
    {
        string s => s.Length,
        System.Collections.ICollection c => c.Count,
        _ => throw new JinjaError("|length su valore non iterabile")
    };
    static bool IsIterable(object? v) => v is string || v is System.Collections.IEnumerable;

    // ---------------- tokenizer ----------------
    enum Kind { Text, Output, Stmt, Comment }
    sealed class Seg { public Kind K; public string S = ""; }

    static List<Seg> Tokenize(string t)
    {
        var segs = new List<Seg>();
        var text = new StringBuilder();
        int i = 0;
        bool trimNext = false;
        void Flush()
        {
            if (text.Length == 0) return;
            if (trimNext) { int k = 0; while (k < text.Length && char.IsWhiteSpace(text[k])) k++; if (k > 0) text.Remove(0, k); trimNext = false; }
            segs.Add(new Seg { K = Kind.Text, S = text.ToString() });
            text.Clear();
        }
        int Min(int a, int b, int c)
        {
            int m = -1;
            foreach (var x in new[] { a, b, c }) if (x >= 0 && (m < 0 || x < m)) m = x;
            return m;
        }
        while (i < t.Length)
        {
            int o = t.IndexOf("{{", i), p = t.IndexOf("{%", i), cm = t.IndexOf("{#", i);
            int n = Min(o, p, cm);
            if (n < 0) { text.Append(t, i, t.Length - i); break; }
            text.Append(t, i, n - i);
            if (n == cm)
            {
                int e = t.IndexOf("#}", n + 2); if (e < 0) throw new JinjaError("commento non chiuso");
                bool cm_tl = n + 2 < t.Length && t[n + 2] == '-';
                bool cm_tr = e > n + 2 && t[e - 1] == '-';
                if (cm_tl) TrimEndWs(text);
                Flush();
                trimNext = cm_tr;
                i = e + 2;
                continue;
            }
            bool out_ = n == o;
            int beg = n + 2;
            bool tg_tl = beg < t.Length && t[beg] == '-';
            if (tg_tl) beg++;
            int close = out_ ? t.IndexOf("}}", beg) : t.IndexOf("%}", beg);
            if (close < 0) throw new JinjaError(out_ ? "{{ non chiuso" : "{% non chiuso");
            int end = close;
            bool tg_tr = end > beg && t[end - 1] == '-';
            if (tg_tr) end--;
            string content = t[beg..end];
            if (tg_tl) TrimEndWs(text);
            Flush();
            segs.Add(new Seg { K = out_ ? Kind.Output : Kind.Stmt, S = content.Trim() });
            trimNext = tg_tr;
            i = close + 2;
            continue;
        }
        Flush();
        // normalizza: fondi testi adiacenti
        var res = new List<Seg>();
        foreach (var s in segs)
            if (s.K == Kind.Text && res.Count > 0 && res[^1].K == Kind.Text) res[^1].S += s.S;
            else res.Add(s);
        return res;
    }
    static void TrimEndWs(StringBuilder b)
    {
        while (b.Length > 0 && char.IsWhiteSpace(b[^1])) b.Length--;
    }

    // ---------------- parser (AST) ----------------
    abstract class Node { }
    sealed class Txt : Node { public string S = ""; }
    sealed class Out : Node { public string Expr = ""; }
    sealed class SetN : Node { public string Name = ""; public string Expr = ""; }
    sealed class IfN : Node { public List<(string? Cond, List<Node> Body)> Branches = new(); }
    sealed class ForN : Node { public string Var = ""; public string Expr = ""; public List<Node> Body = new(); }
    sealed class MacroN : Node { public string Name = ""; public List<(string Name, string? Default)> Params = new(); public List<Node> Body = new(); }
    sealed class SetBlockN : Node { public string Name = ""; public List<Node> Body = new(); }
    sealed class Macro
    {
        public List<(string Name, string? Default)> Params = new();
        public List<Node> Body = new();
        public Env? Closure;
    }

    static List<Node> Parse(List<Seg> segs)
    {
        int i = 0;
        var nodes = ParseUntil(segs, ref i, new HashSet<string>());
        if (i < segs.Count) throw new JinjaError("tag di blocco senza apertura");
        return nodes;
    }
    static List<Node> ParseUntil(List<Seg> segs, ref int i, HashSet<string> stop)
    {
        var nodes = new List<Node>();
        while (i < segs.Count)
        {
            var s = segs[i];
            if (s.K == Kind.Text) { nodes.Add(new Txt { S = s.S }); i++; continue; }
            if (s.K == Kind.Output) { nodes.Add(new Out { Expr = s.S }); i++; continue; }
            if (s.K == Kind.Comment) { i++; continue; }
            string code = s.S;
            string head = FirstWord(code);
            if (stop.Contains(head)) return nodes;   // lascia il tag al chiamante
            switch (head)
            {
                case "set":
                {
                    int sp = 3; while (sp < code.Length && char.IsWhiteSpace(code[sp])) sp++;
                    int ne = sp; while (ne < code.Length && !char.IsWhiteSpace(code[ne]) && code[ne] != '=') ne++;
                    string sname = code[sp..ne].Trim();
                    int eq = code.IndexOf('=', ne);
                    if (eq >= 0)
                    {
                        nodes.Add(new SetN { Name = sname, Expr = code[(eq + 1)..].Trim() });
                        i++;
                    }
                    else
                    {
                        i++;
                        var body = ParseUntil(segs, ref i, new HashSet<string> { "endset" });
                        if (i >= segs.Count || FirstWord(segs[i].S) != "endset")
                            throw new JinjaError("set senza '=' e senza endset: " + code);
                        i++;
                        nodes.Add(new SetBlockN { Name = sname, Body = body });
                    }
                    break;
                }
                case "macro":
                {
                    int lp = code.IndexOf('(');
                    if (lp < 0 || !code.TrimEnd().EndsWith(")")) throw new JinjaError("macro malformato: " + code);
                    string mname = code[5..lp].Trim();
                    string paramStr = code[(lp + 1)..].TrimEnd();
                    paramStr = paramStr[..^1];
                    var ps = new List<(string, string?)>();
                    foreach (var raw in SplitTopLevel(paramStr, ','))
                    {
                        string p = raw.Trim();
                        if (p.Length == 0) continue;
                        int pe = TopLevelIndexOf(p, '=');
                        ps.Add(pe < 0 ? (p, null) : (p[..pe].Trim(), p[(pe + 1)..].Trim()));
                    }
                    i++;
                    var mbody = ParseUntil(segs, ref i, new HashSet<string> { "endmacro" });
                    if (i >= segs.Count || FirstWord(segs[i].S) != "endmacro") throw new JinjaError("macro senza endmacro");
                    i++;
                    nodes.Add(new MacroN { Name = mname, Params = ps, Body = mbody });
                    break;
                }
                case "for":
                {
                    int in_ = code.IndexOf(" in ", StringComparison.Ordinal);
                    if (in_ < 0) throw new JinjaError("for senza 'in': " + code);
                    nodes.Add(new ForN { Var = code[3..in_].Trim(), Expr = code[(in_ + 4)..].Trim(), Body = new List<Node>() });
                    i++;
                    var fn = (ForN)nodes[^1];
                    fn.Body = ParseUntil(segs, ref i, new HashSet<string> { "endfor" });
                    if (i >= segs.Count || FirstWord(segs[i].S) != "endfor") throw new JinjaError("for senza endfor");
                    i++;
                    break;
                }
                case "if":
                {
                    i++;
                    var ifn = new IfN();
                    string cond = code[2..].Trim();
                    var body = ParseUntil(segs, ref i, new HashSet<string> { "elif", "else", "endif" });
                    ifn.Branches.Add((cond, body));
                    while (i < segs.Count)
                    {
                        string w = FirstWord(segs[i].S);
                        if (w == "elif") { string c2 = segs[i].S[4..].Trim(); i++; var b2 = ParseUntil(segs, ref i, new HashSet<string> { "elif", "else", "endif" }); ifn.Branches.Add((c2, b2)); continue; }
                        if (w == "else") { i++; var b3 = ParseUntil(segs, ref i, new HashSet<string> { "endif" }); ifn.Branches.Add((null, b3)); continue; }
                        if (w == "endif") { i++; break; }
                        throw new JinjaError($"if malformato (tag: {segs[i].S})");
                    }
                    if (i > segs.Count) throw new JinjaError("if senza endif");
                    nodes.Add(ifn);
                    break;
                }
                default: throw new JinjaError($"statement non supportato: {head}");
            }
        }
        return nodes;
    }
    static string FirstWord(string s)
    {
        int k = 0; while (k < s.Length && !char.IsWhiteSpace(s[k])) k++;
        return s[..k];
    }

    // ---------------- eval ----------------
    sealed class Env
    {
        readonly Dictionary<string, object?> d;
        public Env(IReadOnlyDictionary<string, object?>? vars) { d = new Dictionary<string, object?>(vars ?? new Dictionary<string, object?>(), StringComparer.Ordinal); }
        public Env(Env p) { d = new Dictionary<string, object?>(p.d, StringComparer.Ordinal); }
        public bool Has(string k) => d.ContainsKey(k);
        public object? Get(string k) => d.TryGetValue(k, out var v) ? v : null;
        public void Set(string k, object? v) => d[k] = v;
    }

    static void Eval(List<Node> nodes, Env env, StringBuilder sb)
    {
        foreach (var n in nodes)
        {
            switch (n)
            {
                case Txt t: sb.Append(t.S); break;
                case Out o: sb.Append(ToStr(EvalExpr(o.Expr, env))); break;
                case SetN sn: Assign(sn.Name, EvalExpr(sn.Expr, env), env); break;
                case SetBlockN sbn: env.Set(sbn.Name, EvalToString(sbn.Body, env)); break;
                case MacroN mn:
                    env.Set(mn.Name, new Macro { Params = mn.Params, Body = mn.Body, Closure = env });
                    break;
                case IfN ifn:
                {
                    foreach (var (cond, body) in ifn.Branches)
                        if (cond == null || Truth(EvalExpr(cond, env))) { Eval(body, env, sb); break; }
                    break;
                }
                case ForN fn:
                {
                    var it = EvalExpr(fn.Expr, env);
                    if (!IsIterable(it)) throw new JinjaError($"'for' su non iterabile: {fn.Expr}");
                    var list = Items(it).ToList();
                    var vars = fn.Var.Split(',').Select(v => v.Trim()).Where(v => v.Length > 0).ToList();
                    for (int idx = 0; idx < list.Count; idx++)
                    {
                        var le = new Env(env);
                        if (vars.Count == 1) le.Set(vars[0], list[idx]);
                        else Unpack(vars, list[idx], le, fn.Var);
                        le.Set("loop", new Dictionary<string, object?>
                        {
                            ["index"] = (long)(idx + 1),
                            ["index0"] = (long)idx,
                            ["revindex"] = (long)(list.Count - idx),
                            ["revindex0"] = (long)(list.Count - idx - 1),
                            ["first"] = idx == 0,
                            ["last"] = idx == list.Count - 1,
                            ["length"] = (long)list.Count,
                            ["previtem"] = idx > 0 ? list[idx - 1] : (object)Undef,
                            ["nextitem"] = idx < list.Count - 1 ? list[idx + 1] : (object)Undef,
                        });
                        Eval(fn.Body, le, sb);
                    }
                    break;
                }
            }
        }
    }
    static string EvalToString(List<Node> nodes, Env env)
    {
        var sb = new StringBuilder();
        Eval(nodes, env, sb);
        return sb.ToString();
    }
    // Assegnazione: `name`, `ns.attr` o `ns['key']` (namespace/dict mutabile per riferimento).
    static void Assign(string target, object? val, Env env)
    {
        int dot = target.IndexOf('.');
        int br = target.IndexOf('[');
        int cut = dot < 0 ? br : br < 0 ? dot : System.Math.Min(dot, br);
        if (cut <= 0) { env.Set(target, val); return; }
        string baseName = target[..cut].Trim();
        object? obj = env.Has(baseName) ? env.Get(baseName) : Undef;
        string rest = target[cut..].Trim();
        if (rest.StartsWith('['))
        {
            int end = rest.IndexOf(']');
            if (end < 0) throw new JinjaError("assegnazione con '[' non chiusa: " + target);
            object key = (object?)EvalExpr(rest[1..end], env) ?? Undef;
            if (obj is System.Collections.IDictionary d) { d[key] = val; return; }
            throw new JinjaError("assegnazione a indice su valore non-mapping: " + target);
        }
        string attr = rest[1..].Trim();
        if (attr.Contains('.') || attr.Contains('[')) throw new JinjaError("assegnazione annidata non supportata: " + target);
        if (obj is IDictionary<string, object?> m) { m[attr] = val; return; }
        if (obj is System.Collections.IDictionary id) { id[attr] = val; return; }
        throw new JinjaError("assegnazione a attributo su valore non-mapping: " + target);
    }
    static void Unpack(List<string> vars, object? item, Env env, string rawVar)
    {
        List<object?>? seq = item is string || !IsIterable(item) ? null : Items(item).ToList();
        if (seq is null || seq.Count < vars.Count)
            throw new JinjaError($"'for {rawVar} ...' non puo' scompattare un valore non-pair");
        for (int k = 0; k < vars.Count; k++) env.Set(vars[k], seq[k]);
    }
    static IEnumerable<object?> Items(object? it)
    {
        if (it is string s) { foreach (var c in s) yield return c.ToString(); yield break; }
        if (it is System.Collections.IEnumerable e) foreach (var x in e) yield return x;
    }

    static bool Truth(object? v) => v switch
    {
        null => false,
        Undefined => false,
        bool b => b,
        string s => s.Length > 0,
        VarName => false,
        long l => l != 0,
        int i => i != 0,
        double d => d != 0,
        System.Collections.ICollection c => c.Count > 0,
        System.Collections.IEnumerable e => e.Cast<object?>().Any(),
        _ => true
    };
    static string ToStr(object? v) => v switch
    {
        null => "",
        Undefined => "",
        string s => s,
        bool b => b ? "True" : "False",
        double d => d.ToString(CultureInfo.InvariantCulture),
        long l => l.ToString(CultureInfo.InvariantCulture),
        int x => x.ToString(CultureInfo.InvariantCulture),
        _ when IsList(v) => "[" + string.Join(", ", Items(v).Select(ToStr)) + "]",
        _ => v.ToString() ?? ""
    };

    // ---------------- espressioni ----------------
    static string[] Toks(string s) => LexExpr(s).ToArray();
    static List<string> LexExpr(string s)
    {
        var r = new List<string>(); int i = 0;
        while (i < s.Length)
        {
            char c = s[i];
            if (char.IsWhiteSpace(c)) { i++; continue; }
            if (c == '\'' || c == '"')
            {
                char q = c; i++; var b = new StringBuilder();
                while (i < s.Length && s[i] != q)
                {
                    if (s[i] == '\\' && i + 1 < s.Length)
                    {
                        i++;
                        switch (s[i]) { case 'n': b.Append('\n'); break; case 't': b.Append('\t'); break; case 'r': b.Append('\r'); break; case '\\': b.Append('\\'); break; case '"': b.Append('"'); break; case '\'': b.Append('\''); break; default: b.Append(s[i]); break; }
                        i++;
                    }
                    else { b.Append(s[i]); i++; }
                }
                if (i >= s.Length) throw new JinjaError("stringa non chiusa");
                i++;
                r.Add("STR:" + b);
                continue;
            }
            if (char.IsDigit(c) || (c == '-' && i + 1 < s.Length && char.IsDigit(s[i + 1])))
            {
                int st = i; if (c == '-') i++;
                while (i < s.Length && (char.IsDigit(s[i]) || s[i] == '.')) i++;
                r.Add(s[st..i]); continue;
            }
            if (char.IsLetter(c) || c == '_')
            {
                int st = i; while (i < s.Length && (char.IsLetterOrDigit(s[i]) || s[i] == '_')) i++;
                r.Add(s[st..i]); continue;
            }
            foreach (var op in new[] { "==", "!=", "<=", ">=", "->", "<", ">" })
                if (s.AsSpan(i).StartsWith(op)) { r.Add(op); i += op.Length; goto next; }
            if ("|()[]:.,+-=~".Contains(c)) { r.Add(c.ToString()); i++; }
            else throw new JinjaError($"token inatteso '{c}'");
        next:;
        }
        return r;
    }

    static object? EvalExpr(string expr, Env env)
    {
        var toks = LexExpr(expr);
        int p = 0;
        var v = PConditional(toks, ref p, env);
        if (p != toks.Count) throw new JinjaError($"espressione malformata: {expr} (token {toks[p]})");
        return v;
    }
    // Jinja: `val if cond else alt` (precedenza minima, associativo a destra).
    static object? PConditional(List<string> t, ref int p, Env e)
    {
        var v = POr(t, ref p, e);
        if (p < t.Count && t[p] == "if")
        {
            p++;
            var cond = POr(t, ref p, e);
            if (p >= t.Count || t[p] != "else") throw new JinjaError("condizionale 'if' senza 'else'");
            p++;
            var alt = PConditional(t, ref p, e);
            return Truth(cond) ? v : alt;
        }
        return v;
    }
    // Jinja/Python: `or`/`and` ritornano l'OPERANDO, non un bool (es. `x or y` -> x se truthy).
    static object? POr(List<string> t, ref int p, Env e)
    {
        var v = PAnd(t, ref p, e);
        while (p < t.Count && t[p] == "or") { p++; var r = PAnd(t, ref p, e); v = Truth(v) ? v : r; }
        return v;
    }
    static object? PAnd(List<string> t, ref int p, Env e)
    {
        var v = PNot(t, ref p, e);
        while (p < t.Count && t[p] == "and") { p++; var r = PNot(t, ref p, e); v = Truth(v) ? r : v; }
        return v;
    }
    static object? PNot(List<string> t, ref int p, Env e)
    {
        if (p < t.Count && t[p] == "not") { p++; var v = PNot(t, ref p, e); return !Truth(v); }
        return PTest(t, ref p, e);
    }
    static object? PTest(List<string> t, ref int p, Env e)
    {
        var lhs = PConcat(t, ref p, e);
        if (p >= t.Count) return lhs;
        if (t[p] == "not" && p + 1 < t.Count && t[p + 1] == "in") { p += 2; var r = PConcat(t, ref p, e); return !Contains(r, lhs); }
        if (t[p] == "==") { p++; var r = PConcat(t, ref p, e); return Equal(lhs, r); }
        if (t[p] == "!=") { p++; var r = PConcat(t, ref p, e); return !Equal(lhs, r); }
        if (t[p] == "<") { p++; var r = PConcat(t, ref p, e); return Compare(lhs, r) < 0; }
        if (t[p] == ">") { p++; var r = PConcat(t, ref p, e); return Compare(lhs, r) > 0; }
        if (t[p] == "<=") { p++; var r = PConcat(t, ref p, e); return Compare(lhs, r) <= 0; }
        if (t[p] == ">=") { p++; var r = PConcat(t, ref p, e); return Compare(lhs, r) >= 0; }
        if (t[p] == "in") { p++; var r = PConcat(t, ref p, e); return Contains(r, lhs); }
        if (t[p] == "is")
        {
            p++; bool neg = false;
            if (p < t.Count && t[p] == "not") { neg = true; p++; }
            if (p >= t.Count) throw new JinjaError("is senza test");
            string test = t[p]; p++;
            bool res = test switch
            {
                "defined" => lhs switch { Undefined => false, VarName vn => e.Has(vn.Name), _ => true },
                "undefined" => lhs switch { Undefined => true, VarName vn2 => !e.Has(vn2.Name), _ => false },
                "none" => lhs == null,
                "mapping" => IsDict(lhs),
                "iterable" => IsIterable(lhs),
                "string" => lhs is string,
                "boolean" => lhs is bool,
                "true" => lhs is bool bt && bt,
                "false" => lhs is bool bf && !bf,
                "number" => lhs is long || lhs is double,
                "sequence" => IsList(lhs) || lhs is string,
                _ => throw new JinjaError($"test is non supportato: {test}")
            };
            return neg ? !res : res;
        }
        return lhs;
    }
    sealed class VarName { public string Name = ""; }
    static object? PConcat(List<string> t, ref int p, Env e)
    {
        var v = PUnary(t, ref p, e);
        while (p < t.Count && (t[p] == "+" || t[p] == "-" || t[p] == "~"))
        {
            string op = t[p]; p++;
            var r = PUnary(t, ref p, e);
            v = op == "+" ? Concat(v, r) : op == "-" ? Subtract(v, r) : ToStr(v) + ToStr(r);
        }
        return v;
    }
    static object? PUnary(List<string> t, ref int p, Env e)
    {
        if (p < t.Count && t[p] == "-") { p++; var v = PUnary(t, ref p, e); if (v is long l) return -l; if (v is double d) return -d; throw new JinjaError("meno unario non numerico"); }
        return PFilter(t, ref p, e);
    }
    static object? PFilter(List<string> t, ref int p, Env e)
    {
        var v = PPostfix(t, ref p, e);
        while (p < t.Count && t[p] == "|")
        {
            p++;
            if (p >= t.Count) throw new JinjaError("| senza filtro");
            string f = t[p]; p++;
            var args = new List<object?>();
            var kwargs = new Dictionary<string, object?>(StringComparer.Ordinal);
            if (p < t.Count && t[p] == "(") { p++; ParseCallArgs(t, ref p, e, args, kwargs); }
            v = ApplyFilter(f, v, args, kwargs);
        }
        return v;
    }
    static void ParseCallArgs(List<string> t, ref int p, Env e, List<object?> pos, Dictionary<string, object?> kw)
    {
        while (p < t.Count && t[p] != ")")
        {
            if (IsNameToken(t[p]) && p + 1 < t.Count && t[p + 1] == "=")
            {
                string key = t[p]; p += 2;
                kw[key] = POr(t, ref p, e);
            }
            else pos.Add(POr(t, ref p, e));
            if (p < t.Count && t[p] == ",") p++;
            else if (p < t.Count && t[p] != ")") throw new JinjaError("argomenti funzione malformati");
        }
        if (p >= t.Count) throw new JinjaError("')' mancante nella chiamata");
        p++;
    }
    static bool IsNameToken(string s) => s.Length > 0 && (char.IsLetter(s[0]) || s[0] == '_') && !s.StartsWith("STR:", StringComparison.Ordinal);
    static object? ApplyFilter(string f, object? v, List<object?> args, Dictionary<string, object?> kw)
    {
        if (v is Undefined)
            return f switch
            {
                "default" => args.Count > 0 ? args[0] : "",
                "length" or "count" => 0L,
                "join" => "",
                "list" or "items" or "dictsort" or "map" => new List<object?>(),
                "tojson" => "null",
                "safe" => v,
                _ => ""
            };
        switch (f)
        {
            case "trim": return (v as string)?.Trim() ?? v;
            case "length": return (long)Len(v);
            case "count": return (long)Len(v);
            case "tojson": return ToJson(v);
            case "safe": return v;
            case "string": return ToStr(v);
            case "upper": return ToStr(v).ToUpperInvariant();
            case "lower": return ToStr(v).ToLowerInvariant();
            case "default":
            {
                bool useDefault = v is null || v is VarName || v is Undefined;
                if (!useDefault && args.Count > 1 && Truth(args[1])) useDefault = !Truth(v);
                return useDefault ? (args.Count > 0 ? args[0] : "") : v;
            }
            case "join":
            {
                string sep = args.Count > 0 ? ToStr(args[0]) : "";
                return string.Join(sep, ToList(v).Select(ToStr));
            }
            case "list": return ToList(v);
            case "items":
            {
                var res = new List<object?>();
                foreach (var kv in DictPairs(v)) res.Add(new List<object?> { kv.Key, kv.Value });
                return res;
            }
            case "dictsort":
            {
                var res = new List<object?>();
                foreach (var kv in DictPairs(v).OrderBy(x => ToStr(x.Key), StringComparer.Ordinal))
                    res.Add(new List<object?> { kv.Key, kv.Value });
                return res;
            }
            case "map":
            {
                var res = new List<object?>();
                if (kw.TryGetValue("attribute", out var attr) && attr is string an)
                    foreach (var x in ToList(v)) res.Add(DictGet(x!, an));
                else if (args.Count > 0 && args[0] is string fn)
                    foreach (var x in ToList(v)) res.Add(ApplyByName(fn, x));
                else throw new JinjaError("map richiede attribute= o un filtro");
                return res;
            }
            case "replace":
            {
                if (args.Count < 2) throw new JinjaError("replace richiede 2 argomenti");
                return ToStr(v).Replace(ToStr(args[0]), ToStr(args[1]), StringComparison.Ordinal);
            }
            case "reject":
            {
                if (args.Count < 2) throw new JinjaError("reject richiede test e argomento");
                string test = ToStr(args[0]); object? target = args[1];
                var res = new List<object?>();
                foreach (var x in ToList(v))
                {
                    bool hit = test switch
                    {
                        "in" => Contains(target, x),
                        "eq" or "equalto" => Equal(x, target),
                        _ => throw new JinjaError($"reject: test non supportato '{test}'")
                    };
                    if (!hit) res.Add(x);
                }
                return res;
            }
            default: throw new JinjaError($"filtro non supportato: {f}");
        }
    }
    static object? ApplyByName(string fn, object? x) => fn switch
    {
        "upper" => ToStr(x).ToUpperInvariant(),
        "lower" => ToStr(x).ToLowerInvariant(),
        "trim" => (x as string)?.Trim() ?? x,
        "string" => ToStr(x),
        "length" => (long)Len(x),
        _ => throw new JinjaError($"map: filtro non supportato '{fn}'")
    };
    static List<object?> ToList(object? v)
    {
        if (!IsIterable(v)) throw new JinjaError("valore non iterabile");
        return Items(v).ToList();
    }
    static IEnumerable<KeyValuePair<object?, object?>> DictPairs(object? d)
    {
        if (d is IReadOnlyDictionary<string, object?> r) { foreach (var kv in r) yield return new KeyValuePair<object?, object?>(kv.Key, kv.Value); }
        else if (d is IDictionary<string, object?> m) { foreach (var kv in m) yield return new KeyValuePair<object?, object?>(kv.Key, kv.Value); }
        else if (d is System.Collections.IDictionary id) { foreach (System.Collections.DictionaryEntry de in id) yield return new KeyValuePair<object?, object?>(de.Key, de.Value); }
        else throw new JinjaError("filtro su valore non-mapping");
    }
    static List<string> SplitTopLevel(string s, char sep)
    {
        var res = new List<string>();
        var cur = new StringBuilder();
        int depth = 0; char q = '\0'; bool esc = false;
        foreach (char c in s)
        {
            if (q != '\0')
            {
                cur.Append(c);
                if (esc) esc = false;
                else if (c == '\\') esc = true;
                else if (c == q) q = '\0';
                continue;
            }
            if (c == '\'' || c == '"') { q = c; cur.Append(c); continue; }
            if (c == '(' || c == '[' || c == '{') depth++;
            else if (c == ')' || c == ']' || c == '}') depth--;
            if (c == sep && depth == 0) { res.Add(cur.ToString()); cur.Clear(); }
            else cur.Append(c);
        }
        res.Add(cur.ToString());
        return res;
    }
    static int TopLevelIndexOf(string s, char target)
    {
        int depth = 0; char q = '\0'; bool esc = false;
        for (int i = 0; i < s.Length; i++)
        {
            char c = s[i];
            if (q != '\0')
            {
                if (esc) esc = false;
                else if (c == '\\') esc = true;
                else if (c == q) q = '\0';
                continue;
            }
            if (c == '\'' || c == '"') { q = c; continue; }
            if (c == '(' || c == '[' || c == '{') depth++;
            else if (c == ')' || c == ']' || c == '}') depth--;
            else if (c == target && depth == 0) return i;
        }
        return -1;
    }
    static object? PPostfix(List<string> t, ref int p, Env e)
    {
        var v = PAtom(t, ref p, e);
        while (p < t.Count)
        {
            if (t[p] == ".")
            {
                p++;
                if (p >= t.Count) throw new JinjaError("'.' senza nome");
                string member = t[p]; p++;
                if (p < t.Count && t[p] == "(")
                {
                    p++;
                    var margs = new List<object?>();
                    var mkw = new Dictionary<string, object?>(StringComparer.Ordinal);
                    ParseCallArgs(t, ref p, e, margs, mkw);
                    v = Method(v, member, margs, mkw);
                }
                else v = Attr(v, member);
                continue;
            }
            if (t[p] == "[")
            {
                p++;
                object? start = null, stop = null, step = null;
                bool slice = false;
                if (p < t.Count && t[p] == ":") { slice = true; p++; }
                else
                {
                    start = PConditional(t, ref p, e);
                    if (p < t.Count && t[p] == ":") { slice = true; p++; }
                }
                if (slice)
                {
                    if (p < t.Count && t[p] != "]" && t[p] != ":") stop = PConditional(t, ref p, e);
                    if (p < t.Count && t[p] == ":") { p++; if (p < t.Count && t[p] != "]") step = PConditional(t, ref p, e); }
                    if (p >= t.Count || t[p] != "]") throw new JinjaError("']' mancante");
                    p++;
                    v = Slice(v, start, stop, step);
                }
                else
                {
                    if (p >= t.Count || t[p] != "]") throw new JinjaError("']' mancante");
                    p++;
                    v = Index(v, start);
                }
                continue;
            }
            break;
        }
        return v;
    }
    static object? PAtom(List<string> t, ref int p, Env e)
    {
        if (p >= t.Count) throw new JinjaError("espressione vuota");
        string tk = t[p];
        if (tk.StartsWith("STR:")) { p++; return tk[4..]; }
        if (tk == "true") { p++; return true; }
        if (tk == "false") { p++; return false; }
        if (tk == "none" || tk == "None") { p++; return null; }
        if (tk == "(")
        {
            p++;
            var tup = new List<object?>();
            while (p < t.Count && t[p] != ")")
            {
                tup.Add(PConditional(t, ref p, e));
                if (p < t.Count && t[p] == ",") p++;
            }
            if (p >= t.Count) throw new JinjaError("')' mancante");
            p++;
            return tup.Count == 1 ? tup[0] : tup;   // parentesi o tupla literal
        }
        if (tk == "[")
        {
            p++;
            var lst = new List<object?>();
            while (p < t.Count && t[p] != "]")
            {
                lst.Add(POr(t, ref p, e));
                if (p < t.Count && t[p] == ",") p++;
            }
            if (p >= t.Count) throw new JinjaError("']' mancante");
            p++;
            return lst;
        }
        if (long.TryParse(tk, out var l)) { p++; return l; }
        if (double.TryParse(tk, NumberStyles.Float, CultureInfo.InvariantCulture, out var d)) { p++; return d; }
        // nome -> lookup o chiamata funzione
        string name = tk; p++;
        if (p < t.Count && t[p] == "(")
        {
            p++;
            var args = new List<object?>();
            var kwargs = new Dictionary<string, object?>(StringComparer.Ordinal);
            ParseCallArgs(t, ref p, e, args, kwargs);
            return Call(name, args, kwargs, e);
        }
        if (e.Has(name)) return e.Get(name);
        return new VarName { Name = name }; // per 'is defined'/'is not defined'
    }
    static object? Call(string name, List<object?> args, Dictionary<string, object?> kwargs, Env e)
    {
        if (name == "raise_exception") throw new JinjaError(args.Count > 0 ? ToStr(args[0]) : "raise_exception");
        if (name == "namespace") return new Namespace(kwargs);
        if (name == "range") return Range(args);
        if (name == "strftime_now") return Strftime(DateTimeOffset.Now, args.Count > 0 ? ToStr(args[0]) : "");
        if (e.Has(name) && e.Get(name) is Macro m) return EvalMacro(m, args, kwargs);
        if (e.Has(name) && e.Get(name) is Delegate d) return d.DynamicInvoke(args.ToArray());
        throw new JinjaError($"funzione non supportata: {name}");
    }
    static string EvalMacro(Macro m, List<object?> args, Dictionary<string, object?> kwargs)
    {
        var me = new Env(m.Closure ?? new Env((IReadOnlyDictionary<string, object?>?)null));
        int argi = 0;
        for (int k = 0; k < m.Params.Count; k++)
        {
            var (pname, pdef) = m.Params[k];
            object? val;
            if (kwargs.TryGetValue(pname, out var kwv)) val = kwv;
            else if (argi < args.Count) val = args[argi++];
            else if (pdef is not null) val = EvalExpr(pdef, me);
            else val = null;
            me.Set(pname, val);
        }
        return EvalToString(m.Body, me);
    }
    static long ToLong(object? v) => v switch
    {
        long l => l,
        int i => i,
        double d => (long)d,
        _ => throw new JinjaError("atteso un intero")
    };
    static object? Range(List<object?> args)
    {
        long start, stop, step;
        if (args.Count == 1) { start = 0; stop = ToLong(args[0]); step = 1; }
        else if (args.Count >= 2) { start = ToLong(args[0]); stop = ToLong(args[1]); step = args.Count >= 3 ? ToLong(args[2]) : 1; }
        else throw new JinjaError("range richiede almeno 1 argomento");
        if (step == 0) throw new JinjaError("range con step 0");
        var res = new List<object?>();
        if (step > 0) for (long i = start; i < stop; i += step) res.Add(i);
        else for (long i = start; i > stop; i += step) res.Add(i);
        return res;
    }
    static string Strftime(DateTimeOffset t, string fmt)
    {
        var sb = new StringBuilder();
        for (int i = 0; i < fmt.Length; i++)
        {
            if (fmt[i] != '%' || i + 1 >= fmt.Length) { sb.Append(fmt[i]); continue; }
            char c = fmt[++i];
            switch (c)
            {
                case 'Y': sb.Append(t.Year.ToString("D4", CultureInfo.InvariantCulture)); break;
                case 'y': sb.Append((t.Year % 100).ToString("D2", CultureInfo.InvariantCulture)); break;
                case 'm': sb.Append(t.Month.ToString("D2", CultureInfo.InvariantCulture)); break;
                case 'd': sb.Append(t.Day.ToString("D2", CultureInfo.InvariantCulture)); break;
                case 'H': sb.Append(t.Hour.ToString("D2", CultureInfo.InvariantCulture)); break;
                case 'I': { int h = t.Hour % 12; if (h == 0) h = 12; sb.Append(h.ToString("D2", CultureInfo.InvariantCulture)); break; }
                case 'M': sb.Append(t.Minute.ToString("D2", CultureInfo.InvariantCulture)); break;
                case 'S': sb.Append(t.Second.ToString("D2", CultureInfo.InvariantCulture)); break;
                case 'p': sb.Append(t.Hour < 12 ? "AM" : "PM"); break;
                case 'b': sb.Append(t.ToString("MMM", CultureInfo.InvariantCulture)); break;
                case 'B': sb.Append(t.ToString("MMMM", CultureInfo.InvariantCulture)); break;
                case 'a': sb.Append(t.ToString("ddd", CultureInfo.InvariantCulture)); break;
                case 'A': sb.Append(t.ToString("dddd", CultureInfo.InvariantCulture)); break;
                case 'j': sb.Append(t.DayOfYear.ToString("D3", CultureInfo.InvariantCulture)); break;
                case 'z': sb.Append(t.ToString("zzz", CultureInfo.InvariantCulture).Replace(":", "", StringComparison.Ordinal)); break;
                case 'Z': sb.Append(t.ToString("zzz", CultureInfo.InvariantCulture)); break;
                case 'f': sb.Append(t.ToString("ffffff", CultureInfo.InvariantCulture)); break;
                case '%': sb.Append('%'); break;
                default: sb.Append('%').Append(c); break; // placeholder ignoto: letterale, mai crash
            }
        }
        return sb.ToString();
    }
    static object? Method(object? v, string name, List<object?> args, Dictionary<string, object?> kwargs)
    {
        if (v is Undefined) return Undef;
        if (name == "get")
        {
            if (!IsDict(v)) throw new JinjaError(".get() su valore non-mapping");
            string key = args.Count > 0 ? ToStr(args[0]) : throw new JinjaError(".get() richiede una chiave");
            var got = DictGet(v!, key);
            return got is Undefined && args.Count > 1 ? args[1] : got;
        }
        if (name == "items")
        {
            if (!IsDict(v)) throw new JinjaError(".items() su valore non-mapping");
            var res = new List<object?>();
            foreach (var kv in DictPairs(v!)) res.Add(new List<object?> { kv.Key, kv.Value });
            return res;
        }
        if (name == "keys" || name == "values")
        {
            if (!IsDict(v)) throw new JinjaError($".{name}() su valore non-mapping");
            var res = new List<object?>();
            foreach (var kv in DictPairs(v!)) res.Add(name == "keys" ? kv.Key : kv.Value);
            return res;
        }
        if (v is string s)
        {
            switch (name)
            {
                case "split": return SplitString(s, args);
                case "strip": return PyStrip(s, args.Count > 0 && args[0] is not null ? ToStr(args[0]) : null, true, true);
                case "lstrip": return PyStrip(s, args.Count > 0 && args[0] is not null ? ToStr(args[0]) : null, true, false);
                case "rstrip": return PyStrip(s, args.Count > 0 && args[0] is not null ? ToStr(args[0]) : null, false, true);
                case "startswith": return StartsEnds(s, args, true);
                case "endswith": return StartsEnds(s, args, false);
                case "upper": return s.ToUpperInvariant();
                case "lower": return s.ToLowerInvariant();
            }
        }
        throw new JinjaError($"metodo non supportato: .{name}()");
    }
    static object? SplitString(string s, List<object?> args)
    {
        if (args.Count == 0 || args[0] is null)
            return s.Split((char[]?)null, StringSplitOptions.RemoveEmptyEntries).Cast<object?>().ToList();
        string sep = ToStr(args[0]);
        if (sep.Length == 0) throw new JinjaError("split con separatore vuoto");
        int max = args.Count > 1 && args[1] is long ml ? (int)ml : -1;
        string[] parts = max < 0 ? s.Split(sep) : s.Split(new[] { sep }, max + 1, StringSplitOptions.None);
        return parts.Cast<object?>().ToList();
    }
    static string PyStrip(string s, string? chars, bool left, bool right)
    {
        bool In(char c) => chars is null ? char.IsWhiteSpace(c) : chars.Contains(c, StringComparison.Ordinal);
        int lo = 0, hi = s.Length;
        if (left) while (lo < hi && In(s[lo])) lo++;
        if (right) while (hi > lo && In(s[hi - 1])) hi--;
        return s[lo..hi];
    }
    static bool StartsEnds(string s, List<object?> args, bool start)
    {
        bool Test(string p) => start ? s.StartsWith(p, StringComparison.Ordinal) : s.EndsWith(p, StringComparison.Ordinal);
        if (args.Count == 0) return false;
        if (args[0] is string p) return Test(p);
        if (args[0] is System.Collections.IEnumerable e and not string)
            foreach (var x in e) if (x is string px && Test(px)) return true;
        return false;
    }
    static object? Attr(object? v, string name)
    {
        if (v is VarName || v is Undefined) return Undef;
        if (IsDict(v)) return DictGet(v!, name);
        throw new JinjaError($"attributo '{name}' su valore non-mapping");
    }
    static object? Index(object? v, object? i)
    {
        if (i is VarName) throw new JinjaError("indice su variabile non definita");
        if (v is Undefined) return Undef;
        if (IsDict(v) && i is string ks) return DictGet(v!, ks);
        if (v is System.Collections.IList l && i is long idx)
        {
            long n = idx < 0 ? l.Count + idx : idx;
            if (n < 0 || n >= l.Count) throw new JinjaError("indice fuori range");
            return l[(int)n];
        }
        throw new JinjaError($"subscript non supportato ({v?.GetType().Name}[{i}])");
    }
    // Slice Python-like: a[:], a[x:], a[:y], a[x:y], a[::-1], a[x:y:step].
    static object? Slice(object? v, object? a, object? b, object? stepObj)
    {
        if (v is not System.Collections.IList && v is not string) throw new JinjaError("slice su valore non indicizzabile");
        long step = stepObj is null ? 1 : ToLong(stepObj);
        if (step == 0) throw new JinjaError("slice con step 0");
        int n = v is System.Collections.IList l0 ? l0.Count : ((string)v).Length;
        long lo, hi;
        if (step > 0)
        {
            lo = a is null ? 0 : ToLong(a); if (lo < 0) lo += n; lo = System.Math.Clamp(lo, 0, n);
            hi = b is null ? n : ToLong(b); if (hi < 0) hi += n; hi = System.Math.Clamp(hi, 0, n);
            var res = new List<object?>();
            for (long k = lo; k < hi; k += step) res.Add(v is System.Collections.IList l ? l[(int)k] : ((string)v)[(int)k].ToString());
            return v is string ? string.Concat(res.Select(ToStr)) : res;
        }
        lo = a is null ? n - 1 : ToLong(a); if (lo < 0) lo += n; lo = System.Math.Clamp(lo, -1, n - 1);
        if (b is null) hi = -1;
        else { hi = ToLong(b); if (hi < 0) hi += n; hi = System.Math.Clamp(hi, -1, n - 1); }
        var res2 = new List<object?>();
        for (long k = lo; k > hi; k += step) res2.Add(v is System.Collections.IList l ? l[(int)k] : ((string)v)[(int)k].ToString());
        return v is string ? string.Concat(res2.Select(ToStr)) : res2;
    }
    static int Compare(object? a, object? b)
    {
        if (a is int ai) a = (long)ai;
        if (b is int bi) b = (long)bi;
        if (a is long la && b is long lb) return la.CompareTo(lb);
        if (a is double || b is double)
        {
            if (!(a is long or double) || !(b is long or double)) throw new JinjaError("confronto tra tipi non numerici");
            double da = a is long l1 ? l1 : (double)a!, db = b is long l2 ? l2 : (double)b!;
            return da.CompareTo(db);
        }
        if (a is string sa && b is string sb) return string.CompareOrdinal(sa, sb);
        if (a is VarName || b is VarName) throw new JinjaError("confronto con variabile non definita");
        throw new JinjaError("confronto tra tipi non supportati");
    }
    static bool Contains(object? container, object? item)
    {
        if (container is string s && item is string sub) return s.Contains(sub, StringComparison.Ordinal);
        if (IsDict(container) && item is string k) return DictHas(container!, k);
        if (container is System.Collections.IEnumerable e) foreach (var x in e) if (Equal(x, item)) return true;
        return false;
    }
    static bool Equal(object? a, object? b)
    {
        if (a is int ai) a = (long)ai;
        if (b is int bi) b = (long)bi;
        if (a is VarName || b is VarName) return false; // confronto con non definito
        if (a is Undefined || b is Undefined) return a is Undefined && b is Undefined;
        if (a == null || b == null) return a == null && b == null;
        if (a is string sa && b is string sb) return sa == sb;
        if (a is bool && b is bool) return a.Equals(b);
        if (a is long la && b is long lb) return la == lb;
        if (a is long l2 && b is double d2) return l2 == d2;
        if (a is double d3 && b is long l3) return d3 == l3;
        if (a is double da && b is double db) return da == db;
        return a.Equals(b);
    }
    static object? Concat(object? a, object? b)
    {
        if (a is int ai) a = (long)ai;
        if (b is int bi) b = (long)bi;
        if (a is string s1 && b is string s2) return s1 + s2;
        if (a is string sa) return sa + ToStr(b);
        if (b is string sb2) return ToStr(a) + sb2;
        if (a is long l1 && b is long l2) return l1 + l2;
        if (a is long l3 && b is double dd) return l3 + dd;
        if (a is double ddd && b is long l4) return ddd + l4;
        if (a is double d1 && b is double d2) return d1 + d2;
        throw new JinjaError("'+' non supportato tra questi tipi");
    }
    static object? Subtract(object? a, object? b)
    {
        if (a is int ai) a = (long)ai;
        if (b is int bi) b = (long)bi;
        if (a is long l1 && b is long l2) return l1 - l2;
        if (a is long l3 && b is double d1) return l3 - d1;
        if (a is double d2 && b is long l4) return d2 - l4;
        if (a is double d3 && b is double d4) return d3 - d4;
        throw new JinjaError("'-' non supportato tra questi tipi");
    }
    static string ToJson(object? v) =>
        JsonSerializer.Serialize(v, new JsonSerializerOptions { WriteIndented = false });
}
