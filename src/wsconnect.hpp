#pragma once

#include <boost/asio/ssl.hpp>
#include <boost/asio/buffers_iterator.hpp>
#include <boost/json.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/ssl.hpp>
#include <openssl/ssl.h>
#include <iostream>

namespace asio = boost::asio;
namespace ssl  = boost::asio::ssl;
namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace json = boost::json;

using tcp = asio::ip::tcp;
using i64 = long long;

struct WsConfig {
  std::string host   = "ws-subscriptions-clob.polymarket.com";
  std::string port   = "443";
  std::string target = "/ws/market";
};

websocket::stream<ssl::stream<tcp::socket>> 
connect_ws(asio::io_context& ioc, ssl::context& ssl_ctx, const WsConfig& cfg);

void send_subscribe(websocket::stream<ssl::stream<tcp::socket>>& ws, 
const std::vector<std::string>& ids);