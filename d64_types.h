// Written by Paul Baxter
#pragma once

#pragma pack(push, 1)

inline constexpr int TRACKS_35 = 35;
inline constexpr int TRACKS_40 = 40;
inline constexpr int SECTOR_SIZE = 256;
inline constexpr int DISK_NAME_SZ = 16;
inline constexpr int FILE_NAME_SZ = 16;
inline constexpr int UNUSED3_SZ = 5;
inline constexpr int UNUSED4_SZ = 84;
inline constexpr int DIR_ENTRY_SZ = 30;
inline constexpr int DIR_SLOT_STRIDE = 32;
inline constexpr int DIRECTORY_TRACK = 18;
inline constexpr int DIRECTORY_SECTOR = 1;
inline constexpr int TRACK_SECTOR = 0;
inline constexpr int BAM_SECTOR = 0;
inline constexpr int FILES_PER_SECTOR = 8;
inline constexpr int D64_DISK35_SZ = 174848;
inline constexpr int D64_DISK40_SZ = 196608;

inline constexpr int SIDE_SECTOR_ENTRY_SIZE = 6;
inline constexpr int SIDE_SECTOR_CHAIN_SZ = ((SECTOR_SIZE - 15) / (2));

static constexpr uint8_t A0_VALUE = 0xA0;
static constexpr uint8_t DOS_VERSION = 'A';
static constexpr uint8_t DOS_TYPE = '2';

enum diskType {
    thirty_five_track,
    forty_track
};

enum d64FileTypes : uint8_t {
    DEL = 0,
    SEQ = 1,
    PRG = 2,
    USR = 3,
    REL = 4
};

// Track and sector
struct trackSector {
public:
    uint8_t track;
    uint8_t sector;

    bool operator ==(const trackSector& other) const
    {
        return track == other.track && sector == other.sector;
    }

    trackSector(int track, int sector) : track(track), sector(sector) {};
    trackSector(uint8_t track, uint8_t sector) : track(track), sector(sector) {};
};

struct sector {
public:
    trackSector next;
    std::array<uint8_t, SECTOR_SIZE - sizeof(trackSector)> data;
};
typedef sector* sectorPtr;

// side sector
class sideSector {
public:
    trackSector next;                                   // $01 - $02
    uint8_t block;                                      // $02
    uint8_t recordsize;                                 // $03
    trackSector sideSectors[SIDE_SECTOR_ENTRY_SIZE];    // $04 - $0F
    trackSector chain[SIDE_SECTOR_CHAIN_SZ];            // chain T/S
};
typedef sideSector* sideSectorPtr;

class c64FileType {
public:
    d64FileTypes type : 4;
    uint8_t unused : 1;
    uint8_t replace : 1;
    uint8_t locked : 1;
    uint8_t closed : 1;

public:
    c64FileType() : closed(0), locked(0), replace(0), unused(0), type(d64FileTypes::DEL) {}
    c64FileType(bool a, bool l, d64FileTypes t) : closed(a ? 1 : 0), locked(l ? 1 : 0), replace(0), unused(0), type(t) {}
    c64FileType(d64FileTypes t) : closed(1), locked(0), replace(0), unused(0), type(t) {}
    c64FileType(uint8_t value) :
        closed(value & 0x80),
        locked(value & 0x40),
        replace(value & 0x20),
        unused(value & 0x10),
        type(static_cast<d64FileTypes>(value & 0x0F))
    {
    }

    operator uint8_t() const { return (closed << 7) | (locked << 6) | (replace << 5) | (unused << 4) | type; }
    operator d64FileTypes() const { return type; }
};

struct bamTrackEntry {

public:
    uint8_t free;
    std::array <uint8_t, 3> bytes;

    /// <summary>
    /// test if a sector is used in bam  
    /// </summary>
    /// <param name="sector">sector to test</param>
    bool test(int sector)
    {
        if (sector < 0 || sector >= 24) return false;
        auto byte = sector / 8;
        auto bit = sector % 8;

        std::bitset<8> bits(bytes[byte]);
        return bits.test(bit);
    }

