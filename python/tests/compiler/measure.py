# ============================================================================ #
# Copyright (c) 2022 - 2023 NVIDIA Corporation & Affiliates.                   #
# All rights reserved.                                                         #
#                                                                              #
# This source code and the accompanying materials are made available under     #
# the terms of the Apache License 2.0 which accompanies this distribution.     #
# ============================================================================ #

# RUN: PYTHONPATH=../../ pytest -rP  %s | FileCheck %s

import os

import pytest
import numpy as np

import cudaq

def test_kernel_measure_1q():
    """
    Test the measurement instruction for `cudaq.Kernel` by applying
    measurements to qubits one at a time.
    """
    kernel = cudaq.make_kernel()
    qreg = kernel.qalloc(2)
    qubit_0 = qreg[0]
    qubit_1 = qreg[1]
    # Check that we can apply measurements to 1 qubit at a time.
    kernel.mx(qubit_0)
    kernel.mx(qubit_1)
    kernel.my(qubit_0)
    kernel.my(qubit_1)
    kernel.mz(qubit_0)
    kernel.mz(qubit_1)
    kernel()
    assert kernel.arguments == []
    assert kernel.argument_count == 0
    print(kernel)


# CHECK-LABEL:   func.func @__nvqpp__mlirgen____nvqppBuilderKernel_{{.*}}() {
# CHECK:           %[[VAL_0:.*]] = arith.constant 1 : i32
# CHECK:           %[[VAL_1:.*]] = arith.constant 0 : i32
# CHECK:           %[[VAL_2:.*]] = quake.alloca : !quake.qvec<2>
# CHECK:           %[[VAL_3:.*]] = quake.qextract %[[VAL_2]]{{\[}}%[[VAL_1]]] : !quake.qvec<2>[i32] -> !quake.qref
# CHECK:           %[[VAL_4:.*]] = quake.qextract %[[VAL_2]]{{\[}}%[[VAL_0]]] : !quake.qvec<2>[i32] -> !quake.qref
# CHECK:           %[[VAL_5:.*]] = quake.mx(%[[VAL_3]] : !quake.qref) {registerName = ""} : i1
# CHECK:           %[[VAL_6:.*]] = quake.mx(%[[VAL_4]] : !quake.qref) {registerName = ""} : i1
# CHECK:           %[[VAL_7:.*]] = quake.my(%[[VAL_3]] : !quake.qref) {registerName = ""} : i1
# CHECK:           %[[VAL_8:.*]] = quake.my(%[[VAL_4]] : !quake.qref) {registerName = ""} : i1
# CHECK:           %[[VAL_9:.*]] = quake.mz(%[[VAL_3]] : !quake.qref) {registerName = ""} : i1
# CHECK:           %[[VAL_10:.*]] = quake.mz(%[[VAL_4]] : !quake.qref) {registerName = ""} : i1
# CHECK:           return
# CHECK:         }


def test_kernel_measure_qreg():
    """
    Test the measurement instruciton for `cudaq.Kernel` by applying
    measurements to an entire qreg.
    """
    kernel = cudaq.make_kernel()
    qreg = kernel.qalloc(3)
    # Check that we can apply measurements to an entire register.
    kernel.mx(qreg)
    kernel.my(qreg)
    kernel.mz(qreg)
    kernel()
    assert kernel.arguments == []
    assert kernel.argument_count == 0
    print(kernel)


