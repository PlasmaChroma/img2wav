#include <iostream>
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

    int rowSampleModulus = m_height / m_tableRows;
    //cout << "row modulus is " << rowSampleModulus << "\n";
    // Normalize to -1 to 1 and create wavetable data
    std::vector<int16_t> wavetableData(m_frameSize * m_tableRows);
    int writeRow = 0;
    // nagivate this data backwards because ableton puts first sample in bottom of the table
    for (int row = m_height; row > 0; row--)
    {
        // reduce image sample to end up at 64 rows in table
        if (row % rowSampleModulus == 0)
        {
            //cout << "sampling row# " << row << " as wavtable# " << writeRow << "\n";
            for (int i = 0; i < m_frameSize; ++i)
            {            
                float normalizedSample = resizedData[row * m_frameSize + i] * 2.0f - 1.0f;
                wavetableData[writeRow * m_frameSize + i] = static_cast<int16_t>(normalizedSample * 32767.0f);
            }
            writeRow++;
            if (writeRow == m_tableRows) break; // this is the size limit for the wavetable, trailing rows are ignored
        }
    }

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
    std::string imagePath = "image.png";
    std::string wavetablePath = "wavetable.wav";
    std::string wavetablePathInv = "wavetable_inverted.wav";

    WaveTableWriter wt(1024, 256); // maximum table that Ableton will accept for user data
    wt.GetDataFromImageFile(imagePath);
    wt.PrintRowMinMax();
    cout << "Trimmed " << wt.TrimData(3000) << " rows under 3k variance.\n";
    wt.WriteWaveTableToFile(wavetablePath, false);
    wt.WriteWaveTableToFile(wavetablePathInv, true);

    return 0;
}