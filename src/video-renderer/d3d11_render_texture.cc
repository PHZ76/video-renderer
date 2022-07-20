#include "d3d11_render_texture.h"
#include "log.h"

#include "shader/d3d11/shader_d3d11_vertex.h"

#define DX_SAFE_RELEASE(p) { if(p) { (p)->Release(); (p) = NULL; } } 

using namespace DX;
using namespace DirectX;

namespace dx = DirectX;
constexpr auto PI = 3.14159265358979;

struct VertexShaderConstants
{
    DirectX::XMMATRIX world;
    DirectX::XMMATRIX view;
    DirectX::XMMATRIX proj;
};

struct Vertex
{
    XMFLOAT3 pos;
    XMFLOAT2 tex;
    XMFLOAT4 color;
};


const Vertex vertices[] =
{
    { XMFLOAT3(-1, 1,   0), XMFLOAT2(0, 0), XMFLOAT4(0,0,0,0) },
    { XMFLOAT3(1,  1,   0), XMFLOAT2(1, 0), XMFLOAT4(0,0,0,0) },
    { XMFLOAT3(1, -1,   0), XMFLOAT2(1, 1), XMFLOAT4(0,0,0,0) },
    { XMFLOAT3(-1, -1,  0), XMFLOAT2(0, 1), XMFLOAT4(0,0,0,0) },
};


static HRESULT CompileShaderFromFile(const WCHAR* file_name, LPCSTR entry_point, LPCSTR shader_model, ID3DBlob** blob_out)
{
    HRESULT hr = S_OK;

    DWORD shader_flags = D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY;
#ifdef _DEBUG
    // Set the D3DCOMPILE_DEBUG flag to embed debug information in the shaders.
    // Setting this flag improves the shader debugging experience, but still allows 
    // the shaders to be optimized and to run exactly the way they will run in 
    // the release configuration of this program.
    shader_flags |= D3DCOMPILE_DEBUG;

    // Disable optimizations to further improve shader debugging
    shader_flags |= D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    ID3DBlob* error_blob = nullptr;
    hr = D3DCompileFromFile(file_name, nullptr, nullptr, entry_point, shader_model,
        shader_flags, 0, blob_out, &error_blob);
    if (FAILED(hr)) {
        if (error_blob) {
            OutputDebugStringA(reinterpret_cast<const char*>(error_blob->GetBufferPointer()));
            error_blob->Release();
        }
        LOG("D3DCompileFromFile() failed, %x", hr);
        return hr;
    }

    if (error_blob) {
        error_blob->Release();
    }
    return hr;
}

D3D11RenderTexture::D3D11RenderTexture(ID3D11Device* d3d11_device)
    : d3d11_device_(d3d11_device)
{
    d3d11_device_->AddRef();
    d3d11_device_->GetImmediateContext(&d3d11_context_);
}

D3D11RenderTexture::~D3D11RenderTexture()
{
    Cleanup();
}

