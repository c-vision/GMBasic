// Test del percorso SPEED che BASIC usa: creare una tabella da sorgente (SPEED USE ... OF Type)
// e poi RIAPRIRE il database.
//
// Il bug qui e' UNO solo, e non e' la serializzazione: lo schema su disco funziona, perche'
// DataType dichiara [JsonDerivedType] e System.Text.Json lo polymorphizza da solo (e infatti i
// manifest esistenti sono gia' in quel formato). Il confronto dello schema, invece, era per
// riferimento: Fields e SecondaryKeys sono List, List non sovrascrive Equals, e i DataType
// sono nodi AST che portano Position e SourceToken. Manifest.CreateTable rigettava quindi
// SEMPRE la riapertura con "Lo schema del TYPE 'X' non corrisponde a quello memorizzato".
//
// Il percorso RESTO non e' coperto dai test: il driver RDD evita CreateTable in riapertura
// (usa la tabella gia' presente), quindi il ramo dell'uguaglianza restava scoperto.

using System.Text;
using System.Text.Json;
using Basic;
using Basic.Databases.Speed;
using SpeedSchema = Basic.Databases.Speed.Config.SpeedSchema;
using SpeedFieldSchema = Basic.Databases.Speed.Config.SpeedFieldSchema;
using SpeedIndexDefinition = Basic.Databases.Speed.Config.SpeedIndexDefinition;
using SpeedDatabase = Basic.Databases.Speed.Database;
using Xunit;

namespace Basic.Database.Native.Tests;

public sealed class SpeedSchemaManifestTests : IDisposable
{
    private readonly string _dir;

    public SpeedSchemaManifestTests()
    {
        _dir = Path.Combine(AppContext.BaseDirectory, "work", nameof(SpeedSchemaManifestTests));
        if (Directory.Exists(_dir))
            Directory.Delete(_dir, recursive: true);
        Directory.CreateDirectory(_dir);
    }

    public void Dispose()
    {
        if (Directory.Exists(_dir))
            Directory.Delete(_dir, recursive: true);
    }

    /// <summary>Schema costruito come fa BASIC: i tipi vengono dal tipo dichiarato nel sorgente.</summary>
    private static SpeedSchema SchemaDaSorgente(string tableName) => new(
        TypeName: tableName,
        Fields:
        [
            new SpeedFieldSchema("CODICE", new StringType(8), 8),
            new SpeedFieldSchema("IMPORTO", new LongType(), 8),
        ],
        PrimaryKey: new SpeedIndexDefinition("PK", "CODICE", new StringType(8), 8, true, false),
        SecondaryKeys: []);

    [Fact]
    public void Lo_schema_sopravvive_al_round_trip_sul_formato_reale_del_manifest()
    {
        // il manifest usa la serializzazione di DEFAULT: e' [JsonDerivedType] su DataType a
        // polymorphicizzarlo, quindi qui non serve nessun converter
        var schema = SchemaDaSorgente("ARTICOLI");
        var json = JsonSerializer.Serialize(schema);
        Assert.Contains("$type", json);

        var riletto = JsonSerializer.Deserialize<SpeedSchema>(json);
        Assert.NotNull(riletto);
        Assert.Equal("ARTICOLI", riletto!.TypeName);
        Assert.Equal("STRING * 8", riletto.Fields[0].DataType.ToString());
        Assert.Equal("LONG", riletto.Fields[1].DataType.ToString());
        Assert.Equal(8, riletto.PrimaryKey.KeyLength);
    }

    [Fact]
    public void Due_schemi_con_lo_stesso_contenuto_sono_uguali_anche_se_hanno_Position_diverse()
    {
        // e' il caso reale: lo schema arriva dal sorgente BASIC (Position valorizzate) e quello
        // salvato sul disco ha Position a zero. L'uguaglianza del record le confronterebbe.
        var daSorgente = SchemaDaSorgente("ARTICOLI");
        var json = JsonSerializer.Serialize(daSorgente);
        var daDisco = JsonSerializer.Deserialize<SpeedSchema>(json)!;

        Assert.True(Config.SchemaEquals(daSorgente, daSorgente));
        Assert.True(Config.SchemaEquals(daSorgente, daDisco));
        Assert.True(Config.SchemaEquals(daDisco, daSorgente));
    }

    [Fact]
    public void Uno_schema_diverso_non_e_uguale()
    {
        var a = SchemaDaSorgente("ARTICOLI");
        var b = SchemaDaSorgente("ARTICOLI");
        b.Fields[1] = new SpeedFieldSchema("IMPORTO", new DoubleType(), 8);
        Assert.False(Config.SchemaEquals(a, b));
    }

