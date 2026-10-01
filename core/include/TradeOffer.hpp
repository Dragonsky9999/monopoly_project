#ifndef TRADE_OFFER_HPP
#define TRADE_OFFER_HPP

#include "Card.hpp"
#include <vector>

struct TradeOffer {
    int proposerId = -1;
    int targetId = -1;
    int offerMoney = 0;
    int requestMoney = 0;
    std::vector<CardId> offerCards;
    std::vector<CardId> requestCards;
};

#endif
