
ASTERISK_INCLUDE_DIR := $(ASTERISKDIR)/include

RPATH := -Wl,-rpath=/usr/local/lib/video_tools/

LDFLAGS = -L$(MPEG4IPDIR)/lib/mp4/.libs -L$(MPEG4IPDIR)/lib/mp4v2/.libs -lmp4 -lmp4v2 
LDFFMPEG = -L$(VBROWSERDIR)/usr/lib -L$(VBROWSERDIR)/usr/local/lib \
           -lavcodec -lavformat -lavutil -lswscale -lx264
CFLAGS =  -I$(ASTERISK_INCLUDE_DIR) -I$(MPEG4IPDIR) -I$(MPEG4IPDIR)/include -I$(MPEG4IPDIR)/lib/mp4 -I$(MPEG4IPDIR)/lib/mp4v2 -I/usr/local/include
CFFMPEG = -DDEBUG=1 -I$(FFMPEGDIR) -I$(FFMPEGDIR)/libavcodec -I$(FFMPEGDIR)/libavformat -I$(FFMPEGDIR)/libswscale

%.o: %.c
	gcc $(CFLAGS) $(CFFMPEG) -c -o $@ $<
%.o: %.cpp
	gcc $(CFLAGS) $(CFFMPEG) -c -o $@ $<

all: pcm2mp4 mp4band mp4asterisk audiomark mp4asterisk2

pcm2mp4: pcm2mp4.o
	gcc -o pcm2mp4 pcm2mp4.o $(LDFLAGS) $(RPATH)

mp4band: mp4band.o
	gcc -o mp4band mp4band.o $(LDFLAGS) $(RPATH)

mp4asterisk: mp4asterisk.o
	gcc -o mp4asterisk mp4asterisk.o $(LDFLAGS) $(RPATH)
	
audiomark: audiomark.o
	gcc -o audiomark audiomark.o

mp4asterisk2: mp4asterisk2.o
	gcc -o mp4asterisk2 mp4asterisk2.o $(LDFLAGS) $(LDFFMPEG) $(RPATH)

clean:
	rm -f pcm2mp4 mp4band mp4asterisk audiomark mp4asterisk2 *.o
