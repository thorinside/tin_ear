# Professional Spatial Audio Library for ARM Cortex-M7

# Target
HRTF_LIB := plugins/steamaudio_hrtf.o

# Compiler settings
CXX := arm-none-eabi-c++
CXXFLAGS := -std=c++11 \
            -mcpu=cortex-m7 \
            -mfpu=fpv5-d16 \
            -mfloat-abi=hard \
            -mthumb \
            -fno-rtti \
            -fno-exceptions \
            -Os \
            -fPIC

# Professional spatial audio implementation source
SPATIAL_AUDIO_SOURCE := professional_spatial_audio.cpp

# Build rules
$(HRTF_LIB): $(SPATIAL_AUDIO_SOURCE) | plugins
	@echo "Building professional spatial audio for ARM Cortex-M7..."
	$(CXX) $(CXXFLAGS) -c $(SPATIAL_AUDIO_SOURCE) -o $@
	@echo "Professional spatial audio built successfully"

clean:
	rm -f $(HRTF_LIB)

.PHONY: clean