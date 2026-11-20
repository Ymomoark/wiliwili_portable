//
// Created by fang on 2022/7/31.
//

#include <utility>
#include <algorithm>
#include <unordered_set>
#include <borealis/views/applet_frame.hpp>
#include <borealis/core/thread.hpp>

#include "fragment/mine_collection_video_list.hpp"
#include "view/video_card.hpp"
#include "view/text_box.hpp"
#include "utils/image_helper.hpp"
#include "utils/activity_helper.hpp"
#include "bilibili.h"
#include "utils/shortcut_helper.hpp"
#include "utils/config_helper.hpp"
#include "utils/dialog_helper.hpp"
#include <fmt/format.h>

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

        bool watched = (watchedBvids && !r.bvid.empty() && watchedBvids->count(r.bvid) > 0) ||
                       (watchedAids && r.id > 0 && watchedAids->count(static_cast<uint64_t>(r.id)) > 0);
        std::string extra = watched ? "已看完" : "";

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
        if (index < list.size()) list.erase(list.begin() + static_cast<std::ptrdiff_t>(index));
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
                    isOwnerCollection = (requestType == COLLECTION_UI_TYPE_1) &&
                                        (std::to_string(result.info.upper.mid) == ProgramConfig::instance().getUserID());
                    recyclingGrid->setDataSource(
                        new DataSourceCollectionVideoList(result.medias, &watchedBvids, &watchedAids));
                    labelTitle->setText(result.info.title);
                    labelSubtitle->setText(fmt::format("{}{} · {}: {}", result.info.media_count,
                                                       "wiliwili/mine/num"_i18n, "wiliwili/mine/creator"_i18n,
                                                       result.info.upper.name));
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

void MineCollectionVideoList::requestWatchedHistory() {
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
        [this, csrf, item]() {
            ASYNC_RETAIN
            auto onSuccess = [ASYNC_TOKEN, item]() {
                brls::sync([ASYNC_TOKEN, item]() {
                    ASYNC_RELEASE
                    auto* currentDs = dynamic_cast<DataSourceCollectionVideoList*>(recyclingGrid->getDataSource());
                    if (currentDs) currentDs->removeById(item.id);
                    if (collectionData.media_count > 0) collectionData.media_count--;
                    recyclingGrid->reloadData();
                    brls::Application::notify("已从收藏夹移除");
                });
            };
            auto onError = [ASYNC_TOKEN](BILI_ERR) {
                brls::sync([ASYNC_TOKEN, error]() {
                    ASYNC_RELEASE
                    DialogHelper::showDialog(error);
                });
            };

            // 失效稿件（attr != 0）必须用 batch-del，普通 deal 接口会返回错误
            if (item.isInvalid()) {
                BILI::delInvalidResource(csrf, static_cast<uint64_t>(item.id),
                                         static_cast<uint64_t>(collectionData.id), onSuccess, onError);
            } else {
                BILI::add_resource(csrf, static_cast<uint64_t>(item.id), item.type, "",
                                   std::to_string(collectionData.id), onSuccess, onError);
            }
        });
}