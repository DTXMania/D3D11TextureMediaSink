using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Runtime.InteropServices;
using System.Threading;
using System.Threading.Tasks;
using MediaFoundation;
using MediaFoundation.Misc;

namespace D3D11TextureMediaSinkDemoCSharp
{
    class DemoPlay : IMFAsyncCallback, IDisposable
    {
        public DemoPlay( IntPtr hWnd, System.Drawing.Size clientSize )
        {
            // MediaFoundation のセットアップ
            SharpDX.MediaFoundation.MediaManager.Startup();

            // ID3D11Device の作成
            this.D3DDevice = new SharpDX.Direct3D11.Device(
                SharpDX.Direct3D.DriverType.Hardware,
                SharpDX.Direct3D11.DeviceCreationFlags.BgraSupport,
                new SharpDX.Direct3D.FeatureLevel[] { SharpDX.Direct3D.FeatureLevel.Level_11_0 } );

            // マルチスレッドモードを ON に設定する。DXVAを使う場合は必須。
            using( var multithread = this.D3DDevice.QueryInterfaceOrNull<SharpDX.Direct3D.DeviceMultithread>() )
            {
                if( null != multithread )
                    multithread.SetMultithreadProtected( true );
            }

            using( var dxgiDevice = this.D3DDevice.QueryInterfaceOrNull<SharpDX.DXGI.Device>() )
            using( var dxgiAdapter = dxgiDevice.Adapter )
            using( var dxgiFactory = dxgiAdapter.GetParent<SharpDX.DXGI.Factory>() )
            {
                // IDXGISwapChain の作成
                this.DXGISwapChain = new SharpDX.DXGI.SwapChain( dxgiFactory, this.D3DDevice, new SharpDX.DXGI.SwapChainDescription() {
                    BufferCount = 2,
                    Flags = SharpDX.DXGI.SwapChainFlags.None,
                    IsWindowed = true,
                    OutputHandle = hWnd,
                    ModeDescription = new SharpDX.DXGI.ModeDescription {
                        Width = clientSize.Width,
                        Height = clientSize.Height,
                        Format = SharpDX.DXGI.Format.B8G8R8A8_UNorm,
                        RefreshRate = new SharpDX.DXGI.Rational( 60, 1 ),
                    },
                    SampleDescription = new SharpDX.DXGI.SampleDescription( 1, 0 ),
                    SwapEffect = SharpDX.DXGI.SwapEffect.Discard,
                    Usage = SharpDX.DXGI.Usage.RenderTargetOutput,
                } );

                // IMFDXGIDeviceManager の作成とD3Dデバイスの登録
                this.DXGIDeviceManager = new SharpDX.MediaFoundation.DXGIDeviceManager();
                this.DXGIDeviceManager.ResetDevice( this.D3DDevice );

                // IDXGIOutput の取得
                this.DXGIOutput = dxgiAdapter.Outputs[ 0 ];
            }
        }
        public void Dispose()
        {
            // 再生停止
            if( null != this.MediaSession )
                this.MediaSession.Stop();

            // 表示中なら終了を待つ。
            while( Interlocked.Read( ref this.nPresentNow ) != 0 )
                Thread.Sleep( 10 );

            // MediaFoundation の解放 (RCW なので Dispose はない）
            this.MediaSinkAttributes = null;
            this.MediaSink = null;
            this.MediaSource = null;
            this.MediaSession = null;

            // DXGI, D3D11 の解放（RCW ではないので Dispose する）
            this.DXGIOutput?.Dispose();
            this.DXGIDeviceManager?.Dispose();
            this.DXGISwapChain?.Dispose();
            this.D3DDevice?.Dispose();

            // MediaFoundation のシャットダウン
            SharpDX.MediaFoundation.MediaManager.Shutdown();
        }
        public HResult Play( string filePath )
        {
            HResult hr;

            this.evReadyOrFailed = new ManualResetEvent( false );

            // MediaSession を作成する。
            if( ( hr = this.CreateMediaSession( out this.MediaSession ) ).Failed() )
                return hr;

            // ファイルから MediaSource を作成する。
            if( ( hr = this.CreateMediaSourceFromFile( filePath, out this.MediaSource ) ).Failed() )
                return hr;

            // 部分トポロジーを作成する。
            if( ( hr = this.CreateTopology( out IMFTopology topology ) ).Failed() )
                return hr;

            // 部分トポロジーを MediaSession に登録する。
            this.hrStatus = HResult.S_OK;
            if( ( hr = this.MediaSession.SetTopology( MFSessionSetTopologyFlags.None, topology ) ).Failed() )
                return hr;

            // MediaSession が完全トポロジーの作成に成功（または失敗）したら MESessionTopologyStatus イベントが発出されるので、それまで待つ。
            this.evReadyOrFailed.WaitOne( 5000 );

            if( this.hrStatus.Failed() )
                return hrStatus;    // 作成失敗

            // MediaSession の再生を開始。
            if( ( hr = this.MediaSession.Start( Guid.Empty, new PropVariant() ) ).Failed() )
                return hr;

            return HResult.S_OK;
        }
        public void UpdateAndPresent()
        {
            if( Interlocked.Read( ref this.nPresentNow ) == 0 )
            {
                // 描画処理
                this.Draw();

                // SwapChain 表示
                Interlocked.Increment( ref this.nPresentNow );        // 1: 表示開始
                Task.Run( () => {
                    this.DXGIOutput.WaitForVerticalBlank();
                    this.DXGISwapChain.Present( 1, SharpDX.DXGI.PresentFlags.None );
                    Interlocked.Decrement( ref this.nPresentNow );    // 0: 表示完了
                } );
            }
            else
            {
                // 進行処理
                this.Update();
            }
        }
        protected void Update()
        {
            // todo: 描画以外に行いたい処理
        }
        protected void Draw()
        {
            HResult hr;
            IMFSample mfsample = null;

            try
            {
                // 現在表示すべき IMFSample が TextureMediaSink の TMS_SAMPLE 属性に設定されているので、それを取得する。
                if( ( hr = this.MediaSinkAttributes.GetUnknown( TMS_SAMPLE, out mfsample ) ).Failed() )
                    return;
                if( null == mfsample )
                    return; // 未設定

                var sample = new SharpDX.MediaFoundation.Sample( Marshal.GetIUnknownForObject( mfsample ) );    // MediaFoundation.NET to SharpDX.MediaFoundation

                // 注意:
                // 変数 mfsample は RCW（Runtime Callable Wrapper) オブジェクトであり、内部に COM (ネイティブIMFSample) への参照を保持している。
                // このあと mfsample への参照が 0 になっても、内部の COM は mfsample が GC によりファイナライズされるまで Release されないことに注意。（RCW の仕様）
                // D3D11TextureMediaSink.dll では、mfsample の内包する COM の実体がこのプログラム（とdll）が終了するまで不変であるよう設計されているため、直接の問題はない。
                // （RCW による同一の COM への AddRef は1回しか行われないので、mfsample に何度値を設定しても、内部の COM 参照カウンタは初回以降増加しない。）

                // IMFSample からメディアバッファを取得。
                using( var mediaBuffer = sample.GetBufferByIndex( 0 ) )
                {
                    // メディアバッファからDXGIバッファを取得。
                    using( var dxgiBuffer = mediaBuffer.QueryInterfaceOrNull<SharpDX.MediaFoundation.DXGIBuffer>() )
                    {
                        // DXGIバッファから転送元 Texture2D を取得。
                        dxgiBuffer.GetResource( typeof( SharpDX.Direct3D11.Texture2D ).GUID, out IntPtr ppv );
                        using( var texture = new SharpDX.Direct3D11.Texture2D( ppv ) )
                        {
                            // 転送元 Texture2D のサブリソースインデックスを取得。
                            int subIndex = dxgiBuffer.SubresourceIndex;

                            //
                            // これで、IMFSample から ID3D11Texture2D を取得することができた。
                            // 今回のデモでは、これを単純に SwapChain に矩形描画することにする。
                            //

                            // SwapChain から転送先 Texture2D を取得。
                            using( var dst = this.DXGISwapChain.GetBackBuffer<SharpDX.Direct3D11.Texture2D>( 0 ) )
                            {
                                // 転送。
                                this.D3DDevice.ImmediateContext.CopySubresourceRegion( texture, subIndex, null, dst, 0 );
                            }
                        }
                    }
                }
            }
            finally
            {
                // GetUnknown で TMS_SAMPLE を得たとき、その IMFSample は TextureMediaSink から更新されないよう lock されている。
                // この lock を解除するために、TMS_SAMPLE 属性に何か（なんでもいい）を SetUnknwon する。これで IMFSample が TextureMediaSink から更新可能になる。
                this.MediaSinkAttributes.SetUnknown( TMS_SAMPLE, mfsample );
            }
        }