    [Fact]
    public void Il_database_si_riapre_e_i_dati_restano_leggibili()
    {
        // il ciclo completo del percorso BASIC: crea, scrive, riapre con lo stesso schema
        var schema = SchemaDaSorgente("ARTICOLI");
        using (var db = new SpeedDatabase(_dir))
        {
            var table = db.CreateTable(schema);
            db.Set(table.Id, Encoding.UTF8.GetBytes("A100"), Encoding.UTF8.GetBytes("tastiera"));
            db.Commit();
        }

        using (var db = new SpeedDatabase(_dir))
        {
            // prima del fix questo lanCIava InvalidOperationException
            var table = db.CreateTable(schema);
            Assert.Equal("STRING * 8", table.Schema.Fields[0].DataType.ToString());
            Assert.Equal("LONG", table.Schema.Fields[1].DataType.ToString());
            Assert.Equal("tastiera", Encoding.UTF8.GetString(db.Get(table.Id, Encoding.UTF8.GetBytes("A100"))!));
        }
    }

    [Fact]
    public void Lo_schema_senza_tipo_del_motore_chiave_valore_va_trattato_a_se()
    {
        // l'area chiave/valore che crea il backend usa DataType null: due schemi null sono
        // uguali, ma uno con tipo e uno senza no
        var senza = new SpeedSchema("KV",
            [new SpeedFieldSchema("__key__", null!, 0)],
            new SpeedIndexDefinition("PK", "__key__", null!, 8, true, false),
            []);
        var uguale = new SpeedSchema("KV",
            [new SpeedFieldSchema("__key__", null!, 0)],
            new SpeedIndexDefinition("PK", "__key__", null!, 8, true, false),
            []);
        var tipizzato = new SpeedSchema("KV",
            [new SpeedFieldSchema("__key__", new StringType(8), 8)],
            new SpeedIndexDefinition("PK", "__key__", new StringType(8), 8, true, false),
            []);

        Assert.True(Config.SchemaEquals(senza, uguale));
        Assert.False(Config.SchemaEquals(senza, tipizzato));
    }

    // =====================================================================
    // La validazione deve rifiutare solo le divergenze VERE. Ogni tolleranza
    // qui sotto e' motivata dal comportamento del motore, non da comodo.
    // =====================================================================

    [Fact]
    public void Tolleranza_al_caso_dei_nomi_perche_il_motore_cerca_anche_senza_caso()
    {
        // Manifest tiene le tabelle in un dizionario OrdinalIgnoreCase e cerca gli indici
        // con OrdinalIgnoreCase: un programma che riapre con nomi diversi per caso sta
        // indicando la stessa tabella e non deve essere respinto
        var a = SchemaDaSorgente("ARTICOLI");
        var b = new SpeedSchema("articoli",
            [
                new SpeedFieldSchema("codice", new StringType(), 8),
                new SpeedFieldSchema("IMPORTO", new LongType(), 8),
            ],
            new SpeedIndexDefinition("pk", "CODICE", new StringType(), 8, true, false),
            []);
        Assert.True(Config.SchemaEquals(a, b));
        Assert.True(Config.SchemaEquals(b, a));
    }

    [Fact]
    public void Tolleranza_al_tipo_senza_dimensione_perche_la_larghezza_e_un_campo_a_se()
    {
        // la larghezza sta in Length: StringType() e StringType(8) con Length 8 sono lo
        // stesso campo. Confrontare la formattazione del tipo ("STRING * 8") rigetterebbe
        // una riapertura perfettamente valida
        var senza = new SpeedSchema("T", [new SpeedFieldSchema("A", new StringType(), 8)],
            new SpeedIndexDefinition("PK", "A", new StringType(), 8, true, false), []);
        var con = new SpeedSchema("T", [new SpeedFieldSchema("A", new StringType(8), 8)],
            new SpeedIndexDefinition("PK", "A", new StringType(8), 8, true, false), []);
        Assert.True(Config.SchemaEquals(senza, con));
    }

    [Fact]
    public void Tolleranza_a_KeyLength_dichiarata_che_il_motore_deriva()
    {
        // chi crea la tabella puo' non dichiarare KeyLength (0): il motore la deriva dal
        // campo. Confrontare 0 contro 32 sarebbe una divergenza inventata
        var dichiarata = new SpeedSchema("T", [new SpeedFieldSchema("A", new StringType(), 32)],
            new SpeedIndexDefinition("PK", "A", new StringType(), 32, true, false), []);
        var derivata = new SpeedSchema("T", [new SpeedFieldSchema("A", new StringType(), 32)],
            new SpeedIndexDefinition("PK", "A", new StringType(), 0, true, false), []);
        Assert.True(Config.SchemaEquals(dichiarata, derivata));
        Assert.True(Config.SchemaEquals(derivata, dichiarata));
    }

