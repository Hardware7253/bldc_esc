#include "map.h"

// Maps a number with range [num_low, num_high] to a new output with range [out_low, out_high]
// This works fine with negative numbers
// The full int32 range can't be used because a multiply by 1000 is performed to keep precision
// The output range should be less than 2 million to avoid integer overflow errors
int32_t map(int32_t num, int32_t num_low, int32_t num_high, int32_t out_low, int32_t out_high) {
    int32_t out = ((1000 * (num - num_low) / (num_high - num_low)) * (out_high - out_low) / 1000 + out_low);

    // Clamp output
    if (out > out_high) {
        return out_high;
    } else if (out < out_low) {
        return out_low;
    }
    return out;
}