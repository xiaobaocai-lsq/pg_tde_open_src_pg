/*
 * pg_tde_xlog_smgr.c - WAL Storage Manager stubs for open-source PostgreSQL
 *
 * WAL encryption is NOT available in this build.
 * This file provides no-op stubs for all WAL-related functions.
 * Table data encryption (the main feature) still works.
 *
 * Original Percona file: src/access/pg_tde_xlog_smgr.c
 * Percona API dependency removed for open-source PostgreSQL compatibility.
 */

#include "postgres.h"

#ifndef FRONTEND
#include "access/xlog_internal.h"
#include "access/xloginsert.h"
#include "storage/bufmgr.h"
#include "utils/guc.h"
#include "utils/memutils.h"

#include "access/pg_tde_xlog_keys.h"
#include "access/pg_tde_xlog_smgr.h"
#include "catalog/tde_global_space.h"
#include "encryption/enc_tde.h"
#include "pg_tde.h"
#include "pg_tde_defines.h"

#include "access/xlog.h"
#include "port/atomics.h"
#include "storage/shmem.h"
#endif

#include "pg_tde_fe.h"

/* Stub implementations - WAL encryption is disabled in this build */

void *
TDEXLogSmgrGetEncryptionCtx(void)
{
	return NULL;  /* No encryption context without Percona API */
}

void
TDEXLogSmgrFreeEncryptionCtx(void *ctx)
{
	/* No-op */
}

Size
TDEXLogSmgrShmemSize(void)
{
	return 0;  /* No shared memory needed without WAL encryption */
}

void
TDEXLogSmgrShmemInit(void)
{
	/* No-op: WAL encryption shared memory not initialized */
	ereport(LOG, errmsg("pg_tde: WAL encryption is disabled (open-source PostgreSQL build)"));
}

void
TDEXLogSmgrInit(void)
{
	/* No-op: WAL encryption not available in open-source PostgreSQL */
	ereport(LOG, errmsg("pg_tde: WAL storage manager not initialized (Percona API not available)"));
}

void
TDEXLogSmgrInitWrite(bool encrypt_xlog, int key_len)
{
	/* No-op: WAL encryption write initialization not available */
}

void
TDEXLogSmgrInitWriteOldKeys(void)
{
	/* No-op: Old key migration not needed without WAL encryption */
}

void
TDEXLogCryptBuffer(const void *buf, void *out_buf, size_t count, off_t offset,
				   TimeLineID tli, XLogSegNo segno, int segSize)
{
	/* No-op: WAL encryption not available */
	/* Copy data unchanged when called unexpectedly */
	if (buf != out_buf && count > 0)
		memcpy(out_buf, buf, count);
}

bool
tde_ensure_xlog_key_location(WalLocation loc)
{
	/* No-op: WAL key location tracking not available */
	return false;
}

/*
 * XLogSmgr stub - not registered in open-source PostgreSQL
 * The actual WAL storage manager is the default PostgreSQL WAL manager.
 */
