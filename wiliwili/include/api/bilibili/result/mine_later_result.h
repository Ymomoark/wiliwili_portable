//
// Created by 贾海峰 on 2023/7/6.
//

#pragma once

#include "bilibili/util/json.hpp"

namespace bilibili {

class Up {
public:
    std::string name;
    uint64_t mid;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Up, name, mid);

class Stat {
public:
    int view, danmaku;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Stat, view, danmaku);

class WatchLaterItem {
public:
    std::string pic;
    Up owner;
    int duration = 0;  // video length in seconds
    int progress = 0;  // watched seconds (-1 means finished)
    std::string title;
    uint64_t aid = 0;
    Stat stat;
    std::string bvid;
    uint64_t cid = 0;

    bool isWatched() const {
        return progress == -1 || (duration > 0 && progress >= duration - 5);
    }
};
inline void from_json(const nlohmann::json& nlohmann_json_j, WatchLaterItem& nlohmann_json_t) {
    if (nlohmann_json_j.contains("progress") && nlohmann_json_j.at("progress").is_number_integer()) {
        nlohmann_json_j.at("progress").get_to(nlohmann_json_t.progress);
    }
    NLOHMANN_JSON_EXPAND(
        NLOHMANN_JSON_PASTE(NLOHMANN_JSON_FROM, aid, pic, title, duration, owner, stat, bvid, cid));
}

typedef std::vector<WatchLaterItem> WatchLaterList;

class WatchLaterListWrapper {
public:
    int count;
    WatchLaterList list;
};
inline void from_json(const nlohmann::json& nlohmann_json_j, WatchLaterListWrapper& nlohmann_json_t) {
    NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(NLOHMANN_JSON_FROM, count, list));
}
}  // namespace bilibili