bool D3D11RenderTexture::InitTexture(UINT width, UINT height, DXGI_FORMAT format, D3D11_USAGE usage, UINT bind_flags, UINT cpu_flags, UINT misc_flags)
{
    if (!d3d11_device_) {
        return false;
    }

    DX_SAFE_RELEASE(texture_);
    DX_SAFE_RELEASE(texture_rtv_);
    DX_SAFE_RELEASE(texture_srv_);
    DX_SAFE_RELEASE(nv12_y_rtv_);
    DX_SAFE_RELEASE(nv12_uv_rtv_);
    DX_SAFE_RELEASE(nv12_y_srv_);
    DX_SAFE_RELEASE(nv12_uv_srv_);

    HRESULT hr = S_OK;

    D3D11_TEXTURE2D_DESC texture_desc;
    memset(&texture_desc, 0, sizeof(D3D11_TEXTURE2D_DESC));
    texture_desc.Width = width;
    texture_desc.Height = height;
    texture_desc.MipLevels = 1;
    texture_desc.ArraySize = 1;
    texture_desc.SampleDesc.Count = 1;
    texture_desc.SampleDesc.Quality = 0;
    texture_desc.Format = format;
    texture_desc.Usage = usage;
    texture_desc.BindFlags = bind_flags;
    texture_desc.CPUAccessFlags = cpu_flags;
    texture_desc.MiscFlags = misc_flags;

    hr = d3d11_device_->CreateTexture2D(&texture_desc, nullptr, &texture_);
    if (FAILED(hr)) {
        LOG("ID3D11Device::CreateTexture2D() failed, %x", hr);
        return false;
    }

    D3D11_RENDER_TARGET_VIEW_DESC rtv_desc;
    D3D11_SHADER_RESOURCE_VIEW_DESC rsv_desc;
    memset(&rtv_desc, 0, sizeof(D3D11_RENDER_TARGET_VIEW_DESC));
    memset(&rsv_desc, 0, sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC));

    if (format == DXGI_FORMAT_NV12)
    {
        if (bind_flags & D3D11_BIND_RENDER_TARGET) {
            rtv_desc.Format = DXGI_FORMAT_R8_UNORM;
            rtv_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
            rtv_desc.Texture2D.MipSlice = 0;

            hr = d3d11_device_->CreateRenderTargetView(texture_, &rtv_desc, &nv12_y_rtv_);
            if (FAILED(hr)) {
                LOG("ID3D11Device::CreateRenderTargetView(R8) failed, %x \n", hr);
                goto failed;
            }

            rtv_desc.Format = DXGI_FORMAT_R8G8_UNORM;
            hr = d3d11_device_->CreateRenderTargetView(texture_, &rtv_desc, &nv12_uv_rtv_);
            if (FAILED(hr)) {
                LOG("ID3D11Device::CreateRenderTargetView(R8G8) failed, %x \n", hr);
                goto failed;
            }
        }

        memset(&rsv_desc, 0, sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC));
        rsv_desc.Format = DXGI_FORMAT_R8_UNORM;
        rsv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        rsv_desc.Texture2D.MostDetailedMip = 0;
        rsv_desc.Texture2D.MipLevels = 1;

        hr = d3d11_device_->CreateShaderResourceView(texture_, &rsv_desc, &nv12_y_srv_);
        if (FAILED(hr)) {
            LOG("ID3D11Device::CreateShaderResourceView(R8) failed, %x \n", hr);
            goto failed;
        }

        rsv_desc.Format = DXGI_FORMAT_R8G8_UNORM;
        hr = d3d11_device_->CreateShaderResourceView(texture_, &rsv_desc, &nv12_uv_srv_);
        if (FAILED(hr)) {
            LOG("ID3D11Device::CreateShaderResourceView(R8G8) failed, %x \n", hr);
            goto failed;
        }
    }
    else
    {
        if (bind_flags & D3D11_BIND_RENDER_TARGET) {
            rtv_desc.Format = texture_desc.Format;
            rtv_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
            rtv_desc.Texture2D.MipSlice = 0;

            hr = d3d11_device_->CreateRenderTargetView(texture_, &rtv_desc, &texture_rtv_);
            if (FAILED(hr)) {
                LOG("ID3D11Device::CreateRenderTargetView() failed, %x \n", hr);
                goto failed;
            }
        }

        memset(&rsv_desc, 0, sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC));
        rsv_desc.Format = format;
        rsv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        rsv_desc.Texture2D.MostDetailedMip = 0;
        rsv_desc.Texture2D.MipLevels = 1;

        hr = d3d11_device_->CreateShaderResourceView(texture_, &rsv_desc, &texture_srv_);
        if (FAILED(hr)) {
            LOG("ID3D11Device::CreateShaderResourceView() failed, %x \n", hr);
            goto failed;
        }
    }

    return true;

failed:
    DX_SAFE_RELEASE(texture_);
    DX_SAFE_RELEASE(texture_rtv_);
    DX_SAFE_RELEASE(texture_srv_);
    DX_SAFE_RELEASE(nv12_y_rtv_);
    DX_SAFE_RELEASE(nv12_uv_rtv_);
    DX_SAFE_RELEASE(nv12_y_srv_);
    DX_SAFE_RELEASE(nv12_uv_srv_);
    return false;
}

