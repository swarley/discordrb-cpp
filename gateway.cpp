#include "discordrb/gateway.hpp"

#include <pthread.h>
#include <zlib.h>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/device/back_inserter.hpp>
#include <boost/iostreams/filter/zlib.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <chrono>
#include <csignal>
#include <rice/Hash.hpp>
#include <thread>
#include <tuple>
#include <vector>
#include "discordrb.hpp"
#include "discordrb/json_converters.hpp"
#include "zstr/zstr.hpp"

#include <ruby/thread.h>

namespace Discordrb {
using json = nlohmann::json;

void* handle_dispatch_with_gvl(void* data) {
  Discordrb::DispatchData* d_data = static_cast<Discordrb::DispatchData*>(data);
  json j_data = json::parse(d_data->payload);
  Rice::Hash payload_data =
      Discordrb::JSONConverter::hash_from_json(j_data.at("d"));
  d_data->bot.call("handle_dispatch",
                   Rice::Symbol(static_cast<std::string>(j_data.at("t"))),
                   payload_data);
  return NULL;
}

void* raise_heartbeat_event_with_gvl(void* bot) {
  (static_cast<Rice::Object*>(bot))->call("raise_heartbeat_event");
  return NULL;
}

class Gateway {
  std::string token;
  std::string recieve_buffer;
  websocketpp::connection_hdl handler;
  std::string uri;
  std::stringstream ws_buff;
  std::unique_ptr<zstr::istream> zlib_ctx;
  Discordrb::Websocket client;
  Discordrb::Session session;
  Rice::Object bot;
  Rice::Object logger;
  bool should_reconnect;
  bool connected;

 private:
  // Inflate our payload. zstr autodetects if this is neccessary
  // so it's easier to just send everything through here for now.
  std::string inflate_string(std::string payload_raw) {
    std::string payload;
    ws_buff.clear();
    ws_buff.str("");
    ws_buff << payload_raw;

    // TODO: this should throw an error for an incomplete
    //       payload.
    if (std::strcmp((payload_raw.data() + payload_raw.size() - 4),
                    "\x00\x00\xff\xff")) {
      return "";
    }

    // TODO: ZLIB can produce errors during getline that need to be handled.
    std::stringstream s_stream;
    std::string buff;
    zlib_ctx->clear();
    while (getline(*zlib_ctx, buff))
      s_stream << buff;
    payload = s_stream.str();

    return payload;
  }

  // Send our IDENTIFY payload to the gateway
  void identify() {
    json ident_data = {{"op", 2},
                       {"d",
                        {{"token", token},
                         {"properties",
                          {
                              {"$os", "linux"},
                              {"$browser", "discordrb-native"},
                              {"$device", "discordrb-native"},
                          }},
                         {"compress", true},
                         {"large_threshold", 100}}}};
    client.send(handler, ident_data.dump(),
                websocketpp::frame::opcode::value::text);
  }

  // TODO: reorganize some of this out to handle_message
  //       so that we can call `handle_dispatch_with_gvl` directly?
  void handle_dispatch(json data, std::string payload) {
    std::string d_type = data["t"];
    if (data["t"] == "READY") {
      DRB_DEBUG("Recieved READY dispatch");
      session.id = data["d"]["session_id"];
      session.sequence = 0;
    } else if (data["t"] == "RESUMED") {
      rb_thread_call_with_gvl(
          [](void* logger) -> void* {
            static_cast<Rice::Object*>(logger)->call("info", "Resumed");
            return NULL;
          },
          static_cast<void*>(&logger));
      DRB_DEBUG("Resumed");
    }

    Discordrb::DispatchData d_data = {bot, payload};
    rb_thread_call_with_gvl(&Discordrb::handle_dispatch_with_gvl,
                            static_cast<void*>(&d_data));
  }

  void handle_heartbeat() {
    json hb_data = {{"op", 1}, {"d", session.sequence}};
    send(hb_data.dump());
    // Neither of these solutions are currently acceptable.
    // // Segfault, we do not have the gvl and cannot interact
    // bot.call("raise_heartbeat_event");
    // // We are not within the main thread, so we may not use the gvl
    // rb_thread_call_with_gvl(
    //     [](void* data) -> void* {

    //       static_cast<Rice::Object*>(data)->call("raise_heartbeat_event");
    //       return NULL;
    //     },
    //     static_cast<void*>(&bot));
  }

