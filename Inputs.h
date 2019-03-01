/*
    microsoft-oms-auditd-plugin

    Copyright (c) Microsoft Corporation

    All rights reserved.

    MIT License

    Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the ""Software""), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED *AS IS*, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#ifndef AUOMS_UNIXDOMAINLISTENER_H
#define AUOMS_UNIXDOMAINLISTENER_H

#include "IO.h"
#include "RunBase.h"
#include "InputBuffer.h"
#include "Input.h"

#include <string>
#include <unordered_map>

class Inputs: public RunBase {
public:
    explicit Inputs(std::string addr): _addr(std::move(addr)), _listener_fd(-1), _buffer(std::make_shared<InputBuffer>()) {}

    bool Initialize();

    bool HandleData(std::function<void(void*,size_t)> fn) {
        return _buffer->HandleData(std::move(fn));
    }

protected:
    void on_stopping() override;
    void on_stop() override;
    void run() override;

private:
    std::string _addr;
    std::function<void(IOBase&)> _handler_fn;
    int _listener_fd;
    std::unordered_map<int, std::shared_ptr<Input>> _inputs;
    std::shared_ptr<InputBuffer> _buffer;

    void add_connection(int fd);
};


#endif //AUOMS_UNIXDOMAINLISTENER_H