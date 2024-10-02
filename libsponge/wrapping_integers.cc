#include "wrapping_integers.hh"

#include <iostream>
// Dummy implementation of a 32-bit wrapping integer

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) { return isn + static_cast<uint32_t>(n); }

//! Transform a WrappingInt32 into an "absolute" 64-bit sequence number (zero-indexed)
//! \param n The relative sequence number
//! \param isn The initial sequence number
//! \param checkpoint A recent absolute 64-bit sequence number
//! \returns the 64-bit sequence number that wraps to `n` and is closest to `checkpoint`
//!
//! \note Each of the two streams of the TCP connection has its own ISN. One stream
//! runs from the local TCPSender to the remote TCPReceiver and has one ISN,
//! and the other stream runs from the remote TCPSender to the local TCPReceiver and
//! has a different ISN.

uint64_t unwrap(WrappingInt32 n, WrappingInt32 isn, uint64_t checkpoint) {
    // Calculate the offset from ISN
    // if n < isn, equal: n - isn + (1u << 32)
    uint64_t offset;
    if (n - isn < 0) {
        offset = static_cast<uint64_t>(n - isn + (1l << 32));
    } else {
        offset = static_cast<uint64_t>(n - isn);
    }

    // If offset is already greater than checkpoint, it's the absolute sequence number
    if (offset > checkpoint) {
        return offset;
    }
    // Align offset with checkpoint's higher 32 bits
    offset |= (checkpoint & 0xFFFFFFFF00000000);

    if (offset > checkpoint)
        offset -= 1ULL << 32;

    // Calculate the next possible sequence number (one wrap higher)
    uint64_t upper_bound = offset + (1ULL << 32);

    // Return the closer value to checkpoint
    return (checkpoint - offset < upper_bound - checkpoint) ? offset : upper_bound;
}