        // IMFAsyncCallback 実装
        public HResult GetParameters( out MFASync pdwFlags, out MFAsyncCallbackQueue pdwQueue )
        {
            // このメソッドの実装はオプション。今回は実装しない。
            pdwFlags = MFASync.None;
            pdwQueue = MFAsyncCallbackQueue.Undefined;
            return HResult.E_NOTIMPL;
        }
        public HResult Invoke( IMFAsyncResult pAsyncResult )
        {
            HResult hr;

            if( ( hr = pAsyncResult.GetState( out object ppvState ) ).Succeeded() )
            {
                // (A) State がある（E_POINTERエラーじゃない）場合

                if( this.ID_RegistarTopologyWorkQueueWithMMCSS == ppvState )
                {
                    this.OnEndRegistarTopologyWorkQueueWithMMCSS( pAsyncResult );
                    return HResult.S_OK;
                }
                else
                {
                    return HResult.E_INVALIDARG;
                }
            }
            else
            {
                // (B) State がない場合
                
                // MediaSession からのイベントを受信する。
                if( ( hr = this.MediaSession.EndGetEvent( pAsyncResult, out IMFMediaEvent mediaEvent ) ).Failed() ) // イベント
                {
                    this.hrStatus = hr;
                    return hr;
                }
                if( ( hr = mediaEvent.GetType( out MediaEventType mediaEventType ) ).Failed() ) // イベント種別
                {
                    this.hrStatus = hr;
                    return hr;
                }
                if( ( hr = mediaEvent.GetStatus( out this.hrStatus ) ).Failed() )   // イベントステータス → this.hrStatus に格納して、メインスレッドから結果を確認できるようにする。
                    return hr;

                try
                {
                    // 結果が失敗なら終了。
                    if( this.hrStatus.Failed() )
                    {
                        this.evReadyOrFailed.Set(); // 成功でも失敗でもイベントは発火する。失敗の場合、this.hrStatus は Failed になる。
                        return hrStatus;
                    }

                    // 成功の場合、イベント種別で分岐。
                    switch( mediaEventType )
                    {
                        case MediaEventType.MESessionTopologyStatus:

                            // トポロジーの Status 取得。
                            if( ( hr = mediaEvent.GetUINT32( MFAttributesClsid.MF_EVENT_TOPOLOGY_STATUS, out int topoStatus ) ).Failed() )
                            {
                                this.hrStatus = hr; // 失敗
                                return hr;
                            }

                            var mfTopoStatus = (MFTopoStatus) topoStatus;

                            switch( mfTopoStatus )
                            {
                                case MFTopoStatus.Ready:
                                    this.OnTopologyReady( mediaEvent );
                                    break;
                            }
                            break;

                        case MediaEventType.MESessionStarted:
                            this.OnSessionStarted( mediaEvent );
                            break;

                        case MediaEventType.MESessionPaused:
                            this.OnSessionPaused( mediaEvent );
                            break;

                        case MediaEventType.MESessionStopped:
                            this.OnSessionStopped( mediaEvent );
                            break;

                        case MediaEventType.MESessionClosed:
                            this.OnSessionClosed( mediaEvent );
                            break;

                        case MediaEventType.MEEndOfPresentation:
                            this.OnPresentationEnded( mediaEvent );
                            break;
                    }
                }
                finally
                {
                    // 次の MediaSession イベントの受信を待つ。
                    if( mediaEventType != MediaEventType.MESessionClosed )  // SessionClosed イベントの後なら待たない。
                        this.MediaSession.BeginGetEvent( this, null );
                }

                return HResult.S_OK;
            }
        }


