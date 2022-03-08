// Copyright (c) 2019 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "common_directx11.h"

#include<map>

namespace d3d11 {
	static std::map<mfxMemId*, mfxHDL>             alloc_responses;
	static std::map<mfxHDL, mfxFrameAllocResponse> alloc_decode_responses;
	static std::map<mfxHDL, int>                   alloc_decode_ref_count;

	mfxStatus Initialize(mfxIMPL impl, mfxVersion ver, MFXVideoSession* session, mfxFrameAllocator* allocator, ID3D11Device* device)
	{
		mfxHandleType mfx_handle_type = MFX_HANDLE_D3D11_DEVICE;
		mfxStatus sts = MFX_ERR_NONE;

		impl |= MFX_IMPL_VIA_D3D11;

		sts = session->Init(impl, &ver);
		MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

		// If mfxFrameAllocator is provided it means we need to setup DirectX device and memory allocator
		if (allocator) {
			// Provide device manager to Media SDK
			sts = session->SetHandle(mfx_handle_type, device);
			MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

			allocator->pthis  = *session; // We use Media SDK session ID as the allocation identifier
			allocator->Alloc  = d3d11::simple_alloc;
			allocator->Free   = d3d11::simple_free;
			allocator->Lock   = d3d11::simple_lock;
			allocator->Unlock = d3d11::simple_unlock;
			allocator->GetHDL = d3d11::simple_gethdl;

			// Since we are using video memory we must provide Media SDK with an external allocator
			sts = session->SetFrameAllocator(allocator);
			MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
		}

		return sts;
	}