bool D3D11RenderTexture::InitVertexShader()
{
    if (!d3d11_device_) {
        return false;
    }

    DX_SAFE_RELEASE(vertex_buffer_);
    DX_SAFE_RELEASE(vertex_layout_);
    DX_SAFE_RELEASE(vertex_constants_);
    DX_SAFE_RELEASE(vertex_shader_);

    HRESULT hr = S_OK;

    D3D11_TEXTURE2D_DESC desc;
    texture_->GetDesc(&desc);

    float width = static_cast<FLOAT>(desc.Width);
    float height = static_cast<FLOAT>(desc.Height);

    const D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
#if 0
    ID3DBlob* vs_blob = nullptr;
    bool result = CompileShaderFromFile(L"d3d11_vs.hlsl", "main", "vs_4_0", &vs_blob);
    if (!result) {
        LOG("CompileShaderFromFile(d3d11_vs.hlsl) failed.");
        return false;
    }

    hr = d3d11_device_->CreateVertexShader(
        vs_blob->GetBufferPointer(),
        vs_blob->GetBufferSize(),
        NULL,
        &vertex_shader_
    );
    if (FAILED(hr)) {
        vs_blob->Release();
        return false;
    }

    hr = d3d11_device_->CreateInputLayout(
        layout,
        ARRAYSIZE(layout),
        vs_blob->GetBufferPointer(),
        vs_blob->GetBufferSize(),
        &vertex_layout_
    );
    vs_blob->Release();

    if (FAILED(hr)) {
        LOG("ID3D11Device::CreateInputLayout() failed, %x.", hr);
        return false;
    }
#else
    hr = d3d11_device_->CreateVertexShader(
        shader_d3d11_vertex,
        sizeof(shader_d3d11_vertex),
        NULL,
        &vertex_shader_
    );

    hr = d3d11_device_->CreateInputLayout(
        layout,
        ARRAYSIZE(layout),
        shader_d3d11_vertex,
        sizeof(shader_d3d11_vertex),
        &vertex_layout_
    );

    if (FAILED(hr)) {
        LOG("ID3D11Device::CreateInputLayout() failed, %x.", hr);
        return false;
    }
#endif

    // 顶点
    {
        D3D11_BUFFER_DESC vertex_buffer_desc;
        D3D11_SUBRESOURCE_DATA vertex_buffer_data;

#if 0
        Vertex vertices[] =
        {
            { XMFLOAT3(0.0f,  0.0f,   0.0f), XMFLOAT2(0.0f, 0.0f), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) },
            { XMFLOAT3(0.0f,  height, 0.0f), XMFLOAT2(0.0f, 1.0f), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) },
            { XMFLOAT3(width, 0.0f,   0.0f), XMFLOAT2(1.0f, 0.0f), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) },
            { XMFLOAT3(width, height, 0.0f), XMFLOAT2(1.0f, 1.0f), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) },
        };
#else 