  // TODO: Maybe more work can be done here to be a little more smart
  //       about this?
  void handle_reconnect() {
    client.close(handler, websocketpp::close::status::service_restart,
                 "Reconnect packet recieved");

    if (should_reconnect) {
      connect(uri);
    }
  }

  // FIXME: We need to do some serious work here to use ruby's threads
  //        since we don't want to block but we need to be within a
  //        ruby thread in order to propagate a heartbeat event
  //        to the bot. Because this is currently operating outside
  //        of the GVL we aren't able to safely interact with ruby objects.
  //
  // WARN: currently no heartbeat events are raised.
  void handle_hello(json data) {
    try {
      std::chrono::milliseconds interval(data["heartbeat_interval"]);

      while (1) {
        std::this_thread::sleep_for(interval);
        if (connected) {
          handle_heartbeat();
        } else {
          break;
        }
      }
    } catch (websocketpp::exception) {
      DRB_DEBUG("Heartbeat Loop Died");
    }
  }

  // // TODO: Route closing logic through here to we can DRY event raises.
  // void handle_close(websocketpp::close::status s_code, const char* reason) {
  //   // Incomplete, raise a disconnectevent on bot.
  //   // rb_thread_call_with_gvl([](void* _bot) -> void* {
  //   //   static_cast<Rice::Object*>(_bot)->call
  //   // })
  // }

  // FIXME: use ruby threads instead in order to acquire the GVL later.
  void start_heartbeat_loop(json data) {
    std::thread heartbeat_loop(&Discordrb::Gateway::handle_hello, this, data);
    heartbeat_loop.detach();
  }

  // Handle an open event. Store our handler and set connected to true.
  // Send our identify payload immediately
  void ws_open_handler(websocketpp::connection_hdl hdl) {
    connected = true;
    handler = hdl;
    rb_thread_call_with_gvl(
        [](void* logger) -> void* {
          static_cast<Rice::Object*>(logger)->call("info",
                                                   "Websocket Connected");
          return NULL;
        },
        static_cast<void*>(&logger));

    zlib_ctx = std::make_unique<zstr::istream>(ws_buff);
    identify();
    return;
  }

  // Handle a message event.
  // OUTSIDE OF GVL
  void ws_message_handler(websocketpp::connection_hdl hdl,
                          Discordrb::Websocket::message_ptr msg) {
    std::string payload_raw = msg->get_payload();
    std::string payload = inflate_string(payload_raw);

    json data = json::parse(payload);
    int op = data["op"];

    if (!data["s"].is_null())
      session.sequence = data["s"];

    switch (op) {
      case Discordrb::Opcodes::Dispatch:
        handle_dispatch(data, payload);
        break;
      case Discordrb::Opcodes::Heartbeat:
        handle_heartbeat();
        break;
      case Discordrb::Opcodes::Reconnect:
        handle_reconnect();
        break;
      case Discordrb::Opcodes::InvalidSession:
        // TODO
        break;
      case Discordrb::Opcodes::Hello:
        start_heartbeat_loop(data["d"]);
        break;
      default:
        // TODO Error here maybe?
        break;
    }
    DRB_DEBUG(payload);
    return;
  }

  // Handler for closing a connection. If should_reset is set
  // we will immediately attempt to reconnect.
  // OUTSIDE OF GVL
  // TODO: add an increasing reconnect attempt timer?
  void ws_close_handler(websocketpp::connection_hdl) {
    connected = false;
    std::cout << "Websocket closed" << std::endl;

    if (should_reconnect) {
      connect(uri);
    }
  }

