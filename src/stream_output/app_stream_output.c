/******************************************************************************
*  用于统一给app对接的函数
*
*f
*
*
******************************************************************************/
#ifdef HAVE_RTSP_MODULE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <memory.h>
#include <sys/time.h>
#include <errno.h>
#include <sys/types.h>
#include <pthread.h>
#include <time.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <sys/time.h>

#include "mid_timer.h"

#include "rtsp_server.h"
#include "rtp_build.h"
#include "rtp_ts_build.h"
#include "ts_build.h"

#include "stream_output_struct.h"
#include "stream_output_client.h"
#include "multicast_output_server.h"
#include "rtsp_output_server.h"
#include "traffic_shaping.h"
#include "app_stream_output.h"
#include "rtsp_porting_sdp_info.h"

#include "log_common.h"

#include "input_to_channel.h"
#include "app_protocol_handle.h"



/*初始化stream output需要的资源*/
void app_stream_output_init(unsigned int localip1, unsigned int localip2)
{
	/*init some middle function*/
	//	mid_timer_init();
	stream_client_init();
	tc_init();

	//init rtp ,ts ,rtpts  build handle
	app_build_ts_init();
	app_build_rtp_init();
	app_build_rtpts_init();

	rtsp_porting_sdp_init();
	app_multicast_server_init();

	rtsp_set_local_ip(localip1);
	rtsp_module_init();
	app_rtsp_server_init();
	return ;
}


/*输出h264给stream output模块*/
static int app_stream_video_output_channel(unsigned char *buff, int len, APP_VIDEO_DATA_INFO *info);

int app_stream_video_output(unsigned char *buff, int len, APP_VIDEO_DATA_INFO *info)
{
	return app_stream_video_output_channel(buff, len, info);
}

static int app_stream_video_output_channel(unsigned char *buff, int len, APP_VIDEO_DATA_INFO *info)
{


	if(len / (1024) > 1024) {
		PRINTF("len= %d\n", len / (1024));
	}

	int width = info->width;
	int height = info->height;
	unsigned int timestamp = info->timestamp;
	unsigned int rtp_time = info->rtp_time;
	int IsIframe = info->IsIframe;
	int mtu = 0;
	int rtsp_mtu = 0;
	int channel = 0;

	APP_MEDIA_INFO media_info ;
	memset(&media_info, 0, sizeof(APP_MEDIA_INFO));
	media_info.width = width;
	media_info.height = height;
	media_info.stream_channel = info->stream_channel;
	channel = media_info.stream_channel;
	//if(IsIframe == 1)
	//	PRINTF("IsIframe = %u.\n", IsIframe);

	//	static int g_test = 0;

	/*if it is iframe,i will update the video sdp info*/
	if(IsIframe == 1) {
		//test
		rtsp_porting_video_filter_sdp_info(buff, len, width, height, channel);
		//PRINTF("i Frame\n");
	}

	/*if ts is need,i must build ts.*/
	//	PRINTF("mult_ts_get_active=%d\n",mult_ts_get_active());
	if(mult_ts_get_active(channel) == 1 || mult_rtpts_get_active(channel) == 1) {
		app_build_ts_vdata(buff, len, timestamp, &media_info);
	}

	/*if rtp is need,i must build rtp ,maybe rtsp also need rtp*/
	//if(mult_rtp_get_active(channel) == 1 || stream_rtsp_get_active() == 1) {
	if(mult_rtp_get_active(channel) == 1) {
		mtu = mult_get_direct_rtp_mtu(channel);
		rtsp_mtu = stream_get_rtsp_mtu();

		if(rtsp_mtu < mtu) {
			mtu = rtsp_mtu;
		}

		//PRINTF("len=%06d\n",len);
		//PRINTF("buff=%p,buflen=%06d,g_test = %05d\n",buff,len,g_test++);
		//PRINTF("mtu=%d\n", mtu);
		//rtp_build_video_data(len, buff, IsIframe, mtu, timestamp, &media_info);
		app_build_rtp_vdata(len, buff, IsIframe, mtu, rtp_time, &media_info);
	} else {
		//	rtp_build_reset_time();
	}

	return 0;
}

#if 0
/*输出aac给stream output模块*/
static int app_stream_audio_output_channel(unsigned char *buff, int len, APP_AUDIO_DATA_INFO *info);

