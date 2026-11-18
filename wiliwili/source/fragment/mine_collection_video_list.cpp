//
// Created by fang on 2022/7/31.
//

#include <algorithm>
#include <memory>
#include <utility>
#include <borealis/views/applet_frame.hpp>
#include <borealis/views/button.hpp>
#include <borealis/core/thread.hpp>
#include <fmt/format.h>

#include "fragment/mine_collection_video_list.hpp"
#include "view/video_card.hpp"
#include "view/text_box.hpp"
#include "utils/image_helper.hpp"
#include "utils/activity_helper.hpp"
#include "bilibili.h"
#include "utils/shortcut_helper.hpp"
#include "utils/config_helper.hpp"
#include "utils/dialog_helper.hpp"

using namespace brls::literals;

class DataSourceCollectionVideoList : public RecyclingGridDataSource {
public:
    DataSourceCollectionVideoList(bilibili::CollectionVideoListResult result,
                                  const std::unordered_set<std::string>* watchedBvids,
                                  const std::unordered_set<uint64_t>* watchedAids)
        : list(std::move(result)), watchedBvids(watchedBvids), watchedAids(watchedAids) {}

    RecyclingGridItem* cellForRow(RecyclingGrid* recycler, size_t index) override {
        //从缓存列表中取出 或者 新生成一个表单项
        RecyclingGridItemVideoCard* item = (RecyclingGridItemVideoCard*)recycler->dequeueReusableCell("Cell");

        bilibili::CollectionVideoResult& r = this->list[index];

        std::string author = r.upper.name;
        if (r.type == 24) author = r.intro;

        bool isWatched = (watchedBvids && !r.bvid.empty() && watchedBvids->count(r.bvid) > 0) ||
                         (watchedAids && r.id > 0 && watchedAids->count(static_cast<uint64_t>(r.id)) > 0);
        std::string extra = isWatched ? "已看完" : "";

        item->setCard(r.cover + ImageHelper::h_ext, r.title, author, r.pubtime, r.cnt_info.play, r.cnt_info.danmaku,
                      r.duration, extra);
        return item;
    }

    size_t getItemCount() override { return list.size(); }

    void onItemSelected(RecyclingGrid* recycler, size_t index) override {
        auto& data = list[index];
        switch (data.type) {
            case 2:  // 普通视频
                Intent::openBV(data.bvid);
                break;
            case 12:  // 音频
            case 21:  // 视频合集
                break;
            case 24:  // 番剧
                Intent::openSeasonByEpId(data.id);
                break;
        }
    }

    void appendData(const bilibili::CollectionVideoListResult& data) {
        this->list.insert(this->list.end(), data.begin(), data.end());
    }

    void clearData() override { this->list.clear(); }

    const bilibili::CollectionVideoListResult& getList() const { return list; }

    void removeAt(size_t index) {
        if (index < list.size()) {
            list.erase(list.begin() + static_cast<std::ptrdiff_t>(index));
        }
    }

    void removeById(int id) {
        list.erase(std::remove_if(list.begin(), list.end(),
                                  [id](const bilibili::CollectionVideoResult& v) { return v.id == id; }),
                   list.end());
    }

private:
    bilibili::CollectionVideoListResult list;
    const std::unordered_set<std::string>* watchedBvids = nullptr;
    const std::unordered_set<uint64_t>* watchedAids     = nullptr;
};

MineCollectionVideoList::MineCollectionVideoList() {
    brls::Logger::debug("Fragment MineCollectionVideoList: create");
    this->inflateFromXMLRes("xml/fragment/mine_collection_video_list.xml");
    registerFloatXMLAttribute("type", [this](float value) {
        if (this->collectionData.id != 0) brls::fatal("You must set type before collection id.");
        this->requestType = (int)value;
    });
    registerStringXMLAttribute("collection", [this](const std::string& value) {
        this->collectionData.id = std::stoll(value);
        this->requestCollectionList();
    });
    registerBoolXMLAttribute("footerHidden", [this](bool value) {
        this->appletFrame->setFooterVisibility(value ? brls::Visibility::GONE : brls::Visibility::VISIBLE);
    });

    // 初始化列表
    recyclingGrid->registerCell("Cell", [this]() {
        auto* item = RecyclingGridItemVideoCard::create();
        item->registerAction("取消收藏", brls::ControllerButton::BUTTON_Y,
                             [this, item](brls::View* view) -> bool {
                                 if (!this->isOwnerCollection) return false;
                                 this->removeItemAt(item->getIndex());
                                 return true;
                             });
        item->registerAction(brls::BRLS_KBD_KEY_DELETE,
                             [this, item](brls::View* view) -> bool {
                                 if (!this->isOwnerCollection) return false;
                                 this->removeItemAt(item->getIndex());
                                 return true;
                             });
        return item;
    });
    recyclingGrid->onNextPage([this]() { this->requestCollectionList(); });
    recyclingGrid->setRefreshAction([this]() {
        brls::Application::giveFocus(this->imageCover);
        this->recyclingGrid->showSkeleton();
        this->requestIndex = 1;
        this->hasMore      = true;
        this->requestCollectionList();
        this->requestWatchedHistory();
    });
    btnClearViewed->registerClickAction([this](brls::View* view) -> bool {
        this->clearViewedVideos();
        return true;
    });
    this->registerAction("wiliwili/home/common/refresh"_i18n, brls::ControllerButton::BUTTON_X,
                         [this](brls::View* view) -> bool {
                             this->recyclingGrid->refresh();
                             return true;
                         });
    this->registerAction(ShortcutHelper::getRefresh(),
                         [this](brls::View* view) -> bool {
                             this->recyclingGrid->refresh();
                             return true;
                         });
    this->requestWatchedHistory();
}

