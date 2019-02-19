# D3D11TextureMediaSink

D3D11TexutreMediaSink は、Direct3D11テクスチャ（ID3D11Texture2D）の形で動画を出力する、Media Session 用のカスタム MediaSink です。
Microsoft の [DX11VideoRenderer サンプル](https://github.com/Microsoft/Windows-classic-samples/tree/master/Samples/DX11VideoRenderer) を元にして作りました。

* Media Session を構築する際に D3D11TextureMediaSink をビデオレンダラとして登録すると、動画の再生時に、フレーム画像を含んだ `IMFSample` COM オブジェクトを取得することができます。この `IMFSample` のメディアバッファには `ID3D11Texture2D` リソースが含まれています。
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

MIT Licence