int app_stream_audio_output(unsigned char *buff, int len, APP_AUDIO_DATA_INFO *info)
{
	int channel = info->stream_channel;
	int recreate = info->recreate;

	if(channel == CHANNEL_INPUT_1) {
		app_stream_audio_output_channel(buff, len, info);
		//also send to CHANNEL_LOW_0
		info->stream_channel = CHANNEL_INPUT_1_LOW;
		info->recreate = recreate;
		app_stream_audio_output_channel(buff, len, info);
		return 0;
	} else if(channel == CHANNEL_INPUT_2) {
		app_stream_audio_output_channel(buff, len, info);
		//also send to CHANNEL_LOW_1
		info->stream_channel = CHANNEL_INPUT_2_LOW;
		info->recreate = recreate;
		app_stream_audio_output_channel(buff, len, info);
		return 0;
	}

	//compose stream select audio channle
	//预留
	return 0;
}
#endif
int app_stream_audio_output_channel(unsigned char *buff, int len, APP_AUDIO_DATA_INFO *info)
{
	static int g_test = 0;

	if(g_test++ % 100 == 0) {
		//PRINTF("buf=%02x,%02x,%02x,%02x,len=%d=srate=%d\n", buff[0], buff[1], buff[2], buff[3], len,info->samplerate);
	}

	int recreate = info->recreate;
	int samplerate = info->samplerate ;
	int channel = info->channel;
	unsigned int timestamp = info->timestamp;
	unsigned int rtp_time = info->rtp_time;
	int mtu = 0;
	int rtsp_mtu = 0;

	int achannel = 0;
	APP_MEDIA_INFO media_info ;
	memset(&media_info, 0, sizeof(APP_MEDIA_INFO));
	media_info.sample = info->samplerate;

	media_info.stream_channel = info->stream_channel;
	achannel = info->stream_channel;

	if(len > 1300) {
		PRINTF("Warnning,the len = %d\n", len);
	}

	//PRINTF("audio capture time = %u.\n", timestamp);

	/*if samplerate is change ,i must update audio sdp info.*/
	if(recreate == 1) {
		PRINTF("the audio changed ,update audio sdp.\n");

		rtsp_porting_audio_filter_sdp_info(buff, channel, samplerate, achannel);
	}

	/*if ts is need,i must build ts.*/
	if(mult_ts_get_active(achannel) == 1 || mult_rtpts_get_active(achannel) == 1) {
		app_build_ts_adata(buff, len, timestamp, &media_info);
	}

	/*if rtp is need,i must build rtp ,maybe rtsp also need rtp*/
	//	if(mult_rtp_get_active(achannel) == 1 || stream_rtsp_get_active() == 1) {
	if(mult_rtp_get_active(achannel) == 1) {
		mtu = mult_get_direct_rtp_mtu(achannel);
		rtsp_mtu = stream_get_rtsp_mtu();

		if(rtsp_mtu < mtu) {
			mtu = rtsp_mtu;
		}

		//	PRINTF("audio len=%d=samplerate=%d\n",len,samplerate);
		//	PRINTF("audio header =%02x,%02x,%02x,%02x,%02x,%02x,%02x,",buff[0],buff[1],
		//		buff[2],buff[3],buff[4],buff[5],buff[6]);
		//			PRINTF("audio header =%02x,%02x,%02x,%02x,%02x,%02x,%02x,\n",buff[7],buff[8],
		//		buff[9],buff[10],buff[11],buff[12],buff[13]);
		//rtp_build_audio_data(len, buff, samplerate, mtu, timestamp, &media_info);
		app_build_rtp_adata(len, buff, samplerate, mtu, rtp_time, &media_info);

	}

	return 0;
}

#if 0
/*==============================================================================
	函数: app_build_reset_time
	作用: 用于同步时间之后的处理
	输入:
	输出:
	作者:  modify by zm    2012.04.26
==============================================================================*/
void app_build_reset_time()
{
	if(g_ts_build_handle == NULL) {
		PRINTF("Error.\n");
		return ;
	}

	ts_build_reset_time(g_ts_build_handle);
	rtp_build_reset_time();
	rtpts_build_reset_time();
	return ;
}
#endif

#endif

