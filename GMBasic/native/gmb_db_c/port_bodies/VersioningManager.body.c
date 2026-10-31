#include <stdio.h>
#include <unistd.h>

/* C#: VersioningManager — sidecar .gmv, 3×int64 LE pack=1 (24 byte/record). */

#define VersionMetadata_Size 24

struct VersioningManager {
    FILE* stream;
    char path[1024];
};

static void vm_change_ext(const char* dbf, char* out, int cap)
{
    char* dot;
    char* slash;
    int n;
    snprintf(out, (size_t)cap, "%s", dbf ? dbf : "");
    slash = strrchr(out, '/');
    dot = strrchr(slash ? slash : out, '.');
    if (dot) *dot = 0;
    n = (int)strlen(out);
    snprintf(out + n, (size_t)(cap - n), ".gmv");
}

VersioningManager* VersioningManager_New(const char* dbfFilePath)
{
    VersioningManager* self;
    char path[1024];
    if (!dbfFilePath) return NULL;
    vm_change_ext(dbfFilePath, path, (int)sizeof(path));
    self = (VersioningManager*)calloc(1, sizeof(*self));
    if (!self) return NULL;
    snprintf(self->path, sizeof(self->path), "%s", path);
    self->stream = fopen(path, "r+b");
    if (!self->stream) self->stream = fopen(path, "w+b");
    if (!self->stream) {
        free(self);
        return NULL;
    }
    return self;
}

void VersioningManager_Dispose(VersioningManager* self)
{
    if (!self) return;
    if (self->stream) {
        fflush(self->stream);
        fclose(self->stream);
    }
    free(self);
}

static void vm_put_i64(uint8_t* p, int64_t v)
{
    int i;
    for (i = 0; i < 8; i++) p[i] = (uint8_t)((uint64_t)v >> (8 * i));
}

static int64_t vm_get_i64(const uint8_t* p)
{
    int i;
    uint64_t u = 0;
    for (i = 0; i < 8; i++) u |= (uint64_t)p[i] << (8 * i);
    return (int64_t)u;
}

VersionMetadata VersioningManager_GetMetadata(VersioningManager* self, int recNo)
{
    VersionMetadata m;
    uint8_t buf[VersionMetadata_Size];
    int64_t offset;
    memset(&m, 0, sizeof(m));
    if (!self || recNo <= 0) return m;
    offset = (int64_t)(recNo - 1) * VersionMetadata_Size;
    fseek(self->stream, 0, SEEK_END);
    if (offset >= ftell(self->stream)) return m;
    fseek(self->stream, (long)offset, SEEK_SET);
    if (fread(buf, 1, VersionMetadata_Size, self->stream) != VersionMetadata_Size) return m;
    m.Version = vm_get_i64(buf);
    m.TransactionId = vm_get_i64(buf + 8);
    m.PreviousVersionRecNo = vm_get_i64(buf + 16);
    return m;
}

void VersioningManager_SetMetadata(VersioningManager* self, int recNo, VersionMetadata metadata)
{
    uint8_t buf[VersionMetadata_Size];
    int64_t offset;
    if (!self || recNo <= 0) return;
    offset = (int64_t)(recNo - 1) * VersionMetadata_Size;
    vm_put_i64(buf, metadata.Version);
    vm_put_i64(buf + 8, metadata.TransactionId);
    vm_put_i64(buf + 16, metadata.PreviousVersionRecNo);
    fseek(self->stream, (long)offset, SEEK_SET);
    fwrite(buf, 1, VersionMetadata_Size, self->stream);
    fflush(self->stream);
}

void VersioningManager_AddNewVersionEntry(VersioningManager* self, VersionMetadata initialMetadata)
{
    uint8_t buf[VersionMetadata_Size];
    if (!self) return;
    vm_put_i64(buf, initialMetadata.Version);
    vm_put_i64(buf + 8, initialMetadata.TransactionId);
    vm_put_i64(buf + 16, initialMetadata.PreviousVersionRecNo);
    fseek(self->stream, 0, SEEK_END);
    fwrite(buf, 1, VersionMetadata_Size, self->stream);
    fflush(self->stream);
}

void VersioningManager_Clear(VersioningManager* self)
{
    if (!self || !self->stream) return;
    rewind(self->stream);
    if (ftruncate(fileno(self->stream), 0) != 0) {
        /* keep going: next writes still valid */
    }
    fflush(self->stream);
}

int VersioningManager_SelfCheck(void)
{
    char dir[] = "native/gmb_db_c/build/gmvsc_XXXXXX";
    char dbf[1024], gmv[1024];
    VersioningManager* vm;
    VersionMetadata m, got;
    if (!mkdtemp(dir)) return -1;
    snprintf(dbf, sizeof(dbf), "%s/t.dbf", dir);
    snprintf(gmv, sizeof(gmv), "%s/t.gmv", dir);
    {
        FILE* f = fopen(dbf, "wb");
        if (f) fclose(f);
    }
    vm = VersioningManager_New(dbf);
    if (!vm) return -2;
    memset(&m, 0, sizeof(m));
    m.Version = 42;
    m.TransactionId = 7;
    m.PreviousVersionRecNo = 3;
    VersioningManager_SetMetadata(vm, 1, m);
    got = VersioningManager_GetMetadata(vm, 1);
    VersioningManager_Dispose(vm);
    unlink(gmv);
    unlink(dbf);
    rmdir(dir);
    if (got.Version != 42 || got.TransactionId != 7 || got.PreviousVersionRecNo != 3) return -3;
    return GMB_DB_OK;
}
