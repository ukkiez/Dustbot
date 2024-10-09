#include <iostream>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <random>

/** This file is just used for some mini-tests when I don't understand something that's going on.
 *  It can generally be ignored.
 */

std::string generate_session_token(uint32_t length) {
    std::ifstream urandom("/dev/urandom");
    std::string chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!#$%&'*+-.^_`|~";
    std::string sessionId;
    sessionId.reserve(length);

    std::random_device rd;
    std::mt19937 generator(rd());
    std::uniform_int_distribution<int> distribution(0, chars.size() - 1);

    for (int i = 0; i < length; ++i) {
        char random_char;
        urandom.read(&random_char, 1);
        int index = distribution(generator) % chars.size();
        sessionId += chars[index];
    }

    urandom.close();
    return sessionId;
}

int main () {
	std::string token = generate_session_token(128);
	std::cout << token << std::endl;
	return 0;
}
