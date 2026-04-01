/* src/include/access/pg_tde_hook.h */

#ifndef PG_TDE_HOOK_H
#define PG_TDE_HOOK_H

/* Hook-based TDE functions (open-source PostgreSQL compatible) */

/*
 * pg_tde_hook_init - Register all pg_tde hooks
 *
 * Called from _PG_init() during shared_preload_libraries initialization.
 * Sets up:
 *   - Shared memory for key registry
 *   - object_access_hook (OAT_DROP, OAT_CLUSTER, OAT_VACUUM_FULL)
 *   - file_acess_hook (fork I/O interception)
 */
extern void pg_tde_hook_init(void);

/*
 * pg_tde_hook_shutdown - Cleanup hooks at server shutdown
 */
extern void pg_tde_hook_shutdown(void);

/*
 * SQL-callable TDE management functions
 */
extern Datum pg_tde_add_relation_key(PG_FUNCTION_ARGS);
extern Datum pg_tde_remove_relation_key(PG_FUNCTION_ARGS);
extern Datum pg_tde_is_encrypted(PG_FUNCTION_ARGS);
extern Datum pg_tde_list_encrypted_relations(PG_FUNCTION_ARGS);

#endif /* PG_TDE_HOOK_H */
