#include "XMediaEncode.h"
#include "common.h"

extern "C"
{
#include <libswscale/swscale.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
}

#include <iostream>

#pragma comment(lib, "swscale.lib")
#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "avutil.lib")
#pragma comment(lib, "swresample.lib")

using std::cout;
using std::endl;

#if defined WIN32 || defined _WIN32
#include <windows.h>
#endif 

// ��ȡCPU����
static int XGetCPUCore()
{
#if defined WIN32 || defined _WIN32
	SYSTEM_INFO sysinfo;
	GetSystemInfo(&sysinfo);

	return (int)sysinfo.dwNumberOfProcessors;
#elif defined __linux__
	return (int)sysconf(_SC_NPROCESSORS_ONLN);
#elif defined __APPLE__
	int numCPU = 0;
	int mib[4];
	size_t len = sizeof(numCPU);

	// set the mib for hw.ncpu
	mib[0] = CTL_HW;
	mib[1] = HW_AVAILCPU;  // alternatively, try HW_NCPU;

						   // get the number of CPUs from the system
	sysctl(mib, 2, &numCPU, &len, NULL, 0);

	if (numCPU < 1)
	{
		mib[1] = HW_NCPU;
		sysctl(mib, 2, &numCPU, &len, NULL, 0);

		if (numCPU < 1)
			numCPU = 1;
	}
	return (int)numCPU;
#else
	return 1;
#endif
}

class CXMediaEncode : public XMediaEncode
{
public:
    void Close() {
        if (nullptr != vsc) { 
            sws_freeContext(vsc); 
            vsc = NULL;
        }

        if (nullptr != asc) {
            swr_free(&asc);
            asc = nullptr;
        }

        if (nullptr != yuv) { 
            av_frame_free(&yuv);
        }

        if (nullptr != vc) { 
            avcodec_free_context(&vc);
        }

        if (nullptr != pcm) {
            av_frame_free(&pcm);
        }

        vpts = 0;
        av_packet_unref(&apack);

        apts = 0;
        av_packet_unref(&vpack);
        
    }

    bool InitAudioCode() {
        if (! CreateCodec(AV_CODEC_ID_AAC)) { return false; }

        ac->bit_rate = 40000;
        ac->sample_rate = sample_rate;
        ac->sample_fmt = AV_SAMPLE_FMT_FLTP;
        ac->channels = channels;
        ac->channel_layout = av_get_default_channel_layout(channels);
        return OpenCodec(&ac);
    }

    bool InitVideoCodec() {
        // ��ʼ���������
        if (! CreateCodec(AV_CODEC_ID_H264)) { return false; }

        // ѹ����ÿ����Ƶ��bitλ��С50KB
        vc->bit_rate = 50*1024*8;
        vc->width = out_width;
        vc->height = out_height;
        vc->time_base = {1, fps};
        vc->framerate = {fps, 1};

        // ������Ĵ�С������֡һ���ؼ�֡
        vc->gop_size = 50;
        vc->max_b_frames =0;
        vc->pix_fmt = AV_PIX_FMT_YUV420P;

        return OpenCodec(&vc);
    }

    AVPacket *EncodeAudio(AVFrame *frame) {
        pcm->pts = apts;

        apts += av_rescale_q(pcm->nb_samples, {1, sample_rate}, ac->time_base);
        if (0 != avcodec_send_frame(ac,pcm)) { return nullptr; }
        av_packet_unref(&apack);
        if (0 != avcodec_receive_packet(ac, &apack)) { return nullptr; }

        return &apack;
    }

    AVPacket *EncodeVideo(AVFrame *frame) {
        av_packet_unref(&vpack);
        // h264����
        frame->pts = vpts++;

        int ret = avcodec_send_frame(this->vc, frame);
        if (ret != 0) { return NULL; }

		ret = avcodec_receive_packet(vc, &vpack);
        if (ret != 0 || vpack.size<= 0) { return NULL; }

		return &vpack;
    }

   bool InitScale() {
        // ��ʼ����ʽת��������
        this->vsc = sws_getCachedContext(this->vsc, 
                                    in_width, in_height, AV_PIX_FMT_BGR24,
                                    out_width, out_height, AV_PIX_FMT_YUV420P,
                                    SWS_BICUBIC, 0, 0, 0);
        if (nullptr == this->vsc) {
            cout << "sws_getCachedContext failed" << endl;
            return false;
        }

        // ��ʼ����������ݽṹ
        this->yuv = av_frame_alloc();
        this->yuv->format = AV_PIX_FMT_YUV420P;
        this->yuv->width = in_width;
        this->yuv->height = in_height;
        this->yuv->pts = 0;
        
        // ����yuv�ռ�
        int ret = av_frame_get_buffer(this->yuv, 32);
        if (0 != ret) {
            av_error(ret);
            return false;
        }

        return true;
    }
	