    /// <summary>
    /// mark a sector as free in bam  
    /// </summary>
    /// <param name="sector">sector to mark</param>
    inline void set(int sector)
    {
        if (sector < 0 || sector >= 24) return;
        auto byte = sector / 8;
        auto bit = sector % 8;

        std::bitset<8> bits(bytes[byte]);
        bits.set(bit);
        bytes[byte] = static_cast<uint8_t>(bits.to_ulong());
    }

    /// <summary>
    /// mark a sector as used in bam  
    /// </summary>
    /// <param name="sector">sector to mark</param>
    inline void reset(int sector)
    {
        if (sector < 0 || sector >= 24) return;
        auto byte = sector / 8;
        auto bit = sector % 8;

        std::bitset<8> bits(bytes[byte]);
        bits.reset(bit);
        bytes[byte] = static_cast<uint8_t>(bits.to_ulong());
    }

    /// <summary>
    /// clear the bam sectors
    /// this marks them all as in use
    /// </summary>
    inline void clear()
    {
        bytes[0] = 0;
        bytes[1] = 0;
        bytes[2] = 0;
    }
};

// This folows DOLPHIN DOS for 40 tracks
struct bam {
    trackSector dirStart;                   // $00 - $01
    uint8_t dosVersion;                     // $02          'A' dos version
    uint8_t unused;                         // $03          unused should be 0
    bamTrackEntry bamTrack[TRACKS_35];      // $04 - $8F    BAM to each track
    char diskName[DISK_NAME_SZ];            // $90 - $9F    disk name padded with A0
    uint8_t a0[2];                          // $A0 - $A1    contains A0
    uint8_t diskId[2];                      // $A2 - $A3    disk id
    uint8_t unused2;                        // $A4          contains A0
    char dos_type[2];                       // $A5 - $A6    '2' 'A'
    uint8_t unused3[UNUSED3_SZ];            // $A7 - $AB    00
    uint8_t unused4[UNUSED4_SZ];            // $AC - $FF    00
};
typedef struct bam* bamPtr;

struct directoryEntry {
    c64FileType file_type;              // $00          file type
    trackSector start;                  // $01 - $02    first track  and sector of file entry
    char fileName[FILE_NAME_SZ];        // $03 - $12    file name padded with $A0
    trackSector side;                   // $13 - $14    first side track/sector .REL file only
    uint8_t recordLength;               // $15          record side track .REL file only
    uint8_t unused[4];                  // $16 - $19    unused
    trackSector replace;                // $1A - $1B    track / sector of replacement file during @save
    uint8_t fileSize[2];                // $1C - $1D    low byte high byte for file size

    bool operator==(const directoryEntry& other) const
    {
        return (uint8_t)file_type == (uint8_t)other.file_type &&
            start.track == other.start.track &&
            start.sector == other.start.sector &&
            std::memcmp(fileName, other.fileName, FILE_NAME_SZ) == 0 &&
            side.track == other.side.track &&
            side.sector == other.side.sector &&
            recordLength == other.recordLength &&
            std::memcmp(unused, other.unused, sizeof(unused)) == 0 &&
            replace.track == other.replace.track &&
            replace.sector == other.replace.sector &&
            std::memcmp(fileSize, other.fileSize, sizeof(fileSize)) == 0;
    }
    bool operator!=(const directoryEntry& other) const
    {
        return !(*this == other);
    }
};
typedef struct directoryEntry* directoryEntryPtr;

struct directorySector {
    uint8_t raw[SECTOR_SIZE];

    trackSector& next() { return *reinterpret_cast<trackSector*>(raw); }
    const trackSector& next() const { return *reinterpret_cast<const trackSector*>(raw); }

    directoryEntry& entry(int i)
    {
        return *reinterpret_cast<directoryEntry*>(raw + 2 + i * DIR_SLOT_STRIDE);
    }
    const directoryEntry& entry(int i) const
    {
        return *reinterpret_cast<const directoryEntry*>(raw + 2 + i * DIR_SLOT_STRIDE);
    }
};
typedef struct directorySector* directorySectorPtr;

static_assert(sizeof(directoryEntry) == DIR_ENTRY_SZ);
static_assert(sizeof(directorySector) == SECTOR_SIZE);

#pragma pack(pop)
