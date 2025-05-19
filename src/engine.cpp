#include "engine.h"
#include "global.h"
#include "config.h"
#include "ImageMetrics.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

// Shader byte code.
#include "vs_quad_hlsl.h"
#include "ps_resample_ortho_hlsl.h"
#include "ps_resample_cyl_hlsl.h"
#include "ps_linearize_hlsl.h"
#include "ps_delinearize_hlsl.h"

// Used for constant buffer data.
struct Cb_data
{
    union Cb_types
    {
        float f;
        int32_t i;
        uint32_t u;
    };

    Cb_types x;
    Cb_types y;
    Cb_types z;
    Cb_types w;
};

void Engine::init()
{
    create_device();
    create_sampler();
    create_vertex_shader();
}

void Engine::create_image(const void* data)
{
    D3D11_TEXTURE2D_DESC texture2d_desc = {};
    texture2d_desc.Width = g_src_width;
    texture2d_desc.Height = g_src_height;
    texture2d_desc.MipLevels = 1;
    texture2d_desc.ArraySize = 1;
    texture2d_desc.Format = DXGI_FORMAT_R8_UNORM;
    texture2d_desc.SampleDesc.Count = 1;
    texture2d_desc.Usage = D3D11_USAGE_IMMUTABLE;
    texture2d_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA subresource_data = {};
    subresource_data.pSysMem = data;
    subresource_data.SysMemPitch = g_src_width;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture2d;
    ensure(device->CreateTexture2D(&texture2d_desc, &subresource_data, texture2d.GetAddressOf()), == S_OK);
    ensure(device->CreateShaderResourceView(texture2d.Get(), nullptr, srv_image.ReleaseAndGetAddressOf()), == S_OK);
}

void Engine::resample_image()
{
    static const bool linearize = scale < 1.0f;
    srv_pass = srv_image;
    if (linearize) {
        pass_linearize();
    }
    if (config::filter == 0) {
        pass_orthogonal_resample();
    }
    else {
        pass_cylindrical_resample();
    }
    if (linearize) {
        pass_delinearize();
    }
}

double Engine::compare()
{
    // Create staging texture.
    D3D11_TEXTURE2D_DESC texture2d_desc = {};
    texture2d_desc.Width = g_dst_width;
    texture2d_desc.Height = g_dst_height;
    texture2d_desc.MipLevels = 1;
    texture2d_desc.ArraySize = 1;
    texture2d_desc.Format = DXGI_FORMAT_R32_FLOAT;
    texture2d_desc.SampleDesc.Count = 1;
    texture2d_desc.Usage = D3D11_USAGE_STAGING;
    texture2d_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;	
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture2d;
    ensure(device->CreateTexture2D(&texture2d_desc, nullptr, texture2d.GetAddressOf()), == S_OK);

    // Get data from the last pass.
    Microsoft::WRL::ComPtr<ID3D11Resource> resource;
    srv_pass->GetResource(resource.GetAddressOf());
    device_context->CopyResource(texture2d.Get(), resource.Get());
    D3D11_MAPPED_SUBRESOURCE mapped_subresource;
    ensure(device_context->Map(texture2d.Get(), 0, D3D11_MAP_READ, 0, &mapped_subresource), == S_OK);

    #if 0
    std::vector<uint8_t> data(g_dst_width * g_dst_height);
    for (int i = 0; i < g_dst_width; ++i) {
        for (int j = 0; j < g_dst_height; ++j) {
            data[i + j * g_dst_width] = reinterpret_cast<float*>(mapped_subresource.pData)[i + j * mapped_subresource.RowPitch / sizeof(float)] * 255.0f;
        }
    }
    stbi_write_png("saved.png", g_dst_width, g_dst_height, 1, data.data(), 0);
    #endif

    // Get SSIM between rescaled and reference image.
    auto reference_image = [&](int i, int j) { return static_cast<double>(g_reference_image_data[i + j * g_dst_width]) / 255.0; };
    auto resampled_image = [&](int i, int j) { return static_cast<double>(reinterpret_cast<float*>(mapped_subresource.pData)[i + j * mapped_subresource.RowPitch / sizeof(float)]); };
    const double result = Lomont::Graphics::ImageMetrics::SSIM(g_dst_width, g_dst_height, reference_image, resampled_image);

    device_context->Unmap(texture2d.Get(), 0);
    device_context->Flush();
    return result;
}

