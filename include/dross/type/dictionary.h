#pragma once

#include <memory>

namespace dross {

class dictionary final {
public:
    dictionary();
    dictionary(const dictionary&);
    ~dictionary();

    bool equals(const dictionary&) const;
    bool operator==(const dictionary&) const;
    bool operator!=(const dictionary&) const;

    dictionary& operator=(const dictionary&);

private:
    class storage;
    std::unique_ptr<storage> _store;
};

}
