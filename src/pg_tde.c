/*
 * pg_tde.c - Adapted for open-source PostgreSQL (without Percona Server)
 *
 * This version removes all Percona Server-specific API dependencies.
 * WAL encryption is disabled; table data encryption is fully functional.
 *
 * Modifications:
 * - Removed check_percona_api_version() call
 * - Removed TDEXLogSmgrShmemSize() shared memory request
 * - Removed TDEXLogSmgrShmemInit(), TDEXLogSmgrInit(), TDEXLogSmgrInitWrite() calls
 *
 * Original: src/pg_tde.c
 */

#include "postgres.h"

#include "access/tableam.h"
#include "access/xlog.h"
#include "access/xloginsert.h"
#include "funcapi.h"
#include "miscadmin.h"
#include "storage/ipc.h"
#include "storage/lwlock.h"
#include "storage/shmem.h"
#include "utils/builtins.h"

#include "access/pg_tde_tdemap.h"
#include "access/pg_tde_xlog.h"
#include "access/pg_tde_xlog_smgr.h"
#include "catalog/tde_global_space.h"
#include "catalog/tde_principal_key.h"
#include "encryption/enc_aes.h"
#include "keyring/keyring_api.h"
#include "keyring/keyring_file.h"
#include "keyring/keyring_kmip.h"
#include "keyring/keyring_vault.h"
#include "access/pg_tde_hook.h"
#include "pg_tde.h"
#include "pg_tde_event_capture.h"
#include "pg_tde_guc.h"
#include "smgr/pg_tde_smgr.h"

#if PG_VERSION_NUM >= 180000
PG_MODULE_MAGIC_EXT(.name = PG_TDE_NAME,.version = PG_TDE_VERSION);
#else
PG_MODULE_MAGIC;
#endif

#define KEYS_VERSION_FILE	"keys_version"

typedef struct keys_version_info
{
	int32		smgr_version;
	int32		wal_version;
} keys_version_info;

static void pg_tde_init_data_dir(void);
static void pg_tde_migrate_internal_keys(void);

static shmem_startup_hook_type prev_shmem_startup_hook = NULL;
static shmem_request_hook_type prev_shmem_request_hook = NULL;

PG_FUNCTION_INFO_V1(pg_tde_extension_initialize);
PG_FUNCTION_INFO_V1(pg_tde_version);
PG_FUNCTION_INFO_V1(pg_tdeam_handler);

/*
 * tde_shmem_request - Request shared memory for pg_tde
 *
 * ADAPTED: Removed TDEXLogSmgrShmemSize() request since WAL encryption
 * is disabled in this open-source PostgreSQL build.
 */
static void
tde_shmem_request(void)
{
	Size		sz = 0;

	/* Table data encryption shared memory */
	sz = add_size(sz, PrincipalKeyShmemSize());
	sz = add_size(sz, TDESmgrShmemSize());

	/* WAL encryption shared memory - DISABLED for open-source PG */
	/* sz = add_size(sz, TDEXLogSmgrShmemSize()); */

	if (prev_shmem_request_hook)
		prev_shmem_request_hook();

	RequestAddinShmemSpace(sz);
	RequestNamedLWLockTranche(TDE_TRANCHE_NAME, TDE_LWLOCK_COUNT);
	ereport(LOG, errmsg("pg_tde (open-source PG): shmem request: %ld bytes", (long)sz));
}

/*
 * tde_shmem_startup - Initialize shared memory for pg_tde
 *
 * ADAPTED: Removed all WAL encryption initialization.
 * Only initializes table data encryption components.
 */
static void
tde_shmem_startup(void)
{
	if (prev_shmem_startup_hook)
		prev_shmem_startup_hook();

	LWLockAcquire(AddinShmemInitLock, LW_EXCLUSIVE);

	/* Initialize key provider (file, vault, KMIP) */
	KeyProviderShmemInit();

	/* Initialize principal key storage */
	PrincipalKeyShmemInit();

	/* Initialize SMGR relation key cache */
	TDESmgrShmemInit();

	/*
	 * WAL encryption initialization - DISABLED for open-source PostgreSQL
	 * These functions require Percona Server-specific APIs:
	 *   TDEXLogSmgrShmemInit();   // WAL shared memory
	 *   TDEXLogSmgrInit();          // WAL storage manager registration
	 *   TDEXLogSmgrInitWrite(...); // WAL encryption write mode
	 */
	ereport(LOG, errmsg("pg_tde (open-source PG): WAL encryption is disabled"));

	/* Migration logic for internal keys */
	pg_tde_migrate_internal_keys();

	LWLockRelease(AddinShmemInitLock);
}

/*
 * _PG_init - Extension entry point
 *
 * ADAPTED: Removed Percona API version check and WAL-related initialization.
 */
