// FILE: Basic.Database.Native.Tests.IsamOversizedBufferTests
using System.Text;
using Basic.Databases;
using Basic.Databases.Rdd;
using Basic.ISAM;
using Basic.Tools.Isam;

namespace Basic.Database.Native.Tests;

/// <summary>
/// E1.6: <c>RecordMarshaller.DeserializeRecord</c> con buffer SOVRADIMENSIONATO.
/// E' la condizione per cui esiste il parametro <c>length</c> (commit isam-recordread-ai):
/// <c>new MemoryStream(buffer)</c> prende per lunghezza l'array intero, quindi con un buffer da
/// <c>ArrayPool</c> (potenzialmente piu' grande del record) un campo che dichiara una lunghezza
/// oltre la fine del record leggerebbe la coda spazzatura del pool invece di lanciare
/// <see cref="EndOfStreamException"/> — silenziosamente e con dati non deterministici.
/// </summary>
public sealed class IsamOversizedBufferTests
{
    private static (Config.IsamSchema Schema, UserDefinedType Udt, IsamFileHandle Handle, Engine Engine, int RecordSize) OpenArea(string path)
    {
        var fields = new List<Config.IsamFieldSchema>
        {
            new("ID", new Basic.StringType(), 10),
            new("V", new Basic.StringType(), 20),
        };
        var schema = new Config.IsamSchema("R", fields,
            new Config.IndexDefinition("ID", "ID", 10, true, false),
            new List<Config.IndexDefinition>());
        var udt = IsamOfflineToolkit.BuildMinimalUdt(schema);
        var handle = new IsamFileHandle(path, schema, udt, LockingStrategy.IntraProcess);
        var engine = new Engine(handle);
        engine.Evaluator = (expr, ctx, tt) =>
            expr == "__recno__" ? (double)engine.GetCurrentRecordNumber()
            : (ctx != null && ctx.TryGetValue(expr, out var v) ? v : expr);
        return (schema, udt, handle, engine, handle.Header.RecordSize);
    }

    [Fact]
    public void DeserializeRecord_OversizedBufferWithPoisonTail_ReadsOnlyRecordBytes()
    {
        var path = NativeDb.Tmp("oversized_buf.isdb");
        if (File.Exists(path)) File.Delete(path);
        var (schema, udt, handle, engine, recordSize) = OpenArea(path);
        using (handle)
        using (engine)
        {
            var idSym = udt.Fields[0].SymbolId;
            var vSym = udt.Fields[1].SymbolId;
            engine.Append(new UdtInstance(udt, new Dictionary<int, object?> { [idSym] = "T1", [vSym] = "V1" }), engine.Evaluator);
            engine.Go(Config.CursorMoveOperation.Top);

            var exact = new byte[recordSize];
            handle.ReadRecordBytes(engine.CurrentRecordOffset, exact);

            // Risposta del percorso corretto (array di dimensione esatta).
            var correct = RecordMarshaller.DeserializeRecord(schema, exact, udt, handle);
            var correctId = correct.instance.Fields[idSym]?.ToString()?.Trim();
            var correctV = correct.instance.Fields[vSym]?.ToString()?.Trim();
            Assert.Equal("T1", correctId);
            Assert.Equal("V1", correctV);

            // Buffer sovradimensionato con VELENO nella coda (resto di un record precedente).
            var big = new byte[recordSize + 64];
            Array.Copy(exact, big, recordSize);
            for (int i = recordSize; i < big.Length; i++) big[i] = (byte)'Z';

            // Con length: stessi valori del percorso corretto, il veleno NON viene letto.
            var viaLen = RecordMarshaller.DeserializeRecord(schema, big, udt, handle, recordSize);
            Assert.Equal(correctId, viaLen.instance.Fields[idSym]?.ToString()?.Trim());
            Assert.Equal(correctV, viaLen.instance.Fields[vSym]?.ToString()?.Trim());

            // Senza length, il MemoryStream arriva fino alla fine dell'array: su un record
            // VALIDO i valori coincidono (i campi finiscono dentro recordSize), quindi questo
            // assert documenta solo che la lunghezza e' comunque rispettata dal layout.
            var noLen = RecordMarshaller.DeserializeRecord(schema, big, udt, handle);
            Assert.Equal(correctId, noLen.instance.Fields[idSym]?.ToString()?.Trim());
        }

        if (File.Exists(path)) File.Delete(path);
    }