#endif

        memset(&vertex_buffer_desc, 0, sizeof(D3D11_BUFFER_DESC));
        vertex_buffer_desc.ByteWidth = sizeof(vertices);
        vertex_buffer_desc.Usage = D3D11_USAGE_DYNAMIC;
        vertex_buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        vertex_buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        memset(&vertex_buffer_data, 0, sizeof(D3D11_SUBRESOURCE_DATA));
        vertex_buffer_data.pSysMem = vertices;
        vertex_buffer_data.SysMemPitch = 0;
        vertex_buffer_data.SysMemSlicePitch = 0;

        hr = d3d11_device_->CreateBuffer(&vertex_buffer_desc, &vertex_buffer_data, &vertex_buffer_);
        if (FAILED(hr)) {
            LOG("ID3D11Device::CreateBuffer(VERTEX_BUFFER) failed, %x \n", hr);
            return false;
        }
    }

    // 索引
    {
        // ******************
        const UINT16 indices[] = {
            // 正面
            0, 1, 2,
            0, 2, 3, //  2, 3, 0,
        };
        indicesSize = sizeof(indices) / sizeof(indices[0]);

        // 设置索引缓冲区描述
        D3D11_BUFFER_DESC ibd = {};
        ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
        ibd.ByteWidth = sizeof(indices);
        ibd.StructureByteStride = sizeof(UINT16);

        // 新建索引缓冲区
        D3D11_SUBRESOURCE_DATA isd = {};
        isd.pSysMem = indices;

        d3d11_device_->CreateBuffer(&ibd, &isd, &m_pIndexBuffer);
        // 输入装配阶段的索引缓冲区设置
        d3d11_context_->IASetIndexBuffer(m_pIndexBuffer, DXGI_FORMAT_R32_UINT, 0);
    }

    // 常量向量
    {
        D3D11_BUFFER_DESC buffer_desc;
        memset(&buffer_desc, 0, sizeof(D3D11_BUFFER_DESC));
        buffer_desc.Usage = D3D11_USAGE_DEFAULT;
        buffer_desc.ByteWidth = sizeof(VertexShaderConstants);
        buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        buffer_desc.CPUAccessFlags = 0;


        // 常量向量的值
        nv::Camera camera;
        camera.Reset();
        camera.MoveBack(1);

        VertexShaderConstants m_CBuffer = {};
        m_CBuffer.world = DirectX::XMMatrixIdentity();// 获取单位向量// 单位矩阵的转置是它本身
        m_CBuffer.proj = DirectX::XMMatrixIdentity();

        m_CBuffer.view = camera.GetMatrix() * DirectX::XMMatrixTranspose(transformMatrix);

        D3D11_SUBRESOURCE_DATA isd = {};
        isd.pSysMem = &m_CBuffer;

        hr = d3d11_device_->CreateBuffer(&buffer_desc, &isd, &vertex_constants_);
        if (FAILED(hr)) {
            LOG("ID3D11Device::CreateBuffer() failed, %x \n", hr);
            return false;
        }

        d3d11_context_->VSSetConstantBuffers(0, 1, &vertex_constants_);
    }

    return true;
}

bool D3D11RenderTexture::InitPixelShader(CONST WCHAR* pathname, const BYTE* pixel_shader, size_t pixel_shader_size)
{
    HRESULT hr = S_OK;
    ID3DBlob* ps_blob = nullptr;

#if 0
    bool result = CompileShaderFromFile(pathname, "main", "ps_4_0", &ps_blob);
    if (!result) {
        LOG("CompileShaderFromFile() failed. \n");
        return false;
    }

    hr = d3d11_device_->CreatePixelShader(
        ps_blob->GetBufferPointer(),
        ps_blob->GetBufferSize(),
        nullptr,
        &pixel_shader_);
    ps_blob->Release();
#else 
    hr = d3d11_device_->CreatePixelShader(
        pixel_shader,
        pixel_shader_size,
        nullptr,
        &pixel_shader_);
#endif
    if (FAILED(hr)) {
        LOG("ID3D11Device::CreatePixelShader() failed, %x  \n", hr);
        return false;
    }

    return true;
}

bool D3D11RenderTexture::InitRasterizerState()
{
    D3D11_RASTERIZER_DESC raster_desc;
    memset(&raster_desc, 0, sizeof(D3D11_RASTERIZER_DESC));
    raster_desc.AntialiasedLineEnable = FALSE;
    raster_desc.CullMode = D3D11_CULL_NONE;
    raster_desc.DepthBias = 0;
    raster_desc.DepthBiasClamp = 0.0f;
    raster_desc.DepthClipEnable = TRUE;
    raster_desc.FillMode = D3D11_FILL_SOLID;
    raster_desc.FrontCounterClockwise = FALSE;
    raster_desc.MultisampleEnable = FALSE;
    raster_desc.ScissorEnable = FALSE;
    raster_desc.SlopeScaledDepthBias = 0.0f;

    HRESULT hr = d3d11_device_->CreateRasterizerState(&raster_desc, &rasterizer_state_);
    if (FAILED(hr)) {
        LOG("ID3D11Device::CreateRasterizerState(CULL_NONE) failed, %x \n", hr);
        return false;
    }

    return true;
}

ID3D11Texture2D* D3D11RenderTexture::GetTexture()
{
    return texture_;
}


ID3D11RenderTargetView* D3D11RenderTexture::GetRenderTargetView()
{
    return texture_rtv_;
}

ID3D11ShaderResourceView* D3D11RenderTexture::GetShaderResourceView()
{
    return texture_srv_;
}

