#include "AudioProcessor.hpp"
#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>
#include <algorithm>

struct AudioProcessor::Impl {};

AudioProcessor::AudioProcessor() : pimpl(std::make_unique<Impl>()) {}
AudioProcessor::~AudioProcessor() = default;

// Safe little-endian reader
static uint16_t read16(const unsigned char* p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t read32(const unsigned char* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

bool AudioProcessor::loadAudio(const std::string& path, int target_sr, std::vector<float>& out_samples) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return false;

    unsigned char header[44];
    if (!file.read(reinterpret_cast<char*>(header), 44)) return false;

    // RIFF check
    if (header[0] != 'R' || header[1] != 'I' || header[2] != 'F' || header[3] != 'F') return false;
    if (header[8] != 'W' || header[9] != 'A' || header[10] != 'V' || header[11] != 'E') return false;

    uint16_t channels = read16(header + 22);
    uint32_t sample_rate = read32(header + 24);
    uint16_t bits_per_sample = read16(header + 34);

    if (channels == 0) return false;

    // Search for "data" chunk
    file.seekg(12);
    while (file) {
        unsigned char chunk_hdr[8];
        if (!file.read(reinterpret_cast<char*>(chunk_hdr), 8)) break;
        
        uint32_t chunk_size = read32(chunk_hdr + 4);
        
        if (chunk_hdr[0] == 'd' && chunk_hdr[1] == 'a' && chunk_hdr[2] == 't' && chunk_hdr[3] == 'a') {
            std::vector<char> buffer(chunk_size);
            if (!file.read(buffer.data(), chunk_size)) break;

            out_samples.clear();
            int bytes_per_sample = bits_per_sample / 8;
            int total_samples = chunk_size / bytes_per_sample;
            
            for (int i = 0; i < total_samples; i += channels) {
                float val = 0.0f;
                if (bits_per_sample == 16) {
                    int16_t s16 = (int16_t)read16(reinterpret_cast<unsigned char*>(&buffer[i * 2]));
                    val = s16 / 32768.0f;
                } else if (bits_per_sample == 32) {
                    val = *reinterpret_cast<float*>(&buffer[i * 4]);
                }
                out_samples.push_back(val);
            }

            // Simple resampling
            if (sample_rate != (uint32_t)target_sr && !out_samples.empty()) {
                std::vector<float> resampled;
                float ratio = (float)sample_rate / (float)target_sr;
                int target_count = (int)(out_samples.size() / ratio);
                resampled.reserve(target_count);
                for (int i = 0; i < target_count; ++i) {
                    float pos = i * ratio;
                    int idx = (int)pos;
                    float frac = pos - idx;
                    if (idx + 1 < (int)out_samples.size())
                        resampled.push_back(out_samples[idx] * (1.0f - frac) + out_samples[idx + 1] * frac);
                    else
                        resampled.push_back(out_samples[idx]);
                }
                out_samples = std::move(resampled);
            }
            return !out_samples.empty();
        }
        file.seekg(chunk_size, std::ios::cur);
    }

    return false;
}

bool AudioProcessor::saveAudio(const std::string& path, const std::vector<float>& samples, int sr, const std::string& format) {
    if (samples.empty()) return false;

    if (format == "wav") {
        std::ofstream file(path, std::ios::binary);
        if (!file.is_open()) return false;

        uint32_t num_samples = static_cast<uint32_t>(samples.size());
        uint32_t data_size = num_samples * 2;

        file.write("RIFF", 4);
        uint32_t chunk_size = 36 + data_size;
        file.write(reinterpret_cast<char*>(&chunk_size), 4);
        file.write("WAVE", 4);
        file.write("fmt ", 4);
        uint32_t subchunk1_size = 16;
        file.write(reinterpret_cast<char*>(&subchunk1_size), 4);
        uint16_t audio_format = 1; 
        file.write(reinterpret_cast<char*>(&audio_format), 2);
        uint16_t num_channels = 1;
        file.write(reinterpret_cast<char*>(&num_channels), 2);
        file.write(reinterpret_cast<const char*>(&sr), 4);
        uint32_t byte_rate = sr * num_channels * 2;
        file.write(reinterpret_cast<char*>(&byte_rate), 4);
        uint16_t block_align = num_channels * 2;
        file.write(reinterpret_cast<char*>(&block_align), 2);
        uint16_t bits_per_sample = 16;
        file.write(reinterpret_cast<char*>(&bits_per_sample), 2);
        file.write("data", 4);
        file.write(reinterpret_cast<char*>(&data_size), 4);

        for (float s : samples) {
            float clamped = (std::max)(-1.0f, (std::min)(1.0f, s));
            int16_t val = static_cast<int16_t>(clamped * 32767.0f);
            file.write(reinterpret_cast<char*>(&val), 2);
        }
        return true;
    }
    return false;
}
