// FILE: Interpreter.StdLib.Http.cs
// ReSharper disable InconsistentNaming
// ReSharper disable CheckNamespace

/* ==== GMB-STDLIB-AUDIT-BEGIN ====
 * FILE: Http — namespace 'HTTP'
 * 8 funzioni | 6 comandi | 12 tipi | 0 costanti
 *
 * FUNZIONI (8):
 *   DELETE
 *   DETECTCONTENTTYPE
 *   FETCH
 *   GET
 *   POST
 *   PUT
 *   SAFEFILENAME
 *   SESSIONCREATE
 * COMANDI (6):
 *   CONFIG
 *   FETCH
 *   ROUTE
 *   SERVER
 *   STATIC
 *   USE
 * TIPI (12):
 *   CookieJarType
 *   FetchResultType
 *   FileFieldType
 *   FormFieldType
 *   HttpConfigType
 *   HttpHeadersType
 *   HttpOptionsType
 *   HttpPostedDataType
 *   HttpRequestType
 *   HttpResponseType
 *   NextHandler
 *   WebSocketType
    ==== GMB-STDLIB-AUDIT-END ==== */


namespace Basic;

public sealed partial class StdLib
{
    // ========================================================================
    // --- HTTP NAMESPACE ---
    // ========================================================================

    /// <summary>
    /// Crea e registra tutte le definizioni per il namespace 'HTTP'.
    /// </summary>
    private void CreateHttpNamespace()
    {
        const string namespaceName = "HTTP";
        const SymbolFlags publicFlag = SymbolFlags.IsPublic | SymbolFlags.IsFunction;
        const SymbolFlags noFlags = SymbolFlags.None;
        const SymbolFlags optionalFlag = SymbolFlags.IsOptional;

        RegisterCommand(TokenKind.Http, "HTTP", namespaceName, Parser.ParseHttpStatement);
        RegisterCommand(TokenKind.Fetch, "FETCH", namespaceName, Parser.ParseFetchStatement);

        
        

        var procedures = new ProcedureDefinition[]
        {
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Fetch, "FETCH"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "url"), new StringType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "options"), new HttpOptionsType(), optionalFlag, null, false)
            ], new FetchResultType(), TokenKind.Fetch),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.HttpGet, "GET"),
                [new Parameter(-1, T(TokenKind.Identifier, "url"), new StringType(), noFlags, null, false)],
                new StringType(), TokenKind.HttpGet),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.HttpPost, "POST"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "url"), new StringType(), noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "body"), new StringType(), optionalFlag, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "contentType"), new StringType(), optionalFlag, null, false)
                ], new StringType(), TokenKind.HttpPost),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.HttpPut, "PUT"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "url"), new StringType(), noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "body"), new StringType(), optionalFlag, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "contentType"), new StringType(), optionalFlag, null, false)
                ], new StringType(), TokenKind.HttpPut),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.HttpDelete, "DELETE"),
                [new Parameter(-1, T(TokenKind.Identifier, "url"), new StringType(), noFlags, null, false)],
                new StringType(), TokenKind.HttpDelete),

            // Upload security (2026-09-02): nomi file sicuri + sniff magic-number.
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SafeFileName, "SAFEFILENAME"),
                [new Parameter(-1, T(TokenKind.Identifier, "fileName"), new StringType(), noFlags, null, false)],
                new StringType(), TokenKind.SafeFileName, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.DetectContentType, "DETECTCONTENTTYPE"),
                [new Parameter(-1, T(TokenKind.Identifier, "data"), new InferredType(), noFlags, null, false)],
                new StringType(), TokenKind.DetectContentType, ExecutionKind: NativeExecutionKind.Pure),

            // Sessione: il server CREA la sessione (id+utente/ruolo) e restituisce il cookie.
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SessionCreate, "SESSIONCREATE"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "user"), new StringType(), noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "role"), new StringType(), noFlags, null, false)
                ], new StringType(), TokenKind.SessionCreate),
        };

        // --- AGGIORNAMENTO TIPI ---
        var types = new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase)
        {
            { "HttpOptions", new HttpOptionsType() },
            { "HttpHeaders", new HttpHeadersType() },
            { "HttpConfig", new HttpConfigType() },
            { "CookieJar", new CookieJarType() },
            { "HttpRequest", new HttpRequestType() },
            { "HttpResponse", new HttpResponseType() },
            { "WebSocket", new WebSocketType() },
            { "FetchResult", new FetchResultType() },
            { "NextHandler", new NextHandlerType() },
            // Nuovi tipi per multipart/form-data
            { "HttpPostedData", new HttpPostedDataType() },
            { "FormField", new FormFieldType() },
            { "FileField", new FileFieldType() }
        };

        RegisterTypeSchema(typeof(HttpOptionsType), new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase)
        {
            { "Method", new StringType() },
            { "Body", new StringType() },
            { "Headers", new HttpHeadersType() },
            { "Timeout", new IntegerType() },
            { "UseCookies", new BitType() },
            { "CookieJar", new CookieJarType() },
            { "Username", new StringType() },
            { "Password", new StringType() },
            { "BearerToken", new StringType() },
            { "ProxyUrl", new StringType() },
            { "ProxyUsername", new StringType() },
            { "ProxyPassword", new StringType() },
            { "ClientCertificatePath", new StringType() },
            { "ClientCertificatePassword", new StringType() },
            { "AllowRedirects", new BitType() },
            { "MaxRedirects", new IntegerType() },
            { "AllowInsecureSSL", new BitType() },
            { "BrowserPreset", new StringType() },
            { "PinnedCertificateSha256", new StringType() }
        });

        RegisterTypeSchema(typeof(HttpConfigType), new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase)
        {
            { "Port", new IntegerType() },
            { "RequestTimeoutMs", new IntegerType() },
            { "DefaultRequestTimeout", new IntegerType() },
            { "MaxRequestBodySize", new LongType() },
            { "EnableKeepAlive", new BitType() },
            { "RateLimitRequestsPerMinute", new IntegerType() },
            { "RateLimitBurstSize", new IntegerType() },
            { "CertificatePath", new StringType() },
            { "CertificatePassword", new StringType() },
            { "SecurityHeaders", new BitType() },
            { "AllowedIPs", new StringType() },
            { "BlockedIPs", new StringType() },
            { "Sessions", new BitType() },
            { "SessionFile", new StringType() },
            // Memoizzazione della resa BSP (ms; 0 = disattivata)
            { "BspCacheMs", new IntegerType() }
        });

        RegisterTypeSchema(typeof(CookieJarType),
            new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase),
            new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase)
            {
                { "Clear", new VoidType() }
            });

        RegisterTypeSchema(typeof(WebSocketType), new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase)
        {
            { "OnMessage", new StringType() },
            { "OnClose", new StringType() },
            { "OnError", new StringType() }
        }, new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase)
        {
            { "Send", new VoidType() },
            { "SendBytes", new VoidType() },
            { "Close", new VoidType() }
        });

        RegisterTypeSchema(typeof(HttpHeadersType), 
            new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase)
            {
                { "ContentType", new StringType() },
                { "Authorization", new StringType() },
                { "UserAgent", new StringType() },
                { "Connection", new StringType() }
            },
            new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase)
            {
                { "Get", new StringType() },
                { "Add", new VoidType() }, 
                { "Remove", new BitType() }
            }
        );
        
        // --- AGGIORNAMENTO SCHEMA HttpRequest ---
        RegisterTypeSchema(typeof(HttpRequestType), 
            new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase)
            {
                { "Method", new StringType() },
                { "Path", new StringType() },
                { "QueryString", new StringType() },
                { "Body", new StringType() },
                { "BodyJson", new JsonType() },
                { "BodyBytes", new ArrayType(new ByteType()) },
                { "Headers", new HttpHeadersType() },
                { "Params", new JsonType() },
                { "Query", new JsonType() },
                { "RemoteIp", new StringType() },
                { "SessionId", new StringType() }
            },
            new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase)
            {
                { "ParseMultipartForm", new HttpPostedDataType() },
                { "ParseForm", new JsonType() }
            }
        );

        // HttpResponse (Metodi e proprietà per inviare la risposta)
        RegisterTypeSchema(typeof(HttpResponseType), 
            new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase)
            {
                { "StatusCode", new IntegerType() },
                { "ContentType", new StringType() },
                { "Headers", new HttpHeadersType() }
            },
            new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase)
            {
                { "Send", new VoidType() },
                { "SendJSON", new VoidType() },
                { "SendBytes", new VoidType() },
                { "SendFile", new VoidType() },
                { "SetCookie", new VoidType() },
                { "Redirect", new VoidType() },
                // Z4: SetStatus/SetHeader mancavano dallo schema METODI → il linker li
                // risolveva come FIELD (res.SetStatus(404) → OP_LOAD_FIELD 'SetStatus' +
                // OP_LOAD_ELEMENT, un no-op) invece che come MethodCallExpr → la risposta
                // restava 200 mentre l'assegnazione proprietà res.StatusCode = 404
                // (OP_STORE_FIELD) propagava. Coerenti con i case già presenti in
                // OP_CALL_MEMBER (PCodeProcessor) e col runtime IL.
                { "SetStatus", new VoidType() },
                { "Status", new VoidType() },
                { "SetHeader", new VoidType() }
            }
        );
        
        RegisterTypeSchema(typeof(FetchResultType), 
            new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase)
            {
                { "StatusCode", new LongType() },
                { "Body", new StringType() },
                { "Headers", new HttpHeadersType() },
                { "ContentLength", new LongType() },
                { "Url", new StringType() },
                { "IsSuccess", new BitType() }
            },
            new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase)
            {
                { "SaveToFile", new VoidType() },
                { "GetBodyBytes", new ArrayType(new ByteType()) }
            }
        );

        // --- NUOVI SCHEMI ---
        RegisterTypeSchema(typeof(HttpPostedDataType), 
            new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase)
            {
                { "FormFields", new ArrayType(new FormFieldType()) },
                { "FileFields", new ArrayType(new FileFieldType()) }
            }
        );

        RegisterTypeSchema(typeof(FormFieldType),
            new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase)
            {
                { "Name", new StringType() },
                { "Value", new StringType() }
            }
        );

        RegisterTypeSchema(typeof(FileFieldType),
            new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase)
            {
                { "Name", new StringType() },
                { "FileName", new StringType() },
                { "Data", new ArrayType(new ByteType()) }
            }
        );
        
        RegisterBuiltinNamespace(namespaceName, procedures, types);
    }
}