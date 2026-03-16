#pragma once
#include "symbol.h"
#include <set>
#include <iterator>

class SymbolTable : public std::multiset<Symbol, SymbolComparer>
{
public:
    const_iterator find(size_t address) const
    {
        if (empty()) return end();
        auto it = upper_bound(address);
        if (it == begin()) return end();
        auto curr = std::prev(it);
        if (address >= curr->address && address < curr->address + curr->size) return curr;
        if (address == curr->address) return curr;
        return end();
    }

    iterator find(size_t address)
    {
        if (empty()) return end();
        auto it = upper_bound(address);
        if (it == begin()) return end();
        auto curr = std::prev(it);
        if (address >= curr->address && address < curr->address + curr->size) return curr;
        if (address == curr->address) return curr;
        return end();
    }
};
