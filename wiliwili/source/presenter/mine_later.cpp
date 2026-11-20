//
// Created by 贾海峰 on 2023/7/7.
//

#include <borealis/core/thread.hpp>
#include "bilibili.h"
#include "presenter/mine_later.hpp"
#include "borealis/core/i18n.hpp"
#include "utils/config_helper.hpp"

using namespace brls::literals;

void MineLaterRequest::onWatchLaterList(const bilibili::WatchLaterListWrapper& result) {}

void MineLaterRequest::onDelWatchLaterSuccess(size_t index) {}

void MineLaterRequest::onActionError(const std::string& error) {}

void MineLaterRequest::onWatchedHistoryUpdated() {}

void MineLaterRequest::onError(const std::string& error) {}

void MineLaterRequest::requestData(bool refresh) { this->requestWatchLaterList(); }

void MineLaterRequest::requestWatchLaterList() {
    auto mid = ProgramConfig::instance().getUserID();
    if (mid.empty() || mid == "0") {
        this->onError("wiliwili/home/common/no_login"_i18n);
        return;
    }
    BILI::getWatchLater([this](const auto& result) { this->onWatchLaterList(result); },
                        [this](BILI_ERR) { this->onError(error); });
}

void MineLaterRequest::requestDelWatchLater(uint64_t aid, size_t index) {
    auto csrf = ProgramConfig::instance().getCSRF();
    if (csrf.empty()) {
        this->onActionError("wiliwili/home/common/no_login"_i18n);
        return;
    }
    ASYNC_RETAIN
    BILI::delWatchLater(
        csrf, aid,
        [ASYNC_TOKEN, index]() {
            brls::sync([ASYNC_TOKEN, index]() {
                ASYNC_RELEASE
                this->onDelWatchLaterSuccess(index);
            });
        },
        [ASYNC_TOKEN](BILI_ERR) {
            brls::sync([ASYNC_TOKEN, error]() {
                ASYNC_RELEASE
                this->onActionError(error);
            });
        });
}

void MineLaterRequest::requestWatchedHistory() {
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
                this->onWatchedHistoryUpdated();
            });
        },
        [ASYNC_TOKEN](BILI_ERR) {
            brls::sync([ASYNC_TOKEN]() { ASYNC_RELEASE });
        });
}