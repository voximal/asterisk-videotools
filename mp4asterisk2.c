/* mp4asterisk
 * Video bandwith control for mp4
 * Copyright (C) 2006 Sergio Garcia Murillo
 *
 * sergio.garcia@fontventa.com
 * http://sip.fontventa.com
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#include <mp4.h>

#include <avcodec.h>
#include <avformat.h>
#include <libavutil/pixdesc.h>
#include <libavcodec/audioconvert.h>
#include <libswscale/swscale.h>

static void dump_buffer_hex(unsigned char * text, unsigned char * buff, int len)
{
        int i,j;
        char *temp;
        temp = (char*)malloc(10*9+20);
        if (temp == NULL) {
                printf("dump_buffer_hex: failed to allocate buffer!\n");
                return;
        }
        printf("hex : (%d) %s\n",len, text);
        j=0;
        for (i=0;i<len;i++) {
                sprintf( temp+(9*j), "%04d %02x  ", i, buff[i]);
                j++;
                if (j==10)
                {
                 printf("hex : %s\n",temp);
                 j=0;
                }
        }
        if (j!=0)
        printf("hex : %s\n",temp);
        free(temp);
}

int mp4asterisk(char *name)
{
  int index;
  int namelen;
	unsigned char type2; 
	MP4TrackId hintId;
	MP4TrackId trackId;
	const char *type = NULL;	
 	unsigned short numHintSamples;
 	unsigned short packetIndex;
	
	u_int32_t datalen;
	u_int8_t databuffer[8000];
  uint8_t* data;	
  
  int outfile;

	char filename[8000];
	
	int i,j;
  	
  strcpy(filename, name);  	
	namelen=strlen(name);
		
	if ((strcmp(name+namelen-3, "mp4")) 
   && (strcmp(name+namelen-3, "3gp")) 
   && (strcmp(name+namelen-3, "mov"))) 
	{
		printf("mp4asterisk\nusage: mp4asterisk file\n");
		printf("invalide file format : %s\n", name+namelen-3);
		return -1;
	}
  		
	/* Open mp4*/
	MP4FileHandle mp4 = MP4Read(name, 9);

	/* Disable Verbosity */
	MP4SetVerbosity(mp4, 0);
	
	index = 0;

	/* Find first hint track */
	hintId = MP4FindTrackId(mp4, index++, MP4_HINT_TRACK_TYPE, 0);

	/* If not found video track*/
	while (hintId!=MP4_INVALID_TRACK_ID)
	{
		unsigned int frameTotal;
		int timeScale;

		trackId = MP4GetHintTrackReferenceTrackId(mp4, hintId);
		
    /* Get type */
		type = MP4GetTrackType(mp4, trackId);		
		
		if (strcmp(type, MP4_VIDEO_TRACK_TYPE) == 0)
    { 

		/* Get video type */
		MP4GetHintTrackRtpPayload(mp4, hintId, &name, NULL, NULL, NULL);		

    if (name)
		printf("Track name: %s\n", name);
		else
		printf("Track name: (null)\n");
		
		timeScale = MP4GetTrackTimeScale(mp4, hintId);	

		printf("Track timescale: %d\n", timeScale);

    frameTotal = MP4GetTrackNumberOfSamples(mp4,hintId);
    
		printf("Number of samples: %d\n", frameTotal);

    if (name == NULL) // Vidiator
    //strcat(filename, ".h263");
    strcpy(filename+namelen-4, ".h263p");
    else    
    if (!strcmp(name, "H263-2000"))
    //strcat(filename, ".h263");
    strcpy(filename+namelen-4, ".h263p");
    else
    if (!strcmp(name, "H264"))
    //strcat(filename, ".h264");
    strcpy(filename+namelen-4, ".h264");
    else
    if (!strcmp(name, "H263"))
    //strcat(filename, ".h263");
    strcpy(filename+namelen-4, ".h263");
    else
    strcpy(filename, name);

		printf("Create file: %s\n", filename);

		outfile = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0666) ;
		if (outfile < 0)
		{
			fprintf(stderr, "Failed to create %s\n", filename) ;
			return 1;
		}

    if (name)
    if (!strcmp(name, "H264"))
    {
      uint8_t **seqheader, **pictheader;
      uint32_t *pictheadersize, *seqheadersize;
      uint32_t ix;
      
			int samples;
			int zero = 0;
      unsigned int ts;			  
			unsigned short len;
			int mark = 0x8000;      

      MP4GetTrackH264SeqPictHeaders(mp4, trackId, 
				&seqheader, &seqheadersize,
				&pictheader, &pictheadersize);  
        
      for (ix = 0; seqheadersize[ix] != 0; ix++)
      {
        //dump_buffer_hex("SeqHeader", seqheader[ix], seqheadersize[ix]);
 
        memcpy(databuffer, seqheader[ix], seqheadersize[ix]);
				  datalen = seqheadersize[ix];

        ts = htonl(zero);
        len = htons(datalen | mark);			   
			   
		    dump_buffer_hex((unsigned char *)name, (unsigned char *)databuffer, datalen);
			   
 		  	write(outfile, &ts, 4) ;
	      write(outfile, &len, 2) ;
	      write(outfile, databuffer, datalen) ;
      }
      for (ix = 0; pictheadersize[ix] != 0; ix++)
      {
        //dump_buffer_hex("PictHeader", pictheader[ix], pictheadersize[ix]);

        memcpy(databuffer, pictheader[ix], pictheadersize[ix]);
				  datalen = pictheadersize[ix];
	
	      ts = htonl(zero);
        len = htons(datalen | mark);

		    dump_buffer_hex((unsigned char *)name, (unsigned char *)databuffer, datalen);

 		  	write(outfile, &ts, 4) ;
	      write(outfile, &len, 2) ;
	      write(outfile, databuffer, datalen) ;
      }           
    }

		/* Iterate frames */
		for (i=1;i<frameTotal+1;i++)
		//for (int i=1;i<10+1;i++)
		{
		
		  if (!MP4ReadRtpHint(mp4, hintId, i, &numHintSamples)) {
			  printf("MP4ReadRtpHint failed [%d,%d]\n", hintId, i);
		  }

		  //printf("Hint samples: %d\n", numHintSamples);
		
			/* Get duration of sample */
		  unsigned int frameDuration = MP4GetSampleDuration(mp4, hintId, i);		
		
			/* Get size of sample */
			unsigned int frameSize = MP4GetSampleSize(mp4, hintId, i);

			/* Get sample timestamp */
			unsigned int frameTime = MP4GetSampleTime(mp4, hintId, i);

			printf("%d\t%d\t%d\t%d\t%d\n",i,frameDuration, frameTime,frameSize,frameSize*8/10);
			
			for (j=0;j<numHintSamples;j++)
			{
			  int samples = frameDuration * (90000 / timeScale);
			  int zero = 0;
       	unsigned int ts;			  
			  unsigned short len;
			  int mark;
			  
			  printf(" Packet index %d, samples %d\n",j, samples);

        data = databuffer;

        /* Read next rtp packet */
	       if (!MP4ReadRtpPacket(mp4, hintId, j,
        (u_int8_t **)&data,
				(u_int32_t *)&datalen,
				0, 0,	1)) 
        {
		      printf("Error reading packet [%d,%d]\n", hintId, trackId);
		    }
		    else
		    {
		      //dump_buffer_hex((unsigned char *)name, (unsigned char *)databuffer, datalen);
        }
	
  		  //printf(" Data Header + data length : %d\n", datalen);  		  

        if (j==(numHintSamples-1))
        mark = 0x8000;
        else
        mark = 0;
  
        ts = htonl(samples);
        len = htons(datalen | mark);
  		   
  		  //if (j==0)
 		  	write(outfile, &ts, 4) ;
        //else					
 		  	//write(outfile, &zero, 4) ; 	
	      write(outfile, &len, 2) ;
	      write(outfile, databuffer, datalen) ;

		  }
			
		}
		
	  close(outfile);		
		
		}
		
		/* Get the next hint track */
		hintId = MP4FindTrackId(mp4, index++, MP4_HINT_TRACK_TYPE, 0);		
	}

	/* Close */
	MP4Close(mp4);

	/* End */
	return 0;
}


