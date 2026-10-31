// FILE: Basic.Databases.Rdd.NullDriver.cs
// NullDriver — no-op RDD per testing (XSharp NULLRDD).
// Tutte le operazioni sono no-op, restituisce sempre 0/false/empty.

using System.Collections.Generic;

namespace Basic.Databases.Rdd;

[DatabaseDriver("NULL", Extensions = new string[0], Description = "No-op driver for testing (NULLRDD)")]
public class NullDriver : BaseDriver
{
    public override string DriverName => "NULL";

    public override void Open(DbOpenInfo info) { _isOpen = true; _alias = info.Alias; }
    public override void Close() { _isOpen = false; }

    public override void GoTop() { }
    public override void GoBottom() { }
    public override void GoTo(long n) { }
    public override void Skip(int n) { }
    public override void SkipRaw(int n) { }

    public override long Append(Dictionary<int, object?> fields) => 0;
    public override void Delete() { }
    public override void Recall() { }
    public override void Pack() { }
    public override void Zap() { }
    public override Dictionary<int, object?>? GetCurrentRecord() => null;
    public override void PutCurrentRecord(Dictionary<int, object?> fields) { }

    public override void CreateIndex(DbOrderInfo info) { }
    public override void OpenIndex(string name, string filePath) { }
    public override void CloseIndex(string name) { }
    public override void SetOrder(string name) { }
    public override bool Seek(DbSeekInfo info) => false;
    public override string? CurrentKey() => null;
    public override int CurrentKeyNo() => 0;

    public override bool Lock(long rec) => false;
    public override void Unlock(long rec) { }

    public override object? Info(int infoType, object? param) => null;
}