# CHECK-LABEL:   func.func @__nvqpp__mlirgen____nvqppBuilderKernel_{{.*}}() {
# CHECK:           %[[VAL_0:.*]] = arith.constant 3 : index
# CHECK:           %[[VAL_1:.*]] = arith.constant 3 : i64
# CHECK:           %[[VAL_2:.*]] = arith.constant 1 : index
# CHECK:           %[[VAL_3:.*]] = arith.constant 0 : index
# CHECK:           %[[VAL_4:.*]] = quake.alloca : !quake.qvec<3>
# CHECK:           %[[VAL_5:.*]] = llvm.alloca %[[VAL_1]] x i1 : (i64) -> !llvm.ptr<i1>
# CHECK:           %[[VAL_6:.*]] = cc.loop while ((%[[VAL_7:.*]] = %[[VAL_3]]) -> (index)) {
# CHECK:             %[[VAL_8:.*]] = arith.cmpi slt, %[[VAL_7]], %[[VAL_0]] : index
# CHECK:             cc.condition %[[VAL_8]](%[[VAL_7]] : index)
# CHECK:           } do {
# CHECK:           ^bb0(%[[VAL_9:.*]]: index):
# CHECK:             %[[VAL_10:.*]] = quake.qextract %[[VAL_4]]{{\[}}%[[VAL_9]]] : !quake.qvec<3>[index] -> !quake.qref
# CHECK:             %[[VAL_11:.*]] = quake.mx(%[[VAL_10]] : !quake.qref) : i1
# CHECK:             %[[VAL_12:.*]] = arith.index_cast %[[VAL_9]] : index to i64
# CHECK:             %[[VAL_13:.*]] = llvm.getelementptr %[[VAL_5]]{{\[}}%[[VAL_12]]] : (!llvm.ptr<i1>, i64) -> !llvm.ptr<i1>
# CHECK:             llvm.store %[[VAL_11]], %[[VAL_13]] : !llvm.ptr<i1>
# CHECK:             cc.continue %[[VAL_9]] : index
# CHECK:           } step {
# CHECK:           ^bb0(%[[VAL_14:.*]]: index):
# CHECK:             %[[VAL_15:.*]] = arith.addi %[[VAL_14]], %[[VAL_2]] : index
# CHECK:             cc.continue %[[VAL_15]] : index
# CHECK:           } {counted}
# CHECK:           %[[VAL_16:.*]] = llvm.alloca %[[VAL_1]] x i1 : (i64) -> !llvm.ptr<i1>
# CHECK:           %[[VAL_17:.*]] = cc.loop while ((%[[VAL_18:.*]] = %[[VAL_3]]) -> (index)) {
# CHECK:             %[[VAL_19:.*]] = arith.cmpi slt, %[[VAL_18]], %[[VAL_0]] : index
# CHECK:             cc.condition %[[VAL_19]](%[[VAL_18]] : index)
# CHECK:           } do {
# CHECK:           ^bb0(%[[VAL_20:.*]]: index):
# CHECK:             %[[VAL_21:.*]] = quake.qextract %[[VAL_4]]{{\[}}%[[VAL_20]]] : !quake.qvec<3>[index] -> !quake.qref
# CHECK:             %[[VAL_22:.*]] = quake.my(%[[VAL_21]] : !quake.qref) : i1
# CHECK:             %[[VAL_23:.*]] = arith.index_cast %[[VAL_20]] : index to i64
# CHECK:             %[[VAL_24:.*]] = llvm.getelementptr %[[VAL_16]]{{\[}}%[[VAL_23]]] : (!llvm.ptr<i1>, i64) -> !llvm.ptr<i1>
# CHECK:             llvm.store %[[VAL_22]], %[[VAL_24]] : !llvm.ptr<i1>
# CHECK:             cc.continue %[[VAL_20]] : index
# CHECK:           } step {
# CHECK:           ^bb0(%[[VAL_25:.*]]: index):
# CHECK:             %[[VAL_26:.*]] = arith.addi %[[VAL_25]], %[[VAL_2]] : index
# CHECK:             cc.continue %[[VAL_26]] : index
# CHECK:           } {counted}
# CHECK:           %[[VAL_27:.*]] = llvm.alloca %[[VAL_1]] x i1 : (i64) -> !llvm.ptr<i1>
# CHECK:           %[[VAL_28:.*]] = cc.loop while ((%[[VAL_29:.*]] = %[[VAL_3]]) -> (index)) {
# CHECK:             %[[VAL_30:.*]] = arith.cmpi slt, %[[VAL_29]], %[[VAL_0]] : index
# CHECK:             cc.condition %[[VAL_30]](%[[VAL_29]] : index)
# CHECK:           } do {
# CHECK:           ^bb0(%[[VAL_31:.*]]: index):
# CHECK:             %[[VAL_32:.*]] = quake.qextract %[[VAL_4]]{{\[}}%[[VAL_31]]] : !quake.qvec<3>[index] -> !quake.qref
# CHECK:             %[[VAL_33:.*]] = quake.mz(%[[VAL_32]] : !quake.qref) : i1
# CHECK:             %[[VAL_34:.*]] = arith.index_cast %[[VAL_31]] : index to i64
# CHECK:             %[[VAL_35:.*]] = llvm.getelementptr %[[VAL_27]]{{\[}}%[[VAL_34]]] : (!llvm.ptr<i1>, i64) -> !llvm.ptr<i1>
# CHECK:             llvm.store %[[VAL_33]], %[[VAL_35]] : !llvm.ptr<i1>
# CHECK:             cc.continue %[[VAL_31]] : index
# CHECK:           } step {
# CHECK:           ^bb0(%[[VAL_36:.*]]: index):
# CHECK:             %[[VAL_37:.*]] = arith.addi %[[VAL_36]], %[[VAL_2]] : index
# CHECK:             cc.continue %[[VAL_37]] : index
# CHECK:           } {counted}
# CHECK:           return
# CHECK:         }


# leave for gdb debugging
if __name__ == "__main__":
    loc = os.path.abspath(__file__)
    pytest.main([loc, "-rP"])