void Engine::create_device()
{
#ifdef NDEBUG
    const UINT flags = 0;
#else
    const UINT flags = D3D11_CREATE_DEVICE_DEBUG;
#endif

    const std::array feature_levels = {
        D3D_FEATURE_LEVEL_12_1,
        D3D_FEATURE_LEVEL_12_0,
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0
    };
    ensure(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, feature_levels.data(), feature_levels.size(), D3D11_SDK_VERSION, device.ReleaseAndGetAddressOf(), nullptr, device_context.ReleaseAndGetAddressOf()), == S_OK);
}

void Engine::create_sampler()
{
    D3D11_SAMPLER_DESC sampler_desc = {};
    sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler_desc.MaxAnisotropy = 1;
    sampler_desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler_state;
    ensure(device->CreateSamplerState(&sampler_desc, sampler_state.GetAddressOf()), == S_OK);
    device_context->PSSetSamplers(0, 1, sampler_state.GetAddressOf());
}

void Engine::create_vertex_shader()
{
    device_context->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertex_shader;
    ensure(device->CreateVertexShader(VS_QUAD, sizeof(VS_QUAD), nullptr, vertex_shader.GetAddressOf()), == S_OK);
    device_context->VSSetShader(vertex_shader.Get(), nullptr, 0);
}

void Engine::create_pixel_shader(const BYTE* shader, size_t shader_size)
{
    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixel_shader;
    ensure(device->CreatePixelShader(shader, shader_size, nullptr, pixel_shader.GetAddressOf()), == S_OK);
    device_context->PSSetShader(pixel_shader.Get(), nullptr, 0);
}

void Engine::create_constant_buffer(UINT byte_width, const void* data, ID3D11Buffer** buffer)
{
    D3D11_BUFFER_DESC buffer_desc = {};
    buffer_desc.ByteWidth = byte_width;
    buffer_desc.Usage = D3D11_USAGE_DYNAMIC;
    buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    D3D11_SUBRESOURCE_DATA subresource_data = {};
    subresource_data.pSysMem = data;
    ensure(device->CreateBuffer(&buffer_desc, &subresource_data, buffer), == S_OK);
    device_context->PSSetConstantBuffers(0, 1, buffer);
}

void Engine::update_constant_buffer(ID3D11Buffer* buffer, const void* data, size_t size)
{
    D3D11_MAPPED_SUBRESOURCE mapped_subresource;
    ensure(device_context->Map(buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_subresource), == S_OK);
    std::memcpy(mapped_subresource.pData, data, size);
    device_context->Unmap(buffer, 0);
}

void Engine::unbind_render_targets()
{
    ID3D11RenderTargetView* rtvs[] = { nullptr };
    device_context->OMSetRenderTargets(1, rtvs, nullptr);
}

void Engine::draw_pass(UINT width, UINT height)
{
    // Create texture.
    D3D11_TEXTURE2D_DESC texture2d_desc = {};
    texture2d_desc.Width = width;
    texture2d_desc.Height = height;
    texture2d_desc.MipLevels = 1;
    texture2d_desc.ArraySize = 1;
    texture2d_desc.Format = DXGI_FORMAT_R32_FLOAT;
    texture2d_desc.SampleDesc.Count = 1,
    texture2d_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture2d;
    ensure(device->CreateTexture2D(&texture2d_desc, nullptr, texture2d.GetAddressOf()), == S_OK);
    
    // Create render target view.
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv;
    ensure(device->CreateRenderTargetView(texture2d.Get(), nullptr, rtv.GetAddressOf()), == S_OK);

    // Draw to the render target view.
    device_context->OMSetRenderTargets(1, rtv.GetAddressOf(), nullptr);
    create_viewport(width, height);
    device_context->Draw(3, 0);
    unbind_render_targets();

    ensure(device->CreateShaderResourceView(texture2d.Get(), nullptr, srv_pass.ReleaseAndGetAddressOf()), == S_OK);
}

