//
// Created by 贾海峰 on 2023/7/7.
//

#include <utility>
#include <unordered_set>
#include <borealis/core/thread.hpp>

#include "fragment/mine_later.hpp"
#include "view/video_card.hpp"
#include "utils/number_helper.hpp"
#include "utils/activity_helper.hpp"
#include "utils/image_helper.hpp"
#include "utils/shortcut_helper.hpp"
#include "utils/config_helper.hpp"
#include "utils/dialog_helper.hpp"
#include "bilibili.h"
#include <fmt/format.h>

using namespace brls::literals;

class DataSourceMineWatchLaterList : public RecyclingGridDataSource {
public:
    DataSourceMineWatchLaterList(bilibili::WatchLaterList result,
                                 const std::unordered_set<std::string>* watchedBvids,
                                 const std::unordered_set<uint64_t>* watchedAids)
        : list(std::move(result)), watchedBvids(watchedBvids), watchedAids(watchedAids) {
    }

    RecyclingGridItem* cellForRow(RecyclingGrid* recycler, size_t index) override {
        RecyclingGridItemVideoCard* item = (RecyclingGridItemVideoCard*)recycler->dequeueReusableCell("Cell");
        bilibili::WatchLaterItem& r      = this->list[index];
        bool watched = (watchedBvids && !r.bvid.empty() && watchedBvids->count(r.bvid) > 0) ||
                       (watchedAids && r.aid != 0 && watchedAids->count(r.aid) > 0);
        std::string extra = (watched || r.isWatched()) ? "已看完" : "";
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
        if (index < list.size()) list.erase(list.begin() + static_cast<std::ptrdiff_t>(index));
    }

private:
    bilibili::WatchLaterList list;
    const std::unordered_set<std::string>* watchedBvids = nullptr;
    const std::unordered_set<uint64_t>* watchedAids     = nullptr;
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
        this->requestWatchedHistory();
    });
    this->requestData();
    this->requestWatchedHistory();
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
}

void MineLater::onWatchLaterList(const bilibili::WatchLaterListWrapper& result) {
    brls::Threading::sync([this, result]() {
        recyclingGrid->setDataSource(new DataSourceMineWatchLaterList(result.list, &watchedBvids, &watchedAids));
    });
}

void MineLater::removeItemAt(size_t index) {
    auto* ds = dynamic_cast<DataSourceMineWatchLaterList*>(recyclingGrid->getDataSource());
    if (!ds || index >= ds->getItemCount()) return;
    // 从数据源取标题用于确认提示
    auto item = ds->getList()[index];
    DialogHelper::showCancelableDialog(fmt::format("确定从稍后再看中移出「{}」吗？", item.title),
                                       [this, aid = item.aid, index]() {
                                           this->requestDelWatchLater(aid, index);
                                       });
}

void MineLater::requestWatchedHistory() {
    auto mid = ProgramConfig::instance().getUserID();
    if (mid.empty() || mid == "0") return;

    ASYNC_RETAIN
    BILI::get_my_history(
        {},
        [ASYNC_TOKEN](const bilibili::HistoryVideoResultWrapper& result) {
            brls::sync([ASYNC_TOKEN, result]() {
                ASYNC_RELEASE
                watchedBvids.clear();
                watchedAids.clear();
                for (const auto& item : result.list) {
                    if (item.isFinished()) {
                        if (!item.history.bvid.empty()) watchedBvids.insert(item.history.bvid);
                        if (item.history.oid != 0) watchedAids.insert(item.history.oid);
                    }
                }
                if (recyclingGrid->getDataSource()) recyclingGrid->reloadData();
            });
        },
        [ASYNC_TOKEN](BILI_ERR) {
            brls::sync([ASYNC_TOKEN]() { ASYNC_RELEASE });
        });
}

void MineLater::onDelWatchLaterSuccess(size_t index) {
    auto* ds = dynamic_cast<DataSourceMineWatchLaterList*>(recyclingGrid->getDataSource());
    if (ds && index < ds->getItemCount()) {
        ds->removeAt(index);
        recyclingGrid->reloadData();
    }
    brls::Application::notify("已移出稍后再看");
}

void MineLater::onActionError(const std::string& error) { DialogHelper::showDialog(error); }

void MineLater::onError(const std::string& error) {
    brls::Logger::error("MineLater::onError: {}", error);
    brls::sync([this, error]() { this->recyclingGrid->setError(error); });
}