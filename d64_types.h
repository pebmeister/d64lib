// Written by Paul Baxter
/**
 * @file d64_types.h
 * @author Paul Baxter
 * @brief Data structures and constants for Commodore 64 D64 disk image manipulation.
 *
 * Provides low-level constants, enumerations, and packed structures mapping directly 
 * to the physical byte layout of a 1541 floppy disk (BAM, directories, side sectors).
 */

#pragma once

#pragma pack(push, 1)

/** @name D64 Disk Geometry Constants */
/**@{*/
inline constexpr int TRACKS_35 = 35;
inline constexpr int TRACKS_40 = 40;
inline constexpr int SECTOR_SIZE = 256;
/**@}*/

/** @name Directory and File Constants */
/**@{*/
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
/**@}*/

/** @name Disk Image Size Constants */
/**@{*/
inline constexpr int D64_DISK35_SZ = 174848;
inline constexpr int D64_DISK40_SZ = 196608;
/**@}*/

/** @name REL File Constants */
/**@{*/
inline constexpr int SIDE_SECTOR_ENTRY_SIZE = 6;
inline constexpr int SIDE_SECTOR_CHAIN_SZ = ((SECTOR_SIZE - 15) / (2));
/**@}*/

/** @name DOS Constants */
/**@{*/
static constexpr uint8_t A0_VALUE = 0xA0;    /**< Padding byte for strings */
static constexpr uint8_t DOS_VERSION = 'A';  /**< Standard DOS version identifier */
static constexpr uint8_t DOS_TYPE = '2';     /**< Standard DOS type identifier */
/**@}*/

/**
 * @enum diskType
 * @brief Represents the physical track capacity of the disk image.
 */
enum diskType {
    thirty_five_track, /**< Standard 1541 35-track disk */
    forty_track        /**< Extended 40-track disk (e.g., Dolphin DOS) */
};

/**
 * @enum d64FileTypes
 * @brief Standard Commodore 64 DOS file types.
 */
enum d64FileTypes : uint8_t {
    DEL = 0, /**< Deleted/Unallocated file */
    SEQ = 1, /**< Sequential file */
    PRG = 2, /**< Program file */
    USR = 3, /**< User file */
    REL = 4  /**< Relative file */
};

/**
 * @struct trackSector
 * @brief Represents a physical pointer to a specific track and sector.
 */
struct trackSector {
public:
    uint8_t track;  /**< Track number (1-based) */
    uint8_t sector; /**< Sector number (0-based) */

    /**
     * @brief Equality operator for trackSector.
     * @param other The trackSector to compare against.
     * @return true if both track and sector match.
     */
    bool operator ==(const trackSector& other) const
    {
        return track == other.track && sector == other.sector;
    }

    /** @brief Constructs a trackSector with integer coordinates. */
    trackSector(int track, int sector) : track(track), sector(sector) {};
    
    /** @brief Constructs a trackSector with 8-bit coordinates. */
    trackSector(uint8_t track, uint8_t sector) : track(track), sector(sector) {};
};

/**
 * @struct sector
 * @brief Represents a standard 256-byte disk sector.
 */
struct sector {
public:
    trackSector next;                                            /**< Link to the next track/sector in the chain */
    std::array<uint8_t, SECTOR_SIZE - sizeof(trackSector)> data; /**< The raw sector payload (typically 254 bytes) */
};
typedef sector* sectorPtr;

/**
 * @class sideSector
 * @brief Represents a REL file side sector mapping data block pointers.
 */
class sideSector {
public:
    trackSector next;                                 /**< $01 - $02 Next side sector T/S */
    uint8_t block;                                    /**< $02 Side sector block number */
    uint8_t recordsize;                               /**< $03 Record size for the REL file */
    trackSector sideSectors[SIDE_SECTOR_ENTRY_SIZE];  /**< $04 - $0F Pointers to other side sectors */
    trackSector chain[SIDE_SECTOR_CHAIN_SZ];          /**< $10+ Array of T/S pointers to actual data sectors */
};
typedef sideSector* sideSectorPtr;