#if defined (_LITTLE_ENDIAN) || defined (__LITTLE_ENDIAN)

/* 
	Macros to convert from Intel little endian data structures to this machine's
	and vice versa.
 */
#define L2H_SHORT(from, to)  *((short *)(to)) = *((short *)(from))
#define H2L_SHORT(from, to)  *((short *)(to)) = *((short *)(from))
#define L2H_LONG(from, to)   *((long *)(to)) = *((long *)(from))
#define H2L_LONG(from, to)   *((long *)(to)) = *((long *)(from))

/* 
	Macros to convert from Sun big endian data structures to this machine's
	and vice versa.
 */
#define B2H_SHORT(from, to) \
	((char *)(to))[0] = ((char *)(from))[1];	\
	((char *)(to))[1] = ((char *)(from))[0];
#define H2B_SHORT(from, to) \
	((char *)(to))[0] = ((char *)(from))[1];	\
	((char *)(to))[1] = ((char *)(from))[0];

#define B2H_LONG(from, to) \
	((char *)(to))[0] = ((char *)(from))[3];	\
	((char *)(to))[1] = ((char *)(from))[2];	\
	((char *)(to))[2] = ((char *)(from))[1];	\
	((char *)(to))[3] = ((char *)(from))[0];
#define H2B_LONG(from, to) \
	((char *)(to))[0] = ((char *)(from))[3];	\
	((char *)(to))[1] = ((char *)(from))[2];	\
	((char *)(to))[2] = ((char *)(from))[1];	\
	((char *)(to))[3] = ((char *)(from))[0];

#else /* Must be _BIG_ENDIAN */

#define L2H_SHORT(from, to) \
	((char *)(to))[0] = ((char *)(from))[1];	\
	((char *)(to))[1] = ((char *)(from))[0];
#define H2L_SHORT(from, to) \
	((char *)(to))[0] = ((char *)(from))[1];	\
	((char *)(to))[1] = ((char *)(from))[0];

#define L2H_LONG(from, to) \
	((char *)(to))[0] = ((char *)(from))[3];	\
	((char *)(to))[1] = ((char *)(from))[2];	\
	((char *)(to))[2] = ((char *)(from))[1];	\
	((char *)(to))[3] = ((char *)(from))[0];
#define H2L_LONG(from, to) \
	((char *)(to))[0] = ((char *)(from))[3];	\
	((char *)(to))[1] = ((char *)(from))[2];	\
	((char *)(to))[2] = ((char *)(from))[1];	\
	((char *)(to))[3] = ((char *)(from))[0];

#define B2H_SHORT(from, to)  *((short *)(to)) = *((short *)(from))
#define H2B_SHORT(from, to)  *((short *)(to)) = *((short *)(from))

#define B2H_LONG(from, to)   *((long *)(to)) = *((long *)(from))
#define H2B_LONG(from, to)   *((long *)(to)) = *((long *)(from))

#endif /*big-endian*/



