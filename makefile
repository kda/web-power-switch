TARGET ?= pwrcntrl
SRC_DIRS ?= .

CC = g++

SRCS := $(shell find $(SRC_DIRS) -maxdepth 1 -name '*.cc' -or -name '*.c')
OBJS := $(addsuffix .o,$(basename $(SRCS)))
DEPS := $(OBJS:.o=.d)

LIB_SRCS := $(shell find lib -name '*.cc' -or -name '*.c')
LIB_OBJS := $(addsuffix .o,$(basename $(LIB_SRCS)))

WPSLIB := lib/libwps.so

#INC_DIRS := $(shell find $(SRC_DIRS) -type d)
INC_DIRS := lib
INC_FLAGS := $(addprefix -I,$(INC_DIRS))

#CPPFLAGS ?= $(INC_FLAGS) -MMD -MP --std=c++17
CPPFLAGS ?= $(INC_FLAGS) -MMD -MP --std=c++17 -g1
INC_FLAGS := $(addprefix -I,$(INC_DIRS))
LDLIBS += -Llib -lwps
#LDLIBS += -lwps
BASE_LDLIBS += $(shell pkg-config --libs libcurl openssl tidy yaml-cpp) -lpthread
#LDLIBS += $(shell pkg-config --libs libcurl openssl tidy yaml-cpp) -lSegFault

$(TARGET): $(OBJS) $(LIB_OBJS)
	$(CC) $(OBJS) $(LIB_OBJS) -o $@ $(BASE_LDLIBS)

$(TARGET)_dyn: $(OBJS) $(WPSLIB)
	$(CC) $(OBJS) -o $@ $(LDLIBS)

$(WPSLIB):  FORCE
	cd lib && make

FORCE:

.PHONY: clean
clean:
		$(RM) $(TARGET) $(OBJS) $(DEPS)
		cd lib && $(MAKE) clean

-include $(DEPS)

