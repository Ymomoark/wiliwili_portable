//
// Created by fang on 2022/7/31.
//

// register this fragment in main.cpp
//#include "fragment/mine_collection_video_list.hpp"
//    brls::Application::registerXMLView("MineCollectionVideoList", MineCollectionVideoList::create);
// <brls:View xml=@res/xml/fragment/mine_collection_video_list.xml

#pragma once

#include <unordered_set>
#include <borealis/core/box.hpp>
#include <borealis/core/bind.hpp>

#include "bilibili.h"
#include "view/recycling_grid.hpp"
#include "bilibili/result/mine_collection_result.h"
#include "bilibili/result/mine_history_result.h"

namespace brls {
class Button;
}
class TextBox;

class MineCollectionVideoList : public brls::Box {
public:
    MineCollectionVideoList();

    MineCollectionVideoList(const bilibili::CollectionResult& data, int type);

    ~MineCollectionVideoList() override;

    static View* create();

    void requestCollectionList();

private:
    void requestWatchedHistory(const bilibili::HistoryVideoListCursor& cursor = {}, int pagesLeft = 2);

    void removeItemAt(size_t index);

    void clearViewedVideos();

    void removeNextWatchedItem(std::shared_ptr<std::vector<std::pair<int, int>>> items, size_t idx, size_t removedCount,
                               const std::string& csrf);

    void updateSubtitle();

    bilibili::CollectionResult collectionData;
    std::unordered_set<std::string> watchedBvids;
    std::unordered_set<uint64_t> watchedAids;
    bool isOwnerCollection = false;

    BRLS_BIND(TextBox, labelTitle, "collection/label/title");
    BRLS_BIND(brls::Label, labelSubtitle, "collection/label/subtitle");
    BRLS_BIND(brls::Image, imageCover, "collection/cover");
    BRLS_BIND(RecyclingGrid, recyclingGrid, "collection/recyclingGrid");
    BRLS_BIND(brls::AppletFrame, appletFrame, "collection/appletFrame");
    BRLS_BIND(brls::Button, btnClearViewed, "collection/btn/clear_viewed");

    unsigned int requestIndex = 1;
    int requestType           = 1;
    bool hasMore              = true;
};