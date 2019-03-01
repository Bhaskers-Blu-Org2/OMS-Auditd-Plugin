/*
    microsoft-oms-auditd-plugin

    Copyright (c) Microsoft Corporation

    All rights reserved.

    MIT License

    Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the ""Software""), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED *AS IS*, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <iostream>

#include "RawEventRecord.h"

using namespace std::literals;

class RecordFieldIterator {
public:
    explicit RecordFieldIterator(std::string_view str): _str(str), _idx(0) {}

    bool next() {
        if (_idx == std::string_view::npos || _idx >= _str.size()) {
            return false;
        }
        auto idx = _str.find_first_of(" \n"sv, _idx);
        if (idx == std::string_view::npos) {
            idx = _str.size();
        }
        _val = _str.substr(_idx, idx-_idx);
        // For certain record types, the data is inside a "msg='...'" field.
        if (_val.substr(0, 5) == "msg='"sv) {
            _idx+=5;
            return next();
        } else {
            _idx = _str.find_first_not_of(" \n"sv, idx);
        }
        // The field might have been inside a "msg='...'" so ignore the "'"
        if (_val.back() == '\'') {
            _val = _val.substr(0, _val.size()-1);
        }
        return true;
    }

    inline std::string_view value() {
        return _val;
    }

    inline std::string_view remainder() {
        return _str.substr(_idx);
    }

private:
    std::string_view _str;
    std::string_view _val;
    size_t _idx;
};

bool starts_with(const std::string_view& str, const std::string_view& prefix) {
    if (prefix.size() > str.size()) {
        return false;
    }
    return str.substr(0, prefix.size()) == prefix;
}

bool ends_with(const std::string_view& str, const std::string_view& suffix) {
    if (suffix.size() > str.size()) {
        return false;
    }
    return str.substr(str.size()-suffix.size(), suffix.size()) == suffix;
}

bool RawEventRecord::Parse(RecordType record_type, size_t size) {
    _size = size;
    _record_type = record_type;
    _record_fields.resize(0);
    std::string_view str = std::string_view(_data.data(), _size);
    RecordFieldIterator itr(str);
    if (!itr.next()) {
        return false;
    }

    // Event record prefixes have three possible formats:
    //  From the dispatcher (audisp)
    //      node=<> type=<> msg=audit(<sec>.<msec>:<serial>): <...>
    //      type=<> msg=audit(<sec>.<msec>:<serial>): <...>
    //  From the kernel:
    //      audit(<sec>.<msec>:<serial>): <...>
    //

    if (starts_with(itr.value(), "node="sv)) {
        _node = itr.value().substr(5);
        if (!itr.next()) {
            return false;
        }
    } else {
        _node = std::string_view();
    }

    if (starts_with(itr.value(), "type="sv)) {
        _type_name = itr.value().substr(5);
        if (!itr.next()) {
            return false;
        }
    } else {
        _type_name = std::string_view();
    }

    if (_type_name.empty() && _record_type != RecordType::UNKNOWN) {
        _type_name = LookupTables::RecordTypeCodeToString(_record_type);
    } else if (!_type_name.empty() && _record_type == RecordType::UNKNOWN) {
        _record_type = LookupTables::RecordTypeNameToCode(std::string(_type_name));
    }

    auto val = itr.value();
    if (starts_with(val, "msg="sv)) {
        val = val.substr(4);
    }

    if (starts_with(val, "audit("sv) && ends_with(val, "):"sv)) {
        auto event_id_str = val.substr(10, val.size()-12);
        auto pidx = event_id_str.find_first_of('.');
        if (pidx == std::string_view::npos) {
            return false;
        }
        auto cidx = event_id_str.find_first_of(':', pidx);
        if (cidx == std::string_view::npos) {
            return false;
        }
        auto sec_str = event_id_str.substr(0, pidx);
        auto msec_str = event_id_str.substr(pidx+1, 3);
        auto ser_str = event_id_str.substr(cidx+1);
        try {
            uint64_t sec = static_cast<uint64_t>(std::stoll(std::string(sec_str.data(), sec_str.size()), 0));
            uint32_t msec = static_cast<uint32_t>(std::stoi(std::string(msec_str.data(), msec_str.size()), 0));
            uint64_t ser = static_cast<uint64_t>(std::stoll(std::string(ser_str.data(), ser_str.size()), 0));
            _event_id = EventId(sec, msec, ser);
        } catch (std::invalid_argument&) {
            _event_id = EventId();
            return false;
        } catch (std::out_of_range&) {
            _event_id = EventId();
            return false;
        }

        while(itr.next()) {
            _record_fields.push_back(itr.value());
        }
        return true;
    }

    return false;
}

int RawEventRecord::AddRecord(EventBuilder& builder) {
    uint16_t num_fields = static_cast<uint16_t>(_record_fields.size());
    if (!_node.empty()) {
        num_fields++;
    }

    auto ret = builder.BeginRecord(static_cast<uint32_t>(_record_type), _type_name, std::string_view(_data.data(), _size), num_fields);
    if (ret != 1) {
        return ret;
    }

    if (!_node.empty()) {
        ret = builder.AddField("node", _node, nullptr, field_type_t::UNCLASSIFIED);
        if (ret != 1) {
            return ret;
        }
    }

    for (auto f: _record_fields) {
        auto idx = f.find_first_of('=');
        if (idx == std::string_view::npos) {
            // TODO:: Log this??
            continue;
        }
        ret = builder.AddField(f.substr(0, idx), f.substr(idx+1), nullptr, field_type_t::UNCLASSIFIED);
        if (ret != 1) {
            return ret;
        }
    }

    return builder.EndRecord();
}