/*************************************************************** -*- C++ -*- ***
 * Copyright (c) 2022 - 2023 NVIDIA Corporation & Affiliates.                  *
 * All rights reserved.                                                        *
 *                                                                             *
 * This source code and the accompanying materials are made available under    *
 * the terms of the Apache License 2.0 which accompanies this distribution.    *
 *******************************************************************************/

#pragma once

#include "mlir/IR/OpImplementation.h"

namespace cudaq {

template <typename ConcreteType>
class Hermitian : public mlir::OpTrait::TraitBase<ConcreteType, Hermitian> {};

template <typename ConcreteType>
class QuantumGate : public mlir::OpTrait::TraitBase<ConcreteType, QuantumGate> {
};

} // namespace cudaq
