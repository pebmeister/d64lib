// Written by Paul Baxter
/**
 * @file d64.h
 * @author Paul Baxter
 * @brief Class definition for Commodore 64 D64 disk image manipulation.
 */

#pragma once
#include <string>
#include <vector>
#include <array>
#include <optional>
#include <functional>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <bitset>

#include "d64_types.h"

#pragma pack(push, 1)

/**
 * @class d64
 * @brief Represents and manipulates a Commodore 64 D64 disk image.
 * 
 * Provides a comprehensive API for reading, writing, formatting, and 
 * manipulating files, directories, and raw sectors on a D64 disk image, 
 * including full support for Block Availability Map (BAM) operations.
 */
class d64 {
public:

    /** @brief Default constructor. Initializes an empty 35-track disk. */
    d64();
    
    /** 
     * @brief Constructor initializing a specific disk type.
     * @param type The disk type (e.g., 35-track or 40-track).
     */
    d64(diskType type);
    
    /** 
     * @brief Constructor that loads an existing D64 image from a file.
     * @param name The file path to the D64 image.
     */
    d64(std::string name);

    /**
     * @brief Formats the disk image, clearing all data and re-initializing the BAM.
     * @param name The new disk name.
     */
    void formatDisk(std::string_view name);
    
    /**
     * @brief Renames the disk volume header.
     * @param name The new disk name.
     * @return true if successful, false otherwise.
     */
    bool rename_disk(std::string_view name) const;
    
    /**
     * @brief Gets the current disk name from the directory header.
     * @return A string containing the disk name.
     */
    std::string diskname();
    
    /**
     * @brief Locates a file in the disk directory.
     * @param filename The name of the file to find.
     * @return An optional pointer to the directory entry if found.
     */
    std::optional<directoryEntryPtr> findFile(std::string_view filename);
    
    /**
     * @brief Adds a new file to the disk image.
     * @param filename The name of the new file.
     * @param type The Commodore 64 file type (PRG, SEQ, USR, REL).
     * @param fileData A vector containing the raw file bytes.
     * @param recirdSize The record size (only applicable for REL files).
     * @return true if the file was successfully added, false if disk is full or error occurs.
     */
    bool addFile(std::string_view filename, c64FileType type, const std::vector<uint8_t>& fileData, int recirdSize = 0);
    
    /**
     * @brief Removes (scratches) a file from the disk and frees its sectors in the BAM.
     * @param filename The name of the file to remove.
     * @return true if successful, false if the file was not found.
     */
    bool removeFile(std::string_view filename);
    
    /**
     * @brief Renames an existing file on the disk.
     * @param oldfilename The current name of the file.
     * @param newfilename The new name for the file.
     * @return true if successful.
     */
    bool renameFile(std::string_view oldfilename, std::string_view newfilename);
    
    /**
     * @brief Extracts a file from the disk image to the host filesystem.
     * @param filename The name of the file to extract.
     * @return true if extraction was successful.
     */
    bool extractFile(std::string filename);
    
    /**
     * @brief Saves the current state of the D64 image to the host filesystem.
     * @param filename The destination file path.
     * @return true if successful.
     */
    bool save(std::string filename);
    
    /**
     * @brief Loads a D64 image from the host filesystem into memory.
     * @param filename The source file path.
     * @return true if successful.
     */
    bool load(std::string filename);
    
    /**
     * @brief Calculates the absolute byte offset of a given track and sector.
     * @param track The track number (1-based).
     * @param sector The sector number (0-based).
     * @return The linear byte offset in the raw disk image data.
     */
    int calcOffset(int track, int sector) const;
    
    /**
     * @brief Writes a single byte to a specific position within a sector.
     * @param track The track number.
     * @param sector The sector number.
     * @param offset The byte offset within the sector (0-255).
     * @param value The byte value to write.
     * @return true if successful.
     */
    bool writeByte(int track, int sector, int offset, uint8_t value);
    
    /**
     * @brief Writes an entire sector of data to the disk.
     * @param track The track number.
     * @param sector The sector number.
     * @param bytes A vector containing the sector data (typically 256 bytes).
     * @return true if successful.
     */
    bool writeSector(int track, int sector, std::vector<uint8_t> bytes);
    
    /**
     * @brief Reads a single byte from a specific position in a sector.
     * @param track The track number.
     * @param sector The sector number.
     * @param offset The byte offset within the sector (0-255).
     * @return An optional containing the byte if read was successful.
     */
    std::optional<uint8_t> readByte(int track, int sector, int offset);
    
    /**
     * @brief Reads an entire sector from the disk.
     * @param track The track number.
     * @param sector The sector number.
     * @return An optional vector containing the sector data if successful.
     */
    std::optional<std::vector<uint8_t>> readSector(int track, int sector);
    