int asteriskmp4(char *name)
{
  int index;
  int namelen;
	unsigned char type2; 
	MP4TrackId track_id;
	MP4TrackId hint_id;
	const char *type = NULL;	
 	unsigned short numHintSamples;
 	unsigned short packetIndex;
	  
  int infile;

	char filename[8000];

	int profile_format = 0;
  	
  int loop=1;
  
  char str[4];
	unsigned short packet_size;
	unsigned long packet_delta;
	unsigned long image_delta = 0;
	int mark = 0;
  unsigned char h263_header[4];
	unsigned char h263_data[2*1024];
	unsigned long h263_packet = 0;
	unsigned long h263_size;  
	unsigned long time = 0;
	
	int sample_id = 1;
	
  int buffer_size = 200000;
  u_int8_t *buffer = malloc(buffer_size);
  int buffer_length = 0;

  	
	namelen=strlen(name);
		
	if ((strcmp(name+namelen-5, ".h263")) 
   && (strcmp(name+namelen-5, ".h264")) 
   && (strcmp(name+namelen-6, ".h263p"))) 
	{
		printf("mp4asterisk\nusage: mp4asterisk file\n");
		printf("invalide file format : %s\n", name+namelen-3);
		return -1;
	}

	if (!strcmp(name+namelen-5, ".h263")) 
	{
	  profile_format = 1;
	}
	else
	if (!strcmp(name+namelen-5, ".h263")) 
	{
	  profile_format = 2;
	}
	if (!strcmp(name+namelen-5, ".h264")) 
	{
	  profile_format = 3;
	}
    
	infile = open(name, O_RDONLY, 0);
	if (infile < 0)
	{
		close(infile);
		fprintf(stderr, "Failed to open asterisk input file %s\n", name) ;
		return -1;				
  }	

  strcpy(filename, name);
  strcpy(filename+namelen-5, ".mp4");
  		
	/* Open mp4*/
	//MP4FileHandle mp4 = MP4Create(filename, 9);
	MP4FileHandle mp4 = MP4CreateEx((char *) filename, 9, 0, 1, 1, 0, 0, 0, 0);	
	/* If failed */
	if (mp4 == MP4_INVALID_FILE_HANDLE)
  return -1;

	MP4SetMetadataTool(mp4, "mp4asterisk");	  
	  
	/* Disable Verbosity */
	MP4SetVerbosity(mp4, 0);
	
	if (profile_format == 1)
	{
		int type = 34;

		/* Create video track */
		track_id = MP4AddH263VideoTrack(mp4, 90000, 0, 176, 144, 0, 0, 0, 0);									
		/* Create video hint track */
		hint_id = MP4AddHintTrack(mp4, track_id);
					/* Set payload type for hint track */
		MP4SetHintTrackRtpPayload(mp4, hint_id, "H263", &type, 0, NULL, 1, 0);	
  }
	
	index = 0;
		
	while (loop)
	{
		/* read delta_t */
		if (read(infile, str, 4) <= 0)
		{
			fprintf(stderr, "Failed to read delta_t\n") ;
			loop = 0;
			break ;
		}
				
    B2H_LONG(str, &packet_delta);
 		printf("Video frame delta #%ld:\n", packet_delta);		
 			
 			if (image_delta == 0)
 			image_delta = packet_delta;

		/* read packet size */
		if (read(infile, str, 2) < 0)
		{
			fprintf(stderr, "Failed to read packet size\n") ;
			return -1;
		}
		B2H_SHORT(str, &packet_size);
 		printf("Video frame length #%d:\n", packet_size);		
 			
		if (packet_size & 0x8000) {
			mark = 1;
		}
		else {
			mark = 0;
		}
   	packet_size &= 0x7fff;
	
    /* read h263 header */
		if (read(infile, h263_header, 4) < 0)
		{
			fprintf(stderr, "Failed to read h263 header\n") ;
			return -1;
		}

		/* read video frame */
		h263_size = packet_size - 4;
		if (read(infile, h263_data, h263_size) > 0)
		{
			h263_packet ++;
		}
		else
		{
			fprintf(stderr, "Failed to read video frame\n") ;
			return -1;
		}

		if (mark)
    {
   		printf("Video image delta #%ld:\n", image_delta);		
			time += image_delta;
			//time += (image_delta * 1000)/90000;
			image_delta = 0;
			printf("Video_time = %ld\n", time);
			printf("Video_time = %ld\n", (time)/90);
											
			//break;
		}
				
		if (hint_id != -1)
		{
		  memcpy(buffer+buffer_length, h263_data, h263_size);
	    
		  MP4AddRtpHint(mp4,hint_id);       		
		  MP4AddRtpPacket(mp4, hint_id, mark, 0);
		  MP4AddRtpImmediateData(mp4, hint_id, h263_header, 4);		 
	    MP4AddRtpSampleData(mp4, hint_id, sample_id, buffer_length, h263_size); 

			buffer_length+=h263_size;
		
		  if (mark)
		  {
	      MP4WriteRtpHint(mp4, hint_id, 9000, mark);
	      MP4WriteSample(mp4, track_id, buffer, buffer_length, 9000, 0, mark);
	      buffer_length=0;
        sample_id++;   
	    }

		}
	}	
	
	close(infile);

	/* Close */
	MP4Close(mp4);
	
	return 0;
}

#ifdef DEBUG
static int convert_tags                 = 0;
static int show_value_unit              = 0;
static int use_value_prefix             = 0;
static int use_byte_value_binary_prefix = 0;
static int use_value_sexagesimal_format = 0;

static const char *binary_unit_prefixes [] = { "", "Ki", "Mi", "Gi", "Ti", "Pi" };
static const char *decimal_unit_prefixes[] = { "", "K" , "M" , "G" , "T" , "P"  };

static const char *unit_second_str          = "s"    ;
static const char *unit_hertz_str           = "Hz"   ;
static const char *unit_byte_str            = "byte" ;
static const char *unit_bit_per_second_str  = "bit/s";

static char *value_string(char *buf, int buf_size, double val, const char *unit)
{
    if (unit == unit_second_str && use_value_sexagesimal_format) {
        double secs;
        int hours, mins;
        secs  = val;
        mins  = (int)secs / 60;
        secs  = secs - mins * 60;
        hours = mins / 60;
        mins %= 60;
        snprintf(buf, buf_size, "%d:%02d:%09.6f", hours, mins, secs);
    } else if (use_value_prefix) {
        const char *prefix_string;
        int index;

        if (unit == unit_byte_str && use_byte_value_binary_prefix) {
            index = (int) (log(val)/log(2)) / 10;
            index = av_clip(index, 0, FF_ARRAY_ELEMS(binary_unit_prefixes) -1);
            val /= pow(2, index*10);
            prefix_string = binary_unit_prefixes[index];
        } else {
            index = (int) (log10(val)) / 3;
            index = av_clip(index, 0, FF_ARRAY_ELEMS(decimal_unit_prefixes) -1);
            val /= pow(10, index*3);
            prefix_string = decimal_unit_prefixes[index];
        }

        snprintf(buf, buf_size, "%.3f %s%s", val, prefix_string, show_value_unit ? unit : "");
    } else {
        snprintf(buf, buf_size, "%f %s", val, show_value_unit ? unit : "");
    }

    return buf;
}

static char *time_value_string(char *buf, int buf_size, int64_t val, const AVRational *time_base)
{
    if (val == AV_NOPTS_VALUE) {
        snprintf(buf, buf_size, "N/A");
    } else {
        value_string(buf, buf_size, val * av_q2d(*time_base), unit_second_str);
    }

    return buf;
}

static const char *media_type_string(enum AVMediaType media_type)
{
    switch (media_type) {
    case AVMEDIA_TYPE_VIDEO:      return "video";
    case AVMEDIA_TYPE_AUDIO:      return "audio";
    case AVMEDIA_TYPE_DATA:       return "data";
    case AVMEDIA_TYPE_SUBTITLE:   return "subtitle";
    case AVMEDIA_TYPE_ATTACHMENT: return "attachment";
    default:                      return "unknown";
    }
}

