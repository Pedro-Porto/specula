#pragma once
#include <string_view>

namespace specula {


inline constexpr std::string_view CMD_AUTH = "AUTH";
inline constexpr std::string_view CMD_PING = "PING";
inline constexpr std::string_view CMD_STATUS =
    "STATUS";
inline constexpr std::string_view CMD_BYE = "BYE";


inline constexpr std::string_view RESP_OK = "OK";
inline constexpr std::string_view RESP_ERR = "ERR";


inline constexpr std::string_view NL = "\n";

}
