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
#include "stb_image.h"
#include "stb_image_resize2.h"
#include "stb_image_write.h"
#include <vector>
#include <cmath>

using std::cout;

struct WavetableData_t
{
    std::vector<int16_t> waveData;
    int frameSize = 1024;
    int rowSize = 256;
};

class imageManager
{
    public:
        imageManager(int frameSize = 1024, int tableRows = 256)
            : m_frameSize(frameSize), m_tableRows(tableRows)
            {}
        ~imageManager()
            {stbi_image_free(m_rawImageData);}
        bool LoadFromFile(const std::string& imagePath);
        WavetableData_t GetProcessedData(void);

    private:
        unsigned char* m_rawImageData;
        int m_frameSize;
        int m_tableRows;
        int m_height;
        int m_width;
        int m_channels;
};

bool imageManager::LoadFromFile(const std::string& imagePath)
{
    m_rawImageData = stbi_load(imagePath.c_str(), &m_width, &m_height, &m_channels, 0);
    if (!m_rawImageData) {
        std::cerr << "Error loading image: " << imagePath << std::endl;
        return false;
    }
    return true;
}

WavetableData_t imageManager::GetProcessedData(void)
{
    WavetableData_t r;
    r.frameSize = m_frameSize;
    r.rowSize = m_tableRows;

    // Convert to grayscale
    std::vector<float> grayscaleData(m_width * m_height);
    for (int i = 0; i < m_width * m_height; ++i) {
        unsigned char r = m_rawImageData[i * m_channels];
        unsigned char g = m_channels > 1 ? m_rawImageData[i * m_channels + 1] : r;
        unsigned char b = m_channels > 2 ? m_rawImageData[i * m_channels + 2] : r;
        grayscaleData[i] = (0.2989f * r + 0.587f * g + 0.114f * b) / 255.0f;
    }

    std::vector<float> resizedData(m_frameSize * m_height);   
    // Resize down (crude version, nearest neighbor)
    // this algorithm samples single pixels proprotionally across the width to arrange the new width
    for (int y = 0; y < m_height; ++y) {
        for (int x = 0; x < m_frameSize; ++x) {
            int originalX = static_cast<int>(x * (static_cast<float>(m_width) / m_frameSize));
            resizedData[y * m_frameSize + x] = grayscaleData[y * m_width + originalX];
        }
    }

    int rowSampleModulus = m_height / 256;
    //cout << "row modulus is " << rowSampleModulus << "\n";
    // Normalize to -1 to 1 and create wavetable
    std::vector<int16_t> wavetableData(m_frameSize * 256);
    int writeRow = 0;
    for (int row = 0; row < m_height; row++)
    {
        // reduce image sample to end up at 64 rows in table
        if (row % rowSampleModulus == 0)
        {
            cout << "sampling row# " << row << " as wavtable# " << writeRow << "\n";
            for (int i = 0; i < m_frameSize; ++i)
            {            
                float normalizedSample = resizedData[row * m_frameSize + i] * 2.0f - 1.0f;
                wavetableData[writeRow * m_frameSize + i] = static_cast<int16_t>(normalizedSample * 32767.0f);
            }
            writeRow++;
            if (writeRow == 256) break; // this is the size limit for the wavetable, trailing rows are ignored
        }
    }

    r.waveData = std::move(wavetableData);
    return r;
}

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
    //stbi_write_jpg("grayscale.jpg", width, height, 1, &grayscaleData[0], 100);
    stbi_image_free(imgData);

    std::vector<float> resizedData(targetFrameSize * height);
#if 1    
    // Resize (crude method, nearest neighbor)
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < targetFrameSize; ++x) {
            int originalX = static_cast<int>(x * (static_cast<float>(width) / targetFrameSize));
            resizedData[y * targetFrameSize + x] = grayscaleData[y * width + originalX];
        }
    }
#endif

    //stbir_pixel_layout layout;
    //stbir_resize_float_linear(&grayscaleData[0], width, height, 256, &resizedData[0], targetFrameSize, height, 256, STBIR_1CHANNEL);    
    //std::vector<float> sampleData(targetFrameSize * 64); // force to 64 rows

    int rowSampleModulus = height / 256;
    //cout << "row modulus is " << rowSampleModulus << "\n";
    // Normalize to -1 to 1 and create wavetable
    std::vector<int16_t> wavetableData(targetFrameSize * 256);
    int writeRow = 0;
    for (int row = 0; row < height; row++)
    {
        // reduce image sample to end up at 64 rows in table
        if (row % rowSampleModulus == 0)
        {
            cout << "sampling row# " << row << " as wavtable# " << writeRow << "\n";
            for (int i = 0; i < targetFrameSize; ++i)
            {            
                float normalizedSample = resizedData[row * targetFrameSize + i] * 2.0f - 1.0f;
                wavetableData[writeRow * targetFrameSize + i] = static_cast<int16_t>(normalizedSample * 32767.0f);
            }
            writeRow++;
            if (writeRow == 256) break;
        }
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