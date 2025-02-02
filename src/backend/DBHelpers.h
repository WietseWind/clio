#ifndef RIPPLE_APP_REPORTING_DBHELPERS_H_INCLUDED
#define RIPPLE_APP_REPORTING_DBHELPERS_H_INCLUDED

#include <ripple/basics/Log.h>
#include <ripple/protocol/TxMeta.h>
#include <boost/container/flat_set.hpp>
#include <backend/Pg.h>

/// Struct used to keep track of what to write to transactions and
/// account_transactions tables in Postgres
struct AccountTransactionsData
{
    boost::container::flat_set<ripple::AccountID> accounts;
    uint32_t ledgerSequence;
    uint32_t transactionIndex;
    ripple::uint256 txHash;

    AccountTransactionsData(
        ripple::TxMeta& meta,
        ripple::uint256 const& txHash,
        beast::Journal& j)
        : accounts(meta.getAffectedAccounts(j))
        , ledgerSequence(meta.getLgrSeq())
        , transactionIndex(meta.getIndex())
        , txHash(txHash)
    {
    }

    AccountTransactionsData() = default;
};

template <class T>
inline bool
isOffer(T const& object)
{
    short offer_bytes = (object[1] << 8) | object[2];
    return offer_bytes == 0x006f;
}
template <class T>
inline bool
isOfferHex(T const& object)
{
    auto blob = ripple::strUnHex(4, object.begin(), object.begin() + 4);
    if (blob)
    {
        short offer_bytes = ((*blob)[1] << 8) | (*blob)[2];
        return offer_bytes == 0x006f;
    }
    return false;
}

template <class T>
inline ripple::uint256
getBook(T const& offer)
{
    ripple::SerialIter it{offer.data(), offer.size()};
    ripple::SLE sle{it, {}};
    ripple::uint256 book = sle.getFieldH256(ripple::sfBookDirectory);
    return book;
}

inline ripple::LedgerInfo
deserializeHeader(ripple::Slice data)
{
    ripple::SerialIter sit(data.data(), data.size());

    ripple::LedgerInfo info;

    info.seq = sit.get32();
    info.drops = sit.get64();
    info.parentHash = sit.get256();
    info.txHash = sit.get256();
    info.accountHash = sit.get256();
    info.parentCloseTime =
        ripple::NetClock::time_point{ripple::NetClock::duration{sit.get32()}};
    info.closeTime =
        ripple::NetClock::time_point{ripple::NetClock::duration{sit.get32()}};
    info.closeTimeResolution = ripple::NetClock::duration{sit.get8()};
    info.closeFlags = sit.get8();

    info.hash = sit.get256();

    return info;
}
#endif
