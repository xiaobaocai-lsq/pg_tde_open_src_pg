/*
 * pg_tde_smgr.c - Table Data Encryption Storage Manager
 * Adapted for open-source PostgreSQL 17
 *
 * Table encryption via SMGR layer - wraps md (magnetic disk) I/O
 *   WRITE: tde_extend() → encrypt block → mdextend()
 *   READ:  mdreadv() → decrypt block → return plaintext
 *
 * Uses PostgreSQL backend types from postgres.h
 * MAX_BACKEND_MAX_BACKEND must be 100 for correct struct layout
 */
#include "postgres.h"
#include "access/xlog.h"
#include "access/xlog_internal.h"
#include "catalog/pg_class.h"
#include "commands/tablecmds.h"
#include "nodes/relation.h"
#include "nodes/pg_list.h"
#include "storage/bufmgr.h"
#include "storage/bufpage.h"
#include "storage/fd.h"
#include "storage/ipc.h"
#include "storage/lwlock.h"
#include "storage/md.h"
#include "storage/proc.h"
#include "storage/procarray.h"
#include "storage/relfilelocator.h"
#include "storage/shmem.h"
#include "storage/sinval.h"
#include "storage/s_lock.h"
#include "storage/sync.h"
#include "utils/elog.h"
#include "utils/guc.h"
#include "utils/memutils.h"
#include "utils/palloc.h"
#include "utils/ps_status.h"
#include "utils/relcache.h"
#include "port/pg_crc32c.h"
#include "pg_tde.h"
#include "encryption/enc_tde.h"
#include "encryption/enc_aes.h"

/* Backend identity - declared in miscadmin.h */
extern int MyBackendId;
#include "common/pg_tde_utils.h"

/* Force MAX_BACKEND_MAX_BACKEND=100 AFTER all postgres headers */
#ifdef MAX_BACKEND_MAX_BACKEND
#undef MAX_BACKEND_MAX_BACKEND
#endif
#define MAX_BACKEND_MAX_BACKEND MAX_BACKEND

/* ====================================================================
 * TDE Per-Relation Key Storage (simple static array)
 * ==================================================================== */

#define MAX_TDE_ENTRIES 4096

typedef struct {
    Oid          spcOid;
    Oid          dbOid;
    Oid          relNumber;
    int          backend_id;
    bool         in_use;
    bool         key_set;
    unsigned char rel_key[32];
    int          key_len;
    bool         encrypt_index;
} TDEEntry;

static TDEEntry tde_entries[MAX_TDE_ENTRIES] = {0};

static TDEEntry *
tde_find(Oid spcOid, Oid dbOid, Oid relNumber, int backend_id, bool create)
{
    int i;
    for (i = 0; i < MAX_TDE_ENTRIES; i++) {
        if (tde_entries[i].in_use &&
            tde_entries[i].spcOid == spcOid &&
            tde_entries[i].dbOid == dbOid &&
            tde_entries[i].relNumber == relNumber &&
            tde_entries[i].backend_id == backend_id)
            return &tde_entries[i];
    }
    if (!create) return NULL;
    for (i = 0; i < MAX_TDE_ENTRIES; i++) {
        if (!tde_entries[i].in_use) {
            memset(&tde_entries[i], 0, sizeof(TDEEntry));
            tde_entries[i].in_use = true;
            tde_entries[i].spcOid = spcOid;
            tde_entries[i].dbOid = dbOid;
            tde_entries[i].relNumber = relNumber;
            tde_entries[i].backend_id = backend_id;
            return &tde_entries[i];
        }
    }
    return NULL;
}

/* ====================================================================
 * Encryption Helpers
 * ==================================================================== */

static void
tde_compute_iv(const unsigned char *key, BlockNumber blkno, unsigned char *iv_out)
{
    memcpy(iv_out, key, 16);
    iv_out[0] ^= (blkno >> 0) & 0xFF;
    iv_out[1] ^= (blkno >> 8) & 0xFF;
    iv_out[4] ^= (blkno >> 16) & 0xFF;
    iv_out[5] ^= (blkno >> 24) & 0xFF;
}

static void
tde_encrypt_block(const void *plaintext, void *ciphertext,
                  const unsigned char *key, BlockNumber blkno)
{
    unsigned char iv[16];
    tde_compute_iv(key, blkno, iv);
    tde_aes_encrypt_cbc(plaintext, ciphertext, BLCKSZ, iv, key, 32);
}

static void
tde_decrypt_block(const void *ciphertext, void *plaintext,
                   const unsigned char *key, BlockNumber blkno)
{
    unsigned char iv[16];
    tde_compute_iv(key, blkno, iv);
    tde_aes_decrypt_cbc(ciphertext, plaintext, BLCKSZ, iv, key, 32);
}

