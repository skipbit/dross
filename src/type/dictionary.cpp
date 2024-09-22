#include "dross/type/dictionary.h"

#include "dross/type/value.h"
#include <map>
#include <memory>
#include <string>

namespace dross {

class dictionary::storage {
public:
    std::map<std::string, dross::value> properties;
};

dictionary::dictionary()
    : _store(std::make_unique<storage>())
{
}

dictionary::dictionary(const dictionary& v)
    : _store(std::make_unique<storage>(*(v._store)))
{
}

dictionary::~dictionary() = default;

bool dictionary::equals(const dictionary& d) const
{
    return _store->properties == d._store->properties;
}

bool dictionary::operator==(const dictionary& d) const
{
    return equals(d);
}

bool dictionary::operator!=(const dictionary& d) const
{
    return (! equals(d));
}

dictionary& dictionary::operator=(const dictionary& d)
{
    _store->properties = d._store->properties;
    return *this;
}

}
