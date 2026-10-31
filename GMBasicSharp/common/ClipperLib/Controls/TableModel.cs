using CommonLib.Consoles.Tui;
using CommonLib.Screens;
namespace ClipperLib.Controls;

/// <summary>Allineamento orizzontale di una colonna.</summary>
public enum HAlign { Left, Center, Right }

/// <summary>Giustifica text su width colonne secondo align.</summary>
public static class TextAlign
{
    public static string Align(string text, int width, HAlign align)
    {
        if (text.Length > width) return text[..width];
        if (text.Length == width || width <= 0) return text;
        var pad = width - text.Length;
        return align switch
        {
            HAlign.Right => new string(' ', pad) + text,
            HAlign.Center => new string(' ', pad / 2) + text + new string(' ', pad - pad / 2),
            _ => text + new string(' ', pad),
        };
    }
}

/// <summary>Interfaccia per estrarre/confrontare elementi di una tabella.</summary>
public interface ITableViewItem<H>
{
    string ToColumn(H column);
    int CompareTo(ITableViewItem<H> other, H column);
}

/// <summary>Ordina una lista in-place per colonna.</summary>
public static class TableSorter
{
    public static void SortByColumn<T, H>(List<T> rows, H column, bool ascending)
        where T : ITableViewItem<H>
    {
        rows.Sort((a, b) =>
        {
            var cmp = a.CompareTo(b, column);
            return ascending ? cmp : -cmp;
        });
    }
}

/// <summary>Descrizione di una colonna.</summary>
public class Column
{
    public string Name { get; set; }
    public int Index { get; set; }
    public int? Width { get; set; }
    public HAlign Align { get; set; } = HAlign.Left;

    public Column(string name, int index) { Name = name; Index = index; }
    public Column WithWidth(int w) { Width = w; return this; }
    public Column WithAlign(HAlign a) { Align = a; return this; }
}

/// <summary>Insieme ordinato di colonne.</summary>
public class Columns
{
    private readonly List<Column> inner = [];

    public int Add(string name)
    {
        var idx = inner.Count;
        inner.Add(new Column(name, idx));
        return idx;
    }

    public int AddWithIndex(string name, int index)
    {
        inner.Add(new Column(name, index));
        return index;
    }

    public bool Remove(int i)
    {
        if (i < 0 || i >= inner.Count) return false;
        inner.RemoveAt(i);
        for (var j = i; j < inner.Count; j++)
            inner[j].Index = inner[j].Index - 1;
        return true;
    }

    public bool Insert(int i, string name)
    {
        if (i < 0 || i > inner.Count) return false;
        inner.Insert(i, new Column(name, i));
        for (var j = i + 1; j < inner.Count; j++)
            inner[j].Index++;
        return true;
    }

    public bool Reorder(int from, int to)
    {
        if (from < 0 || from >= inner.Count || to < 0 || to >= inner.Count) return false;
        var col = inner[from];
        inner.RemoveAt(from);
        inner.Insert(to, col);
        for (var i = 0; i < inner.Count; i++)
            inner[i].Index = i;
        return true;
    }

    public int Count => inner.Count;
    public bool IsEmpty => inner.Count == 0;
    public Column? Get(int i) => i >= 0 && i < inner.Count ? inner[i] : null;
    public IEnumerable<Column> Iter() => inner;
}
