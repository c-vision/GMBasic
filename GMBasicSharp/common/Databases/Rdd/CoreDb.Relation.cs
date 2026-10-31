// FILE: Basic.Databases.Rdd.CoreDb.Relation.cs
// SET RELATION: relazione padre/figlio tra work area.
// Quando il padre si muove, il figlio si riposiziona automaticamente.

using System;
using System.Collections.Generic;

namespace Basic.Databases.Rdd;

public static partial class CoreDb
{
    private sealed class RelationInfo
    {
        public int ParentArea;
        public int ChildArea;
        public string? KeyExpression;
    }

    private static readonly Dictionary<int, List<RelationInfo>> _relations = new();

    /// <summary>
    /// SET RELATION: collega childArea a parentArea tramite keyExpression.
    /// Quando il padre si sposta, il figlio esegue automaticamente SEEK.
    /// </summary>
    public static void SetRelation(DataSession session, int childArea, int parentArea, string keyExpression)
    {
        var child = session.GetWorkArea(childArea)?.Driver;
        var parent = session.GetWorkArea(parentArea)?.Driver;
        if (child == null || parent == null) return;

        child.SetRelation(parent, keyExpression);

        if (!_relations.TryGetValue(parentArea, out var list))
            _relations[parentArea] = list = new List<RelationInfo>();
        list.Add(new RelationInfo { ParentArea = parentArea, ChildArea = childArea, KeyExpression = keyExpression });
    }

    /// <summary>Forza il riposizionamento di tutte le aree figlie dopo un movimento del padre.</summary>
    public static void SyncRelations(DataSession session, int parentArea)
    {
        if (!_relations.TryGetValue(parentArea, out var list)) return;
        var parent = session.GetWorkArea(parentArea)?.Driver;
        if (parent == null) return;

        foreach (var rel in list)
        {
            var child = session.GetWorkArea(rel.ChildArea)?.Driver;
            if (child == null) continue;

            var keyFieldIdx = parent.GetFieldIndex(rel.KeyExpression!);
            if (keyFieldIdx < 0) continue;

            var keyValue = parent.GetFieldValue(keyFieldIdx);
            if (keyValue == null) continue;

            child.Seek(new DbSeekInfo { Key = keyValue });
        }
    }

    /// <summary>Cancella tutte le relazioni per un'area (alla CLOSE).</summary>
    public static void ClearRelations(int area)
    {
        _relations.Remove(area);
        // Rimuovi area anche dalle liste figlie
        foreach (var list in _relations.Values)
            list.RemoveAll(r => r.ChildArea == area || r.ParentArea == area);
    }

    /// <summary>Cancella tutte le relazioni.</summary>
    public static void ClearAllRelations() => _relations.Clear();

    // ═══ Varianti navigazione con sync relazioni ═══

    public static void GoTopWithRelations(DataSession s, int area)
    {
        GoTopEv(s, area);
        SyncRelations(s, area);
    }

    public static void GoBottomWithRelations(DataSession s, int area)
    {
        GoBottomEv(s, area);
        SyncRelations(s, area);
    }

    public static void SkipWithRelations(DataSession s, int area, int n)
    {
        SkipEv(s, area, n);
        SyncRelations(s, area);
    }

    public static bool SeekWithRelations(DataSession s, int area, DbSeekInfo info)
    {
        var result = SeekEv(s, area, info);
        if (result) SyncRelations(s, area);
        return result;
    }
}
