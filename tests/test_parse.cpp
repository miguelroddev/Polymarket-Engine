#include <assert.h>
#include <boost/json.hpp>
#include <queue>
namespace json = boost::json;

int main () {
  std::string test = R"([{"market":"0xedd6f583beb92839a74d2c5c66bb26cfe6b87d82056580c57a78b2f2e0eb8fe5","asset_id":"36058180106267309416569886113196185744009788380740653016655476166674199848738","timestamp":"1775740020016","hash":"6ce350360f6b487abb46766b131d987ea11e4de6","bids":[{"price":"0.01","size":"80.01"},{"price":"0.03","size":"300"},{"price":"0.04","size":"16.3"},{"price":"0.06","size":"170"},{"price":"0.07","size":"100"}],"asks":[{"price":"0.99","size":"64.54"},{"price":"0.98","size":"31.38"},{"price":"0.95","size":"30"},{"price":"0.94","size":"100"},{"price":"0.93","size":"9.7"},{"price":"0.8","size":"5"}],"tick_size":"0.01","event_type":"book","last_trade_price":""},{"market":"0xedd6f583beb92839a74d2c5c66bb26cfe6b87d82056580c57a78b2f2e0eb8fe5","asset_id":"11354133371126376784835090753308106986806090783788568298339951440339753418311","timestamp":"1775740020016","hash":"9ae41e0d3a6b46ae05cb75769fd450d5766598df","bids":[{"price":"0.01","size":"64.54"},{"price":"0.02","size":"31.38"},{"price":"0.05","size":"30"},{"price":"0.06","size":"100"},{"price":"0.07","size":"9.7"},{"price":"0.2","size":"5"}],"asks":[{"price":"0.99","size":"80.01"},{"price":"0.97","size":"300"},{"price":"0.96","size":"16.3"},{"price":"0.94","size":"170"},{"price":"0.93","size":"100"}],"tick_size":"0.01","event_type":"book","last_trade_price":""}])";

  std::queue<std::string> toParse;
  toParse.push(test);
  
  assert(2+2 == 4);
  return 0;
}