include ./ofpkFlags.mk

#export OFPK_ROOT_DIR=/home/le/FPS
 
OFP_IFLAGS += -I$(OFPK_ROOT_DIR)/ofpk_schedule/inc 

CFLAGS += $(OFP_CFLAGS)
CFLAGS += $(OFP_IFLAGS)

LIBS += $(OFP_LLIBS)
LIBS += $(OFP_LIBS)

src_dirs = $(OFPK_ROOT_DIR)/ofpk_schedule/src

objects  = ofpk_common.o \
		   ofpk_paras.o \
		   ofpk_socket.o \
		   ofpk_socket_tcp.o \
		   ofpk_socket_udp.o \
		   ofpk_udp_fwd.o \
		   ofpk_wrapper.o \
		   ofpk_main.o \

targets = ofpk_udp_socket
	
.PHONY: all clean distclean

all:$(objects) $(targets)

$(objects):%.o: $(src_dirs)/%.c
	$(CC) -c -o $@ $< $(CFLAGS)
	
$(targets): $(objects)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS) 

clean:
	rm -rf $(objects) $(targets)

distclean: clean