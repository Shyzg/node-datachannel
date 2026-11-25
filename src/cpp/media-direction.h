#ifndef MEDIA_DIRECTION_H
#define MEDIA_DIRECTION_H

#include <optional>
#include <string>

#include <rtc/rtc.hpp>

std::optional<rtc::Description::Direction> strToDirection(const std::string &dirAsStr);
std::string directionToStr(rtc::Description::Direction dir);

#endif // MEDIA_DIRECTION_H
