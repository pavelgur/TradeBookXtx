#include "tests.h"

#include <algorithm>
#include <cassert>
#include <unordered_map>
#include <vector>

namespace NTesting {

    enum ECommand {
        LIMIT,
        ICEBERG,
        CANCEL,
    };

    void FeedRandom(IBook& book) {
        IMatchAcceptor acc;
        srand(890);

        for (auto i = 0u;; ++i) {
            const auto price = 100u + rand() % 25;
            const auto quantity = 200u + rand() % 300;
            const auto peak = rand() % 50u;

            switch (rand() % 3) {
            case LIMIT:
                book.AcceptOrder(TOrder{
                                     ESide(rand() % 2),
                                     i, // uniqID
                                     price,
                                     quantity,
                                     0u // peak
                                 }, acc);
                break;
            case ICEBERG:
                book.AcceptOrder(TOrder{
                                     ESide(rand() % 2),
                                     i, // uniqID
                                     price,
                                     quantity,
                                     peak,
                                 }, acc);
                break;
            case CANCEL:
                book.CancelOrder(rand() % std::max(i, 1u), acc);
                break;
            }
        }
    }

    class TTestAcceptor final: public IMatchAcceptor {
    public:
        void StartReport() override {
            Orders.clear(), Trades.clear();
            Volume = 0, Cost = 0;
        }

        void BookLine(const TExtendedOrder& order) override {
            Orders.push_back(order);
        }

        void Match(const TTrade& trade) override {
            Trades.push_back(trade);
            Volume += trade.Quantity;
            Cost += trade.Quantity * trade.Price;
        }

    public:
        std::vector<TExtendedOrder> Orders;
        std::vector<TTrade> Trades;
        uint32_t Volume = 0, Cost = 0;
    };

    void Test(IBook& book) {
        TTestAcceptor acc;
        srand(777);

        const auto tradeCmp = [] (const TTrade& a, const TTrade& b) {
            return std::tie(a.BuyOrderID, a.SellOrderID, a.Price, a.Quantity) < std::tie(b.BuyOrderID, b.SellOrderID, b.Price, b.Quantity);
        };

        std::vector<TTrade> expectedTrades;
        for (auto i = 0u; i < 10000u; ++i) {
            const auto price = 100u + rand() % 25;
            const auto quantity = 200u + rand() % 300;
            const auto peak = rand() % 50u;
            const ESide side = ESide(rand() % 2);

            uint32_t expectedVolume = 0, expectedCost = 0;
            int expectedBookSizeDelta = 0;
            expectedTrades.clear();
            const auto calcExpected = [&] (const auto begin, const auto end, const ESide matchSide) {
                auto iter = std::lower_bound(begin, end, matchSide, [&] (const TOrder& o, ESide matchSide) {
                    return (o.Side != matchSide ? 1 : 0);
                });
                auto qLeft = quantity;
                for (; qLeft > 0 && iter != end && iter->Side == matchSide;) {
                    switch(matchSide) {
                    case ESide::BUY:
                        if (iter->Price < price) {
                            ++expectedBookSizeDelta;
                            return;
                        }
                        break;
                    case ESide::SELL:
                        if (iter->Price > price) {
                            ++expectedBookSizeDelta;
                            return;
                        }
                        break;
                    }

                    const auto price = iter->Price;
                    auto peaksEnd = iter;
                    while (peaksEnd != end && peaksEnd->Price == iter->Price) {
                        ++peaksEnd;
                    }
                    std::unordered_map<uint32_t, uint32_t> matcher2volume;
                    uint32_t prevVol = qLeft;
                    for (auto it = iter; qLeft > 0;) {
                        if (auto& matchQ = it->Quantity) {
                            const auto q = std::min(qLeft, matchQ);
                            expectedVolume += q;
                            expectedCost += q * price;
                            qLeft -= q;
                            matchQ -= q;
                            if (matchQ == 0) {
                                matchQ = std::min(it->Peak, it->Hidden);
                                it->Hidden -= matchQ;
                                if (matchQ == 0) {
                                    --expectedBookSizeDelta;
                                }
                            }

                            matcher2volume[it->UniqID] += q;
                        }
                        ++it;

                        if (it == peaksEnd) {
                            if (prevVol == qLeft) {
                                break;
                            } else {
                                prevVol = qLeft;
                                it = iter;
                            }
                        }
                    }

                    for (const auto& pair : matcher2volume) {
                        switch(matchSide) {
                        case ESide::BUY: {
                            expectedTrades.push_back(TTrade{pair.first, i, price, pair.second});
                            break;
                        }
                        case ESide::SELL:
                            expectedTrades.push_back(TTrade{i, pair.first, price, pair.second});
                            break;
                        }
                    }

                    iter = peaksEnd;
                }
                if (qLeft) {
                    ++expectedBookSizeDelta;
                }
            };

            const auto command = ECommand(rand() % 3);
            if (acc.Orders.size()) {
                switch(side) {
                case ESide::BUY: {
                    calcExpected(acc.Orders.begin(), acc.Orders.end(), ESide::SELL);
                    break;
                }
                case ESide::SELL: {
                    calcExpected(acc.Orders.rbegin(), acc.Orders.rend(), ESide::BUY);
                    break;
                }
                }
            } else if (command != CANCEL) {
                expectedBookSizeDelta = 1;
            }

            const int prevSize = acc.Orders.size();

            switch (command) {
            case LIMIT:
                book.AcceptOrder(TOrder{
                                     side,
                                     i, // uniqID
                                     price,
                                     quantity,
                                     0u // peak
                                 }, acc);
                break;
            case ICEBERG:
                book.AcceptOrder(TOrder{
                                     side,
                                     i, // uniqID
                                     price,
                                     quantity,
                                     peak,
                                 }, acc);
                break;
            case CANCEL:
                if (book.CancelOrder(rand() % std::max(i, 1u), acc)) {
                    expectedBookSizeDelta = -1;
                } else {
                    expectedBookSizeDelta = 0;
                }
                break;
            }
            if (command != CANCEL) {
                if ((acc.Cost != expectedCost || acc.Volume != expectedVolume)) {
                    printf("TESTING FAILED (volume/cost).\n");
                    return;
                }
                std::sort(expectedTrades.begin(), expectedTrades.end(), tradeCmp);
                std::sort(acc.Trades.begin(), acc.Trades.end(), tradeCmp);
                if (expectedTrades != acc.Trades) {
                    printf("TESTING FAILED (trades).\n");
                    return;
                }
            }
            {
                const int newSize = acc.Orders.size();
                if (expectedBookSizeDelta != newSize - prevSize) {
                    printf("TESTING FAILED (book size).\n");
                    return;
                }
            }
        }

        printf("Testing OK!\n");
    }
}