/**
 * @class c64FileType
 * @brief Bitfield representing the file type and status flags in a directory entry.
 */
class c64FileType {
public:
    d64FileTypes type : 4;  /**< Base DOS file type (DEL, SEQ, PRG, USR, REL) */
    uint8_t unused : 1;     /**< Unused/Reserved bit */
    uint8_t replace : 1;    /**< Save-with-replace flag */
    uint8_t locked : 1;     /**< File locked (read-only) flag */
    uint8_t closed : 1;     /**< File properly closed flag */

public:
    /** @brief Default constructor. Initializes as a DEL (deleted) file. */
    c64FileType() 
        : type(d64FileTypes::DEL), unused(0), replace(0), locked(0), closed(0) {}

    /**
     * @brief Constructs a file type with specific closed/locked flags.
     * @param a closed flag (true if properly closed).
     * @param l locked flag (true if read-only).
     * @param t The C64 DOS file type.
     */
    c64FileType(bool a, bool l, d64FileTypes t) 
        : type(t), unused(0), replace(0), locked(l ? 1 : 0), closed(a ? 1 : 0) {}

    /**
     * @brief Constructs a file type automatically marked as closed.
     * @param t The C64 DOS file type.
     */
    c64FileType(d64FileTypes t) 
        : type(t), unused(0), replace(0), locked(0), closed(1) {}

    /**
     * @brief Constructs a file type by parsing a raw 8-bit directory entry byte.
     * @param value The raw byte from the directory sector.
     */
    c64FileType(uint8_t value) 
        : type(static_cast<d64FileTypes>(value & 0x0F)),
          unused((value & 0x10) >> 4),
          replace((value & 0x20) >> 5),
          locked((value & 0x40) >> 6),
          closed((value & 0x80) >> 7)
    {
    }

    /** @brief Converts the bitfield back to a single raw byte. */
    operator uint8_t() const { return (closed << 7) | (locked << 6) | (replace << 5) | (unused << 4) | type; }
    
    /** @brief Implicitly converts to the underlying d64FileTypes enum. */
    operator d64FileTypes() const { return type; }
};

/**
 * @struct bamTrackEntry
 * @brief Represents the Block Availability Map (BAM) allocation data for a single track.
 */
struct bamTrackEntry {

public:
    uint8_t free;                  /**< Number of free sectors on this track */
    std::array <uint8_t, 3> bytes; /**< Bitset representing sector availability */

    /**
     * @brief Tests the BAM bit for a specific sector.
     * @param sector The sector index to test (0-23 depending on track).
     * @return true if the BAM bit is set (typically meaning free), false otherwise.
     */
    bool test(int sector)
    {
        if (sector < 0 || sector >= 24) return false;
        auto byte = sector / 8;
        auto bit = sector % 8;

        std::bitset<8> bits(bytes[byte]);
        return bits.test(bit);
    }

    /**
     * @brief Marks a sector as free in the BAM (sets the corresponding bit).
     * @param sector The sector index to mark as free.
     */
    inline void set(int sector)
    {
        if (sector < 0 || sector >= 24) return;
        auto byte = sector / 8;
        auto bit = sector % 8;

        std::bitset<8> bits(bytes[byte]);
        bits.set(bit);
        bytes[byte] = static_cast<uint8_t>(bits.to_ulong());
    }

    /**
     * @brief Marks a sector as used/allocated in the BAM (resets the corresponding bit).
     * @param sector The sector index to mark as used.
     */
    inline void reset(int sector)
    {
        if (sector < 0 || sector >= 24) return;
        auto byte = sector / 8;
        auto bit = sector % 8;

        std::bitset<8> bits(bytes[byte]);
        bits.reset(bit);
        bytes[byte] = static_cast<uint8_t>(bits.to_ulong());
    }

