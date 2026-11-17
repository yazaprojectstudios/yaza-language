#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
COMPILER=${YAZA_LLVM_COMPILER:-"$PROJECT_DIR/build/yaza-llvm"}
CLANG=${CLANG:-clang}
case "$COMPILER" in
  /*) ;;
  *) COMPILER="$(pwd)/$COMPILER" ;;
esac
BUILD_DIR=$(mktemp -d "${TMPDIR:-/tmp}/yazabase-phase0-sanitizers.XXXXXX")
trap 'rm -rf "$BUILD_DIR"' EXIT HUP INT TERM

if [ ! -x "$COMPILER" ]; then
  printf 'error: executable YAZA LLVM compiler not found: %s\n' "$COMPILER" >&2
  exit 127
fi
if ! command -v "$CLANG" >/dev/null 2>&1; then
  printf 'error: Clang executable not found: %s\n' "$CLANG" >&2
  exit 127
fi

# Compiler-host planning needs more arena than the fixture executables. Limit
# the override to compiler invocations so ASan/UBSan programs keep their own
# runtime profile.
COMPILER_EXECUTABLE=$COMPILER
COMPILER_PROCESS_LIMIT_BYTES=${YAZA_COMPILER_PROCESS_LIMIT_BYTES:-34359738368}
COMPILER_LIST_VECTOR_LIMIT_BYTES=${YAZA_COMPILER_LIST_VECTOR_LIMIT_BYTES:-34359738368}
COMPILER_MAX_LIST_ITEMS=${YAZA_COMPILER_MAX_LIST_ITEMS:-32000000}
export YAZA_COMPILER_EXECUTABLE=$COMPILER_EXECUTABLE
export YAZA_COMPILER_PROCESS_LIMIT_BYTES=$COMPILER_PROCESS_LIMIT_BYTES
export YAZA_COMPILER_LIST_VECTOR_LIMIT_BYTES=$COMPILER_LIST_VECTOR_LIMIT_BYTES
export YAZA_COMPILER_MAX_LIST_ITEMS=$COMPILER_MAX_LIST_ITEMS
COMPILER_WRAPPER=$BUILD_DIR/yaza-compiler-wrapper
cat >"$COMPILER_WRAPPER" <<'EOF'
#!/bin/sh
YAZA_RT_V1_PROCESS_LIMIT_BYTES="$YAZA_COMPILER_PROCESS_LIMIT_BYTES" \
YAZA_RT_V1_MAX_LIST_VECTOR_BYTES="$YAZA_COMPILER_LIST_VECTOR_LIMIT_BYTES" \
YAZA_RT_V1_MAX_LIST_ITEMS="$YAZA_COMPILER_MAX_LIST_ITEMS" \
  exec "$YAZA_COMPILER_EXECUTABLE" "$@"
EOF
chmod +x "$COMPILER_WRAPPER"
COMPILER=$COMPILER_WRAPPER

run_disk_quota_sanitized() {
  quota_limit_bytes=$1
  executable=$2
  expected=$3
  root=$4
  mode=$5
  if ! quota_output=$(ASAN_OPTIONS=detect_leaks=0 YAZA_DB_MAX_DISK_BYTES="$quota_limit_bytes" "$executable" "$root" "$mode"); then
    printf 'error: sanitized disk-quota fixture failed: %s\n' "$mode" >&2
    printf '%s\n' "$quota_output" >&2
    return 1
  fi
  printf '%s\n' "$quota_output"
  case "$quota_output" in
    *"$expected"*) ;;
    *)
      printf 'error: sanitized disk-quota fixture did not report its success marker (%s)\n' "$expected" >&2
      return 1
      ;;
  esac
}

cd "$PROJECT_DIR"
for fixture in \
  core-float64-bits-smoke \
  yazabase-sql-lexer-v1-smoke \
  yazabase-sql-parser-v1-smoke \
  yazabase-sql-parser-v2-smoke \
  yazabase-sql-parser-v3-smoke \
  yazabase-sql-create-index-parser-v1-smoke \
  yazabase-sql-parser-fuzz-v1-smoke \
  yazabase-storage-decoder-fuzz-v1-smoke \
  yazabase-sql-insert-v1-smoke \
  yazabase-sql-create-table-v1-smoke \
  yazabase-sql-insert-bind-v1-smoke \
  yazabase-sql-insert-exec-v1-smoke \
  yazabase-sql-binder-v1-smoke \
  yazabase-sql-explain-v1-smoke \
  yazabase-sql-executor-v1-smoke \
  yazabase-sql-inner-join-memory-v1-smoke \
  yazabase-sql-multi-join-memory-v1-smoke \
  yazabase-key-v1-smoke \
  yazabase-page-v1-smoke \
  yazabase-page-wal-v1-smoke \
  yazabase-node-split-wal-v1-smoke \
  yazabase-meta-free-v1-smoke \
  yazabase-catalog-v1-smoke \
  yazabase-row-v1-smoke \
  yazabase-mvcc-v1-smoke \
  yazabase-overflow-v1-smoke \
  yazabase-fallible-parse-smoke; do
  LLVM_IR="$BUILD_DIR/$fixture.ll"
  OBJECT="$BUILD_DIR/$fixture.o"
  OUTPUT="$BUILD_DIR/$fixture"
  YAZA_STDLIB_DIR="$PROJECT_DIR/std" \
    "$COMPILER" emit-llvm "examples/$fixture.yz" -o "$LLVM_IR"
  "$CLANG" -Wno-override-module -fsanitize=address,undefined \
    -x ir -c "$LLVM_IR" -o "$OBJECT"
  "$CLANG" -std=c11 -Wall -Wextra -Werror -pthread \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    "$OBJECT" "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" \
    -I"$PROJECT_DIR/runtime/include" -lm -o "$OUTPUT"
  ASAN_OPTIONS=detect_leaks=0 "$OUTPUT"
done

WAL_IR="$BUILD_DIR/yazabase-wal-v1-smoke.ll"
WAL_OBJECT="$BUILD_DIR/yazabase-wal-v1-smoke.o"
WAL_OUTPUT="$BUILD_DIR/yazabase-wal-v1-smoke"
YAZA_STDLIB_DIR="$PROJECT_DIR/std" \
  "$COMPILER" emit-llvm examples/yazabase-wal-v1-smoke.yz -o "$WAL_IR"
"$CLANG" -Wno-override-module -fsanitize=address,undefined \
  -x ir -c "$WAL_IR" -o "$WAL_OBJECT"
"$CLANG" -std=c11 -Wall -Wextra -Werror -pthread \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  "$WAL_OBJECT" "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" \
  -I"$PROJECT_DIR/runtime/include" -lm -o "$WAL_OUTPUT"
mkdir "$BUILD_DIR/wal-root"
ASAN_OPTIONS=detect_leaks=0 "$WAL_OUTPUT" "$BUILD_DIR/wal-root"

WAL_ROTATION_IR="$BUILD_DIR/yazabase-wal-rotation-smoke.ll"
WAL_ROTATION_OBJECT="$BUILD_DIR/yazabase-wal-rotation-smoke.o"
WAL_ROTATION_OUTPUT="$BUILD_DIR/yazabase-wal-rotation-smoke"
YAZA_STDLIB_DIR="$PROJECT_DIR/std" \
  "$COMPILER" emit-llvm examples/yazabase-wal-rotation-smoke.yz \
  -o "$WAL_ROTATION_IR"
"$CLANG" -Wno-override-module -fsanitize=address,undefined \
  -x ir -c "$WAL_ROTATION_IR" -o "$WAL_ROTATION_OBJECT"
"$CLANG" -std=c11 -Wall -Wextra -Werror -pthread \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  "$WAL_ROTATION_OBJECT" "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" \
  -I"$PROJECT_DIR/runtime/include" -lm -o "$WAL_ROTATION_OUTPUT"
mkdir "$BUILD_DIR/wal-rotation-root"
YAZA_RT_V1_PROCESS_LIMIT_BYTES=1073741824 ASAN_OPTIONS=detect_leaks=0 \
  "$WAL_ROTATION_OUTPUT" "$BUILD_DIR/wal-rotation-root"

VFS_IR="$BUILD_DIR/yazabase-vfs-v1-smoke.ll"
VFS_OBJECT="$BUILD_DIR/yazabase-vfs-v1-smoke.o"
VFS_OUTPUT="$BUILD_DIR/yazabase-vfs-v1-smoke"
YAZA_STDLIB_DIR="$PROJECT_DIR/std" \
  "$COMPILER" emit-llvm examples/yazabase-vfs-v1-smoke.yz -o "$VFS_IR"
"$CLANG" -Wno-override-module -fsanitize=address,undefined \
  -x ir -c "$VFS_IR" -o "$VFS_OBJECT"
"$CLANG" -std=c11 -Wall -Wextra -Werror -pthread \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  "$VFS_OBJECT" "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" \
  -I"$PROJECT_DIR/runtime/include" -lm -o "$VFS_OUTPUT"
mkdir "$BUILD_DIR/vfs-root"
ASAN_OPTIONS=detect_leaks=0 "$VFS_OUTPUT" "$BUILD_DIR/vfs-root"

STORAGE_IR="$BUILD_DIR/yazabase-storage-v1-smoke.ll"
STORAGE_OBJECT="$BUILD_DIR/yazabase-storage-v1-smoke.o"
STORAGE_OUTPUT="$BUILD_DIR/yazabase-storage-v1-smoke"
YAZA_STDLIB_DIR="$PROJECT_DIR/std" \
  "$COMPILER" emit-llvm examples/yazabase-storage-v1-smoke.yz -o "$STORAGE_IR"
"$CLANG" -Wno-override-module -fsanitize=address,undefined \
  -x ir -c "$STORAGE_IR" -o "$STORAGE_OBJECT"
"$CLANG" -std=c11 -Wall -Wextra -Werror -pthread \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  "$STORAGE_OBJECT" "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" \
  -I"$PROJECT_DIR/runtime/include" -lm -o "$STORAGE_OUTPUT"
mkdir "$BUILD_DIR/storage-root"
ASAN_OPTIONS=detect_leaks=0 "$STORAGE_OUTPUT" "$BUILD_DIR/storage-root"

PAGE_IMAGE_IR="$BUILD_DIR/yazabase-page-image-v1-smoke.ll"
PAGE_IMAGE_OBJECT="$BUILD_DIR/yazabase-page-image-v1-smoke.o"
PAGE_IMAGE_OUTPUT="$BUILD_DIR/yazabase-page-image-v1-smoke"
YAZA_STDLIB_DIR="$PROJECT_DIR/std" \
  "$COMPILER" emit-llvm examples/yazabase-page-image-v1-smoke.yz \
  -o "$PAGE_IMAGE_IR"
"$CLANG" -Wno-override-module -fsanitize=address,undefined \
  -x ir -c "$PAGE_IMAGE_IR" -o "$PAGE_IMAGE_OBJECT"
"$CLANG" -std=c11 -Wall -Wextra -Werror -pthread \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  "$PAGE_IMAGE_OBJECT" "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" \
  -I"$PROJECT_DIR/runtime/include" -lm -o "$PAGE_IMAGE_OUTPUT"
mkdir "$BUILD_DIR/page-image-root"
ASAN_OPTIONS=detect_leaks=0 "$PAGE_IMAGE_OUTPUT" "$BUILD_DIR/page-image-root"

ALLOCATOR_IR="$BUILD_DIR/yazabase-allocator-v1-smoke.ll"
ALLOCATOR_OBJECT="$BUILD_DIR/yazabase-allocator-v1-smoke.o"
ALLOCATOR_OUTPUT="$BUILD_DIR/yazabase-allocator-v1-smoke"
YAZA_STDLIB_DIR="$PROJECT_DIR/std" \
  "$COMPILER" emit-llvm examples/yazabase-allocator-v1-smoke.yz \
  -o "$ALLOCATOR_IR"
"$CLANG" -Wno-override-module -fsanitize=address,undefined \
  -x ir -c "$ALLOCATOR_IR" -o "$ALLOCATOR_OBJECT"
"$CLANG" -std=c11 -Wall -Wextra -Werror -pthread \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  "$ALLOCATOR_OBJECT" "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" \
  -I"$PROJECT_DIR/runtime/include" -lm -o "$ALLOCATOR_OUTPUT"
mkdir "$BUILD_DIR/allocator-root"
ASAN_OPTIONS=detect_leaks=0 "$ALLOCATOR_OUTPUT" "$BUILD_DIR/allocator-root"

CHECKPOINT_IR="$BUILD_DIR/yazabase-checkpoint-v1-smoke.ll"
CHECKPOINT_OBJECT="$BUILD_DIR/yazabase-checkpoint-v1-smoke.o"
CHECKPOINT_OUTPUT="$BUILD_DIR/yazabase-checkpoint-v1-smoke"
YAZA_STDLIB_DIR="$PROJECT_DIR/std" \
  "$COMPILER" emit-llvm examples/yazabase-checkpoint-v1-smoke.yz \
  -o "$CHECKPOINT_IR"
"$CLANG" -Wno-override-module -fsanitize=address,undefined \
  -x ir -c "$CHECKPOINT_IR" -o "$CHECKPOINT_OBJECT"
"$CLANG" -std=c11 -Wall -Wextra -Werror -pthread \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  "$CHECKPOINT_OBJECT" "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" \
  -I"$PROJECT_DIR/runtime/include" -lm -o "$CHECKPOINT_OUTPUT"
mkdir "$BUILD_DIR/checkpoint-root"
ASAN_OPTIONS=detect_leaks=0 "$CHECKPOINT_OUTPUT" "$BUILD_DIR/checkpoint-root"

CAPACITY_GROWTH_IR="$BUILD_DIR/yazabase-capacity-growth-v1-smoke.ll"
CAPACITY_GROWTH_OBJECT="$BUILD_DIR/yazabase-capacity-growth-v1-smoke.o"
CAPACITY_GROWTH_OUTPUT="$BUILD_DIR/yazabase-capacity-growth-v1-smoke"
YAZA_STDLIB_DIR="$PROJECT_DIR/std" \
  "$COMPILER" emit-llvm examples/yazabase-capacity-growth-v1-smoke.yz \
  -o "$CAPACITY_GROWTH_IR"
"$CLANG" -Wno-override-module -fsanitize=address,undefined \
  -x ir -c "$CAPACITY_GROWTH_IR" -o "$CAPACITY_GROWTH_OBJECT"
"$CLANG" -std=c11 -Wall -Wextra -Werror -pthread \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  "$CAPACITY_GROWTH_OBJECT" "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" \
  -I"$PROJECT_DIR/runtime/include" -lm -o "$CAPACITY_GROWTH_OUTPUT"
mkdir "$BUILD_DIR/capacity-growth-root"
ASAN_OPTIONS=detect_leaks=0 "$CAPACITY_GROWTH_OUTPUT" \
  "$BUILD_DIR/capacity-growth-root"

DISK_QUOTA_IR="$BUILD_DIR/yazabase-disk-quota-v1-smoke.ll"
DISK_QUOTA_OBJECT="$BUILD_DIR/yazabase-disk-quota-v1-smoke.o"
DISK_QUOTA_OUTPUT="$BUILD_DIR/yazabase-disk-quota-v1-smoke"
YAZA_STDLIB_DIR="$PROJECT_DIR/std" \
  "$COMPILER" emit-llvm examples/yazabase-disk-quota-v1-smoke.yz \
  -o "$DISK_QUOTA_IR"
"$CLANG" -Wno-override-module -fsanitize=address,undefined \
  -x ir -c "$DISK_QUOTA_IR" -o "$DISK_QUOTA_OBJECT"
"$CLANG" -std=c11 -Wall -Wextra -Werror -pthread \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  "$DISK_QUOTA_OBJECT" "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" \
  -I"$PROJECT_DIR/runtime/include" -lm -o "$DISK_QUOTA_OUTPUT"
mkdir "$BUILD_DIR/disk-quota-create-root" "$BUILD_DIR/disk-quota-growth-root" "$BUILD_DIR/disk-quota-wal-root" "$BUILD_DIR/disk-quota-invalid-root"
run_disk_quota_sanitized 134217727 "$DISK_QUOTA_OUTPUT" \
  "yazabase-disk-quota-v1-ok database-create-denied-cleanly" \
  "$BUILD_DIR/disk-quota-create-root" create-denied
run_disk_quota_sanitized 134217728 "$DISK_QUOTA_OUTPUT" \
  "yazabase-disk-quota-v1-ok database-growth-denied-with-files-unchanged" \
  "$BUILD_DIR/disk-quota-growth-root" growth-denied
run_disk_quota_sanitized 134217728 "$DISK_QUOTA_OUTPUT" \
  "yazabase-disk-quota-v1-ok wal-segment-create-denied-cleanly" \
  "$BUILD_DIR/disk-quota-wal-root" wal-rotation-denied
run_disk_quota_sanitized invalid "$DISK_QUOTA_OUTPUT" \
  "yazabase-disk-quota-v1-ok invalid-config-rejected" \
  "$BUILD_DIR/disk-quota-invalid-root" invalid-config

LEAF_RECOVERY_IR="$BUILD_DIR/yazabase-leaf-recovery-v1-smoke.ll"
LEAF_RECOVERY_OBJECT="$BUILD_DIR/yazabase-leaf-recovery-v1-smoke.o"
LEAF_RECOVERY_OUTPUT="$BUILD_DIR/yazabase-leaf-recovery-v1-smoke"
YAZA_STDLIB_DIR="$PROJECT_DIR/std" \
  "$COMPILER" emit-llvm examples/yazabase-leaf-recovery-v1-smoke.yz \
  -o "$LEAF_RECOVERY_IR"
"$CLANG" -Wno-override-module -fsanitize=address,undefined \
  -x ir -c "$LEAF_RECOVERY_IR" -o "$LEAF_RECOVERY_OBJECT"
"$CLANG" -std=c11 -Wall -Wextra -Werror -pthread \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  "$LEAF_RECOVERY_OBJECT" "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" \
  -I"$PROJECT_DIR/runtime/include" -lm -o "$LEAF_RECOVERY_OUTPUT"
mkdir "$BUILD_DIR/leaf-recovery-root"
ASAN_OPTIONS=detect_leaks=0 "$LEAF_RECOVERY_OUTPUT" \
  "$BUILD_DIR/leaf-recovery-root"

NODE_SPLIT_RECOVERY_IR="$BUILD_DIR/yazabase-node-split-recovery-v1-smoke.ll"
NODE_SPLIT_RECOVERY_OBJECT="$BUILD_DIR/yazabase-node-split-recovery-v1-smoke.o"
NODE_SPLIT_RECOVERY_OUTPUT="$BUILD_DIR/yazabase-node-split-recovery-v1-smoke"
YAZA_STDLIB_DIR="$PROJECT_DIR/std" \
  "$COMPILER" emit-llvm examples/yazabase-node-split-recovery-v1-smoke.yz \
  -o "$NODE_SPLIT_RECOVERY_IR"
"$CLANG" -Wno-override-module -fsanitize=address,undefined \
  -x ir -c "$NODE_SPLIT_RECOVERY_IR" -o "$NODE_SPLIT_RECOVERY_OBJECT"
"$CLANG" -std=c11 -Wall -Wextra -Werror -pthread \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  "$NODE_SPLIT_RECOVERY_OBJECT" "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" \
  -I"$PROJECT_DIR/runtime/include" -lm -o "$NODE_SPLIT_RECOVERY_OUTPUT"
mkdir "$BUILD_DIR/node-split-recovery-root"
ASAN_OPTIONS=detect_leaks=0 "$NODE_SPLIT_RECOVERY_OUTPUT" \
  "$BUILD_DIR/node-split-recovery-root"

TRANSACTION_IR="$BUILD_DIR/yazabase-transaction-v1-smoke.ll"
TRANSACTION_OBJECT="$BUILD_DIR/yazabase-transaction-v1-smoke.o"
TRANSACTION_OUTPUT="$BUILD_DIR/yazabase-transaction-v1-smoke"
YAZA_STDLIB_DIR="$PROJECT_DIR/std" \
  "$COMPILER" emit-llvm examples/yazabase-transaction-v1-smoke.yz \
  -o "$TRANSACTION_IR"
"$CLANG" -Wno-override-module -fsanitize=address,undefined \
  -x ir -c "$TRANSACTION_IR" -o "$TRANSACTION_OBJECT"
"$CLANG" -std=c11 -Wall -Wextra -Werror -pthread \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  "$TRANSACTION_OBJECT" "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" \
  -I"$PROJECT_DIR/runtime/include" -lm -o "$TRANSACTION_OUTPUT"
mkdir "$BUILD_DIR/transaction-root"
ASAN_OPTIONS=detect_leaks=0 "$TRANSACTION_OUTPUT" \
  "$BUILD_DIR/transaction-root"

EMPTY_TRANSACTION_IR="$BUILD_DIR/yazabase-empty-transaction-v1-smoke.ll"
EMPTY_TRANSACTION_OBJECT="$BUILD_DIR/yazabase-empty-transaction-v1-smoke.o"
EMPTY_TRANSACTION_OUTPUT="$BUILD_DIR/yazabase-empty-transaction-v1-smoke"
YAZA_STDLIB_DIR="$PROJECT_DIR/std" \
  "$COMPILER" emit-llvm examples/yazabase-empty-transaction-v1-smoke.yz \
  -o "$EMPTY_TRANSACTION_IR"
"$CLANG" -Wno-override-module -fsanitize=address,undefined \
  -x ir -c "$EMPTY_TRANSACTION_IR" -o "$EMPTY_TRANSACTION_OBJECT"
"$CLANG" -std=c11 -Wall -Wextra -Werror -pthread \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  "$EMPTY_TRANSACTION_OBJECT" "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" \
  -I"$PROJECT_DIR/runtime/include" -lm -o "$EMPTY_TRANSACTION_OUTPUT"
mkdir "$BUILD_DIR/empty-transaction-root"
ASAN_OPTIONS=detect_leaks=0 "$EMPTY_TRANSACTION_OUTPUT" \
  "$BUILD_DIR/empty-transaction-root"

DROP_TABLE_IR="$BUILD_DIR/yazabase-sql-drop-table-v1-smoke.ll"
DROP_TABLE_OBJECT="$BUILD_DIR/yazabase-sql-drop-table-v1-smoke.o"
DROP_TABLE_OUTPUT="$BUILD_DIR/yazabase-sql-drop-table-v1-smoke"
YAZA_STDLIB_DIR="$PROJECT_DIR/std" \
  "$COMPILER" emit-llvm examples/yazabase-sql-drop-table-v1-smoke.yz \
  -o "$DROP_TABLE_IR"
"$CLANG" -Wno-override-module -fsanitize=address,undefined \
  -x ir -c "$DROP_TABLE_IR" -o "$DROP_TABLE_OBJECT"
"$CLANG" -std=c11 -Wall -Wextra -Werror -pthread \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  "$DROP_TABLE_OBJECT" "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" \
  -I"$PROJECT_DIR/runtime/include" -lm -o "$DROP_TABLE_OUTPUT"
mkdir "$BUILD_DIR/sql-drop-table-root"
ASAN_OPTIONS=detect_leaks=0 "$DROP_TABLE_OUTPUT" \
  "$BUILD_DIR/sql-drop-table-root"

CREATE_INDEX_IR="$BUILD_DIR/yazabase-sql-create-index-v1-smoke.ll"
CREATE_INDEX_OBJECT="$BUILD_DIR/yazabase-sql-create-index-v1-smoke.o"
CREATE_INDEX_OUTPUT="$BUILD_DIR/yazabase-sql-create-index-v1-smoke"
YAZA_STDLIB_DIR="$PROJECT_DIR/std" \
  "$COMPILER" emit-llvm examples/yazabase-sql-create-index-v1-smoke.yz \
  -o "$CREATE_INDEX_IR"
"$CLANG" -Wno-override-module -fsanitize=address,undefined \
  -x ir -c "$CREATE_INDEX_IR" -o "$CREATE_INDEX_OBJECT"
"$CLANG" -std=c11 -Wall -Wextra -Werror -pthread \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  "$CREATE_INDEX_OBJECT" "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" \
  -I"$PROJECT_DIR/runtime/include" -lm -o "$CREATE_INDEX_OUTPUT"
mkdir "$BUILD_DIR/sql-create-index-root"
ASAN_OPTIONS=detect_leaks=0 "$CREATE_INDEX_OUTPUT" \
  "$BUILD_DIR/sql-create-index-root"

ALTER_TABLE_IR="$BUILD_DIR/yazabase-sql-alter-table-v1-smoke.ll"
ALTER_TABLE_OBJECT="$BUILD_DIR/yazabase-sql-alter-table-v1-smoke.o"
ALTER_TABLE_OUTPUT="$BUILD_DIR/yazabase-sql-alter-table-v1-smoke"
YAZA_STDLIB_DIR="$PROJECT_DIR/std" \
  "$COMPILER" emit-llvm examples/yazabase-sql-alter-table-v1-smoke.yz \
  -o "$ALTER_TABLE_IR"
"$CLANG" -Wno-override-module -fsanitize=address,undefined \
  -x ir -c "$ALTER_TABLE_IR" -o "$ALTER_TABLE_OBJECT"
"$CLANG" -std=c11 -Wall -Wextra -Werror -pthread \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  "$ALTER_TABLE_OBJECT" "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" \
  -I"$PROJECT_DIR/runtime/include" -lm -o "$ALTER_TABLE_OUTPUT"
mkdir "$BUILD_DIR/sql-alter-table-root"
ASAN_OPTIONS=detect_leaks=0 "$ALTER_TABLE_OUTPUT" \
  "$BUILD_DIR/sql-alter-table-root"

ROOT_GROWTH_IR="$BUILD_DIR/yazabase-root-growth-v1-smoke.ll"
ROOT_GROWTH_OBJECT="$BUILD_DIR/yazabase-root-growth-v1-smoke.o"
ROOT_GROWTH_OUTPUT="$BUILD_DIR/yazabase-root-growth-v1-smoke"
YAZA_STDLIB_DIR="$PROJECT_DIR/std" \
  "$COMPILER" emit-llvm examples/yazabase-root-growth-v1-smoke.yz \
  -o "$ROOT_GROWTH_IR"
"$CLANG" -Wno-override-module -fsanitize=address,undefined \
  -x ir -c "$ROOT_GROWTH_IR" -o "$ROOT_GROWTH_OBJECT"
"$CLANG" -std=c11 -Wall -Wextra -Werror -pthread \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  "$ROOT_GROWTH_OBJECT" "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" \
  -I"$PROJECT_DIR/runtime/include" -lm -o "$ROOT_GROWTH_OUTPUT"
mkdir "$BUILD_DIR/root-growth-root"
ASAN_OPTIONS=detect_leaks=0 "$ROOT_GROWTH_OUTPUT" \
  "$BUILD_DIR/root-growth-root"

CATALOG_INIT_IR="$BUILD_DIR/yazabase-catalog-init-v1-smoke.ll"
CATALOG_INIT_OBJECT="$BUILD_DIR/yazabase-catalog-init-v1-smoke.o"
CATALOG_INIT_OUTPUT="$BUILD_DIR/yazabase-catalog-init-v1-smoke"
YAZA_STDLIB_DIR="$PROJECT_DIR/std" \
  "$COMPILER" emit-llvm examples/yazabase-catalog-init-v1-smoke.yz \
  -o "$CATALOG_INIT_IR"
"$CLANG" -Wno-override-module -fsanitize=address,undefined \
  -x ir -c "$CATALOG_INIT_IR" -o "$CATALOG_INIT_OBJECT"
"$CLANG" -std=c11 -Wall -Wextra -Werror -pthread \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  "$CATALOG_INIT_OBJECT" "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" \
  -I"$PROJECT_DIR/runtime/include" -lm -o "$CATALOG_INIT_OUTPUT"
mkdir "$BUILD_DIR/catalog-init-root"
ASAN_OPTIONS=detect_leaks=0 "$CATALOG_INIT_OUTPUT" "$BUILD_DIR/catalog-init-root"

CATALOG_SNAPSHOT_IR="$BUILD_DIR/yazabase-catalog-snapshot-v1-smoke.ll"
CATALOG_SNAPSHOT_OBJECT="$BUILD_DIR/yazabase-catalog-snapshot-v1-smoke.o"
CATALOG_SNAPSHOT_OUTPUT="$BUILD_DIR/yazabase-catalog-snapshot-v1-smoke"
YAZA_STDLIB_DIR="$PROJECT_DIR/std" \
  "$COMPILER" emit-llvm examples/yazabase-catalog-snapshot-v1-smoke.yz \
  -o "$CATALOG_SNAPSHOT_IR"
"$CLANG" -Wno-override-module -fsanitize=address,undefined \
  -x ir -c "$CATALOG_SNAPSHOT_IR" -o "$CATALOG_SNAPSHOT_OBJECT"
"$CLANG" -std=c11 -Wall -Wextra -Werror -pthread \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  "$CATALOG_SNAPSHOT_OBJECT" "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" \
  -I"$PROJECT_DIR/runtime/include" -lm -o "$CATALOG_SNAPSHOT_OUTPUT"
mkdir "$BUILD_DIR/catalog-snapshot-root"
ASAN_OPTIONS=detect_leaks=0 "$CATALOG_SNAPSHOT_OUTPUT" \
  "$BUILD_DIR/catalog-snapshot-root"

CREATE_TABLE_PERSIST_IR="$BUILD_DIR/yazabase-sql-create-table-persist-v1-smoke.ll"
CREATE_TABLE_PERSIST_OBJECT="$BUILD_DIR/yazabase-sql-create-table-persist-v1-smoke.o"
CREATE_TABLE_PERSIST_OUTPUT="$BUILD_DIR/yazabase-sql-create-table-persist-v1-smoke"
YAZA_STDLIB_DIR="$PROJECT_DIR/std" \
  "$COMPILER" emit-llvm examples/yazabase-sql-create-table-persist-v1-smoke.yz \
  -o "$CREATE_TABLE_PERSIST_IR"
"$CLANG" -Wno-override-module -fsanitize=address,undefined \
  -x ir -c "$CREATE_TABLE_PERSIST_IR" -o "$CREATE_TABLE_PERSIST_OBJECT"
"$CLANG" -std=c11 -Wall -Wextra -Werror -pthread \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  "$CREATE_TABLE_PERSIST_OBJECT" "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" \
  -I"$PROJECT_DIR/runtime/include" -lm -o "$CREATE_TABLE_PERSIST_OUTPUT"
mkdir "$BUILD_DIR/create-table-persist-root"
ASAN_OPTIONS=detect_leaks=0 "$CREATE_TABLE_PERSIST_OUTPUT" \
  "$BUILD_DIR/create-table-persist-root"

DECIMAL_PERSIST_IR="$BUILD_DIR/yazabase-sql-decimal-persist-v1-smoke.ll"
DECIMAL_PERSIST_OBJECT="$BUILD_DIR/yazabase-sql-decimal-persist-v1-smoke.o"
DECIMAL_PERSIST_OUTPUT="$BUILD_DIR/yazabase-sql-decimal-persist-v1-smoke"
YAZA_STDLIB_DIR="$PROJECT_DIR/std" \
  "$COMPILER" emit-llvm examples/yazabase-sql-decimal-persist-v1-smoke.yz \
  -o "$DECIMAL_PERSIST_IR"
"$CLANG" -Wno-override-module -fsanitize=address,undefined \
  -x ir -c "$DECIMAL_PERSIST_IR" -o "$DECIMAL_PERSIST_OBJECT"
"$CLANG" -std=c11 -Wall -Wextra -Werror -pthread \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  "$DECIMAL_PERSIST_OBJECT" "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" \
  -I"$PROJECT_DIR/runtime/include" -lm -o "$DECIMAL_PERSIST_OUTPUT"
mkdir "$BUILD_DIR/decimal-persist-root"
ASAN_OPTIONS=detect_leaks=0 "$DECIMAL_PERSIST_OUTPUT" \
  "$BUILD_DIR/decimal-persist-root"

OVERFLOW_SQL_IR="$BUILD_DIR/yazabase-overflow-sql-v1-smoke.ll"
OVERFLOW_SQL_OBJECT="$BUILD_DIR/yazabase-overflow-sql-v1-smoke.o"
OVERFLOW_SQL_OUTPUT="$BUILD_DIR/yazabase-overflow-sql-v1-smoke"
YAZA_STDLIB_DIR="$PROJECT_DIR/std" \
  "$COMPILER" emit-llvm examples/yazabase-overflow-sql-v1-smoke.yz \
  -o "$OVERFLOW_SQL_IR"
"$CLANG" -Wno-override-module -fsanitize=address,undefined \
  -x ir -c "$OVERFLOW_SQL_IR" -o "$OVERFLOW_SQL_OBJECT"
"$CLANG" -std=c11 -Wall -Wextra -Werror -pthread \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  "$OVERFLOW_SQL_OBJECT" "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" \
  -I"$PROJECT_DIR/runtime/include" -lm -o "$OVERFLOW_SQL_OUTPUT"
mkdir "$BUILD_DIR/overflow-sql-root"
ASAN_OPTIONS=detect_leaks=0 "$OVERFLOW_SQL_OUTPUT" \
  "$BUILD_DIR/overflow-sql-root"

BACKUP_IR="$BUILD_DIR/yazabase-backup-v1-smoke.ll"
BACKUP_OBJECT="$BUILD_DIR/yazabase-backup-v1-smoke.o"
BACKUP_OUTPUT="$BUILD_DIR/yazabase-backup-v1-smoke"
YAZA_STDLIB_DIR="$PROJECT_DIR/std" \
  "$COMPILER" emit-llvm examples/yazabase-backup-v1-smoke.yz \
  -o "$BACKUP_IR"
"$CLANG" -Wno-override-module -fsanitize=address,undefined \
  -x ir -c "$BACKUP_IR" -o "$BACKUP_OBJECT"
"$CLANG" -std=c11 -Wall -Wextra -Werror -pthread \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  "$BACKUP_OBJECT" "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" \
  -I"$PROJECT_DIR/runtime/include" -lm -o "$BACKUP_OUTPUT"
mkdir "$BUILD_DIR/backup-source-root"
mkdir "$BUILD_DIR/backup-destination-root"
ASAN_OPTIONS=detect_leaks=0 "$BACKUP_OUTPUT" \
  "$BUILD_DIR/backup-source-root" "$BUILD_DIR/backup-destination-root"

SQL_INNER_SPLIT_IR="$BUILD_DIR/yazabase-sql-inner-split-v1-smoke.ll"
SQL_INNER_SPLIT_OBJECT="$BUILD_DIR/yazabase-sql-inner-split-v1-smoke.o"
SQL_INNER_SPLIT_OUTPUT="$BUILD_DIR/yazabase-sql-inner-split-v1-smoke"
YAZA_STDLIB_DIR="$PROJECT_DIR/std" \
  "$COMPILER" emit-llvm examples/yazabase-sql-inner-split-v1-smoke.yz \
  -o "$SQL_INNER_SPLIT_IR"
"$CLANG" -Wno-override-module -fsanitize=address,undefined \
  -x ir -c "$SQL_INNER_SPLIT_IR" -o "$SQL_INNER_SPLIT_OBJECT"
"$CLANG" -std=c11 -Wall -Wextra -Werror -pthread \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  "$SQL_INNER_SPLIT_OBJECT" "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" \
  -I"$PROJECT_DIR/runtime/include" -lm -o "$SQL_INNER_SPLIT_OUTPUT"
mkdir "$BUILD_DIR/sql-inner-split-root"
ASAN_OPTIONS=detect_leaks=0 "$SQL_INNER_SPLIT_OUTPUT" \
  "$BUILD_DIR/sql-inner-split-root"

SQL_MULTI_ANCESTOR_SPLIT_IR="$BUILD_DIR/yazabase-sql-multi-ancestor-split-v1-smoke.ll"
SQL_MULTI_ANCESTOR_SPLIT_OBJECT="$BUILD_DIR/yazabase-sql-multi-ancestor-split-v1-smoke.o"
SQL_MULTI_ANCESTOR_SPLIT_OUTPUT="$BUILD_DIR/yazabase-sql-multi-ancestor-split-v1-smoke"
YAZA_STDLIB_DIR="$PROJECT_DIR/std" \
  "$COMPILER" emit-llvm examples/yazabase-sql-multi-ancestor-split-v1-smoke.yz \
  -o "$SQL_MULTI_ANCESTOR_SPLIT_IR"
"$CLANG" -Wno-override-module -fsanitize=address,undefined \
  -x ir -c "$SQL_MULTI_ANCESTOR_SPLIT_IR" -o "$SQL_MULTI_ANCESTOR_SPLIT_OBJECT"
"$CLANG" -std=c11 -Wall -Wextra -Werror -pthread \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  "$SQL_MULTI_ANCESTOR_SPLIT_OBJECT" "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" \
  -I"$PROJECT_DIR/runtime/include" -lm -o "$SQL_MULTI_ANCESTOR_SPLIT_OUTPUT"
mkdir "$BUILD_DIR/sql-multi-ancestor-split-root"
ASAN_OPTIONS=detect_leaks=0 "$SQL_MULTI_ANCESTOR_SPLIT_OUTPUT" \
  "$BUILD_DIR/sql-multi-ancestor-split-root"

BTREE_SCAN_IR="$BUILD_DIR/yazabase-btree-scan-v1-smoke.ll"
BTREE_SCAN_OBJECT="$BUILD_DIR/yazabase-btree-scan-v1-smoke.o"
BTREE_SCAN_OUTPUT="$BUILD_DIR/yazabase-btree-scan-v1-smoke"
YAZA_STDLIB_DIR="$PROJECT_DIR/std" \
  "$COMPILER" emit-llvm examples/yazabase-btree-scan-v1-smoke.yz \
  -o "$BTREE_SCAN_IR"
"$CLANG" -Wno-override-module -fsanitize=address,undefined \
  -x ir -c "$BTREE_SCAN_IR" -o "$BTREE_SCAN_OBJECT"
"$CLANG" -std=c11 -Wall -Wextra -Werror -pthread \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  "$BTREE_SCAN_OBJECT" "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" \
  -I"$PROJECT_DIR/runtime/include" -lm -o "$BTREE_SCAN_OUTPUT"
mkdir "$BUILD_DIR/btree-scan-root"
ASAN_OPTIONS=detect_leaks=0 "$BTREE_SCAN_OUTPUT" \
  "$BUILD_DIR/btree-scan-root"

SQL_INNER_JOIN_IR="$BUILD_DIR/yazabase-sql-inner-join-persist-v1-smoke.ll"
SQL_INNER_JOIN_OBJECT="$BUILD_DIR/yazabase-sql-inner-join-persist-v1-smoke.o"
SQL_INNER_JOIN_OUTPUT="$BUILD_DIR/yazabase-sql-inner-join-persist-v1-smoke"
YAZA_STDLIB_DIR="$PROJECT_DIR/std" \
  "$COMPILER" emit-llvm examples/yazabase-sql-inner-join-persist-v1-smoke.yz \
  -o "$SQL_INNER_JOIN_IR"
"$CLANG" -Wno-override-module -fsanitize=address,undefined \
  -x ir -c "$SQL_INNER_JOIN_IR" -o "$SQL_INNER_JOIN_OBJECT"
"$CLANG" -std=c11 -Wall -Wextra -Werror -pthread \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  "$SQL_INNER_JOIN_OBJECT" "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" \
  -I"$PROJECT_DIR/runtime/include" -lm -o "$SQL_INNER_JOIN_OUTPUT"
mkdir "$BUILD_DIR/sql-inner-join-persist-root"
ASAN_OPTIONS=detect_leaks=0 "$SQL_INNER_JOIN_OUTPUT" \
  "$BUILD_DIR/sql-inner-join-persist-root"

SQL_MULTI_JOIN_IR="$BUILD_DIR/yazabase-sql-multi-join-persist-v1-smoke.ll"
SQL_MULTI_JOIN_OBJECT="$BUILD_DIR/yazabase-sql-multi-join-persist-v1-smoke.o"
SQL_MULTI_JOIN_OUTPUT="$BUILD_DIR/yazabase-sql-multi-join-persist-v1-smoke"
YAZA_STDLIB_DIR="$PROJECT_DIR/std" \
  "$COMPILER" emit-llvm examples/yazabase-sql-multi-join-persist-v1-smoke.yz \
  -o "$SQL_MULTI_JOIN_IR"
"$CLANG" -Wno-override-module -fsanitize=address,undefined \
  -x ir -c "$SQL_MULTI_JOIN_IR" -o "$SQL_MULTI_JOIN_OBJECT"
"$CLANG" -std=c11 -Wall -Wextra -Werror -pthread \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  "$SQL_MULTI_JOIN_OBJECT" "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" \
  -I"$PROJECT_DIR/runtime/include" -lm -o "$SQL_MULTI_JOIN_OUTPUT"
mkdir "$BUILD_DIR/sql-multi-join-persist-root"
ASAN_OPTIONS=detect_leaks=0 "$SQL_MULTI_JOIN_OUTPUT" \
  "$BUILD_DIR/sql-multi-join-persist-root"

SQL_SET_OPS_IR="$BUILD_DIR/yazabase-sql-set-ops-v1-smoke.ll"
SQL_SET_OPS_OBJECT="$BUILD_DIR/yazabase-sql-set-ops-v1-smoke.o"
SQL_SET_OPS_OUTPUT="$BUILD_DIR/yazabase-sql-set-ops-v1-smoke"
YAZA_STDLIB_DIR="$PROJECT_DIR/std" \
  "$COMPILER" emit-llvm examples/yazabase-sql-set-ops-v1-smoke.yz \
  -o "$SQL_SET_OPS_IR"
"$CLANG" -Wno-override-module -fsanitize=address,undefined \
  -x ir -c "$SQL_SET_OPS_IR" -o "$SQL_SET_OPS_OBJECT"
"$CLANG" -std=c11 -Wall -Wextra -Werror -pthread \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  "$SQL_SET_OPS_OBJECT" "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" \
  -I"$PROJECT_DIR/runtime/include" -lm -o "$SQL_SET_OPS_OUTPUT"
mkdir "$BUILD_DIR/sql-set-ops-root"
ASAN_OPTIONS=detect_leaks=0 "$SQL_SET_OPS_OUTPUT" \
  "$BUILD_DIR/sql-set-ops-root"

YAZA_LLVM_COMPILER="$COMPILER" \
  YAZA_COMPILER_PROCESS_LIMIT_BYTES="$COMPILER_PROCESS_LIMIT_BYTES" \
  sh "$SCRIPT_DIR/validate-yazabase-sync-recovery.sh" asan