	//
	// Intel Media SDK memory allocator entrypoints....
	//
	mfxStatus _simple_alloc(mfxHDL pthis, mfxFrameAllocRequest* request, mfxFrameAllocResponse* response)
	{
		mfxSession session = reinterpret_cast<mfxSession>(pthis);
		mfxHDL device_handle = NULL;
		mfxStatus sts = MFXVideoCORE_GetHandle(session, MFX_HANDLE_D3D11_DEVICE, &device_handle);
		MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

		ID3D11Device* d3d11_device = reinterpret_cast<ID3D11Device*>(device_handle);

		// Determine surface format
		DXGI_FORMAT format;
		if (MFX_FOURCC_NV12 == request->Info.FourCC)
			format = DXGI_FORMAT_NV12;
		else if (MFX_FOURCC_RGB4 == request->Info.FourCC)
			format = DXGI_FORMAT_B8G8R8A8_UNORM;
		else if (MFX_FOURCC_YUY2 == request->Info.FourCC)
			format = DXGI_FORMAT_YUY2;
		else if (MFX_FOURCC_P8 == request->Info.FourCC) //|| MFX_FOURCC_P8_TEXTURE == request->Info.FourCC
			format = DXGI_FORMAT_P8;
		else
			format = DXGI_FORMAT_UNKNOWN;

		if (DXGI_FORMAT_UNKNOWN == format) {
			return MFX_ERR_UNSUPPORTED;
		}
			
		// Allocate custom container to keep texture and stage buffers for each surface
		// Container also stores the intended read and/or write operation.
		CustomMemId** mids = (CustomMemId**)calloc(request->NumFrameSuggested, sizeof(CustomMemId*));
		if (!mids) return MFX_ERR_MEMORY_ALLOC;

		for (int i = 0; i < request->NumFrameSuggested; i++) {
			mids[i] = (CustomMemId*)calloc(1, sizeof(CustomMemId));
			if (!mids[i]) {
				return MFX_ERR_MEMORY_ALLOC;
			}
			mids[i]->rw = request->Type & 0xF000; // Set intended read/write operation
		}

		request->Type = request->Type & 0x0FFF;

		// because P8 data (bitstream) for h264 encoder should be allocated by CreateBuffer()
		// but P8 data (MBData) for MPEG2 encoder should be allocated by CreateTexture2D()
		if (request->Info.FourCC == MFX_FOURCC_P8) {
			D3D11_BUFFER_DESC desc = { 0 };

			if (!request->NumFrameSuggested) return MFX_ERR_MEMORY_ALLOC;

			desc.ByteWidth = request->Info.Width * request->Info.Height;
			desc.Usage = D3D11_USAGE_STAGING;
			desc.BindFlags = 0;
			desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
			desc.MiscFlags = 0;
			desc.StructureByteStride = 0;

			ID3D11Buffer* buffer = 0;
			HRESULT hr = d3d11_device->CreateBuffer(&desc, 0, &buffer);
			if (FAILED(hr)) {
				return MFX_ERR_MEMORY_ALLOC;
			}
			mids[0]->memId = reinterpret_cast<ID3D11Texture2D*>(buffer);
		}
		else {
			D3D11_TEXTURE2D_DESC desc = { 0 };
			desc.Width = request->Info.Width;
			desc.Height = request->Info.Height;
			desc.MipLevels = 1;
			desc.ArraySize = 1; // number of subresources is 1 in this case
			desc.Format = format;
			desc.SampleDesc.Count = 1;
			desc.Usage = D3D11_USAGE_DEFAULT;
			desc.BindFlags = D3D11_BIND_DECODER;
			desc.MiscFlags = 0;
			//desc.MiscFlags            = D3D11_RESOURCE_MISC_SHARED;

			if ((MFX_MEMTYPE_FROM_VPPIN & request->Type) &&
				(DXGI_FORMAT_B8G8R8A8_UNORM == desc.Format)) {
				desc.BindFlags = D3D11_BIND_RENDER_TARGET;
				if (desc.ArraySize > 2)
					return MFX_ERR_MEMORY_ALLOC;
			}

			if ((MFX_MEMTYPE_FROM_VPPOUT & request->Type) ||
				(MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET & request->Type)) {
				desc.BindFlags = D3D11_BIND_RENDER_TARGET;
				if (desc.ArraySize > 2)
					return MFX_ERR_MEMORY_ALLOC;
			}

			if (DXGI_FORMAT_P8 == desc.Format)
				desc.BindFlags = 0;

			ID3D11Texture2D* pTexture2D;

			// Create surface textures
			for (size_t i = 0; i < request->NumFrameSuggested / desc.ArraySize; i++) {
				HRESULT hr = d3d11_device->CreateTexture2D(&desc, NULL, &pTexture2D);
				if (FAILED(hr)) {
					return MFX_ERR_MEMORY_ALLOC;
				}				
				mids[i]->memId = pTexture2D;
			}

			desc.ArraySize = 1;
			desc.Usage = D3D11_USAGE_STAGING;
			desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;// | D3D11_CPU_ACCESS_WRITE;
			desc.BindFlags = 0;
			desc.MiscFlags = 0;
			//desc.MiscFlags        = D3D11_RESOURCE_MISC_SHARED;

			// Create surface staging textures
			for (size_t i = 0; i < request->NumFrameSuggested; i++) {
				HRESULT hr = d3d11_device->CreateTexture2D(&desc, NULL, &pTexture2D);
				if (FAILED(hr)) {
					return MFX_ERR_MEMORY_ALLOC;
				}					
				mids[i]->memIdStage = pTexture2D;
			}
		}

		response->mids = (mfxMemId*)mids;
		response->NumFrameActual = request->NumFrameSuggested;
		return MFX_ERR_NONE;
	}

