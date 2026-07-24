#include "../../interfaces/RewardReceipt.h"

#include <cassert>
#include <cstdint>
#include <map>

namespace
{
bool ConsumeReceiptOnce(std::map<std::uint64_t, bool>& consumed, std::uint64_t grantId,
    RewardGrantReceipt const& receipt)
{
    if (receipt.itemGuids.empty() && receipt.moneyDelta == 0 && receipt.resourceDeltas.empty())
        return false;

    return consumed.emplace(grantId, true).second;
}

bool CompleteReceiptGrant(bool success, RewardGrantReceipt& receipt)
{
    if (!success)
        receipt = {};

    return success;
}
}

int main()
{
    RewardGrantReceipt failed;
    failed.itemGuids = { 101 };
    failed.moneyDelta = 25;
    failed.resourceDeltas["泡点"] = 3;
    assert(!CompleteReceiptGrant(false, failed));

    assert(failed.itemGuids.empty());
    assert(failed.moneyDelta == 0);
    assert(failed.resourceDeltas.empty());

    RewardGrantReceipt issued;
    issued.itemGuids = { 9001, 9002 };
    issued.moneyDelta = 1234;
    issued.resourceDeltas["积分"] = -7;

    std::map<std::uint64_t, bool> consumed;
    assert(ConsumeReceiptOnce(consumed, 42, issued));
    assert(!ConsumeReceiptOnce(consumed, 42, issued));

    return 0;
}
