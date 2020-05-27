#include "ethereum.h"


// todo: implement
// For document of methods see connector.h

static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

static void parse32ByteHexString(std::string s, uint8_t* out) {
  const char* hexString = s.c_str();

  for(int i=0; i<32; i++) {
    char byteString[3];
    byteString[0] = hexString[2*i];
    byteString[1] = hexString[2*i+1];
    byteString[2] = 0; // termination character

    // convert 1-byte hex string to numeric representation
    uint8_t sNum = (uint8_t) strtoul(byteString, nullptr, 16);
    out[i] = sNum;
  }
}

static std::vector<std::string> Split(const std::string& str, int splitLength)
{
    int NumSubstrings = str.length() / splitLength;
    std::vector<std::string> ret;

    for (auto i = 0; i < NumSubstrings; i++)
    {
        ret.push_back(str.substr(i * splitLength, splitLength));
    }

    // If there are leftover characters, create a shorter item at the end.
    if (str.length() % splitLength != 0)
    {
        ret.push_back(str.substr(splitLength * NumSubstrings));
    }

    return ret;
}



Ethereum::Ethereum(const std::string&) {}

Ethereum::~Ethereum() = default;

int Ethereum::get(TableName, ByteData*, unsigned char*, int) {
  return 0;
}

int Ethereum::put(TableName, ByteData*, ByteData* ) {
  return 0;
}

int Ethereum::remove(TableName, ByteData *) {
  return 0;
}

void Ethereum::tableScan(TableName, std::vector<ByteData> &tuples, size_t keyLength, size_t valueLength) {
    CURL *curl;
    //CURLcode res;
    std::string readBuffer;

    curl = curl_easy_init();
    if(curl) {
        curl_easy_setopt(curl, CURLOPT_URL, "http://localhost:8545");
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "{\"jsonrpc\":\"2.0\",\"method\":\"eth_call\",\"params\":[{\"data\":\"0xb3055e26\",\"to\":\"0xd6F0b586c959671B11c92111De4785b7f4890D96\"}, \"latest\"],\"id\":1}");
        /*res = */curl_easy_perform(curl);
        curl_easy_cleanup(curl);

        std::regex rgx(".*\"result\":\"0x(\\w+)\".*");
        std::smatch match;

        const std::string s = readBuffer;
        if (std::regex_search(s.begin(), s.end(), match, rgx)) {
            //std::cout << "match: " << match[1] << '\n';

            std::vector<std::string> results = Split(match[1], 64);

            unsigned int count;
            std::stringstream ss;
            ss << std::hex << results[2];
            ss >> count;

            for (std::vector<int>::size_type i = 3; i < 3 + count; i++) {

                int valueIndex = i+count+1;

                /*std::string key = hexToASCII(results[i]);
                std::string value = hexToASCII(results[valueIndex]);

                std::cout << i << ": " << key << " - " << valueIndex << ": " << value << std::endl;

                std::string rowStr = key + value;
                unsigned char* row = new unsigned char[64];
                memcpy(row, rowStr.c_str(), 64);*/

                uint8_t key[32]; // 32 byte value
                parse32ByteHexString(results[i], key);

                uint8_t value[32]; // 32 byte key
                parse32ByteHexString(results[valueIndex], value);

                auto row = new unsigned char[keyLength + valueLength];

                // Copy key
                memcpy(&(row[0]), key, keyLength);

                // Copy value
                memcpy(&(row[keyLength]), value, valueLength);

                ByteData bd = ByteData(row, keyLength + valueLength);
                tuples.push_back(bd);
            }

        } else std::cout << "no match" << std::endl;

        //std::cout << readBuffer << std::endl;
    } else std::cout << "no curl" << std::endl;

    //tuples = std::vector<ByteData>();
}
