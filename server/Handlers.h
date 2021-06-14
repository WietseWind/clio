#include <boost/asio/dispatch.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/json.hpp>

#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/trivial.hpp>

#include <etl/ReportingETL.h>
#include <server/WsBase.h>

#include <unordered_map>
#include <iostream>

#ifndef RIPPLE_REPORTING_HANDLERS_H
#define RIPPLE_REPORTING_HANDLERS_H

class ReportingETL;
class SubscriptionManager;
class WsSession;

//------------------------------------------------------------------------------

static std::unordered_set<std::string> forwardCommands{
    "submit",
    "submit_multisigned",
    "fee",
    "path_find",
    "ripple_path_find",
    "manifest"
};

enum RPCCommand {
    tx,
    account_tx,
    ledger,
    account_info,
    ledger_data,
    book_offers,
    ledger_range,
    ledger_entry,
    account_channels,
    account_lines,
    account_currencies,
    account_offers,
    account_objects,
    channel_authorize,
    channel_verify,
    subscribe,
    unsubscribe,
    server_info
};

static std::unordered_map<std::string, RPCCommand> commandMap{
    {"tx", tx},
    {"account_tx", account_tx},
    {"ledger", ledger},
    {"ledger_range", ledger_range},
    {"ledger_entry", ledger_entry},
    {"account_info", account_info},
    {"ledger_data", ledger_data},
    {"book_offers", book_offers},
    {"account_channels", account_channels},
    {"account_lines", account_lines},
    {"account_currencies", account_currencies},
    {"account_offers", account_offers},
    {"account_objects", account_objects},
    {"channel_authorize", channel_authorize},
    {"channel_verify", channel_verify},
    {"subscribe", subscribe},
    {"unsubscribe", unsubscribe},
    {"server_info", server_info}};

boost::json::object
doTx(
    boost::json::object const& request,
    BackendInterface const& backend);
boost::json::object
doAccountTx(
    boost::json::object const& request,
    BackendInterface const& backend);

boost::json::object
doBookOffers(
    boost::json::object const& request,
    BackendInterface const& backend);

boost::json::object
doLedgerData(
    boost::json::object const& request,
    BackendInterface const& backend);
boost::json::object
doLedgerEntry(
    boost::json::object const& request,
    BackendInterface const& backend);
boost::json::object
doLedger(
    boost::json::object const& request,
    BackendInterface const& backend);
boost::json::object
doLedgerRange(
    boost::json::object const& request,
    BackendInterface const& backend);

boost::json::object
doAccountInfo(
    boost::json::object const& request,
    BackendInterface const& backend);
boost::json::object
doAccountChannels(
    boost::json::object const& request,
    BackendInterface const& backend);
boost::json::object
doAccountLines(
    boost::json::object const& request,
    BackendInterface const& backend);
boost::json::object
doAccountCurrencies(
    boost::json::object const& request,
    BackendInterface const& backend);
boost::json::object
doAccountOffers(
    boost::json::object const& request,
    BackendInterface const& backend);
boost::json::object
doAccountObjects(
    boost::json::object const& request,
    BackendInterface const& backend);

boost::json::object
doChannelAuthorize(boost::json::object const& request);
boost::json::object
doChannelVerify(boost::json::object const& request);

boost::json::object
doServerInfo(
    boost::json::object const& request,
    BackendInterface const& backend);

boost::json::object
doSubscribe(
    boost::json::object const& request,
    std::shared_ptr<WsBase>& session,
    SubscriptionManager& manager);
boost::json::object
doUnsubscribe(
    boost::json::object const& request,
    std::shared_ptr<WsBase>& session,
    SubscriptionManager& manager);

std::pair<boost::json::object, uint32_t>
buildResponse(
    boost::json::object const& request,
    std::shared_ptr<BackendInterface> backend,
    std::shared_ptr<SubscriptionManager> manager,
    std::shared_ptr<ETLLoadBalancer> balancer,
    std::shared_ptr<WsBase> session);

#endif // RIPPLE_REPORTING_HANDLERS_H