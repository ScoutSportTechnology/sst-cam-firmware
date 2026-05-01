#pragma once

#include <string>
#include <vector>

#include "adapters/db/sqlite/db-helpers.hpp"
#include "app/db/ports/recording-repository.hpp"

namespace sst::db {

class SqliteRecordingRepository : public IRecordingRepository, private SqliteRepositoryBase {
   public:
    explicit SqliteRecordingRepository(DbConnection& conn);

    auto save(const Recording& data) -> DbResult<Recording> override;
    auto get(const std::string& recording_id) -> DbResult<Recording> override;
    auto listByMatch(const std::string& match_id) -> DbResult<std::vector<Recording>> override;
    auto finalize(const std::string& recording_id, const std::string& ended_at)
        -> DbResult<bool> override;
    auto remove(const std::string& recording_id) -> DbResult<bool> override;

    auto saveSegment(const RecordingSegment& data) -> DbResult<RecordingSegment> override;
    auto listSegments(const std::string& recording_id)
        -> DbResult<std::vector<RecordingSegment>> override;
    auto finalizeSegment(int64_t segment_id, const std::string& ended_at)
        -> DbResult<bool> override;

    auto saveEventClip(const EventClip& data) -> DbResult<EventClip> override;
    auto getEventClip(const std::string& event_clip_id) -> DbResult<EventClip> override;
    auto listEventClipsForMatch(const std::string& match_id)
        -> DbResult<std::vector<EventClip>> override;
};

}  // namespace sst::db
