$(info start load ofpkFlags.mk)

OFP_IFLAGS = -I$(OFPK_ROOT_DIR)/out/dpdk/include/ \
			 -I$(OFPK_ROOT_DIR)/out/odp/include/ \
			 -I$(OFPK_ROOT_DIR)/out/ofp/include/ \

OFP_LLIBS = -L$(OFPK_ROOT_DIR)/out/dpdk/lib/ \
			-L$(OFPK_ROOT_DIR)/out/odp/lib/ \
			-L$(OFPK_ROOT_DIR)/out/ofp/lib/ \
	
OFP_LIBS = -lpthread -lofp -lodp-dpdk -lodphelper-linux -ldpdk -ldl -lrt -lm -lcrypto -lpcap 
			
CC = gcc

OFP_CFLAGS += -g -O0 -Wall