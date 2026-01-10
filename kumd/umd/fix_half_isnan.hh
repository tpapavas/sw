#include <half.h>
#include <cmath>

namespace std {
    inline bool isnan(const half_float::half& h) {
        return std::isnan(static_cast<float>(h));
    }
}