/* ====================================================================
 * SMGR Forward Declarations
 * ==================================================================== */

static void tde_init(void);
static void tde_open(SMgrRelation reln);
static void tde_close(SMgrRelation reln, ForkNumber forknum);
static void tde_create(SMgrRelation reln, ForkNumber forknum, bool isRedo);
static bool tde_exists(SMgrRelation reln, ForkNumber forknum);
static void tde_unlink(RelFileLocatorBackend rlocator,
                            ForkNumber forknum, bool isRedo);
static void tde_extend(SMgrRelation reln, ForkNumber forknum,
                            BlockNumber blkno, const void *buffer, bool skipFsync);
static void tde_zeroextend(SMgrRelation reln, ForkNumber forknum,
                                 BlockNumber blkno, int nblocks, bool skipFsync);
static void tde_prefetch(SMgrRelation reln, ForkNumber forknum,
                               BlockNumber blkno, int nblocks);
static void tde_readv(SMgrRelation reln, ForkNumber forknum,
                             BlockNumber blkno, void **buffers, BlockNumber nblocks);
static void tde_writev(SMgrRelation reln, ForkNumber forknum,
                            BlockNumber blkno,
                            const void **buffers, BlockNumber nblocks, bool skipFsync);
static void tde_writeback(SMgrRelation reln, ForkNumber forknum,
                                BlockNumber blkno, BlockNumber nblocks);
static BlockNumber tde_nblocks(SMgrRelation reln, ForkNumber forknum);
static void tde_truncate(SMgrRelation reln, ForkNumber forknum,
                               BlockNumber blkno, BlockNumber nblocks);
static void tde_immedsync(SMgrRelation reln, ForkNumber forknum);
static void tde_registersync(SMgrRelation reln, ForkNumber forknum);

static const f_smgr tde_smgr = {
    .smgr_init        = tde_init,
    .smgr_shutdown    = NULL,
    .smgr_open        = tde_open,
    .smgr_close       = tde_close,
    .smgr_create      = tde_create,
    .smgr_exists      = tde_exists,
    .smgr_unlink      = tde_unlink,
    .smgr_extend      = tde_extend,
    .smgr_zeroextend = tde_zeroextend,
    .smgr_prefetch   = tde_prefetch,
    .smgr_readv      = tde_readv,
    .smgr_writev     = tde_writev,
    .smgr_writeback  = tde_writeback,
    .smgr_nblocks    = tde_nblocks,
    .smgr_truncate   = tde_truncate,
    .smgr_immedsync  = tde_immedsync,
    .smgr_registersync = tde_registersync,
};

/* ====================================================================
 * SMGR Function Implementations
 * ==================================================================== */

static void tde_init(void)
{
    ereport(DEBUG2, errmsg("pg_tde: TDE storage manager initialized"));
}

static void tde_open(SMgrRelation reln) { }

static void tde_close(SMgrRelation reln, ForkNumber forknum)
{
    mdclose(reln, forknum);
}

static void tde_create(SMgrRelation reln, ForkNumber forknum, bool isRedo)
{
    mdcreate(reln, forknum, isRedo);
}

static bool tde_exists(SMgrRelation reln, ForkNumber forknum)
{
    return mdexists(reln, forknum);
}

static void tde_unlink(RelFileLocatorBackend rlocator,
                     ForkNumber forknum, bool isRedo)
{
    mdunlink(rlocator, forknum, isRedo);
}

static void
tde_extend(SMgrRelation reln, ForkNumber forknum,
               BlockNumber blkno, const void *buffer,
               bool skipFsync)
{
    TDEEntry *entry;
    RelFileLocator *rloc = &reln->smgr_rlocator.locator;
    int bid = reln->smgr_rlocator.backend;

    entry = tde_find(rloc->spcOid, rloc->dbOid, rloc->relNumber, bid, false);

    if (entry != NULL && entry->key_set)
    {
        char encrypted[BLCKSZ];
        tde_encrypt_block(buffer, encrypted, entry->rel_key, blkno);
        mdextend(reln, forknum, blkno, encrypted, skipFsync);
    }
    else
    {
        mdextend(reln, forknum, blkno, buffer, skipFsync);
    }
}

static void
tde_zeroextend(SMgrRelation reln, ForkNumber forknum,
                 BlockNumber blkno, int nblocks, bool skipFsync)
{
    char zero_buf[BLCKSZ];
    memset(zero_buf, 0, BLCKSZ);

    TDEEntry *entry;
    RelFileLocator *rloc = &reln->smgr_rlocator.locator;
    int bid = reln->smgr_rlocator.backend;

    entry = tde_find(rloc->spcOid, rloc->dbOid, rloc->relNumber, bid, false);

    if (entry != NULL && entry->key_set)
    {
        int i;
        for (i = 0; i < nblocks; i++)
        {
            char encrypted[BLCKSZ];
            tde_encrypt_block(zero_buf, encrypted, entry->rel_key, blkno + i);
            mdextend(reln, forknum, blkno + i, encrypted, skipFsync);
        }
    }
    else
    {
        mdzeroextend(reln, forknum, blkno, nblocks, skipFsync);
    }
}

