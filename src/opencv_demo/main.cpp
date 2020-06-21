#include <iostream>

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavformat/avformat.h>
}

using namespace cv;

using std::cout;
using std::endl;
using std::cerr;
using std::flush;

#pragma comment(lib, "swscale.lib")
#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "avutil.lib")
#pragma comment(lib, "avformat.lib")
#pragma comment(lib, "opencv_world331.lib")

void showimage()
{
    Mat image = imread("wechat_web.png");
    namedWindow("img");
    
    imshow("img", image);
    waitKey(0);
}

void padding_rgb()
{
    Mat mat(800, 600, CV_8UC3);

    // Ԫ���ֽ�����С
    int es = mat.elemSize();
    int size = mat.rows*mat.cols*es;

    // ��ַ����������Mat
    for (int i = 0; i < size; i += es) {
        mat.data[i] = 255; // B
        mat.data[i+1] = 100; // G
        mat.data[i+2] = 100; // R
    }

    namedWindow("mat");
    imshow("mat", mat);
    waitKey(0);
}

void open_usb_camera()
{
    // ��ʾ��һ����ͷ�豸
    VideoCapture vediocapture(0);
    
    if (! vediocapture.isOpened()) {
        cout << "vediocapture open error" << endl;
        return ;
    }

    Mat frame;
    while (vediocapture.read(frame)) {
        imshow("video-demo", frame);

        // ����ESC����ʾ�˳�
        char c = waitKey(30);
        if (27 == c) { break; }
    }

    vediocapture.release();
    destroyAllWindows();
}