  // Initialize asio and bind handlers to websocket events
  void ws_init() {
    client.init_asio();
    client.set_message_handler(
        std::bind(&Discordrb::Gateway::ws_message_handler, this,
                  std::placeholders::_1, std::placeholders::_2));
    client.set_open_handler(std::bind(&Discordrb::Gateway::ws_open_handler,
                                      this, std::placeholders::_1));
    client.set_close_handler(std::bind(&Discordrb::Gateway::ws_close_handler,
                                       this, std::placeholders::_1));
#ifndef DISCORDRB_DEBUG
    client.clear_access_channels(websocketpp::log::alevel::all);
#endif
    return;
  }

  /*
   *
   *  PUBLIC FUNCTIONS
   *
   */

 public:
  // Constructor
  // rb_bot should be anything that implements Discordrb::Bot
  Gateway(Rice::Object rb_bot, std::string tok) {
    connected = false;
    should_reconnect = true;
    logger = Rice::define_module("Discordrb").const_get("LOGGER");
    bot = rb_bot;
    token = tok;
    ws_init();
    client.set_tls_init_handler([](websocketpp::connection_hdl) {
      return websocketpp::lib::make_shared<boost::asio::ssl::context>(
          boost::asio::ssl::context::tlsv1);
    });
  }

  // Connect to a given URI.
  // TODO: Use `API` to get our gateway on construct, this will also
  //       allow us to filter out bad tokens early.
  void connect(std::string _uri) {
    uri = _uri;
    websocketpp::lib::error_code ec;
    Discordrb::Websocket::connection_ptr con = client.get_connection(uri, ec);
    if (ec) {
      std::cout << "Error connecting websocket: " << ec << std::endl;
      return;
    } else {
      client.connect(con);
      rb_thread_call_without_gvl(
          [](void* data) -> void* {
            (static_cast<Discordrb::Websocket*>(data))->run();
            return NULL;
          },
          static_cast<void*>(&client), RUBY_UBF_IO, NULL);
    }
    return;
  }

  // Check if the websocket is currently open.
  // TODO: make this return an enum value so we can tell if we're connecting
  bool is_open() { return connected; }

  // Send a message over our connection. Currently only supports text frames,
  // is a binary frame even something we would ever need?
  void send(std::string payload) {
    client.send(handler, payload, websocketpp::frame::opcode::value::text);
  }

  // Close the gateway
  void stop() {
    should_reconnect = false;
    client.close(handler, websocketpp::close::status::normal, "Closed by user");
  }
};

// This is to get around some really stupid behavior with Rice::Constructor
// templating that has some special option when you want to use Rice::Object as
// the first method. It probably could be fixed without explicitly specifying a
// contructor class, but I haven't figured it out yet
class GatewayConstructor
    : Rice::Constructor<Gateway, Rice::Object, std::string> {
 public:
  static void construct(Rice::Object self,
                        Rice::Object _bot,
                        std::string _token) {
    DATA_PTR(self.value()) = new Discordrb::Gateway(_bot, _token);
  }
};

Rice::Hash test(std::string data) {
  json j = json::parse(data);
  return Discordrb::JSONConverter::hash_from_json(j);
}

Rice::Array arr_test(std::string data) {
  json j = json::parse(data);
  return Discordrb::JSONConverter::array_from_json(j);
}

};  // namespace Discordrb

extern "C" void Init_discordrb() {
  Rice::Module rb_mDiscordrb = Rice::define_module("Discordrb");
  Rice::Data_Type<Discordrb::Gateway> rb_cGateway =
      rb_mDiscordrb.define_class<Discordrb::Gateway>("Gateway2")
          .define_constructor(Discordrb::GatewayConstructor(),
                              (Rice::Arg("bot"), Rice::Arg("token")))
          .define_method("connect", &Discordrb::Gateway::connect,
                         (Rice::Arg("gateway_uri") = "wss://gateway.discord.gg/"
                                                     "?encoding=json&v=6"))
          .define_method("send", &Discordrb::Gateway::send,
                         (Rice::Arg("payload")))
          .define_method("open?", &Discordrb::Gateway::is_open)
          .define_method("stop", &Discordrb::Gateway::stop);
  rb_mDiscordrb.define_module_function("test", &Discordrb::test,
                                       (Rice::Arg("data")));
  rb_mDiscordrb.define_module_function("test_arr", &Discordrb::arr_test,
                                       (Rice::Arg("data")));
}
