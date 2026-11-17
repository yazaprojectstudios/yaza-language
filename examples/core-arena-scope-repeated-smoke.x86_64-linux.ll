target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"
; Generated from YAZA Core HIR -> YIR.
declare i32 @printf(ptr, ...)
declare void @yaza_rt_v1_init()
declare void @yaza_rt_v1_set_args(i32, ptr)
declare i64 @yaza_rt_v1_arg_count()
declare ptr @yaza_rt_v1_arg(i64)
declare ptr @yaza_rt_v1_alloc(i64)
declare ptr @yaza_rt_v1_try_alloc(i64)
declare i32 @yaza_rt_v1_module_text_copy(ptr, ptr)
declare i32 @yaza_rt_v1_module_bytes_copy(ptr, ptr)
declare ptr @yaza_rt_v1_text_from_cstr(ptr)
declare ptr @yaza_rt_v1_try_text_from_cstr(ptr)
declare ptr @yaza_rt_v1_text_concat(ptr, ptr)
declare i64 @yaza_rt_v1_text_length(ptr)
declare i64 @yaza_rt_v1_text_byte(ptr, i64)
declare ptr @yaza_rt_v1_text_slice(ptr, i64, i64)
declare i64 @yaza_rt_v1_text_find(ptr, ptr)
declare i32 @yaza_rt_v1_text_starts_with(ptr, ptr)
declare i32 @yaza_rt_v1_text_ends_with(ptr, ptr)
declare i64 @yaza_rt_v1_text_compare(ptr, ptr)
declare ptr @yaza_rt_v1_text_trim(ptr)
declare ptr @yaza_rt_v1_try_text_trim(ptr)
declare ptr @yaza_rt_v1_text_lines(ptr)
declare i32 @yaza_rt_v1_try_text_lines(ptr, ptr)
declare i32 @yaza_rt_v1_try_text_split(ptr, ptr, ptr)
declare ptr @yaza_rt_v1_text_split_delimiter_error()
declare ptr @yaza_rt_v1_text_join(ptr, ptr)
declare i32 @yaza_rt_v1_say_text(ptr)
declare i64 @yaza_rt_v1_number_parse(ptr)
declare double @yaza_rt_v1_float64_parse(ptr)
declare i32 @yaza_rt_v1_say_number(i64)
declare i32 @yaza_rt_v1_say_int32(i32)
declare ptr @yaza_rt_v1_number_text(i64)
declare ptr @yaza_rt_v1_int32_text(i32)
declare ptr @yaza_rt_v1_float64_text(double)
declare ptr @yaza_list_v1_create_typed(ptr, i64)
declare i64 @yaza_list_v1_count_typed(ptr, ptr, i64)
declare i64 @yaza_list_v1_push_typed(ptr, ptr, ptr, i64)
declare i32 @yaza_list_v1_try_push_typed(ptr, ptr, ptr, i64)
declare i32 @yaza_list_v1_get_typed(ptr, i64, ptr, i64, ptr)
declare i32 @yaza_list_v1_try_get_typed(ptr, i64, ptr, i64, ptr)
declare i64 @yaza_list_v1_set_typed(ptr, i64, ptr, ptr, i64)
declare i32 @yaza_list_v1_try_set_typed(ptr, i64, ptr, ptr, i64)
declare i64 @yaza_rt_v1_fail_text(ptr)
declare i64 @yaza_rt_v1_assert_fail_text(ptr)
declare void @llvm.trap()
declare { i64, i1 } @llvm.sadd.with.overflow.i64(i64, i64)
declare { i64, i1 } @llvm.ssub.with.overflow.i64(i64, i64)
declare { i64, i1 } @llvm.smul.with.overflow.i64(i64, i64)
declare { i32, i1 } @llvm.sadd.with.overflow.i32(i32, i32)
declare { i32, i1 } @llvm.ssub.with.overflow.i32(i32, i32)
declare { i32, i1 } @llvm.smul.with.overflow.i32(i32, i32)
declare i1 @llvm.is.fpclass.f64(double, i32)
declare ptr @yaza_list_v1_try_create_typed(ptr, i64)
declare ptr @yaza_rt_v1_list_allocation_error()
declare ptr @yaza_bytes_v1_to_text(ptr, i64)
declare i64 @yaza_bytes_v1_zeroize(ptr, i64)
declare i32 @yaza_rt_v1_module_text_input_copy(ptr, ptr)
declare i32 @yaza_rt_v1_module_bytes_input_copy(ptr, ptr)
declare ptr @yaza_rt_v1_try_text_concat(ptr, ptr)
declare ptr @yaza_rt_v1_try_text_join(ptr, ptr)
declare i32 @yaza_rt_v1_try_text_slice(ptr, i64, i64, ptr)
declare ptr @yaza_rt_v1_text_slice_range_error()
declare ptr @yaza_rt_v1_try_number_text(i64)
declare ptr @yaza_rt_v1_try_int32_text(i32)
declare ptr @yaza_rt_v1_try_float64_text(double)
declare ptr @yaza_rt_v1_text_allocation_error()
declare i64 @yaza_rt_v1_exit(i64)
declare ptr @yaza_rt_v1_scope_enter(i64)
declare ptr @yaza_rt_v1_scope_enter_inplace(i64, ptr, i64)
declare i32 @yaza_rt_v1_scope_leave(ptr)
declare ptr @yaza_text_v1_split(ptr, ptr)
declare void @yaza_bytes_v1_from_text(ptr, ptr)
declare i32 @yaza_bytes_v1_try_from_text(ptr, ptr, ptr)
declare void @yaza_bytes_v1_from_number(i64, ptr)
declare i32 @yaza_bytes_v1_try_from_number(i64, ptr, ptr)
declare void @yaza_bytes_v1_secure_random(i64, ptr)
declare i32 @yaza_bytes_v1_try_secure_random(i64, ptr, ptr)
declare void @yaza_bytes_v1_slice(ptr, i64, i64, i64, ptr)
declare i32 @yaza_bytes_v1_try_slice(ptr, i64, i64, i64, ptr, ptr)
declare void @yaza_bytes_v1_concat(ptr, i64, ptr, i64, ptr)
declare i32 @yaza_bytes_v1_try_concat(ptr, i64, ptr, i64, ptr, ptr)
declare i32 @yaza_bytes_v1_equal(ptr, i64, ptr, i64)
declare i32 @yaza_bytes_v1_equal_constant_time(ptr, i64, ptr, i64)
declare i32 @yaza_bytes_v1_to_text_checked(ptr, i64, ptr)
declare i32 @yaza_bytes_v1_try_to_text(ptr, i64, ptr, ptr)
declare i32 @yaza_bytes_v1_from_text_bounded(ptr, i64, ptr, ptr)
declare i32 @yaza_bytes_v1_join(ptr, ptr, ptr)
declare void @yaza_list_v1_iter_begin_typed(ptr, ptr, i64, ptr)
declare i64 @yaza_list_v1_iter_count(ptr)
declare i32 @yaza_list_v1_iter_get_typed(ptr, i64, ptr)
declare void @yaza_list_v1_iter_end(ptr)
declare void @yaza_list_v1_read_begin_typed(ptr, ptr, i64, ptr)
declare i32 @yaza_list_v1_read_get_typed(ptr, i64, ptr)
declare void @yaza_list_v1_read_end(ptr)
declare i64 @yaza_rt_v1_current_arena_used()
declare i64 @yaza_rt_v1_current_arena_storage_used()
declare i64 @yaza_rt_v1_current_arena_reserved()
declare i64 @yaza_rt_v1_current_arena_peak_used()
declare i64 @yaza_rt_v1_current_arena_peak_storage_used()
declare i64 @yaza_rt_v1_current_arena_peak_reserved()
declare i64 @yaza_rt_v1_current_arena_allocation_count()
declare i64 @yaza_rt_v1_current_arena_generation()
declare i64 @yaza_rt_v1_live_scope_count()
%yaza.option = type { i32, i64 }
%yaza.option.bytes = type { i32, i64, i64 }
%yaza.result = type { i32, i64 }
%yaza.result.bytes = type { i32, i64, i64 }
%yaza.text = type { ptr, i64 }
%yaza.bytes = type { ptr, i64 }
%yaza.module.input = type { i32, i32, i32, i32, ptr, i64 }
%yaza.module.result = type { i32, i32, i32, i32, ptr, i64 }
%yaza.list.borrow = type { ptr, ptr, ptr, ptr, i64, i64, i64, i64 }
@.yaza.core.fmt.f64 = private unnamed_addr constant [7 x i8] c"\25\2E\31\37\67\0A\00", align 1
@.yaza.core.fmt.text = private unnamed_addr constant [4 x i8] c"\25\73\0A\00", align 1
@.yaza.core.true = private unnamed_addr constant [5 x i8] c"\74\72\75\65\00", align 1
@.yaza.core.false = private unnamed_addr constant [6 x i8] c"\66\61\6C\73\65\00", align 1
@.yaza.module.error.input = private unnamed_addr constant [82 x i8] c"\72\65\71\75\65\73\74\20\69\6E\70\75\74\20\41\42\49\2F\74\79\70\65\20\69\73\20\69\6E\76\61\6C\69\64\2C\20\69\6E\70\75\74\20\69\73\20\74\6F\6F\20\6C\61\72\67\65\2C\20\6F\72\20\54\65\78\74\20\69\73\20\6E\6F\74\20\76\61\6C\69\64\20\55\54\46\2D\38\00", align 1
@.yaza.module.error.scope = private unnamed_addr constant [57 x i8] c"\72\65\71\75\65\73\74\20\6D\6F\64\75\6C\65\20\72\65\71\75\69\72\65\73\20\61\6E\20\61\63\74\69\76\65\20\72\65\75\73\61\62\6C\65\20\72\65\71\75\65\73\74\20\63\79\63\6C\65\00", align 1
@.yaza.module.error.resource = private unnamed_addr constant [50 x i8] c"\72\65\71\75\65\73\74\20\61\72\65\6E\61\20\71\75\6F\74\61\20\65\78\68\61\75\73\74\65\64\20\77\68\69\6C\65\20\63\6F\70\79\69\6E\67\20\69\6E\70\75\74\00", align 1
@.yaza.core.str.0.bytes = private unnamed_addr constant [37 x i8] c"\61\72\65\6E\61\5F\73\63\6F\70\65\20\6C\69\6D\69\74\20\63\61\6E\6E\6F\74\20\62\65\20\6E\65\67\61\74\69\76\65\00", align 1
@.yaza.core.str.0 = private constant %yaza.text { ptr getelementptr inbounds ([37 x i8], ptr @.yaza.core.str.0.bytes, i64 0, i64 0), i64 36 }, align 8
@.yaza.core.str.1.bytes = private unnamed_addr constant [53 x i8] c"\61\72\65\6E\61\5F\73\63\6F\70\65\20\63\6F\75\6C\64\20\6E\6F\74\20\63\72\65\61\74\65\20\69\74\73\20\62\6F\75\6E\64\65\64\20\63\68\69\6C\64\20\61\72\65\6E\61\00", align 1
@.yaza.core.str.1 = private constant %yaza.text { ptr getelementptr inbounds ([53 x i8], ptr @.yaza.core.str.1.bytes, i64 0, i64 0), i64 52 }, align 8
@.yaza.core.str.2.bytes = private unnamed_addr constant [7 x i8] c"\63\79\63\6C\65\20\00", align 1
@.yaza.core.str.2 = private constant %yaza.text { ptr getelementptr inbounds ([7 x i8], ptr @.yaza.core.str.2.bytes, i64 0, i64 0), i64 6 }, align 8
@.yaza.core.str.3.bytes = private unnamed_addr constant [45 x i8] c"\41\73\73\65\72\74\69\6F\6E\20\66\61\69\6C\65\64\3A\20\28\74\65\78\74\5F\6C\65\6E\67\74\68\28\70\61\79\6C\6F\61\64\29\20\3E\20\30\29\00", align 1
@.yaza.core.str.3 = private constant %yaza.text { ptr getelementptr inbounds ([45 x i8], ptr @.yaza.core.str.3.bytes, i64 0, i64 0), i64 44 }, align 8
@.yaza.core.str.4.bytes = private unnamed_addr constant [37 x i8] c"\41\73\73\65\72\74\69\6F\6E\20\66\61\69\6C\65\64\3A\20\28\61\72\65\6E\61\5F\75\73\65\64\28\29\20\3E\20\30\29\00", align 1
@.yaza.core.str.4 = private constant %yaza.text { ptr getelementptr inbounds ([37 x i8], ptr @.yaza.core.str.4.bytes, i64 0, i64 0), i64 36 }, align 8
@.yaza.core.str.5.bytes = private unnamed_addr constant [83 x i8] c"\61\72\65\6E\61\5F\73\63\6F\70\65\20\63\6F\75\6C\64\20\6E\6F\74\20\63\6C\6F\73\65\20\62\65\63\61\75\73\65\20\61\20\63\68\69\6C\64\20\73\63\6F\70\65\20\6F\72\20\72\75\6E\74\69\6D\65\20\6C\65\61\73\65\20\69\73\20\73\74\69\6C\6C\20\61\63\74\69\76\65\00", align 1
@.yaza.core.str.5 = private constant %yaza.text { ptr getelementptr inbounds ([83 x i8], ptr @.yaza.core.str.5.bytes, i64 0, i64 0), i64 82 }, align 8
@.yaza.core.str.6.bytes = private unnamed_addr constant [49 x i8] c"\41\73\73\65\72\74\69\6F\6E\20\66\61\69\6C\65\64\3A\20\28\61\72\65\6E\61\5F\75\73\65\64\28\29\20\3D\3D\20\69\6E\69\74\69\61\6C\5F\75\73\65\64\29\00", align 1
@.yaza.core.str.6 = private constant %yaza.text { ptr getelementptr inbounds ([49 x i8], ptr @.yaza.core.str.6.bytes, i64 0, i64 0), i64 48 }, align 8
@.yaza.core.str.7.bytes = private unnamed_addr constant [60 x i8] c"\41\73\73\65\72\74\69\6F\6E\20\66\61\69\6C\65\64\3A\20\28\61\72\65\6E\61\5F\73\74\6F\72\61\67\65\5F\75\73\65\64\28\29\20\3D\3D\20\69\6E\69\74\69\61\6C\5F\73\74\6F\72\61\67\65\29\00", align 1
@.yaza.core.str.7 = private constant %yaza.text { ptr getelementptr inbounds ([60 x i8], ptr @.yaza.core.str.7.bytes, i64 0, i64 0), i64 59 }, align 8
@.yaza.core.str.8.bytes = private unnamed_addr constant [57 x i8] c"\41\73\73\65\72\74\69\6F\6E\20\66\61\69\6C\65\64\3A\20\28\61\72\65\6E\61\5F\72\65\73\65\72\76\65\64\28\29\20\3D\3D\20\69\6E\69\74\69\61\6C\5F\72\65\73\65\72\76\65\64\29\00", align 1
@.yaza.core.str.8 = private constant %yaza.text { ptr getelementptr inbounds ([57 x i8], ptr @.yaza.core.str.8.bytes, i64 0, i64 0), i64 56 }, align 8
@.yaza.core.str.9.bytes = private unnamed_addr constant [58 x i8] c"\41\73\73\65\72\74\69\6F\6E\20\66\61\69\6C\65\64\3A\20\28\61\72\65\6E\61\5F\6C\69\76\65\5F\73\63\6F\70\65\73\28\29\20\3D\3D\20\69\6E\69\74\69\61\6C\5F\73\63\6F\70\65\73\29\00", align 1
@.yaza.core.str.9 = private constant %yaza.text { ptr getelementptr inbounds ([58 x i8], ptr @.yaza.core.str.9.bytes, i64 0, i64 0), i64 57 }, align 8
@.yaza.core.str.10.bytes = private unnamed_addr constant [46 x i8] c"\4E\75\6D\62\65\72\20\6F\76\65\72\66\6C\6F\77\20\6F\72\20\64\69\76\69\73\69\6F\6E\20\65\72\72\6F\72\20\69\6E\20\61\64\64\69\74\69\6F\6E\00", align 1
@.yaza.core.str.10 = private constant %yaza.text { ptr getelementptr inbounds ([46 x i8], ptr @.yaza.core.str.10.bytes, i64 0, i64 0), i64 45 }, align 8
@.yaza.core.str.11.bytes = private unnamed_addr constant [16 x i8] c"\72\65\63\6C\61\69\6D\65\64\3D\31\30\30\30\30\00", align 1
@.yaza.core.str.11 = private constant %yaza.text { ptr getelementptr inbounds ([16 x i8], ptr @.yaza.core.str.11.bytes, i64 0, i64 0), i64 15 }, align 8
@.yaza.core.result.bytes.fallback.bytes = private unnamed_addr constant [45 x i8] c"\43\6F\75\6C\64\20\6E\6F\74\20\61\6C\6C\6F\63\61\74\65\20\42\79\74\65\73\20\69\6E\20\74\68\65\20\61\63\74\69\76\65\20\61\72\65\6E\61\00", align 1
@.yaza.core.result.bytes.fallback.text = private constant %yaza.text { ptr getelementptr inbounds ([45 x i8], ptr @.yaza.core.result.bytes.fallback.bytes, i64 0, i64 0), i64 44 }, align 8
define i32 @main(i32 %argc, ptr %argv) {
entry:
  call void @yaza_rt_v1_init()
  call void @yaza_rt_v1_set_args(i32 %argc, ptr %argv)
  %slot.0 = alloca i64
  %slot.1 = alloca i64
  %slot.2 = alloca i64
  %slot.3 = alloca i64
  %slot.4 = alloca i64
  %arena.scope.5 = alloca ptr
  %arena.scope.5.storage = alloca [512 x i8], align 16
  %slot.6 = alloca ptr
  %v0 = call i64 @yaza_rt_v1_current_arena_used()
  store i64 %v0, ptr %slot.0  ; YAZA line 2
  %v1 = call i64 @yaza_rt_v1_current_arena_storage_used()
  store i64 %v1, ptr %slot.1  ; YAZA line 3
  %v2 = call i64 @yaza_rt_v1_current_arena_reserved()
  store i64 %v2, ptr %slot.2  ; YAZA line 4
  %v3 = call i64 @yaza_rt_v1_live_scope_count()
  store i64 %v3, ptr %slot.3  ; YAZA line 5
  store i64 0, ptr %slot.4  ; YAZA line 6
  br label %while.cond.0
while.cond.0:
  %v4 = load i64, ptr %slot.4
  %v5 = icmp slt i64 %v4, 10000
  br i1 %v5, label %while.body.1, label %while.exit.2
while.body.1:
  %v6 = icmp slt i64 4096, 0
  br i1 %v6, label %trap.3, label %ok.4
trap.3:
  call i64 @yaza_rt_v1_fail_text(ptr @.yaza.core.str.0)
  unreachable
ok.4:
  %v7 = call ptr @yaza_rt_v1_scope_enter_inplace(i64 4096, ptr %arena.scope.5.storage, i64 512)
  store ptr %v7, ptr %arena.scope.5  ; YAZA line 8
  %v8 = icmp eq ptr %v7, null
  br i1 %v8, label %trap.5, label %ok.6
trap.5:
  call i64 @yaza_rt_v1_fail_text(ptr @.yaza.core.str.1)
  unreachable
ok.6:
  %v9 = load i64, ptr %slot.4
  %v10 = call ptr @yaza_rt_v1_number_text(i64 %v9)
  %v11 = call ptr @yaza_rt_v1_text_concat(ptr @.yaza.core.str.2, ptr %v10)
  store ptr %v11, ptr %slot.6  ; YAZA line 9
  %v12 = load ptr, ptr %slot.6
  %v13 = call i64 @yaza_rt_v1_text_length(ptr %v12)
  %v14 = icmp sgt i64 %v13, 0
  %v15 = xor i1 %v14, true
  br i1 %v15, label %assert.fail.7, label %assert.ok.8
assert.fail.7:
  call i64 @yaza_rt_v1_assert_fail_text(ptr @.yaza.core.str.3)
  unreachable
assert.ok.8:
  %v16 = call i64 @yaza_rt_v1_current_arena_used()
  %v17 = icmp sgt i64 %v16, 0
  %v18 = xor i1 %v17, true
  br i1 %v18, label %assert.fail.9, label %assert.ok.10
assert.fail.9:
  call i64 @yaza_rt_v1_assert_fail_text(ptr @.yaza.core.str.4)
  unreachable
assert.ok.10:
  %v19 = load ptr, ptr %arena.scope.5
  %v20 = call i32 @yaza_rt_v1_scope_leave(ptr %v19)
  %v21 = icmp eq i32 %v20, 0
  br i1 %v21, label %trap.11, label %ok.12
trap.11:
  call i64 @yaza_rt_v1_fail_text(ptr @.yaza.core.str.5)
  unreachable
ok.12:
  %v22 = call i64 @yaza_rt_v1_current_arena_used()
  %v23 = load i64, ptr %slot.0
  %v24 = icmp eq i64 %v22, %v23
  %v25 = xor i1 %v24, true
  br i1 %v25, label %assert.fail.13, label %assert.ok.14
assert.fail.13:
  call i64 @yaza_rt_v1_assert_fail_text(ptr @.yaza.core.str.6)
  unreachable
assert.ok.14:
  %v26 = call i64 @yaza_rt_v1_current_arena_storage_used()
  %v27 = load i64, ptr %slot.1
  %v28 = icmp eq i64 %v26, %v27
  %v29 = xor i1 %v28, true
  br i1 %v29, label %assert.fail.15, label %assert.ok.16
assert.fail.15:
  call i64 @yaza_rt_v1_assert_fail_text(ptr @.yaza.core.str.7)
  unreachable
assert.ok.16:
  %v30 = call i64 @yaza_rt_v1_current_arena_reserved()
  %v31 = load i64, ptr %slot.2
  %v32 = icmp eq i64 %v30, %v31
  %v33 = xor i1 %v32, true
  br i1 %v33, label %assert.fail.17, label %assert.ok.18
assert.fail.17:
  call i64 @yaza_rt_v1_assert_fail_text(ptr @.yaza.core.str.8)
  unreachable
assert.ok.18:
  %v34 = call i64 @yaza_rt_v1_live_scope_count()
  %v35 = load i64, ptr %slot.3
  %v36 = icmp eq i64 %v34, %v35
  %v37 = xor i1 %v36, true
  br i1 %v37, label %assert.fail.19, label %assert.ok.20
assert.fail.19:
  call i64 @yaza_rt_v1_assert_fail_text(ptr @.yaza.core.str.9)
  unreachable
assert.ok.20:
  %v38 = load i64, ptr %slot.4
  %v40 = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %v38, i64 1)
  %v39 = extractvalue { i64, i1 } %v40, 0
  %v41 = extractvalue { i64, i1 } %v40, 1
  br i1 %v41, label %trap.21, label %ok.22
trap.21:
  call i64 @yaza_rt_v1_fail_text(ptr @.yaza.core.str.10)
  unreachable
ok.22:
  store i64 %v39, ptr %slot.4  ; YAZA line 16
  br label %while.cond.0
while.exit.2:
  %ignored.42 = call i32 @yaza_rt_v1_say_text(ptr @.yaza.core.str.11)  ; YAZA line 17
  ret i32 0
}