    [Fact]
    public void Tolleranza_all_ordine_degli_indici_secondari()
    {
        var i1 = new SpeedIndexDefinition("IX_A", "A", new StringType(), 8, false, false);
        var i2 = new SpeedIndexDefinition("IX_B", "B", new StringType(), 8, false, false);
        var a = new SpeedSchema("T", [new SpeedFieldSchema("A", new StringType(), 8)],
            new SpeedIndexDefinition("PK", "A", new StringType(), 8, true, false), [i1, i2]);
        var b = new SpeedSchema("T", [new SpeedFieldSchema("A", new StringType(), 8)],
            new SpeedIndexDefinition("PK", "A", new StringType(), 8, true, false), [i2, i1]);
        Assert.True(Config.SchemaEquals(a, b));
    }

    [Fact]
    public void Tolleranza_alla_query_di_una_vista_senza_righi_e_senza_caso()
    {
        var a = new SpeedSchema("V", [], new SpeedIndexDefinition("PK", "ID", new StringType(), 8, true, false),
            [], ViewDefinitionQuery: "select id from t");
        var b = new SpeedSchema("V", [], new SpeedIndexDefinition("PK", "ID", new StringType(), 8, true, false),
            [], ViewDefinitionQuery: "  SELECT id FROM T  ");
        Assert.True(Config.SchemaEquals(a, b));
    }

    // ─── e queste divergenze restano rifiutate ───

    [Fact]
    public void Divergenza_un_campo_diverso_e_rifiutata()
    {
        var a = SchemaDaSorgente("ARTICOLI");
        var b = new SpeedSchema("ARTICOLI",
            [new SpeedFieldSchema("CODICE", new StringType(), 8)],
            new SpeedIndexDefinition("PK", "CODICE", new StringType(), 8, true, false), []);
        Assert.False(Config.SchemaEquals(a, b));
    }

    [Fact]
    public void Divergenza_una_lunghezza_diversa_e_rifiutata()
    {
        var a = SchemaDaSorgente("ARTICOLI");
        var b = new SpeedSchema("ARTICOLI",
            [
                new SpeedFieldSchema("CODICE", new StringType(), 16), // era 8
                new SpeedFieldSchema("IMPORTO", new LongType(), 8),
            ],
            new SpeedIndexDefinition("PK", "CODICE", new StringType(), 16, true, false), []);
        Assert.False(Config.SchemaEquals(a, b));
    }

    [Fact]
    public void Divergenza_un_tipo_diverso_e_rifiutata()
    {
        var a = SchemaDaSorgente("ARTICOLI");
        var b = new SpeedSchema("ARTICOLI",
            [
                new SpeedFieldSchema("CODICE", new StringType(), 8),
                new SpeedFieldSchema("IMPORTO", new StringType(), 8), // era LongType
            ],
            new SpeedIndexDefinition("PK", "CODICE", new StringType(), 8, true, false), []);
        Assert.False(Config.SchemaEquals(a, b));
    }

    [Fact]
    public void Divergenza_l_ordine_dei_campi_e_rifiutato_perche_fissa_gli_offset()
    {
        var a = SchemaDaSorgente("ARTICOLI");
        var b = new SpeedSchema("ARTICOLI",
            [
                new SpeedFieldSchema("IMPORTO", new LongType(), 8),
                new SpeedFieldSchema("CODICE", new StringType(), 8),
            ],
            new SpeedIndexDefinition("PK", "CODICE", new StringType(), 8, true, false), []);
        Assert.False(Config.SchemaEquals(a, b));
    }

    [Fact]
    public void Divergenza_una_chiave_primaria_non_unica_e_rifiutata()
    {
        var a = SchemaDaSorgente("ARTICOLI");
        var b = new SpeedSchema("ARTICOLI",
            [
                new SpeedFieldSchema("CODICE", new StringType(), 8),
                new SpeedFieldSchema("IMPORTO", new LongType(), 8),
            ],
            new SpeedIndexDefinition("PK", "CODICE", new StringType(), 8, IsUnique: false, false), []);
        Assert.False(Config.SchemaEquals(a, b));
    }

    [Fact]
    public void Divergenza_una_query_di_vista_diversa_e_rifiutata()
    {
        var a = new SpeedSchema("V", [], new SpeedIndexDefinition("PK", "ID", new StringType(), 8, true, false),
            [], ViewDefinitionQuery: "select id from t");
        var b = new SpeedSchema("V", [], new SpeedIndexDefinition("PK", "ID", new StringType(), 8, true, false),
            [], ViewDefinitionQuery: "select id from altre");
        Assert.False(Config.SchemaEquals(a, b));
    }

    [Fact]
    public void Divergenza_una_tabella_diversa_e_rifiutata()
    {
        Assert.False(Config.SchemaEquals(SchemaDaSorgente("ARTICOLI"), SchemaDaSorgente("ALTRI")));
    }

}
