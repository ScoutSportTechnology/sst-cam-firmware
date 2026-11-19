// Build the fixed RF-DETR batch-2 FP16 engine on the target Jetson, not under
// QEMU.
#include <NvInfer.h>
#include <NvOnnxParser.h>

#include <cstdio>
#include <exception>
#include <fstream>
#include <initializer_list>
#include <memory>
#include <stdexcept>
#include <string>

namespace {
struct Logger : nvinfer1::ILogger {
  void log(Severity severity, const char *message) noexcept override {
    if (severity <= Severity::kWARNING)
      std::fprintf(stderr, "TensorRT: %s\n", message);
  }
};
template <typename T> struct Delete {
  void operator()(T *p) const { delete p; }
};

void require(bool condition, const std::string &message) {
  if (!condition)
    throw std::runtime_error(message);
}

void checkTensor(const nvinfer1::ITensor *tensor, const char *name,
                 std::initializer_list<int> expected) {
  require(tensor != nullptr, std::string("missing tensor: ") + name);
  auto dims = tensor->getDimensions();
  std::fprintf(stderr, "%s: type=%d dims=", tensor->getName(),
               static_cast<int>(tensor->getType()));
  for (int i = 0; i < dims.nbDims; ++i)
    std::fprintf(stderr, "%s%lld", i ? "x" : "",
                 static_cast<long long>(dims.d[i]));
  std::fputc('\n', stderr);
  require(std::string(tensor->getName()) == name &&
              tensor->getType() == nvinfer1::DataType::kFLOAT &&
              dims.nbDims == static_cast<int>(expected.size()),
          std::string("unexpected tensor name/type/rank: expected FP32 ") +
              name);
  int i = 0;
  for (int dim : expected) {
    require(dims.d[i++] == dim,
            std::string("unexpected dimensions for ") + name);
  }
}
} // namespace

int main(int argc, char **argv) {
  if (argc != 3) {
    std::fprintf(
        stderr,
        "usage: %s <rfdetr-small.onnx> <output.engine>\n"
        "Build the FP16 engine on the Jetson (not the cross-build host).\n",
        argv[0]);
    return 2;
  }
  try {
    Logger logger;
    std::unique_ptr<nvinfer1::IBuilder, Delete<nvinfer1::IBuilder>> builder(
        nvinfer1::createInferBuilder(logger));
    require(builder != nullptr,
            "TensorRT builder unavailable (run on Jetson with CUDA)");
    std::unique_ptr<nvinfer1::INetworkDefinition,
                    Delete<nvinfer1::INetworkDefinition>>
        network(builder->createNetworkV2(0));
    require(network != nullptr, "cannot create TensorRT network");
    std::unique_ptr<nvonnxparser::IParser, Delete<nvonnxparser::IParser>>
        parser(nvonnxparser::createParser(*network, logger));
    require(parser != nullptr, "cannot create ONNX parser");
    if (!parser->parseFromFile(
            argv[1], static_cast<int>(nvinfer1::ILogger::Severity::kWARNING))) {
      for (int i = 0; i < parser->getNbErrors(); ++i)
        std::fprintf(stderr, "ONNX parser: %s\n", parser->getError(i)->desc());
      throw std::runtime_error(std::string("ONNX parse failed: ") + argv[1]);
    }
    require(network->getNbInputs() == 1 && network->getNbOutputs() == 2,
            "RF-DETR requires one input and two outputs");
    checkTensor(network->getInput(0), "input", {2, 3, 512, 512});
    bool dets = false, labels = false;
    for (int i = 0; i < network->getNbOutputs(); ++i) {
      auto *output = network->getOutput(i);
      require(output != nullptr, "null ONNX output");
      if (std::string(output->getName()) == "dets") {
        checkTensor(output, "dets", {2, 300, 4});
        dets = true;
      } else if (std::string(output->getName()) == "labels") {
        checkTensor(output, "labels", {2, 300, 91});
        labels = true;
      } else {
        throw std::runtime_error(std::string("unexpected ONNX output: ") +
                                 output->getName());
      }
    }
    require(dets && labels, "missing RF-DETR outputs");
    std::unique_ptr<nvinfer1::IBuilderConfig, Delete<nvinfer1::IBuilderConfig>>
        config(builder->createBuilderConfig());
    require(config != nullptr, "cannot create builder config");
    // TensorRT 10.16 deprecates both APIs but still uses kFP16 for weakly typed
    // ONNX networks.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    require(builder->platformHasFastFp16(),
            "target does not support fast FP16");
    config->setFlag(nvinfer1::BuilderFlag::kFP16);
#pragma GCC diagnostic pop
    std::unique_ptr<nvinfer1::IHostMemory, Delete<nvinfer1::IHostMemory>> plan(
        builder->buildSerializedNetwork(*network, *config));
    require(plan != nullptr && plan->size() != 0,
            "TensorRT engine build failed (see diagnostics above)");
    std::ofstream out(argv[2], std::ios::binary | std::ios::trunc);
    require(out.good(), std::string("cannot open output engine: ") + argv[2]);
    out.write(static_cast<const char *>(plan->data()),
              static_cast<std::streamsize>(plan->size()));
    out.close();
    require(out.good(), std::string("cannot write output engine: ") + argv[2]);
    std::fprintf(stderr, "wrote %zu-byte FP16 engine to %s\n", plan->size(),
                 argv[2]);
    return 0;
  } catch (const std::exception &e) {
    std::fprintf(stderr, "engine build: %s\n", e.what());
    return 1;
  }
}
