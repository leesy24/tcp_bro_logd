CROSS		= /opt/lemonix/cdt/bin/arm-linux-
LIB_DIRS	= -L/opt/lemonix/cdt/lib -L/opt/lemonix/cdt/bin
INC_DIRS	= -I../include
DEST		= ../../../ramdisk/root/sbin	
DEST_ETC	= ../../../ramdisk/root/etc

LIBS		= -lrt ../SB_APIs/SB_APIs.a

CC		= $(CROSS)gcc
STRIP	= $(CROSS)strip
AR		= $(CROSS)ar
CFLAGS	= -O2 -g -Wall -Wno-nonnull $(INC_DIRS)

TARGETS	= tcp_bro_logd

all : $(TARGETS)

tcp_bro_logd : tcp_bro_logd.o 
	rm -f $@
	$(CC) $(LIB_DIRS) -o $@  $@.o  $(LIBS) 
	$(STRIP) $@

clean: 
	rm -f *.bak *.o


release:
	cp  -f $(TARGETS) $(DEST)

