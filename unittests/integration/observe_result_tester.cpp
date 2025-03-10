/*************************************************************** -*- C++ -*- ***
 * Copyright (c) 2022 - 2023 NVIDIA Corporation & Affiliates.                  *
 * All rights reserved.                                                        *
 *                                                                             *
 * This source code and the accompanying materials are made available under    *
 * the terms of the Apache License 2.0 which accompanies this distribution.    *
 *******************************************************************************/

#include "CUDAQTestUtils.h"
#include <cudaq/algorithm.h>

struct deuteron_n3_ansatz {
  void operator()(double x0, double x1) __qpu__ {
    cudaq::qreg q(3);
    x(q[0]);
    ry(x0, q[1]);
    ry(x1, q[2]);
    x<cudaq::ctrl>(q[2], q[0]);
    x<cudaq::ctrl>(q[0], q[1]);
    ry(-x0, q[1]);
    x<cudaq::ctrl>(q[0], q[1]);
    x<cudaq::ctrl>(q[1], q[0]);
  }
};

CUDAQ_TEST(ObserveResult, checkSimple) {

  using namespace cudaq::spin;
  cudaq::spin_op h = 5.907 - 2.1433 * x(0) * x(1) - 2.1433 * y(0) * y(1) +
                     .21829 * z(0) - 6.125 * z(1);

  auto ansatz = [](double theta) __qpu__ {
    cudaq::qubit q, r;
    x(q);
    ry(theta, r);
    x<cudaq::ctrl>(r, q);
  };

  double result = cudaq::observe(ansatz, h, 0.59);
  EXPECT_NEAR(result, -1.7487, 1e-3);
  printf("Get energy directly as double %lf\n", result);

  auto obs_res = cudaq::observe(ansatz, h, 0.59);
  EXPECT_NEAR(obs_res.exp_val_z(), -1.7487, 1e-3);
  printf("Energy from observe_result %lf\n", obs_res.exp_val_z());

  auto &platform = cudaq::get_platform();

  printf("\n\nLAST ONE!\n");
  auto obs_res2 = cudaq::observe(100000, ansatz, h, 0.59);
  EXPECT_NEAR(obs_res2.exp_val_z(), -1.7, 1e-1);
  printf("Energy from observe_result with shots %lf\n", obs_res2.exp_val_z());
  obs_res2.dump();

  for (std::size_t i = 0; i < h.n_terms(); i++)
    if (!h[i].is_identity())
      printf("Fine-grain data access: %s = %lf\n", h[i].to_string().data(),
             obs_res2.exp_val_z(h[i]));

  auto x0x1Counts = obs_res2.counts(x(0) * x(1));
  x0x1Counts.dump();
  EXPECT_TRUE(x0x1Counts.size() == 4);
  platform.clear_shots();
}
