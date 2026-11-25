#include "media-direction.h"

rtc::Description::Direction strToDirection(const std::string &dirAsStr)
{
  if (dirAsStr == "SendOnly")
    return rtc::Description::Direction::SendOnly;
  if (dirAsStr == "SendRecv")
    return rtc::Description::Direction::SendRecv;
  if (dirAsStr == "RecvOnly")
    return rtc::Description::Direction::RecvOnly;
  if (dirAsStr == "Inactive")
    return rtc::Description::Direction::Inactive;
  return rtc::Description::Direction::Unknown;
}

std::string directionToStr(rtc::Description::Direction dir)
{
  switch (dir)
  {
  case rtc::Description::Direction::Unknown:
    return "Unknown";
  case rtc::Description::Direction::SendOnly:
    return "SendOnly";
  case rtc::Description::Direction::RecvOnly:
    return "RecvOnly";
  case rtc::Description::Direction::SendRecv:
    return "SendRecv";
  case rtc::Description::Direction::Inactive:
    return "Inactive";
  default:
    return "UNKNOWN_DIR_TYPE";
  }
}
