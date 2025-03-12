/*
#include "stb_image.h"
#include <iostream>
using std::cout;

int main(int argc, char *argv[]) {
    cout << "Done!\n";
}
*/
#include <iostream>
#include <fstream>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <vector>
#include <cmath>

// WAV header structure
struct WavHeader {
    // RIFF chunk
    char riffId[4] = {'R', 'I', 'F', 'F'};
    uint32_t riffSize;
    char waveId[4] = {'W', 'A', 'V', 'E'};
    
    // fmt chunk
    char fmtId[4] = {'f', 'm', 't', ' '};
    uint32_t fmtSize = 16;
    uint16_t audioFormat = 1; // PCM
    uint16_t numChannels = 1; // Mono
    uint32_t sampleRate = 44100;
    uint32_t byteRate;        // SampleRate * NumChannels * BitsPerSample/8
    uint16_t blockAlign;      // NumChannels * BitsPerSample/8
    uint16_t bitsPerSample = 16;
    
    // data chunk
    char dataId[4] = {'d', 'a', 't', 'a'};
    uint32_t dataSize;
};

// Function to convert image to wavetable
bool imageToWavetable(const std::string& imagePath, const std::string& wavetablePath, int targetFrameSize = 1024) {
    int width, height, channels;
    unsigned char *imgData = stbi_load(imagePath.c_str(), &width, &height, &channels, 0);
    if (!imgData) {
        std::cerr << "Error loading image: " << imagePath << std::endl;
        return false;
    }
    
    // Convert to grayscale
    std::vector<float> grayscaleData(width * height);
    for (int i = 0; i < width * height; ++i) {
        unsigned char r = imgData[i * channels];
        unsigned char g = channels > 1 ? imgData[i * channels + 1] : r;
        unsigned char b = channels > 2 ? imgData[i * channels + 2] : r;
        grayscaleData[i] = (0.2989f * r + 0.587f * g + 0.114f * b) / 255.0f;
    }
    stbi_image_free(imgData);
    
    // Resize (crude method, nearest neighbor)
    std::vector<float> resizedData(targetFrameSize * height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < targetFrameSize; ++x) {
            int originalX = static_cast<int>(x * (static_cast<float>(width) / targetFrameSize));
            resizedData[y * targetFrameSize + x] = grayscaleData[y * width + originalX];
        }
    }
    
    // Normalize to -1 to 1 and create wavetable
    std::vector<int16_t> wavetableData(targetFrameSize * height);
    for (int i = 0; i < targetFrameSize * height; ++i) {
        float normalizedSample = resizedData[i] * 2.0f - 1.0f;
        wavetableData[i] = static_cast<int16_t>(normalizedSample * 32767.0f);
    }
    
    // Create WAV header
    WavHeader header;
    header.numChannels = 1; // Mono
    header.sampleRate = 44100;
    header.bitsPerSample = 16;
    header.blockAlign = header.numChannels * (header.bitsPerSample / 8);
    header.byteRate = header.sampleRate * header.blockAlign;
    header.dataSize = wavetableData.size() * sizeof(int16_t);
    header.riffSize = 36 + header.dataSize; // 36 = size of header without RIFF chunk
    
    // Write to WAV file
    std::ofstream wavFile(wavetablePath, std::ios::binary);
    if (!wavFile.is_open()) {
        std::cerr << "Error opening WAV file: " << wavetablePath << std::endl;
        return false;
    }
    
    // Write header
    wavFile.write(reinterpret_cast<const char*>(&header), sizeof(WavHeader));
    
    // Write audio data
    wavFile.write(reinterpret_cast<const char*>(wavetableData.data()), 
                  wavetableData.size() * sizeof(int16_t));
    
    wavFile.close();
    std::cout << "Created WAV file with " << height << " frames of " 
              << targetFrameSize << " samples each" << std::endl;
    return true;
}

int main() {
    std::string imagePath = "image.jpg";
    std::string wavetablePath = "wavetable.wav";
    if (imageToWavetable(imagePath, wavetablePath)) {
        std::cout << "Wavetable created successfully: " << wavetablePath << std::endl;
    } else {
        std::cerr << "Failed to create wavetable." << std::endl;
    }
    return 0;
}