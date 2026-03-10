#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/buffers_iterator.hpp>
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

constexpr i64 PRICE_SCALE = 10000;    // supports 0.0001 price precision (Polymarket Max)
constexpr i64 SIZE_SCALE  = 1000000;  // supports 6 decimal places for size (Polymarket Max)

i64 parse_fixed(std::string_view s, int decimals) {
  i64 whole = 0;
  i64 frac = 0;
  int frac_digits = 0;
  bool seen_dot = false;

  for (char c : s) {
    if (c == '.') {
      if (seen_dot) break;
      seen_dot = true;
      continue;
    }

    if (c < '0' || c > '9') break;

    if (!seen_dot) {
      whole = whole * 10 + (c - '0');
    } else if (frac_digits < decimals) {
      frac = frac * 10 + (c - '0');
      ++frac_digits;
    }
  }

  while (frac_digits < decimals) {
    frac *= 10;
    ++frac_digits;
  }

  i64 scale = 1;
  for (int i = 0; i < decimals; ++i) scale *= 10;

  return whole * scale + frac;
}

i64 parse_price_fp(std::string_view s) {
  return parse_fixed(s, 4); // PRICE_SCALE = 10000
}

i64 parse_size_fp(std::string_view s) {
  return parse_fixed(s, 6); // SIZE_SCALE = 1000000
}

i64 parse_timestamp_ms(std::string_view s) {
  i64 v = 0;
  for (char c : s) {
    if (c < '0' || c > '9') break;
    v = v * 10 + (c - '0');
  }
  return v;
}

struct OrderBook {
  std::map<i64, i64, std::greater<>> bids;
  std::map<i64, i64> asks;

  void addBid(i64 price, i64 size) {
    if (size == 0) bids.erase(price);
    else bids[price] = size;
  }

  void addAsk(i64 price, i64 size) {
    if (size == 0) asks.erase(price);
    else asks[price] = size;
  }

  void clear() {
    bids.clear();
    asks.clear();
  }

  i64 best_bid_price() const { return bids.empty() ? -1 : bids.begin()->first; }
  i64 best_ask_price() const { return asks.empty() ? -1 : asks.begin()->first; }
  i64 best_bid_size()  const { return bids.empty() ? 0  : bids.begin()->second; }
  i64 best_ask_size()  const { return asks.empty() ? 0  : asks.begin()->second; }
};

struct AssetBook {
  std::string asset_id;
  std::string market;
  i64 tick_size_fp = 0;
  i64 timestamp_ms = 0;
  i64 last_trade_fp = -1;
  OrderBook book;
};

