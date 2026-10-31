// FILE: CommonLib.Http.HttpPostedData.cs

/* 
 * HttpPostedData.cs: dati parsati da una richiesta multipart/form-data (campi di testo e file)
 * 
 * RECORD/CLASSI:
 * - FormField(Name, Value): campo di testo in un form multipart
 * - FileField(Name, FileName, ContentType, Data): file caricato in un form multipart
 * - HttpPostedData: contenitore per campi e file parsati
 */

namespace CommonLib.Http;

/// <summary>Rappresenta un campo di testo semplice all'interno di un form multipart.</summary>
/// <param name="Name">Il nome del campo (attributo 'name' dell'input HTML).</param>
/// <param name="Value">Il valore testuale del campo.</param>
public sealed record FormField(string Name, string Value);

/// <summary>Rappresenta un file caricato all'interno di un form multipart.</summary>
/// <param name="Name">Il nome del campo (attributo 'name' dell'input file).</param>
/// <param name="FileName">Il nome originale del file fornito dal client.</param>
/// <param name="ContentType">Il tipo MIME del file (es. "image/jpeg").</param>
/// <param name="Data">Byte CRUDI del file caricato. Non è uno Stream: lo schema canonico
/// (StdLib.Http FileFieldType) dichiara <c>Data AS ARRAY OF BYTE</c> e i backend C
/// (VM/C gmb_net_http_parse_multipart) espongono byte grezzi — PUT/LEN
/// gestiti devono vedere la stessa cosa (parità 4 backend, sample HttpMultiPart).</param>
public sealed record FileField(string Name, string FileName, string ContentType, byte[] Data);

/// <summary>
/// Contenitore per i dati parsati da una richiesta multipart/form-data.
/// Fornisce accesso strutturato ai campi di testo (FormFields) e ai file caricati (FileFields).
/// Creato da RequestBodyParser.ParseMultipartData().
/// </summary>
public sealed class HttpPostedData
{
    /// <summary>Lista dei campi di testo parsati dalla richiesta.</summary>
    public IReadOnlyList<FormField> FormFields { get; }

    /// <summary>Lista dei file caricati parsati dalla richiesta.</summary>
    public IReadOnlyList<FileField> FileFields { get; }

    /// <summary>Crea un nuovo contenitore con i campi e i file parsati.</summary>
    public HttpPostedData(List<FormField> formFields, List<FileField> fileFields)
    {
        FormFields = formFields.AsReadOnly();
        FileFields = fileFields.AsReadOnly();
    }
}
