#include "common.h"

#include <iostream>
#include <memory>
#include <functional>

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
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
using std::unique_ptr;

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
    Mat mat(200, 500, CV_8UC3);

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

void init_av()
{
    // ע�����еı������
    avcodec_register_all();

    // ע�����еķ�װ��
    av_register_all();

    // ע����������Э��
    avformat_network_init();
}

void opencv_rtmp() {
    char *out_url = "rtmp://192.168.26.31/live";
    
    init_av();

    // ��������ݽṹ
    AVFrame *yuv = NULL;

    unique_ptr<VideoCapture, std::function<void(VideoCapture *)>> video_ptr(
        new VideoCapture(), [](VideoCapture *captrue) { 
        if (captrue->isOpened()) { captrue->release(); }
        delete captrue;
    });

    Mat frame;

    // 1.ʹ��opencv ��usb ����ͷ
    video_ptr->open(0);

    if (! video_ptr->isOpened()) { 
        cout << "camera open usb camera error" << endl;
        return;
    }

    cout << "open usb camera successful." << endl;

    int width   = video_ptr->get(CAP_PROP_FRAME_WIDTH);
    int height  = video_ptr->get(CAP_PROP_FRAME_HEIGHT);
    int fps     = video_ptr->get(CAP_PROP_FPS);

    // ���fpsΪ0�����������25����Ϊ��fps=0ʱ������avcodec_open2����-22,
    // �������Ϸ�
    if (0 == fps) { fps = 25; }

    // 2.��ʼ����ʽת��������
    SwsContext *sws_context = NULL;
    sws_context = sws_getCachedContext(sws_context,
                    width, height, AV_PIX_FMT_BGR24,    // Դ��ʽ
                    width, height, AV_PIX_FMT_YUV420P,  // Ŀ���ʽ
                    SWS_BICUBIC,    // �ߴ�仯ʹ���㷨
                    0, 0, 0);

    if (NULL == sws_context) { 
        cout << "sws_getCachedContext error" << endl;
        return;
    }

    unique_ptr<SwsContext, std::function<void(SwsContext *)>> swscontext_tr
        (sws_context, [](SwsContext *swscontext) {
            sws_freeContext(swscontext);
            swscontext = nullptr;
    });

    // 3.��ʼ����������ݽṹ
    yuv         = av_frame_alloc();
    yuv->format = AV_PIX_FMT_YUV420P;
    yuv->width  = width;
    yuv->height = height;
    yuv->pts    = 0;

    // ����yuv�ռ�
    int ret_code = av_frame_get_buffer(yuv, 32);
    if (0 != ret_code) {
        av_error(ret_code);
        return;
    }

    // 4.��ʼ������������
    // 4.1�ҵ�������
    AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (NULL == codec) { 
        cout << "Can't find h264 encoder." << endl; 
        return;
    }

    // 4.2����������������
    AVCodecContext *codec_context = avcodec_alloc_context3(codec);
    if (NULL == codec_context) { 
       cout << "avcodec_alloc_context3 failed." << endl;
       return;
    }

    unique_ptr<AVCodecContext, std::function<void(AVCodecContext *)>> codec_context_ptr
        (codec_context, [](AVCodecContext *context) {
            avcodec_free_context(&context);
    });

    // 4.3���ñ���������
    // vc->flags           |= AV_CODEC_FLAG_GLOBAL_HEADER;
    codec_context->codec_id        = codec->id;
    codec_context->thread_count    = 8;

    // ѹ����ÿ����Ƶ��bit�� 50k
    codec_context->bit_rate = 50*1024*8;
    codec_context->width = width;
    codec_context->height = height;
    codec_context->time_base = {1, fps};
    codec_context->framerate = {fps, 1};

    // ������Ĵ�С������֡һ���ؼ�֡
    codec_context->gop_size = 50;
    codec_context->max_b_frames = 0;
    codec_context->pix_fmt = AV_PIX_FMT_YUV420P;
    codec_context->qmin = 10;
    codec_context->qmax = 51;

    // 4.4�򿪱�����������
    ret_code = avcodec_open2(codec_context, 0, 0);
    if (0 != ret_code) { 
        av_error(ret_code);
        return;
    }
    cout << "avcodec_open2 success!" << endl;

    // 5.�����װ������Ƶ������
    // 5.1���������װ��������
    // rtmp flv��װ��
    AVFormatContext *format_context = nullptr;
    ret_code = avformat_alloc_output_context2(&format_context, 0, "flv", out_url);
    if (0 != ret_code) { 
        av_error(ret_code);
        return;
    }

    unique_ptr<AVFormatContext, std::function<void(AVFormatContext *)>> format_context_ptr
        (format_context, [](AVFormatContext *context) {
        avio_closep(&context->pb); 
    });

    // 5.2�����Ƶ��
    AVStream *vs = avformat_new_stream(format_context, NULL);
    if (NULL == vs) { 
        cout << "avformat_new_stream failed." << endl;
        return;
    }

    vs->codecpar->codec_tag = 0;
    // �ӱ��������Ʋ���
    avcodec_parameters_from_context(vs->codecpar, codec_context);
    av_dump_format(format_context, 0, out_url, 1);

    // ��rtmp ���������IO
    ret_code = avio_open(&format_context->pb, out_url, AVIO_FLAG_WRITE);
    if (0 != ret_code) { 
        av_error(ret_code);
        return;
    }

    // д���װͷ
    ret_code = avformat_write_header(format_context, NULL);
    if (0 != ret_code) { 
        av_error(ret_code);
        return;
    }

    AVPacket pack;
    memset(&pack, 0, sizeof(pack));
    int vpts = 0;
    for (;;) {
        // ��ȡrtsp��Ƶ֡��������Ƶ֡
        if (! video_ptr->grab()) { continue; }

        // yuvת��Ϊrgb
        if (! video_ptr->retrieve(frame)) { continue; }

        imshow("video", frame);
        waitKey(1);

        // rgb to yuv
        uint8_t *in_data[AV_NUM_DATA_POINTERS] = {0};
        in_data[0] = frame.data;
        int in_size[AV_NUM_DATA_POINTERS] = {0};
        // һ�У������ݵ��ֽ���
        in_size[0] = frame.cols * frame.elemSize();
        int h = sws_scale(sws_context, in_data, in_size, 0, frame.rows,
                            yuv->data, yuv->linesize);
        if (h <= 0) { continue; }

        // h264����
        yuv->pts = vpts;
        vpts++;

        ret_code = avcodec_send_frame(codec_context, yuv);
        if (0 != ret_code) { continue; }

        ret_code = avcodec_receive_packet(codec_context, &pack);
        
        if (0 != ret_code || pack.buf > 0) {
            //TODO something
        } else { continue; }

        // ����
        pack.pts = av_rescale_q(pack.pts, codec_context->time_base, vs->time_base);
        pack.dts = av_rescale_q(pack.dts, codec_context->time_base, vs->time_base);
        pack.duration = av_rescale_q(pack.duration, 
                                        codec_context->time_base, 
                                        vs->time_base);
        ret_code = av_interleaved_write_frame(format_context, &pack);
        if (0 == ret_code) { cout << "#" << flush; }
    }

    destroyAllWindows();
}

// ˫��ĥƤ�㷨
int bilateral() {
	Mat src = imread("001.jpg");
	
	if (! src.data) {
		cout << "open file failed" << endl;
		getchar();

		return -1;
	}

	const int width		= 400;
	const int height	= 374;

	cv::namedWindow("src");
	cv::moveWindow("src", width, height);
	cv:imshow("src", src);

	Mat image;
	int d = 3;

	cv::namedWindow("image");
	cv::moveWindow("image", width, height);

	for (;;) {

		long long b = getTickCount();
		bilateralFilter(src, image, d, d*2, d/2);
		double sec = (double)(getTickCount()-b)/(double)getTickFrequency();
		cout << "d=" << d << " sec is " << sec << endl;

		imshow("image", image);

		int k = waitKey(0);
		if (k == 'd') {
			d += 2;
		} else if (k == 'f') {
			d -= 2;
		} else {
			break;
		}
	}

	waitKey(0);

	return 0;
}

int main(int argc, char *argv[])
{
    // showimage();

    // padding_rgb();

    // open_usb_camera();

    // opencv_rtmp();

	bilateral();

    getchar();

    return 0;
}