void parser(std::unordered_map<std::string, AssetBook>* books,
std::queue<std::string>* toParse) {
  for (;;) {
    std::string txt;
    {
      std::unique_lock<std::mutex> lock(queueMutex);
      queueCV.wait(lock, [&] {
        return !toParse->empty();
      });

      txt = std::move(toParse->front());
      toParse->pop();
    }

    json::error_code ec;
    json::value v = json::parse(txt, ec);
    if (ec) continue;

    // book event is always an array as long as it's subscribed for the yes and no asset
    if (v.is_array()) {
      json::array const& arr = v.as_array();

      for (json::value const& item : arr) {
        if (!item.is_object()) continue;
        json::object const& obj = item.as_object();

        const auto& ev_j = obj.at("event_type").as_string();
        std::string_view ev_sv(ev_j.data(), ev_j.size());

        if (ev_sv != "book") continue;

        AssetBook book;
        book.market   = json::value_to<std::string>(obj.at("market"));
        book.asset_id = json::value_to<std::string>(obj.at("asset_id"));

        const auto& ts_j = obj.at("timestamp").as_string();
        std::string_view ts_sv(ts_j.data(), ts_j.size());
        book.timestamp_ms = parse_timestamp_ms(ts_sv);

        const auto& ltp_j = obj.at("last_trade_price").as_string();
        std::string_view ltp_sv(ltp_j.data(), ltp_j.size());
        book.last_trade_fp = parse_price_fp(ltp_sv);

        if (auto it = obj.find("tick_size"); it != obj.end() && it->value().is_string()) {
          const auto& tick_j = it->value().as_string();
          std::string_view tick_sv(tick_j.data(), tick_j.size());
          book.tick_size_fp = parse_price_fp(tick_sv);
        }

        OrderBook orderBook;

        json::array const& bids = obj.at("bids").as_array();
        for (json::value const& lv : bids) {
          json::object const& level = lv.as_object();

          const auto& price_j = level.at("price").as_string();
          const auto& size_j  = level.at("size").as_string();

          std::string_view price_sv(price_j.data(), price_j.size());
          std::string_view size_sv(size_j.data(), size_j.size());

          orderBook.addBid(parse_price_fp(price_sv), parse_size_fp(size_sv));
        }

        json::array const& asks = obj.at("asks").as_array();
        for (json::value const& lv : asks) {
          json::object const& level = lv.as_object();

          const auto& price_j = level.at("price").as_string();
          const auto& size_j  = level.at("size").as_string();

          std::string_view price_sv(price_j.data(), price_j.size());
          std::string_view size_sv(size_j.data(), size_j.size());

          orderBook.addAsk(parse_price_fp(price_sv), parse_size_fp(size_sv));
        }

        book.book = std::move(orderBook);
        (*books)[book.asset_id] = std::move(book);
      }
    }

    // DELTAS: price_change, tick_size_change, etc. always comes as object associated exclusively to yes or no event
    else if (v.is_object()) {
      json::object const& obj = v.as_object();

      if (auto it = obj.find("event_type"); it != obj.end() && it->value().is_string()) {
        const auto& ev_j = it->value().as_string();
        std::string_view ev_sv(ev_j.data(), ev_j.size());

        if (ev_sv == "price_change") {
          i64 timestamp_ms = 0;
          if (auto ts_it = obj.find("timestamp"); ts_it != obj.end() && ts_it->value().is_string()) {
            const auto& ts_j = ts_it->value().as_string();
            std::string_view ts_sv(ts_j.data(), ts_j.size());
            timestamp_ms = parse_timestamp_ms(ts_sv);
          }

          json::array const& changes = obj.at("price_changes").as_array();
          for (json::value const& ch : changes) {
            if (!ch.is_object()) continue;
            json::object const& level = ch.as_object();

            std::string asset_id = json::value_to<std::string>(level.at("asset_id"));
            auto book_it = books->find(asset_id);
            if (book_it == books->end()) continue;

            AssetBook& ab = book_it->second;
            ab.timestamp_ms = timestamp_ms;

            const auto& price_j = level.at("price").as_string();
            const auto& size_j  = level.at("size").as_string();
            const auto& side_j  = level.at("side").as_string();

            std::string_view price_sv(price_j.data(), price_j.size());
            std::string_view size_sv(size_j.data(), size_j.size());
            std::string_view side_sv(side_j.data(), side_j.size());

            i64 price_fp = parse_price_fp(price_sv);
            i64 size_fp  = parse_size_fp(size_sv);

            if (side_sv == "BUY") {
              ab.book.addBid(price_fp, size_fp);
            } else if (side_sv == "SELL") {
              ab.book.addAsk(price_fp, size_fp);
            }
          }
        }

        else if (ev_sv == "tick_size_change") {
          std::string asset_id = json::value_to<std::string>(obj.at("asset_id"));
          auto book_it = books->find(asset_id);
          if (book_it != books->end()) {
            if (auto nt_it = obj.find("new_tick_size"); nt_it != obj.end() && nt_it->value().is_string()) {
              const auto& nts_j = nt_it->value().as_string();
              std::string_view nts_sv(nts_j.data(), nts_j.size());
              book_it->second.tick_size_fp = parse_price_fp(nts_sv);
            }
          }
        }
      }
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
  ws.text(true);
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
const std::vector<std::string>& ids) {
  json::object o;
  json::array a;
  for (auto& id : ids) a.emplace_back(id);

  o["assets_ids"] = std::move(a);
  o["type"] = "market";
  o["operation"] = "subscribe";

  auto s = json::serialize(o);
  std::cout << "SUB: " << s << "\n";
  ws.write(asio::buffer(s));
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
      // would be cool to implement a way where the user submits the link and then can choose
      // which assets of a specific event he wants to subscribe to
      // curl -s "https://gamma-api.polymarket.com/events/slug/will-jesus-christ-return-before-2027" | jq -r '.clobTokenIds'
      std::vector<std::string> asset_ids;
      std::string asset_id_yes = "69324317355037271422943965141382095011871956039434394956830818206664869608517";
      std::string asset_id_no = "51797157743046504218541616681751597845468055908324407922581755135522797852101";
      asset_ids.push_back(asset_id_yes);
      asset_ids.push_back(asset_id_no);
      send_subscribe(ws, asset_ids);

      beast::flat_buffer buffer;
      for (;;) {
        buffer.consume(buffer.size());

        beast::error_code ec;
        ws.read(buffer, ec);
        if (ec) {
          std::cerr << "ws.read error: " << ec.message() << "\n";
          break;
        }
        std::cout << "MSG bytes=" << buffer.size()
          << " got_text=" << ws.got_text() << "\n";

        if (!ws.got_text()) continue;

        std::string text = beast::buffers_to_string(buffer.data());
        if (text.find_first_not_of(" \r\n\t") == std::string::npos) continue;
        std::cout << "TEXT: " << text << "\n"; // output must be here, if after std::move(text) then undefined behaviour
        /*
        // output must be above. if after std::move(text) then unspecified state since text(string) might become empty
        */
        {
          std::lock_guard<std::mutex> lock(queueMutex);
          toParse.push(std::move(text));
        }
        queueCV.notify_one();
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