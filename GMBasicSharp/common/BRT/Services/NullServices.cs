// FILE: Basic.Runtime.Services.NullServices.cs

namespace Basic.Runtime.Services;

public class NullNetworkService : INetworkService
{
    public long TcpConnect(string host, int port) => throw new NotSupportedException("Network disabled.");
    public void TcpClose(long handle) { }
    public void TcpSend(long handle, byte[] data) { }
    public byte[]? TcpReceive(long handle, int count) => null;
    public byte[]? TcpPeek(long handle, int count) => null;
    public long TcpListen(string host, int port) => -1;
    public long TcpAccept(long listener) => -1;
    public ValueTask<long> TcpConnectAsync(string host, int port, CancellationToken cancellationToken) => ValueTask.FromException<long>(new NotSupportedException("Network disabled."));
    public ValueTask TcpSendAsync(long handle, byte[] data, CancellationToken cancellationToken) => ValueTask.CompletedTask;
    public ValueTask<byte[]?> TcpReceiveAsync(long handle, int count, CancellationToken cancellationToken) => ValueTask.FromResult<byte[]?>(null);
    public ValueTask<long> TcpAcceptAsync(long listener, CancellationToken cancellationToken) => ValueTask.FromResult(-1L);
    public ValueTask<byte[]?> TcpPeekAsync(long handle, int count, CancellationToken cancellationToken) => ValueTask.FromResult<byte[]?>(null);
    public long UdpOpen(int port) => -1;
    public void UdpClose(long handle) { }
    public void UdpSend(long handle, byte[] data, string host, int port) { }
    public (byte[] Data, string Host, int Port)? UdpReceive(long handle) => null;
    public ValueTask UdpSendAsync(long handle, byte[] data, string host, int port, CancellationToken cancellationToken) => ValueTask.CompletedTask;
    public ValueTask<(byte[] Data, string Host, int Port)?> UdpReceiveAsync(long handle, CancellationToken cancellationToken) => ValueTask.FromResult<(byte[] Data, string Host, int Port)?>(null);
    public long SslConnect(string host, int port, string? certPath) => -1;
    public long SslListen(int port, string? certPath, string? certPassword) => -1;
    public void SslClose(long handle) { }
    public void SslSend(long handle, byte[] data) { }
    public byte[]? SslReceive(long handle, int count) => null;
    public ValueTask<long> SslConnectAsync(string host, int port, string? certPath, CancellationToken cancellationToken) => ValueTask.FromResult(-1L);
    public void TcpSetOption(long handle, string option, string value) { }
    public void TcpShutdown(long handle, string mode) { }
    public CommonLib.Network.NetworkHandle? GetNetworkHandle(long handle) => null;
}

public class NullHttpService : IHttpService
{
    public string HttpGet(string url) => throw new NotSupportedException("HTTP disabled.");
    public string HttpPost(string url, string body, string contentType) => throw new NotSupportedException("HTTP disabled.");
    public string HttpPut(string url, string body, string contentType) => throw new NotSupportedException("HTTP disabled.");
    public string HttpDelete(string url) => throw new NotSupportedException("HTTP disabled.");
    public byte[] HttpDownload(string url) => throw new NotSupportedException("HTTP disabled.");
    public string HttpUpload(string url, string filePath, string fieldName) => throw new NotSupportedException("HTTP disabled.");
    public long HttpServerStart(int port) => -1;
    public long HttpServerStartTls(int port, string certPath, string certPassword) => -1;
    public void HttpServerStop(long handle) { }
    public void HttpAddRoute(long handle, string method, string path, Func<HttpRequest, HttpResponse> handler) { }
    public void HttpAddStatic(long handle, string prefix, string directory) { }
    public void HttpAddMiddleware(long handle, string pathPrefix, Func<HttpRequest, (bool Continue, HttpResponse? Response)> middleware) { }
    public void HttpAddWebSocketRoute(long handle, string path, Func<HttpRequest, System.Net.WebSockets.WebSocket, Task> handler) { }
    public void HttpConfigure(long handle, HttpServerSettings settings) { }
}

public class NullAgentService : IAgentService
{
    public void CreateAgent(string name, string? configJson) { }
    public void DeleteAgent(string name) { }
    public string SaveAgent(string name) => "{}";
    public void LoadAgent(string name, string configJson) { }
}

