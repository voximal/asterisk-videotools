# asterisk-videotools

The first step is to download the source code for the components needed.
The external software used is:
 mpeg4ip version 1.5.0.1
 ffmpeg (snapshot) revision, need to port the last one
 libtool library
 patch

Library Installation

MPEG4IP

To configure, compile and install the mpeg4ip libraries run the following script in the source directory.
Script:

#
# Compiling Mpeg4IP
#
cd mpeg4ip-1.5.0.1/
./bootstrap
./configure --disable-warns-as-err --disable-server --disable-player --enable-shared
make
cd ./server/mp4creator
make
