#pragma once

#include <geos/algorithm/Orientation.h>
#include <geos/geom/Coordinate.h>

#include <cstddef>
#include <deque>
#include <optional>
#include <vector>

namespace gateway {

// Turn direction of a -> b -> c: +1 left, -1 right, 0 collinear (exact).
constexpr auto orientation = &geos::algorithm::Orientation::index;
using Point = geos::geom::CoordinateXY;

struct Segment {
  Point left;
  Point right;
};

// -------------------------------- My solution ---------------------------

// A corner of the shortest path: the apex for the next view and the index of
// the first gateway behind it.
struct Corner {
  Point point;
  std::size_t nextGateway;
};

// The narrowest strip of view from `apex` through the gateways seen so far,
// bounded by the left and right endpoint that are furthest in sight.
class VisibilityStrip {
public:
  VisibilityStrip(const Point &aApex, const std::vector<Segment> &aGateways,
                  std::size_t aFirstGateway)
      : iApex(aApex), iGateways(aGateways), iLeft(aFirstGateway),
        iRight(aFirstGateway) {}

  // Looks through gateway `aIndex`. Either it narrows the strip, or it lies
  // completely beyond one side: then the endpoint on the other side blocks
  // the view and is returned as the next corner.
  std::optional<Corner> lookThrough(std::size_t aIndex) {
    const Segment &gateway = iGateways[aIndex];

    if (isRightOfStrip(gateway.left))
      return Corner{rightBound(), iRight + 1};
    if (isInsideLeftBound(gateway.left))
      iLeft = aIndex;

    if (isLeftOfStrip(gateway.right))
      return Corner{leftBound(), iLeft + 1};
    if (isInsideRightBound(gateway.right))
      iRight = aIndex;

    return std::nullopt;
  }

private:
  const Point &leftBound() const { return iGateways[iLeft].left; }
  const Point &rightBound() const { return iGateways[iRight].right; }

  bool isRightOfStrip(const Point &aPoint) const {
    return orientation(iApex, rightBound(), aPoint) < 0;
  }
  bool isLeftOfStrip(const Point &aPoint) const {
    return orientation(iApex, leftBound(), aPoint) > 0;
  }
  // Right of the left bound, i.e. it would narrow the strip from the left.
  bool isInsideLeftBound(const Point &aPoint) const {
    return orientation(iApex, leftBound(), aPoint) < 0;
  }
  // Left of the right bound, i.e. it would narrow the strip from the right.
  bool isInsideRightBound(const Point &aPoint) const {
    return orientation(iApex, rightBound(), aPoint) > 0;
  }

  Point iApex;
  const std::vector<Segment> &iGateways;
  std::size_t iLeft;
  std::size_t iRight;
};

// Looks from `aApex` through the gateways starting at `aFirstGateway`. Returns
// the first corner that blocks the view, or nothing if the apex sees the last
// gateway.
std::optional<Corner> findNextCorner(const Point &aApex,
                                     const std::vector<Segment> &aGateways,
                                     std::size_t aFirstGateway) {
  VisibilityStrip strip(aApex, aGateways, aFirstGateway);
  for (std::size_t i = aFirstGateway; i < aGateways.size(); ++i)
    if (auto corner = strip.lookThrough(i))
      return corner;
  return std::nullopt;
}

// My solution: the same result as shortestPath. Easier to understand but
// O(n^2) in the worst case.
//
// From the apex we look through the gateways and keep the narrowest strip of
// view. When a gateway lies beyond one side of the strip, the endpoint on the
// other side is a corner of the shortest path and the next apex. We repeat
// this behind the corner until the apex sees the end.
//
// The end point is modelled as a last gateway end -> end.
std::vector<Point> mySolution(const Point &aStart, const Point &aEnd,
                              const std::vector<Segment> &aSegments) {
  std::vector<Segment> gateways = aSegments;
  gateways.push_back({aEnd, aEnd});

  std::vector<Point> path{aStart};
  std::size_t firstGateway = 0;
  while (auto corner = findNextCorner(path.back(), gateways, firstGateway)) {
    path.push_back(corner->point);
    firstGateway = corner->nextGateway;
  }

  path.push_back(aEnd);
  return path;
}

// --------------------------------- Book solution -----------------------------

// Convex chain from the apex (front) to the latest portal endpoint (back).
using Chain = std::deque<Point>;

// Pops from the back while the last two vertices and aPoint do not turn in the
// direction `aTurn` (+1 left, -1 right), then appends aPoint.
void tighten(Chain &aChain, const Point &aPoint, int aTurn) {
  while (aChain.size() >= 2 &&
         orientation(aChain[aChain.size() - 2], aChain.back(), aPoint) != aTurn)
    aChain.pop_back();
  aChain.push_back(aPoint);
}

// Moves the apex along `aOpposite` for as long as `aPoint` lies beyond its
// first edge. Every vertex passed is final and appended to `aPath`. Returns
// whether the apex moved. `aTurn` is the turn direction of the own chain
// (+1 left, -1 right); the opposite chain lies on the other side.
bool advanceApex(Chain &aOpposite, std::vector<Point> &aPath,
                 const Point &aPoint, int aTurn) {
  bool moved = false;
  while (aOpposite.size() >= 2 &&
         orientation(aOpposite[0], aOpposite[1], aPoint) == -aTurn) {
    aPath.push_back(aOpposite.front());
    aOpposite.pop_front();
    moved = true;
  }
  return moved;
}

// Adds a portal endpoint to its own chain, or restarts the own chain at the
// new apex if the endpoint lies beyond the opposite chain.
void addEndpoint(Chain &aOwn, Chain &aOpposite, std::vector<Point> &aPath,
                 const Point &aPoint, int aTurn) {
  if (advanceApex(aOpposite, aPath, aPoint, aTurn)) {
    aOwn = {aOpposite.front()};
    aOwn.push_back(aPoint);
  } else {
    tighten(aOwn, aPoint, aTurn);
  }
}

// Shortest path from `start` to `end` that passes all gateway segments in
// order (funnel algorithm, Lee & Preparata).
//
// The funnel consists of an apex and two convex chains running from the apex
// to the most recent left / right portal endpoint. The left chain turns left,
// the right chain turns right. For every new portal endpoint p:
//
//  * p lies beyond the opposite chain: walk that chain from the apex while p
//    is outside; every vertex we walk over is final (appended to the path) and
//    the last one becomes the new apex. The own chain restarts as [apex, p].
//  * otherwise: pop vertices from the back of the own chain until it is convex
//    again, then append p.
//
// Every vertex is pushed once and popped at most once, so the total work is
// O(n) for n gateway segments.
std::vector<Point> shortestPath(const Point &aStart, const Point &aEnd,
                                const std::vector<Segment> &aSegments) {
  std::vector<Point> path;
  Chain left{aStart}, right{aStart};

  auto addLeft = [&](const Point &aPoint) {
    addEndpoint(left, right, path, aPoint, 1);
  };
  auto addRight = [&](const Point &aPoint) {
    addEndpoint(right, left, path, aPoint, -1);
  };

  for (const Segment &s : aSegments) {
    addLeft(s.left);
    addRight(s.right);
  }
  addLeft(aEnd);
  addRight(aEnd);

  path.push_back(left.front());
  path.push_back(aEnd);

  return path;
}

} // namespace gateway
