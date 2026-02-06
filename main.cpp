#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/ssl.hpp>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include <thread>
#include <chrono>

namespace asio = boost::asio;
namespace ssl  = boost::asio::ssl;
namespace beast = boost::beast;
namespace websocket = beast::websocket;

using tcp = asio::ip::tcp;

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
    std::string(R"({"assets_ids":[")") + asset_id + R"("],"type":"market")" +
    (custom_feature_enabled ? R"(,"custom_feature_enabled":true})" : "}");
  ws.write(asio::buffer(msg));
}

int main(){
  WsConfig cfg;

  // Backoff settings
  int attempt = 0;

  for (;;) {
    try {
      asio::io_context ioc;
      ssl::context ssl_ctx{ssl::context::tlsv12_client};
      ssl_ctx.set_default_verify_paths(); // OS CA store

      auto ws = connect_ws(ioc, ssl_ctx, cfg);

      std::string asset_id = "assetID";

      send_subscribe(ws, asset_id, /*custom_feature_enabled=*/false);

      beast::flat_buffer buffer;
      for (;;) {
        buffer.consume(buffer.size());
        ws.read(buffer);
        std::string text = beast::buffers_to_string(buffer.data());
        std::cout << text << "\n";
      }
    }
    catch (const std::exception& e) {
      std::cerr << "WS error: " << e.what() << "\n";
    }

    // reconnect backoff
    attempt = std::min(attempt + 1, 8);
    auto sleep_ms = std::chrono::milliseconds(250 * (1 << attempt));
    std::this_thread::sleep_for(sleep_ms);
  }
  return 0;
}