public class NullTeamService : ITeamService
{
    public void CreateTeam(string name) { }
    public void AddAgentToTeam(string teamName, string agentName) { }
    public void RemoveAgentFromTeam(string teamName, string agentName) { }
    public void SendMessageToTeam(string teamName, string message) { }
}

public class NullWorkflowService : IWorkflowService
{
    public void DefineWorkflow(string name) { }
    public void AddStep(string name, string action, int retryCount, int timeoutMs) { }
    public void EndWorkflow() { }
    public void ExecuteWorkflow(string name) { }
}

public class NullDatabaseService : IDatabaseService
{
    public long IsamOpen(string path) => -1;
    public long IsamOpenSchema(string path, string alias, string schemaJson) => -1;
    public void IsamClose(long handle) { }
    public void IsamInsert(long handle, object[] values) { }
    public void IsamUpdate(long handle, long recno, object[] values) { }
    public void IsamPut(long handle, object[] values) { }
    public void IsamDelete(long handle, long recno) { }
    public object?[] IsamRead(long handle, long recno) => [];
    public object?[] IsamReadCurrent(long handle) => [];
    public string[] IsamFieldNames(long handle) => [];
    public long IsamGetRecordCount(long handle) => 0;
    public long IsamRecordSize(long handle) => 0;
    public void IsamUse(long handle, string alias, object? type) { }
    public void IsamSelect(long workArea) { }
    public void IsamGo(long handle, long recno) { }
    public void IsamSkip(long handle, int count) { }
    public void IsamSeek(long handle, object key) { }
    public void IsamSetOrder(long handle, int indexNum) { }
    public void IsamSetFilter(long handle, string? filterExpr) { }
    public void IsamPack(long handle) { }
    public void IsamReindex(long handle) { }
    public bool IsamFound(long handle) => false;
    public bool IsamEof(long handle) => true;
    public bool IsamBof(long handle) => true;
    public void IsamRecall(long handle) { }
    public void IsamSetFilterExpr(long handle, Basic.Expr? filterExpr, System.Func<System.Collections.Generic.IReadOnlyDictionary<string, object?>, bool>? evaluator) { }
    public void IsamCreateIndex(long handle, string indexName, string keyExpression, long keyLength) { }
    public void IsamDropIndex(long handle, string indexName) { }
    public void IsamRepair(long handle) { }
    public void IsamBackup(long handle, string destPath) { }
    public bool IsamValidate(long handle) => true;
    public string IsamValidateJson(long handle) => "{\"valid\":true,\"message\":\"\"}";
    public void IsamCopyTo(long handle, string destPath, Basic.Expr? condition, System.Func<System.Collections.Generic.IReadOnlyDictionary<string, object?>, bool>? evaluator) { }
    public void IsamSortTo(long handle, string destPath, string[] fields, bool[] descending, Basic.Expr? condition, System.Func<System.Collections.Generic.IReadOnlyDictionary<string, object?>, bool>? evaluator) { }
    public void IsamJoin(long handle, long secondaryHandle, string destPath, Basic.Expr condition, System.Func<System.Collections.Generic.IReadOnlyDictionary<string, object?>, bool>? evaluator) { }
    public void IsamSetRelation(long handle, long parentHandle, string keyExpression) { }
    public void IsamLock(long handle, bool isRecordLock) { }
    public void IsamUnlock(long handle, bool isRecordLock) { }
    public long DbfOpen(string path) => -1;
    public void DbfClose(long handle) { }
    public void DbfAppend(long handle, object[] values) { }
    public void DbfAppendByName(long handle, System.Collections.Generic.Dictionary<string, object> fields) { }
    public object? DbfFieldByName(long handle, string fieldName) => null;
    public int DbfFieldIndex(long handle, string fieldName) => -1;
    public object?[] DbfRead(long handle, long recno) => [];
    public void DbfUpdate(long handle, long recno, object[] values) { }
    public void DbfDelete(long handle, long recno) { }
    public long DbfGetRecordCount(long handle) => 0;
    public long DbfUse(string path, object?[] indexPaths, int locking, bool unsafeFlag, string? alias, object? typeId, string? typeName) => -1;
    public void DbfSelectWorkArea(long workArea) { }
    public void DbfSelectHandle(long handle) { }
    public object?[] DbfGetRecord(long handle) => [];
    public System.Collections.Generic.Dictionary<string, object?> DbfGetRecordNamed(long handle) => new();
    public void DbfReplace(long handle, int fieldIndex, object value) { }
    public void DbfReplaceByName(long handle, string fieldName, object? value) { }
    public void DbfRecall(long handle) { }
    public void DbfPack(long handle) { }
    public void DbfZap(long handle) { }
    public void DbfGo(long handle, long recno) { }
    public void DbfGoBottom(long handle) { }
    public void DbfSkip(long handle, int count) { }
    public bool DbfSeek(long handle, object key) => false;
    public void DbfSetOrder(long handle, int indexOrder) { }
    public void DbfSetOrderByName(long handle, string name) { }
    public void DbfSetFilter(long handle, string? filterExpr) { }
    public void DbfSetDeleted(long handle, bool on) { }
    public void DbfSetExact(long handle, bool on) { }
    public void DbfAppendBlank(long handle) { }
    public bool DbfEof(long handle) => true;
    public bool DbfBof(long handle) => true;
    public long DbfRecno(long handle) => 0;
    public long DbfActiveOrder(long handle) => -1;
    public bool DbfFound(long handle) => false;
    public bool DbfIsDeleted(long handle) => false;
    public void DbfCloseAll() { }
    public void DbfReindex(long handle, long unused) { }
    public void DbfIndexOn(long handle, string keyExpr, string file, int unique) { }
    public bool DbfValidate(long handle) => true;
    public void DbfList(long handle, string fieldsCsv, string destination, bool toPrint) { }
    public void DbfListCond(long handle, string fieldsCsv, string destination, bool toPrint, System.Func<System.Collections.Generic.IReadOnlyDictionary<string, object?>, bool>? condition) { }
    public void DbfReport(long handle, string formFile, string destination, bool toPrint) { }
    public void DbfAggregate(long handle, string keyField, string destPath, string aggregations) { }
    public void DbfCreateView(string viewName) { }
    public void DbfDropView(string viewName) { }
    public void DbfSortTo(long handle, string destPath, string[] fields, bool[] descending) { }
    public void DbfTotalOn(long handle, string destPath, Basic.Expr keyExpr, Basic.Expr[] sumFieldExprs, System.Func<System.Collections.Generic.IReadOnlyDictionary<string, object?>, bool>? evaluator) { }
    public void DbfCopyTo(long handle, string destPath, System.Func<System.Collections.Generic.IReadOnlyDictionary<string, object?>, bool>? evaluator) { }
    public void DbfAppendFrom(long handle, string sourcePath, System.Func<System.Collections.Generic.IReadOnlyDictionary<string, object?>, bool>? evaluator) { }
    public void DbfCopyStructure(long handle, string destPath) { }
    public void DbfSetRelation(long handle, long parentHandle, string keyExpression) { }
    public void DbfJoin(long handle, long secondaryHandle, string destPath, System.Func<System.Collections.Generic.IReadOnlyDictionary<string, object?>, bool>? evaluator) { }
    public void DbfUpdate(long handle, string keyField, string[] fieldNames, System.Func<System.Collections.Generic.IReadOnlyDictionary<string, object?>, object?>[] valueEvaluators) { }
    public void DbfUpdateOn(long destHandle, long srcHandle, string srcAlias, string keyField, string[] fieldNames,
        System.Func<System.Collections.Generic.IReadOnlyDictionary<string, object?>,
                    System.Collections.Generic.IReadOnlyDictionary<string, object?>, object?>[] valueEvaluators) { }
    public void DbfSetExclusive(long handle, bool on) { }
    public void DbfSetSchema(long handle, bool on) { }
    public void DbfCreateFile(string path, string fieldDefs) { }
    public long SpeedOpen(string path) => -1;
    public long SpeedOpenSchema(string tableName, long dbHandle, string tableAlias, string schemaJson) => -1;
    public void SpeedClose(long handle) { }
    public void SpeedInsert(long handle, string key, object[] values) { }
    public void SpeedInsertByName(long handle, System.Collections.Generic.Dictionary<string, object> fields) { }
    public object?[] SpeedGet(long handle, string key) => [];
    public object?[] SpeedGetCurrent(long handle) => [];
    public void SpeedDelete(long handle, string key) { }
    public void SpeedSeek(long handle, string key) { }
    public bool SpeedEof(long handle) => true;
    public void SpeedSkip(long handle, int count) { }
    public void SpeedGo(long handle, long position) { }
    public void SpeedSetOrder(long handle, string indexName) { }
    public void SpeedSetFilter(long handle, string? filterExpr) { }
    public void SpeedSetRelation(long handle, long parentHandle, string keyExpression) { }
    public void SpeedCreateIndex(long handle, string indexName, string keyExpression) { }
    public void SpeedDropIndex(long handle, string indexName) { }
    public Basic.Databases.Speed.Database? SpeedGetDatabaseFor(long handle) => null;
    public string? SpeedGetTableNameFor(long handle) => null;
    public bool SpeedCreateViewTable(long dbHandle, string viewName, string schemaJson) => false;
    public long SqlOpen(string path) => -1;
    public void SqlClose(long handle) { }
    public void SqlExecute(long handle, string sql) { }
    public object?[][] SqlQuery(long handle, string sql) => [];
    public long SqlLastInsertRowId(long handle) => 0;
    public void SqlBeginTransaction(long handle) { }
    public void SqlCommit(long handle) { }
    public void SqlRollback(long handle) { }
    public long LedgerOpen(string path) => -1;
    public void LedgerClose(long handle) { }
    public long LedgerAppend(long handle, string category, object[] values) => 0;
    public long LedgerUpdate(long handle, long id, object[] values) => 0;
    public object?[] LedgerRead(long handle, long recno) => [];
    public long LedgerGetCount(long handle) => 0;
    public void LedgerGoTop(long handle) { }
    public void LedgerGoBottom(long handle) { }
    public void LedgerGo(long handle, long recno) { }
    public void LedgerSkip(long handle, int count) { }
    public bool LedgerBof(long handle) => false;
    public bool LedgerEof(long handle) => true;
    public long LedgerRecno(long handle) => 0;
    public bool LedgerFound(long handle) => false;
    public bool LedgerValidate(long handle) => false;
    public void LedgerDelete(long handle, long id) { }
    public void LedgerPack(long handle) { }
    public void LedgerSave(long handle) { }
    public object?[] LedgerRegister(long handle, string query) => [];
    public object?[] LedgerBalance(long handle, string query) => [];
    public object?[] LedgerGet(long handle, long id) => [];
    public string[] LedgerAccounts(long handle, string? root) => [];
    public void LedgerBudget(long handle, string account, decimal amount, string period) { }
    public object? LedgerVariance(long handle, string account, string period) => null;
    public object? LedgerPlot(long handle, string query, string chartType) => null;
    public double LedgerImport(long handle, string csvPath, string mappingRules) => 0;
    public long VectorOpen(string path) => -1;
    public void VectorClose(long handle) { }
    public void VectorInsert(long handle, float[] vector, object? metadata) { }
    public object?[] VectorSearch(long handle, float[] query, int topK) => [];
    public object?[] VectorSearchText(string dbPath, string collection, string query, int topK) => [];
    public long VectorCount(string dbPath, string collection) => 0;
    public void VectorEnsureCollection(string dbPath, string collection) { }
    public void VectorIndexText(string dbPath, string collection, string docId, string text) { }
    public void VectorLoadDocuments(string dbPath, string dirPath, string collection) { }

    public void DbfSetDurability(long handle, string mode) { }
    public void DbfFlushDurability(long handle) { }
    public void IsamSetReplication(long handle, string logPath) { }
    public void SpeedSetDurability(long handle, string mode) { }
    public void SpeedFlushDurability(long handle) { }
    public void SpeedBeginGroupCommit(long handle) { }
    public void SpeedEndGroupCommit(long handle) { }
    public string DbfSetReplication(long handle, string logPath) => "";
    public void DbfBeginGroupCommit(long handle) { }
    public void DbfEndGroupCommit(long handle) { }
    public int DbfApplyReplication(long handle, string replLogPath) => 0;
    public int DbfFollowReplication(long handle, string replLogPath) => 0;
    public long DbfAppendMany(long handle, System.Collections.Generic.List<System.Collections.Generic.Dictionary<string, object?>> records) => 0;
    public long IsamAppendMany(long handle, System.Collections.Generic.List<System.Collections.Generic.Dictionary<string, object?>> records) => 0;
    public int IsamApplyReplication(long handle, string replLogPath) => 0;
    public long SpeedSetMany(long handle, System.Collections.Generic.List<System.Collections.Generic.Dictionary<string, object?>> records) => 0;

}
