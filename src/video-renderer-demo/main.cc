#include "main_window.h"
#include "d3d9_screen_capture.h"
#include "d3d11_screen_capture.h"
#include "d3d9_renderer.h"
#include "d3d11_renderer.h"

#include "libyuv/libyuv.h"

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3d9.lib")


#include "av_demuxer.h"
#include "d3d11va_decoder.h"

#pragma comment(lib, "avformat.lib")
#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "avutil.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

static void GetWindowSize(HWND hwnd, int& width, int& height)
{
    RECT rect;
    GetWindowRect(hwnd, &rect);
    width = static_cast<int>(rect.right - rect.left);
    height = static_cast<int>(rect.bottom - rect.top);
}

static void RenderARGB(DX::Renderer* renderer, DX::Image argb_image)
{
    DX::PixelFrame pixel_frame;

    pixel_frame.width = argb_image.width;
    pixel_frame.height = argb_image.height;
    pixel_frame.format = DX::PIXEL_FORMAT_ARGB;
    pixel_frame.pitch[0] = argb_image.width * 4;
    pixel_frame.plane[0] = &argb_image.bgra[0];

    renderer->Render(&pixel_frame);
}

static void RenderI444(DX::Renderer* renderer, DX::Image argb_image)
{
    DX::PixelFrame pixel_frame;

    int dst_stride_y = argb_image.width;
    int dst_stride_u = argb_image.width;
    int dst_stride_v = argb_image.width;
    std::unique_ptr<uint8_t> dst_y(new uint8_t[dst_stride_y * argb_image.height]);
    std::unique_ptr<uint8_t> dst_u(new uint8_t[dst_stride_u * argb_image.height]);
    std::unique_ptr<uint8_t> dst_v(new uint8_t[dst_stride_v * argb_image.height]);

    libyuv::ARGBToI444(
        &argb_image.bgra[0],
        argb_image.width * 4,
        dst_y.get(),
        dst_stride_y,
        dst_u.get(),
        dst_stride_u,
        dst_v.get(),
        dst_stride_v,
        argb_image.width,
        argb_image.height
    );

    pixel_frame.width = argb_image.width;
    pixel_frame.height = argb_image.height;
    pixel_frame.format = DX::PIXEL_FORMAT_I444;
    pixel_frame.pitch[0] = dst_stride_y;
    pixel_frame.pitch[1] = dst_stride_u;
    pixel_frame.pitch[2] = dst_stride_v;
    pixel_frame.plane[0] = dst_y.get();
    pixel_frame.plane[1] = dst_u.get();
    pixel_frame.plane[2] = dst_v.get();

    renderer->Render(&pixel_frame);
}

static void RenderI420(DX::Renderer* renderer, DX::Image argb_image)
{
    DX::PixelFrame pixel_frame;

    int dst_stride_y = argb_image.width;
    int dst_stride_u = (argb_image.width + 1) / 2;
    int dst_stride_v = (argb_image.width + 1) / 2;
    std::unique_ptr<uint8_t> dst_y(new uint8_t[dst_stride_y * argb_image.height]);
    std::unique_ptr<uint8_t> dst_u(new uint8_t[dst_stride_u * argb_image.height / 2]);
    std::unique_ptr<uint8_t> dst_v(new uint8_t[dst_stride_v * argb_image.height / 2]);

    libyuv::ARGBToI420(
        &argb_image.bgra[0],
        argb_image.width * 4,
        dst_y.get(),
        dst_stride_y,
        dst_u.get(),
        dst_stride_u,
        dst_v.get(),
        dst_stride_v,
        argb_image.width,
        argb_image.height
    );

    pixel_frame.width = argb_image.width;
    pixel_frame.height = argb_image.height;
    pixel_frame.format = DX::PIXEL_FORMAT_I420;
    pixel_frame.pitch[0] = dst_stride_y;
    pixel_frame.pitch[1] = dst_stride_u;
    pixel_frame.pitch[2] = dst_stride_v;
    pixel_frame.plane[0] = dst_y.get();
    pixel_frame.plane[1] = dst_u.get();
    pixel_frame.plane[2] = dst_v.get();

    renderer->Render(&pixel_frame);
}