static void
tde_prefetch(SMgrRelation reln, ForkNumber forknum,
                BlockNumber blkno, int nblocks)
{
    mdprefetch(reln, forknum, blkno, nblocks);
}

static void
tde_readv(SMgrRelation reln, ForkNumber forknum,
             BlockNumber blkno, void **buffers, BlockNumber nblocks)
{
    TDEEntry *entry;
    RelFileLocator *rloc = &reln->smgr_rlocator.locator;
    int bid = reln->smgr_rlocator.backend;
    BlockNumber i;
    char decrypted[BLCKSZ];

    mdreadv(reln, forknum, blkno, buffers, nblocks);

    entry = tde_find(rloc->spcOid, rloc->dbOid, rloc->relNumber, bid, false);

    if (entry != NULL && entry->key_set)
    {
        for (i = 0; i < nblocks; i++)
        {
            if (buffers[i] != NULL)
            {
                tde_decrypt_block(buffers[i], decrypted, entry->rel_key, blkno + i);
                memcpy(buffers[i], decrypted, BLCKSZ);
            }
        }
    }
}

static void
tde_writev(SMgrRelation reln, ForkNumber forknum,
               BlockNumber blkno,
               const void **buffers, BlockNumber nblocks,
               bool skipFsync)
{
    TDEEntry *entry;
    RelFileLocator *rloc = &reln->smgr_rlocator.locator;
    int bid = reln->smgr_rlocator.backend;
    char encrypted[BLCKSZ];
    BlockNumber i;

    entry = tde_find(rloc->spcOid, rloc->dbOid, rloc->relNumber, bid, false);

    if (entry != NULL && entry->key_set)
    {
        for (i = 0; i < nblocks; i++)
        {
            tde_encrypt_block(buffers[i], encrypted, entry->rel_key, blkno + i);
            mdextend(reln, forknum, blkno + i, encrypted, skipFsync);
        }
    }
    else
    {
        mdwritev(reln, forknum, blkno, buffers, nblocks, skipFsync);
    }
}

static void
tde_writeback(SMgrRelation reln, ForkNumber forknum,
                  BlockNumber blkno, BlockNumber nblocks)
{
    mdwriteback(reln, forknum, blkno, nblocks);
}

static BlockNumber
tde_nblocks(SMgrRelation reln, ForkNumber forknum)
{
    return mdnblocks(reln, forknum);
}

static void
tde_truncate(SMgrRelation reln, ForkNumber forknum,
                 BlockNumber old_blocks, BlockNumber nblocks)
{
    mdtruncate(reln, forknum, old_blocks, nblocks);
}

static void
tde_immedsync(SMgrRelation reln, ForkNumber forknum)
{
    mdimmedsync(reln, forknum);
}

static void
tde_registersync(SMgrRelation reln, ForkNumber forknum)
{
    mdregistersync(reln, forknum);
}


/* ====================================================================
 * Extension Registration
 * ==================================================================== */

void
RegisterStorageMgr(void)
{
    smgr_register(&tde_smgr);
    ereport(LOG,
            errmsg("pg_tde: TDE storage manager registered "
                   "(open-source PostgreSQL %d)", PG_VERSION_NUM / 100));
}

void
pg_tde_add_relation_key(Relation rel)
{
    unsigned char new_key[32];
    TDEEntry *entry;
    RelFileLocator *rloc = &rel->rd_locator;

    if (pg_tde_generate_random_bytes(new_key, 32) != 0)
    {
        ereport(ERROR,
                errcode(ERRCODE_INTERNAL_ERROR),
                errmsg("pg_tde: failed to generate encryption key"));
        return;
    }

    entry = tde_find(rloc->spcOid, rloc->dbOid, rloc->relNumber,
                     MyBackendId, true);
    if (entry == NULL)
    {
        ereport(ERROR,
                errcode(ERRCODE_INTERNAL_ERROR),
                errmsg("pg_tde: TDE entry table full"));
        return;
    }

    memcpy(entry->rel_key, new_key, 32);
    entry->key_len = 32;
    entry->key_set = true;
    entry->encrypt_index = false;

    ereport(LOG,
            errmsg("pg_tde: encryption key configured "
                   "for relation %u/%u/%u (rel_id=%u)",
                   rloc->spcOid, rloc->dbOid, rloc->relNumber,
                   rel->rd_id));
}
