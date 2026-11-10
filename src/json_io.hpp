#pragma once

#include "solver.hpp"

#include <nlohmann/json.hpp>

namespace gateway {

using Json = nlohmann::json;

struct Request {
  Point from;
  Point to;
  std::vector<Segment> segments;
};

Point parsePoint(const Json &aJson) {
  return Point(aJson.at("x").get<double>(), aJson.at("y").get<double>());
}

Json toJson(const Point &aPoint) {
  return {{"x", aPoint.x}, {"y", aPoint.y}};
}

Request parseRequest(const Json &aJson) {
  Request request{parsePoint(aJson.at("from")), parsePoint(aJson.at("to")), {}};
  const auto &segments = aJson.at("segments");
  request.segments.reserve(segments.size());
  for (const auto &s : segments)
    request.segments.push_back(
        {parsePoint(s.at("left")), parsePoint(s.at("right"))});
  return request;
}

Json toJson(const std::vector<Point> &aPath) {
  Json points = Json::array();
  for (const Point &p : aPath)
    points.push_back(toJson(p));
  return {{"path", points}};
}

} // namespace gateway
