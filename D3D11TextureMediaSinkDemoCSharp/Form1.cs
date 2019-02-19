using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;
using MediaFoundation;

namespace D3D11TextureMediaSinkDemoCSharp
{
    public partial class Form1 : SharpDX.Windows.RenderForm
    {
        public Form1()
        {
            InitializeComponent();
            this.ClientSize = new Size( 1280, 720 );
        }
        protected override void OnLoad( EventArgs e )
        {
            this.Activate();
        }
        protected override void OnClosed( EventArgs e )
        {
            this.DemoPlay?.Dispose();
            this.DemoPlay = null;
        }
        public void Run()
        {
            // ファイルを選択する。
            var ofd = new OpenFileDialog() {
                Title = "Select a movie file",
                Filter = "Movie(*.mp4;*.avi;*.wmv)|*.mp4;*.avi;*.wmv|All(*.*)|*.*",
            };
            if( ofd.ShowDialog() != DialogResult.OK )
                return;

            // 動画再生クラスを生成し、再生を開始。
            this.DemoPlay = new DemoPlay( this.Handle, this.ClientSize );

            if( this.DemoPlay.Play( ofd.FileName ).Failed() )
            {
                MessageBox.Show( "MediaSession build error!", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error );
                return;
            }

            // メッセージループ。
            SharpDX.Windows.RenderLoop.Run( this, () => {

                // 動画再生クラスを呼び出す。
                this.DemoPlay.UpdateAndPresent();

            } );
        }

        private DemoPlay DemoPlay;
    }
}
