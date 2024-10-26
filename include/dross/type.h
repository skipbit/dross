#pragma once

#include <dross/type/array.h>
#include <dross/type/dictionary.h>
#include <dross/type/number.h>
#include <dross/type/string.h>

#include <vector>

namespace dross {

std::vector<std::string> split(const std::string& s, const char& d);

}
