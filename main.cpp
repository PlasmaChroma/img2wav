#include <iostream>
#include <stdio.h>
#include <fstream>
#include "stb_image.h"
#include "stb_image_resize2.h"
#include "stb_image_write.h"
#include <vector>
#include <cmath>
#include <algorithm>

using std::cout;

// WAV header structure
struct WavHeader_t {
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

class imageManager
{
    public:
        imageManager(int frameSize = 1024, int tableRows = 256)
            : m_frameSize(frameSize), m_tableRows(tableRows)
            {}
        ~imageManager()
            {stbi_image_free(m_rawImageData);}
        bool LoadFromFile(const std::string& imagePath);
        std::vector<int16_t> GetProcessedData(void);

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
    // only accepting RGB 3 channel images for now; could be enhanced to handle different images
    m_rawImageData = stbi_load(imagePath.c_str(), &m_width, &m_height, &m_channels, 0);
#if 0    
    if (m_channels != 3)
    {
        std::cerr << "Not a 3 channel image: " << imagePath << std::endl;
        return false;
    }
#endif    
    if (!m_rawImageData) {
        std::cerr << "Unknown error loading image: " << imagePath << std::endl;
        return false;
    }

    // note: resize algo is able to expand width so size is not required
#if 0
    if (m_width < m_frameSize) {
        std::cerr << "Image is not wide enough for frame sizing\n" << std::endl;
        return false;
    }
#endif