static void show_stream(AVFormatContext *fmt_ctx, int stream_idx)
{
    AVStream *stream = fmt_ctx->streams[stream_idx];
    AVCodecContext *dec_ctx;
    AVCodec *dec;
    char val_str[128];
    AVMetadataTag *tag = NULL;
    AVRational display_aspect_ratio;

    printf("[STREAM]\n");

    printf("index=%d\n",        stream->index);

    if ((dec_ctx = stream->codec)) {
        if ((dec = dec_ctx->codec)) {
            printf("codec_name=%s\n",         dec->name);
            printf("codec_long_name=%s\n",    dec->long_name);
        } else {
            printf("codec_name=unknown\n");
        }

        printf("codec_type=%s\n",         media_type_string(dec_ctx->codec_type));
        printf("codec_time_base=%d/%d\n", dec_ctx->time_base.num, dec_ctx->time_base.den);

        /* print AVI/FourCC tag */        
#if 0        
        av_get_codec_tag_string(val_str, sizeof(val_str), dec_ctx->codec_tag);
#else
        if(   isprint(dec_ctx->codec_tag&0xFF) && isprint((dec_ctx->codec_tag>>8)&0xFF)
           && isprint((dec_ctx->codec_tag>>16)&0xFF) && isprint((dec_ctx->codec_tag>>24)&0xFF)){
            snprintf(val_str, sizeof(val_str), "%c%c%c%c / 0x%04X",
                     dec_ctx->codec_tag & 0xff,
                     (dec_ctx->codec_tag >> 8) & 0xff,
                     (dec_ctx->codec_tag >> 16) & 0xff,
                     (dec_ctx->codec_tag >> 24) & 0xff,
                      dec_ctx->codec_tag);
        } else {
            snprintf(val_str, sizeof(val_str), "0x%04x", dec_ctx->codec_tag);
        }
#endif
        printf("codec_tag_string=%s\n", val_str);
        printf("codec_tag=0x%04x\n", dec_ctx->codec_tag);

        switch (dec_ctx->codec_type) {
        case AVMEDIA_TYPE_VIDEO:
            printf("width=%d\n",                   dec_ctx->width);
            printf("height=%d\n",                  dec_ctx->height);
            printf("has_b_frames=%d\n",            dec_ctx->has_b_frames);
            if (dec_ctx->sample_aspect_ratio.num) {
                printf("sample_aspect_ratio=%d:%d\n", dec_ctx->sample_aspect_ratio.num,
                                                      dec_ctx->sample_aspect_ratio.den);
                av_reduce(&display_aspect_ratio.num, &display_aspect_ratio.den,
                          dec_ctx->width  * dec_ctx->sample_aspect_ratio.num,
                          dec_ctx->height * dec_ctx->sample_aspect_ratio.den,
                          1024*1024);
                printf("display_aspect_ratio=%d:%d\n", display_aspect_ratio.num,
                                                       display_aspect_ratio.den);
            }
            printf("pix_fmt=%s\n",                 dec_ctx->pix_fmt != PIX_FMT_NONE ?
                   av_pix_fmt_descriptors[dec_ctx->pix_fmt].name : "unknown");
            break;

        case AVMEDIA_TYPE_AUDIO:
            printf("sample_rate=%s\n",             value_string(val_str, sizeof(val_str),
                                                                dec_ctx->sample_rate,
                                                                unit_hertz_str));
            printf("channels=%d\n",                dec_ctx->channels);
            printf("bits_per_sample=%d\n",         av_get_bits_per_sample(dec_ctx->codec_id));
            break;
        }
    } else {
        printf("codec_type=unknown\n");
    }

    if (fmt_ctx->iformat->flags & AVFMT_SHOW_IDS)
        printf("id=0x%x\n", stream->id);
    printf("r_frame_rate=%d/%d\n",         stream->r_frame_rate.num,   stream->r_frame_rate.den);
    printf("avg_frame_rate=%d/%d\n",       stream->avg_frame_rate.num, stream->avg_frame_rate.den);
    printf("time_base=%d/%d\n",            stream->time_base.num,      stream->time_base.den);
    if (stream->language[0])
        printf("language=%s\n",            stream->language);
    printf("start_time=%s\n",   time_value_string(val_str, sizeof(val_str), stream->start_time,
                                                  &stream->time_base));
    printf("duration=%s\n",     time_value_string(val_str, sizeof(val_str), stream->duration,
                                                  &stream->time_base));
    if (stream->nb_frames)
        printf("nb_frames=%"PRId64"\n",    stream->nb_frames);

    while ((tag = av_metadata_get(stream->metadata, "", tag, AV_METADATA_IGNORE_SUFFIX)))
        printf("TAG:%s=%s\n", tag->key, tag->value);

    printf("[/STREAM]\n");
}

static void show_format(AVFormatContext *fmt_ctx)
{
    AVMetadataTag *tag = NULL;
    char val_str[128];

    printf("[FORMAT]\n");

    printf("filename=%s\n",         fmt_ctx->filename);
    printf("nb_streams=%d\n",       fmt_ctx->nb_streams);
    printf("format_name=%s\n",      fmt_ctx->iformat->name);
    printf("format_long_name=%s\n", fmt_ctx->iformat->long_name);
    printf("start_time=%s\n",       time_value_string(val_str, sizeof(val_str), fmt_ctx->start_time,
                                                      &AV_TIME_BASE_Q));
    printf("duration=%s\n",         time_value_string(val_str, sizeof(val_str), fmt_ctx->duration,
                                                      &AV_TIME_BASE_Q));
    printf("size=%s\n",             value_string(val_str, sizeof(val_str), fmt_ctx->file_size,
                                                 unit_byte_str));
    printf("bit_rate=%s\n",         value_string(val_str, sizeof(val_str), fmt_ctx->bit_rate,
                                                 unit_bit_per_second_str));

    if (convert_tags)
        av_metadata_conv(fmt_ctx, NULL, fmt_ctx->iformat->metadata_conv);
    while ((tag = av_metadata_get(fmt_ctx->metadata, "", tag, AV_METADATA_IGNORE_SUFFIX)))
        printf("TAG:%s=%s\n", tag->key, tag->value);

    printf("[/FORMAT]\n");
}

void dump_frame(AVFrame *frame) {
    printf( "Frame info:\n"
                "\tIs key frame = %s, "
                "type = %c, "
                "presentation timestamp = %0.3f\n", 
                frame->key_frame == 1 ? "Yes" : "No",
                av_get_pict_type_char(frame->pict_type),
                (double)frame->pts / AV_TIME_BASE);
}

static void pgm_save(unsigned char *buf, int wrap, int xsize, int ysize, char *filename)
 {
     FILE *f;
     int i;
 
     f=fopen(filename,"w");
     fprintf(f,"P5\n%d %d\n%d\n",xsize,ysize,255);
     for(i=0;i<ysize;i++)
         fwrite(buf + i * wrap,1,xsize,f);
     fclose(f);
 }
#endif

