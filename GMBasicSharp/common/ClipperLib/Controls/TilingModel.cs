using CommonLib.Consoles.Tui;
using CommonLib.Screens;
using ClipperLib.Clipper;

namespace ClipperLib.Controls;

/// <summary>Orientamento di uno split: come due figli condividono l'area.</summary>
public enum Split
{
    /// Larghezza divisa: figlio sinistro / figlio destro.
    Horizontal,
    /// Altezza divisa: figlio superiore / figlio inferiore.
    Vertical
}

/// <summary>Nodo dell'albero di tiling (arena).</summary>
internal class TilingNode
{
    public int Id;
    public ScreenRect Rect;
    public Split? Split;          // null = foglia
    public double Ratio = 0.5;   // frazione per il primo figlio (0..1)
    public int Offset;            // cell offset cumulativo
    public int?[] Children = [null, null];
    public int? View;             // payload (es. window id)
    public bool Removed;
}

/// <summary>Modello di tiling ad albero binario puro: split-ratio, focus history, zoom, resize.</summary>
public class TilingModel
{
    private readonly List<TilingNode> nodes = [];
    private int root;

    /// <summary>Crea un modello vuoto (unico nodo foglia, nessuna view).</summary>
    public TilingModel()
    {
        nodes.Add(new TilingNode { Id = 0 });
        root = 0;
    }

    public int Root => root;

    public int?[] Children(int id) =>
        nodes.Find(n => n.Id == id)?.Children ?? [null, null];

    public int? ViewOf(int id) =>
        nodes.Find(n => n.Id == id)?.View;

    public double RatioOf(int id) =>
        nodes.Find(n => n.Id == id)?.Ratio ?? 0.5;

    /// <summary>Assegna un payload di view a un nodo.</summary>
    public void SetView(int id, int view)
    {
        var n = nodes.Find(x => x.Id == id);
        if (n != null) n.View = view;
    }

    /// <summary>Divide una foglia in un contenitore con due figli.</summary>
    public int? SplitLeaf(int id, Split direction, double ratio)
    {
        ratio = Math.Clamp(ratio, 0.05, 0.95);
        var node = nodes.Find(n => n.Id == id);
        if (node == null || node.Removed || node.Split.HasValue)
            return null;

        var oldView = node.View;
        var oldRect = node.Rect;
        node.Split = direction;
        node.Ratio = ratio;
        node.Offset = 0;
        node.View = null;

        var c0 = nodes.Count;
        nodes.Add(new TilingNode { Id = c0, Rect = oldRect, View = oldView });
        var c1 = nodes.Count;
        nodes.Add(new TilingNode { Id = c1, Rect = oldRect });
        node.Children = [c0, c1];
        return c1;
    }

    /// <summary>Rimuove una foglia: promuove il sibling al suo posto.</summary>
    public bool Remove(int id)
    {
        var parent = ParentOf(id);
        if (parent == null) return false;

        var n = nodes.Find(x => x.Id == parent.Value);
        if (n == null) return false;

        var sib = n.Children[0] == id ? n.Children[1]!.Value : n.Children[0]!.Value;

        var gp = ParentOf(parent.Value);
        if (gp.HasValue)
        {
            var gpn = nodes.Find(x => x.Id == gp.Value);
            if (gpn != null)
            {
                if (gpn.Children[0] == parent) gpn.Children[0] = sib;
                else if (gpn.Children[1] == parent) gpn.Children[1] = sib;
            }
        }
        else
        {
            root = sib;
        }

        n.Removed = true;
        var target = nodes.Find(x => x.Id == id);
        if (target != null) target.Removed = true;
        return true;
    }

    private int? ParentOf(int id) =>
        nodes.Find(n => !n.Removed && (n.Children[0] == id || n.Children[1] == id))?.Id;

    /// <summary>Imposta il ratio di split del contenitore che contiene il nodo.</summary>
    public void SetRatio(int id, double ratio)
    {
        ratio = Math.Clamp(ratio, 0.05, 0.95);
        var p = ParentOf(id);
        if (p.HasValue)
        {
            var n = nodes.Find(x => x.Id == p.Value);
            if (n != null) n.Ratio = ratio;
        }
    }

    /// <summary>Sposta il divisore di delta celle.</summary>
    public void MoveOffset(int id, int delta)
    {
        var p = ParentOf(id);
        if (!p.HasValue) return;
        var node = nodes.Find(x => x.Id == p.Value);
        if (node == null || !node.Split.HasValue) return;

        var maxOff = node.Split.Value switch
        {
            Split.Horizontal => (int)(node.Rect.W * (1.0 - node.Ratio)) - 1,
            Split.Vertical => (int)(node.Rect.H * (1.0 - node.Ratio)) - 1,
            _ => 0
        };
        node.Offset = Math.Clamp(node.Offset + delta, -maxOff, maxOff);
    }

    /// <summary>Calcola i rettangoli per ogni nodo foglia visibile.</summary>
    public List<(int nodeId, ScreenRect rect)> Layout(ScreenRect area)
    {
        var outList = new List<(int, ScreenRect)>();
        LayoutRec(root, area, outList);
        return outList;
    }

    private void LayoutRec(int id, ScreenRect area, List<(int, ScreenRect)> outList)
    {
        var node = nodes.Find(n => n.Id == id);
        if (node == null || node.Removed) return;

        if (!node.Split.HasValue)
        {
            outList.Add((id, area));
            return;
        }

        var children = node.Children;
        if (children[0] == null || children[1] == null) return;

        var off = node.Offset;
        var c0 = children[0]!.Value;
        var c1 = children[1]!.Value;

        if (node.Split.Value == Split.Horizontal)
        {
            var lw = Math.Max(1, Math.Clamp((int)(area.W * node.Ratio) + off, 1, area.W - 1));
            var rw = Math.Max(1, area.W - lw);
            LayoutRec(c0, new ScreenRect(area.X, area.Y, lw, area.H), outList);
            LayoutRec(c1, new ScreenRect(area.X + lw, area.Y, rw, area.H), outList);
        }
        else
        {
            var th = Math.Max(1, Math.Clamp((int)(area.H * node.Ratio) + off, 1, area.H - 1));
            var bh = Math.Max(1, area.H - th);
            LayoutRec(c0, new ScreenRect(area.X, area.Y, area.W, th), outList);
            LayoutRec(c1, new ScreenRect(area.X, area.Y + th, area.W, bh), outList);
        }
    }
}