MineCollectionVideoList::MineCollectionVideoList(const bilibili::CollectionResult& data, int type)
    : MineCollectionVideoList() {
    // 从 data 中获取数据，设置UI
    collectionData = data;
    requestType    = type;
    this->labelTitle->setText(data.title);
    this->labelSubtitle->setText(fmt::format("{}{}", data.media_count, "wiliwili/mine/num"_i18n));
    ImageHelper::with(this->imageCover)->load(data.cover);

    this->requestCollectionList();
}

MineCollectionVideoList::~MineCollectionVideoList() {
    brls::Logger::debug("Fragment MineCollectionVideoListActivity: delete");
}

brls::View* MineCollectionVideoList::create() { return new MineCollectionVideoList(); }

void MineCollectionVideoList::updateSubtitle() {
    if (collectionData.upper.name.empty()) {
        labelSubtitle->setText(fmt::format("{}{}", collectionData.media_count, "wiliwili/mine/num"_i18n));
    } else {
        labelSubtitle->setText(fmt::format("{}{} · {}: {}", collectionData.media_count,
                                           "wiliwili/mine/num"_i18n, "wiliwili/mine/creator"_i18n,
                                           collectionData.upper.name));
    }
}

void MineCollectionVideoList::requestWatchedHistory(const bilibili::HistoryVideoListCursor& cursor, int pagesLeft) {
    if (pagesLeft <= 0) return;
    auto mid = ProgramConfig::instance().getUserID();
    if (mid.empty() || mid == "0") return;

    bilibili::HistoryVideoListCursor nextCursor = cursor;
    nextCursor.ps                               = 30;
    ASYNC_RETAIN
    BILI::get_my_history(
        nextCursor,
        [ASYNC_TOKEN, pagesLeft](const bilibili::HistoryVideoResultWrapper& result) {
            brls::sync([ASYNC_TOKEN, result, pagesLeft]() {
                ASYNC_RELEASE
                bool updated = false;
                for (const auto& item : result.list) {
                    bool finished = item.progress == -1 || (item.duration > 0 && item.progress >= item.duration - 5);
                    if (finished) {
                        if (!item.history.bvid.empty() && watchedBvids.insert(item.history.bvid).second) {
                            updated = true;
                        }
                        if (item.history.oid != 0 && watchedAids.insert(item.history.oid).second) {
                            updated = true;
                        }
                    }
                }
                if (updated && recyclingGrid->getDataSource()) {
                    recyclingGrid->reloadData();
                }
                if (pagesLeft > 1 && result.cursor.max != 0 && !result.list.empty()) {
                    requestWatchedHistory(result.cursor, pagesLeft - 1);
                }
            });
        },
        [ASYNC_TOKEN](BILI_ERR) {
            brls::sync([ASYNC_TOKEN]() { ASYNC_RELEASE });
        });
}

void MineCollectionVideoList::removeItemAt(size_t index) {
    if (!isOwnerCollection) return;
    auto* ds = dynamic_cast<DataSourceCollectionVideoList*>(recyclingGrid->getDataSource());
    if (!ds || index >= ds->getItemCount()) return;

    auto csrf = ProgramConfig::instance().getCSRF();
    if (csrf.empty()) {
        DialogHelper::showDialog("wiliwili/home/common/no_login"_i18n);
        return;
    }

    const auto item = ds->getList()[index];
    DialogHelper::showCancelableDialog(
        fmt::format("确定将「{}」从当前收藏夹中移除吗？", item.title),
        [this, csrf, item, index]() {
            ASYNC_RETAIN
            BILI::add_resource(
                csrf, static_cast<uint64_t>(item.id), item.type, "", std::to_string(collectionData.id),
                [ASYNC_TOKEN, index]() {
                    brls::sync([ASYNC_TOKEN, index]() {
                        ASYNC_RELEASE
                        auto* currentDs =
                            dynamic_cast<DataSourceCollectionVideoList*>(recyclingGrid->getDataSource());
                        if (currentDs && index < currentDs->getItemCount()) {
                            currentDs->removeAt(index);
                            if (collectionData.media_count > 0) collectionData.media_count--;
                            updateSubtitle();
                            recyclingGrid->reloadData();
                        }
                        brls::Application::notify("已从收藏夹移除");
                    });
                },
                [ASYNC_TOKEN](BILI_ERR) {
                    brls::sync([ASYNC_TOKEN, error]() {
                        ASYNC_RELEASE
                        DialogHelper::showDialog(error);
                    });
                });
        });
}

