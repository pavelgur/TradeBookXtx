#pragma once

#include <cstdint>

enum class ESide {
    BUY,
    SELL,
};

struct TOrder {
    static constexpr ESide SideFromChar(char side) noexcept {
        return side == 'B' ? ESide::BUY : ESide::SELL;
    }

    char SideChar() const noexcept {
        return Side == ESide::BUY ? 'B' : 'S';
    }

    bool operator<(const TOrder& other) const noexcept {
        return Price < other.Price;
    }

    bool operator>(const TOrder& other) const noexcept {
        return Price > other.Price;
    }

    bool operator==(const TOrder& other) const noexcept {
        return Price == other.Price;
    }

    ESide Side = ESide::BUY;
    uint32_t UniqID = 0;
    uint32_t Price = 0;
    mutable uint32_t Quantity = 0;
    uint32_t Peak = 0;
};

struct TExtendedOrder : public TOrder {
    TExtendedOrder() = default;
    TExtendedOrder(const TOrder& order)
        : TOrder(order)
    {}

    uint32_t TotalQuantity() const noexcept {
        return Quantity + Hidden;
    }

    mutable uint32_t Hidden = 0;
};

struct TTrade {
    uint32_t BuyOrderID;
    uint32_t SellOrderID;
    uint32_t Price;
    uint32_t Quantity;

    bool operator==(const TTrade& b) const {
        return std::tie(BuyOrderID, SellOrderID, Price, Quantity) == std::tie(b.BuyOrderID, b.SellOrderID, b.Price, b.Quantity);
    }
};
