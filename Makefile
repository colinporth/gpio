CXX       = clang
CCX       = clang++
//CXX       = g++

TARGET    = lcd
SRCS      = lcdTest.cpp \
	    cLcd.cpp \
	    ../shared/utils/cLog.cpp \
	    fonts/FreeSansBold.cpp \
	    pigpio/pigpio.c \
	    pigpio/command.c \

BUILD_DIR = ./build
CLEAN_DIRS = $(BUILD_DIR)
LIBS      = -lpthread -lbfd `pkg-config --libs freetype2` -lpigpio -lrt
#
#
OBJS      = $(SRCS:%=$(BUILD_DIR)/%.o)
DEPS      = $(OBJS:.o=.d)

CFLAGS = -Wall \
	 -MMD -MP \
	 -g -O2 \
	 `pkg-config --cflags freetype2`


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
