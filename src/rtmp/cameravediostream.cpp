/*
    �������������
*/

extern "C" {
#include "libavformat/avformat.h"
#include "libavutil/time.h"
}

#include "common.h"

#pragma comment(lib, "avformat.lib")
#pragma comment(lib, "avutil.lib")
#pragma comment(lib, "avcodec.lib")

#include <iostream>

using std::cout;
using std::endl;
using std::flush;

void test_vedio_stream()
{
    char *in_url = "rtsp://username:passwd@192.168.26.31";
    char *out_url = "rtmp://192.168.26.31/live";

    av_register_all();

    // ��ʼ�������
    avformat_network_init();

    // ���������ļ������װ
    AVFormatContext *ictx = NULL;

    // ����rtspЭ����ʱ���ֵ
    AVDictionary *opts = NULL;
    char key[] = "max_delay";
    char val[] = "500";
    av_dict_set(&opts, key, val, 0);

    char key2[] = "rtsp_transport";
    char val2[] = "tcp";
    av_dict_set(&opts, key2, val2, 0);

    int ret_code = avformat_open_input(&ictx, in_url, 0, &opts);
    if (ret_code < 0) { av_error(ret_code); return; }

    cout << "open file " << in_url << " success." << endl;

    // ��ȡ��Ƶ��Ƶ����Ϣ��h.264 flv
    ret_code = avformat_find_stream_info(ictx, 0);
    if (ret_code < 0) { av_error(ret_code); return; }

    av_dump_format(ictx, 0, in_url, 0);

    cout << "//////////////////////////////////////" << endl;
    cout << "//////////////////////////////////////" << endl;


    // �����
    AVFormatContext *octx = NULL;
    ret_code = avformat_alloc_output_context2(&octx, 0, "flv", out_url);
    if (NULL == octx) { av_error(ret_code); return; }

    cout << "octx create successful!" << endl;

    // ���������
    for (int i = 0; i < ictx->nb_streams; ++i) {
        // ���������
        AVStream *out = avformat_new_stream(octx, ictx->streams[i]->codec->codec);
        if (NULL == out) { av_error(0); return; }

        ret_code = avcodec_parameters_copy(out->codecpar, ictx->streams[i]->codecpar);
        out->codec->codec_tag = 0;
    }

    av_dump_format(octx, 0, out_url, 1);


    // RTMP����
    ret_code = avio_open(&octx->pb, out_url, AVIO_FLAG_WRITE);
    if (NULL == octx->pb) { av_error(0); return; }

    // д��ͷ��Ϣ
    ret_code = avformat_write_header(octx, 0);
    if (ret_code < 0) { av_error(ret_code); return; }

    // ������ÿһ֡����
    long long start_time = av_gettime();
    AVPacket pkt;
    for (;;) {
        ret_code = av_read_frame(octx, &pkt); 
        if (0 != ret_code || 0 != pkt.size) { continue; }

        cout << pkt.pts << " " << flush;

        //����ת��pts dts
		AVRational itime = ictx->streams[pkt.stream_index]->time_base;
		AVRational otime = octx->streams[pkt.stream_index]->time_base;
		pkt.pts = av_rescale_q_rnd(pkt.pts, itime, otime, (AVRounding)(AV_ROUND_NEAR_INF |AV_ROUND_NEAR_INF));
		pkt.dts = av_rescale_q_rnd(pkt.pts, itime, otime, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_NEAR_INF));
		pkt.duration = av_rescale_q_rnd(pkt.duration, itime, otime, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_NEAR_INF));
		pkt.pos = -1;

        av_interleaved_write_frame(octx, &pkt);

        av_packet_unref(&pkt);
    }
}

