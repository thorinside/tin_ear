ifndef NT_API_PATH
	NT_API_PATH := libs/distingNT_API
endif

INCLUDE_PATH := $(NT_API_PATH)/include

# List of source files to compile
srcs := th_tinear.cpp professional_spatial_audio.cpp

# Generate output object file paths
outputs := $(patsubst %.cpp,plugins/%.o,$(srcs))

# Final linked plugin
plugin_binary := plugins/th_tinear_plugin.o

# Compiler settings
CXX := arm-none-eabi-c++
CXXFLAGS := -std=c++14 \
            -mcpu=cortex-m7 \
            -mfpu=fpv5-d16 \
            -mfloat-abi=hard \
            -mthumb \
            -fno-rtti \
            -fno-exceptions \
            -Os \
            -fPIC \
            -Wall \
            -I$(INCLUDE_PATH)

# Default target
all: $(plugin_binary)

# Native host build for nt_emu / VCV Rack (not part of the hardware build)
HOST_CXX ?= clang++
HOST_OS := $(shell uname -s)
ifeq ($(HOST_OS),Darwin)
	HOST_DYLIB_EXT := dylib
	HOST_DYLIB_LDFLAGS := -dynamiclib -undefined dynamic_lookup
else
	HOST_DYLIB_EXT := so
	HOST_DYLIB_LDFLAGS := -shared
endif
HOST_DYLIB := plugins/th_tinear_plugin.$(HOST_DYLIB_EXT)
HOST_CXXFLAGS := -std=c++14 -fPIC -Os -Wall -fno-rtti -fno-exceptions -I$(INCLUDE_PATH)

dylib: $(HOST_DYLIB)

$(HOST_DYLIB): $(srcs) | plugins
	$(HOST_CXX) $(HOST_CXXFLAGS) $(HOST_DYLIB_LDFLAGS) -o $@ $(srcs)
	@echo "Built host plugin: $@"

# Clean target
clean:
	rm -f $(outputs) $(plugin_binary) $(HOST_DYLIB)

# Rule to create the plugins directory
$(outputs): | plugins

plugins:
	mkdir -p $@

# Compilation rule for .cpp to .o
plugins/%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $^

# Link the final plugin binary
$(plugin_binary): $(outputs)
	@echo "Linking final plugin binary..."
	arm-none-eabi-ld -r -o $@ $(outputs)
	@echo "Plugin binary created: $@"

.PHONY: all clean dylib