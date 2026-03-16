#include "app.hpp"

std::mutex queueMutex;
std::condition_variable queueCV;

void App::run(){
  // Backoff settings
  int attempt = 0;
  std::thread t(parser,&books, &toParse);
  for (;;) {
    try {
      runSession();
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
}

void App::runSession(){
  asio::io_context ioc;
  ssl::context ssl_ctx{ssl::context::tlsv12_client};
  ssl_ctx.set_default_verify_paths(); // OS CA store
  
  WsConfig cfg;
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