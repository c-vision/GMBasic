// FILE: Basic.DBF.VersioningManager.cs

using System.Runtime.InteropServices;

// ReSharper disable InconsistentNaming
// ReSharper disable CheckNamespace

namespace Basic.DBF
{
    /// <summary>
    /// Gestisce un file satellite (.gmv) per memorizzare i metadati di versione dei record
    /// di un file DBF, abilitando il Multi-Version Concurrency Control (MVCC).
    /// </summary>
    internal class VersioningManager : IDisposable
    {
        /// <summary>
        /// Rappresenta la struttura dei metadati memorizzata per ogni record nel file .gmv.
        /// </summary>
        [StructLayout(LayoutKind.Sequential, Pack = 1)]
        internal struct VersionMetadata
        {
            /// <summary>
            /// Versione per la concorrenza ottimistica.
            /// </summary>
            public long Version;
            /// <summary>
            /// ID della transazione che ha creato questa versione.
            /// </summary>
            public long TransactionId;
            /// <summary>
            /// Puntatore (numero di record) alla versione precedente del record.
            /// </summary>
            public long PreviousVersionRecNo; // Usiamo long per coerenza, ma rappresenta un recNo int
        }

        private static readonly int EntrySize = Marshal.SizeOf<VersionMetadata>();
        private readonly FileStream _stream;

        public VersioningManager(string dbfFilePath)
        {
            var versionFilePath = Path.ChangeExtension(dbfFilePath, ".gmv");
            _stream = new FileStream(versionFilePath, FileMode.OpenOrCreate, FileAccess.ReadWrite, FileShare.Read);
        }

        /// <summary>
        /// Recupera i metadati di versione completi per un record specifico.
        /// </summary>
        /// <param name="recNo">Il numero del record (1-based).</param>
        /// <returns>La struttura <see cref="VersionMetadata"/>, o una struttura di default se non esiste.</returns>
        public VersionMetadata GetMetadata(int recNo)
        {
            if (recNo <= 0) return default;

            var offset = (long)(recNo - 1) * EntrySize;
            if (offset >= _stream.Length)
            {
                return default;
            }

            _stream.Seek(offset, SeekOrigin.Begin);
            
            var buffer = new byte[EntrySize];
            var bytesRead = _stream.Read(buffer, 0, EntrySize);

            if (bytesRead != EntrySize) return default;
            
            // Deserializza i byte nella struct.
            var handle = GCHandle.Alloc(buffer, GCHandleType.Pinned);
            try
            {
                return Marshal.PtrToStructure<VersionMetadata>(handle.AddrOfPinnedObject());
            }
            finally
            {
                handle.Free();
            }
        }

        /// <summary>
        /// Imposta o aggiorna i metadati di versione per un record specifico.
        /// </summary>
        /// <param name="recNo">Il numero del record (1-based).</param>
        /// <param name="metadata">La nuova struttura di metadati da scrivere.</param>
        public void SetMetadata(int recNo, VersionMetadata metadata)
        {
            if (recNo <= 0) return;

            var offset = (long)(recNo - 1) * EntrySize;
            _stream.Seek(offset, SeekOrigin.Begin);

            // Serializza la struct in byte.
            var buffer = new byte[EntrySize];
            var handle = GCHandle.Alloc(buffer, GCHandleType.Pinned);
            try
            {
                Marshal.StructureToPtr(metadata, handle.AddrOfPinnedObject(), false);
                _stream.Write(buffer, 0, buffer.Length);
            }
            finally
            {
                handle.Free();
            }
        }

        /// <summary>
        /// Aggiunge una nuova voce di metadati alla fine del file, per un nuovo record.
        /// </summary>
        /// <param name="initialMetadata">I metadati iniziali da assegnare al nuovo record.</param>
        public void AddNewVersionEntry(VersionMetadata initialMetadata)
        {
            _stream.Seek(0, SeekOrigin.End);
            var buffer = new byte[EntrySize];
            var handle = GCHandle.Alloc(buffer, GCHandleType.Pinned);
            try
            {
                Marshal.StructureToPtr(initialMetadata, handle.AddrOfPinnedObject(), false);
                _stream.Write(buffer, 0, buffer.Length);
            }
            finally
            {
                handle.Free();
            }
        }
        
        /// <summary>
        /// Rimuove tutte le voci, troncando il file. Usato in operazioni come ZAP.
        /// </summary>
        public void Clear()
        {
            _stream.SetLength(0);
        }
        
        public void Dispose()
        {
            _stream.Flush();
            _stream.Dispose();
        }
    }
}