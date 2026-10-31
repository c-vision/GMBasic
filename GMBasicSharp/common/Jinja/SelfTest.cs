// Self-test CI-abile del motore Jinja (nessun modello/Python richiesto). Usato come regressione
// veloce; la parità completa con jinja2/transformers è verificata offline sui template reali
// (Llama-3.2-Instruct: system+user e multi-turn, byte-identici).
using System.Collections.Generic;

namespace GmBasic.Jinja;

public static class JinjaSelfTest
{
    public static IReadOnlyList<string> Run()
    {
        var fails = new List<string>();
        void Check(string tpl, IReadOnlyDictionary<string, object?> vars, string exp, string name)
        {
            try
            {
                string got = Jinja.Render(tpl, vars);
                if (got != exp) fails.Add($"{name}: atteso '{exp}' ottenuto '{got}'");
            }
            catch (Exception e) { fails.Add($"{name}: eccezione {e.Message}"); }
        }
        Check("a{{- 'b' -}}c", new Dictionary<string, object?>(), "abc", "whitespace-control");
        Check("{% set x = 'y' %}{{ x }}", new Dictionary<string, object?>(), "y", "set");
        Check("{% if x is defined %}D{% else %}N{% endif %}", new Dictionary<string, object?>(), "N", "is-defined");
        Check("{% if not x is defined %}D{% else %}N{% endif %}", new Dictionary<string, object?>(), "D", "not-is-defined");
        Check("{% for m in messages %}{{ m['r'] }}{% endfor %}",
            new Dictionary<string, object?> { ["messages"] = new List<object?> {
                new Dictionary<string, object?> { ["r"] = "a" }, new Dictionary<string, object?> { ["r"] = "b" } } },
            "ab", "for-dict-index");
        Check("{{ s | trim }}", new Dictionary<string, object?> { ["s"] = "  x  " }, "x", "filter-trim");
        Check("{% if 'k' in m %}yes{% endif %}",
            new Dictionary<string, object?> { ["m"] = new Dictionary<string, object?> { ["k"] = 1 } },
            "yes", "in-dict");
        Check("{{ xs[1:] }}", new Dictionary<string, object?> { ["xs"] = new List<object?> { 1, 2, 3 } }, "[2, 3]", "slice-list");
        Check("{% macro greet(name) %}Hi {{ name }}{% endmacro %}{{ greet('Bob') }}",
            new Dictionary<string, object?>(), "Hi Bob", "macro");
        Check("{% macro g(name, suffix='!') %}{{ name }}{{ suffix }}{% endmacro %}{{ g('A') }}{{ g('B', '?') }}",
            new Dictionary<string, object?>(), "A!B?", "macro-default-arg");
        Check("{% set x %}ab{% endset %}{{ x }}", new Dictionary<string, object?>(), "ab", "set-block-endset");
        Check("{{ missing | default('D') }}", new Dictionary<string, object?>(), "D", "filter-default-undefined");
        Check("{{ v | default('D') }}", new Dictionary<string, object?> { ["v"] = "V" }, "V", "filter-default-defined");
        Check("{{ xs | join('-') }}", new Dictionary<string, object?> { ["xs"] = new List<object?> { "a", "b" } }, "a-b", "filter-join");
        Check("{% for k, v in d | items %}{{ k }}={{ v }};{% endfor %}",
            new Dictionary<string, object?> { ["d"] = new Dictionary<string, object?> { ["a"] = 1, ["b"] = 2 } },
            "a=1;b=2;", "filter-items-tuple-for");
        Check("{{ items | map(attribute='n') | join(',') }}",
            new Dictionary<string, object?> { ["items"] = new List<object?> {
                new Dictionary<string, object?> { ["n"] = "x" }, new Dictionary<string, object?> { ["n"] = "y" } } },
            "x,y", "filter-map-attribute");
        Check("{{ s | replace('a', 'X') }}", new Dictionary<string, object?> { ["s"] = "banana" }, "bXnXnX", "filter-replace");
        Check("{{ s | upper }}", new Dictionary<string, object?> { ["s"] = "abc" }, "ABC", "filter-upper");
        Check("{% for m in xs %}{{ loop.index }}{{ m }}{% endfor %}",
            new Dictionary<string, object?> { ["xs"] = new List<object?> { "a", "b" } }, "1a2b", "loop-index");
        Check("{% if tools %}>t{% else %}>n{% endif %}", new Dictionary<string, object?>(), ">n", "undefined-falsy");
        Check("{% if m.y is defined %}D{% else %}N{% endif %}",
            new Dictionary<string, object?> { ["m"] = new Dictionary<string, object?>() }, "N", "missing-attr-not-defined");
        Check("{% set xs = [1, 2] %}{{ xs | length }}", new Dictionary<string, object?>(), "2", "list-literal");
        Check("{% for m in messages %}{{ m['content'] }}{% endfor %}",
            new Dictionary<string, object?> { ["messages"] = new List<object?> {
                new Dictionary<string, object?> { ["role"] = "user", ["content"] = "Ciao" },
                new Dictionary<string, object?> { ["role"] = "assistant", ["content"] = "Ok" } } },
            "CiaoOk", "real-fragment-messages");
        Check("{% set ns = namespace(count=0) %}{% for x in xs %}{% set ns.count = ns.count + 1 %}{% endfor %}{{ ns.count }}",
            new Dictionary<string, object?> { ["xs"] = new List<object?> { 1, 2, 3 } }, "3", "namespace-counter");
        Check("{% set ns = namespace(flag=false) %}{% set ns.flag = true %}{{ ns.flag }}",
            new Dictionary<string, object?>(), "True", "namespace-mutation");
        Check("{{ m.get('missing', 'D') }}", new Dictionary<string, object?> { ["m"] = new Dictionary<string, object?>() }, "D", "dict-get-default");
        Check("{{ m.get('k') }}", new Dictionary<string, object?> { ["m"] = new Dictionary<string, object?> { ["k"] = "V" } }, "V", "dict-get");
        Check("{{ s.split('</think>')[0] }}", new Dictionary<string, object?> { ["s"] = "a</think>b" }, "a", "str-split-index");
        Check("{{ s.strip(' ') }}|{{ s.lstrip(' x') }}|{{ s.rstrip(' y') }}",
            new Dictionary<string, object?> { ["s"] = " xy " }, "xy|y | x", "str-strip");
        Check("{{ s.startswith('<tool') }}", new Dictionary<string, object?> { ["s"] = "<tool_response>" }, "True", "str-startswith");
        Check("{{ s.endswith('</tool_response>') }}", new Dictionary<string, object?> { ["s"] = "x</tool_response>" }, "True", "str-endswith");
        Check("{{ 'a' ~ 'b' ~ 1 }}", new Dictionary<string, object?>(), "ab1", "tilde-concat");
        Check("{{ 5 - 2 }}", new Dictionary<string, object?>(), "3", "subtract");
        Check("{% if 2 not in (1, 3) %}E{% endif %}", new Dictionary<string, object?>(), "E", "not-in-tuple");
        Check("{{ 'A' if c else 'B' }}", new Dictionary<string, object?> { ["c"] = true }, "A", "inline-if-true");
        Check("{{ 'A' if c else 'B' }}", new Dictionary<string, object?> { ["c"] = false }, "B", "inline-if-false");
        Check("{% for i in range(3) %}{{ i }}{% endfor %}", new Dictionary<string, object?>(), "012", "range");
        Check("{{ missing or 'fallback' }}", new Dictionary<string, object?>(), "fallback", "or-returns-operand");
        Check("{{ a or 'b' }}{{ a and 'c' }}", new Dictionary<string, object?> { ["a"] = "A" }, "Ac", "and-or-operands");
        Check("{{ xs[1:] }}", new Dictionary<string, object?> { ["xs"] = new List<object?> { 1, 2, 3 } }, "[2, 3]", "slice-still");
        Check("{{ xs | reject('in', (1, 3)) | join(',') }}",
            new Dictionary<string, object?> { ["xs"] = new List<object?> { 1, 2, 3 } }, "2", "filter-reject");
        Check("{% macro g(name, suffix='!') %}{{ name }}{{ suffix }}{% endmacro %}{{ g('A') }}{{ g('B', '?') }}",
            new Dictionary<string, object?>(), "A!B?", "macro-default-still");
        var year = Jinja.Render("{{ strftime_now('%Y') }}", new Dictionary<string, object?>());
        if (year.Length != 4 || !year.All(char.IsDigit)) fails.Add($"strftime_now-Year: '{year}' non e' 4 cifre");
        var dashed = Jinja.Render("{{ strftime_now('%Y-%m-%d %H:%M:%S') }}", new Dictionary<string, object?>());
        if (dashed.Length != 19 || dashed[4] != '-' || dashed[7] != '-') fails.Add($"strftime_now-datetime: '{dashed}'");
        return fails;
    }
}
