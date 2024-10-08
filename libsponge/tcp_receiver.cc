#include "tcp_receiver.hh"

#include <algorithm>
#include <cstddef>
#include <cstdint>

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    const TCPHeader head = seg.header();
    // Ignore segments without SYN if we haven't received a SYN yet
    if (!head.syn && !_synReceived) {
        return;
    }

    // Extract data from the payload
    string data = seg.payload().copy();

    bool eof = false;

    // Handle the first SYN packet
    if (head.syn && !_synReceived) {
        _synReceived = true;
        _isn = head.seqno;

        // Check if this is also a FIN packet (rare, but possible)
        if (head.fin) {
            _finReceived = eof = true;
        }

        // Push any data in the SYN packet to the reassembler
        // Index is 0 because this is the start of the stream, support for "TCP Fast Open"（TFO)
        _reassembler.push_substring(data, 0, eof);
        return;
    }

    // FIN received
    if (_synReceived && head.fin) {
        _finReceived = eof = true;
    }

    // convert the seqno into absolute seqno
    uint64_t checkpoint = _reassembler.ack_index();
    uint64_t abs_seqno = unwrap(head.seqno, _isn, checkpoint);
    uint64_t stream_idx = abs_seqno - _synReceived;

    // push the data into stream reassembler
    _reassembler.push_substring(data, stream_idx, eof);
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if (!_synReceived) {
        return nullopt;
    }
    return wrap(_reassembler.ack_index() + 1 + (_reassembler.empty() && _finReceived), _isn);
}

size_t TCPReceiver::window_size() const { return _capacity - _reassembler.stream_out().buffer_size(); }
