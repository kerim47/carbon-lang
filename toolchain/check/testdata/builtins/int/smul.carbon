// Part of the Carbon Language project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// AUTOUPDATE

// --- int_mul.carbon

fn Mul(a: i32, b: i32) -> i32 = "int.smul";

var arr: [i32; Mul(3, 2)];
let arr_p: [i32; 6]* = &arr;

fn RuntimeCall(a: i32, b: i32) -> i32 {
  return Mul(a, b);
}

// --- fail_overflow.carbon

package FailOverflow api;

fn Mul(a: i32, b: i32) -> i32 = "int.smul";

let a: i32 = Mul(0x7FFF, 0x10000);
// CHECK:STDERR: fail_overflow.carbon:[[@LINE+3]]:14: ERROR: Integer overflow in calculation 32768 * 65536.
// CHECK:STDERR: let b: i32 = Mul(0x8000, 0x10000);
// CHECK:STDERR:              ^~~~
let b: i32 = Mul(0x8000, 0x10000);

// CHECK:STDOUT: --- int_mul.carbon
// CHECK:STDOUT:
// CHECK:STDOUT: constants {
// CHECK:STDOUT:   %Mul: type = fn_type @Mul [template]
// CHECK:STDOUT:   %.1: type = tuple_type () [template]
// CHECK:STDOUT:   %struct.1: Mul = struct_value () [template]
// CHECK:STDOUT:   %.2: i32 = int_literal 3 [template]
// CHECK:STDOUT:   %.3: i32 = int_literal 2 [template]
// CHECK:STDOUT:   %.4: i32 = int_literal 6 [template]
// CHECK:STDOUT:   %.5: type = array_type %.4, i32 [template]
// CHECK:STDOUT:   %.6: type = ptr_type [i32; 6] [template]
// CHECK:STDOUT:   %RuntimeCall: type = fn_type @RuntimeCall [template]
// CHECK:STDOUT:   %struct.2: RuntimeCall = struct_value () [template]
// CHECK:STDOUT: }
// CHECK:STDOUT:
// CHECK:STDOUT: file {
// CHECK:STDOUT:   package: <namespace> = namespace [template] {
// CHECK:STDOUT:     .Core = %Core
// CHECK:STDOUT:     .Mul = %Mul.decl
// CHECK:STDOUT:     .arr = %arr
// CHECK:STDOUT:     .RuntimeCall = %RuntimeCall.decl
// CHECK:STDOUT:   }
// CHECK:STDOUT:   %Core: <namespace> = namespace [template] {}
// CHECK:STDOUT:   %Mul.decl: Mul = fn_decl @Mul [template = constants.%struct.1] {
// CHECK:STDOUT:     %a.loc2_8.1: i32 = param a
// CHECK:STDOUT:     @Mul.%a: i32 = bind_name a, %a.loc2_8.1
// CHECK:STDOUT:     %b.loc2_16.1: i32 = param b
// CHECK:STDOUT:     @Mul.%b: i32 = bind_name b, %b.loc2_16.1
// CHECK:STDOUT:     @Mul.%return: ref i32 = var <return slot>
// CHECK:STDOUT:   }
// CHECK:STDOUT:   %Mul.ref: Mul = name_ref Mul, %Mul.decl [template = constants.%struct.1]
// CHECK:STDOUT:   %.loc4_20: i32 = int_literal 3 [template = constants.%.2]
// CHECK:STDOUT:   %.loc4_23: i32 = int_literal 2 [template = constants.%.3]
// CHECK:STDOUT:   %int.smul: init i32 = call %Mul.ref(%.loc4_20, %.loc4_23) [template = constants.%.4]
// CHECK:STDOUT:   %.loc4_25: type = array_type %int.smul, i32 [template = constants.%.5]
// CHECK:STDOUT:   %arr.var: ref [i32; 6] = var arr
// CHECK:STDOUT:   %arr: ref [i32; 6] = bind_name arr, %arr.var
// CHECK:STDOUT:   %.loc5_18: i32 = int_literal 6 [template = constants.%.4]
// CHECK:STDOUT:   %.loc5_19: type = array_type %.loc5_18, i32 [template = constants.%.5]
// CHECK:STDOUT:   %.loc5_20: type = ptr_type [i32; 6] [template = constants.%.6]
// CHECK:STDOUT:   %arr.ref: ref [i32; 6] = name_ref arr, %arr
// CHECK:STDOUT:   %.loc5_24: [i32; 6]* = addr_of %arr.ref
// CHECK:STDOUT:   %arr_p: [i32; 6]* = bind_name arr_p, %.loc5_24
// CHECK:STDOUT:   %RuntimeCall.decl: RuntimeCall = fn_decl @RuntimeCall [template = constants.%struct.2] {
// CHECK:STDOUT:     %a.loc7_16.1: i32 = param a
// CHECK:STDOUT:     @RuntimeCall.%a: i32 = bind_name a, %a.loc7_16.1
// CHECK:STDOUT:     %b.loc7_24.1: i32 = param b
// CHECK:STDOUT:     @RuntimeCall.%b: i32 = bind_name b, %b.loc7_24.1
// CHECK:STDOUT:     @RuntimeCall.%return: ref i32 = var <return slot>
// CHECK:STDOUT:   }
// CHECK:STDOUT: }
// CHECK:STDOUT:
// CHECK:STDOUT: fn @Mul(%a: i32, %b: i32) -> i32 = "int.smul";
// CHECK:STDOUT:
// CHECK:STDOUT: fn @RuntimeCall(%a: i32, %b: i32) -> i32 {
// CHECK:STDOUT: !entry:
// CHECK:STDOUT:   %Mul.ref: Mul = name_ref Mul, file.%Mul.decl [template = constants.%struct.1]
// CHECK:STDOUT:   %a.ref: i32 = name_ref a, %a
// CHECK:STDOUT:   %b.ref: i32 = name_ref b, %b
// CHECK:STDOUT:   %int.smul: init i32 = call %Mul.ref(%a.ref, %b.ref)
// CHECK:STDOUT:   %.loc8_19.1: i32 = value_of_initializer %int.smul
// CHECK:STDOUT:   %.loc8_19.2: i32 = converted %int.smul, %.loc8_19.1
// CHECK:STDOUT:   return %.loc8_19.2
// CHECK:STDOUT: }
// CHECK:STDOUT:
// CHECK:STDOUT: --- fail_overflow.carbon
// CHECK:STDOUT:
// CHECK:STDOUT: constants {
// CHECK:STDOUT:   %Mul: type = fn_type @Mul [template]
// CHECK:STDOUT:   %.1: type = tuple_type () [template]
// CHECK:STDOUT:   %struct: Mul = struct_value () [template]
// CHECK:STDOUT:   %.2: i32 = int_literal 32767 [template]
// CHECK:STDOUT:   %.3: i32 = int_literal 65536 [template]
// CHECK:STDOUT:   %.4: i32 = int_literal 2147418112 [template]
// CHECK:STDOUT:   %.5: i32 = int_literal 32768 [template]
// CHECK:STDOUT:   %.6: i32 = int_literal -2147483648 [template]
// CHECK:STDOUT: }
// CHECK:STDOUT:
// CHECK:STDOUT: file {
// CHECK:STDOUT:   package: <namespace> = namespace [template] {
// CHECK:STDOUT:     .Core = %Core
// CHECK:STDOUT:     .Mul = %Mul.decl
// CHECK:STDOUT:   }
// CHECK:STDOUT:   %Core: <namespace> = namespace [template] {}
// CHECK:STDOUT:   %Mul.decl: Mul = fn_decl @Mul [template = constants.%struct] {
// CHECK:STDOUT:     %a.loc4_8.1: i32 = param a
// CHECK:STDOUT:     @Mul.%a: i32 = bind_name a, %a.loc4_8.1
// CHECK:STDOUT:     %b.loc4_16.1: i32 = param b
// CHECK:STDOUT:     @Mul.%b: i32 = bind_name b, %b.loc4_16.1
// CHECK:STDOUT:     @Mul.%return: ref i32 = var <return slot>
// CHECK:STDOUT:   }
// CHECK:STDOUT:   %Mul.ref.loc6: Mul = name_ref Mul, %Mul.decl [template = constants.%struct]
// CHECK:STDOUT:   %.loc6_18: i32 = int_literal 32767 [template = constants.%.2]
// CHECK:STDOUT:   %.loc6_26: i32 = int_literal 65536 [template = constants.%.3]
// CHECK:STDOUT:   %int.smul.loc6: init i32 = call %Mul.ref.loc6(%.loc6_18, %.loc6_26) [template = constants.%.4]
// CHECK:STDOUT:   %.loc6_34.1: i32 = value_of_initializer %int.smul.loc6 [template = constants.%.4]
// CHECK:STDOUT:   %.loc6_34.2: i32 = converted %int.smul.loc6, %.loc6_34.1 [template = constants.%.4]
// CHECK:STDOUT:   %a.loc6: i32 = bind_name a, %.loc6_34.2
// CHECK:STDOUT:   %Mul.ref.loc10: Mul = name_ref Mul, %Mul.decl [template = constants.%struct]
// CHECK:STDOUT:   %.loc10_18: i32 = int_literal 32768 [template = constants.%.5]
// CHECK:STDOUT:   %.loc10_26: i32 = int_literal 65536 [template = constants.%.3]
// CHECK:STDOUT:   %int.smul.loc10: init i32 = call %Mul.ref.loc10(%.loc10_18, %.loc10_26) [template = constants.%.6]
// CHECK:STDOUT:   %.loc10_34.1: i32 = value_of_initializer %int.smul.loc10 [template = constants.%.6]
// CHECK:STDOUT:   %.loc10_34.2: i32 = converted %int.smul.loc10, %.loc10_34.1 [template = constants.%.6]
// CHECK:STDOUT:   %b.loc10: i32 = bind_name b, %.loc10_34.2
// CHECK:STDOUT: }
// CHECK:STDOUT:
// CHECK:STDOUT: fn @Mul(%a: i32, %b: i32) -> i32 = "int.smul";
// CHECK:STDOUT:
