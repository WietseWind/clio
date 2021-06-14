//------------------------------------------------------------------------------
/*
    This file is part of rippled: https://github.com/ripple/rippled
    Copyright (c) 2020 Ripple Labs Inc.

    Permission to use, copy, modify, and/or distribute this software for any
    purpose  with  or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL ,  DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

#ifndef RIPPLE_REPORTING_SSL_WS_SESSION_H
#define RIPPLE_REPORTING_SSL_WS_SESSION_H

#include <boost/asio/dispatch.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket/ssl.hpp>

#include <reporting/server/Handlers.h>
#include <reporting/ReportingETL.h>

#include <reporting/server/WsBase.h>

namespace http = boost::beast::http;
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;       
using tcp = boost::asio::ip::tcp;

class ReportingETL;

class SslWsSession : public WsBase
                   , public std::enable_shared_from_this<SslWsSession>
{
    boost::beast::websocket::stream<
        boost::beast::ssl_stream<boost::beast::tcp_stream>> ws_;
    std::string response_;
    boost::beast::flat_buffer buffer_;
    http::request_parser<http::string_body> parser_;
    ReportingETL& etl_;

public:
    // Take ownership of the socket
    explicit SslWsSession(
        boost::beast::ssl_stream<boost::beast::tcp_stream>&& stream,
        ReportingETL& etl,
        boost::beast::flat_buffer b)
        : WsBase()
        , ws_(std::move(stream))
        , etl_(etl)
    {
    }

    void
    send(std::string&& msg)
    {
        ws_.text(ws_.got_text());
        ws_.async_write(
            boost::asio::buffer(msg),
            boost::beast::bind_front_handler(
                &SslWsSession::on_write, shared_from_this()));
    }

    void
    run(http::request<http::string_body> req)
    {
        std::cout << "Running ws" << std::endl;
        // Set suggested timeout settings for the websocket
        ws_.set_option(
            websocket::stream_base::timeout::suggested(
                boost::beast::role_type::server));
    
        std::cout << "Trying to decorate" << std::endl;
        // Set a decorator to change the Server of the handshake
        ws_.set_option(websocket::stream_base::decorator(
            [](websocket::response_type& res)
            {
                res.set(http::field::server,
                    std::string(BOOST_BEAST_VERSION_STRING) +
                        " websocket-server-async");
            }));
        
        std::cout << "trying to async accept" << std::endl;

        ws_.async_accept(
            req,
            boost::beast::bind_front_handler(
                &SslWsSession::on_accept, shared_from_this()));
    }

    void
    on_accept(boost::beast::error_code ec)
    {
        if (ec)
            return wsFail(ec, "accept");

        // Read a message
        do_read();
    }

    void
    do_read()
    {
        // Read a message into our buffer
        ws_.async_read(
            buffer_,
            boost::beast::bind_front_handler(
                &SslWsSession::on_read, shared_from_this()));
    }

    void
    on_read(boost::beast::error_code ec, std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        // This indicates that the session was closed
        if (ec == boost::beast::websocket::error::closed)
            return;

        if (ec)
            wsFail(ec, "read");
        std::string msg{
            static_cast<char const*>(buffer_.data().data()), buffer_.size()};
        // BOOST_LOG_TRIVIAL(debug) << __func__ << msg;
        boost::json::object response;
        try
        {
            boost::json::value raw = boost::json::parse(msg);
            boost::json::object request = raw.as_object();
            BOOST_LOG_TRIVIAL(debug) << " received request : " << request;
            try
            {
                response = buildResponse(
                    request, 
                    etl_,
                    shared_from_this());
            }
            catch (Backend::DatabaseTimeout const& t)
            {
                BOOST_LOG_TRIVIAL(error) << __func__ << " Database timeout";
                response["error"] =
                    "Database read timeout. Please retry the request";
            }
        }
        catch (std::exception const& e)
        {
            BOOST_LOG_TRIVIAL(error)
                << __func__ << "caught exception : " << e.what();
        }
        BOOST_LOG_TRIVIAL(trace) << __func__ << response;
        response_ = boost::json::serialize(response);

        // Echo the message
        ws_.text(ws_.got_text());
        ws_.async_write(
            boost::asio::buffer(response_),
            boost::beast::bind_front_handler(
                &SslWsSession::on_write, shared_from_this()));
    }

    void
    on_write(boost::beast::error_code ec, std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        if (ec)
            return wsFail(ec, "write");

        // Clear the buffer
        buffer_.consume(buffer_.size());

        // Do another read
        do_read();
    }
};

class SslWsUpgrader : public std::enable_shared_from_this<SslWsUpgrader>
{
    boost::beast::ssl_stream<boost::beast::tcp_stream> https_;
    boost::optional<http::request_parser<http::string_body>> parser_;
    boost::beast::flat_buffer buffer_;
    ssl::context& ctx_;
    ReportingETL& etl_;

public:
    SslWsUpgrader(
        boost::asio::ip::tcp::socket&& socket,
        ssl::context& ctx,
        ReportingETL& etl,
        boost::beast::flat_buffer&& b)
        : https_(std::move(socket), ctx)
        , ctx_(ctx)
        , etl_(etl)
        , buffer_(std::move(b))
        {}

    void
    run()
    {
        // Set the timeout.
        boost::beast::get_lowest_layer(https_).expires_after(std::chrono::seconds(30));

        // Perform the SSL handshake
        // Note, this is the buffered version of the handshake.
        https_.async_handshake(
            ssl::stream_base::server,
            buffer_.data(),
            boost::beast::bind_front_handler(
                &SslWsUpgrader::on_handshake,
                shared_from_this()));
    }

private:

    void
    on_handshake(
        boost::beast::error_code ec,
        std::size_t bytes_used)
    {
        if(ec)
            return wsFail(ec, "handshake");

        // Consume the portion of the buffer used by the handshake
        buffer_.consume(bytes_used);

        do_upgrade();
    }

    void
    do_upgrade()
    {
        std::cout << "doing upgrade" << std::endl;
        parser_.emplace();

        // Apply a reasonable limit to the allowed size
        // of the body in bytes to prevent abuse.
        parser_->body_limit(10000);

        // Set the timeout.
        boost::beast::get_lowest_layer(https_).expires_after(std::chrono::seconds(30));

        // // Read a request using the parser-oriented interface
        http::async_read(
            https_,
            buffer_,
            *parser_,
            boost::beast::bind_front_handler(
                &SslWsUpgrader::on_upgrade,
                shared_from_this()));
    }

    void
    on_upgrade(boost::beast::error_code ec, std::size_t bytes_transferred)
    {
        std::cout << "upgraded WS" << std::endl;
        boost::ignore_unused(bytes_transferred);

        // This means they closed the connection
        if(ec == http::error::end_of_stream)
            return;

        if (ec)
            return wsFail(ec, "upgrade");

        // See if it is a WebSocket Upgrade
        if(!websocket::is_upgrade(parser_->get()))
            return wsFail(ec, "is_upgrade");

        // Disable the timeout.
        // The websocket::stream uses its own timeout settings.
        boost::beast::get_lowest_layer(https_).expires_never();

        std::make_shared<SslWsSession>(
            std::move(https_),
            etl_,
            std::move(buffer_))->run(parser_->release());
    }
};

#endif // RIPPLE_REPORTING_SSL_WS_SESSION_H