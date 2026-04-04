#include "parse.hpp"


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

void OrderBook::addBid(i64 price, i64 size) {
  if (size == 0) bids.erase(price);
  else bids[price] = size;
}

void OrderBook::addAsk(i64 price, i64 size) {
  if (size == 0) asks.erase(price);
  else asks[price] = size;
}

void OrderBook::clear() {
  bids.clear();
  asks.clear();
}

i64 OrderBook::best_bid_price() const { return bids.empty() ? -1 : bids.begin()->first; }
i64 OrderBook::best_ask_price() const { return asks.empty() ? -1 : asks.begin()->first; }
i64 OrderBook::best_bid_size()  const { return bids.empty() ? 0  : bids.begin()->second; }
i64 OrderBook::best_ask_size()  const { return asks.empty() ? 0  : asks.begin()->second; }

void OrderBook::debug_bid(std::string assetID) {
  std::cout<<"BID INFO FOR: " << assetID << '\n';
  for (auto const& [price, size] : bids) std::cout << "price: " << price << ", size: " << size << '\n';
}
void OrderBook::debug_ask(std::string assetID) {
  std::cout<<"ASK INFO FOR: " << assetID << '\n';
  for (auto const& [price, size] : asks) std::cout << "price: " << price << ", size: " << size << '\n';
}


void AssetBook::debug() {
  book.debug_bid(asset_id);
  book.debug_ask(asset_id);
}

void parse_book_event(std::unordered_map<std::string, AssetBook>* books, json::object const& obj){

  AssetBook book;
  book.market   = json::value_to<std::string>(obj.at("market"));
  book.asset_id = json::value_to<std::string>(obj.at("asset_id"));


  if (auto ts_it = obj.find("timestamp"); ts_it != obj.end() && ts_it->value().is_string()) {
    const auto& ts_j = ts_it->value().as_string();
    std::string_view ts_sv(ts_j.data(), ts_j.size());
    book.timestamp_ms = parse_timestamp_ms(ts_sv);
  }

  if (auto it = obj.find("last_trade_price"); it != obj.end() && it->value().is_string()) {
    const auto& ltp_j = it->value().as_string();
    std::string_view ltp_sv(ltp_j.data(), ltp_j.size());
    book.tick_size_fp = parse_price_fp(ltp_sv);
  }

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
  // lock probably missing? below
  book.debug(); //DEBUG!!!!!
  (*books)[book.asset_id] = std::move(book);
}

void parse_price_change_event(std::unordered_map<std::string, AssetBook>* books, json::object const& obj){
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
    ab.debug();
  }
}

void parse_tick_size_change_event(std::unordered_map<std::string, AssetBook>* books, json::object const& obj){
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

void parser(std::unordered_map<std::string, AssetBook>* books,
std::queue<std::string>* toParse) {
  for (;;) {
    try {
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

      // initial book event will be an array as long as it's subscribed for the yes and no asset/multiple assets
      if (v.is_array()) {
        json::array const& arr = v.as_array();

        for (json::value const& item : arr) {
          if (!item.is_object()) continue;
          json::object const& obj = item.as_object();
          const auto& ev_j = obj.at("event_type").as_string();
          std::string_view ev_sv(ev_j.data(), ev_j.size());
          
          if (ev_sv != "book") {
            // this is kinda dumb rn, technically the only array our code will see will be with
            // book event type, but maybe in the future I might accept other events so I should
            // modularize this better...
            std::cout<< '\n'<< '\n'<< '\n'<<"UNKNOWN EVENT TYPE: " << ev_sv << '\n' << '\n' << '\n';
            continue;
          }
          parse_book_event(books, obj);
        }
      }

      // DELTAS: price_change, tick_size_change, etc. always comes as object associated exclusively to yes or no event
      else if (v.is_object()) {
        json::object const& obj = v.as_object();

        if (auto it = obj.find("event_type"); it != obj.end() && it->value().is_string()) {
          const auto& ev_j = it->value().as_string();
          std::string_view ev_sv(ev_j.data(), ev_j.size());

          if (ev_sv == "price_change") parse_price_change_event(books, obj);
          else if (ev_sv == "tick_size_change") parse_tick_size_change_event(books, obj);
          else if(ev_sv == "book") parse_book_event(books, obj);
        }
      }
    } catch(const std::exception& e){
      std::cerr<<"Unexpected Error While parsing: " << e.what() <<'\n';
    }
  }
}