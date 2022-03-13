#include "App/App.h"
#include <cryptopp/cryptlib.h>
#include <cryptopp/sha.h>

int main()
{
    CryptoPP::SHA256 hash;
    std::cout << "Name: " << hash.AlgorithmName() << std::endl;
    std::cout << "Digest size: " << hash.DigestSize() << std::endl;
    std::cout << "Block size: " << hash.BlockSize() << std::endl;
    App filterApp;
    filterApp.run();
    return 0;
}
