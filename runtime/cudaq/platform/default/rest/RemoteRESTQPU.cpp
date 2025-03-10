/*************************************************************** -*- C++ -*- ***
 * Copyright (c) 2022 - 2023 NVIDIA Corporation & Affiliates.                  *
 * All rights reserved.                                                        *
 *                                                                             *
 * This source code and the accompanying materials are made available under    *
 * the terms of the Apache License 2.0 which accompanies this distribution.    *
 *******************************************************************************/

#include "Executor.h"
#include "common/ExecutionContext.h"
#include "common/Logger.h"
#include "common/RestClient.h"
#include "cudaq/platform/qpu.h"
#include "nvqpp_config.h"

#include "common/RuntimeMLIR.h"
#include "cudaq/platform/quantum_platform.h"
#include <cudaq/spin_op.h>
#include <fmt/core.h>
#include <fstream>
#include <iostream>
#include <netinet/in.h>
#include <regex>
#include <sys/socket.h>
#include <sys/types.h>

#include "cudaq/Frontend/nvqpp/AttributeNames.h"
#include "cudaq/Optimizer/CodeGen/Passes.h"
#include "cudaq/Optimizer/Dialect/CC/CCDialect.h"
#include "cudaq/Optimizer/Dialect/Quake/QuakeDialect.h"
#include "cudaq/Optimizer/Transforms/Passes.h"
#include "cudaq/Support/Plugin.h"
#include "cudaq/Target/OpenQASM/OpenQASMEmitter.h"
#include "llvm/Bitcode/BitcodeWriter.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/Base64.h"
#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/Dialect/Math/IR/Math.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/ExecutionEngine/OptUtils.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/Diagnostics.h"
#include "mlir/IR/ImplicitLocOpBuilder.h"
#include "mlir/Parser/Parser.h"
#include "mlir/Pass/PassManager.h"
#include "mlir/Pass/PassRegistry.h"
#include "mlir/Target/LLVMIR/Dialect/LLVMIR/LLVMToLLVMIRTranslation.h"
#include "mlir/Target/LLVMIR/Export.h"
#include "mlir/Tools/mlir-translate/Translation.h"

using namespace mlir;

namespace cudaq {
std::string get_quake_by_name(const std::string &);
} // namespace cudaq

namespace {

constexpr char platformLoweringConfig[] = "PLATFORM_LOWERING_CONFIG";
constexpr char codeEmissionType[] = "CODEGEN_EMISSION";

/// @brief The RemoteRESTQPU is a subtype of QPU that enables the
/// execution of CUDA Quantum kernels on remotely hosted quantum computing
/// services via a REST Client / Server interaction. This type is meant
/// to be general enough to support any remotely hosted service. Specific
/// details about JSON payloads are abstracted via an abstract type called
/// ServerHelper, which is meant to be subtyped by each provided remote QPU
/// service. Moreover, this QPU handles launching kernels under a number of
/// Execution Contexts, including sampling and observation via synchronous or
/// asynchronous client invocations. This type should enable both QIR-based
/// backends as well as those that take OpenQASM2 as input.
class RemoteRESTQPU : public cudaq::QPU {
protected:
  /// The number of shots
  std::optional<int> nShots;

  /// @brief the platform file path, CUDAQ_INSTALL/platforms
  std::filesystem::path platformPath;

  /// @brief The Pass pipeline string, configured by the
  /// QPU config file in the platform path.
  std::string passPipelineConfig = "canonicalize";

  /// @brief The name of the QPU being targeted
  std::string qpuName;

  std::string codegenTranslation = "";

  // Pointer to the concrete Executor for this QPU
  std::unique_ptr<cudaq::Executor> executor;

  /// @brief Pointer to the concrete ServerHelper, provides
  /// specific JSON payloads and POST/GET URL paths.
  std::unique_ptr<cudaq::ServerHelper> serverHelper;

  /// @brief Mapping of general key-values for backend
  /// configuration.
  std::map<std::string, std::string> backendConfig;

public:
  /// @brief The constructor
  RemoteRESTQPU() : QPU() {
    std::filesystem::path cudaqLibPath{cudaq::getCUDAQLibraryPath()};
    platformPath = cudaqLibPath.parent_path().parent_path() / "platforms";
    // Default is to run sampling via the remote rest call
    executor = std::make_unique<cudaq::Executor>();
  }

  RemoteRESTQPU(RemoteRESTQPU &&) = delete;
  virtual ~RemoteRESTQPU() = default;

