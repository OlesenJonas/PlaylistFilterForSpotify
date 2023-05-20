#include <cassert>
#include <functional>

#include "DynamicBitset.hpp"

DynBitset::DynBitset(uint32_t _size) : size(_size)
{
    internal.resize(UintDivAndCeil(size, 32), 0);
}

DynBitset& DynBitset::operator=(uint32_t val)
{
    size = 32;
    internal.resize(1, val);
    return *this;
}
DynBitset& DynBitset::operator=(uint64_t val)
{
    size = 64;
    uint32_t first = val & 0xFFFFFFFFU;
    uint32_t second = val >> 32U;
    internal.resize(2);
    internal[0] = first;
    internal[1] = second;
    return *this;
}

DynBitset operator^(const DynBitset& lhs, const DynBitset& rhs)
{
    return DynBitset{lhs, rhs, std::bit_xor<>()};
}
DynBitset operator&(const DynBitset& lhs, const DynBitset& rhs)
{
    return DynBitset{lhs, rhs, std::bit_and<>()};
}
DynBitset operator|(const DynBitset& lhs, const DynBitset& rhs)
{
    return DynBitset{lhs, rhs, std::bit_or<>()};
}

DynBitset::operator bool() const
{
    if(size == 0)
    {
        return false;
    }
    bool anyBitSet = false;
    for(auto i = 0; i < internal.size() - 1; i++)
    {
        if(internal[i] != 0U)
        {
            return true;
        }
    }
    // bits left over = size % 32
    uint32_t lastBits = internal[internal.size() - 1];
    lastBits &= ((~0U) >> (32 - (size % 32)));
    return lastBits != 0U;
}
void DynBitset::clear()
{
    std::fill(internal.begin(), internal.end(), 0U);
}
bool DynBitset::getBit(uint32_t index) const
{
    assert(index < size);
    uint32_t dwordIndex = index / 32;
    return (internal[dwordIndex] & (1U << index % 32)) != 0U;
}
void DynBitset::setBit(uint32_t index)
{
    assert(index < size);
    uint32_t dwordIndex = index / 32;
    internal[dwordIndex] |= (1U << index % 32);
}
void DynBitset::clearBit(uint32_t index)
{
    assert(index < size);
    uint32_t dwordIndex = index / 32;
    internal[dwordIndex] &= ~(1U << index % 32);
}
void DynBitset::toggleBit(uint32_t index)
{
    assert(index < size);
    uint32_t dwordIndex = index / 32;
    internal[dwordIndex] ^= (1U << index % 32);
}
uint32_t DynBitset::getFirstBitSet() const
{
    for(auto i = 0; i < internal.size() - 1; i++)
    {
        if(internal[i] != 0U)
        {
            // todo: ifdef the MSVC specific name for this
            return 32 * i + __builtin_ctz(internal[i]);
        }
    }
    uint32_t lastBits = internal[internal.size() - 1];
    lastBits &= ((~0U) >> (32 - (size % 32)));
    if(lastBits != 0U)
    {
        return (internal.size() - 1) * 32 + __builtin_ctz(lastBits);
    }
    return ~0U;
}

bool DynBitset::resize(uint32_t nSize)
{
    size = nSize;
    uint32_t nInternalSize = UintDivAndCeil(nSize, 32);
    if(nInternalSize != internal.size())
    {
        internal.resize(nInternalSize, 0);
        return true;
    }
    return false;
}
uint32_t DynBitset::getSize() const
{
    return size;
}
const std::vector<uint32_t>& DynBitset::getInternal()
{
    return internal;
}