static void RenderNV12(DX::Renderer* renderer, DX::Image argb_image)
{
    DX::PixelFrame pixel_frame;

    int dst_stride_y = argb_image.width;
    int dst_stride_uv = argb_image.width;

    std::unique_ptr<uint8_t> dst_y(new uint8_t[dst_stride_y * argb_image.height]);
    std::unique_ptr<uint8_t> dst_uv(new uint8_t[dst_stride_uv * argb_image.height / 2]);

    libyuv::ARGBToNV12(
        &argb_image.bgra[0],
        argb_image.width * 4,
        dst_y.get(),
        dst_stride_y,
        dst_uv.get(),
        dst_stride_uv,
        argb_image.width,
        argb_image.height
    );

    pixel_frame.width = argb_image.width;
    pixel_frame.height = argb_image.height;
    pixel_frame.format = DX::PIXEL_FORMAT_NV12;
    pixel_frame.pitch[0] = dst_stride_y;
    pixel_frame.pitch[1] = dst_stride_uv;
    pixel_frame.plane[0] = dst_y.get();
    pixel_frame.plane[1] = dst_uv.get();

    renderer->Render(&pixel_frame);
}

int main(int argc, char** argv)
{
    MainWindow window;
    if (!window.Init(100, 100, 1920 * 4 / 5, 1080 * 4 / 5)) {
        return -1;
    }

    DX::D3D11ScreenCapture screen_capture;
    if (!screen_capture.Init()) {
        return -2;
    }

    DX::D3D11Renderer renderer;
    if (!renderer.Init(window.GetHandle())) {
        return -3;
    }

    //renderer.SetSharpen(0.5);

    int original_width = 0, original_height = 0;
    GetWindowSize(window.GetHandle(), original_width, original_height);

    MSG msg;
    ZeroMemory(&msg, sizeof(msg));


    AVDemuxer demuxer;
    AVDecoder decoder;
    AVStream* video_stream = nullptr;


    bool abort_request = false;
    std::string pathname = "piper.h264";
    pathname = "F:/FFOutput/input.mp4";

    if (!demuxer.Open(pathname)) {
        abort_request = true;
    }

    video_stream = demuxer.GetVideoStream();

    if (!decoder.Init(video_stream, renderer.GetD3D11Device(), false)) {
        abort_request = true;
    }

    AVPacket av_packet1, * av_packet = &av_packet1;
    AVFrame* av_frame = av_frame_alloc();


    while (msg.message != WM_QUIT)
    {
        if (::PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            continue;
        }

        int current_width = 0, current_height = 0;
        GetWindowSize(window.GetHandle(), current_width, current_height);
        if (current_width != original_width || current_height != original_height) {
            original_width = current_width;
            original_height = current_height;
            renderer.Resize();
        }

        if (1)
        {
            int ret = demuxer.Read(av_packet);
            if (ret < 0) {
                continue;
            }

            if (av_packet->stream_index == video_stream->index)
            {
                ret = decoder.Send(&av_packet1);
                while (ret >= 0)
                {
                    ret = decoder.Recv(av_frame);
                    if (ret >= 0)
                    {
                        DX::PixelFrame frame;
                        frame.format = DX::PIXEL_FORMAT_NV12;
                        frame.width = demuxer.GetVideoStream()->codecpar->width;
                        frame.height = demuxer.GetVideoStream()->codecpar->height;

                        int dst_stride_y = frame.width;
                        int dst_stride_uv = frame.width;
                        int height = frame.height;

                        //std::unique_ptr<uint8_t> dst_y(new uint8_t[dst_stride_y * height]);
                        std::unique_ptr<uint8_t> dst_uv(new uint8_t[dst_stride_uv * height / 2]);

                        frame.pitch[0] = av_frame->linesize[0];
                        frame.pitch[1] = av_frame->linesize[1] * 2;

                        int s = dst_stride_uv * height / 2;

                        uint8_t* buffer = (uint8_t*)dst_uv.get();
                        for (size_t i = 0, a = 0, b = 0; i < s; i++) {
                            if (i % 2 == 0) {
                                buffer[i] = av_frame->data[1][a++];
                            }
                            else {
                                buffer[i] = av_frame->data[2][b++];
                            }
                        }

                        frame.plane[0] = av_frame->data[0];
                        frame.plane[1] = buffer;

                        renderer.Render(&frame);
                    }
                }
                Sleep(25);
            }
            av_packet_unref(av_packet);

            if (demuxer.IsEOF()) {
                demuxer.Close();
                demuxer.Open(pathname);
            }
        }
        else
        {
            DX::PixelFormat render_format = DX::PIXEL_FORMAT_NV12;


            DX::Image argb_image;
            if (screen_capture.Capture(argb_image))
            {
                if (render_format == DX::PIXEL_FORMAT_ARGB) {
                    RenderARGB(&renderer, argb_image);
                }
                else if (render_format == DX::PIXEL_FORMAT_I444) {
                    RenderI444(&renderer, argb_image);
                }
                else if (render_format == DX::PIXEL_FORMAT_I420) {
                    RenderI420(&renderer, argb_image);
                }
                else if (render_format == DX::PIXEL_FORMAT_NV12) {
                    RenderNV12(&renderer, argb_image);
                }
            }
        }

    }

    return 0;
}