    if (m_height < m_tableRows) {
        std::cerr << "Image not tall enough for requested rows\n" << std::endl;
        return false;
    }
    return true;
}

std::vector<int16_t> imageManager::GetProcessedData(void)
{
    // ----- Start by resizing the image to target wavetable size -----
    int dst_width = m_frameSize;
    int dst_height = m_tableRows;
    
    // Allocate memory for the resized image (could be refactored)
    unsigned char *dst_data = (unsigned char*)malloc(dst_width * dst_height * m_channels);
    
    if (!dst_data) {
        std::cerr << "Memory allocation failed\n";
        exit(1);
    }
    
    // Perform the resize operation using stbir_resize_uint8_linear
    unsigned char* result = stbir_resize_uint8_linear(
        m_rawImageData,  // source image data 
        m_width,         // source width
        m_height,        // source height
        0,               // source stride in bytes (0 = computed automatically)
        dst_data,        // destination image data
        dst_width,       // destination width
        dst_height,      // destination height
        0,               // destination stride in bytes (0 = computed automatically)
        (stbir_pixel_layout)m_channels // number of channels
    );
    
    if (result == 0) {
        printf("Resize operation failed\n");
        free(dst_data);
        exit(1);
    }
    
    printf("Image resized successfully to %d x %d\n", dst_width, dst_height);

    // Convert to grayscale
    std::vector<float> grayscaleData(m_frameSize * m_tableRows);
    for (int i = 0; i < m_frameSize * m_tableRows; ++i) {
        unsigned char r = dst_data[i * m_channels];
        unsigned char g = m_channels > 1 ? m_rawImageData[i * m_channels + 1] : r;
        unsigned char b = m_channels > 2 ? m_rawImageData[i * m_channels + 2] : r;
        grayscaleData[i] = (0.2989f * r + 0.587f * g + 0.114f * b) / 255.0f;
        if (i < 1024) cout << " " << grayscaleData[i] << ",";
    }

    std::vector<int16_t> wavetableData(m_frameSize * m_tableRows);
    // nagivate this data in reverse, because ableton puts the first sample at the bottom
    for (int row = m_height; row > 0; row--)
    {
        //cout << "sampling row# " << row << " as wavtable# " << writeRow << "\n";
        for (int i = 0; i < m_frameSize; ++i)
        {            
            float normalizedSample = grayscaleData[row * m_frameSize + i] * 2.0f - 1.0f;
            wavetableData[row * m_frameSize + i] = static_cast<int16_t>(normalizedSample * 32767.0f);
        }
    }

    // Clean up
    free(dst_data);

    return wavetableData;
}

class WaveTableWriter
{
    public:
        WaveTableWriter(int frameSize = 1024, int tableRows = 256)
            : m_frameSize(frameSize), m_tableRows(tableRows) {}
        ~WaveTableWriter() {}        
        bool GetDataFromImageFile(const std::string& imagePath);
        bool WriteWaveTableToFile(const std::string& filename, bool invert);
        int TrimData(uint16_t thresholdVariance);
        void PrintRowMinMax(void);
    private:
        int m_frameSize;
        int m_tableRows;
        std::vector<int16_t> m_wavData;
        bool m_dataReady = false;
};

bool WaveTableWriter::GetDataFromImageFile(const std::string& imagePath)
{
    imageManager im(m_frameSize, m_tableRows);

    if (im.LoadFromFile(imagePath))
    {
        m_wavData = im.GetProcessedData();
        m_dataReady = true;
    } else {
        std::cerr << "Image loader unable to process file: " << imagePath << std::endl;
        return false;
    }

    return true;
}

bool WaveTableWriter::WriteWaveTableToFile(const std::string& filename, bool invert)
{
    if (!m_dataReady) {
        std::cerr << "Data is not ready for writing!" << std::endl;
        return false;
    }

    // Create WAV header
    WavHeader_t header;
    header.numChannels = 1; // Mono
    header.sampleRate = 44100;
    header.bitsPerSample = 16;
    header.blockAlign = header.numChannels * (header.bitsPerSample / 8);
    header.byteRate = header.sampleRate * header.blockAlign;
    header.dataSize = m_wavData.size() * sizeof(int16_t);
    header.riffSize = 36 + header.dataSize; // 36 = size of header without RIFF chunk

    // Write to WAV file
    std::ofstream wavFile(filename, std::ios::binary);
    if (!wavFile.is_open()) {
        std::cerr << "Error opening WAV file: " << filename << std::endl;
        return false;
    }
    
    // Write header
    wavFile.write(reinterpret_cast<const char*>(&header), sizeof(WavHeader_t));
    
    if (invert)
    {
        for (auto &x : m_wavData)
        {
            x = x * -1;
        }
    }

    // Write audio data
    wavFile.write(reinterpret_cast<const char*>(m_wavData.data()), m_wavData.size() * sizeof(int16_t));
    
    wavFile.close();
    std::cout << "Created WAV file with " << m_tableRows << " rows of " 
              << m_frameSize << " samples each" << std::endl;

    return true;
}

int WaveTableWriter::TrimData(uint16_t thresholdVariance)
{
    if (!m_dataReady) {
        std::cerr << "Data is not ready!" << std::endl;
        return 0;
    }
    std::vector<int16_t> filteredData;
    int filterCounter = 0;
    // iterate through rows
    for (int r = 0; r < m_tableRows; ++r)
    {
        std::vector<int16_t> rowSample;
        // put one row into our sample vector so we can just use algorithm
        // note: inefficient but with the data sizes we are dealing with almost irrelevent
        for (int x = 0; x < m_frameSize; ++x)
        {            
            int index = (r * m_frameSize) + x;
            rowSample.push_back(m_wavData[index]);
        }
        auto minmax = std::minmax_element(rowSample.begin(), rowSample.end());
        int variance = (*minmax.second - *minmax.first);
        if (variance > thresholdVariance) {
            filteredData.insert(filteredData.end(), rowSample.begin(), rowSample.end());
        } else {
            filterCounter++;
        }
    }
    m_wavData = filteredData;
    return filterCounter;
}

void WaveTableWriter::PrintRowMinMax(void)
{
    if (!m_dataReady) {
        std::cerr << "Data is not ready!" << std::endl;
        return;
    }

    // iterate through rows
    for (int r = 0; r < m_tableRows; ++r)
    {
        std::vector<int16_t> rowSample;
        // put one row into our sample vector so we can just use algorithm
        // note: inefficient but with the data sizes we are dealing with almost irrelevent
        for (int x = 0; x < m_frameSize; ++x)
        {            
            int index = (r * m_frameSize) + x;
            rowSample.push_back(m_wavData[index]);
        }
        auto minmax = std::minmax_element(rowSample.begin(), rowSample.end());
        cout << "Row# " << r << " min: " << *minmax.first << " max: " << *minmax.second 
            << " variance " << (*minmax.second - *minmax.first) << std::endl;
    }
}

int main(int argc, char *argv[])
{    
    std::string imagePath = "image.jpg";
    std::string wavetablePath = "wavetable.wav";
    std::string wavetablePathInv = "wavetable_inverted.wav";

    WaveTableWriter wt(1024, 256); // maximum table that Ableton will accept for user data
    wt.GetDataFromImageFile(imagePath);
    //wt.PrintRowMinMax();
    cout << "Trimmed " << wt.TrimData(3000) << " rows under 3k variance.\n";
    wt.WriteWaveTableToFile(wavetablePath, false);
    wt.WriteWaveTableToFile(wavetablePathInv, true);

    return 0;
}