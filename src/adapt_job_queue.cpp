#include "adapt_job_queue.h"

#include <algorithm>
#include <utility>

void AdaptJobQueue::push_incremental(AdaptJob job) {
    job.kind = AdaptJobKind::Incremental;
    const bool final_is_pending = std::any_of(
        jobs_.begin(), jobs_.end(), [&](const AdaptJob & pending) {
            return pending.session_id == job.session_id &&
                   pending.kind == AdaptJobKind::Final;
        });
    if (final_is_pending) {
        return;
    }
    jobs_.erase(
        std::remove_if(jobs_.begin(), jobs_.end(), [&](const AdaptJob & pending) {
            return pending.session_id == job.session_id &&
                   pending.kind == AdaptJobKind::Incremental;
        }),
        jobs_.end());
    jobs_.push_back(std::move(job));
}

void AdaptJobQueue::push_final(AdaptJob job) {
    job.kind = AdaptJobKind::Final;
    jobs_.erase(
        std::remove_if(jobs_.begin(), jobs_.end(), [&](const AdaptJob & pending) {
            return pending.session_id == job.session_id &&
                   pending.kind != AdaptJobKind::ModelPath;
        }),
        jobs_.end());
    jobs_.push_back(std::move(job));
}

void AdaptJobQueue::push_model_path(AdaptJob job) {
    job.kind = AdaptJobKind::ModelPath;
    jobs_.push_back(std::move(job));
}

void AdaptJobQueue::cancel_session(uint64_t session_id) {
    jobs_.erase(
        std::remove_if(jobs_.begin(), jobs_.end(), [&](const AdaptJob & pending) {
            return pending.session_id == session_id &&
                   pending.kind != AdaptJobKind::ModelPath;
        }),
        jobs_.end());
}

bool AdaptJobQueue::empty() const {
    return jobs_.empty();
}

size_t AdaptJobQueue::size() const {
    return jobs_.size();
}

std::optional<AdaptJob> AdaptJobQueue::pop_front() {
    if (jobs_.empty()) {
        return std::nullopt;
    }
    AdaptJob job = std::move(jobs_.front());
    jobs_.pop_front();
    return job;
}
