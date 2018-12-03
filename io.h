#pragma once

#include "lib.h"

class IMatchAcceptor {
public:
    virtual ~IMatchAcceptor() = default;

    virtual void StartReport() {
    }

    virtual void Match(const TTrade& /*trade*/) {
    }

    virtual void BookLine(const TExtendedOrder& /*order*/) {
    }

    virtual void FinishReport() {
    }
};
