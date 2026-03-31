# pg_tde Open Source PostgreSQL Adaptation

> **⚠️ Build Status**: This repository contains the adapted source code for compiling pg_tde with open-source PostgreSQL (without Percona Server). The WAL encryption feature is disabled in this build. Table data encryption is fully functional.

## 功能说明 | Feature Overview

本适配版本基于 Percona pg_tde v2.1.2，移除了对 Percona Server 特有 API 的依赖，使表数据加密功能可以在开源 PostgreSQL 上编译运行。

| 功能 | 状态 | 说明 |
|------|------|------|
| 表数据加密 | ✅ 已启用 | 透明加密读写，完整功能 |
| WAL 日志加密 | ❌ 已禁用 | 需要 Percona API (`xlog_smgr.h`) |
| 主密钥管理 | ✅ 正常工作 | 支持文件密钥提供者 |
| 密钥映射 | ✅ 正常工作 | 两级密钥架构 |
| Vault 密钥提供者 | ✅ 已编译 | HashiCorp Vault V2 |
| KMIP 密钥提供者 | ⚠️ 已排除 | 依赖 libkmip，可选 |

## 环境要求 | Requirements

- **PostgreSQL**: 15 / 16 / 17 / 18
- **操作系统**: Linux (Debian/Ubuntu/CentOS)
- **依赖**: `libssl-dev`, `libkrb5-dev`, `libcurl4-openssl-dev`, `zlib1g-dev`, `libreadline-dev`

### 安装 PostgreSQL 开发包

**Ubuntu/Debian:**
```bash
# Add PostgreSQL APT repository
curl -fsSL https://www.postgresql.org/media/keys/ACCC4CF8.asc | gpg --dearmor -o /usr/share/keyrings/postgresql.gpg
echo "deb [arch=amd64 signed-by=/usr/share/keyrings/postgresql.gpg] https://apt.postgresql.org/pub/repos/apt $(lsb_release -cs)-pgdg main" > /etc/apt/sources.list.d/pgdg.list
apt-get update
apt-get install -y postgresql-server-dev-17 libssl-dev libkrb5-dev libcurl4-openssl-dev zlib1g-dev libreadline-dev
```

**CentOS/RHEL:**
```bash
yum install -y https://download.postgresql.org/pub/repos/yum/reporpms/EL-$(rpm -E '%{rhel}')-x86_64/pgdg-redhat-repo-latest.noarch.rpm
yum install -y postgresql17-devel openssl-devel krb5-devel libcurl-devel zlib-devel readline-devel
```

## 快速开始 | Quick Start

```bash
# 1. Clone this repository
git clone https://github.com/YOUR_USERNAME/pg_tde_open_src_pg.git
cd pg_tde_open_src_pg

# 2. Build
make -f Makefile.open_src_pg PG_CONFIG=/usr/lib/postgresql/17/bin/pg_config

# Or if pg_config is in PATH:
export PATH="/usr/lib/postgresql/17/bin:$PATH"
make -f Makefile.open_src_pg

# 3. Install
sudo make -f Makefile.open_src_pg install

# 4. Configure PostgreSQL
# Add to postgresql.conf:
# shared_preload_libraries = 'pg_tde'

# 5. Restart PostgreSQL
sudo systemctl restart postgresql

# 6. Create extension
psql -U postgres -c "CREATE EXTENSION pg_tde;"

# 7. Create master key
psql -U postgres -c "SELECT pg_tde_create_key_using_database_key_provider();"

# 8. Create encrypted table
psql -U postgres -c "CREATE TABLE secret (id int, data text) USING pg_tde;"
psql -U postgres -c "INSERT INTO secret VALUES (1, 'Hello World');"
psql -U postgres -c "SELECT * FROM secret;"
```

## 构建选项 | Build Options

```bash
# Full build with version check
make -f Makefile.open_src_pg check-pg-version

# Show build info
make -f Makefile.open_src_pg info

# Clean build
make -f Makefile.open_src_pg clean

# Run regression tests (requires a running PostgreSQL instance)
make -f Makefile.open_src_pg installcheck
```

## 文件修改说明 | Code Modification Summary

以下文件经过修改以移除 Percona Server 特有 API 依赖：

| 文件 | 修改类型 | 说明 |
|------|----------|------|
| `src/pg_tde.c` | 重写 | 移除 Percona API 版本检查、WAL 初始化调用 |
| `src/access/pg_tde_xlog_smgr.c` | 重写为 Stub | WAL 存储管理器桩函数（空实现） |
| `src/access/pg_tde_xlog_keys.c` | 重写为 Stub | WAL 密钥管理函数桩（空实现） |
| `src/access/xlog_smgr.h` | 新增 | WAL SMGR 头文件桩（兼容层） |
| `Makefile.open_src_pg` | 新增 | 适配后的 Makefile，移除 WAL/KMIP 对象文件 |

### 详细修改说明

#### 1. `src/pg_tde.c` 修改

**移除的内容：**
- `#include "utils/percona.h"` — Percona API 版本检查头文件
- `check_percona_api_version()` — Percona API 版本检查调用
- `TDEXLogSmgrShmemSize()` — WAL 共享内存请求
- `TDEXLogSmgrShmemInit()` — WAL 共享内存初始化
- `TDEXLogSmgrInit()` — WAL 存储管理器注册
- `TDEXLogSmgrInitWrite()` — WAL 加密写入初始化

**新增内容：**
- `PG_TDE_OPEN_SOURCE_BUILD` 编译宏定义
- 启动日志说明 WAL 加密已禁用
- 向后兼容的密钥迁移逻辑

