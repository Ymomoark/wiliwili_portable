//
// Created by 贾海峰 on 2023/7/6.
//

#pragma once

#include "view/auto_tab_frame.hpp"
#include "view/recycling_grid.hpp"
#include "presenter/mine_later.hpp"

class MineLater : public AttachedView, public MineLaterRequest {
public:
    MineLater();

    ~MineLater() override;

    static View *create();

    void onCreate() override;

    void onWatchLaterList(const bilibili::WatchLaterListWrapper &result) override;

    void onDelWatchLaterSuccess(size_t index) override;

    void onClearViewedSuccess() override;

    void onActionError(const std::string &error) override;

    void onError(const std::string &error) override;

private:
    void removeItemAt(size_t index);

    void clearViewedVideos();

    BRLS_BIND(RecyclingGrid, recyclingGrid, "mine/later/recyclingGrid");
    BRLS_BIND(brls::Button, btnClearViewed, "mine/later/clear_viewed");
};