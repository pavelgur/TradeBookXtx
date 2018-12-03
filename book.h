#pragma once

#include <memory>

#include "lib.h"
#include "io.h"

class IBook {
public:
    virtual ~IBook() = default;
    virtual void AcceptOrder(TOrder order, IMatchAcceptor& acc) = 0;
    virtual bool CancelOrder(const uint32_t orderId, IMatchAcceptor& acc) = 0;
    virtual void ListBook(IMatchAcceptor& acc) const = 0;
};

std::unique_ptr<IBook> MakeBook();