        private long nPresentNow;
        private ManualResetEvent evReadyOrFailed;

        private SharpDX.Direct3D11.Device D3DDevice;
        private SharpDX.DXGI.SwapChain DXGISwapChain;
        private SharpDX.DXGI.Output DXGIOutput;
        private SharpDX.MediaFoundation.DXGIDeviceManager DXGIDeviceManager;
        private IMFMediaSession MediaSession;
        private IMFMediaSource MediaSource;
        private IMFMediaSink MediaSink;
        private IMFAttributes MediaSinkAttributes;
        private HResult hrStatus;

        private HResult CreateMediaSession( out IMFMediaSession mediaSession )
        {
            HResult hr;

            // MediaSession を生成する。
            if( ( hr = MFExtern.MFCreateMediaSession( null, out mediaSession ) ).Failed() )
                return hr;

            // MediaSession からのイベント送信を開始する。
            if( ( hr = mediaSession.BeginGetEvent( this, null ) ).Failed() )    // 受信者は自分
                return hr;

            return HResult.S_OK;
        }
        private HResult CreateMediaSourceFromFile( string filePath, out IMFMediaSource mediaSource )
        {
            HResult hr;
            mediaSource = null;

            // ソースリゾルバを作成する。
            if( ( hr = MFExtern.MFCreateSourceResolver( out IMFSourceResolver sourceResolver ) ).Failed() )
                return hr;

            // ファイルパスからメディアソースを作成する。
            if( ( hr = sourceResolver.CreateObjectFromURL( filePath, MFResolution.MediaSource, null, out mediaSource ) ).Failed() )
                return hr;

            return HResult.S_OK;
        }
        private HResult CreateTopology( out IMFTopology topology )
        {
            HResult hr;

            // 新しいトポロジーを作成する。
            if( ( hr = MFExtern.MFCreateTopology( out topology ) ).Failed() )
                return hr;

            // メディアソース用のプレゼン識別子を作成する。
            if( ( hr = this.MediaSource.CreatePresentationDescriptor( out IMFPresentationDescriptor presentationDesc ) ).Failed() )
                return hr;

            // プレゼン識別子から、メディアソースのストリームの数を取得する。
            if( ( hr = presentationDesc.GetStreamDescriptorCount( out int dwDescriptionCount ) ).Failed() )
                return hr;

            // メディアソースの各ストリームについて、トポロジーノードを作成してトポロジーに追加する。
            for( int i = 0; i < dwDescriptionCount; i++ )
            {
                if( ( hr = this.AddTopologyNodes( topology, presentationDesc, i ) ).Failed() )
                    return hr;
            }

            return HResult.S_OK;
        }
        private HResult AddTopologyNodes( IMFTopology topology, IMFPresentationDescriptor presentationDesc, int index )
        {
            HResult hr;

            // 指定されたストリーム番号のストリーム記述子を取得する。
            if( ( hr = presentationDesc.GetStreamDescriptorByIndex( index, out bool bSelected, out IMFStreamDescriptor streamDesc ) ).Failed() )
                return hr;

            if( bSelected )
            {
                // (A) ストリームが選択されているなら、トポロジーに追加する。

                if( ( hr = this.CreateSourceNode( this.MediaSource, presentationDesc, streamDesc, out IMFTopologyNode sourceNode ) ).Failed() )
                    return hr;

                if( ( hr = this.CreateOutputNode( streamDesc, out IMFTopologyNode outputNode, out Guid majorType ) ).Failed() )
                    return hr;

                if( majorType == MFMediaType.Audio )
                {
                    sourceNode.SetString( MFAttributesClsid.MF_TOPONODE_WORKQUEUE_MMCSS_CLASS, "Audio" );
                    sourceNode.SetUINT32( MFAttributesClsid.MF_TOPONODE_WORKQUEUE_ID, 1 );
                }
                if( majorType == MFMediaType.Video )
                {
                    sourceNode.SetString( MFAttributesClsid.MF_TOPONODE_WORKQUEUE_MMCSS_CLASS, "Playback" );
                    sourceNode.SetUINT32( MFAttributesClsid.MF_TOPONODE_WORKQUEUE_ID, 2 );
                }

                if( null != sourceNode && null != outputNode )
                {
                    topology.AddNode( sourceNode );
                    topology.AddNode( outputNode );

                    sourceNode.ConnectOutput( 0, outputNode, 0 );
                }
            }
            else
            {
                // (B) ストリームが選択されていないなら、何もしない。
            }

            return HResult.S_OK;
        }
        private HResult CreateSourceNode( IMFMediaSource mediaSource, IMFPresentationDescriptor presentationDesc, IMFStreamDescriptor streamDesc, out IMFTopologyNode sourceNode )
        {
            HResult hr;

            // ソースノードを作成する。
            if( ( hr = MFExtern.MFCreateTopologyNode( MFTopologyType.SourcestreamNode, out sourceNode ) ).Failed() )
                return hr;

            // ソースノードに３属性を設定する。
            if( ( hr = sourceNode.SetUnknown( MFAttributesClsid.MF_TOPONODE_SOURCE, mediaSource ) ).Failed() )
                return hr;
            if( ( hr = sourceNode.SetUnknown( MFAttributesClsid.MF_TOPONODE_PRESENTATION_DESCRIPTOR, presentationDesc ) ).Failed() )
                return hr;
            if( ( hr = sourceNode.SetUnknown( MFAttributesClsid.MF_TOPONODE_STREAM_DESCRIPTOR, streamDesc ) ).Failed() )
                return hr;

            return HResult.S_OK;
        }
        private HResult CreateOutputNode( IMFStreamDescriptor streamDesc, out IMFTopologyNode outputNode, out Guid guidMajorType )
        {
            HResult hr;
            outputNode = null;
            guidMajorType = Guid.Empty;

            // 出力ノードを作成。
            if( ( hr = MFExtern.MFCreateTopologyNode( MFTopologyType.OutputNode, out outputNode ) ).Failed() )
                return hr;

            // メディアハンドラを取得。
            if( ( hr = streamDesc.GetMediaTypeHandler( out IMFMediaTypeHandler mediaTypeHandler ) ).Failed() )
                return hr;

            // メジャータイプを取得。
            if( ( hr = mediaTypeHandler.GetMajorType( out guidMajorType ) ).Failed() )
                return hr;

            if( guidMajorType == MFMediaType.Video )
            {
                // (A) ビデオレンダラ

                // TextureMediaSink が未生成なら生成する。
                if( null == this.MediaSink )
                {
                    CreateD3D11TextureMediaSink(
                        typeof( IMFMediaSink ).GUID,
                        out IntPtr ppv,
                        this.DXGIDeviceManager.NativePointer,
                        this.D3DDevice.NativePointer );

                    this.MediaSink = (IMFMediaSink) Marshal.GetObjectForIUnknown( ppv );
                    this.MediaSinkAttributes = (IMFAttributes) this.MediaSink;
                }

                // ストリームシンク #0（固定）を取得する。
                if( ( hr = this.MediaSink.GetStreamSinkById( 0, out IMFStreamSink streamSink ) ).Failed() )
                    return hr;

                // 出力ノードにストリームシンクを割り当てる。
                if( ( hr = outputNode.SetObject( streamSink ) ).Failed() )
                    return hr;

                #region " 参考 "
                //----------------
                // IMFActivate でカスタムMediaSinkを使うパターン:
                //var activate = (IMFActivate) new カスタムMediaSink.MediaSinkActivate();
                //if( ( hr = 出力ノード.SetObject( activate ) ).Failed() ||
                //    ( hr = 出力ノード.SetUINT32( MFAttributesClsid.MF_TOPONODE_STREAMID, 0 ) ).Failed() ||
                //    ( hr = 出力ノード.SetUINT32( MFAttributesClsid.MF_TOPONODE_NOSHUTDOWN_ON_REMOVE, 0 ) ).Failed() )
                //    throw Marshal.GetExceptionForHR( (int) hr );

                // 既定のEVRを使うパターン:
                //if( ( hr = MFExtern.MFCreateVideoRendererActivate( this._hWnd, out IMFActivate activate ) ).Failed() )
                //    throw Marshal.GetExceptionForHR( (int) hr );
                //if( ( hr = 出力ノード.SetObject( activate ) ).Failed() ||
                //    ( hr = 出力ノード.SetUINT32( MFAttributesClsid.MF_TOPONODE_STREAMID, 0 ) ).Failed() ||
                //    ( hr = 出力ノード.SetUINT32( MFAttributesClsid.MF_TOPONODE_NOSHUTDOWN_ON_REMOVE, 0 ) ).Failed() )
                //    throw Marshal.GetExceptionForHR( (int) hr );
                //----------------
                #endregion
            }
            else if( guidMajorType == MFMediaType.Audio )
            {
                // (B) オーディオレンダラ

                // 既定のオーディオレンダラを生成する。
                if( ( hr = MFExtern.MFCreateAudioRendererActivate( out IMFActivate activate ) ).Failed() )
                    return hr;

                // 出力ノードにオーディオレンダラを割り当てる。
                if( ( hr = outputNode.SetObject( activate ) ).Failed() )
                    return hr;
            }

            return HResult.S_OK;
        }

