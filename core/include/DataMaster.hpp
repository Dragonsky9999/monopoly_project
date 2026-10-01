#ifndef DATA_MASTER_HPP
#define DATA_MASTER_HPP

#include "Card.hpp"
#include <unordered_map>

class DataMaster {
public:
    static std::unordered_map<CardId, Card> createCardMaster();
};

#endif
