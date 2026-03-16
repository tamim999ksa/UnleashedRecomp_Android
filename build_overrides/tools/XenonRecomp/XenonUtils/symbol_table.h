#pragma once
#include "symbol.h"
#include <set>
#include <iterator>

class SymbolTable : public std::multiset<Symbol, SymbolComparer>
{
public:
    const_iterator find(size_t address) const
    {
        if (empty())
        {
            return end();
        }

        auto it = upper_bound(address);
        if (it != begin())
        {
            it = std::prev(it);
            // Match if address is within range, OR if it exactly matches the start (for 0-sized symbols)
            if ((address >= it->address && address < it->address + it->size) || (address == it->address))
            {
                return it;
            }
        }

        return end();
    }

    iterator find(size_t address)
    {
        if (empty())
        {
            return end();
        }

        auto it = upper_bound(address);
        if (it != begin())
        {
            it = std::prev(it);
            if ((address >= it->address && address < it->address + it->size) || (address == it->address))
            {
                return it;
            }
        }

        return end();
    }
};
