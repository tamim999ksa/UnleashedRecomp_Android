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
        if (it == begin())
        {
            return end();
        }

        // Iterate backwards from upper_bound to find the first (closest) symbol that contains the address.
        // multiset is sorted by address, so the first one we find moving backward is the best candidate.
        auto curr = std::prev(it);
        while (true)
        {
            if (address >= curr->address && address < curr->address + curr->size)
            {
                return curr;
            }
            // Also handle 0-sized exact matches
            if (address == curr->address)
            {
                return curr;
            }

            if (curr == begin())
            {
                break;
            }
            curr = std::prev(curr);

            // Optimization: if we moved so far back that the address is significantly beyond the possible range, stop.
            // Assuming no symbol is larger than 1MB for sanity.
            if (address > curr->address + 1024 * 1024)
            {
                break;
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
        if (it == begin())
        {
            return end();
        }

        auto curr = std::prev(it);
        while (true)
        {
            if (address >= curr->address && address < curr->address + curr->size)
            {
                return curr;
            }
            if (address == curr->address)
            {
                return curr;
            }

            if (curr == begin())
            {
                break;
            }
            curr = std::prev(curr);

            if (address > curr->address + 1024 * 1024)
            {
                break;
            }
        }

        return end();
    }
};
