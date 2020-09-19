//CXX       = clang
//CCX       = clang++
CXX       = gcc
CCX       = g++

TARGET    = test
SRCS      = test.cpp \
	    ../lcd/cLcd.cpp \
	    ../lcd/cFrameDiff.cpp \
	    ../lcd/cDrawAA.cpp \
	    ../lcd/cSnapshot.cpp \
	    ../fonts/FreeSansBold.cpp \
	    ../../shared/utils/cLog.cpp \
	    ../pigpio/pigpio.c \
	    ../pigpio/command.c \

BUILD_DIR = ./build
CLEAN_DIRS = $(BUILD_DIR)
LIBS      = -l pthread -l bfd -l pigpio -l rt \
	    `pkg-config --libs freetype2` \
	    -L /opt/vc/lib -l vchiq_arm -l vchostif -l vcos -l bcm_host
#
OBJS      = $(SRCS:%=$(BUILD_DIR)/%.o)
DEPS      = $(OBJS:.o=.d)

CFLAGS = -Wall \
	 -MMD -MP \
	 `pkg-config --cflags freetype2` \
	 -I../../opt/vc/include \
	 -I../../opt/vc/include/interface/vcos/pthreads\
	 -I../../opt/vc/include/interface/vmcs_host \
	 -I../../opt/vc/include/interface/vmcs_host/linux \
	 -fomit-frame-pointer \
	 -g \
	 -O2 \

LD_VERSION = $(shell ld -v 2>&1 | sed -ne 's/.*\([0-9]\+\.[0-9]\+\).*/\1/p')
ifeq "$(LD_VERSION)" "2.34"
	CFLAGS += -D HAS_BINUTILS_234
endif

$(BUILD_DIR)/%.c.o: %.c
	mkdir -p $(dir $@)
	$(CXX) -std=c11 $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.cpp.o: %.cpp
	mkdir -p $(dir $@)
	$(CCX) -std=c++17 $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJS)
	$(CCX) $(OBJS) -o $@ $(LIBS)

clean:
	rm -rf $(TARGET) $(CLEAN_DIRS)
rebuild:
	make clean && make -j 4

all: $(TARGET)

-include $(DEPS)
