#include "XMediaEncode.h"
#include "common.h"

extern "C"
{
#include <libswscale/swscale.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

#include <iostream>

#pragma comment(lib, "swscale.lib")
#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "avutil.lib")

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

        if (nullptr != yuv) { 
            av_frame_free(&yuv);
        }

        if (nullptr != vc) { 
            avcodec_free_context(&vc);
        }

        vpts = 0;
        av_packet_unref(&pack);
    }

    bool InitVideoCodec()
    {
        // ��ʼ���������
        AVCodec * codec = avcodec_find_encoder(AV_CODEC_ID_H264);
        if (nullptr == codec) {
            cout << "Can't find H264 encoder" << endl;
            return false;
        }

        // ����������������
        this->vc = avcodec_alloc_context3(codec);
        if (nullptr == vc) {
            cout << "avcodec_alloc_context3 error" << endl;
            return false;
        }

        // ���ñ���������
        vc->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        vc->codec_id = codec->id;
        vc->thread_count = XGetCPUCore();

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

        // �򿪱�����������
        int ret = avcodec_open2(this->vc, 0, 0);
        if (0 != ret) {
            av_error(ret);
            return false;
        }

        cout << "avcodec_open2 success" << endl;
        return true;
    }

    AVPacket *EncodeVideo(AVFrame *frame)
    {
        av_packet_unref(&pack);
        // h264����
        frame->pts = vpts++;

        int ret = avcodec_send_frame(this->vc, frame);
        if (0 != ret) {
            av_error(ret);
            return false;
        }

        ret = avcodec_receive_packet(this->vc, &this->pack);
        if (0 != ret || this->pack.size <= 0) { return nullptr; }

        return &this->pack;
    }

    bool InitScale()
    {
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

    CXMediaEncode() 
        : pack({0})
    {}

private:
    // ���ظ�ʽת��������
    SwsContext *vsc = NULL;

    // �����YUV
    AVFrame *yuv = NULL;

    AVPacket pack;

    int vpts = 0;

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
        is_first = true;
    }

    static CXMediaEncode media[255];

    return &media[index];
}
