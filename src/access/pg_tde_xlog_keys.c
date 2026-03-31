/*
 * pg_tde_xlog_keys.c - WAL Key Management stubs for open-source PostgreSQL
 *
 * WAL encryption is NOT available in this build.
 * All WAL-related functions are stubbed out as no-ops.
 *
 * Table data encryption still works - only WAL encryption is disabled.
 *
 * Original: src/access/pg_tde_xlog_keys.c
 */

#include "postgres.h"

#include <openssl/err.h>
#include <openssl/rand.h>

#include "access/xlog_internal.h"
#include "access/xlog.h"
#include "access/xloginsert.h"
#include "storage/fd.h"
#include "utils/memutils.h"

#include "access/pg_tde_xlog_keys.h"
#include "access/pg_tde_xlog.h"
#include "catalog/tde_global_space.h"
#include "catalog/tde_principal_key.h"
#include "common/pg_tde_utils.h"
#include "encryption/enc_aes.h"
#include "encryption/enc_tde.h"
#include "pg_tde.h"
#include "utils/palloc.h"

#ifdef FRONTEND
#include "pg_tde_fe.h"
#endif

/*
 * WAL key management - all stubbed out for open-source PostgreSQL
 * WAL encryption requires Percona Server-specific APIs.
 */

/*
 * pg_tde_update_wal_keys_file - Update WAL keys version file
 *
 * STUB: WAL encryption is disabled, this is a no-op.
 */
void
pg_tde_update_wal_keys_file(void)
{
	/* WAL encryption disabled - no-op */
}

/*
 * pg_tde_count_wal_ranges_in_file - Count WAL ranges in key file
 *
 * STUB: Returns 0 since WAL encryption is disabled.
 */
int
pg_tde_count_wal_ranges_in_file(void)
{
	return 0;
}

/*
 * pg_tde_create_wal_range - Create a new WAL encryption range
 *
 * STUB: No-op since WAL encryption is disabled.
 */
void
pg_tde_create_wal_range(WalEncryptionRange *range, WalEncryptionRangeType type, int key_len)
{
	/* WAL encryption disabled - no-op */
}

/*
 * pg_tde_delete_server_key - Delete server-wide WAL encryption key
 *
 * STUB: No-op since WAL encryption is disabled.
 */
void
pg_tde_delete_server_key(void)
{
	/* WAL encryption disabled - no-op */
}

/*
 * pg_tde_fetch_wal_keys - Fetch WAL encryption keys for a range
 *
 * STUB: Returns NULL since WAL encryption is disabled.
 */
WALKeyCacheRec *
pg_tde_fetch_wal_keys(WalLocation start)
{
	return NULL;
}

/*
 * pg_tde_free_wal_key_cache - Free WAL key cache
 *
 * STUB: No-op since no cache exists without WAL encryption.
 */
void
pg_tde_free_wal_key_cache(void)
{
	/* WAL encryption disabled - no-op */
}

/*
 * pg_tde_get_last_wal_key - Get the most recently created WAL key
 *
 * STUB: Returns NULL since WAL encryption is disabled.
 */
WALKeyCacheRec *
pg_tde_get_last_wal_key(void)
{
	return NULL;
}

/*
 * pg_tde_get_server_key_info - Get server key metadata
 *
 * STUB: Returns NULL since server key is not used without WAL encryption.
 */
TDESignedPrincipalKeyInfo *
pg_tde_get_server_key_info(void)
{
	return NULL;
}

/*
 * pg_tde_get_wal_cache_keys - Get all cached WAL keys
 *
 * STUB: Returns NULL since WAL encryption is disabled.
 */
WALKeyCacheRec *
pg_tde_get_wal_cache_keys(void)
{
	return NULL;
}

/*
 * pg_tde_perform_rotate_server_key - Rotate the server-wide WAL key
 *
 * STUB: No-op since WAL encryption is disabled.
 */
void
pg_tde_perform_rotate_server_key(const TDEPrincipalKey *principal_key,
								  const TDEPrincipalKey *new_principal_key,
								  bool write_xlog)
{
	/* WAL encryption disabled - no-op */
}

/*
 * pg_tde_read_last_wal_range - Read the last WAL encryption range
 *
 * STUB: Returns NULL since WAL encryption is disabled.
 */
WalEncryptionRange *
pg_tde_read_last_wal_range(void)
{
	return NULL;
}

/*
 * pg_tde_save_server_key - Save server-wide WAL encryption key
 *
 * STUB: No-op since WAL encryption is disabled.
 */
void
pg_tde_save_server_key(const TDEPrincipalKey *principal_key, bool write_xlog)
{
	/* WAL encryption disabled - no-op */
}

/*
 * pg_tde_save_server_key_redo - WAL redo for server key save
 *
 * STUB: No-op since WAL encryption is disabled.
 */
void
pg_tde_save_server_key_redo(const TDESignedPrincipalKeyInfo *signed_key_info)
{
	/* WAL encryption disabled - no-op */
}

/*
 * pg_tde_wal_last_range_set_location - Set location for last WAL range
 *
 * STUB: No-op since WAL encryption is disabled.
 */
void
pg_tde_wal_last_range_set_location(WalLocation loc)
{
	/* WAL encryption disabled - no-op */
}

/*
 * pg_tde_wal_cache_extra_palloc - Extra palloc for WAL cache
 *
 * STUB: No-op since WAL cache is not used without WAL encryption.
 */
void
pg_tde_wal_cache_extra_palloc(void)
{
	/* WAL encryption disabled - no-op */
}
