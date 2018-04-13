//
//  mempool_index.hpp
//  blocksci
//
//  Created by Harry Kalodner on 4/12/18.
//

#ifndef mempool_index_hpp
#define mempool_index_hpp

#include <blocksci/blocksci_export.h>

#include <blocksci/util/file_mapper.hpp>
#include <blocksci/util/data_configuration.hpp>

#include <range/v3/algorithm/upper_bound.hpp>
#include <range/v3/view/transform.hpp>

#include <chrono>

namespace blocksci {
    
    struct MempoolRecord {
        time_t time;
    };
    
    struct TimestampIndex {
        FixedSizeFileMapper<MempoolRecord> timestampFile;
        uint32_t firstTxIndex;
        
        explicit TimestampIndex(const boost::filesystem::path &path, uint32_t firstTxIndex_) : timestampFile(path), firstTxIndex(firstTxIndex_) {}
        
        ranges::optional<std::chrono::system_clock::time_point> getTimestamp(uint32_t index) const {
            auto record = timestampFile.getData(index - firstTxIndex);
            if (record->time > 1) {
                return std::chrono::system_clock::from_time_t(record->time);
            } else {
                return ranges::nullopt;
            }
        }
        
        bool observed(uint32_t index) const {
            auto record = timestampFile.getData(index - firstTxIndex);
            if (record->time > 0) {
                return true;
            } else {
                return false;
            }
        }
        
        bool contains(uint32_t index) const {
            return index >= firstTxIndex && (index - firstTxIndex) < timestampFile.size();
        }
        
        void reload() {
            timestampFile.reload();
        }
    };
    
    class MempoolIndex {
        DataConfiguration config;
        FixedSizeFileMapper<uint32_t> recordingPositions;
        std::vector<TimestampIndex> timestampFiles;
        
        void setup() {
            timestampFiles.clear();
            for (size_t i = 0; i < recordingPositions.size(); i++) {
                timestampFiles.emplace_back(config.mempoolDirectory()/std::to_string(i), *recordingPositions[i]);
            }
        }
        
    public:
        explicit MempoolIndex(const DataConfiguration &config_) :  config(config_), recordingPositions(config.mempoolDirectory()/"index") {
            setup();
        }
        
        ranges::optional<std::reference_wrapper<const TimestampIndex>> selectPossibleRecording(uint32_t txIndex) const {
            if (recordingPositions.size() > 0) {
                auto it = ranges::upper_bound(timestampFiles, txIndex, ranges::ordered_less(), &TimestampIndex::firstTxIndex);
                if (it != timestampFiles.begin()) {
                    --it;
                }
                if (it->contains(txIndex)) {
                    return std::ref(*it);
                }
            }
            return ranges::nullopt;
        }

        ranges::optional<std::chrono::system_clock::time_point> getTimestamp(uint32_t index) const {
            auto possibleFile = selectPossibleRecording(index);
            if (possibleFile) {
                return (*possibleFile).get().getTimestamp(index);
            } else {
                return ranges::nullopt;
            }
        }
        
        bool observed(uint32_t index) const {
            auto possibleFile = selectPossibleRecording(index);
            if (possibleFile) {
                return (*possibleFile).get().observed(index);
            } else {
                return false;
            }
        }
        
        void reload() {
            for (auto &file : timestampFiles) {
                file.reload();
            }
            setup();
        }
    };
} // namespace blocksci

#endif /* mempool_index_hpp */
