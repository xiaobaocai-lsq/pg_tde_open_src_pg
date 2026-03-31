/*
 * xlog_smgr.h - Stub header for open-source PostgreSQL compatibility
 *
 * This is a minimal stub to allow pg_tde to compile without Percona Server.
 * WAL encryption functionality is NOT available in this build.
 *
 * Only the minimal type/function declarations needed by pg_tde are provided.
 */
#ifndef PG_TDE_XLOG_SMGR_STUB_H
#define PG_TDE_XLOG_SMGR_STUB_H

#include "c.h"

/* WAL segment number type */
typedef uint64 XLogSegNo;

/* WAL location/pointer type */
typedef struct
{
	uint32 tli;
	uint32 xlogid;
	uint32 xrecoff;
} XLogRecPtr;

#define InvalidXLogRecPtr \
	((XLogRecPtr) {0, 0, 0})

/* WAL encryption range types - stub values */
typedef enum
{
	WAL_ENCRYPTION_RANGE_INVALID = 0,
	WAL_ENCRYPTION_RANGE_KEY = 1,
	WAL_ENCRYPTION_NONE = 2
} WalEncryptionRangeType;

typedef struct
{
	WalEncryptionRangeType type;
	struct {
		uint32 tli;
		XLogRecPtr lsn;
	} start;
	struct {
		uint32 tli;
		XLogRecPtr lsn;
	} end;
} WalEncryptionRange;

/* WAL key cache record - stub */
typedef struct WALKeyCacheRec
{
	Oid dbOid;
	WalEncryptionRange range;
} WALKeyCacheRec;

/*
 * XLogSmgr - WAL Storage Manager interface (stub)
 * This structure is registered with Percona Server to intercept WAL I/O.
 * In open-source PG, this is not available - WAL encryption is disabled.
 */
typedef struct XLogSmgr
{
	const char *name;
} XLogSmgr;

#endif /* PG_TDE_XLOG_SMGR_STUB_H */