void opencv_rtmp() {
    char *out_url = "rtmp://192.168.26.31/live";

    // ע�����еı������
    avcodec_register_all();

    // ע�����еķ�װ��
    av_register_all();

    // ע����������Э��
    avformat_network_init();

    VideoCapture camera;
    Mat frame;
    namedWindow("vedio");

    // ���ظ�ʽת��������
    SwsContext *vsc = NULL;

    // ��������ݽṹ
    AVFrame *yuv = NULL;

    // ������������
    AVCodecContext *vc = NULL;

    // rtmp flv��װ��
    AVFormatContext *ic = NULL;

    try {
        // 1.ʹ��opencv ��usb ����ͷ
        camera.open(0);

        if (! camera.isOpened()) { 
            throw std::exception("camera open usb camera error"); 
        }

        cout << "open usb camera successful." << endl;

        int width   = camera.get(CAP_PROP_FRAME_WIDTH);
        int height  = camera.get(CAP_PROP_FRAME_HEIGHT);
        int fps     = camera.get(CAP_PROP_FPS);

        if (0 == fps) { fps = 25; }

        // 2.��ʼ����ʽת��������
        vsc = sws_getCachedContext(vsc,
                        width, height, AV_PIX_FMT_BGR24,    // Դ��ʽ
                        width, height, AV_PIX_FMT_YUV420P,  // Ŀ���ʽ
                        SWS_BICUBIC,    // �ߴ�仯ʹ���㷨
                        0, 0, 0);

        if (NULL == vsc) { throw std::exception("sws_getCachedContext error"); }

        // 3.��ʼ����������ݽṹ
        yuv         = av_frame_alloc();
        yuv->format = AV_PIX_FMT_YUV420P;
        yuv->width  = width;
        yuv->height = height;
        yuv->pts    = 0;

        // ����yuv�ռ�
        int ret_code = av_frame_get_buffer(yuv, 32);
        if (0 != ret_code) {
            char buff[1024] = {0};
            av_strerror(ret_code, buff, sizeof(buff)-1);
            throw std::exception(buff);
        }

        // 4.��ʼ������������
        // 4.1�ҵ�������
        AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_H264);
        if (NULL == codec) { throw std::exception("Can't find h264 encoder."); }

        // 4.2����������������
        vc = avcodec_alloc_context3(codec);
        if (NULL == vc) { 
           throw std::exception("avcodec_alloc_context3 failed."); 
        }

        // 4.3���ñ���������
        // vc->flags           |= AV_CODEC_FLAG_GLOBAL_HEADER;
        vc->codec_id        = codec->id;
        vc->thread_count    = 8;

        // ѹ����ÿ����Ƶ��bit�� 50k
        vc->bit_rate = 50*1024*8;
        vc->width = width;
        vc->height = height;
        vc->time_base = {1, fps};
        vc->framerate = {fps, 1};

        // ������Ĵ�С������֡һ���ؼ�֡
        vc->gop_size = 50;
        vc->max_b_frames = 0;
        vc->pix_fmt = AV_PIX_FMT_YUV420P;
        vc->qmin = 10;
        vc->qmax = 51;

        // 4.4�򿪱�����������
        ret_code = avcodec_open2(vc, 0, 0);
        if (0 != ret_code) { 
            char buff[1024] = {0};
            av_strerror(ret_code, buff, sizeof(buff)-1);
            throw std::exception(buff);
        }
        cout << "avcodec_open2 success!" << endl;

        // 5.�����װ������Ƶ������
        // 5.1���������װ��������
        ret_code = avformat_alloc_output_context2(&ic, 0, "flv", out_url);
        if (0 != ret_code) { 
            char buff[1024] = {0};
            av_strerror(ret_code, buff, sizeof(buff)-1);
            throw std::exception(buff);
        }
        // 5.2�����Ƶ��
        AVStream *vs = avformat_new_stream(ic, NULL);
        if (NULL == vs) { throw std::exception("avformat_new_stream failed."); }

        vs->codecpar->codec_tag = 0;
        // �ӱ��������Ʋ���
        avcodec_parameters_from_context(vs->codecpar, vc);
        av_dump_format(ic, 0, out_url, 1);

        // ��rtmp ���������IO
        ret_code = avio_open(&ic->pb, out_url, AVIO_FLAG_WRITE);
        if (0 != ret_code) { 
            char buff[1024] = {0};
            av_strerror(ret_code, buff, sizeof(buff)-1);
            throw std::exception(buff);
        }

        // д���װͷ
        ret_code = avformat_write_header(ic, NULL);
        if (0 != ret_code) { 
            char buff[1024] = {0};
            av_strerror(ret_code, buff, sizeof(buff)-1);
            throw std::exception(buff);
        }

        AVPacket pack;
        memset(&pack, 0, sizeof(pack));
        int vpts = 0;
        for (;;) {
            // ��ȡrtsp��Ƶ֡��������Ƶ֡
            if (! camera.grab()) { continue; }

            // yuvת��Ϊrgb
            if (! camera.retrieve(frame)) { continue; }

            imshow("video", frame);
            waitKey(1);

            // rgb to yuv
            uint8_t *in_data[AV_NUM_DATA_POINTERS] = {0};
            in_data[0] = frame.data;
            int in_size[AV_NUM_DATA_POINTERS] = {0};
            // һ�У������ݵ��ֽ���
            in_size[0] = frame.cols * frame.elemSize();
            int h = sws_scale(vsc, in_data, in_size, 0, frame.rows,
                                yuv->data, yuv->linesize);
            if (h <= 0) { continue; }

            // h264����
            yuv->pts = vpts;
            vpts++;

            ret_code = avcodec_send_frame(vc, yuv);
            if (0 != ret_code) { continue; }

            ret_code = avcodec_receive_packet(vc, &pack);
            
            if (0 != ret_code || pack.buf > 0) {
                //TODO something
            } else { continue; }

            // ����
            pack.pts = av_rescale_q(pack.pts, vc->time_base, vs->time_base);
            pack.dts = av_rescale_q(pack.dts, vc->time_base, vs->time_base);
            pack.duration = av_rescale_q(pack.duration, 
                                            vc->time_base, 
                                            vs->time_base);
			ret_code = av_interleaved_write_frame(ic, &pack);
            if (0 == ret_code) { cout << "#" << flush; }
        }

    } catch (std::exception &ex) {
        if (camera.isOpened()) { camera.release(); }

		if (vsc) {
			sws_freeContext(vsc);
			vsc = NULL;
		}

        if (vc) {
            if (ic) { avio_closep(&ic->pb); }
			avcodec_free_context(&vc);
		}

		cerr << ex.what() << endl;
    }

    camera.release();
    destroyAllWindows();
}

int main(int argc, char *argv[])
{
    // showimage();

    // padding_rgb();

    // open_usb_camera();

    opencv_rtmp();

    getchar();

    return 0;
}