  void enqueue(cudaq::QuantumTask &task) override {
    execution_queue->enqueue(task);
  }

  /// @brief Return true if the current backend is a simulator
  /// @return
  bool isSimulator() override { return false; }

  /// @brief Return true if the current backend supports conditional feedback
  bool supportsConditionalFeedback() override { return false; }

  /// Provide the number of shots
  void setShots(int _nShots) override {
    nShots = _nShots;
    executor->setShots(static_cast<std::size_t>(_nShots));
  }

  /// Clear the number of shots
  void clearShots() override { nShots = std::nullopt; }
  virtual bool isRemote() override { return true; }

  /// Store the execution context for launchKernel
  void setExecutionContext(cudaq::ExecutionContext *context) override {
    if (!context)
      return;

    cudaq::info("Remote Rest QPU setting execution context to {}",
                context->name);

    // Execution context is valid
    executionContext = context;
  }

  /// Reset the execution context
  void resetExecutionContext() override {
    // do nothing here
    executionContext = nullptr;
  }

  /// @brief This setTargetBackend override is in charge of reading the
  /// specific target backend configuration file (bundled as part of this
  /// CUDA Quantum installation) and extract MLIR lowering pipelines and
  /// specific code generation output required by this backend (QIR/QASM2).
  void setTargetBackend(const std::string &backend) override {
    cudaq::info("Remote REST platform is targeting {}.", backend);

    // First we see if the given backend has extra config params
    auto mutableBackend = backend;
    if (mutableBackend.find(";") != std::string::npos) {
      auto split = cudaq::split(mutableBackend, ';');
      mutableBackend = split[0];
      // Must be key-value pairs, therefore an even number of values here
      if ((split.size() - 1) % 2 != 0)
        throw std::runtime_error(
            "Backend config must be provided as key-value pairs: " +
            std::to_string(split.size()));

      // Add to the backend configuration map
      for (std::size_t i = 1; i < split.size(); i += 2)
        backendConfig.insert({split[i], split[i + 1]});
    }

    /// Once we know the backend, we should search for the config file
    /// from there we can get the URL/PORT and the required MLIR pass
    /// pipeline.
    std::string fileName = mutableBackend + std::string(".config");
    auto configFilePath = platformPath / fileName;
    cudaq::info("Config file path = {}", configFilePath.string());
    std::ifstream configFile(configFilePath.string());
    std::string configContents((std::istreambuf_iterator<char>(configFile)),
                               std::istreambuf_iterator<char>());

    // Loop through the file, extract the pass pipeline and CODEGEN Type
    auto lines = cudaq::split(configContents, '\n');
    for (auto &line : lines) {
      if (line.find(platformLoweringConfig) != std::string::npos) {
        auto keyVal = cudaq::split(line, '=');
        auto value = std::regex_replace(keyVal[1], std::regex("\""), "");
        cudaq::info("Appending lowering pipeline: {}", value);
        passPipelineConfig += "," + value;
      } else if (line.find(codeEmissionType) != std::string::npos) {
        auto keyVal = cudaq::split(line, '=');
        codegenTranslation = keyVal[1];
      }
    }

    // Set the qpu name
    qpuName = mutableBackend;

    // Create the ServerHelper for this QPU and give it the backend config
    serverHelper = cudaq::registry::get<cudaq::ServerHelper>(qpuName);
    serverHelper->initialize(backendConfig);

    // Give the server helper to the executor
    executor->setServerHelper(serverHelper.get());
  }