void MineCollectionVideoList::clearViewedVideos() {
    if (!isOwnerCollection) return;
    auto* ds = dynamic_cast<DataSourceCollectionVideoList*>(recyclingGrid->getDataSource());
    if (!ds) return;

    auto csrf = ProgramConfig::instance().getCSRF();
    if (csrf.empty()) {
        DialogHelper::showDialog("wiliwili/home/common/no_login"_i18n);
        return;
    }

    auto itemsToRemove = std::make_shared<std::vector<std::pair<int, int>>>();
    for (const auto& r : ds->getList()) {
        bool isWatched = (!r.bvid.empty() && watchedBvids.count(r.bvid) > 0) ||
                         (r.id > 0 && watchedAids.count(static_cast<uint64_t>(r.id)) > 0);
        if (isWatched) {
            itemsToRemove->emplace_back(r.id, r.type);
        }
    }

    if (itemsToRemove->empty()) {
        brls::Application::notify("当前已加载列表中未检测到已看完的视频（可按 Delete / Y 键单独移除）");
        return;
    }

    DialogHelper::showCancelableDialog(
        fmt::format("检测到 {} 个已看完的视频，确定从当前收藏夹中移除吗？", itemsToRemove->size()),
        [this, itemsToRemove, csrf]() {
            this->removeNextWatchedItem(itemsToRemove, 0, 0, csrf);
        });
}

void MineCollectionVideoList::removeNextWatchedItem(std::shared_ptr<std::vector<std::pair<int, int>>> items, size_t idx,
                                                    size_t removedCount, const std::string& csrf) {
    if (idx >= items->size()) {
        recyclingGrid->reloadData();
        updateSubtitle();
        brls::Application::notify(fmt::format("已移除 {} 个已看完的视频", removedCount));
        return;
    }

    auto [rid, rtype] = (*items)[idx];
    ASYNC_RETAIN
    BILI::add_resource(
        csrf, static_cast<uint64_t>(rid), rtype, "", std::to_string(collectionData.id),
        [ASYNC_TOKEN, items, idx, removedCount, csrf, rid]() {
            brls::sync([ASYNC_TOKEN, items, idx, removedCount, csrf, rid]() {
                ASYNC_RELEASE
                auto* ds = dynamic_cast<DataSourceCollectionVideoList*>(recyclingGrid->getDataSource());
                if (ds) {
                    ds->removeById(rid);
                }
                if (collectionData.media_count > 0) collectionData.media_count--;
                this->removeNextWatchedItem(items, idx + 1, removedCount + 1, csrf);
            });
        },
        [ASYNC_TOKEN, items, idx, removedCount, csrf](BILI_ERR) {
            brls::sync([ASYNC_TOKEN, items, idx, removedCount, csrf]() {
                ASYNC_RELEASE
                this->removeNextWatchedItem(items, idx + 1, removedCount, csrf);
            });
        });
}

void MineCollectionVideoList::requestCollectionList() {
    if (!hasMore) return;

    brls::Logger::debug("requestCollectionList id: {}, type: {}, index: {}", collectionData.id, requestType,
                        requestIndex);

    ASYNC_RETAIN
    BILI::get_collection_video_list(
        collectionData.id, (int)requestIndex, 20, requestType,
        [ASYNC_TOKEN](const bilibili::CollectionVideoListResultWrapper& result) {
            brls::sync([ASYNC_TOKEN, result]() {
                ASYNC_RELEASE
                hasMore = result.has_more;
                if (requestIndex != result.index) {
                    brls::Logger::error("request index error: request {} got {}", requestIndex, result.index);
                }
                requestIndex++;

                auto* datasource = dynamic_cast<DataSourceCollectionVideoList*>(recyclingGrid->getDataSource());
                if (datasource && result.index != 1) {
                    if (!result.medias.empty()) {
                        datasource->appendData(result.medias);
                        recyclingGrid->notifyDataChanged();
                    }
                } else {
                    collectionData.media_count = result.info.media_count;
                    collectionData.upper       = result.info.upper;
                    isOwnerCollection = (requestType == COLLECTION_UI_TYPE_1) &&
                                        (std::to_string(result.info.upper.mid) == ProgramConfig::instance().getUserID());
                    btnClearViewed->setVisibility(isOwnerCollection ? brls::Visibility::VISIBLE : brls::Visibility::GONE);

                    recyclingGrid->setDataSource(
                        new DataSourceCollectionVideoList(result.medias, &watchedBvids, &watchedAids));
                    labelTitle->setText(result.info.title);
                    updateSubtitle();
                    ImageHelper::with(this->imageCover)->load(result.info.cover);
                }
            });
        },
        [ASYNC_TOKEN](BILI_ERR) {
            brls::sync([ASYNC_TOKEN, error]() {
                ASYNC_RELEASE
                this->recyclingGrid->setError(error);
            });
        });
}