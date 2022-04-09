#pragma once

#include <cstdint>
#include <vector>

class DynBitset
{
  public:
    explicit DynBitset(uint32_t _size);

    DynBitset& operator=(uint32_t val);
    DynBitset& operator=(uint64_t val);
    // DynBitset& operator^=(const DynBitset& other);

    template <class Func>
    DynBitset(const DynBitset& lhs, const DynBitset& rhs, Func func) : size(std::max(lhs.size, rhs.size))
    {
        internal.resize(UintDivAndCeil(size, 32), 0U);
        setFromOperator(lhs, rhs, func);
    }

    friend DynBitset operator^(const DynBitset& lhs, const DynBitset& rhs);
    friend DynBitset operator&(const DynBitset& lhs, const DynBitset& rhs);
    friend DynBitset operator|(const DynBitset& lhs, const DynBitset& rhs);
    // todo: bitshift not yet implemented

    operator bool() const; // NOLINT
    [[nodiscard]] bool getBit(uint32_t index) const;
    void setBit(uint32_t index);
    void clearBit(uint32_t index);
    void toggleBit(uint32_t index);
    // returns 0xffffffff is no bit was set
    [[nodiscard]] uint32_t getFirstBitSet() const;

    // returns true if internal resize happended
    bool resize(uint32_t nSize);
    [[nodiscard]] uint32_t getSize() const;
    const std::vector<uint32_t>& getInternal();

  private:
    template <class Func>
    // fill the values of this DynBitset with (lhs func rhs)
    // if any operands (lhs, rhs) are smaller than this, elements will be treated as 0
    void setFromOperator(const DynBitset& lhs, const DynBitset& rhs, Func func)
    {
        assert(size >= lhs.size && size >= rhs.size);
        assert(internal.size() >= lhs.internal.size() && internal.size() >= rhs.internal.size());
        const bool lhsLarger = lhs.internal.size() > rhs.internal.size();
        const DynBitset& larger = lhsLarger ? lhs : rhs;
        const DynBitset& smaller = lhsLarger ? rhs : lhs;
        for(auto i = 0; i < smaller.internal.size(); i++)
        {
            internal[i] = func(smaller.internal[i], larger.internal[i]);
        }
        for(auto i = smaller.internal.size(); i < larger.internal.size(); i++)
        {
            internal[i] = func(larger.internal[i], 0U);
        }
        for(auto i = larger.internal.size(); i < internal.size(); i++)
        {
            internal[i] = func(0U, 0U);
        }
    }

    static inline uint32_t UintDivAndCeil(uint32_t x, uint32_t y) // NOLINT
    {
        return (x + y - 1) / y;
    }

    uint32_t size = 0;
    std::vector<uint32_t> internal;
};