    /**
     * @brief Frees a specific sector in the Block Availability Map (BAM).
     * @param track The track number.
     * @param sector The sector number.
     * @return true if successful.
     */
    bool freeSector(const int& track, const int& sector);
    
    /**
     * @brief Allocates a specific sector in the Block Availability Map (BAM).
     * @param track The track number.
     * @param sector The sector number.
     * @return true if successful.
     */
    bool allocateSector(const int& track, const int& sector);
    
    /**
     * @brief Finds the next free sector on the disk and allocates it.
     * @param track [in,out] The suggested starting track; updated to the allocated track.
     * @param sector [in,out] Updated to the allocated sector.
     * @param directory True if allocating for directory space (changes track preference).
     * @return true if a sector was found and allocated.
     */
    bool findAndAllocateFreeSector(int& track, int& sector, bool directory = false);
    
    /**
     * @brief Reads the entire contents of a file from the disk.
     * @param filename The name of the file to read.
     * @return An optional vector containing the raw file payload.
     */
    std::optional<std::vector<uint8_t>> readFile(std::string filename);
    
    /**
     * @brief Reads a specific record from a REL (Relative) file.
     * @param filename The name of the REL file.
     * @param recordNumber The index of the record to read (1-based).
     * @return An optional vector containing the record data.
     */
    std::optional<std::vector<uint8_t>> readRecord(std::string_view filename, int recordNumber);
    
    /**
     * @brief Writes data to a specific record in a REL file, expanding it if necessary.
     * @param filename The name of the REL file.
     * @param recordNumber The index of the record (1-based).
     * @param recordData The data to write.
     * @return true if successful.
     */
    bool writeRecord(std::string_view filename, int recordNumber, const std::vector<uint8_t>& recordData);
    
    /**
     * @brief Appends a new record to the end of a REL file.
     * @param filename The name of the REL file.
     * @param recordData The data for the new record.
     * @return true if successful.
     */
    bool appendRecord(std::string_view filename, const std::vector<uint8_t>& recordData);
    
    /**
     * @brief Clears/deletes a specific record in a REL file.
     * @param filename The name of the REL file.
     * @param recordNumber The index of the record to delete.
     * @return true if successful.
     */
    bool deleteRecord(std::string_view filename, int recordNumber);
    
    /**
     * @brief Gets the total number of records in a REL file.
     * @param filename The name of the REL file.
     * @return The record count.
     */
    int getRecordCount(std::string_view filename);
    
    /**
     * @brief Gets the fixed record size of a given REL file.
     * @param filename The name of the REL file.
     * @return The record size in bytes.
     */
    int getRecordSize(std::string_view filename);
    
    /**
     * @brief Calculates the total number of free sectors available on the disk.
     * @return The free sector count based on the BAM.
     */
    uint16_t getFreeSectorCount();
    
    /**
     * @brief Compacts the disk directory, removing scratched file entries to save space.
     * @return true if successful.
     */
    bool compactDirectory();
    
    /**
     * @brief Validates the BAM against actual file sector chains.
     * @param fix If true, corrects the BAM to reflect actual sector usage.
     * @param logFile Path to write the validation log to.
     * @return true if the BAM is intact or was successfully fixed.
     */
    bool verifyBAMIntegrity(bool fix, const std::string& logFile);
    
    /**
     * @brief Reorders the directory entries using a custom comparison function.
     * @param compare Lambda/function defining the sorting logic.
     * @return true if successful.
     */
    bool reorderDirectory(std::function<bool(const directoryEntry&, const directoryEntry&)> compare);
    
    /**
     * @brief Reorders the directory based on a given vector of entries.
     * @param files Vector of ordered directory entries.
     * @return true if successful.
     */
    bool reorderDirectory(std::vector<directoryEntry>& files);
    
    /**
     * @brief Reorders the directory based on an ordered list of file names.
     * @param fileOrder Vector of filenames in the desired order.
     * @return true if successful.
     */
    bool reorderDirectory(const std::vector<std::string>& fileOrder);
    
    /**
     * @brief Moves a specific file to the first position in the directory.
     * @param file The name of the file to move.
     * @return true if successful.
     */
    bool movefileFirst(std::string file);
    
    /**
     * @brief Locks or unlocks a file (setting the locked flag in the directory entry).
     * @param file The name of the file.
     * @param lock true to lock, false to unlock.
     * @return true if successful.
     */
    bool lockfile(std::string file, bool lock);
    
    /**
     * @brief Retrieves all active directory entries on the disk.
     * @return A vector of valid directoryEntry objects.
     */
    std::vector<directoryEntry> directory();
    
    /**
     * @brief Trims trailing A0 padding bytes from a raw C64 filename.
     * @param filename Raw 16-byte filename array.
     * @return A standard std::string representing the trimmed filename.
     */
    static std::string Trim(const char filename[FILE_NAME_SZ]);

