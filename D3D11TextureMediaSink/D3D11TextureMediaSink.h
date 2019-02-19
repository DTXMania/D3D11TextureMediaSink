#pragma once

// D3D11TextureMediaSink のクラスID。
// {6A0D25F9-1A0B-4512-AEAC-E2AC1CBDC333}
DEFINE_GUID(CLSID_D3D11TextureMediaSink, 0x6a0d25f9, 0x1a0b, 0x4512, 0xae, 0xac, 0xe2, 0xac, 0x1c, 0xbd, 0xc3, 0x33);

// D3D11TextureMediaSinkActivate のクラスID。
// {9EA010E9-21E3-4E07-A9EE-0AB55D624EAB}
DEFINE_GUID(CLSID_D3D11TextureMediaSinkActivate, 0x9ea010e9, 0x21e3, 0x4e07, 0xa9, 0xee, 0xa, 0xb5, 0x5d, 0x62, 0x4e, 0xab);

// D3D11TextureMediaSink との IMFSample 受け取りに使う属性GUID。
// {87468A84-1CD6-41E3-9522-A8625EB3A5F8}
DEFINE_GUID(TMS_SAMPLE, 0x87468a84, 0x1cd6, 0x41e3, 0x95, 0x22, 0xa8, 0x62, 0x5e, 0xb3, 0xa5, 0xf8);

// creation methods exposed by the lib
STDAPI CreateD3D11TextureMediaSink(REFIID ridd, void** ppvObject, void* pDXGIDeviceManager, void* pD3D11Device);

