#ifndef PARAM_LANE_TYPES_HPP
#define PARAM_LANE_TYPES_HPP

#include <vector>
#include <variant>
#include <string>

// Returns the maximum value for a param lane: 16383 for pitch bend, 127 for CC lanes.
inline int laneMaxValue(const std::string& type)
{
    return (type == "Pitch") ? 16383 : 127;
}

struct ParamPtLocal {
    int   id;
    float beat;
    int   value;
    bool  anchor;
};

struct ParamLaneLocal {
    int                       id;
    std::string               type;
    std::vector<ParamPtLocal> points;  // sorted by beat
};

struct ParamIdle {};
struct ParamPendingCreate { int laneIdx; float beat; int value; };
struct ParamDragState     { int laneIdx, ptIdx; float origBeat; int origValue; bool moved = false; };
struct ParamVirtualDrag   { int laneIdx, predPtIdx, origValue; bool moved = false; };
using ParamState = std::variant<ParamIdle, ParamPendingCreate, ParamDragState, ParamVirtualDrag>;

#endif
