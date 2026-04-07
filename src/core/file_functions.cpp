#include "file_functions.hpp"

#include <vector>
#include <fstream>
#include <string>

// Helper function for reading files
std::vector<char> readFile(const std::string& filename)
{
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open())
    {
        throw std::runtime_error("Failed to open file!");
    }

    // Make buffer to store file data
    std::vector<char> buffer(file.tellg());

    // Go back to beginning and copy all data
    file.seekg(0, std::ios::beg);
    file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));

    file.close();

    return buffer;
}
