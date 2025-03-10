/*************************************************************** -*- C++ -*- ***
 * Copyright (c) 2022 - 2023 NVIDIA Corporation & Affiliates.                  *
 * All rights reserved.                                                        *
 *                                                                             *
 * This source code and the accompanying materials are made available under    *
 * the terms of the Apache License 2.0 which accompanies this distribution.    *
 *******************************************************************************/

#pragma once

// The OptCodeGen library includes passes that lower the MLIR module for some
// particular quantum target representation. There is a bevy of such targets
// that provide targets on which the quantum code can be run.

#include "mlir/Pass/Pass.h"
#include "mlir/Pass/PassRegistry.h"

namespace mlir {
class MLIRContext;
class Pass;
class PassManager;
} // namespace mlir

namespace cudaq::opt {

std::unique_ptr<mlir::Pass> createConvertToQIRPass();
void registerConvertToQIRPass();

/// Convert (generic) QIR to the Base Profile QIR for a specific target.
/// TODO: Decide how to convey the selected target information.
void addBaseProfilePipeline(mlir::PassManager &pm);

// Use the addBaseProfilePipeline() for the following passes.
std::unique_ptr<mlir::Pass> createQIRToBaseProfilePass();
std::unique_ptr<mlir::Pass> createBaseProfilePreparationPass();
std::unique_ptr<mlir::Pass> createConvertToQIRFuncPass();

std::unique_ptr<mlir::Pass> createR1DecompositionGateSetMapping();
std::unique_ptr<mlir::Pass> createRnDecompositionGateSetMapping();
std::unique_ptr<mlir::Pass> createQuantinuumGateSetMapping();
std::unique_ptr<mlir::Pass> createIQMGateSetMapping();

// declarative passes
#define GEN_PASS_REGISTRATION
#include "cudaq/Optimizer/CodeGen/Passes.h.inc"

} // namespace cudaq::opt
