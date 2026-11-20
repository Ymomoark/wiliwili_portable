//
// Created by 贾海峰 on 2023/7/6.
//

#pragma once

#include <cstddef>
#include <unordered_set>
#include "bilibili/result/mine_later_result.h"
#include "bilibili/result/mine_history_result.h"
#include "presenter.h"

class MineLaterRequest : public Presenter {
public:
    virtual void onWatchLaterList(const bilibili::WatchLaterListWrapper& result);

    virtual void onDelWatchLaterSuccess(size_t index);

    virtual void onActionError(const std::string& error);

    /** 观看历史拉取完成后回调（此时 watchedBvids/watchedAids 已更新） */
    virtual void onWatchedHistoryUpdated();

    virtual void onError(const std::string& error);

    void requestData(bool refresh = false);

    void requestWatchLaterList();

    /** 移出单个稍后再看视频，成功后回调 onDelWatchLaterSuccess(index) */
    void requestDelWatchLater(uint64_t aid, size_t index);

    /** 拉取观看历史，得到已看完视频集合，用于给卡片打角标 */
    void requestWatchedHistory();

protected:
    // 从观看历史得到的已看完集合，用于给卡片打"已看完"角标
    std::unordered_set<std::string> watchedBvids;
    std::unordered_set<uint64_t> watchedAids;
};