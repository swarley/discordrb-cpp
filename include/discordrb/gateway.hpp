#pragma once
#include <string>
#include <websocketpp/client.hpp>
#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/connection.hpp>
#include <websocketpp/extensions/permessage_deflate/enabled.hpp>
// #include "discordrb.hpp"
#include "nlohmann/json.hpp"

// I don't know why, but these have to come last otherwise nlohmann/json has
// some issues with finite
#include <rice/Class.hpp>
#include <rice/Constructor.hpp>

namespace Discordrb {

enum Opcodes {
  Dispatch,
  Heartbeat,
  Identify,
  Presence,
  VoiceState,
  VoicePing,
  Resume,
  Reconnect,
  RequestMembers,
  InvalidSession,
  Hello,
  HeartbeatAck
};

struct Session {
  std::string id;
  int sequence;
  bool suspended, invalid;

  Session(std::string session_id) {
    id = session_id;
    sequence = 0;
    suspended = false;
    invalid = false;
  }

  Session() {
    sequence = 0;
    suspended = false;
    invalid = false;
  }

  bool should_resume() { return suspended && !invalid; }
};

struct DispatchData {
  Rice::Object bot;
  std::string payload;
};

struct deflate_config : public websocketpp::config::asio_tls_client {
  struct permessage_deflate_config {};

  typedef websocketpp::extensions::permessage_deflate::enabled<
      permessage_deflate_config>
      permessage_deflate_type;
};
typedef websocketpp::client<deflate_config> ws;
typedef websocketpp::client<deflate_config> Websocket;

void ws_message_handler(ws* client,
                        websocketpp::connection_hdl c_hdl,
                        ws::message_ptr msg);
void ws_open_handler(ws* client, websocketpp::connection_hdl hdl);
void ws_init(ws client);
}  // namespace Discordrb