	mfxStatus simple_alloc(mfxHDL pthis, mfxFrameAllocRequest* request, mfxFrameAllocResponse* response)
	{
		mfxStatus sts = MFX_ERR_NONE;

		if (request->Type & MFX_MEMTYPE_SYSTEM_MEMORY)
			return MFX_ERR_UNSUPPORTED;

		if (alloc_decode_responses.find(pthis) != alloc_decode_responses.end() &&
			MFX_MEMTYPE_EXTERNAL_FRAME & request->Type &&
			MFX_MEMTYPE_FROM_DECODE & request->Type) {
			// Memory for this request was already allocated during manual allocation stage. Return saved response
			//   When decode acceleration device (DXVA) is created it requires a list of d3d surfaces to be passed.
			//   Therefore Media SDK will ask for the surface info/mids again at Init() stage, thus requiring us to return the saved response
			//   (No such restriction applies to Encode or VPP)
			*response = alloc_decode_responses[pthis];
			alloc_decode_ref_count[pthis]++;
		}
		else {
			sts = _simple_alloc(pthis, request, response);

			if (MFX_ERR_NONE == sts) {
				if (MFX_MEMTYPE_EXTERNAL_FRAME & request->Type &&
					MFX_MEMTYPE_FROM_DECODE & request->Type) {
					// Decode alloc response handling
					alloc_decode_responses[pthis] = *response;
					alloc_decode_ref_count[pthis]++;
				}
				else {
					// Encode and VPP alloc response handling
					alloc_responses[response->mids] = pthis;
				}
			}
		}

		return sts;
	}

	mfxStatus simple_lock(mfxHDL pthis, mfxMemId mid, mfxFrameData* ptr)
	{
		mfxSession session = reinterpret_cast<mfxSession>(pthis);
		mfxHDL device_handle = NULL;
		mfxStatus sts = MFXVideoCORE_GetHandle(session, MFX_HANDLE_D3D11_DEVICE, &device_handle);
		MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

		ID3D11Device* d3d11_device = reinterpret_cast<ID3D11Device*>(device_handle);

		HRESULT hr = S_OK;
		D3D11_TEXTURE2D_DESC        desc = { 0 };
		D3D11_MAPPED_SUBRESOURCE    lockedRect = { 0 };

		CustomMemId* memId = (CustomMemId*)mid;
		ID3D11Texture2D* pSurface = (ID3D11Texture2D*)memId->memId;
		ID3D11Texture2D* pStage = (ID3D11Texture2D*)memId->memIdStage;

		D3D11_MAP   mapType = D3D11_MAP_READ;
		UINT        mapFlags = D3D11_MAP_FLAG_DO_NOT_WAIT;

		ID3D11DeviceContext* d3d11_context = NULL;
		d3d11_device->GetImmediateContext(&d3d11_context);

		if (NULL == pStage) {
			hr = d3d11_context->Map(pSurface, 0, mapType, mapFlags, &lockedRect);
			desc.Format = DXGI_FORMAT_P8;
		}
		else {
			pSurface->GetDesc(&desc);

			// copy data only in case of user wants o read from stored surface
			if (memId->rw & WILL_READ)
				d3d11_context->CopySubresourceRegion(pStage, 0, 0, 0, 0, pSurface, 0, NULL);

			do {
				hr = d3d11_context->Map(pStage, 0, mapType, mapFlags, &lockedRect);
				if (S_OK != hr && DXGI_ERROR_WAS_STILL_DRAWING != hr)
					return MFX_ERR_LOCK_MEMORY;
			} while (DXGI_ERROR_WAS_STILL_DRAWING == hr);
		}

		d3d11_context->Release();

		if (FAILED(hr)) {
			return MFX_ERR_LOCK_MEMORY;
		}
			
		switch (desc.Format) {
		case DXGI_FORMAT_NV12:
			ptr->Pitch = (mfxU16)lockedRect.RowPitch;
			ptr->Y = (mfxU8*)lockedRect.pData;
			ptr->U = (mfxU8*)lockedRect.pData + desc.Height * lockedRect.RowPitch;
			ptr->V = ptr->U + 1;
			break;
		case DXGI_FORMAT_B8G8R8A8_UNORM:
			ptr->Pitch = (mfxU16)lockedRect.RowPitch;
			ptr->B = (mfxU8*)lockedRect.pData;
			ptr->G = ptr->B + 1;
			ptr->R = ptr->B + 2;
			ptr->A = ptr->B + 3;
			break;
		case DXGI_FORMAT_YUY2:
			ptr->Pitch = (mfxU16)lockedRect.RowPitch;
			ptr->Y = (mfxU8*)lockedRect.pData;
			ptr->U = ptr->Y + 1;
			ptr->V = ptr->Y + 3;
			break;
		case DXGI_FORMAT_P8:
			ptr->Pitch = (mfxU16)lockedRect.RowPitch;
			ptr->Y = (mfxU8*)lockedRect.pData;
			ptr->U = 0;
			ptr->V = 0;
			break;
		default:
			return MFX_ERR_LOCK_MEMORY;
		}

		return MFX_ERR_NONE;
	}

