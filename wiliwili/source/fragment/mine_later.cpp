//
// Created by 贾海峰 on 2023/7/7.
//

#include <utility>
#include <borealis/core/thread.hpp>
#include <borealis/views/button.hpp>
#include <fmt/format.h>

#include "bilibili.h"
#include "fragment/mine_later.hpp"
#include "view/video_card.hpp"
#include "utils/number_helper.hpp"
#include "utils/activity_helper.hpp"
#include "utils/image_helper.hpp"
#include "utils/shortcut_helper.hpp"
#include "utils/config_helper.hpp"
#include "utils/dialog_helper.hpp"

using namespace brls::literals;

class DataSourceMineWatchLaterList : public RecyclingGridDataSource {
public:
    explicit DataSourceMineWatchLaterList(bilibili::WatchLaterList result) : list(std::move(result)) {
    }

    RecyclingGridItem* cellForRow(RecyclingGrid* recycler, size_t index) override {
        RecyclingGridItemVideoCard* item = (RecyclingGridItemVideoCard*)recycler->dequeueReusableCell("Cell");
        bilibili::WatchLaterItem& r      = this->list[index];
        std::string extra                = r.isWatched() ? "已看完" : "";
        item->setCard(r.pic + ImageHelper::h_ext, r.title, r.owner.name, 0, r.stat.view, r.stat.danmaku,
                      r.duration, extra);
        return item;
    }

    size_t getItemCount() override { return list.size(); }

    void onItemSelected(RecyclingGrid* recycler, size_t index) override {
        auto& data = list[index];

        Intent::openBV(data.bvid);
    }

    void clearData() override { this->list.clear(); }

    const bilibili::WatchLaterList& getList() const { return list; }

    void removeAt(size_t index) {
        if (index < list.size()) {
            list.erase(list.begin() + static_cast<std::ptrdiff_t>(index));
        }
    }

    size_t getWatchedCount() const {
        size_t count = 0;
        for (const auto& item : list) {
            if (item.isWatched()) ++count;
        }
        return count;
    }

private:
    bilibili::WatchLaterList list;
};

MineLater::MineLater() {
    this->inflateFromXMLRes("xml/fragment/mine_later.xml");
    brls::Logger::debug("frag mine later created");
    recyclingGrid->registerCell("Cell", [this]() {
        auto* item = RecyclingGridItemVideoCard::create();
        item->registerAction("移出稍后再看", brls::ControllerButton::BUTTON_Y,
                             [this, item](brls::View* view) -> bool {
                                 this->removeItemAt(item->getIndex());
                                 return true;
                             });
        item->registerAction(brls::BRLS_KBD_KEY_DELETE,
                             [this, item](brls::View* view) -> bool {
                                 this->removeItemAt(item->getIndex());
                                 return true;
                             });
        return item;
    });
    recyclingGrid->setRefreshAction([this]() {
        AutoTabFrame::focus2Sidebar(this);
        this->recyclingGrid->showSkeleton();
        this->requestData(true);
    });
    btnClearViewed->registerClickAction([this](brls::View* view) -> bool {
        this->clearViewedVideos();
        return true;
    });
    this->requestData();
}

MineLater::~MineLater() { brls::Logger::debug("frag mine later deleted"); }

brls::View* MineLater::create() { return new MineLater(); }

void MineLater::onCreate() {
    this->registerTabAction("wiliwili/mine/refresh_later"_i18n, brls::ControllerButton::BUTTON_X,
                            ShortcutHelper::getRefresh(),
                            [this](brls::View* view) -> bool {
                                this->recyclingGrid->refresh();
                                return true;
                            });
    this->registerTabAction("清除已看视频", brls::ControllerButton::BUTTON_BACK,
                            [this](brls::View* view) -> bool {
                                this->clearViewedVideos();
                                return true;
                            });
}

void MineLater::removeItemAt(size_t index) {
    auto* ds = dynamic_cast<DataSourceMineWatchLaterList*>(recyclingGrid->getDataSource());
    if (!ds || index >= ds->getItemCount()) return;

    auto csrf = ProgramConfig::instance().getCSRF();
    if (csrf.empty()) {
        DialogHelper::showDialog("wiliwili/home/common/no_login"_i18n);
        return;
    }

    const auto item = ds->getList()[index];
    DialogHelper::showCancelableDialog(
        fmt::format("确定从稍后再看中移出「{}」吗？", item.title),
        [this, aid = item.aid, index]() {
            this->requestDelWatchLater(aid, index);
        });
}

void MineLater::clearViewedVideos() {
    auto* ds = dynamic_cast<DataSourceMineWatchLaterList*>(recyclingGrid->getDataSource());
    if (!ds) return;

    auto csrf = ProgramConfig::instance().getCSRF();
    if (csrf.empty()) {
        DialogHelper::showDialog("wiliwili/home/common/no_login"_i18n);
        return;
    }

    size_t watchedCount = ds->getWatchedCount();
    std::string msg     = watchedCount > 0
                              ? fmt::format("检测到 {} 个已看完的视频，确定从稍后再看中一键清除吗？", watchedCount)
                              : "当前列表中未检测到已看完的视频，是否仍尝试清除所有已看记录？";

    DialogHelper::showCancelableDialog(msg, [this]() {
        this->requestClearViewedWatchLater();
    });
}

void MineLater::onDelWatchLaterSuccess(size_t index) {
    auto* currentDs = dynamic_cast<DataSourceMineWatchLaterList*>(recyclingGrid->getDataSource());
    if (currentDs && index < currentDs->getItemCount()) {
        currentDs->removeAt(index);
        recyclingGrid->reloadData();
    }
    brls::Application::notify("已移出稍后再看");
}

void MineLater::onClearViewedSuccess() {
    brls::Application::notify("已清除稍后再看中的已看视频");
    this->recyclingGrid->refresh();
}

void MineLater::onActionError(const std::string& error) {
    DialogHelper::showDialog(error);
}

void MineLater::onWatchLaterList(const bilibili::WatchLaterListWrapper& result) {
    brls::Threading::sync(
        [this, result]() { recyclingGrid->setDataSource(new DataSourceMineWatchLaterList(result.list)); });
}

void MineLater::onError(const std::string& error) {
    brls::Logger::error("MineLater::onError: {}", error);
    brls::sync([this, error]() { this->recyclingGrid->setError(error); });
}