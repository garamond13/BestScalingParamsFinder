#pragma once

#include "common.h"

class Engine
{
public:
    void init();
    void create_image(const void* data);
    void resample_image();
    double compare();
    float scale;
private:
    void create_device();
    void create_sampler();
    void create_vertex_shader();
    void create_pixel_shader(const BYTE* shader, size_t shader_size);
    void create_constant_buffer(UINT byte_width, const void* data, ID3D11Buffer** buffer);
    void update_constant_buffer(ID3D11Buffer* buffer, const void* data, size_t size);
    void unbind_render_targets();
    void draw_pass(UINT width, UINT height);
	void create_viewport(float width, float height);
    void pass_linearize();
    void pass_delinearize();
    void pass_cylindrical_resample();
    void pass_orthogonal_resample();
    Microsoft::WRL::ComPtr<ID3D11Device> device;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> device_context;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv_pass;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv_image;
};