#### 2. `src/access/pg_tde_xlog_smgr.c` 修改

原始文件（约 600 行）完全重写为桩实现（约 80 行）：

```c
// 原始：调用 Percona SetXLogSmgr() 注册自定义 WAL 存储管理器
// 适配后：所有函数为空实现，记录日志说明 WAL 加密不可用

void TDEXLogSmgrInit(void) {
    ereport(LOG, errmsg("pg_tde: WAL storage manager not initialized (Percona API not available)"));
}
```

#### 3. `src/access/pg_tde_xlog_keys.c` 修改

所有 WAL 密钥管理函数重写为空实现：

```c
// 原始：pg_tde_fetch_wal_keys() 从密钥映射文件读取 WAL 密钥
// 适配后：直接返回 NULL（WAL 加密不可用）

WALKeyCacheRec *pg_tde_fetch_wal_keys(WalLocation start) {
    return NULL;
}
```

#### 4. `Makefile.open_src_pg` 修改

```makefile
# 移除的对象文件（Percona 特有）：
#   src/access/pg_tde_xlog_smgr.o  - WAL 存储管理器
#   src/access/pg_tde_xlog_keys.o  - WAL 密钥管理

# 保留的对象文件（与开源 PG 兼容）：
#   src/encryption/enc_tde.o       - 流加密
#   src/encryption/enc_aes.o       - AES 算法
#   src/access/pg_tde_tdemap.o     - 密钥映射
#   src/access/pg_tde_xlog.o       - WAL 资源管理器（基础功能）
#   src/smgr/pg_tde_smgr.o         - 表数据加密核心
#   src/catalog/tde_principal_key.o - 主密钥管理
#   ... 其他密钥提供者文件
```

## 已知限制 | Known Limitations

1. **WAL 日志未加密** — 表数据的变更会在 WAL 中以某种形式记录，敏感场景不建议使用
2. **索引加密状态** — 索引与主表共用密钥，删除加密属性时索引会被重建
3. **全文检索** — 全文检索索引会被加密，性能可能下降
4. **大对象** — TOAST 大对象按块加密，粒度为块级

## 架构说明 | Architecture

```
┌──────────────────────────────────────────────────────────┐
│            Open Source PostgreSQL + pg_tde               │
├──────────────────────────────────────────────────────────┤
│                                                          │
│  SQL Layer → Access Method Layer → TDE SMGR Layer        │
│                                        │                 │
│                              ┌─────────┴──────┐          │
│                              │ pg_tde_smgr.c  │          │
│                              │ (表数据加密)   │          │
│                              └─────────┬──────┘          │
│                                        │                 │
│                              ┌─────────┴──────┐          │
│                              │  md.c (文件)   │          │
│                              │ (开源PG原生)   │          │
│                              └────────────────┘          │
│                                                          │
│  WAL Layer ────────────────── [DISABLED in this build]  │
│  ↑ WAL 加密需要 Percona SetXLogSmgr()，本版本已禁用   │
│                                                          │
│  Key Management Layer                                    │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐      │
│  │ 主密钥管理   │  │ 密钥映射    │  │ 密钥提供者  │      │
│  │ (正常)      │  │ (正常)     │  │ (文件/Vault)│      │
│  └─────────────┘  └─────────────┘  └─────────────┘      │
└──────────────────────────────────────────────────────────┘
```

## 测试说明 | Testing

回归测试（部分测试因 WAL 加密禁用而跳过）：

```bash
# 安装扩展
make -f Makefile.open_src_pg install

# 配置 PG 路径
export PGPORT=5432
export PGHOST=/var/run/postgresql
make -f Makefile.open_src_pg installcheck

# 预期结果：大部分测试通过，以下测试预期失败/跳过：
#   - vault_v2_test（需要 Vault 服务器）
#   - kmip_test（KMIP 库未编译）
```

## 与 Percona pg_tde 的差异 | Differences from Percona pg_tde

| 功能 | Percona pg_tde | 本适配版本 |
|------|----------------|-----------|
| 表数据加密 | ✅ | ✅ |
| WAL 加密 | ✅ | ❌ |
| 主密钥管理 | ✅ | ✅ |
| 表级密钥 | ✅ | ✅ |
| Percona Server 依赖 | 必须 | 无 |
| 开源 PostgreSQL 支持 | ❌ | ✅ (15/16/17/18) |
| 密钥提供者 | 文件/Vault/KMIP | 文件/Vault |

## 故障排查 | Troubleshooting

### 编译错误：`fatal error: access/xlog_smgr.h: No such file or directory`

这是正常的——原始 Percona pg_tde 包含此头文件但开源 PostgreSQL 中不存在。使用本仓库的适配代码即可解决。

### 编译错误：大量 `unknown type` 错误

PostgreSQL 开发包未安装。安装 `postgresql-server-dev-17` 或对应版本。

### 运行时错误：`pg_tde can only be loaded at server startup`

pg_tde 必须通过 `shared_preload_libraries` 在服务器启动时加载，不能通过 `CREATE EXTENSION` 加载。

### 运行时错误：`principal key not configured`

创建加密表之前必须先创建主密钥：
```sql
SELECT pg_tde_create_key_using_database_key_provider();
```

## 贡献 | Contributing

欢迎提交 Issue 和 Pull Request！

## 许可证 | License

继承 Percona pg_tde 的许可证（PostgreSQL License）。

## 参考链接 | References

- Percona pg_tde: https://github.com/percona/pg_tde
- 设计文档: `doc/pg_tde_open_source_pg_design.md`
- 兼容性分析: `doc/compatibility_analysis.md`
