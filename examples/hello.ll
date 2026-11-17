; Minimal host-oriented LLVM IR smoke artifact.
; This is hand-written LLVM IR, not generated from YAZA source or YIR.

@.message = private unnamed_addr constant [31 x i8] c"Hello from YAZA LLVM prototype\00", align 1

declare i32 @puts(ptr)

define i32 @main() {
entry:
  %result = call i32 @puts(ptr @.message)
  ret i32 0
}
