#!/bin/bash
#==============================================================================
# pg_tde Open Source PostgreSQL Build Script
# 自动化构建脚本 - 在具备 PostgreSQL 开发环境的机器上运行
#==============================================================================
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

log_info()  { echo -e "${GREEN}[INFO]${NC} $*"; }
log_warn()  { echo -e "${YELLOW}[WARN]${NC} $*"; }
log_error() { echo -e "${RED}[ERROR]${NC} $*" >&2; }

echo "=============================================="
echo " pg_tde Open Source PostgreSQL Build Script"
echo "=============================================="

#------------------------------------------------------------------------------
# Step 1: Detect PostgreSQL installation
#------------------------------------------------------------------------------
log_info "Step 1: Detecting PostgreSQL installation..."

PG_CONFIG=""
for dir in \
    "/usr/lib/postgresql/17/bin/pg_config" \
    "/usr/lib/postgresql/16/bin/pg_config" \
    "/usr/pgsql-17/bin/pg_config" \
    "/usr/pgsql-16/bin/pg_config" \
    "/usr/local/pgsql/bin/pg_config" \
    "pg_config"; do
    if [ -x "$(dirname "$dir")/pg_config" ] || command -v "$dir" &>/dev/null; then
        PG_CONFIG="$dir"
        break
    fi
done

if [ -z "$PG_CONFIG" ]; then
    log_error "pg_config not found. Please install PostgreSQL development packages."
    log_error "On Ubuntu/Debian: apt-get install postgresql-server-dev-17"
    log_error "On CentOS/RHEL: yum install postgresql17-devel"
    exit 1
fi

PG_CONFIG="$(which "$PG_CONFIG" 2>/dev/null || echo "$PG_CONFIG")"
PG_VERSION=$("$PG_CONFIG" --version 2>/dev/null | grep -oP '\d+' | head -1)
PG_SERVER_INC=$("$PG_CONFIG" --includedir-server)

log_info "Found: $($PG_CONFIG --version)"
log_info "Server include: $PG_SERVER_INC"

# Check PG version
if [ "$PG_VERSION" -lt 15 ]; then
    log_error "PostgreSQL 15+ required. Found: $($PG_CONFIG --version)"
    exit 1
fi

#------------------------------------------------------------------------------
# Step 2: Check required dependencies
#------------------------------------------------------------------------------
log_info "Step 2: Checking dependencies..."

MISSING_DEPS=""
for dep in libssl libkrb5 libcurl zlib readline; do
    pkg="${dep}-dev"
    if ! dpkg -l | grep -q "^ii.*${pkg}" 2>/dev/null && \
       ! rpm -q "${dep}-devel" &>/dev/null; then
        MISSING_DEPS="$MISSING_DEPS $pkg"
    fi
done

if [ -n "$MISSING_DEPS" ]; then
    log_warn "Missing dependencies:$MISSING_DEPS"
    log_warn "On Ubuntu/Debian, run:"
    log_warn "  sudo apt-get install$MISSING_DEPS"
    log_warn "Continuing anyway (build may fail)..."
fi

#------------------------------------------------------------------------------
# Step 3: Verify server headers exist
#------------------------------------------------------------------------------
log_info "Step 3: Verifying PostgreSQL server headers..."

if [ ! -f "$PG_SERVER_INC/storage/smgr.h" ]; then
    log_error "Server headers not found at: $PG_SERVER_INC"
    log_error "Please install postgresql-server-dev package:"
    log_error "  sudo apt-get install postgresql-server-dev-$PG_VERSION"
    exit 1
fi
log_info "Server headers OK: $PG_SERVER_INC"

#------------------------------------------------------------------------------
# Step 4: Copy Makefile
#------------------------------------------------------------------------------
log_info "Step 4: Setting up build..."

cd "$PROJECT_DIR"

if [ ! -f Makefile.open_src_pg ]; then
    log_error "Makefile.open_src_pg not found!"
    exit 1
fi

