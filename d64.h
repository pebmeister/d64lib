// Written by Paul Baxter
#pragma once
#include <string>
#include <array>
#include <optional>
#include <functional>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <bitset>

#include "d64_types.h"

#pragma pack(push, 1)

class d64 {
public:

    d64();
    d64(diskType type);
    d64(std::string name);

    void formatDisk(std::string_view name);
    bool rename_disk(std::string_view name) const;
    std::string diskname();
    std::optional<directoryEntryPtr> findFile(std::string_view filename);
    bool addFile(std::string_view filename, c64FileType type, const std::vector<uint8_t>& fileData, int recirdSize = 0);
    bool removeFile(std::string_view filename);
    bool renameFile(std::string_view oldfilename, std::string_view newfilename);
    bool extractFile(std::string filename);
    bool save(std::string filename);
    bool load(std::string filename);
    int calcOffset(int track, int sector) const;
    bool writeByte(int track, int sector, int offset, uint8_t value);
    bool writeSector(int track, int sector, std::vector<uint8_t> bytes);
    std::optional<uint8_t> readByte(int track, int sector, int offset);
    std::optional<std::vector<uint8_t>> readSector(int track, int sector);
    bool freeSector(const int& track, const int& sector);
    bool allocateSector(const int& track, const int& sector);
    bool findAndAllocateFreeSector(int& track, int& sector, bool directory);
    std::optional<std::vector<uint8_t>> readFile(std::string filename);
    std::optional<std::vector<uint8_t>> readRecord(std::string_view filename, int recordNumber);
    bool writeRecord(std::string_view filename, int recordNumber, const std::vector<uint8_t>& recordData);
    bool appendRecord(std::string_view filename, const std::vector<uint8_t>& recordData);
    bool deleteRecord(std::string_view filename, int recordNumber);
    int getRecordCount(std::string_view filename);
    int getRecordSize(std::string_view filename);
    uint16_t getFreeSectorCount();
    bool compactDirectory();
    bool verifyBAMIntegrity(bool fix, const std::string& logFile);
    bool reorderDirectory(std::function<bool(const directoryEntry&, const directoryEntry&)> compare);
    bool reorderDirectory(std::vector<directoryEntry>& files);
    bool reorderDirectory(const std::vector<std::string>& fileOrder);
    bool movefileFirst(std::string file);
    bool lockfile(std::string file, bool lock);
    std::vector<directoryEntry> directory();
    static std::string Trim(const char filename[FILE_NAME_SZ]);

    int TRACKS;

    // Constants for D64 format
    static constexpr std::array<int, TRACKS_40> SECTORS_PER_TRACK = {
        21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, // Tracks 1-17
        19, 19, 19, 19, 19, 19, 19,                                         // Tracks 18-24
        18, 18, 18, 18, 18, 18,                                             // Tracks 25-30
        17, 17, 17, 17, 17,                                                 // Tracks 31-35
        17, 17, 17, 17, 17                                                  // Tracks 36-40
    };

    static constexpr std::array<int, TRACKS_40> TRACK_OFFSETS = {
        0x00000, 0x01500, 0x02A00, 0x03F00, 0x05400, 0x06900, 0x07E00, 0x09300, 0x0A800, 0x0BD00,
        0x0D200, 0x0E700, 0x0FC00, 0x11100, 0x12600, 0x13B00, 0x15000, 0x16500, 0x17800, 0x18B00,
        0x19E00, 0x1B100, 0x1C400, 0x1D700, 0x1EA00, 0x1FC00, 0x20E00, 0x22000, 0x23200, 0x24400,
        0x25600, 0x26700, 0x27800, 0x28900, 0x29A00, 0x2AB00, 0x2BC00, 0x2CD00, 0x2DE00, 0x2EF00
    };

    inline bamTrackEntry* bamtrack(int t)
    {
        return (t < TRACKS_35) ?
            &bamTrackPtr[(t)] :
            &bamExtraTrackPtr[((t)-TRACKS_35)];
    }
    inline sectorPtr getSectorPtr(uint8_t track, uint8_t sector)
    {
        return reinterpret_cast<sectorPtr>(&data[calcOffset(track, sector)]);
    }
    inline trackSector* getTrackSectorPtr(uint8_t track, uint8_t sector)
    {
        return reinterpret_cast<trackSector*>(&data[calcOffset(track, sector)]);
    }
    inline sideSectorPtr getSideSectorPtr(uint8_t track, uint8_t sector)
    {
        return reinterpret_cast<sideSectorPtr>(&data[calcOffset(track, sector)]);
    }
    inline directorySectorPtr getDirectory_SectorPtr(const int& track, const int& sector)
    {
        return reinterpret_cast<directorySectorPtr>(&data[calcOffset(track, sector)]);
    }

private:
    static constexpr int INTERLEAVE = 10;
    std::array<int, TRACKS_40> lastSectorUsed = { -1 };
    bamPtr diskBamPtr;
    bamTrackEntry* bamTrackPtr;
    bamTrackEntry* bamExtraTrackPtr;
    diskType disktype = diskType::thirty_five_track;

    bool validateD64();
    void initBAM(std::string_view name);
    void initializeBAMFields(std::string_view name);
    bool writeData(int track, int sector, std::vector<uint8_t> bytes, int byteoffset);
    std::vector<trackSector> parseSideSectors(int sideTrack, int sideSector);
    void init_disk();
    bool findAndAllocateFreeOnTrack(int t, int& sector);
    std::optional<directoryEntryPtr> findEmptyDirectorySlot();
    bool allocateSideSector(int& track, int& sector, sideSectorPtr& side);
    bool allocateDataSector(int& track, int& sector, sectorPtr& sectorPtr);
    void writeDataToSector(sectorPtr sectorPtr, const std::vector<uint8_t>& fileData, int& offset, int& bytesLeft);
    std::vector<trackSector> writeFileDataToSectors(int start_track, int start_sector, const std::vector<uint8_t>& fileData);
    std::optional<std::vector<sideSectorPtr>> createSideSectors(const std::vector<trackSector>& allocatedSectors, uint8_t record_size);
    bool createDirectoryEntry(std::string_view filename, c64FileType type, int start_track, int start_sector, const std::vector<trackSector>& allocatedSectors, uint8_t record_size);
    bool findAndAllocateFirstSector(int& start_track, int& start_sector);
    bool allocateNewDirectorySector(int& dir_track, int& dir_sector, directorySectorPtr& dirSectorPtr);
    bool expandRelFile(std::string_view filename, int requiredBytes);

    inline void initBAMPtr()
    {
        auto index = calcOffset(DIRECTORY_TRACK, BAM_SECTOR);
        diskBamPtr = reinterpret_cast<bamPtr>(&data[index]);
        bamTrackPtr = &(diskBamPtr->bamTrack[0]);
        bamExtraTrackPtr = reinterpret_cast<bamTrackEntry*>(&data[index + 0xAC]);
    }
    bool isValidTrackSector(int track, int sector) const;

    std::vector<uint8_t> data;
};

#pragma pack(pop)
