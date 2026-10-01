#include "openupdater/core/sha256.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>

int main() {
    using namespace openupdater;

    const auto temp =
        std::filesystem::temp_directory_path() / "openupdater_sha256_test";
    std::filesystem::remove_all(temp);
    std::filesystem::create_directories(temp);

    const auto empty = temp / "empty.bin";
    std::ofstream(empty, std::ios::binary).close();

    const auto abc = temp / "abc.txt";
    std::ofstream(abc, std::ios::binary) << "abc";

    assert(
        Sha256::hash_file(empty) ==
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");

    assert(
        Sha256::hash_file(abc) ==
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");

    assert(verify_sha256(
        abc,
        "BA7816BF8F01CFEA414140DE5DAE2223B00361A396177A9CB410FF61F20015AD"));

    assert(!verify_sha256(
        abc,
        "0000000000000000000000000000000000000000000000000000000000000000"));

    std::filesystem::remove_all(temp);
    return 0;
}