int ffasterisk(char *filename, char *format)
{
  AVFormatContext *format_context=NULL;
	//AVFormatParameters params;	
  AVCodecContext *audio_codec_context=NULL;
  AVCodecContext *video_codec_context=NULL;
  AVCodecContext *encoder_codec_context=NULL;
  AVCodec *audio_codec=NULL;
  AVCodec *video_codec=NULL;
  AVCodec *encoder_codec=NULL;
  AVAudioConvert *audio_convert=NULL;
	AVPacket packet;
	
	ReSampleContext *audio_resample_context;
	
	struct SwsContext* resize_context;

  AVFrame *frame=NULL;	
  AVFrame *encoder_frame=NULL;	
  int i, err, result = 0;
  
  int video_index = -1;
	int video_found=0;  

  int audio_index = -1;
	int audio_found=0;  

  uint16_t *audio_buffer;
  int audio_buffer_size;

  uint8_t *encoder_buffer;
  int encoder_buffer_size;
  int encoder_length;

  DECLARE_ALIGNED(16,uint8_t,audio_buffer1)[(AVCODEC_MAX_AUDIO_FRAME_SIZE * 3) / 2];
  DECLARE_ALIGNED(16,uint8_t,audio_buffer2)[(AVCODEC_MAX_AUDIO_FRAME_SIZE * 3) / 2];

	uint8_t	*picture=NULL;	
  uint8_t	*encoder_picture=NULL;
	
  FILE *audio_outfile = NULL;
  FILE *video_outfile = NULL;
  FILE *outfile = NULL;

	int profile_width 	= 176;
	int profile_heigth 	= 144;
	int profile_fps	= 7;
	int profile_bitrate 	= 40*1024;
	int profile_qmin	= 4;
	int profile_qmax	= 8;
	int profile_gop_size	= 10;
	int profile_flip = 0;
	int profile_mirror = 0;
	int profile_format = 0;
	
	uint32_t obsolute_ts;
  uint32_t old_ts = 0;

	char *p;
	char *extension = "raw";

	char output_filename[8000];
	int namelen;
	  	
  strcpy(output_filename, filename);    	
	namelen=strlen(filename);		
	
	if (strncasecmp(format,"h263",4) && strncasecmp(format,"h264",4))
	{
		fprintf(stderr,"Only h263 or h264 output by now\n");
		return -1;
	}
	
	p = strchr(format,'@');
	while (p)
	{
		/* skip separator */
		p++;

		/* compare */
		if (strncasecmp(p,"qcif",4)==0)
		{
			/* Set qcif */
      profile_width = 176;
	    profile_heigth = 144;
		} else if (strncasecmp(p,"cif",3)==0) {
			/* Set cif */
      profile_width = 352;
	    profile_heigth = 288;
		} else if (strncasecmp(p,"fps=",4)==0) {
			/* Set fps */
			profile_fps = atoi(p+4);
		} else if (strncasecmp(p,"kb=",3)==0) {
			/* Set bitrate */
			profile_bitrate = atoi(p+3)*1024;
		} else if (strncasecmp(p,"qmin=",5)==0) {
			/* Set qMin */
			profile_qmin = atoi(p+5);
		} else if (strncasecmp(p,"qmax=",5)==0) {
			/* Set qMax */
			profile_qmax = atoi(p+5);
		} else if (strncasecmp(p,"gs=",3)==0) {
			/* Set gop size */
			profile_gop_size = atoi(p+3);
		} else if (strncasecmp(p,"flip",4)==0) {
			/* Flip the image */
			profile_flip = 1;
		} else if (strncasecmp(p,"mirror",4)==0) {
			/* Flip the image */
			profile_mirror = 1;
		} else if (strncasecmp(p,"rotate",4)==0) {
			/* Flip the image */
			profile_flip = 1;
			profile_mirror = 1;			
		}		

		/* Find next param*/
		p = strchr(p,'/');
	}

	printf("Video profile [%dx%d,fps=%d,kb=%d,qmin=%d,qmax=%d,gs=%d]\n",
  profile_width, profile_heigth,
  profile_fps,profile_bitrate,profile_qmin,profile_qmax,profile_gop_size);
	
  avcodec_init();
	av_register_all();
	
	err = av_open_input_file(&format_context, filename, NULL, 0, NULL);	
	if(err<0){
		fprintf(stderr,"Can't open file: %s\n", filename);
		return -1;
	}	

#ifdef DEBUG
  show_format(format_context);
#endif

  // Find Video stream
	{
		// Find the stream info.
		av_find_stream_info(format_context);

		// Find the first supported codec in the video streams.
		for(i=0; i<format_context->nb_streams; i++){
			video_codec_context=format_context->streams[i]->codec;
			//video_codec_context->pix_fmt 	= PIX_FMT_YUV420P;
			if(video_codec_context->codec_type==CODEC_TYPE_VIDEO) {
				/* Found a video stream, check if codec is supported */
				video_codec = avcodec_find_decoder(video_codec_context->codec_id);
				if(video_codec) {
					// codec is supported, proceed
					video_index=i;
					video_found=1;
					break;
				}
			}
		}
	}

#ifdef DEBUG
  if (video_found)
  show_stream(format_context, video_index);
#endif

  // Find Audio stream
	{	
		// Find the stream info.
		av_find_stream_info(format_context);

		/* Find the first supported codec in the audio streams */
		for(i=0; i<format_context->nb_streams; i++){
			audio_codec_context=format_context->streams[i]->codec;
			if(audio_codec_context->codec_type==CODEC_TYPE_AUDIO) {
				/* Found an audio stream, check if codec is supported */
				audio_codec = avcodec_find_decoder(audio_codec_context->codec_id);
				if(audio_codec){
					/* codec is supported, proceed */
					audio_index = i;
					audio_found=1;
					break;
				}
			}
		}
	}
	
#ifdef DEBUG
  if (audio_found)
  show_stream(format_context, audio_index);
#endif	

	/* Find audio codec id */
	if(audio_index < 0) {
	 	printf("Audio stream with supported codec not found.\n");
	}
	else {
		/* open it */
		if (avcodec_open(audio_codec_context, audio_codec) < 0) {
			fprintf(stderr, "could not open audio codec\n");
		}
	}

	/* Find video codec id */
	if(video_index < 0) {
	 	printf("Video stream with supported codec not found.\n");
	}
	else {
		/* open it */
		if (avcodec_open(video_codec_context, video_codec) < 0) {
			fprintf(stderr, "could not open video codec\n");
		}
	}


  frame = avcodec_alloc_frame();
  encoder_frame = avcodec_alloc_frame();
  
  audio_buffer_size = 10000;
  audio_buffer = malloc(audio_buffer_size*sizeof(uint16_t));

  encoder_buffer_size = 200000;
  encoder_buffer = malloc(encoder_buffer_size);
  
	/* Malloc pictures */
	picture = (uint8_t *)malloc(1179648); /* Max YUV 1024x768 */
  encoder_picture = (uint8_t *)malloc(1179648); /* Max YUV 1024x768 */
    
  if (0)
  if (audio_codec_context->sample_fmt != SAMPLE_FMT_S16)
  {
    audio_convert = av_audio_convert_alloc(SAMPLE_FMT_S16, 1, audio_codec_context->sample_fmt, 1, NULL, 0);
    if (!audio_convert) {
      fprintf(stderr, "Cannot convert %s sample format to %s sample format\n",
        avcodec_get_sample_fmt_name(audio_codec_context->sample_fmt),
        avcodec_get_sample_fmt_name(SAMPLE_FMT_S16));
    }
  }

  audio_resample_context = audio_resample_init(1, audio_codec_context->channels,
    8000, audio_codec_context->sample_rate);   
  
  encoder_codec_context = avcodec_alloc_context();
  if (encoder_codec_context)
  {
	  /* Find encoder */
	  if (!strncasecmp(format,"h263sorenson",12))
    {
      /* H263 Sorenson */	
		  encoder_codec = avcodec_find_encoder(CODEC_ID_FLV1); 
		  if (!encoder_codec) 
      fprintf(stderr, "SORENSON encoder codec not found\n");
      
      extension = "h263";  
      profile_format = 2;
    }
    else
    if (!strncasecmp(format,"h264",4)) 
    {	
		  /* H264 encoder */
		  encoder_codec = avcodec_find_encoder(CODEC_ID_H264); 
		  if (!encoder_codec) 
      fprintf(stderr, "H264 encoder codec not found\n");	  
		  /* Add x4->params.i_slice_max_size     = 1350; 
           x4->params.rc.i_lookahead       = 0; 		
           in X264_init function of in libavcodec/libx264.c */
		  /* Fast encodinf parameters */
		  encoder_codec_context->refs = 1;
		  encoder_codec_context->scenechange_threshold = 0;
		  encoder_codec_context->me_subpel_quality = 0;
		  encoder_codec_context->partitions = X264_PART_I8X8 | X264_PART_I8X8;
		  encoder_codec_context->me_method = ME_EPZS;
		  encoder_codec_context->trellis = 0;
		
	    encoder_codec_context->me_range 	    = 24;
	    encoder_codec_context->max_qdiff 	    = 31;
      encoder_codec_context->i_quant_factor = (float)-0.6;
      encoder_codec_context->i_quant_offset = (float)0.0;
	    encoder_codec_context->qcompress	    = 0.6;		

      extension = "h264"; 
      profile_format = 3;       
	  }	  
	  else
	  {
		  /* H263 encoder */
		  encoder_codec = avcodec_find_encoder(CODEC_ID_H263); 
		  if (!encoder_codec) 
      fprintf(stderr, "H263 encoder codec not found\n");
      
		  /* Flags */
		  encoder_codec_context->mb_decision = FF_MB_DECISION_SIMPLE;
		  encoder_codec_context->flags |= CODEC_FLAG_PASS1;                 //PASS1
		  encoder_codec_context->flags &= ~CODEC_FLAG_H263P_UMV;            //unrestricted motion vector
		  encoder_codec_context->flags &= ~CODEC_FLAG_4MV;                  //advanced prediction
		  // Sergio : vtc->encoderCtx->flags &= ~CODEC_FLAG_H263P_SLICE_STRUCT;
      encoder_codec_context->flags |= CODEC_FLAG_H263P_SLICE_STRUCT;

      extension = "h263";  
      profile_format = 1;      
    } 
    
    /* Picture data */
	  encoder_codec_context->pix_fmt 	= PIX_FMT_YUV420P; //video_codec_context->pix_fmt;
	  encoder_codec_context->width		= profile_width;
	  encoder_codec_context->height 	= profile_heigth;
	  
	  /* fps*/
    encoder_codec_context->time_base= (AVRational){1,profile_fps};/* frames per second */	  
	
		/* bit_rate */
		encoder_codec_context->bit_rate = profile_bitrate;
		encoder_codec_context->rc_min_rate = encoder_codec_context->bit_rate;
		encoder_codec_context->rc_max_rate = encoder_codec_context->bit_rate;		
    encoder_codec_context->bit_rate_tolerance = encoder_codec_context->bit_rate*av_q2d(encoder_codec_context->time_base) + 1;

	  /* gop size */
    encoder_codec_context->gop_size = profile_gop_size;
                  	
		encoder_codec_context->mb_qmin = encoder_codec_context->qmin= profile_qmin;
		encoder_codec_context->mb_qmax = encoder_codec_context->qmax= profile_qmax;

    /* Video quality */
    encoder_codec_context->rc_buffer_size     = 100000;
    encoder_codec_context->rc_qsquish         = 0; //ratecontrol qmin qmax limiting method.
    encoder_codec_context->max_b_frames       = 0;
            
    if (avcodec_open(encoder_codec_context, encoder_codec) != -1)
    {
    }     
    else
    {
      encoder_codec_context = NULL;
    }
  }

  resize_context = sws_getContext(video_codec_context->width, video_codec_context->height, video_codec_context->pix_fmt, encoder_codec_context->width, encoder_codec_context->height, encoder_codec_context->pix_fmt, SWS_BICUBIC, NULL, NULL, NULL);


  strcpy(output_filename+namelen-3, extension);  

  outfile = fopen(output_filename, "wb");
                                                                                                   
  //audio_outfile = fopen("/tmp/audio.raw", "wb");
  //video_outfile = fopen("/tmp/video.raw", "wb");

	/* Decode loop */
	while(result == 0) {
	   int packet_pts;
	   int got_picture;
	   int len;
	
		// Read a frame/packet
		if(av_read_frame(format_context, &packet) < 0 ) 
    {
     //printf("Error reading frame.\n");
     break;
    }
    
    //av_pkt_dump_log(NULL, AV_LOG_DEBUG, &packet, 1);
   
    if(packet.stream_index==audio_index){
      //printf("Read audio packet.\n");

      packet_pts = av_rescale_q(packet.pts, format_context->streams[audio_index]->time_base, AV_TIME_BASE_Q);
      //printf("pts=%d us.\n", packet_pts);

      avcodec_get_frame_defaults(frame);
      audio_buffer_size = sizeof(audio_buffer1);
      //printf("audio_buffer_size=%d.\n", audio_buffer_size);
			len = avcodec_decode_audio3(audio_codec_context, (int16_t *)audio_buffer1, &audio_buffer_size, &packet);
      //printf("audio_buffer_size=%d.\n", audio_buffer_size);
      //printf("len=%d.\n", len);
      
      //printf("audio_buffer_size=%d.\n", audio_buffer_size);
      
      if (audio_convert)
      {
        const void *ibuf[6]= {audio_buffer1};
        void *obuf[6]= {audio_buffer2};
        int istride[6]= {av_get_bits_per_sample_format(audio_codec_context->sample_fmt)/8};
        int ostride[6]= {2};
        int len= audio_buffer_size/istride[0];
        
        //printf("av_audio_convert() failed\n");
                            
        if (av_audio_convert(audio_convert, obuf, ostride, ibuf, istride, len)<0) {
                    printf("av_audio_convert() failed\n");
                    break;
        }
        
        /* FIXME: existing code assume that data_size equals framesize*channels*2
                          remove this legacy cruft */
        //audio_buffer_size= len*2;
        memcpy(audio_buffer1, audio_buffer2, audio_buffer_size);
        
        //printf("audio_buffer_size(convert)=%d.\n", audio_buffer_size);
      }            

      if (audio_resample_context) {
        audio_buffer_size = audio_resample(audio_resample_context,
                                  (short *)audio_buffer2, (short *)audio_buffer1,
                                  audio_buffer_size/(2*audio_codec_context->channels));
        audio_buffer_size = audio_buffer_size * 2;
        memcpy(audio_buffer1, audio_buffer2, audio_buffer_size);
        
        //printf("audio_buffer_size(resample)=%d.\n", audio_buffer_size);
    }
      
      if (audio_buffer_size > 0) 
      {
       /* if a frame has been decoded, output it */
       if (audio_outfile)
       fwrite(audio_buffer1, 1, audio_buffer_size, audio_outfile);
      }
    }
    
    if(packet.stream_index==video_index){
      //printf("Read video packet.\n");
      
      packet_pts = av_rescale_q(packet.pts, format_context->streams[video_index]->time_base, AV_TIME_BASE_Q);
      printf("pts=%d us (%d).\n", packet_pts, packet.pts);
      
      //avcodec_get_frame_defaults(frame);
			len = avcodec_decode_video2(video_codec_context, frame, &got_picture, &packet);
      //printf("len=%d.\n", len);
      //printf("got_picture=%d.\n", got_picture);
      
      //dump_frame(frame);
      
      if (got_picture)
      if (encoder_codec_context)
      {      
         int	encoder_linesize[3];
         uint8_t* encoder_data[3];
        
         encoder_linesize[0] = encoder_codec_context->width;
	       encoder_linesize[1] = encoder_codec_context->width/2;
	       encoder_linesize[2] = encoder_codec_context->width/2;
	       
	       encoder_data[0] = encoder_picture;
				 encoder_data[1] = encoder_picture+(encoder_codec_context->width*encoder_codec_context->height);
				 encoder_data[2] = encoder_picture+(encoder_codec_context->width*encoder_codec_context->height)*5/4;
					
         //pgm_save(frame->data[0], frame->linesize[0], video_codec_context->width, video_codec_context->height, "/tmp/image.pgm");
         
         avcodec_get_frame_defaults(encoder_frame);
               
         sws_scale(resize_context, frame->data, frame->linesize, 0, video_codec_context->height, encoder_data, encoder_linesize);
         
         encoder_frame->data[0] = encoder_data[0];
			   encoder_frame->data[1] = encoder_data[1];
			   encoder_frame->data[2] = encoder_data[2];
			   encoder_frame->linesize[0] = encoder_linesize[0];
			   encoder_frame->linesize[1] = encoder_linesize[1];
			   encoder_frame->linesize[2] = encoder_linesize[2];
         
         encoder_length = avcodec_encode_video(encoder_codec_context, encoder_buffer, encoder_buffer_size, encoder_frame);
         /* if a frame has been decoded, output it */
         //printf("len=%d.\n", encoder_length);
         
         if (video_outfile)
         fwrite(encoder_buffer, 1, encoder_length, video_outfile); 
         
         if (profile_format == 1) // h263 RFC2190    
         { 
 	         unsigned char RFC2190_header[4] = {0} ;  
           uint32_t *p = NULL;
           int gob_num = 0;             
           char *dat = NULL;
           int sent = 0;	  
           int last = 0;
           int data_size = 0;                          
         
           static uint32_t tr = 0; //Static to have it when needed for splitting into multiple
           static uint32_t sz = 0; //packets 
           
           uint32_t packet_ts;
           uint16_t packet_size;
           
           int maxsize = 1400;
          
           obsolute_ts = (packet_pts*9)/100;  
           
           while(sent<encoder_length)
           {
				     /* Check remaining */
				     if (sent+maxsize>encoder_length)
				     {
					     /* last */
					     last = 1;
					     /* send the rest */
					     data_size = encoder_length-sent;
				     } else 
					   /* Fill */
					   data_size = maxsize;                      
           
             p = (uint32_t *)(encoder_buffer+sent);
             gob_num = (ntohl(*p) >> 10) & 0x1f;             
             dat = (char *)(encoder_buffer+sent);
           
             if(gob_num == 0)
             {
               /* Get relevant framedata and memorize it for later use */
               /* Get the "temporal reference" from the H.263 frameheader. */
               tr = ((dat[2] & 0x03) * 64) + ((dat[3] & 0xfc) / 4);
               /* Get the Imgesize from the H.263 frameheader. */
               sz = (dat[4] >> 2) & 0x07;
             }
             else
             {
               /* The memorized values from the frame start will be used */
             }
  
             /* Construct payload header.
                Set videosize and the temporal reference to that of the frame */
             ((uint32_t *)RFC2190_header)[0] = ntohl((sz << 21) | (tr & 0x000000ff));

             if (last == 1)
             {
               if (obsolute_ts < old_ts)
               packet_ts = htonl(0);
               else
               {
                 packet_ts = htonl(obsolute_ts - old_ts);                             
                 old_ts = obsolute_ts;   
               }
             }
             else
             packet_ts = htonl(0);
                          
             //packet_ts = htonl(ts-ts_old);	 
             //packet_ts = htonl(90000/);
             //ts_old = ts;
  
             if (last == 1)
             packet_size = htons((data_size+4) | 0x8000) ;
             else
             packet_size = htons(data_size+4);

             //printf("pts=%d.\n", data_size);         
  
             fwrite(&packet_ts, 1, 4, outfile); 
             fwrite(&packet_size, 1, 2, outfile); 
             fwrite(RFC2190_header, 1, 4, outfile); 
             fwrite(dat, 1, data_size, outfile);  
             
             sent += data_size;                     
           }
         }         
         else          
         if (profile_format == 2) // h263sorenson    
         { 
 	         unsigned char Sorenson_header[30] = {0} ;
           uint32_t *p = NULL;
           int gob_num = 0;             
           char *dat = NULL;
           int sent = 0;	  
           int last = 0;
           int data_size = 0;                          
                    
           uint16_t cseq = 0;
           static uint32_t txseq = 0;
           
 	         uint32_t msgtype = 9;
 	         uint32_t msgsize = encoder_length;
 	         //uint32_t msgtime = ((90000/10) * txseq) / 1000; // For 10 fps           
 	         uint32_t msgtime = 0; //packet_pts / 1000; // in ms           
                      
           uint32_t packet_ts;
           uint16_t packet_size;
           
           int maxsize = 1400;

           printf("pts=%d %d.\n", txseq, msgtime);
                 
           /* Construct payload header.
              Set videosize and the temporal reference to that of the frame */
           Sorenson_header[0] = 'R';
           Sorenson_header[1] = 'T';
           Sorenson_header[2] = 'M';
           Sorenson_header[3] = 'P';
           
           *((uint32_t *)(&Sorenson_header[4]))=htonl(txseq);
           *((uint16_t *)(&Sorenson_header[8]))=htons(cseq);
           *((uint16_t *)(&Sorenson_header[10]))=htons((uint16_t)encoder_length+13);
  
           *((uint32_t *)(&Sorenson_header[12]))=htonl(msgtype);
           *((uint32_t *)(&Sorenson_header[16]))=htonl(msgsize+1);
           *((uint32_t *)(&Sorenson_header[20]))=htonl(msgtime);
           Sorenson_header[24]=0x12;           
            
           obsolute_ts = (packet_pts*9)/100;  
           
           printf("pts * abs =%d.\n", obsolute_ts);
           printf("pts * old =%d.\n", old_ts);
           printf("pts * =%d.\n",  obsolute_ts - old_ts);
                       
           while(sent<encoder_length)
           {
				     /* Check remaining */
				     if (sent+maxsize>encoder_length)
				     {
					     /* last */
					     last = 1;
					     /* send the rest */
					     data_size = encoder_length-sent;
				     } else 
					   /* Fill */
					   data_size = maxsize;                      
           
             p = (uint32_t *)(encoder_buffer+sent);
             dat = (char *)(encoder_buffer+sent); 
                    
               
             if (last == 1)
             {
               if (obsolute_ts < old_ts)
               packet_ts = htonl(0);
               else
               {
                 packet_ts = htonl(obsolute_ts - old_ts);                             
                 old_ts = obsolute_ts;   
               }
             }
             else
             packet_ts = htonl(0);
             
             if (cseq == 0)
             packet_size = data_size + 25;
             else
             packet_size = data_size + 10;              
  
             if (last == 1)
             packet_size = htons((packet_size) | 0x8000) ;
             else
             packet_size = htons(packet_size);                       

             //printf("pts=%d.\n", data_size);         
  
             fwrite(&packet_ts, 1, 4, outfile); 
             fwrite(&packet_size, 1, 2, outfile);
             if (cseq == 0) 
             fwrite(Sorenson_header, 1, 25, outfile); 
             else
             fwrite(Sorenson_header, 1, 10, outfile); 
             fwrite(dat, 1, data_size, outfile);  
             
             cseq++;
             *((uint16_t *)(&Sorenson_header[8]))=htons(cseq);
             
             sent += data_size;                     
           }
           
           txseq++;                  
         }         
         else          
         if (outfile)
         fwrite(encoder_buffer, 1, encoder_length, outfile); 
      }
    }
    
    if (&packet != NULL)
    av_free_packet(&packet);
  }

	/* Free pictures */
	if (picture)
	free(picture);

	if (encoder_picture)
	free(encoder_picture);

	/* if got contex */
	if (resize_context!=NULL)
	sws_freeContext(resize_context);


  if (audio_resample_context!=NULL)
  audio_resample_close(audio_resample_context);

  if (audio_convert!=NULL)
  av_audio_convert_free(audio_convert);

  if (audio_outfile!=NULL)
  fclose(audio_outfile);

  if (video_outfile!=NULL)
  fclose(video_outfile);

  if (audio_buffer != NULL)
  free(audio_buffer);

  if(frame != NULL)
  av_free(frame);  

	if (audio_codec_context != NULL)    
  {  
    avcodec_close(audio_codec_context);
    //av_free(audio_codec_context);
  }

	if (video_codec_context != NULL)
  {      
    avcodec_close(video_codec_context);
    //av_free(video_codec_context);
  }
  
  if (encoder_codec_context != NULL)      
  {
    avcodec_close(encoder_codec_context);
    //av_free(encoder_codec_context);
  }
	
	if (format_context != NULL)
  av_close_input_file(format_context);
  
  return 0;
}