    /** @brief Number of tracks for the currently loaded disk image type. */
    int TRACKS;

    /** @brief Lookup table for the number of sectors per track (standard CBM formatting). */
    static constexpr std::array<int, TRACKS_40> SECTORS_PER_TRACK = {
        21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, // Tracks 1-17
        19, 19, 19, 19, 19, 19, 19,                                         // Tracks 18-24
        18, 18, 18, 18, 18, 18,                                             // Tracks 25-30
        17, 17, 17, 17, 17,                                                 // Tracks 31-35
        17, 17, 17, 17, 17                                                  // Tracks 36-40
    };

    /** @brief Precomputed absolute byte offsets for the start of each track. */
    static constexpr std::array<int, TRACKS_40> TRACK_OFFSETS = {
        0x00000, 0x01500, 0x02A00, 0x03F00, 0x05400, 0x06900, 0x07E00, 0x09300, 0x0A800, 0x0BD00,
        0x0D200, 0x0E700, 0x0FC00, 0x11100, 0x12600, 0x13B00, 0x15000, 0x16500, 0x17800, 0x18B00,
        0x19E00, 0x1B100, 0x1C400, 0x1D700, 0x1EA00, 0x1FC00, 0x20E00, 0x22000, 0x23200, 0x24400,
        0x25600, 0x26700, 0x27800, 0x28900, 0x29A00, 0x2AB00, 0x2BC00, 0x2CD00, 0x2DE00, 0x2EF00
    };

    /**
     * @brief Gets a pointer to the BAM entry for a given track.
     * @param t The track number (0-based internally for the lookup).
     * @return Pointer to the bamTrackEntry.
     */
    inline bamTrackEntry* bamtrack(int t)
    {
        return (t < TRACKS_35) ?
            &bamTrackPtr[(t)] :
            &bamExtraTrackPtr[((t)-TRACKS_35)];
    }

    /**
     * @brief Retrieves a generic pointer to a sector.
     * @param track The track number.
     * @param sector The sector number.
     * @return sectorPtr casted to the absolute memory offset.
     */
    inline sectorPtr getSectorPtr(uint8_t track, uint8_t sector)
    {
        return reinterpret_cast<sectorPtr>(&data[calcOffset(track, sector)]);
    }

    /**
     * @brief Retrieves a pointer representing the Track/Sector linkage struct.
     * @param track The track number.
     * @param sector The sector number.
     * @return trackSector pointer.
     */
    inline trackSector* getTrackSectorPtr(uint8_t track, uint8_t sector)
    {
        return reinterpret_cast<trackSector*>(&data[calcOffset(track, sector)]);
    }

    /**
     * @brief Retrieves a pointer to a REL file side sector structure.
     * @param track The track number.
     * @param sector The sector number.
     * @return sideSectorPtr pointer.
     */
    inline sideSectorPtr getSideSectorPtr(uint8_t track, uint8_t sector)
    {
        return reinterpret_cast<sideSectorPtr>(&data[calcOffset(track, sector)]);
    }

    /**
     * @brief Retrieves a pointer to a directory sector structure.
     * @param track The track number.
     * @param sector The sector number.
     * @return directorySectorPtr pointer.
     */
    inline directorySectorPtr getDirectory_SectorPtr(const int& track, const int& sector)
    {
        return reinterpret_cast<directorySectorPtr>(&data[calcOffset(track, sector)]);
    }

private:
    /** @brief Helper struct for validating circular or infinite sector chains. */
    struct sector_chain_validator {
        const d64& disk;
        std::bitset<TRACKS_40 * 21> visited;

        explicit sector_chain_validator(const d64& d) : disk(d) {}

        bool visit(int track, int sector)
        {
            if (track == 0) return true;
            if (!disk.isValidTrackSector(track, sector)) return false;
            size_t idx = static_cast<size_t>(disk.calcOffset(track, sector) / SECTOR_SIZE);
            if (visited.test(idx)) return false;
            visited.set(idx);
            return true;
        }
    };

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
    bool writeDirectoryChain(const std::vector<directoryEntry>& files);
    bool expandRelFile(std::string_view filename, int requiredBytes);

    /** @brief Sets up internal BAM pointer maps based on Track 18, Sector 0. */
    inline void initBAMPtr()
    {
        auto index = calcOffset(DIRECTORY_TRACK, BAM_SECTOR);
        diskBamPtr = reinterpret_cast<bamPtr>(&data[index]);
        bamTrackPtr = &(diskBamPtr->bamTrack[0]);
        bamExtraTrackPtr = reinterpret_cast<bamTrackEntry*>(&data[index + 0xAC]);
    }
    
    bool isValidTrackSector(int track, int sector) const;

    /** @brief The raw buffer containing the entire D64 disk image in memory. */
    std::vector<uint8_t> data;
};

#pragma pack(pop)