	mfxStatus simple_unlock(mfxHDL pthis, mfxMemId mid, mfxFrameData* ptr)
	{
		mfxSession session = reinterpret_cast<mfxSession>(pthis);
		mfxHDL device_handle = NULL;
		mfxStatus sts = MFXVideoCORE_GetHandle(session, MFX_HANDLE_D3D11_DEVICE, &device_handle);
		MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

		ID3D11Device* d3d11_device = reinterpret_cast<ID3D11Device*>(device_handle);

		CustomMemId*        memId = (CustomMemId*)mid;
		ID3D11Texture2D*    pSurface = (ID3D11Texture2D*)memId->memId;
		ID3D11Texture2D*    pStage = (ID3D11Texture2D*)memId->memIdStage;

		ID3D11DeviceContext* d3d11_context = NULL;
		d3d11_device->GetImmediateContext(&d3d11_context);

		if (NULL == pStage) {
			d3d11_context->Unmap(pSurface, 0);
		}
		else {
			d3d11_context->Unmap(pStage, 0);
			// copy data only in case of user wants to write to stored surface
			if (memId->rw & WILL_WRITE)
				d3d11_context->CopySubresourceRegion(pSurface, 0, 0, 0, 0, pStage, 0, NULL);
		}

		d3d11_context->Release();

		if (ptr) {
			ptr->Pitch = 0;
			ptr->U = ptr->V = ptr->Y = 0;
			ptr->A = ptr->R = ptr->G = ptr->B = 0;
		}

		return MFX_ERR_NONE;
	}

	mfxStatus simple_gethdl(mfxHDL pthis, mfxMemId mid, mfxHDL* handle)
	{
		pthis; // To suppress warning for this unused parameter

		if (NULL == handle)
			return MFX_ERR_INVALID_HANDLE;

		mfxHDLPair*     pPair = (mfxHDLPair*)handle;
		CustomMemId*    memId = (CustomMemId*)mid;

		pPair->first = memId->memId; // surface texture
		pPair->second = 0;

		return MFX_ERR_NONE;
	}


	mfxStatus _simple_free(mfxFrameAllocResponse* response)
	{
		if (response->mids) {
			for (mfxU32 i = 0; i < response->NumFrameActual; i++) {
				if (response->mids[i]) {
					CustomMemId*        mid = (CustomMemId*)response->mids[i];
					ID3D11Texture2D*    pSurface = (ID3D11Texture2D*)mid->memId;
					ID3D11Texture2D*    pStage = (ID3D11Texture2D*)mid->memIdStage;

					if (pSurface)
						pSurface->Release();
					if (pStage)
						pStage->Release();

					free(mid);
				}
			}
			free(response->mids);
			response->mids = NULL;
		}

		return MFX_ERR_NONE;
	}

	mfxStatus simple_free(mfxHDL pthis, mfxFrameAllocResponse* response)
	{
		if (NULL == response)
			return MFX_ERR_NULL_PTR;

		if (alloc_responses.find(response->mids) == alloc_responses.end()) {
			// Decode free response handling
			if (--alloc_decode_ref_count[pthis] == 0) {
				_simple_free(response);
				alloc_decode_responses.erase(pthis);
				alloc_decode_ref_count.erase(pthis);
			}
		}
		else {
			// Encode and VPP free response handling
			alloc_responses.erase(response->mids);
			_simple_free(response);
		}

		return MFX_ERR_NONE;
	}
}