ID3D11RenderTargetView* D3D11RenderTexture::GetNV12YRenderTargetView()
{
    return nv12_y_rtv_;
}

ID3D11RenderTargetView* D3D11RenderTexture::GetNV12UVRenderTargetView()
{
    return nv12_uv_rtv_;
}

ID3D11ShaderResourceView* D3D11RenderTexture::GetNV12YShaderResourceView()
{
    return nv12_y_srv_;
}

ID3D11ShaderResourceView* D3D11RenderTexture::GetNV12UVShaderResourceView()
{
    return nv12_uv_srv_;
}

void D3D11RenderTexture::Cleanup()
{
    DX_SAFE_RELEASE(rasterizer_state_);

    DX_SAFE_RELEASE(vertex_layout_);
    DX_SAFE_RELEASE(vertex_constants_);
    DX_SAFE_RELEASE(vertex_shader_);
    DX_SAFE_RELEASE(vertex_buffer_);
    DX_SAFE_RELEASE(pixel_shader_);

    DX_SAFE_RELEASE(texture_);
    DX_SAFE_RELEASE(texture_srv_);
    DX_SAFE_RELEASE(texture_rtv_);

    DX_SAFE_RELEASE(nv12_y_rtv_);
    DX_SAFE_RELEASE(nv12_uv_rtv_);
    DX_SAFE_RELEASE(nv12_y_srv_);
    DX_SAFE_RELEASE(nv12_uv_srv_);

    DX_SAFE_RELEASE(d3d11_device_);
    DX_SAFE_RELEASE(d3d11_context_);
}


void D3D11RenderTexture::ResetCameraMatrix() {
    transformMatrix = dx::XMMatrixRotationX(0);
}
void D3D11RenderTexture::MulTransformMatrix(const DirectX::XMMATRIX& matrix)
{
    transformMatrix *= matrix;
}

void D3D11RenderTexture::UpdateScaling(double videoW, double videoH, double winW, double winH, int angle)
{
    double srcRatio = 1;
    double dstRatio = 1;

    if (0 == angle % 180)
    {
        srcRatio = videoW / videoH;
        dstRatio = winW / winH;
    }
    else if (0 == angle % 90)
    {
        srcRatio = videoW / videoH;
        dstRatio = winH / winW;
    }


    if (srcRatio > dstRatio) {
        // video 比较宽, 缩小宽度
        MulTransformMatrix(DirectX::XMMatrixScaling(1, dstRatio / srcRatio, 1));
    }
    else if (srcRatio < dstRatio) {
        // video 比较高, 缩小高度
        MulTransformMatrix(DirectX::XMMatrixScaling(srcRatio / dstRatio, 1, 1));
    }
    else {
        MulTransformMatrix(DirectX::XMMatrixScaling(1, 1, 1));
    }

    m_angle = angle;
}