  /// @brief Extract the Quake representation for the given kernel name and
  /// lower it to the code format required for the specific backend. The
  /// lowering process is controllable via the platforms/BACKEND.config file for
  /// this targeted backend.
  std::vector<cudaq::KernelExecution>
  lowerQuakeCode(const std::string &kernelName, void *kernelArgs) {

    auto contextPtr = cudaq::initializeMLIR();
    MLIRContext &context = *contextPtr.get();

    // Get the quake representation of the kernel
    auto quakeCode = cudaq::get_quake_by_name(kernelName);
    auto m_module = parseSourceString<ModuleOp>(quakeCode, &context);

    // Extract the kernel name
    auto func = m_module->lookupSymbol<mlir::func::FuncOp>(
        std::string("__nvqpp__mlirgen__") + kernelName);

    // Create a new Module to clone the function into
    auto location = FileLineColLoc::get(&context, "<builder>", 1, 1);
    ImplicitLocOpBuilder builder(location, &context);

    // FIXME this should be added to the builder.
    if (!func->hasAttr(cudaq::entryPointAttrName))
      func->setAttr(cudaq::entryPointAttrName, builder.getUnitAttr());
    auto moduleOp = builder.create<ModuleOp>();
    moduleOp.push_back(func.clone());

    // Lambda to apply a specific pipeline to the given ModuleOp
    auto runPassPipeline = [&](const std::string &pipeline,
                               ModuleOp moduleOpIn) {
      PassManager pm(&context);
      std::string errMsg;
      llvm::raw_string_ostream os(errMsg);
      cudaq::info("Pass pipeline for {} = {}", kernelName, pipeline);
      if (failed(parsePassPipeline(pipeline, pm, os)))
        throw std::runtime_error(
            "Remote rest platform failed to add passes to pipeline (" + errMsg +
            ").");
      if (failed(pm.run(moduleOpIn)))
        throw std::runtime_error("Remote rest platform Quake lowering failed.");
    };

    // Run the config-specified pass pipeline
    runPassPipeline(passPipelineConfig, moduleOp);

    if (kernelArgs) {
      PassManager pm(&context);
      pm.addPass(cudaq::opt::createQuakeSynthesizer(kernelName, kernelArgs));
      if (failed(pm.run(moduleOp)))
        throw std::runtime_error("Could not successfully apply quake-synth.");
    }

    std::vector<std::pair<std::string, ModuleOp>> modules;
    // Apply observations if necessary
    if (executionContext && executionContext->name == "observe") {

      cudaq::spin_op &spin = *executionContext->spin.value();
      for (std::size_t i = 0; i < spin.n_terms(); i++) {
        auto term = spin[i];
        if (term.is_identity())
          continue;

        // Get the ansatz
        auto ansatz = moduleOp.lookupSymbol<func::FuncOp>(
            std::string("__nvqpp__mlirgen__") + kernelName);

        // Create a new Module to clone the ansatz into it
        auto tmpModuleOp = builder.create<ModuleOp>();
        tmpModuleOp.push_back(ansatz.clone());

        // Extract the binary symplectic encoding
        auto binarySymplecticForm = term.get_bsf()[0];

        // Create the pass manager, add the quake observe ansatz pass
        // and run it followed by the canonicalizer
        PassManager pm(&context);
        OpPassManager &optPM = pm.nest<func::FuncOp>();
        optPM.addPass(
            cudaq::opt::createQuakeObserveAnsatzPass(binarySymplecticForm));
        if (failed(pm.run(tmpModuleOp)))
          throw std::runtime_error("Could not apply measurements to ansatz.");
        runPassPipeline("canonicalize", tmpModuleOp);
        modules.emplace_back(term.to_string(false), tmpModuleOp);
      }

    } else
      modules.emplace_back(kernelName, moduleOp);

    // Get the code gen translation
    auto translation = cudaq::getTranslation(codegenTranslation);

    // Apply user-specified codegen
    std::vector<cudaq::KernelExecution> codes;
    for (auto &[name, moduleOpI] : modules) {
      std::string codeStr;
      {
        llvm::raw_string_ostream outStr(codeStr);
        if (failed(translation(moduleOpI, outStr)))
          throw std::runtime_error("Could not successfully translate to " +
                                   codegenTranslation + ".");
      }
      codes.emplace_back(name, codeStr);
    }
    return codes;
  }

  /// @brief Launch the kernel. Extract the Quake code and lower to
  /// the representation required by the targeted backend. Handle all pertinent
  /// modifications for the execution context as well as async or sync
  /// invocation.
  void launchKernel(const std::string &kernelName, void (*kernelFunc)(void *),
                    void *args, std::uint64_t voidStarSize,
                    std::uint64_t resultOffset) override {
    cudaq::info("launching remote rest kernel ({})", kernelName);

    // TODO future iterations of this should support non-void return types.
    if (!executionContext)
      throw std::runtime_error("Remote rest execution can only be performed "
                               "via cudaq::sample() or cudaq::observe().");

    // Get the Quake code, lowered according to config file.
    auto codes = lowerQuakeCode(kernelName, args);

    // Execute the codes produced in quake lowering
    auto future = executor->execute(codes);

    // Keep this asynchronous if requested
    if (executionContext->asyncExec) {
      executionContext->futureResult = future;
      return;
    }

    // Otherwise make this synchronous
    executionContext->result = future.get();
  }
};
} // namespace

CUDAQ_REGISTER_TYPE(cudaq::QPU, RemoteRESTQPU, remote_rest)
