#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity)
    : unass_base(0)
    , unass_size(0)
    , _eof(0)
    , buffer(capacity, '\0')
    , bitmap(capacity, false)
    , _output(capacity)
    , _capacity(capacity) {}

//! \details This functions calls just after pushing a substring into the
//! _output stream. It aims to check if there exists any contiguous substrings
//! recorded earlier can be push into the stream.
void StreamReassembler::check_contiguous() {
    string tmp = "";
    while (bitmap.front()) {
        // cout<<"check one more contiguous substring"<<endl;
        tmp += buffer.front();
        buffer.pop_front();
        bitmap.pop_front();
        buffer.push_back('\0');
        bitmap.push_back(false);
    }
    if (tmp.length() > 0) {
        // cout << "push one contiguous substring with length " << tmp.length() << endl;
        _output.write(tmp);
        unass_base += tmp.length();
        unass_size -= tmp.length();
    }
}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    // Set EOF flag if this is the last segment
    if (eof) {
        _eof = true;
    }
    size_t len = data.length();

    // Handle empty data with EOF
    if (len == 0 && _eof && unass_size == 0) {
        _output.end_input();
        return;
    }

    // Ignore data beyond capacity
    if (index >= unass_base + _capacity)
        return;

    // Process data that starts at or after unass_base
    if (index >= unass_base) {
        int offset = index - unass_base;

        // Calculate how much of the data can actually be stored
        size_t real_len = min(len, _capacity - _output.buffer_size() - offset);

        // Check if all data can be stored
        if (real_len < len) {
            _eof = false;
        }
        for (size_t i = 0; i < real_len; i++) {
            if (bitmap[i + offset])
                continue;
            buffer[i + offset] = data[i];
            bitmap[i + offset] = true;
            unass_size++;
        }
    }
    // Process data that overlaps with unass_base
    else if (index + len > unass_base) {
        int offset = unass_base - index;
        size_t real_len = min(len - offset, _capacity - _output.buffer_size());
        if (real_len < len - offset) {
            _eof = false;
        }
        for (size_t i = 0; i < real_len; i++) {
            if (bitmap[i])
                continue;
            buffer[i] = data[i + offset];
            bitmap[i] = true;
            unass_size++;
        }
    }
    check_contiguous();

    // End input only if EOF is set and all data has been reassembled
    if (_eof && unass_size == 0) {
        _output.end_input();
    }
}

size_t StreamReassembler::unassembled_bytes() const { return unass_size; }

bool StreamReassembler::empty() const { return unass_size == 0; }

size_t StreamReassembler::ack_index() const { return unass_base; }