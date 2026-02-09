#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/json.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/ssl.hpp>

#include <openssl/ssl.h>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include <thread>
#include <chrono>
#include <queue>
#include <map>

namespace asio = boost::asio;
namespace ssl  = boost::asio::ssl;
namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace json = boost::json;


using tcp = asio::ip::tcp;
using i64 = long long;

std::mutex queueMutex;
std::condition_variable queueCV;

struct BookPrint{
  std::string best_bid;
  std::string best_ask;
  std::string spread;
  std::string last_trade;
  std::string timestamp;
  std::vector<std::string> bid;
  std::vector<std::string> ask;
};


struct OrderBook {
  std::map<i64, i64, std::greater<>> bids;
  std::map<i64, i64> asks;

  void clear() { bids.clear(); asks.clear(); }

  i64 best_bid_price() const { return bids.empty() ? -1 : bids.begin()->first; }
  i64 best_ask_price() const { return asks.empty() ? -1 : asks.begin()->first; }
  i64 best_bid_size()  const { return bids.empty() ? 0  : bids.begin()->second; }
  i64 best_ask_size()  const { return asks.empty() ? 0  : asks.begin()->second; }
};

struct AssetBook {
  std::string asset_id;
  std::string market;
  i64 timestamp_ms = 0;
  i64 last_trade_fp = -1;
  OrderBook book;
};

void parser(std::unordered_map<std::string, AssetBook> *books,
std::queue<std::string> *toParse ){
  for (;;){
    
  std::string txt;
  {
    std::unique_lock<std::mutex> lock(queueMutex);

    queueCV.wait(lock, [&] {
      return !toParse->empty();
    });

    if (toParse->empty())
      return;

    txt = std::move(toParse->front());
    toParse->pop();
  }
  json::error_code ec;
  json::value v = json::parse(txt, ec);
  if (ec) {
    continue;
  }




  }
}


struct WsConfig {
  std::string host   = "ws-subscriptions-clob.polymarket.com";
  std::string port   = "443";
  std::string target = "/ws/market";
};

websocket::stream<ssl::stream<tcp::socket>> 
connect_ws(asio::io_context& ioc, ssl::context& ssl_ctx, const WsConfig& cfg){
  tcp::resolver resolver{ioc};
  websocket::stream<ssl::stream<tcp::socket>> ws{ioc, ssl_ctx};
  auto results = resolver.resolve(cfg.host, cfg.port);
  //TCP CONNECT
  asio::connect(
    ws.next_layer().next_layer(),
    results.begin(),
    results.end()
  );
  // TLS verification
  ws.next_layer().set_verify_mode(ssl::verify_peer);
  ws.next_layer().set_verify_callback(ssl::host_name_verification(cfg.host));

  // TLS handshake
  SSL_set_tlsext_host_name(ws.next_layer().native_handle(), cfg.host.c_str());
  ws.next_layer().handshake(ssl::stream_base::client);
  // WebSocket timeouts + automatic keepalive pings
  ws.set_option(websocket::stream_base::timeout::suggested(beast::role_type::client));

  // set User-Agent
  ws.set_option(websocket::stream_base::decorator(
    [](websocket::request_type& req) {
      req.set(beast::http::field::user_agent,std::string("beast/") + BOOST_BEAST_VERSION_STRING);
    }));

  // WS handshake
  ws.handshake(cfg.host + ":" + cfg.port, cfg.target);

  return ws;
}

void send_subscribe(websocket::stream<ssl::stream<tcp::socket>>& ws, 
const std::string& asset_id, bool custom_feature_enabled) {
  std::string msg =
    std::string(R"({"assets_ids": [")") + asset_id + R"("],"type": "market")" +
    (custom_feature_enabled ? R"(,"custom_feature_enabled": true})" : "}");
  std::cout << "SUB: " << msg << "\n"; //DEBUG
  ws.write(asio::buffer(msg));
}

int main(){
  WsConfig cfg;

  // Backoff settings
  int attempt = 0;

  for (;;) {
    std::unordered_map<std::string, AssetBook> books;
    std::queue<std::string> toParse;
    std::thread t(parser,&books, &toParse);

    try {
      asio::io_context ioc;
      ssl::context ssl_ctx{ssl::context::tlsv12_client};
      ssl_ctx.set_default_verify_paths(); // OS CA store

      auto ws = connect_ws(ioc, ssl_ctx, cfg);
      std::cout<<"sucess\n";
      // curl -s "https://gamma-api.polymarket.com/markets/517310" | jq -r '.clobTokenIds'
      std::string asset_id_yes = "101676997363687199724245607342877036148401850938023978421879460310389391082353";
      std::string asset_id_no = "4153292802911610701832309484716814274802943278345248636922528170020319407796";
      std::string asset_ids = asset_id_yes + R"(")" + ',' + R"(")" + asset_id_no;
      send_subscribe(ws, asset_ids, /*custom_feature_enabled=*/false);

      beast::flat_buffer buffer;
      for (;;) {
        buffer.consume(buffer.size());
        ws.read(buffer);
        std::string text = beast::buffers_to_string(buffer.data());
        {
          std::lock_guard<std::mutex> lock(queueMutex);
          toParse.push(std::move(text));
        }
        queueCV.notify_one();
        // maybe add a thread create with + a lock for an order book associated with each market
        // send the reference of the queue to the thread, maybe create this permanent thread before
        // the for loop, think about it tomorrow
        std::cout << text << "\n";
      }
    }
    catch (const std::exception& e) {
      std::cerr << "WS error: " << e.what() << "\n";
      queueCV.notify_all();
      if (t.joinable()) t.join();
    }

    // reconnect backoff
    attempt = std::min(attempt + 1, 8);
    auto sleep_ms = std::chrono::milliseconds(250 * (1 << attempt));
    std::this_thread::sleep_for(sleep_ms);
  }
  return 0;
}