int main(int argc,char **argv)
{
  int index;
	char *name;  
  int namelen;
	unsigned char type2; 
	MP4TrackId hintId;
	MP4TrackId trackId;
	const char *type = NULL;	
 	unsigned short numHintSamples;
 	unsigned short packetIndex;
	
	u_int32_t datalen;
	u_int8_t databuffer[8000];
  uint8_t* data;	
  
  int outfile;

	char filename[8000];

	char *videoprofile="h263@qcif/fps=7/kb=40/qmin=4/qmax=8/gs=10";  
  	
	/* Check args */
	if (argc<2)
	{
		printf("mp4asterisk2\nusage: mp4asterisk2 file [video profile]\n");
		return -1;
	}
	
	name=argv[1];
	namelen=strlen(name);
	
	if (argc>2)
	videoprofile=argv[2];
	
	if (strlen(name) < namelen)
	{
		printf("mp4asterisk2\nusage: mp4asterisk2 file\n");
		return -1;
	}
	
	if ((!strcmp(name+namelen-4, "h263")) || (!strcmp(name+namelen-4, "h263p"))|| (!strcmp(name+namelen-4, "h264"))) 
	{
		return asteriskmp4(name);
	}
	else
	if (!strcmp(name+namelen-3, "mov")) 
	{
		return mp4asterisk(name);
	}
	else
	if ((!strcmp(name+namelen-3, "mp4")) || (!strcmp(name+namelen-3, "3gp"))) 
	{
		return mp4asterisk(name);
	}
	else
	if ((!strcmp(name+namelen-3, "avi")) || (!strcmp(name+namelen-3, "flv")) || (!strcmp(name+namelen-3, "mpg"))) 
	{
		return ffasterisk(name, videoprofile);
	}
	else
	{
		printf("mp4asterisk\nusage: mp4asterisk file\n");
		printf("invalide file format : %s\n", name+namelen-3);
		return -1;
	}  		

	/* End */
	return 0;
}