        protected virtual void OnTopologyReady( IMFMediaEvent mediaEvent )
        {
            // トポロジーワークキューの MMCSS へのクラス割り当て（非同期処理）を開始する。

            ( (IMFGetService) this.MediaSession ).GetService( MFServices.MF_WORKQUEUE_SERVICES, typeof( IMFWorkQueueServices ).GUID, out object ppvObject );
            var workQueueService = (IMFWorkQueueServices) ppvObject;
            workQueueService.BeginRegisterTopologyWorkQueuesWithMMCSS( this, this.ID_RegistarTopologyWorkQueueWithMMCSS );
        }
        protected virtual void OnSessionStarted( IMFMediaEvent mediaEvent )
        {
        }
        protected virtual void OnSessionPaused( IMFMediaEvent mediaEvent )
        {
        }
        protected virtual void OnSessionStopped( IMFMediaEvent mediaEvent )
        {
        }
        protected virtual void OnSessionClosed( IMFMediaEvent mediaEvent )
        {
        }
        protected virtual void OnPresentationEnded( IMFMediaEvent mediaEvent )
        {
        }
        protected virtual void OnEndRegistarTopologyWorkQueueWithMMCSS( IMFAsyncResult ar )
        {
            // トポロジーワークキューの MMCSS へのクラス割り当て（非同期処理）を完了する。

            HResult hr;

            ( (IMFGetService) this.MediaSession ).GetService( MFServices.MF_WORKQUEUE_SERVICES, typeof( IMFWorkQueueServices ).GUID, out object ppvObject );
            var workQueueServices = (IMFWorkQueueServices) ppvObject;
            if( ( hr = workQueueServices.EndRegisterTopologyWorkQueuesWithMMCSS( ar ) ).Failed() )
                this.hrStatus = hr;
            else
                this.hrStatus = HResult.S_OK;   // 成功ステータスをもって

            this.evReadyOrFailed.Set();     // イベント発火。
        }


        // D3D11TextureMediaSink.dll は、D3D11TextureMediaSinkDemoCSharpプロジェクトの bin/Debug, bin/Release に xcopy するよう、
        // dllプロジェクトのビルド後イベントで設定してある。
        [DllImport( "D3D11TextureMediaSink" )]
        private extern static void CreateD3D11TextureMediaSink(
            [MarshalAs( UnmanagedType.LPStruct ), In] Guid riid,
            out IntPtr ppvObject,
            IntPtr pDXGIDeviceManager,
            IntPtr pD3D11Device );

        private readonly Guid TMS_SAMPLE = new Guid( "{87468A84-1CD6-41E3-9522-A8625EB3A5F8}" );    // D3D11TextureMediaSink.h にある定義と同じ
        private readonly object ID_RegistarTopologyWorkQueueWithMMCSS = new object();
    }
}
