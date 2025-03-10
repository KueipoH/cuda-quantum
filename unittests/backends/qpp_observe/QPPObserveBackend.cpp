/*************************************************************** -*- C++ -*- ***
 * Copyright (c) 2022 - 2023 NVIDIA Corporation & Affiliates.                  *
 * All rights reserved.                                                        *
 *                                                                             *
 * This source code and the accompanying materials are made available under    *
 * the terms of the Apache License 2.0 which accompanies this distribution.    *
 *******************************************************************************/

#define __NVQIR_QPP_TOGGLE_CREATE
#include "qpp/QppCircuitSimulator.cpp"
#undef __NVQIR_QPP_TOGGLE_CREATE
#include "cudaq/spin_op.h"

namespace cudaq {

/// @brief Create a new CircuitSimulator backend for testing
/// that provides its own observe functionality.
class QppObserveTester : public nvqir::QppCircuitSimulator<qpp::ket> {
public:
  NVQIR_SIMULATOR_CLONE_IMPL(QppObserveTester)
  bool canHandleObserve() override { return true; }
  cudaq::ExecutionResult observe(const cudaq::spin_op &op) override {
    flushGateQueue();

    ::qpp::cmat X = ::qpp::Gates::get_instance().X;
    ::qpp::cmat Y = ::qpp::Gates::get_instance().Y;
    ::qpp::cmat Z = ::qpp::Gates::get_instance().Z;

    auto nQ = op.n_qubits();
    auto bsf = op.get_bsf();
    double sum = 0.0;

    // Want to loop over all terms in op and
    // compute E_i = coeff_i * < psi | Term | psi >
    // = coeff_i * sum_k <psi | Pauli_k psi>
    for (std::size_t i = 0; i < op.n_terms(); i++) {
      auto term = op[i];
      if (!term.is_identity()) {
        ::qpp::ket cached = state;
        auto bsf = term.get_bsf()[0];
        for (std::size_t i = 0; i < nQ; i++) {
          if (bsf[i] && bsf[i + nQ])
            cached = ::qpp::apply(cached, Y, {i});
          else if (bsf[i])
            cached = ::qpp::apply(cached, X, {i});
          else if (bsf[i + nQ])
            cached = ::qpp::apply(cached, Z, {i});
        }

        sum += term.get_term_coefficient(0).real() *
               state.transpose().dot(cached).real();
      } else {
        sum += term.get_term_coefficient(0).real();
      }
    }

    return cudaq::ExecutionResult({}, sum);
  }

  std::string name() const override { return "qpp-test"; }
};
} // namespace cudaq

NVQIR_REGISTER_SIMULATOR(cudaq::QppObserveTester, qpp_test)
