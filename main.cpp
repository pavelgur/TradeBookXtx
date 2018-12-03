#include <cassert>
#include <cstdio>
#include <iostream>
#include <sstream>
#include <cstring>

#include "book.h"
#include "io.h"
#include "tests.h"

class TStdOutMatchAcceptor final: public IMatchAcceptor {
public:
    void Match(const TTrade& trade) override {
        printf("M %u %u %u %u\n", trade.BuyOrderID, trade.SellOrderID, trade.Price, trade.Quantity);
    }
    void BookLine(const TExtendedOrder& order) override {
        printf("O %c %u %u %u\n", order.SideChar(), order.UniqID, order.Price, order.Quantity);
    }
    void FinishReport() override {
        printf("\n");
    }
};

static void FeedFromStdinToStdout(IBook& book) {
    TStdOutMatchAcceptor acc;

    for (std::string line; std::getline(std::cin, line);) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        assert(line.size() >= 3);
        std::istringstream ss(line);

        char code;
        uint32_t orderId;
        ss >> code;

        if (code == 'C') {
            // cancel
            ss >> orderId;
            if (!book.CancelOrder(orderId, acc)) {
                std::cerr << "Failed to delete order " << orderId << std::endl;
            }
            continue;
        }

        assert(code == 'L' || code == 'I');
        TOrder order;
        {
            char side;
            ss >> side >> order.UniqID >> order.Price >> order.Quantity;
            order.Side = order.SideFromChar(side);
        }

        if (code == 'I') {
            ss >> order.Peak;
        }

        book.AcceptOrder(order, acc);
    }
}

int main(const int argc, const char** argv) {
    const auto book = MakeBook();

    if (argc >= 2) {
        const auto mode = argv[argc - 1];
        if (!strcmp(mode, "profile")) {
            printf("Program started in profiling mode\n");
            NTesting::FeedRandom(*book);
            return EXIT_SUCCESS;
        }
        if (!strcmp(mode, "test")) {
            printf("Program started in testing mode\n");
            NTesting::Test(*book);
            return EXIT_SUCCESS;
        }
    }

    FeedFromStdinToStdout(*book);
    return EXIT_SUCCESS;
}
