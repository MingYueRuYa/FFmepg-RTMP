#pragma once

struct AVFrame;
struct AVPacket;
struct AVCodecContext;

// ����Ƶ����ӿ���
class XMediaEncode
{
public:
    // �������
    int in_width    = 1280;
    int in_height   = 720;
    int pixsize     = 3;

    // �������
    int out_width   = 1280;
    int out_height  = 720;
    // ѹ����ÿ����Ƶ��bitλ��С50K
    int bitrate     = 4000000; 
    int fps         = 25;

    static XMediaEncode *Get(unsigned char index = 0);

    // ��ʼ�����ظ�ʽת���������ĳ�ʼ��
    virtual bool InitScale() = 0;

    virtual AVFrame *RGBToYUV(char *rgb) = 0;

    // ��Ƶ������ʼ��
    virtual bool InitVideoCodec() = 0;

    // ��Ƶ����
    virtual AVPacket *EncodeVideo(AVFrame *frame) = 0;

    virtual ~XMediaEncode();

    // ������������
    AVCodecContext *vc = 0;

public:
    XMediaEncode();
};

