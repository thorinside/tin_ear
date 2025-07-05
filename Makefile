ifndef NT_API_PATH
	NT_API_PATH := libs/distingNT_API
endif

ifndef STEAM_AUDIO_PATH
	STEAM_AUDIO_PATH := libs/steam-audio
endif

INCLUDE_PATH := $(NT_API_PATH)/include
STEAM_AUDIO_INCLUDE := $(STEAM_AUDIO_PATH)/core/include
STEAM_AUDIO_CORE_SRC := $(STEAM_AUDIO_PATH)/core/src/core

# List of source files to compile
srcs := th_tinear.cpp

# Generate output object file paths
outputs := $(patsubst %.cpp,plugins/%.o,$(srcs))

# Steam Audio HRTF library
steam_audio_hrtf := plugins/steamaudio_hrtf.o

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
            -I$(INCLUDE_PATH) \
            -I$(STEAM_AUDIO_INCLUDE) \
            -I$(STEAM_AUDIO_CORE_SRC)

# Default target
all: $(plugin_binary)

# Clean target
clean:
	rm -f $(outputs) $(steam_audio_hrtf) $(plugin_binary)
	$(MAKE) -f steam_audio_hrtf.mk clean

# Rule to create the plugins directory
$(outputs) $(steam_audio_hrtf): | plugins

plugins:
	mkdir -p $@

# Compilation rule for .cpp to .o
plugins/%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $^

# Steam Audio HRTF library compilation
$(steam_audio_hrtf):
	$(MAKE) -f steam_audio_hrtf.mk

# Link the final plugin binary
$(plugin_binary): $(outputs) $(steam_audio_hrtf)
	@echo "Linking final plugin binary..."
	arm-none-eabi-ld -r -o $@ $(outputs) $(steam_audio_hrtf)
	@echo "Plugin binary created: $@"

.PHONY: all clean