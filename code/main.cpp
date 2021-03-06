#include"headers.h"

int main(int argc,char**argv)
{
	AVFormatContext *pFormatCtx = NULL;
	int i, videoStream;
	AVCodecContext *pCodecCtx = NULL;
	AVCodec *pCodec = NULL;
	AVFrame *pFrame = NULL;
	AVFrame *pFrameRGB = NULL;
	AVPacket packet;
	int frameFinished=0,numBytes;
	uint8_t *buffer = NULL;
	AVDictionary *optionsDict = NULL;
	struct SwsContext *sws_ctx = NULL;
	int width=0,height=0,skipme=0;
	
	// current image
	uint8_t *image;

	// background/foreground segmentation map
	uint8_t *segMap;
	
	//Erosion Structure
	uint8_t erosion_mat[]={255,255,255,255,255,255,255,255,255};
	uint8_t dilation_mat[]={0,0,0,0,0,0,0,0,0};

	int model_initialized=0;

	//srand((int)&width);
	time_t stime,etime;
	time(&stime);

	av_register_all();

	// Open video file
	if(avformat_open_input(&pFormatCtx, argv[1], NULL, NULL)!=0)
		return -1; // Couldn't open file

	// Retrieve stream information
	if(avformat_find_stream_info(pFormatCtx, NULL)<0)
		return -1; // Couldn't find stream information


	// Dump information about file onto standard error
	av_dump_format(pFormatCtx, 0, argv[1], 0);

	// Find the first video stream
	videoStream=-1;
	for(i=0; i<pFormatCtx->nb_streams; i++)
		if(pFormatCtx->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO) 
		{
			videoStream=i;
			break;
		}
	
	if(videoStream==-1)
		return -1; // Didn't find a video stream

	// Get a pointer to the codec context for the video stream
	pCodecCtx=pFormatCtx->streams[videoStream]->codec;

	// Find the decoder for the video stream
	pCodec=avcodec_find_decoder(pCodecCtx->codec_id);
	if(pCodec==NULL)
       	{
		fprintf(stderr, "Unsupported codec!\n");
		return -1; // Codec not found
	}
	// Open codec
	if(avcodec_open2(pCodecCtx, pCodec, &optionsDict)<0)
		return -1; // Could not open codec

	// Allocate video frame
	pFrame=avcodec_alloc_frame();

	// Allocate an AVFrame structure
	pFrameRGB=avcodec_alloc_frame();
	if(pFrameRGB==NULL)
		return -1;

	// Determine required buffer size and allocate buffer
	numBytes=avpicture_get_size(PIX_FMT_RGB24, pCodecCtx->width,
                                pCodecCtx->height);
	buffer=(uint8_t *)av_malloc(numBytes*sizeof(uint8_t));

	sws_ctx =
		sws_getContext
        (
            pCodecCtx->width,
            pCodecCtx->height,
            pCodecCtx->pix_fmt,
            pCodecCtx->width,
            pCodecCtx->height,
            PIX_FMT_RGB24,
            SWS_BILINEAR,
            NULL,
            NULL,
            NULL
        );
	// Assign appropriate parts of buffer to image planes in pFrameRGB
	// Note that pFrameRGB is an AVFrame, but AVFrame is a superset
	// of AVPicture
	avpicture_fill((AVPicture *)pFrameRGB, buffer, PIX_FMT_RGB24,
                   pCodecCtx->width, pCodecCtx->height);

	//alloc for gray and foreground image
	width=pCodecCtx->width;
	height=pCodecCtx->height;
	image=new uint8_t[height*width];
	segMap=new uint8_t[height*width];

	i=0;

	while(av_read_frame(pFormatCtx, &packet)>=0) 
	{
		// Is this a packet from the video stream?
		//if(skipme<800)
		//{
			skipme++;
		//}
		//else{
		if(packet.stream_index!=videoStream) 
			continue;
			// Decode video frame
			int ret = avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished,
                                  &packet);
			if(!frameFinished)
			{
				continue;
			}
			// Did we get a video frame? yuv420 format is converted to rgb
			assert(ret>0);
			sws_scale
				(
				 sws_ctx,
				 (uint8_t const * const *)pFrame->data,
				 pFrame->linesize,
				 0,
				 pCodecCtx->height,
				 pFrameRGB->data,
				 pFrameRGB->linesize
				 );

			rgb_to_gray(pFrameRGB,image,width,height);
		
		if(!model_initialized)
		{
			model_initialized=1;
			init_background_model(image,width,height);
		}
		bool val = is_key_frame(image,width,height,i);
		if(i<500||val)
		{
			background_subtract(image,segMap,width,height);

			if(val)
			{
				erode(segMap,width,height,erosion_mat,3);  //erroision remove small elements	
				dilate(segMap,width,height,dilation_mat,3);// to get back small egdes after erroision
				char infile[100],outfile[100];
				sprintf(infile,"framekey%d.ppm",i);
				sprintf(outfile,"bkframekey%d.ppm",i);
				ofstream o,orig;
				orig.open(infile,ios::out|ios::binary);
				orig<<"P6"<<endl<<width<<" "<<height<<endl<<"255"<<endl;
				orig.write((char*)pFrameRGB->data[0],height*pFrameRGB->linesize[0]);
				orig.close();
				o.open(outfile,ios::out|ios::binary);
				o<<"P6"<<endl;
				o<<width<<" "<<height<<endl;
				o<<"255"<<endl;
				for(int x=0;x<height;x++)
				{
					for(int y=0;y<width;y++)
					{
						int pixel=x*width+y;
						if(!segMap[pixel])
						{
							o<<(unsigned char)0;
							o<<(unsigned char)0;
							o<<(unsigned char)0;
						}
						else
						{
							o.write((char*)pFrameRGB->data[0]+x*pFrameRGB->linesize[0]+3*y,3);
						}
					}
				}
				o.close();
			}
		}
		i++;
		av_free_packet(&packet);
		}	
//	}
	av_free(pFrame);
	av_free(pFrameRGB);
	av_free(buffer);
	avcodec_close(pCodecCtx);
	avformat_close_input(&pFormatCtx);
	delete []image;
	delete []segMap;

	time(&etime);
	cout<<"Approx CPU time "<<etime-stime<<endl;
	return 0;
}					
