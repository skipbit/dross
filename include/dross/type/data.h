#pragma once

#include <cstddef>

namespace dross {

class data {
public:
    data();
    data(const data&);
    ~data();

    bool empty() const noexcept;
    size_t length() const noexcept;

private:
};

}
