#ifndef AC_REWARD_RECEIPT_H
#define AC_REWARD_RECEIPT_H

#include <cstdint>
#include <map>
#include <string>
#include <vector>

struct RewardGrantReceipt
{
    std::vector<std::uint32_t> itemGuids;
    std::int64_t moneyDelta = 0;
    std::map<std::string, std::int64_t> resourceDeltas;
};

#endif // AC_REWARD_RECEIPT_H
