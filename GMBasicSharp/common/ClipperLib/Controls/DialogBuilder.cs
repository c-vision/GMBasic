using CommonLib.Consoles.Tui;
using CommonLib.Screens;
namespace ClipperLib.Controls;

/// <summary>Risultato di una validazione.</summary>
public enum Validation { Valid, Invalid }

/// <summary>Una possibile scelta in una lista (select / multi_select).</summary>
public class Choice
{
    public string? Text { get; }
    public bool IsSeparator { get; }
    public Choice(string text) { Text = text; IsSeparator = false; }
    private Choice() { IsSeparator = true; }
    public static Choice Separator(string text = "──────────────") => new();
}

/// <summary>Una risposta di tipo lista.</summary>
public class ListItem
{
    public int Index { get; }
    public string Text { get; }
    public ListItem(int index, string text) { Index = index; Text = text; }
}

/// <summary>Una risposta di tipo expand.</summary>
public class ExpandItem
{
    public char Key { get; }
    public string Text { get; }
    public ExpandItem(char key, string text) { Key = key; Text = text; }
}

/// <summary>I diversi tipi di risposta restituiti dalle domande.</summary>
public class Answer
{
    public string? AsString { get; }
    public long? AsInt { get; }
    public double? AsFloat { get; }
    public bool? AsBool { get; }
    public ListItem? AsListItem { get; }
    public ListItem[]? AsListItems { get; }
    public ExpandItem? AsExpandItem { get; }

    private Answer(string? s, long? i, double? f, bool? b, ListItem? li, ListItem[]? lis, ExpandItem? ei)
    { AsString = s; AsInt = i; AsFloat = f; AsBool = b; AsListItem = li; AsListItems = lis; AsExpandItem = ei; }

    public static Answer FromString(string v) => new(v, null, null, null, null, null, null);
    public static Answer FromInt(long v) => new(null, v, null, null, null, null, null);
    public static Answer FromFloat(double v) => new(null, null, v, null, null, null, null);
    public static Answer FromBool(bool v) => new(null, null, null, v, null, null, null);
    public static Answer FromListItem(ListItem v) => new(null, null, null, null, v, null, null);
    public static Answer FromListItems(ListItem[] v) => new(null, null, null, null, null, v, null);
    public static Answer FromExpandItem(ExpandItem v) => new(null, null, null, null, null, null, v);
}

/// <summary>Collezione di risposte indicizzate per nome.</summary>
public class Answers
{
    private readonly Dictionary<string, Answer> map = [];

    public void Insert(string name, Answer answer) => map[name] = answer;
    public Answer? Get(string name) => map.TryGetValue(name, out var a) ? a : null;
    public bool Contains(string name) => map.ContainsKey(name);
    public int Count => map.Count;
    public bool IsEmpty => map.Count == 0;
    public IEnumerable<(string name, Answer answer)> Iter() =>
        map.Select(kv => (kv.Key, kv.Value));
}

/// <summary>Tipi di domanda supportati.</summary>
public enum QuestionKind { Input, Password, Confirm, Int, Float, Select, MultiSelect, Expand }

/// <summary>Domanda dichiarativa.</summary>
public class Question
{
    public string Name { get; }
    public QuestionKind Kind { get; }
    public string Message { get; set; }
    public string? DefaultStr { get; set; }
    public bool? DefaultBool { get; set; }
    public long? DefaultInt { get; set; }
    public double? DefaultFloat { get; set; }
    public int? DefaultIndex { get; set; }
    public char? DefaultKey { get; set; }
    public char? Mask { get; set; }
    public bool AskIfAnswered { get; set; }
    public Func<bool>? When { get; set; }
    public Func<string, string>? Filter { get; set; }
    public Func<string, string>? Transform { get; set; }
    public Func<string, string?>? Validate { get; set; }

    public List<Choice> Choices { get; } = [];
    public List<(string text, bool checked_)> MultiSelectChoices { get; } = [];
    public List<(char key, string text)> ExpandChoices { get; } = [];

    public Question(string name, QuestionKind kind)
    {
        Name = name;
        Kind = kind;
        Message = $"{name}: ";
    }

    public Question WithMessage(string msg) { Message = msg; return this; }
    public Question WithDefault(string s) { DefaultStr = s; return this; }
    public Question WithDefault(bool b) { DefaultBool = b; return this; }
    public Question WithDefault(long i) { DefaultInt = i; return this; }
    public Question WithDefault(double f) { DefaultFloat = f; return this; }
    public Question WithDefaultIndex(int i) { DefaultIndex = i; return this; }
    public Question WithDefaultKey(char k) { DefaultKey = k; return this; }

    public Question WithMask(char m) { Mask = m; return this; }
    public Question WithAskIfAnswered(bool v) { AskIfAnswered = v; return this; }
    public Question WithWhen(Func<bool> when) { When = when; return this; }
    public Question WithFilter(Func<string, string> f) { Filter = f; return this; }
    public Question WithTransform(Func<string, string> t) { Transform = t; return this; }
    public Question WithValidate(Func<string, string?> v) { Validate = v; return this; }

    public void AddChoice(string text) => Choices.Add(new Choice(text));
    public void AddChoice(string text, bool checked_) => MultiSelectChoices.Add((text, checked_));
    public void AddExpandChoice(char key, string text) => ExpandChoices.Add((key, text));

    public bool ShouldAsk(Answers answers)
    {
        if (!AskIfAnswered && answers.Contains(Name)) return false;
        return When?.Invoke() ?? true;
    }

    public Answer? Resolve(string raw, Answers answers)
    {
        if (Kind is QuestionKind.Input or QuestionKind.Password)
        {
            var value = string.IsNullOrEmpty(raw) ? DefaultStr ?? "" : raw;
            if (Validate != null)
            {
                var err = Validate(value);
                if (err != null) return null;
            }
            if (Filter != null) value = Filter(value);
            return Answer.FromString(value);
        }
        if (Kind == QuestionKind.Confirm)
        {
            var val = raw.ToLowerInvariant() switch
            {
                "y" or "yes" => true,
                "n" or "no" => false,
                "" when DefaultBool.HasValue => DefaultBool.Value,
                _ => throw new InvalidOperationException("inserire y o n"),
            };
            return Answer.FromBool(val);
        }
        if (Kind == QuestionKind.Int)
        {
            var val = string.IsNullOrEmpty(raw)
                ? DefaultInt ?? throw new InvalidOperationException("numero richiesto")
                : long.Parse(raw);
            return Answer.FromInt(val);
        }
        if (Kind == QuestionKind.Float)
        {
            var val = string.IsNullOrEmpty(raw)
                ? DefaultFloat ?? throw new InvalidOperationException("numero richiesto")
                : double.Parse(raw);
            return Answer.FromFloat(val);
        }
        if (Kind == QuestionKind.Select)
        {
            var idx = Choices.FindIndex(c => c.Text == raw);
            if (idx < 0) return null;
            return Answer.FromListItem(new ListItem(idx, raw));
        }
        if (Kind == QuestionKind.MultiSelect)
        {
            var parts = raw.Split(',', StringSplitOptions.TrimEntries);
            var items = parts.Select(p =>
            {
                var idx = int.Parse(p);
                return new ListItem(idx, MultiSelectChoices[idx].text);
            }).ToArray();
            return Answer.FromListItems(items);
        }
        if (Kind == QuestionKind.Expand)
        {
            var key = raw.FirstOrDefault();
            var found = ExpandChoices.Find(e => e.key == key);
            return found == default ? null
                : Answer.FromExpandItem(new ExpandItem(key, found.text));
        }
        return null;
    }
}
