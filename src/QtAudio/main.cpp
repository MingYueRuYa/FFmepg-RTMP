#include "common.h"

#include <QtCore/QDebug>
#include <QtCore/QThread>
#include <QtCore/QCoreApplication>
#include <QtMultimedia/QAudioInput>

#include <iostream>

extern "C"
{
#include <libswresample/swresample.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

#pragma comment(lib, "swresample.lib")
#pragma comment(lib, "avutil.lib")
#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "avformat.lib")

using std::cout;
using std::endl;

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

	//ע�����еı������
	avcodec_register_all();

	//ע�����еķ�װ��
	av_register_all();

	//ע����������Э��
	avformat_network_init();

    char *out_url = "rtmp://192.168.26.31/live";

    int sample_rate = 44100;
    int channels    = 2;
    int sample_byte = 2;

    AVSampleFormat in_sample_fmt    = AV_SAMPLE_FMT_S16;
    AVSampleFormat out_sample_fmt   = AV_SAMPLE_FMT_FLTP;

    QAudioFormat audio_format;
    audio_format.setSampleRate(sample_rate);
    audio_format.setChannelCount(channels);
    audio_format.setSampleSize(sample_byte*8);
    audio_format.setCodec("audio/pcm");
    audio_format.setByteOrder(QAudioFormat::LittleEndian);
    audio_format.setSampleType(QAudioFormat::UnSignedInt);

    QAudioDeviceInfo device_info = QAudioDeviceInfo::defaultInputDevice();

    if (! device_info.isFormatSupported(audio_format)) {
        cout << "Audio format not support " << endl;
        return -1;
    }

    QAudioInput *input = new QAudioInput(audio_format);
    // ��ʼ¼����Ƶ
    QIODevice *io = input->start();

    // 2 ��Ƶ�ز����������ĳ�ʼ��
    SwrContext *asc = nullptr;
    asc = swr_alloc_set_opts(asc,
        // �����ʽ
        av_get_default_channel_layout(channels), out_sample_fmt, sample_rate, 
        // �����ʽ
        av_get_default_channel_layout(channels), in_sample_fmt, sample_rate, 
        0, 0);
    if (nullptr == asc) {
        cout << "swr_alloc_set_opts failed.\n";
        return -1;
    }

    int ret = swr_init(asc);
    has_error(ret);

    cout << "��Ƶ�ز����������ĳ�ʼ���ɹ���" << endl;

    // 3 ��Ƶ�ز�������ռ����
    AVFrame *pcm    = av_frame_alloc();
    pcm->format     = out_sample_fmt;
    pcm->channels   = channels;
    pcm->channel_layout = av_get_default_channel_layout(channels);
    // һ֡��Ƶһͨ���Ĳ�������
    pcm->nb_samples = 1024;
    // pcm����洢�ռ�
    ret = av_frame_get_buffer(pcm, 0);
    has_error(ret);

    // 4 ��ʼ����Ƶ������
    AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_AAC);
    if (nullptr == codec) {
        cout << "avcodec_find_encoder AV_CODEC_ID_AAC failed." << endl;
        return -1;
    }

    // ��Ƶ������������
    AVCodecContext *ac = avcodec_alloc_context3(codec);
    if (nullptr == ac) {
        cout << "avcodec_alloc_context3 failed." << endl;
        return -1;
    }

    cout << "avcodec_alloc_context3 successful." << endl;

    ac->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    ac->thread_count = 8;
    ac->bit_rate = 40000;
    ac->sample_rate = sample_rate;
    ac->sample_fmt = AV_SAMPLE_FMT_FLTP;
    ac->channels = channels;
    ac->channel_layout = av_get_default_channel_layout(channels);

    // ����Ƶ������
    ret = avcodec_open2(ac, 0, 0);
    has_error(ret);

    cout << "avcodec_open2 successful." << endl;

    // 5 �����װ������Ƶ������
    // a. ���������װ��������
    AVFormatContext *ic = nullptr;
    ret = avformat_alloc_output_context2(&ic, 0, "flv", out_url);
    has_error(ret);

    // b.�����Ƶ��
    AVStream *as = avformat_new_stream(ic, nullptr);
    if (nullptr == as) {
        cout << "avformat_new_stream failed" << endl;
        return -1;
    }

    as->codecpar->codec_tag = 0;

    // �ӱ��������Ʋ���
    avcodec_parameters_from_context(as->codecpar, ac);
    av_dump_format(ic, 0, out_url, 1);

    // ��rtmp���������io
    ret = avio_open(&ic->pb, out_url, AVIO_FLAG_WRITE);
    has_error(ret);

    // д���װͷ
    ret = avformat_write_header(ic, nullptr);
    has_error(ret);

    // һ�ζ�ȡһ֡��Ƶ���ֽ���
    int read_size = pcm->nb_samples * channels * sample_byte;
    char *buf = new char[read_size];
    int apts = 0;
    AVPacket pkt = {0};
    for (;;) {
        // һ�ζ�ȡһ֡��Ƶ
        if (input->bytesReady() < read_size) {
            QThread::msleep(1); 
            continue;
        }

        int size = 0;
        while (size != read_size) {
            int len = io->read(buf+size, read_size-size);
            if (len < 0) { break; }
            size += len;
        }

        if (size != read_size) { continue; }

        // �Ѿ���ȡһ֡����
        // �ز���Դ����
        const uint8_t *indata[AV_NUM_DATA_POINTERS] = {0};
        indata[0] = (uint8_t *)buf;
        int len = swr_convert(
                                // �������������洢��ַ����������
                                asc, pcm->data, pcm->nb_samples,
                                indata, pcm->nb_samples);

        // pts����
        // nb_sample/sample_rate = һ֡��Ƶ������sec
        // timebase pts = sec*timebaes.den
        pcm->pts = apts;
        apts += av_rescale_q(pcm->nb_samples, {1, sample_rate}, ac->time_base);

        int ret = avcodec_send_frame(ac, pcm);
        if (0 != ret) { continue; }

        av_packet_unref(&pkt);

        ret = avcodec_receive_packet(ac, &pkt);
        if (0 != ret) { continue; }

        cout << pkt.size << " " << flush;

        // ����
        pkt.pts = av_rescale_q(pkt.pts, ac->time_base, as->time_base);
        pkt.dts = av_rescale_q(pkt.dts, ac->time_base, as->time_base);
        pkt.duration = av_rescale_q(pkt.duration, ac->time_base, as->time_base);
        ret = av_interleaved_write_frame(ic, &pkt);
        if (0 == ret) { cout << "#" << flush; }
            
    }

    delete[] buf;

    /*
    QList<QAudioDeviceInfo> device_list = 
                        QAudioDeviceInfo::availableDevices(QAudio::AudioInput);

    for each (QAudioDeviceInfo info in device_list)
    {
        qDebug() << info.deviceName() << "\n";
    }
    */

    return a.exec();
}
