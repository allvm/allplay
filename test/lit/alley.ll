; RUN: llvm-as %s -o %t.bc
; RUN: bc2allvm %t.bc -o %t
; RUN: alley %t

define i32 @main() {
	ret i32 0;
}
