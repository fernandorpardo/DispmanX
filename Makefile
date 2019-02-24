# SDKSTAGE needs to point to where you have installed 'firmware'
SDKSTAGE=/home/pi/programs/firmware.d

# Use g++ rather than c compiler
CC= g++ -std=c++0x
OBJS=fox.o
BIN=fox

CFLAGS+=-DSTANDALONE -D__STDC_CONSTANT_MACROS -D__STDC_LIMIT_MACROS -DTARGET_POSIX -D_LINUX -fPIC -DPIC -D_REENTRANT -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -U_FORTIFY_SOURCE -Wall -g -DHAVE_LIBOPENMAX=2 -DOMX -DOMX_SKIP64BIT -ftree-vectorize -pipe -DUSE_EXTERNAL_OMX -DHAVE_LIBBCM_HOST -DUSE_EXTERNAL_LIBBCM_HOST -DUSE_VCHIQ_ARM -Wno-psabi
LDFLAGS+=-L$(SDKSTAGE)/opt/vc/lib/ -lbrcmGLESv2 -lbrcmEGL -lopenmaxil -lbcm_host -lvcos -lvchiq_arm -lpthread -lrt -lm -L$(SDKSTAGE)/opt/vc/src/hello_pi/libs/ilclient -L$(SDKSTAGE)/opt/vc/src/hello_pi/libs/vgfont
INCLUDES+=-I$(SDKSTAGE)/opt/vc/include/ -I$(SDKSTAGE)/opt/vc/include/interface/vcos/pthreads -I$(SDKSTAGE)/opt/vc/include/interface/vmcs_host/linux -I./ -I$(SDKSTAGE)/opt/vc/src/hello_pi/libs/ilclient -I$(SDKSTAGE)/opt/vc/src/hello_pi/libs/vgfont

all: fox

%.o: %.cpp
	@rm -f $@ 
	$(CC) $(CFLAGS) $(INCLUDES) -g -c $< -o $@ -Wno-deprecated-declarations

fox: $(OBJS)
	$(CC) -o $(BIN) -Wl,--whole-archive $(OBJS) $(LDFLAGS) -Wl,--no-whole-archive -rdynamic
	mv fox ~/bin

clean:
	for i in $(OBJS); do (if test -e "$$i"; then ( rm $$i ); fi ); done
	@rm -f $(BIN) 
	@rm -f ~/bin/$(BIN)


