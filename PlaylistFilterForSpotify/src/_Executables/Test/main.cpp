#include <cassert>

#include "DynamicBitset/DynamicBitset.h"

int main()
{
    {
        DynBitset a{32};
        DynBitset b{32};
        a.setBit(1);
        b.setBit(4);
        DynBitset c = a | b;
        const uint32_t& test = c.getInternal()[0];
        assert(test == 0b10010);
        c.clearBit(1);
        assert(test == 0b10000);
        c.setBit(2);
        assert(test == 0b10100);
        c.toggleBit(4);
        assert(test == 0b00100);
        c.toggleBit(4);
        assert(test == 0b10100);
    }

    {
        DynBitset a{65};
        assert(a.getInternal().size() == 3);
        assert(a.getSize() == 65);
        assert(a == false);
        a.setBit(64);
        assert(a == true);
    }

    {
        DynBitset a{65};
        a.setBit(64);
        assert(a.getFirstBitSet() == 64);
        a.setBit(23);
        assert(a.getFirstBitSet() == 23);
        a.setBit(33);
        assert(a.getFirstBitSet() == 23);
        a.toggleBit(23);
        assert(a.getFirstBitSet() == 33);
    }
}