    /**
     * @brief Clears the BAM byte array for this track, marking all sectors as in use.
     */
    inline void clear()
    {
        bytes[0] = 0;
        bytes[1] = 0;
        bytes[2] = 0;
    }
};

/**
 * @struct bam
 * @brief Represents the complete Track 18, Sector 0 Block Availability Map.
 * 
 * Supports both standard 35-track disks and 40-track extensions (DOLPHIN DOS).
 */
struct bam {
    trackSector dirStart;                   /**< $00 - $01 Track/Sector of first directory block */
    uint8_t dosVersion;                     /**< $02 DOS version (usually 'A') */
    uint8_t unused;                         /**< $03 Unused (should be 0) */
    bamTrackEntry bamTrack[TRACKS_35];      /**< $04 - $8F BAM entries for tracks 1-35 */
    char diskName[DISK_NAME_SZ];            /**< $90 - $9F Disk name padded with $A0 */
    uint8_t a0[2];                          /**< $A0 - $A1 Contains $A0 */
    uint8_t diskId[2];                      /**< $A2 - $A3 Disk ID */
    uint8_t unused2;                        /**< $A4 Contains $A0 */
    char dos_type[2];                       /**< $A5 - $A6 DOS type string (e.g., "2A") */
    uint8_t unused3[UNUSED3_SZ];            /**< $A7 - $AB Padding bytes ($00) */
    uint8_t unused4[UNUSED4_SZ];            /**< $AC - $FF Padding bytes ($00) (DOLPHIN DOS uses this for Tracks 36-40) */
};
typedef struct bam* bamPtr;

/**
 * @struct directoryEntry
 * @brief Represents a single file entry within a directory sector.
 */
struct directoryEntry {
    c64FileType file_type;              /**< $00 File type and status flags */
    trackSector start;                  /**< $01 - $02 First track and sector of the file data */
    char fileName[FILE_NAME_SZ];        /**< $03 - $12 File name string padded with $A0 */
    trackSector side;                   /**< $13 - $14 First side track/sector (REL files only) */
    uint8_t recordLength;               /**< $15 Record length (REL files only) */
    uint8_t unused[4];                  /**< $16 - $19 Unused space */
    trackSector replace;                /**< $1A - $1B Track/Sector of replacement file during \@save */
    uint8_t fileSize[2];                /**< $1C - $1D File size in sectors (low byte, high byte) */

    /**
     * @brief Checks if two directory entries are completely identical.
     * @param other The entry to compare with.
     * @return true if all bytes match.
     */
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
    
    /** @brief Inequality operator. */
    bool operator!=(const directoryEntry& other) const
    {
        return !(*this == other);
    }
};
typedef struct directoryEntry* directoryEntryPtr;

/**
 * @struct directorySector
 * @brief Represents a full 256-byte directory block containing multiple entries.
 */
struct directorySector {
    uint8_t raw[SECTOR_SIZE]; /**< The raw block buffer */

    /** @brief Retrieves the T/S pointer to the next directory sector in the chain. */
    trackSector& next() { return *reinterpret_cast<trackSector*>(raw); }
    const trackSector& next() const { return *reinterpret_cast<const trackSector*>(raw); }

    /**
     * @brief Retrieves a specific directory entry from the sector.
     * @param i The index of the entry slot (0-7).
     * @return A reference to the directory entry at the specified slot.
     */
    directoryEntry& entry(int i)
    {
        return *reinterpret_cast<directoryEntry*>(raw + 2 + i * DIR_SLOT_STRIDE);
    }
    
    /** @brief Retrieves a specific directory entry from the sector (const). */
    const directoryEntry& entry(int i) const
    {
        return *reinterpret_cast<const directoryEntry*>(raw + 2 + i * DIR_SLOT_STRIDE);
    }
};
typedef struct directorySector* directorySectorPtr;

// Compile-time checks ensuring proper packing and byte alignment
static_assert(sizeof(directoryEntry) == DIR_ENTRY_SZ);
static_assert(sizeof(directorySector) == SECTOR_SIZE);

#pragma pack(pop)