void Engine::create_viewport(float width, float height)
{
    D3D11_VIEWPORT viewport = {};
    viewport.Width = width;
    viewport.Height = height;
    device_context->RSSetViewports(1, &viewport);
}

void Engine::pass_linearize()
{
    device_context->PSSetShaderResources(0, 1, srv_pass.GetAddressOf());
    create_pixel_shader(PS_LINEARIZE, sizeof(PS_LINEARIZE));
    draw_pass(g_src_width, g_src_height);
}

void Engine::pass_delinearize()
{
    device_context->PSSetShaderResources(0, 1, srv_pass.GetAddressOf());
    create_pixel_shader(PS_DELINEARIZE, sizeof(PS_DELINEARIZE));
    draw_pass(g_dst_width, g_dst_height);
}

void Engine::pass_cylindrical_resample()
{
    alignas(16) Cb_data data[3];
    data[0].x.i = config::kernel; // index
    data[0].y.f = g_kernel_radius; // radius
    data[0].z.f = g_kernel_blur; // blur
    data[0].w.f = g_kernel_parameter1; // p1
    data[1].x.f = g_kernel_parameter2; // p2
    data[1].y.f = scale > 1.0f ? config::ar : -1.0f; // ar
    data[1].z.f = std::min(scale, 1.0f); // scale
    data[1].w.f = std::ceil(g_kernel_radius / std::min(scale, 1.0f)); // bound
    data[2].x.f = g_src_width; // dims.x
    data[2].y.f = g_src_height; // dims.y
    data[2].z.f = 1.0f / static_cast<float>(g_src_width); // pt.x
    data[2].w.f = 1.0f / static_cast<float>(g_src_height); // pt.y
    Microsoft::WRL::ComPtr<ID3D11Buffer> cb;
    create_constant_buffer(sizeof(data), &data, cb.GetAddressOf());
    device_context->PSSetShaderResources(0, 1, srv_pass.GetAddressOf());
    create_pixel_shader(PS_RESAMPLE_CYL, sizeof(PS_RESAMPLE_CYL));
    draw_pass(g_dst_width, g_dst_height);
}

void Engine::pass_orthogonal_resample()
{
    // Pass y axis.
    alignas(16) Cb_data data[4];
    data[0].x.i = config::kernel; // index
    data[0].y.f = g_kernel_radius; // radius
    data[0].z.f = g_kernel_blur; // blur
    data[0].w.f = g_kernel_parameter1; // p1
    data[1].x.f = g_kernel_parameter2; // p2
    data[1].y.f = scale > 1.0f ? config::ar : -1.0f; // ar
    data[1].z.f = std::min(scale, 1.0f); // scale
    data[1].w.f = std::ceil(g_kernel_radius / std::min(scale, 1.0f)); // bound
    data[2].x.f = g_src_width; // dims.x
    data[2].y.f = g_src_height; // dims.y
    data[2].z.f = 1.0f / static_cast<float>(g_src_width); // pt.x
    data[2].w.f = 1.0f / static_cast<float>(g_src_height); // pt.y
    data[3].x.f = 0.0f; // axis.x
    data[3].y.f = 1.0f; // axis.y
    Microsoft::WRL::ComPtr<ID3D11Buffer> cb0;
    create_constant_buffer(sizeof(data), data, cb0.GetAddressOf());
    device_context->PSSetShaderResources(0, 1, srv_pass.GetAddressOf());
    create_pixel_shader(PS_RESAMPLE_ORTHO, sizeof(PS_RESAMPLE_ORTHO));
    draw_pass(g_src_width, g_dst_height);

    // Pass x axis.
    data[3].x.f = 1.0f; // axis.x
    data[3].y.f = 0.0f; // axis.y
    update_constant_buffer(cb0.Get(), data, sizeof(data));
    device_context->PSSetShaderResources(0, 1, srv_pass.GetAddressOf());
    draw_pass(g_dst_width, g_dst_height);
}
