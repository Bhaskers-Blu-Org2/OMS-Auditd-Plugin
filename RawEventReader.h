/*
    microsoft-oms-auditd-plugin

    Copyright (c) Microsoft Corporation

    All rights reserved.

    MIT License

    Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the ""Software""), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED *AS IS*, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#ifndef AUOMS_RAWEVENTREADER_H
#define AUOMS_RAWEVENTREADER_H

#include "IEventReader.h"
#include "Logger.h"

class RawEventReader: public IEventReader {
public:
    ssize_t ReadEvent(void *buf, size_t buf_size, IReader* reader, std::function<bool()> fn) override {
        uint32_t event_size;

        if (buf_size < sizeof(event_size)) {
            return IO::FAILED;
        }

        // Read header (SIZE, MSG_NUM)
        ssize_t ret = reader->ReadAll(&event_size, sizeof(uint32_t), fn);
        if (ret != IO::OK) {
            if (ret == IO::FAILED) {
                Logger::Info("RawEventReader: Unexpected error while reading message header");
            }
            return ret;
        }

        if (event_size > buf_size) {
            Logger::Info("RawEventReader: Message size (%d) in header is too large (> %d)", event_size, buf_size);
            return IO::FAILED;
        }

        // Read message
        *reinterpret_cast<uint32_t*>(buf) = event_size;
        ret = reader->ReadAll(reinterpret_cast<uint32_t*>(buf)+1, event_size-sizeof(uint32_t), fn);
        if (ret != IO::OK) {
            if (ret == IO::FAILED) {
                Logger::Info("RawEventReader: Unexpected error while reading message");
            }
            return ret;
        }
        return event_size;
    };

    ssize_t WriteAck(const Event& event, IWriter* writer) override {
        std::array<uint8_t, 8+4+8> ack_data;
        *reinterpret_cast<uint64_t*>(ack_data.data()) = event.Seconds();
        *reinterpret_cast<uint32_t*>(ack_data.data()+8) = event.Milliseconds();
        *reinterpret_cast<uint64_t*>(ack_data.data()+12) = event.Serial();

        return writer->WriteAll(ack_data.data(), ack_data.size());
    };

    ssize_t WriteAck(const EventId& event_id, IWriter* writer) override {
        std::array<uint8_t, 8+4+8> ack_data;
        *reinterpret_cast<uint64_t*>(ack_data.data()) = event_id.Seconds();
        *reinterpret_cast<uint32_t*>(ack_data.data()+8) = event_id.Milliseconds();
        *reinterpret_cast<uint64_t*>(ack_data.data()+12) = event_id.Serial();

        return writer->WriteAll(ack_data.data(), ack_data.size());
    };
};

#endif //AUOMS_RAWEVENTREADER_H