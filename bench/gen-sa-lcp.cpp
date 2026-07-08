#include <iostream>
#include <climits>
#include <fstream>
#include <vector>
#include <filesystem>
#include <omp.h>
#include "libsais64.h"

template <typename T>
void read_from_file(std::istream& in, T* data, uint64_t size) {
    uint64_t size_left = size;
    uint64_t bytes_to_read;

    while (size_left > 0) {
        bytes_to_read = std::min<uint64_t>(size_left, INT_MAX);
        in.read((char*) &data[size - size_left], bytes_to_read);
        size_left -= bytes_to_read;
    }
}

int main(int argc, char *argv[]) {
    if(argc != 4) {
        std::cout << "invalid input, usage: <input_file> <sa_file> <lcp_file>" << std::endl;
        return -1;
    }

    std::ifstream input_file(argv[1]);
    std::ofstream sa_file(argv[2]);
    std::ofstream lcp_file(argv[3]);

    if(!input_file.good()) {
        std::cout << "invalid input: could not read <input_file>" << std::endl;
        return -1;
    }

    if(!sa_file.is_open()) {
        std::cout << "invalid input: could not create <sa_file>" << std::endl;
        return -1;
    }

    if(!lcp_file.is_open()) {
        std::cout << "invalid input: could not create <lcp_file>" << std::endl;
        return -1;
    }

    std::cout << "reading T" << std::endl;
	int64_t n = std::filesystem::file_size(argv[1]) + 1;
    std::vector<uint8_t> T(n);
    read_from_file(input_file, T.data(), n - 1);
    T[n - 1] = 1;

    std::cout << "building SA" << std::endl;
    std::vector<int64_t> SA(n);
    libsais64_omp(T.data(), SA.data(), n, 0, NULL, omp_get_max_threads());

    std::cout << "building PLCP" << std::endl;
    std::vector<int64_t> PLCP(n);
    libsais64_plcp_omp(T.data(), SA.data(), PLCP.data(), n, omp_get_max_threads());

    std::cout << "building LCP" << std::endl;
    std::vector<int64_t> LCP(n);
    libsais64_lcp_omp(PLCP.data(), SA.data(), LCP.data(), n, omp_get_max_threads());

    std::cout << "writing SA to <sa_file>" << std::endl;
    for (int64_t i = 0; i < n; i++) {
        sa_file.write((char*) &SA[i], 5);
    }

    std::cout << "writing LCP to <lcp_file>" << std::endl;
    for (int64_t i = 0; i < n; i++) {
        lcp_file.write((char*) &LCP[i], 5);
    }
    
    return 0;
}