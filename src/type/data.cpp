#include "dross/type/data.h"

namespace dross {

data::data() = default;

data::data(const data&) = default;

data::~data() = default;

bool data::empty() const noexcept
{
    return true;
}

size_t data::length() const noexcept
{
    return 0z;
}

}
