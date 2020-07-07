#pragma once

struct AVFrame;
struct AVPacket;

class AVCodecContext;

enum XSampleFMT {
    X_S16 = 1,
    X_FLATP = 8
};

// ����Ƶ����ӿ���
class XMediaEncode
{
public:
    // �������
    int in_width    = 1280;
    int in_height   = 720;
    int pixsize     = 3;
    int channels    = 2;
    int sample_rate = 44100;
    XSampleFMT in_smaple_fmt = X_S16;

    // �������
    int out_width   = 1280;
    int out_height  = 720;
    // ѹ����ÿ����Ƶ��bitλ��С50K
    int bitrate     = 4000000; 
    int fps         = 25;
    int nb_sample   = 1024;
    XSampleFMT  out_sample_fmt = X_FLATP;

    static XMediaEncode *Get(unsigned char index = 0);

    // ��ʼ�����ظ�ʽת���������ĳ�ʼ��
    virtual bool InitScale() = 0;

    // ��Ƶ�ز��������ĳ�ʼ��
    virtual bool InitResample() = 0;

    // ����ֵ �������������
    virtual AVFrame *Resample(char *data) = 0;

    // ����ֵ �������������
    virtual AVFrame *RGBToYUV(char *rgb) = 0;

    // ��Ƶ�����ʼ��
    virtual bool InitVideoCodec() = 0;

    // ��Ƶ�����ʼ��
    virtual bool InitAudioCode() = 0;

    // ��Ƶ����
    virtual AVPacket *EncodeVideo(AVFrame *frame) = 0;

    // ��Ƶ����
    virtual AVPacket *EncodeAudio(AVFrame *frame) = 0;

    virtual ~XMediaEncode();

    // ��Ƶ������������
    AVCodecContext *vc = 0;

    // ��Ƶ������������
    AVCodecContext *ac = 0;

protected:
    XMediaEncode();
};