# Backup original Makefile and use our adapted one
if [ -f Makefile ] && ! cp Makefile Makefile.percona_backup 2>/dev/null; then
    log_warn "Could not backup original Makefile"
fi
cp Makefile.open_src_pg Makefile

log_info "Using Makefile.open_src_pg"

#------------------------------------------------------------------------------
# Step 5: Clean
#------------------------------------------------------------------------------
log_info "Step 5: Cleaning previous build..."
make clean PG_CONFIG="$PG_CONFIG" 2>/dev/null || true

#------------------------------------------------------------------------------
# Step 6: Build
#------------------------------------------------------------------------------
log_info "Step 6: Building pg_tde..."
log_info "This may take a few minutes..."

BUILD_OUTPUT=$(make PG_CONFIG="$PG_CONFIG" 2>&1) || true
echo "$BUILD_OUTPUT" | tail -30

if echo "$BUILD_OUTPUT" | grep -qi "error\|undefined reference"; then
    log_error "Build failed. See errors above."
    
    # Show summary of errors
    ERRORS=$(echo "$BUILD_OUTPUT" | grep -E "^.*error:|undefined reference" | head -20)
    if [ -n "$ERRORS" ]; then
        log_error "Error summary:"
        echo "$ERRORS" | while read -r line; do log_error "  $line"; done
    fi
    exit 1
fi

if echo "$BUILD_OUTPUT" | grep -qi "warning.*implicit\|warning.*deprecated"; then
    WARN_COUNT=$(echo "$BUILD_OUTPUT" | grep -c "warning" || echo "0")
    log_warn "Build completed with $WARN_COUNT warnings (this is OK)"
else
    log_info "Build completed successfully!"
fi

#------------------------------------------------------------------------------
# Step 7: Verify built artifacts
#------------------------------------------------------------------------------
log_info "Step 7: Verifying build artifacts..."

if [ -f "src/pg_tde_smgr.o" ]; then
    log_info "✅ pg_tde_smgr.o built"
fi
if [ -f "src/encryption/enc_aes.o" ]; then
    log_info "✅ enc_aes.o built"
fi
if [ -f "src/catalog/tde_principal_key.o" ]; then
    log_info "✅ tde_principal_key.o built"
fi

# Check for shared library
SO_FILE=$(find . -name "pg_tde.so" -o -name "pg_tde.sl" 2>/dev/null | head -1)
if [ -n "$SO_FILE" ]; then
    log_info "✅ Extension library: $SO_FILE ($(du -sh "$SO_FILE" | cut -f1))"
else
    log_warn "No .so file found. Check if MODULE_big is correct."
fi

#------------------------------------------------------------------------------
# Step 8: Installation
#------------------------------------------------------------------------------
log_info ""
log_info "=============================================="
log_info " Build Successful!"
log_info "=============================================="
log_info ""
log_info "To install the extension:"
log_info ""
log_info "  # Install (requires root)"
log_info "  sudo make PG_CONFIG=\"$PG_CONFIG\" install"
log_info ""
log_info "  # Configure PostgreSQL (add to postgresql.conf):"
log_info "  #   shared_preload_libraries = 'pg_tde'"
log_info ""
log_info "  # Restart PostgreSQL"
log_info "  sudo systemctl restart postgresql"
log_info ""
log_info "  # Create extension (as superuser)"
log_info "  psql -U postgres -c \"CREATE EXTENSION pg_tde;\""
log_info ""
log_info "  # Create master key"
log_info "  psql -U postgres -c \"SELECT pg_tde_create_key_using_database_key_provider();\""
log_info ""
log_info "  # Create encrypted table"
log_info "  psql -U postgres -c \"CREATE TABLE t1 (id int, data text) USING pg_tde;\""
log_info "  psql -U postgres -c \"INSERT INTO t1 VALUES (1, 'Hello TDE!');\""
log_info "  psql -U postgres -c \"SELECT * FROM t1;\""
log_info ""

echo "$BUILD_OUTPUT" | grep -E "pg_tde|error" | head -20 || true