    AVFrame *RGBToYUV(char *rgb)
    {
        // ��������ݽṹ
        uint8_t *indata[AV_NUM_DATA_POINTERS] = {0};
        // indata[0] bgrbgrbgr
        // plane indata[0] bbbbb indata[1] ggggg indata[2] rrrrr
        indata[0]= (uint8_t *)rgb;
        int insize[AV_NUM_DATA_POINTERS] = {0};
        // һ�У������ݵ��ֽ���
        insize[0] = in_width * pixsize;

        int h = sws_scale(this->vsc, indata, insize, 0,
                            in_height, this->yuv->data, this->yuv->linesize);
        if (h <= 0) { return nullptr; }

        return this->yuv;
    }

    bool InitResample() {
        asc = nullptr;
        asc = swr_alloc_set_opts(asc,
            av_get_default_channel_layout(channels), 
            (AVSampleFormat)out_sample_fmt,
            sample_rate, // �����ʽ
            av_get_default_channel_layout(channels),
            (AVSampleFormat)in_smaple_fmt, // �����ʽ
            sample_rate,
            0, 0);

        if (nullptr == asc) {
            cout << "sws_alloc_set_opts" << endl;
            return false;
        }

        int ret = swr_init(asc);
        has_error(ret);

        cout << "��Ƶ�ز����������ĳ�ʼ���ɹ�" << endl;

        // ��Ƶ�ز�������ռ����
        pcm = av_frame_alloc();
        pcm->format = out_sample_fmt;
        pcm->channels = channels;
        pcm->channel_layout = av_get_default_channel_layout(channels);
        // һ֡��Ƶһͨ���Ĳ�������
        pcm->nb_samples = nb_sample;
        // ��pcm����洢�ռ�
        ret = av_frame_get_buffer(pcm, 0);
        has_error(ret);

        return true;
    }

    AVFrame *Resample(char *data) {
        if (nullptr == data) { return nullptr; }

        const uint8_t *indata[AV_NUM_DATA_POINTERS] = {0};
        indata[0] = (uint8_t *)data;
        int len = swr_convert(asc, pcm->data, pcm->nb_samples, // �������
                                indata, pcm->nb_samples);
        if (len <= 0) { return nullptr; }

        return pcm;
    }


    CXMediaEncode() 
        : vpack({0}), apack({0})
    {}

private:
    bool OpenCodec(AVCodecContext **c) {
        int ret = avcodec_open2(*c, 0, 0);
        has_error(ret);

        cout << "avcodec_open2 successful" << endl;
        return true;
    }

    bool CreateCodec(AVCodecID cid) {
        AVCodec *codec = avcodec_find_encoder(cid);
        if (nullptr == codec) {
            cout << "avcodec_find_encoder failed" << endl;
            return false;
        }

        // ��Ƶ������������
        ac = avcodec_alloc_context3(codec);
        if (nullptr == ac) {
            cout << "avcodec_alloc_context3 failed" << endl;
            return false;
        }

        ac->flags != AV_CODEC_FLAG_GLOBAL_HEADER;
        ac->thread_count = XGetCPUCore();

        cout << "avcodec_alloc_context3 success" << endl;
        
        return true;
    }

private:
    // ���ظ�ʽת��������
    SwsContext *vsc = NULL;
    // ��Ƶ�ز���������
    SwrContext *asc = NULL;

    // �����YUV
    AVFrame *yuv = NULL;
    // �ز������pcm
    AVFrame *pcm = NULL;

    // ��Ƶ֡
    AVPacket apack;
    // ��Ƶ֡
    AVPacket vpack;

    int vpts = 0;
    int apts = 0;
};

XMediaEncode::XMediaEncode()
{
}


XMediaEncode::~XMediaEncode()
{
}

XMediaEncode *XMediaEncode::Get(unsigned char index)
{
    static bool is_first = true;
    if (is_first) {
        avcodec_register_all();
        av_register_all();
        avformat_network_init();

        is_first = true;
    }

    static CXMediaEncode media[255];

    return &media[index];
}
