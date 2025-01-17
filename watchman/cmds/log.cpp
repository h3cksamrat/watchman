/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "watchman/Client.h"
#include "watchman/Logging.h"
#include "watchman/watchman_cmd.h"

using namespace watchman;

// log-level "debug"
// log-level "error"
// log-level "off"
static json_ref cmd_loglevel(Client* client, const json_ref& args) {
  if (json_array_size(args) != 2) {
    client->sendErrorResponse("wrong number of arguments to 'log-level'");
    return nullptr;
  }

  watchman::LogLevel level;
  try {
    level = watchman::logLabelToLevel(json_to_w_string(args.at(1)));
  } catch (std::out_of_range&) {
    client->sendErrorResponse("invalid log level for 'log-level'");
    return nullptr;
  }

  auto clientRef = client->shared_from_this();
  auto notify = [clientRef]() { clientRef->ping->notify(); };
  auto& log = watchman::getLog();

  switch (level) {
    case watchman::OFF:
      client->debugSub.reset();
      client->errorSub.reset();
      break;
    case watchman::DBG:
      client->debugSub = log.subscribe(watchman::DBG, notify);
      client->errorSub = log.subscribe(watchman::ERR, notify);
      break;
    case watchman::ERR:
    default:
      client->debugSub.reset();
      client->errorSub = log.subscribe(watchman::ERR, notify);
  }

  auto resp = make_response();
  resp.set("log_level", json_ref(args.at(1)));
  return resp;
}
W_CMD_REG("log-level", cmd_loglevel, CMD_DAEMON, NULL);

// log "debug" "text to log"
static json_ref cmd_log(Client* client, const json_ref& args) {
  if (json_array_size(args) != 3) {
    client->sendErrorResponse("wrong number of arguments to 'log'");
    return nullptr;
  }

  watchman::LogLevel level;
  try {
    level = watchman::logLabelToLevel(json_to_w_string(args.at(1)));
  } catch (std::out_of_range&) {
    client->sendErrorResponse("invalid log level for 'log'");
    return nullptr;
  }

  auto text = json_to_w_string(args.at(2));

  watchman::log(level, text, "\n");

  auto resp = make_response();
  resp.set("logged", json_true());
  return resp;
}
W_CMD_REG("log", cmd_log, CMD_DAEMON | CMD_ALLOW_ANY_USER, NULL);

// change the server log level for the logs
static json_ref cmd_global_log_level(Client* client, const json_ref& args) {
  if (json_array_size(args) != 2) {
    client->sendErrorResponse(
        "wrong number of arguments to 'global-log-level'");
    return nullptr;
  }

  watchman::LogLevel level;
  try {
    level = watchman::logLabelToLevel(json_to_w_string(args.at(1)));
  } catch (std::out_of_range&) {
    client->sendErrorResponse("invalid log level for 'global-log-level'");
    return nullptr;
  }

  watchman::getLog().setStdErrLoggingLevel(level);

  auto resp = make_response();
  resp.set("log_level", json_ref(args.at(1)));
  return resp;
}
W_CMD_REG(
    "global-log-level",
    cmd_global_log_level,
    CMD_DAEMON | CMD_ALLOW_ANY_USER,
    nullptr);

/* vim:ts=2:sw=2:et:
 */
