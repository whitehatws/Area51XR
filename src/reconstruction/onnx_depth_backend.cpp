#include "area51xr/onnx_depth_backend.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#define ORT_API_MANUAL_INIT
#include <onnxruntime_cxx_api.h>
#undef ORT_API_MANUAL_INIT

#include <array>
#include <cstring>
#include <string>

namespace area51xr {

namespace {

#ifdef _WIN32
using OrtGetApiBaseFn = const OrtApiBase* (ORT_API_CALL*)();
#endif

} // namespace

struct OnnxRuntimeBackend::Impl {
#ifdef _WIN32
    HMODULE runtime_module{};
#endif
    std::unique_ptr<Ort::Env> env;
    std::unique_ptr<Ort::SessionOptions> options;
    std::unique_ptr<Ort::Session> session;
    std::string input_name;
    std::string output_name;
    std::string error;

    explicit Impl(const std::filesystem::path& model_path) {
#ifdef _WIN32
        runtime_module = LoadLibraryW(L"onnxruntime.dll");
        if (!runtime_module) {
            error = "failed to load onnxruntime.dll (Win32 error " +
                std::to_string(static_cast<unsigned long>(GetLastError())) + ")";
            return;
        }

        const auto get_api_base = reinterpret_cast<OrtGetApiBaseFn>(
            GetProcAddress(runtime_module, "OrtGetApiBase"));
        if (!get_api_base) {
            error = "onnxruntime.dll does not export OrtGetApiBase";
            FreeLibrary(runtime_module);
            runtime_module = nullptr;
            return;
        }

        const OrtApiBase* api_base = get_api_base();
        const OrtApi* api = api_base ? api_base->GetApi(ORT_API_VERSION) : nullptr;
        if (!api) {
            error = "onnxruntime.dll does not support the requested ORT API version";
            FreeLibrary(runtime_module);
            runtime_module = nullptr;
            return;
        }
        Ort::InitApi(api);
#endif

        try {
            env = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "Area51XR");
            options = std::make_unique<Ort::SessionOptions>();
            options->SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
#ifdef _WIN32
            session = std::make_unique<Ort::Session>(*env, model_path.wstring().c_str(), *options);
#else
            session = std::make_unique<Ort::Session>(*env, model_path.string().c_str(), *options);
#endif
            Ort::AllocatorWithDefaultOptions allocator;
            auto input = session->GetInputNameAllocated(0, allocator);
            auto output = session->GetOutputNameAllocated(0, allocator);
            if (!input || !output) {
                error = "ONNX model input/output names unavailable";
                session.reset();
                return;
            }
            input_name = input.get();
            output_name = output.get();
        } catch (const Ort::Exception& ex) {
            error = ex.what();
            session.reset();
            options.reset();
            env.reset();
        }
    }

    ~Impl() {
        session.reset();
        options.reset();
        env.reset();
#ifdef _WIN32
        if (runtime_module) {
            FreeLibrary(runtime_module);
            runtime_module = nullptr;
        }
#endif
    }
};

OnnxRuntimeBackend::OnnxRuntimeBackend(const std::filesystem::path& model_path)
    : impl_(std::make_unique<Impl>(model_path)) {}

OnnxRuntimeBackend::~OnnxRuntimeBackend() = default;

bool OnnxRuntimeBackend::run(
    std::span<const float> chw_rgb,
    std::uint32_t input_width,
    std::uint32_t input_height,
    std::vector<float>& output,
    std::uint32_t& output_width,
    std::uint32_t& output_height) {
    output.clear();
    output_width = 0;
    output_height = 0;
    auto& p = *impl_;
    p.error.clear();
    if (!p.session || input_width == 0 || input_height == 0 ||
        chw_rgb.size() != static_cast<std::size_t>(3u) * input_width * input_height) {
        p.error = p.session ? "invalid ONNX input tensor" : "ONNX session is unavailable";
        return false;
    }

    try {
        const std::array<std::int64_t, 4> shape{
            1,
            3,
            static_cast<std::int64_t>(input_height),
            static_cast<std::int64_t>(input_width)
        };
        auto memory = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        auto input_tensor = Ort::Value::CreateTensor<float>(
            memory,
            const_cast<float*>(chw_rgb.data()),
            chw_rgb.size(),
            shape.data(),
            shape.size());

        const char* input_names[] = {p.input_name.c_str()};
        const char* output_names[] = {p.output_name.c_str()};
        auto outputs = p.session->Run(
            Ort::RunOptions{nullptr},
            input_names,
            &input_tensor,
            1,
            output_names,
            1);
        if (outputs.empty() || !outputs[0].IsTensor()) {
            p.error = "ONNX model returned no tensor output";
            return false;
        }

        const auto info = outputs[0].GetTensorTypeAndShapeInfo();
        const auto output_shape = info.GetShape();
        if (output_shape.size() < 2) {
            p.error = "ONNX output tensor has unexpected rank";
            return false;
        }
        const std::int64_t h = output_shape[output_shape.size() - 2];
        const std::int64_t w = output_shape[output_shape.size() - 1];
        if (w <= 0 || h <= 0) {
            p.error = "ONNX output tensor has dynamic/invalid dimensions";
            return false;
        }

        const std::size_t count = info.GetElementCount();
        const std::size_t expected = static_cast<std::size_t>(w) * static_cast<std::size_t>(h);
        if (count != expected) {
            p.error = "ONNX output tensor contains unexpected extra dimensions";
            return false;
        }

        const float* data = outputs[0].GetTensorData<float>();
        output.assign(data, data + count);
        output_width = static_cast<std::uint32_t>(w);
        output_height = static_cast<std::uint32_t>(h);
        return true;
    } catch (const Ort::Exception& ex) {
        p.error = ex.what();
        return false;
    }
}

bool OnnxRuntimeBackend::valid() const noexcept {
    return impl_ && impl_->session != nullptr;
}

const char* OnnxRuntimeBackend::last_error() const noexcept {
    return impl_ ? impl_->error.c_str() : "ONNX backend unavailable";
}

} // namespace area51xr
