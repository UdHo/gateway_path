#include "json_io.hpp"
#include "solver.hpp"

#include <httplib.h>

#include <cstdlib>
#include <iostream>

using gateway::Json;
using gateway::parseRequest;
using gateway::Request;
using gateway::shortestPath;
using gateway::toJson;

namespace {

constexpr const char *kJson = "application/json";
constexpr int kDefaultPort = 8080;

void sendError(httplib::Response &res, int status, const std::string &message) {
  res.status = status;
  res.set_content(Json{{"error", message}}.dump(), kJson);
}

void handleShortestPath(const httplib::Request &req, httplib::Response &res) {
  Request input;
  try {
    input = parseRequest(Json::parse(req.body));
  } catch (const Json::exception &e) {
    sendError(res, 400, e.what());
    return;
  }

  const auto path = shortestPath(input.from, input.to, input.segments);
  res.set_content(toJson(path).dump(), kJson);
}

} // namespace

int main(int argc, char **argv) {
  const int port = argc > 1 ? std::atoi(argv[1]) : kDefaultPort;

  httplib::Server server;
  server.Post("/shortest-path", handleShortestPath);

  std::cout << "Listening on http://localhost:" << port << std::endl;
  server.listen("0.0.0.0", port);
  return 0;
}
