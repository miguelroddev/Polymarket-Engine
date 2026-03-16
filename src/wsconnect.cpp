#include "wsconnect.hpp"

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