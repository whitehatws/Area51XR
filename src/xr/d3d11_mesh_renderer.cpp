#include "area51xr/d3d11_mesh_renderer.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <d3d11.h>
#endif

#include <limits>
#include <string>

namespace area51xr {

struct D3D11MeshRenderer::Impl {
#ifdef _WIN32
    ID3D11Device* device{};
    ID3D11DeviceContext* context{};
    ID3D11Buffer* vertex_buffer{};
    ID3D11Buffer* index_buffer{};
#endif
    std::uint64_t frame_number{};
    std::uint32_t vertex_count{};
    std::uint32_t triangle_count{};
    bool initialized{};
    std::string error;

    bool fail(const char* message) noexcept {
        error = message;
        return false;
    }
};

D3D11MeshRenderer::D3D11MeshRenderer() : impl_(std::make_unique<Impl>()) {}
D3D11MeshRenderer::~D3D11MeshRenderer() { reset(); }

bool D3D11MeshRenderer::initialize(void* d3d_device, void* d3d_context) noexcept {
    reset();
#ifdef _WIN32
    if (!d3d_device || !d3d_context) {
        return impl_->fail("D3D11 mesh renderer requires device and context");
    }
    impl_->device = static_cast<ID3D11Device*>(d3d_device);
    impl_->context = static_cast<ID3D11DeviceContext*>(d3d_context);
    impl_->device->AddRef();
    impl_->context->AddRef();
    impl_->initialized = true;
    impl_->error.clear();
    return true;
#else
    (void)d3d_device;
    (void)d3d_context;
    return impl_->fail("D3D11 mesh renderer is only available on Windows");
#endif
}

bool D3D11MeshRenderer::upload(const SpatialMeshView& mesh) noexcept {
    if (!impl_->initialized) {
        return impl_->fail("D3D11 mesh renderer is not initialized");
    }
    if (mesh.vertices.empty() || mesh.indices.empty()) {
        return impl_->fail("spatial mesh is empty");
    }
    if ((mesh.indices.size() % 3u) != 0u) {
        return impl_->fail("spatial mesh index count is not divisible by three");
    }
    if (mesh.vertices.size() > std::numeric_limits<std::uint32_t>::max() ||
        mesh.indices.size() > std::numeric_limits<std::uint32_t>::max()) {
        return impl_->fail("spatial mesh is too large");
    }

    const std::uint32_t vertex_count = static_cast<std::uint32_t>(mesh.vertices.size());
    const std::uint32_t index_count = static_cast<std::uint32_t>(mesh.indices.size());
    const std::uint32_t triangle_count = index_count / 3u;

#ifdef _WIN32
    const std::uint64_t vertex_bytes64 =
        static_cast<std::uint64_t>(mesh.vertices.size()) * sizeof(SpatialMeshVertexView);
    const std::uint64_t index_bytes64 =
        static_cast<std::uint64_t>(mesh.indices.size()) * sizeof(std::uint32_t);
    if (vertex_bytes64 > std::numeric_limits<UINT>::max() ||
        index_bytes64 > std::numeric_limits<UINT>::max()) {
        return impl_->fail("spatial mesh GPU buffers are too large");
    }

    D3D11_BUFFER_DESC vb_desc{};
    vb_desc.ByteWidth = static_cast<UINT>(vertex_bytes64);
    vb_desc.Usage = D3D11_USAGE_DEFAULT;
    vb_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA vb_data{};
    vb_data.pSysMem = mesh.vertices.data();

    ID3D11Buffer* new_vb{};
    if (FAILED(impl_->device->CreateBuffer(&vb_desc, &vb_data, &new_vb))) {
        return impl_->fail("failed to create D3D11 mesh vertex buffer");
    }

    D3D11_BUFFER_DESC ib_desc{};
    ib_desc.ByteWidth = static_cast<UINT>(index_bytes64);
    ib_desc.Usage = D3D11_USAGE_DEFAULT;
    ib_desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    D3D11_SUBRESOURCE_DATA ib_data{};
    ib_data.pSysMem = mesh.indices.data();

    ID3D11Buffer* new_ib{};
    if (FAILED(impl_->device->CreateBuffer(&ib_desc, &ib_data, &new_ib))) {
        new_vb->Release();
        return impl_->fail("failed to create D3D11 mesh index buffer");
    }

    if (impl_->vertex_buffer) impl_->vertex_buffer->Release();
    if (impl_->index_buffer) impl_->index_buffer->Release();
    impl_->vertex_buffer = new_vb;
    impl_->index_buffer = new_ib;
#endif

    impl_->frame_number = mesh.frame_number;
    impl_->vertex_count = vertex_count;
    impl_->triangle_count = triangle_count;
    impl_->error.clear();
    return true;
}

void D3D11MeshRenderer::reset() noexcept {
#ifdef _WIN32
    if (impl_->vertex_buffer) impl_->vertex_buffer->Release();
    if (impl_->index_buffer) impl_->index_buffer->Release();
    if (impl_->context) impl_->context->Release();
    if (impl_->device) impl_->device->Release();
    impl_->vertex_buffer = nullptr;
    impl_->index_buffer = nullptr;
    impl_->context = nullptr;
    impl_->device = nullptr;
#endif
    impl_->frame_number = 0;
    impl_->vertex_count = 0;
    impl_->triangle_count = 0;
    impl_->initialized = false;
}

bool D3D11MeshRenderer::initialized() const noexcept { return impl_->initialized; }
std::uint64_t D3D11MeshRenderer::frame_number() const noexcept { return impl_->frame_number; }
std::uint32_t D3D11MeshRenderer::vertex_count() const noexcept { return impl_->vertex_count; }
std::uint32_t D3D11MeshRenderer::triangle_count() const noexcept { return impl_->triangle_count; }
const char* D3D11MeshRenderer::last_error() const noexcept { return impl_->error.c_str(); }

} // namespace area51xr
