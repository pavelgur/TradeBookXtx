#include <cassert>
#include <set>
#include <unordered_map>

#include "book.h"

namespace {
    template<class TIt>
    void PrintBookRange(TIt it, const TIt end, IMatchAcceptor& matchesAcc) {
        for (; it != end; ++it) {
            assert(it->Quantity != 0);
            assert(it->Price != 0);
            assert(it->Peak == 0 || it->Quantity <= it->Peak);
            matchesAcc.BookLine(*it);
        }
    }

    template<class TBook>
    bool TryDelete(const uint32_t orderId, TBook& orders) {
        auto& book = orders.Book;
        auto& id2Order = orders.Id2Order;

        const auto iter = id2Order.find(orderId);
        if (iter != id2Order.end()) {
            book.erase(iter->second);
            id2Order.erase(iter);
            return true;
        }
        return false;
    }

    template<ESide Side>
    constexpr TTrade MakeTrade(uint32_t takerId, uint32_t makerId, uint32_t price, uint32_t quantity) noexcept {
        switch (Side) {
        case ESide::BUY:
            return {takerId, makerId, price, quantity};
        case ESide::SELL:
            return {makerId, takerId, price, quantity};
        }
    }

    template<ESide Side, class TPutOrders, class TMatchOrders>
    void MatchOrder(TExtendedOrder order, IMatchAcceptor& matchesAcc, TPutOrders& putOrders, TMatchOrders& matchOrders) {
        using TPriceCmp = typename TPutOrders::TPriceComparator; // BUY: >, SELL: <

        auto& putBook = putOrders.Book;
        auto& putId2Order = putOrders.Id2Order;

        auto& matchBook = matchOrders.Book;
        auto& matchId2Order = matchOrders.Id2Order;

        if (!matchBook.empty()) {
            std::unordered_map<uint32_t, uint32_t> maker2volume; // we should optimize it with memory pool, STL doesn't provide that though
            for (auto matchOrder = matchBook.begin(); matchOrder != matchBook.end() && !TPriceCmp()(matchOrder->Price, order.Price);) {
                const auto icePrice = matchOrder->Price;
                maker2volume.clear();

                const auto flushMatches = [&] () {
                    // "Multiple executions of an iceberg order <...> will only generate a single trade message"
                    for (const auto& v : maker2volume) {
                        matchesAcc.Match(MakeTrade<Side>(order.UniqID, v.first, icePrice, v.second));
                    }
                };

                const auto next = std::next(matchOrder);
                auto end = (next == matchBook.end() || next->Price != matchOrder->Price ? next : matchBook.upper_bound(*matchOrder));
                for (; matchOrder != end;) {
                    auto& matchQuant = matchOrder->Quantity;
                    const auto tradeVol = std::min(matchQuant, order.Quantity);
                    maker2volume[matchOrder->UniqID] += tradeVol;

                    if (tradeVol == matchQuant) {
                        const auto orderId = matchOrder->UniqID;
                        if (matchQuant = std::min(matchOrder->Peak, matchOrder->Hidden)) {
                            matchOrder->Hidden -= matchQuant;
                            if (std::next(matchOrder) != end) {
                                matchId2Order[orderId] = matchBook.insert(end, std::move(*matchOrder)); // "The execution will refresh the peak and generate a new time priority"
                                matchOrder = matchBook.erase(matchOrder);
                            }
                        } else {
                            matchId2Order.erase(orderId);
                            matchOrder = matchBook.erase(matchOrder);
                        }

                        if (tradeVol == order.Quantity) {
                            flushMatches();
                            return;
                        } else {
                            order.Quantity -= tradeVol;
                        }

                    } else {
                        assert(tradeVol == order.Quantity);
                        matchQuant -= tradeVol;
                        flushMatches();
                        return;
                    }
                }
                flushMatches();
            }
        }

        const auto orderId = order.UniqID;
        if (order.Peak) {
            const auto q = std::min(order.Peak, order.Quantity);
            order.Hidden = order.Quantity - q;
            order.Quantity = q;
        }
        putId2Order[orderId] = putBook.insert(std::move(order));
    }
}

class TBook final: public IBook {
    template<class TCmp>
    struct TOrderBook {
        using TPriceComparator = TCmp;

    private:
        using TCont = std::multiset<TExtendedOrder, TPriceComparator>;

    public:
        TCont Book;
        std::unordered_map<uint32_t, typename TCont::iterator> Id2Order;
    };

public:
    TBook() = default;

    void AcceptOrder(TOrder order, IMatchAcceptor& acc) override {
        acc.StartReport();

        switch (order.Side) {
        case ESide::BUY:
            MatchOrder<ESide::BUY>(order, acc, BuyOrders, SellOrders);
            break;
        case ESide::SELL:
            MatchOrder<ESide::SELL>(order, acc, SellOrders, BuyOrders);
            break;
        }

        PrintBook(acc);

        acc.FinishReport();
    }

    bool CancelOrder(const uint32_t orderId, IMatchAcceptor& acc) override {
        const bool ok = TryDelete(orderId, BuyOrders) || TryDelete(orderId, SellOrders);

        acc.StartReport();
        PrintBook(acc);
        acc.FinishReport();

        return ok;
    }

    void ListBook(IMatchAcceptor& acc) const override {
        PrintBook(acc);
    }

private:
    void PrintBook(IMatchAcceptor& matchesAcc) const {
        PrintBookRange(BuyOrders.Book.rbegin(), BuyOrders.Book.rend(), matchesAcc);
        PrintBookRange(SellOrders.Book.begin(), SellOrders.Book.end(), matchesAcc);
    }

private:
    TOrderBook<std::greater<>> BuyOrders;
    TOrderBook<std::less<>> SellOrders;
};

std::unique_ptr<IBook> MakeBook() {
    return std::make_unique<TBook>();
}