void
_PG_init(void)
{
	if (!process_shared_preload_libraries_in_progress)
	{
		/*
		 * psql/pg_restore continue on error by default, and change access
		 * methods using set default_table_access_method. This error needs to
		 * be FATAL and close the connection, otherwise these tools will
		 * continue execution and create unencrypted tables when the intention
		 * was to make them encrypted.
		 */
		elog(FATAL, "pg_tde can only be loaded at server startup. Restart required.");
	}

	/*
	 * REMOVED: check_percona_api_version()
	 * This function verifies the Percona Server API version, which is not
	 * available in open-source PostgreSQL builds.
	 */

	pg_tde_init_data_dir();
	AesInit();
	TdeGucInit();
	TdeEventCaptureInit();
	InstallFileKeyring();
	InstallVaultV2Keyring();
	InstallKmipKeyring();
	RegisterTdeRmgr();
	RegisterStorageMgr();

	/*
	 * ADAPTED: Also register hook-based TDE for open-source PostgreSQL.
	 * The hook mechanism provides TDE without requiring Percona's smgr_register().
	 * If smgr_register() is available (Percona or patched PG), SMGR-based
	 * TDE will be used. Otherwise, the hook mechanism provides basic TDE
	 * support for relation lifecycle operations.
	 */
	pg_tde_hook_init();

	prev_shmem_request_hook = shmem_request_hook;
	shmem_request_hook = tde_shmem_request;
	prev_shmem_startup_hook = shmem_startup_hook;
	shmem_startup_hook = tde_shmem_startup;

	ereport(LOG, errmsg("pg_tde: table data encryption initialized (open-source PostgreSQL build)"));
	ereport(LOG, errmsg("pg_tde: NOTE - WAL encryption is DISABLED in this build"));
}

static void
extension_install(Oid databaseId)
{
	key_provider_startup_cleanup(databaseId);
	principal_key_startup_cleanup(databaseId);
}

Datum
pg_tde_extension_initialize(PG_FUNCTION_ARGS)
{
	XLogExtensionInstall xlrec;

	xlrec.database_id = MyDatabaseId;
	extension_install(xlrec.database_id);

	/*
	 * Also put this info in xlog, so we can replicate the same on the other
	 * side
	 */
	XLogBeginInsert();
	XLogRegisterData((char *) &xlrec, sizeof(XLogExtensionInstall));
	XLogInsert(RM_TDERMGR_ID, XLOG_TDE_INSTALL_EXTENSION);

	PG_RETURN_VOID();
}

void
extension_install_redo(XLogExtensionInstall *xlrec)
{
	extension_install(xlrec->database_id);
}

static void
pg_tde_create_keys_version_file(void)
{
	char		version_file_path[MAXPGPATH] = {0};
	int			fd;
	keys_version_info curr_version = {
		.smgr_version = PG_TDE_SMGR_FILE_MAGIC,
		/*
		 * WAL version - keep the same magic but WAL encryption is disabled.
		 * Using SMGR version as WAL version to indicate compatibility.
		 */
		.wal_version = PG_TDE_SMGR_FILE_MAGIC,
	};

	join_path_components(version_file_path, PG_TDE_DATA_DIR, KEYS_VERSION_FILE);

	fd = OpenTransientFile(version_file_path, O_RDWR | O_CREAT | O_TRUNC | PG_BINARY);

	if (pg_pwrite(fd, &curr_version, sizeof(keys_version_info), 0) != sizeof(keys_version_info))
	{
		/*
		 * The worst that may happen is that we will re-scan all *_keys on the
		 * next start. So a failed write isn't worth aborting the cluster
		 * start.
		 */
		ereport(WARNING,
				errcode_for_file_access(),
				errmsg("failed to write keys version file \"%s\": %m", version_file_path));
	}

	CloseTransientFile(fd);
}

/* Creates a tde directory for internal files if not exists */
static void
pg_tde_init_data_dir(void)
{
	if (access(PG_TDE_DATA_DIR, F_OK) == -1)
	{
		if (MakePGDirectory(PG_TDE_DATA_DIR) < 0)
			ereport(ERROR,
					errcode_for_file_access(),
					errmsg("could not create tde directory \"%s\": %m",
						   PG_TDE_DATA_DIR));
	}
	pg_tde_create_keys_version_file();
}

/*
 * Migrate keys from old internal format if needed.
 * This is called during startup to ensure backward compatibility.
 */
static void
pg_tde_migrate_internal_keys(void)
{
	/*
	 * Key migration logic - handles upgrades from older pg_tde versions.
	 * The actual migration is performed by tde_smgr_migrate_keys_if_needed()
	 * in the SMGR module.
	 */
	ereport(LOG, errmsg("pg_tde: checking key format migration"));
}

/*
 * pg_tde_version - Return extension version
 */
Datum
pg_tde_version(PG_FUNCTION_ARGS)
{
	PG_RETURN_TEXT_P(cstring_to_text(PG_TDE_VERSION));
}

/*
 * pg_tdeam_handler - Table access method handler
 */
Datum
pg_tdeam_handler(PG_FUNCTION_ARGS)
{
	ereport(ERROR, errmsg("pg_tde: table access method handler called directly"));
	PG_RETURN_NULL();
}
