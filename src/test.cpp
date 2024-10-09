#include <iostream>
#include <chrono>
#include <cstdint>

/** This file is just used for some mini-tests when I don't understand something that's going on.
 *  It can generally be ignored.
 */

int main () {
	std::chrono::time_point now = std::chrono::system_clock::now();
	std::chrono::duration epoch = now.time_since_epoch();
	uint32_t seconds_since_epoch = std::chrono::duration_cast<std::chrono::seconds>(epoch).count();
	std::cout << seconds_since_epoch << std::endl;
	return 0;
}
