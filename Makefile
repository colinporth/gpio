TARGET    = blink
SRCS      = blink.c

BUILD_DIR = ./build
CLEAN_DIRS = $(BUILD_DIR)
LIBS      = -lpthread -lpigpio -lrt
#
#
OBJS      = $(SRCS:%=$(BUILD_DIR)/%.o)
DEPS      = $(OBJS:.o=.d)

CFLAGS = -Wall -Wno-unused-result \
	 -g \
	 -MMD -MP \
#         -O2

$(BUILD_DIR)/%.c.o: %.c
	mkdir -p $(dir $@)
	gcc -std=c11 $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.cpp.o: %.cpp
	mkdir -p $(dir $@)
	g++ -std=c++17 $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJS)
	g++ $(OBJS) -o $@ $(LIBS)

clean:
	rm -rf $(TARGET) $(CLEAN_DIRS)
rebuild:
	make clean && make -j 4

all: $(TARGET)

-include $(DEPS)