void D3D11RenderTexture::Begin()
{
    if (!texture_) {
        return;
    }

    D3D11_TEXTURE2D_DESC texture_desc;
    texture_->GetDesc(&texture_desc);

    float width = static_cast<FLOAT>(texture_desc.Width);
    float height = static_cast<FLOAT>(texture_desc.Height);

    D3D11_VIEWPORT viewport;
    viewport.Width = width;
    viewport.Height = height;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    d3d11_context_->RSSetViewports(1, &viewport);


    if (0) {
        D3D11_MAPPED_SUBRESOURCE mapped_resource;
        HRESULT hr = d3d11_context_->Map(vertex_buffer_, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_resource);
        if (FAILED(hr)) {
            LOG("ID3D11DeviceContext::Map() failed, %x \n", hr);
            return;
        }

        
        memcpy(mapped_resource.pData, vertices, sizeof(vertices));

        d3d11_context_->Unmap((ID3D11Resource*)vertex_buffer_, 0);
    }

    if (1)
    {
        VertexShaderConstants m_CBuffer = {};
        m_CBuffer.world = DirectX::XMMatrixIdentity();// 获取单位向量// 单位矩阵的转置是它本身
        m_CBuffer.view = DirectX::XMMatrixIdentity();
        m_CBuffer.proj = DirectX::XMMatrixIdentity();


        nv::Camera camera;
        camera.Reset();

        //this->MulTransformMatrix(dx::XMMatrixRotationRollPitchYaw(0, PI, 0));
        //camera.cameraPosX += 0.1;
        //camera.viewHeight -= 0.3;
        
        // DirectX::XMMatrixTranspose(transformMatrix);
        // camera.GetMatrix();
        // dx::XMMatrixRotationRollPitchYaw(0, 0, PI / 2);
        // dx::XMMatrixRotationZ(PI / 2);

        auto newMatrix = DirectX::XMMatrixIdentity();
        newMatrix *= camera.GetMatrix();
        newMatrix *= transformMatrix;
        newMatrix *= dx::XMMatrixRotationZ(PI * m_angle / 180);

        m_CBuffer.view = DirectX::XMMatrixTranspose(newMatrix);

        d3d11_context_->UpdateSubresource(
            (ID3D11Resource*)vertex_constants_,
            0,
            NULL,
            &m_CBuffer,
            0,
            0);

        // 变化需要调用
        // d3d11_context_->VSSetConstantBuffers();
    }


    d3d11_context_->OMGetRenderTargets(1, &cache_rtv_, &cache_dsv_);
    d3d11_context_->OMSetRenderTargets(0, NULL, NULL);

    if (texture_desc.Format == DXGI_FORMAT_NV12) {
        ID3D11RenderTargetView* nv12_rtv[2] = { nv12_y_rtv_ , nv12_uv_rtv_ };
        d3d11_context_->OMSetRenderTargets(2, nv12_rtv, NULL);
        d3d11_context_->ClearRenderTargetView(nv12_rtv[0], Colors::Black);
        d3d11_context_->ClearRenderTargetView(nv12_rtv[1], Colors::Black);
    }
    else {
        d3d11_context_->OMSetRenderTargets(1, &texture_rtv_, NULL);
        d3d11_context_->ClearRenderTargetView(texture_rtv_, Colors::Black);
    }

    if (rasterizer_state_) {
        d3d11_context_->RSSetState(rasterizer_state_);
    }

    if (vertex_layout_) {
        d3d11_context_->IASetInputLayout(vertex_layout_);
    }

    if (vertex_shader_) {
        d3d11_context_->VSSetShader(vertex_shader_, NULL, 0);
    }

    if (vertex_buffer_) {
        const UINT stride = sizeof(Vertex);
        const UINT offset = 0;
        d3d11_context_->IASetVertexBuffers(0, 1, &vertex_buffer_, &stride, &offset);

        d3d11_context_->IASetIndexBuffer(m_pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
    }

    if (vertex_constants_) {
        d3d11_context_->VSSetConstantBuffers(0, 1, &vertex_constants_);
    }

    if (pixel_shader_) {
        d3d11_context_->PSSetShader(pixel_shader_, NULL, 0);
    }
}

void D3D11RenderTexture::PSSetTexture(UINT slot, ID3D11ShaderResourceView* shader_resource_view)
{
    if (!d3d11_context_) {
        return;
    }

    d3d11_context_->PSSetShaderResources(slot, 1, &shader_resource_view);
}

void D3D11RenderTexture::PSSetConstant(UINT slot, ID3D11Buffer* buffer)
{
    if (!d3d11_context_) {
        return;
    }

    d3d11_context_->PSSetConstantBuffers(slot, 1, &buffer);
}

void D3D11RenderTexture::PSSetSamplers(UINT slot, ID3D11SamplerState* sampler)
{
    if (!d3d11_context_) {
        return;
    }
    d3d11_context_->PSSetSamplers(slot, 1, &sampler);
}

void D3D11RenderTexture::Draw()
{
    if (!d3d11_context_) {
        return;
    }

    d3d11_context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY::D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    d3d11_context_->DrawIndexed(indicesSize, 0, 0);
    d3d11_context_->Flush();
}

void D3D11RenderTexture::End()
{
    if (!texture_) {
        return;
    }
    d3d11_context_->OMSetRenderTargets(0, NULL, NULL);
    d3d11_context_->OMSetRenderTargets(1, &cache_rtv_, cache_dsv_);
    DX_SAFE_RELEASE(cache_rtv_);
    DX_SAFE_RELEASE(cache_dsv_);
}