#pragma once

#include "parse.hpp"
#include "wsconnect.hpp"
#include <thread>
#include <chrono>

namespace asio = boost::asio;
namespace ssl  = boost::asio::ssl;
namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace json = boost::json;

using tcp = asio::ip::tcp;

constexpr i64 PRICE_SCALE = 10000;    // supports 0.0001 price precision (Polymarket Max)
constexpr i64 SIZE_SCALE  = 1000000;  // supports 6 decimal places for size (Polymarket Max)

// struct BookPrint{
//   std::string best_bid;
//   std::string best_ask;
//   std::string spread;
//   std::string last_trade;
//   std::string timestamp;
//   std::vector<std::string> bid;
//   std::vector<std::string> ask;
// };

class App {
public:
  void run();
private:
  asio::awaitable<void> runSession();
  std::unordered_map<std::string, AssetBook> books;
  std::queue<std::string> toParse;
};