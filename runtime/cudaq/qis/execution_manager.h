/*************************************************************** -*- C++ -*- ***
 * Copyright (c) 2022 - 2023 NVIDIA Corporation & Affiliates.                  *
 * All rights reserved.                                                        *
 *                                                                             *
 * This source code and the accompanying materials are made available under    *
 * the terms of the Apache License 2.0 which accompanies this distribution.    *
 *******************************************************************************/

#pragma once

#include "cudaq/spin_op.h"
#include <cassert>
#include <complex>
#include <cstddef>
#include <deque>
#include <span>
#include <string_view>
#include <vector>

namespace cudaq {
class ExecutionContext;
using SpinMeasureResult = std::pair<double, sample_result>;

// A QuditInfo, to the ExecutionManager, is a type encoding
// the number of levels and the id of the qudit.
struct QuditInfo {
  std::size_t levels = 0;
  std::size_t id = 0;
};

/// The ExecutionManager provides a base class describing a
/// concrete sub-system for allocating qudits and executing quantum
/// instructions on those qudits. This type is templated on the concrete
/// qudit type (qubit, qmode, etc). It exposes an API for getting an
/// available qudit id, returning that id, setting and resetting the
/// current execution context, and applying specific quantum instructions.
class ExecutionManager {
protected:
  /// Available qudit indices
  std::deque<std::size_t> availableIndices;

  /// Total qudits available
  std::size_t totalQudits;

  /// Internal - return the next qudit index
  std::size_t getNextIndex() {
    assert(!availableIndices.empty() && "No more qudits available.");
    auto next = availableIndices.front();
    availableIndices.pop_front();
    return next;
  }

  /// Internal - At qudit deallocation, return the qudit index
  void returnIndex(std::size_t idx) {
    availableIndices.push_front(idx);
    std::sort(availableIndices.begin(), availableIndices.end());
  }

  /// Internal - Get the number of remaining available qudit ids
  std::size_t numAvailable() { return availableIndices.size(); }

  /// Internal - Get the total number of qudit ids available
  std::size_t totalNumQudits() { return totalQudits; }

public:
  ExecutionManager() {
    totalQudits = 30; // platform.get_num_qubits();
    for (std::size_t i = 0; i < totalQudits; i++) {
      availableIndices.push_back(i);
    }
  }
  /// Return the next available qudit index
  virtual std::size_t getAvailableIndex(std::size_t quditLevels = 2) = 0;

  /// QuditInfo has been deallocated, return the qudit / id to the pool of
  /// qudits.
  virtual void returnQudit(const QuditInfo &q) = 0;

  /// Checker for qudits that were not deallocated
  bool memoryLeaked() { return numAvailable() != totalNumQudits(); }

  /// Provide an ExecutionContext for the current cudaq kernel
  virtual void setExecutionContext(cudaq::ExecutionContext *ctx) = 0;

  /// Reset the execution context
  virtual void resetExecutionContext() = 0;

  /// Apply the quantum instruction with the given name, on the provided
  /// target qudits. Supports input of control qudits and rotational parameters.
  virtual void apply(const std::string_view gateName,
                     const std::vector<double> &&params,
                     const std::vector<QuditInfo> &controls,
                     const std::vector<QuditInfo> &targets,
                     bool isAdjoint = false) = 0;

  virtual void resetQudit(const QuditInfo &id) = 0;

  /// Begin an region of code where all operations will be adjoint-ed
  virtual void startAdjointRegion() = 0;
  /// End the adjoint region
  virtual void endAdjointRegion() = 0;

  /// Start a region of code where all operations will be
  /// controlled on the given qudits.
  virtual void
  startCtrlRegion(const std::vector<std::size_t> &control_qubits) = 0;
  /// End the control region
  virtual void endCtrlRegion(std::size_t n_controls) = 0;

  /// Measure the qudit and return the observed state (0,1,2,3,...)
  /// e.g. for qubits, this can return 0 or 1;
  virtual int measure(const QuditInfo &target) = 0;

  /// Measure the current state in the given pauli basis, return
  /// the expectation value <term>.
  virtual SpinMeasureResult measure(cudaq::spin_op &op) = 0;

  /// Synchronize - run all queue-ed instructions
  virtual void synchronize() = 0;
  virtual ~ExecutionManager() {}
};

ExecutionManager *getExecutionManager();
} // namespace cudaq

// The following macro is to be used by ExecutionManager subclass
// developers. It will define the global thread_local execution manager
// pointer instance, and define the factory function for clients to
// get reference to the execution manager
#define CUDAQ_REGISTER_EXECUTION_MANAGER(Manager)                              \
  namespace cudaq {                                                            \
  ExecutionManager *getExecutionManager() {                                    \
    thread_local static std::unique_ptr<ExecutionManager> qis_manager =        \
        std::make_unique<Manager>();                                           \
    return qis_manager.get();                                                  \
  }                                                                            \
  }
