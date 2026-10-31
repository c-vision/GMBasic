#include <stdio.h>

/* C#: DbfReplicationLog — append-only sidecar, flush per riga, UTF-8 no BOM. */

struct DbfReplicationLog {
    FILE* writer;
    char LogPath[1024];
};

DbfReplicationLog* DbfReplicationLog_New(const char* logPath)
{
    DbfReplicationLog* self;
    if (!logPath || !*logPath) return NULL;
    self = (DbfReplicationLog*)calloc(1, sizeof(*self));
    if (!self) return NULL;
    snprintf(self->LogPath, sizeof(self->LogPath), "%s", logPath);
    self->writer = fopen(logPath, "ab");
    if (!self->writer) {
        free(self);
        return NULL;
    }
    return self;
}

void DbfReplicationLog_Dispose(DbfReplicationLog* self)
{
    if (!self) return;
    if (self->writer) {
        fflush(self->writer);
        fclose(self->writer);
    }
    free(self);
}

const char* DbfReplicationLog_get_LogPath(const DbfReplicationLog* self)
{
    return self ? self->LogPath : NULL;
}

void DbfReplicationLog_WriteLine(DbfReplicationLog* self, const char* line)
{
    if (!self || !self->writer || !line) return;
    fputs(line, self->writer);
    fputc('\n', self->writer);
    fflush(self->writer);
}
