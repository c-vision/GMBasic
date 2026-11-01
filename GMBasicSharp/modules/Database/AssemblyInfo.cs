using System.Runtime.CompilerServices;

// Il P-Code Interpreter interno dell'IL Backend (common/IlBackend/PCode/, porting da PCodeVm
// GMBasic) accede a campi/proprietà internal dei driver RDD (DbfDriver._engine,
// IsamDriver._engine, SpeedDriver._db/_tableId, Database.Manifest) per gli stessi motivi per cui
// li usava già in GMBasic: bypassare l'API pubblica RDD per operazioni a basso livello (SEEK su
// chiave binaria, accesso diretto al Manifest SPEED) che il livello pubblico non espone.
[assembly: InternalsVisibleTo("GMBasic.IlBackend")]
[assembly: InternalsVisibleTo("GMBasic.Runtime")]
[assembly: InternalsVisibleTo("GMBasic.Runtime.IL")]
// E1.6 (2026-09-29): i test xUnit di Database.Native devono chiamare
// RecordMarshaller.DeserializeRecord (internal) con un buffer SOVRADIMENSIONATO, che e' la
// condizione per cui esiste il parametro length. Senza questa riga quel comportamento
// (introdotto dal commit isam-recordread-ai) resterebbe non coperto da alcun test.
[assembly: InternalsVisibleTo("GMBASIC.Database.Native.Tests")]
