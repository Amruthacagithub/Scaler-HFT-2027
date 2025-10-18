#include <string>
#include <sstream>
#include <iomanip>
#include <cstdint>

std::string format_price(double p) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(2) << p;
    return ss.str();
}

std::string format_qty(uint64_t q) {
    return std::to_string(q);
}