    [Fact]
    public void DeserializeRecord_TruncatedStream_StopsAtLengthInsteadOfReadingPoison()
    {
        var path = NativeDb.Tmp("oversized_trunc.isdb");
        if (File.Exists(path)) File.Delete(path);
        var (schema, udt, handle, engine, recordSize) = OpenArea(path);
        using (handle)
        using (engine)
        {
            var idSym = udt.Fields[0].SymbolId;
            var vSym = udt.Fields[1].SymbolId;
            engine.Append(new UdtInstance(udt, new Dictionary<int, object?> { [idSym] = "T1", [vSym] = "V1" }), engine.Evaluator);
            engine.Go(Config.CursorMoveOperation.Top);

            var exact = new byte[recordSize];
            handle.ReadRecordBytes(engine.CurrentRecordOffset, exact);
            var correctV = RecordMarshaller.DeserializeRecord(schema, exact, udt, handle)
                .instance.Fields[vSym]?.ToString() ?? "";

            // Buffer sovradimensionato con VELENO DENTRO il tratto dichiarato dal record:
            // ci sono solo `truncated` byte reali (finiscono 5 byte DENTRO l'ultimo campo
            // stringa: V dichiara 20 byte, ne trova 15), da `truncated` in poi e' veleno.
            // Il percorso con length=truncated DEVE fermarsi allo stream limitato:
            // BinaryReader.ReadBytes restituisce i 15 byte reali (non lancia mai), quindi la
            // stringa e' un prefisso della V corretta e NON contiene veleno.
            var big = new byte[recordSize + 64];
            int truncated = recordSize - 5;
            for (int i = 0; i < big.Length; i++) big[i] = (byte)'Z';
            Array.Copy(exact, big, truncated);

            var viaLen = RecordMarshaller.DeserializeRecord(schema, big, udt, handle, truncated);
            var shortV = viaLen.instance.Fields[vSym]?.ToString() ?? "";
            Assert.DoesNotContain("Z", shortV);
            Assert.True(correctV.StartsWith(shortV),
                $"atteso prefisso di '{correctV}', trovato '{shortV}'");

            // Stesso array, SENZA length (comportamento storico su array interi): il
            // MemoryStream arriva alla fine dell'array e la V legge anche i 5 byte Z.
            // Questo assert documenta PERCHE' length esiste: senza, il veleno passa silenzioso.
            var noLen = RecordMarshaller.DeserializeRecord(schema, big, udt, handle);
            var poisonedV = noLen.instance.Fields[vSym]?.ToString() ?? "";
            Assert.Contains("Z", poisonedV);
        }

        if (File.Exists(path)) File.Delete(path);
    }

    [Fact]
    public void MemBackedArea_ReadGoesThroughTableReadAt()
    {
        // E1.7: il path MEM: (IsamMemStream.TableReadAt -> ReadSpan, posizionale) e' quello
        // che ReadBytesAtCore usa per primo (FileHandle.cs). Prima di questo test era
        // verificato solo per lettura del codice: nessun minitest copre MEM:.
        const string memPath = "MEM:E17ProbeTable";
        var fields = new List<Config.IsamFieldSchema>
        {
            new("ID", new Basic.StringType(), 10),
            new("V", new Basic.StringType(), 20),
        };
        var schema = new Config.IsamSchema("R", fields,
            new Config.IndexDefinition("ID", "ID", 10, true, false),
            new List<Config.IndexDefinition>());
        IsamFileHandle.CreateNew(memPath, schema, Encoding.UTF8);

        var udt = IsamOfflineToolkit.BuildMinimalUdt(schema);
        using var handle = new IsamFileHandle(memPath, schema, udt, LockingStrategy.IntraProcess);
        using var engine = new Engine(handle);
        engine.Evaluator = (expr, ctx, tt) =>
            expr == "__recno__" ? (double)engine.GetCurrentRecordNumber()
            : (ctx != null && ctx.TryGetValue(expr, out var v) ? v : expr);

        var idSym = udt.Fields[0].SymbolId;
        var vSym = udt.Fields[1].SymbolId;
        engine.Append(new UdtInstance(udt,
            new Dictionary<int, object?> { [idSym] = "M1", [vSym] = "VMEM" }),
            engine.Evaluator);
        engine.Go(Config.CursorMoveOperation.Top);

        var rec = engine.GetCurrentRecord();
        Assert.NotNull(rec);
        Assert.Equal("M1", rec.Fields[idSym]?.ToString()?.Trim());
        Assert.Equal("VMEM", rec.Fields[vSym]?.ToString()?.Trim());
    }
}
