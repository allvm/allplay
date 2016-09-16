; RUN: llvm-as %s -o %t.bc

; Try reading from file that doesn't exist
; RUN: not bc2allvm %t.nope -f -o %t |& FileCheck %s

; Test trying to write to a file that already exists
; XXX: Ideally this would fail while with error about opening outputfile for write
; RUN: bc2allvm %t.bc -f -o %t
; RUN: not bc2allvm %t.bc -o %t |& FileCheck %s

; It'd be nice if we said something better than this,
; but at least we catch these errors.
; CHECK: Error adding file
; CHECK: unknown reason

; Test trying to write to a directory (output error)
; RUN: not bc2allvm %t.bc -o %T |& FileCheck -check-prefix=OUTPUT %s

; OUTPUT: Could not open output file

define i32 @main() {
	ret i32 0;
}
