# D3D11TextureMediaSink

D3D11TexutreMediaSink は、Direct3D11テクスチャ（ID3D11Texture2D）の形で動画を出力する、MediaSession 用のカスタム MediaSink です。
Microsoft の [DX11VideoRenderer サンプル](https://github.com/Microsoft/Windows-classic-samples/tree/master/Samples/DX11VideoRenderer) を元にして作りました。

* MediaSession を構築する際に D3D11TextureMediaSink をビデオレンダラとして登録すると、動画の再生時に、フレーム画像を含んだ `IMFSample` COM オブジェクトを取得することができます。この `IMFSample` のメディアバッファには `ID3D11Texture2D` リソースが含まれています。
* テクスチャのフォーマットは `DXGI_FORMAT_B8G8R8A8_UNorm` で固定です。
* スケジューラでキャッシュすることができる出力テクスチャは、最大5枚です。
* デコード後のビデオ処理（インタレース解除や色変換）には D3D11/DXVA2.0 を使用します。XVP (Media Foundation Transcode Video Processor) には対応していません。
* `IMFActivate` には対応していません。
* レジストリへの登録には対応していません。

## ソリューション構成

* D3D11TextureMediaSink プロジェクト(C++)
  * D3D11TextureMediaSink.lib/dll を生成します。

* D3D11TextureMediaSinkDemo プロジェクト(C++)
  * D3D11TextureMediaSink.lib/dll を使って動画を再生するサンプルです。

* D3D11TextureMediaSinkDemoCShart プロジェクト(C#)
  * D3D11TextureMediaSink.dll を使って動画を再生するサンプルのC#版です。
  * C# で DirectX と Media Foundation を扱うために、ビルド時に [SharpDX](http://sharpdx.org/) と [MediaFoundation](https://www.nuget.org/packages/MediaFoundation/) を NuGet から取得します。

## 開発/動作要件

* Windows 10 October 2018 Update (1809)
* DirectX 11.0
* Visual Studio Community 2017 Version 15.9.7
  * C++
    * Windows 10 SDK for October 2018 Update (10.0.17763.0)
    * Visual Studio 2017 (v141) Toolset
  * C#
    * Visual C# 7.0
    * .NET Framework 4.7.1

## ライセンス

* MIT Licence

## 使い方

### (1) 初期化

* 初めに、アプリ側で、`ID3D11Device` と `IMFDXGIDeviceManager` を作成しておきます。
  * D3D11TextureMediaSink は、`IMFDXGIDeviceManager` の lock 機能を使用せず、渡された `ID3D11Device` をそのまま使います。  
  そのため、`ID3D11Device` には `ID3D10Multithread::SetMultithreadProtected(TRUE)` を設定しておく必要があります。

* D3D11TextureMediaSink の `CreateD3D11TextureMediaSink` 関数で `IMFMediaSink` オブジェクトを生成します。  
引数には、アプリ側で生成した `IMFDXGIDeviceManager` オブジェクトと `ID3D11Device` オブジェクトを渡します。

```C++
// D3D11TextureMediaSink オブジェクトを生成し、IMFMediaSink インターフェースで受け取ります。
HRESULT hr;
hr = CreateD3D11TextureMediaSink(
  IID_IMFMediaSink,
  (void**)&m_pMediaSink, // IMFMediaSink* m_pMediaSink
  m_pDXGIDeviceManager,  // 作成済みの IMFDXGIDeviceManager
  m_pD3DDevice);         // 作成済みの ID3D11Device
```

* D3D11TextureMediaSink から、動画フレームの受け取りに必要となる `IMFAttributes` も取得しておきます。

```C++
hr = m_pMediaSink->QueryInterface(
  IID_IMFAttributes, 
  (void**)&m_pMediaSinkAttributes); // IMFAttributes* m_pMediaSinkAttributes
```

### (2) MediaSession の構築

* MediaSession を構築します。
* 部分トポロジーを作る際、ビデオの出力ノード（`IMFTopologyNode`）に、D3D11TextureMediaSink から取得できる最初の `IMFStreamSink` オブジェクトを割り当てます。

```C++
// IMFMediaSink から、ID 0（固定値）の IMFStreamSink を取得します。
IMFStreamSink* pStreamSink;
hr = pMediaSink->GetStreamSinkById(0, &pStreamSink);

// IMFStreamSink を、出力ノードに割り当てます。
hr = pOutputNode->SetObject(pStreamSink); // IMFTopologyNode* pOutputNode;
```

### (3) 動画フレームの取得と描画
* MediaSession の再生を開始します。
* 動画のフレーム（`IMFSample`）を、D3D11TextureMediaSink の `TMS_SAMPLE` 属性から取得します。

```C++
hr = m_pMediaSinkAttributes->GetUnknown(
  TMS_SAMPLE,         // D3D11TextureMediaSink.h で定義されているGUID
  IID_IMFSample,
  (void**)&pSample);  // IMFSample* pSample
```
* 取得した `IMFSample` から、`ID3D11Texture2D` を取得します。

```C++
// IMFSample からメディアバッファを取得します。
IMFMediaBuffer* pMediaBuffer;
hr = pSample->GetBufferByIndex(0, &pMediaBuffer);

// メディアバッファからDXGIバッファを取得します。
IMFDXGIBuffer* pDXGIBuffer;
hr = pMediaBuffer->QueryInterface(IID_IMFDXGIBuffer, (void**)&pDXGIBuffer);

// DXGIバッファから Texture2D を取得します。
ID3D11Texture2D* pTexture2D;
hr = pDXGIBuffer->GetResource(IID_ID3D11Texture2D, (void**)&pTexture2D);

// Texture2D のサブリソースインデックスを取得します。
UINT subIndex;
hr = pDXGIBuffer->GetSubresourceIndex(&subIndex);
```

* 取得した `ID3D11Texture2D` リソースを、アプリ側の好みの方法で描画します。

```C++
（略）
```

* 描画が終わったら、`IMFSample` を解放します。

```C++
pTexture2D->Release();
pDXGIBuffer->Release();
pMediaBufer->Release();
pSample->Release();
hr = m_pMediaSinkAttributes->SetUnknown(TMS_SAMPLE, NULL);
```

* 注意：  
D3D11TextureMediaSink は、内部のスケジューラに従い、常に、TMS_SAMPLE 属性で提供する `IMFSample` を、現在表示すべき最新のものに入れ替えていきます。  
アプリが TMS_SAMPLE 属性に対して `IMFAttributes::GetUnknown()` を呼び出すと、D3D11TextureMediaSink は、この `IMFSample` の入れ替え作業をいったん保留（ロック）します。  
アプリは、`IMFSample` の描画を終えてこれが不要になったら、`IMFAttributes::SetUnknown()` を呼び出して、解放を宣言してください。これが呼び出されると、D3D11TexutreMediaSink は、スケジュールによる最新の `IMFSample` の入れ替え作業を再開します。

* さらに注意：  
`IMFSample` の入れ替え作業は、長時間保留されるべきではありません。  
例えば、`IMFSample` から取得した `ID3D11Texture2D` リソースをピクセルシェーダに設定する場合には、シェーダーが実行されるまでそれを維持する必要があります。  
このような場合には、別のリソースに画像をコピーして利用することをお勧めします。
