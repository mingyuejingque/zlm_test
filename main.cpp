#include <iostream>
#include <thread>
#include <chrono>

extern "C" {
#include <mk_mediakit.h>
#include <libavformat/avformat.h>
#include <libavcodec/bsf.h>
}

#define ENABLE_AUDIO 1
#define ENABLE_VIDEO 1

typedef struct ZLM_CTX_s {
    mk_media zlm;
    AVFormatContext *ifmt_ctx;
    AVBSFContext *bsf;
    size_t   frame_count;
    int idxv;  //video stream index
    int idxa;  //audio stream index
    const char *stream_name;
}zlm_ctx_t;

static zlm_ctx_t g_ctx;

static void start_local_servers();
static void start_send_rtp();
static void dz_on_mk_media_source_send_rtp_result(void *user_data, uint16_t local_port, int err, const char *msg);
static int  init_ffmpeg(const char* file_name);
static int  read_frame(AVPacket *packet);
static int  filter_frame(AVPacket* packet);

int main(int argc, char *argv[] ) {
    using namespace std::chrono_literals;
    auto sleep_duration = 40ms;
    const char* input_file = argv[1];
    const char* stream_name = argv[2];
    if (argc > 3) {
        sleep_duration = 1ms * atoi(argv[3]);
    }
    mk_media ctx = nullptr;
    g_ctx.stream_name = stream_name;

    do {

        if (!init_ffmpeg(input_file)) {
            break;
        }

        start_local_servers(); //如果本地的zlm1也要播放各种协议

        ctx = mk_media_create("__defaultVhost__", "live", "zlm_test", 0, 0, 0);
        if (!ctx)
            break;

#if (ENABLE_VIDEO)        
        auto res = mk_media_init_video(ctx, 0, 800, 600, 20, 0);
        if (!res)
            break;
#endif
#if (ENABLE_AUDIO)
        res = mk_media_init_audio(ctx, 3, 8000, 1, 16);
        if (!res)
            break;
#endif

        g_ctx.zlm = ctx;
        start_send_rtp();

        int r = 0; 
        AVPacket packet;
        do {
           r = read_frame(&packet);
           if (r < 0)
               break;
#if ENABLE_VIDEO
            if (packet.stream_index == g_ctx.idxv) {
                mk_media_input_h264(ctx, packet.data, packet.size, 0, 0);
            }
#endif
#if ENABLE_AUDIO
            if (packet.stream_index == g_ctx.idxa) {
                mk_media_input_audio(ctx, packet.data, packet.size, 0);
            }
#endif
           std::this_thread::sleep_for(sleep_duration);
           av_packet_unref(&packet);

        } while(r >= 0);

    } while(0);

    std::cout << "has send " << g_ctx.frame_count << " frames." << std::endl;
    if (ctx) {
        mk_media_stop_send_rtp(ctx, stream_name);
        mk_media_release(ctx);
        ctx = nullptr;
    }

    if (g_ctx.bsf) {
        av_bsf_free(&g_ctx.bsf);
    }
    if (g_ctx.ifmt_ctx) {
        avformat_close_input(&g_ctx.ifmt_ctx);
    }
}


void dz_on_mk_media_source_send_rtp_result(void *user_data, uint16_t local_port, int err, const char *msg) {
    std::cout << "msg: " << msg << std::endl;
}

//return 1 success, 0 false
int  init_ffmpeg(const char* file_name) {
    if (avformat_open_input(&g_ctx.ifmt_ctx, file_name, 0, 0) < 0) {
        std::cerr << "open file faild." <<  file_name << std::endl;
        return 0;
    }

    if (avformat_find_stream_info(g_ctx.ifmt_ctx, 0) < 0) {
        std::cerr << "find stream info err." << std::endl;
        return 0;
    }

    g_ctx.idxv = av_find_best_stream(g_ctx.ifmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    g_ctx.idxa = av_find_best_stream(g_ctx.ifmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    //av_dump_format(g_ctx.ifmt_ctx, 0, file_name, 0);
    std::cout << "video index: " << g_ctx.idxv << ", audio index: " << g_ctx.idxa << std::endl;

    if (g_ctx.idxv >= 0) {
        const AVBitStreamFilter *filter = av_bsf_get_by_name("h264_mp4toannexb");
        auto ret = av_bsf_alloc(filter, &g_ctx.bsf);
        if (ret < 0) {
            std::cerr << "av_bsf_alloc faild." << std::endl;
            return 0;
        }

        AVStream *st = g_ctx.ifmt_ctx->streams[g_ctx.idxv];
        ret = avcodec_parameters_copy(g_ctx.bsf->par_in, st->codecpar);
        if (ret < 0) {
            std::cerr << "avcodec_paramters_copy faild." << std::endl;
            return 0;
        }

        ret = av_bsf_init(g_ctx.bsf);
        if (ret < 0) {
            std::cerr << "av_bsf_init faild." << std::endl;
            return 0;
        }

        ret = avcodec_parameters_copy(st->codecpar, g_ctx.bsf->par_out);
        if (ret < 0) {
            std::cerr << "avcodec_parameters_copy 222 faild." << std::endl;
            return 0;
        }
    }
    return 1;
}

int  read_frame(AVPacket *packet) {
    auto ret = 0;
    do {
        ret = av_read_frame(g_ctx.ifmt_ctx, packet);
#if (ENABLE_AUDIO ==0 )
        //不启用音频，却读到了音频帧 就扔掉它
        if (packet->stream_index == g_ctx.idxa) {
            av_packet_unref(packet);
            continue; //扔掉x帧
        }
#endif

#if (ENABLE_VIDEO == 0)        
        if (packet->stream_index == g_ctx.idxv) {
            av_packet_unref(packet);
            continue; //扔掉x帧
        }
#endif

        if (packet->stream_index == g_ctx.idxv) {
            filter_frame(packet);
        }
        std::cout << "av_read_frame: " << ret
            << ", type: " << (packet->stream_index == g_ctx.idxv ? "v" : "a")
            << ", packet size: " << packet->size << std::endl;
        g_ctx.frame_count++;
        break;
    } while(ret >= 0);
    return ret;
}

int  filter_frame(AVPacket* packet) {
    if (packet->stream_index != g_ctx.idxv)
        return 0;

    int ret = 0;
    ret = av_bsf_send_packet(g_ctx.bsf, packet);
    if (ret < 0) {
        std::cerr << "av_bsf_send_packet error." << std::endl;
        return 0;
    }
    while(!ret) {
        ret = av_bsf_receive_packet(g_ctx.bsf, packet);
    }
    if (ret < 0 && (ret != AVERROR(EAGAIN))) {
        std::cerr << "faild to receive output packet" << std::endl;
        return 0;
    }
    return 1;
}

void start_send_rtp() {
        mk_media_init_complete(g_ctx.zlm);
        mk_media_start_send_rtp(g_ctx.zlm, "127.0.0.1", 30443, g_ctx.stream_name, true, dz_on_mk_media_source_send_rtp_result, nullptr);
}

void start_local_servers() {

    //根据需要自己启动一些服务
    //mk_http_server_start(80, false);
    mk_rtsp_server_start(554, false);
    mk_rtmp_server_start(1935, false);
    //mk_rtp_server_start(10000);
    //mk_rtc_server_start(8000);
}