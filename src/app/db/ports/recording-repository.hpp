#pragma once

#include <string>
#include <vector>

#include "domain/db/models/db-result.hpp"
#include "domain/db/models/recording.hpp"

namespace sst::db {

class IRecordingRepository {
   public:
    virtual ~IRecordingRepository() = default;

    // Recordings (full-game or event-clip parents).
    virtual auto save(const Recording& data) -> DbResult<Recording> = 0;
    virtual auto get(const std::string& recording_id) -> DbResult<Recording> = 0;
    virtual auto listByMatch(const std::string& match_id) -> DbResult<std::vector<Recording>> = 0;
    virtual auto finalize(const std::string& recording_id, const std::string& ended_at)
        -> DbResult<bool> = 0;
    virtual auto remove(const std::string& recording_id) -> DbResult<bool> = 0;

    // Segments belong to a full-game recording (one row per pause→resume cycle).
    virtual auto saveSegment(const RecordingSegment& data) -> DbResult<RecordingSegment> = 0;
    virtual auto listSegments(const std::string& recording_id)
        -> DbResult<std::vector<RecordingSegment>> = 0;
    virtual auto finalizeSegment(int64_t segment_id, const std::string& ended_at)
        -> DbResult<bool> = 0;

    // Event clips link a match_event to its recorded mp4.
    virtual auto saveEventClip(const EventClip& data) -> DbResult<EventClip> = 0;
    virtual auto getEventClip(const std::string& event_clip_id) -> DbResult<EventClip> = 0;
    virtual auto listEventClipsForMatch(const std::string& match_id)
        -> DbResult<std::vector<EventClip>> = 0;
};

}  // namespace sst::db
