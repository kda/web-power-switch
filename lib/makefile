TARGET ?= libwps.so
SRC_DIRS ?= .

CC = g++

SRCS := $(shell find $(SRC_DIRS) -name '*.cc' -or -name '*.c')
OBJS := $(addsuffix .o,$(basename $(SRCS)))
DEPS := $(OBJS:.o=.d)

INC_DIRS := $(shell find $(SRC_DIRS) -type d)
INC_DIRS += ../include
INC_FLAGS := $(addprefix -I,$(INC_DIRS))

#CPPFLAGS ?= $(INC_FLAGS) -MMD -MP --std=c++17
CPPFLAGS ?= $(INC_FLAGS) -MMD -MP --std=c++17 -g1
CPPFLAGS += -fPIC
LDFLAGS += -shared
LDLIBS += $(shell pkg-config --libs libcurl openssl tidy yaml-cpp) -lpthread
#LDLIBS += $(shell pkg-config --libs libcurl openssl tidy yaml-cpp) -lSegFault

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) -o $@ $(LDLIBS)

#		$(AR) $(ARFLAGS) $@ $(OBJS)

.PHONY: clean
clean:
		$(RM) $(TARGET) $(OBJS) $(DEPS)

-include $(DEPS)

