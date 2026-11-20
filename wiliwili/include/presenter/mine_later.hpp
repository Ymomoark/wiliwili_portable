//
// Created by 贾海峰 on 2023/7/6.
//

#pragma once

#include <cstddef>
#include "bilibili/result/mine_later_result.h"
#include "presenter.h"

class MineLaterRequest : public Presenter {
public:
    virtual void onWatchLaterList(const bilibili::WatchLaterListWrapper& result);

    virtual void onDelWatchLaterSuccess(size_t index);

    virtual void onActionError(const std::string& error);

    virtual void onError(const std::string& error);

    void requestData(bool refresh = false);

    void requestWatchLaterList();

    /** 移出单个稍后再看视频，成功后回调 onDelWatchLaterSuccess(index) */
    void requestDelWatchLater(uint64_t aid, size_t index);
};