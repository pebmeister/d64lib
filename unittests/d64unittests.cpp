// Written by Paul Baxter
#include <gtest/gtest.h>
#include <string>

#include "d64.h"

#pragma warning(disable:4996)

namespace d64lib_unit_test
{
    static bool saveDiskImages = true;

    static void d64lib_unit_test_method_initialize();
    static void d64lib_unit_test_method_cleanup(d64& disk);

    static void d64lib_unit_test_method_initialize()
    {
    }

    static void d64lib_unit_test_method_cleanup(d64& disk)
    {
        if (saveDiskImages) {
            auto name = std::string(::testing::UnitTest::GetInstance()->current_test_info()->name()) + ".d64";
            disk.save(name.c_str());
        }
    }

    std::string getPaddingString(std::string const& str, int n)
    {
        std::ostringstream oss;
        oss << std::left << std::setw(n) << str;
        return oss.str();
    }

    void allocation_helper(d64* disk)
    {
        std::array<std::array<bool, 21>, TRACKS_40> sectorUsage = {}; // Max sectors per track
        std::array<uint8_t, TRACKS_40> trackFreeUsage = {};

        std::copy_n(disk->SECTORS_PER_TRACK.begin(), TRACKS_40, trackFreeUsage.begin());

        sectorUsage[DIRECTORY_TRACK - 1][DIRECTORY_SECTOR] = true;
        sectorUsage[DIRECTORY_TRACK - 1][BAM_SECTOR] = true;
        trackFreeUsage[DIRECTORY_TRACK - 1] -= 2;
        auto expected_free_sectors = disk->TRACKS == TRACKS_35 ?
            (D64_DISK35_SZ / SECTOR_SIZE) - disk->SECTORS_PER_TRACK[DIRECTORY_TRACK - 1] :
            (D64_DISK40_SZ / SECTOR_SIZE) - disk->SECTORS_PER_TRACK[DIRECTORY_TRACK - 1];

        auto count = disk->getFreeSectorCount();
        std::vector<trackSector> allocations;
        for (auto allocation = 0; allocation < count; ++allocation) {
            int track, sector;

            if (disk->findAndAllocateFreeSector(track, sector)) {
                if (track != DIRECTORY_TRACK) {
                    --expected_free_sectors;
                }
                trackFreeUsage[track - 1] -= 1;
                sectorUsage[track - 1][sector] = true;

                count = disk->getFreeSectorCount();
                EXPECT_EQ(count, expected_free_sectors);
                for (auto track = 1; track <= disk->TRACKS; ++track) {
                    EXPECT_EQ(trackFreeUsage[track - 1], disk->bamtrack(track - 1)->free);
                    for (auto s = 0; s < disk->SECTORS_PER_TRACK[track - 1]; ++s) {
                        EXPECT_NE(disk->bamtrack(track - 1)->test(s), sectorUsage[track - 1][s]);
                    }
                }

                trackSector ts(track, sector);
                auto it = std::find(allocations.begin(), allocations.end(), ts);
                EXPECT_EQ(it, allocations.end());
                allocations.push_back(ts);
            }
        }
    }


    TEST(d64lib_unit_test, sector_allocation_test)
    {
        d64lib_unit_test_method_initialize();

        d64 disk;
        allocation_helper(&disk);

        d64lib_unit_test_method_cleanup(disk);
    }

    TEST(d64lib_unit_test, sector_allocation_40_test)
    {
        d64lib_unit_test_method_initialize();

        d64 disk(diskType::forty_track);
        allocation_helper(&disk);

        d64lib_unit_test_method_cleanup(disk);
    }

    TEST(d64lib_unit_test, create_unit_test)
    {
        d64lib_unit_test_method_initialize();

        d64 disk;
        EXPECT_STREQ(disk.diskname().c_str(), "NEW DISK");
        EXPECT_EQ(disk.TRACKS, TRACKS_35);
        auto dir = disk.directory();
        EXPECT_TRUE(dir.size() == 0);
        EXPECT_TRUE(disk.verifyBAMIntegrity(false, ""));
        EXPECT_EQ(disk.getFreeSectorCount(), (D64_DISK35_SZ / SECTOR_SIZE) -
            disk.SECTORS_PER_TRACK[DIRECTORY_TRACK - 1]);

        d64lib_unit_test_method_cleanup(disk);
    }


    TEST(d64lib_unit_test, create_40_unit_test)
    {
        d64lib_unit_test_method_initialize();

        d64 disk(diskType::forty_track);
        EXPECT_STREQ(disk.diskname().c_str(), "NEW DISK");
        EXPECT_EQ(disk.TRACKS, TRACKS_40);
        auto dir = disk.directory();
        EXPECT_TRUE(dir.size() == 0);
        EXPECT_TRUE(disk.verifyBAMIntegrity(false, ""));
        EXPECT_EQ(disk.getFreeSectorCount(), (D64_DISK40_SZ / SECTOR_SIZE) -
            disk.SECTORS_PER_TRACK[DIRECTORY_TRACK - 1]);

        disk.save("create_40_unit_test.d64");
        d64lib_unit_test_method_cleanup(disk);
    }

    TEST(d64lib_unit_test, addrelfile_test)
    {
        d64lib_unit_test_method_initialize();
        constexpr int RECORD_SIZE = 64;
        constexpr int NUM_RECORDS = 200;

        std::vector<uint8_t> rel_file;
        for (auto record = 0; record < NUM_RECORDS; ++record) {
            std::string rec = getPaddingString(std::string("RECORD ") + std::to_string(record + 1), RECORD_SIZE);
            rel_file.insert(rel_file.end(), rec.begin(), rec.end());
        }

        d64 disk;
        auto added = disk.addFile("RELFILE", d64FileTypes::REL, rel_file, RECORD_SIZE);
        EXPECT_TRUE(added);

        d64lib_unit_test_method_cleanup(disk);
    }

    TEST(d64lib_unit_test, readrelfile_test)
    {
        d64lib_unit_test_method_initialize();
        constexpr int RECORD_SIZE = 64;
        constexpr int NUM_RECORDS = 200;

        std::vector<uint8_t> rel_file;
        for (auto record = 0; record < NUM_RECORDS; ++record) {
            std::string rec = getPaddingString(std::string("RECORD ") + std::to_string(record + 1), RECORD_SIZE);
            rel_file.insert(rel_file.end(), rec.begin(), rec.end());
        }

        d64 disk;
        auto added = disk.addFile("RELFILE", d64FileTypes::REL, rel_file, 64);
        EXPECT_TRUE(added);

        auto readrelfile = disk.readFile("RELFILE");

        EXPECT_TRUE(readrelfile.has_value());
        if (readrelfile.has_value()) {
            EXPECT_EQ(rel_file, readrelfile.value());
        }

        d64lib_unit_test_method_cleanup(disk);
    }

    TEST(d64lib_unit_test, large_file_unit_test)
    {
        d64lib_unit_test_method_initialize();

        const auto bigSize = 90000;

        std::vector<uint8_t> big_file(bigSize);
        for (auto i = 0; i < bigSize; ++i) {
            big_file[i] = i % 256;
        }

        d64 disk;
        auto added = disk.addFile("BIG", d64FileTypes::SEQ, big_file);
        EXPECT_TRUE(added);

        auto readfile = disk.readFile("BIG");
        EXPECT_TRUE(readfile.has_value());
        if (readfile.has_value()) {
            for (auto i = 0; i < bigSize; ++i) {
                EXPECT_TRUE(readfile.value()[i] == big_file[i]);
            }
        }
        d64lib_unit_test_method_cleanup(disk);
    }

    TEST(d64lib_unit_test, add_file_unit_test)
    {
        d64lib_unit_test_method_initialize();

        std::vector<uint8_t> prog = {
0x01, 0x08, 0x0f, 0x08, 0x0a, 0x00, 0x99, 0x20, 0x22, 0x48, 0x45, 0x4c, 0x4c, 0x4f, 0x22, 0x00,
0x1b, 0x08, 0x14, 0x00, 0x81, 0x4b, 0xb2, 0x31, 0xa4, 0x31, 0x30, 0x00, 0x27, 0x08, 0x1e, 0x00,
0x81, 0x4c, 0xb2, 0x4b, 0xa4, 0x31, 0x31, 0x00, 0x31, 0x08, 0x28, 0x00, 0x99, 0x20, 0x4b, 0x2c,
0x4c, 0x00, 0x39, 0x08, 0x32, 0x00, 0x82, 0x3a, 0x82, 0x00, 0x3f, 0x08, 0x3c, 0x00, 0x80, 0x00,
0x00, 0x00
        };

        d64 disk;
        for (auto file = 1; disk.getFreeSectorCount() > 5; ++file) {
            auto numpart = std::to_string(file);

            std::string filename = "FILE";
            filename += numpart;
            auto added = disk.addFile(filename, d64FileTypes::PRG, prog);
            EXPECT_TRUE(added);
            auto dir = disk.directory();
            EXPECT_TRUE(dir.size() == file);

            auto readfile = disk.readFile(filename);
            EXPECT_TRUE(readfile.has_value());
            if (readfile.has_value()) {
                EXPECT_TRUE(readfile.value() == prog);
            }
        }
        d64lib_unit_test_method_cleanup(disk);
    }

    TEST(d64lib_unit_test, extract_file_unit_test)
    {
        d64lib_unit_test_method_initialize();

        std::vector<uint8_t> prog = {
0x01, 0x08, 0x15, 0x08, 0x0a, 0x00, 0x99, 0x20, 0x22, 0x48, 0x45, 0x4c, 0x4c, 0x4f, 0x20,
0x57, 0x4f, 0x52, 0x4c, 0x44, 0x22, 0x00, 0x1b, 0x08, 0x14, 0x00, 0x80, 0x00, 0x00, 0x00
        };

        d64 disk;
        std::vector<std::string> files;

        for (auto file = 1; disk.getFreeSectorCount() > 5; ++file) {
            auto numpart = std::to_string(file);

            std::string filename = "FILE";
            filename += numpart;
            auto added = disk.addFile(filename, d64FileTypes::PRG, prog);
            EXPECT_TRUE(added);
            auto dir = disk.directory();
            EXPECT_TRUE(dir.size() == file);
            files.push_back(filename);
        }

        for (auto& filename : files) {
            auto extracted = disk.extractFile(filename);
            EXPECT_TRUE(extracted);
            if (extracted) {
                std::remove((filename + ".prg").c_str());
            }
        }
        d64lib_unit_test_method_cleanup(disk);
    }


    // Stub test functions for all public methods in the d64 class

    TEST(d64lib_unit_test, constructor_default_test)
    {
        d64lib_unit_test_method_initialize();

        d64 disk;
        // Add assertions here

        d64lib_unit_test_method_cleanup(disk);
    }

    TEST(d64lib_unit_test, constructor_with_type_test)
    {
        d64lib_unit_test_method_initialize();

        d64 disk(diskType::forty_track);
        // Add assertions here

        d64lib_unit_test_method_cleanup(disk);
    }

    TEST(d64lib_unit_test, constructor_with_filename_test)
    {
        d64lib_unit_test_method_initialize();

        EXPECT_ANY_THROW(d64 disk("testingthediskyes.d64"));

        // d64lib_unit_test_method_cleanup(disk);
    }

    TEST(d64lib_unit_test, formatDisk_test)
    {
        d64lib_unit_test_method_initialize();

        d64 disk;
        disk.formatDisk("NEW DISK");
        // Add assertions here

        d64lib_unit_test_method_cleanup(disk);
    }

    TEST(d64lib_unit_test, rename_disk_test)
    {
        d64lib_unit_test_method_initialize();

        d64 disk;
        bool result = disk.rename_disk("TEST DISK");
        // Add assertions here

        d64lib_unit_test_method_cleanup(disk);
    }

    TEST(d64lib_unit_test, diskname_test)
    {
        d64lib_unit_test_method_initialize();

        d64 disk;
        std::string name = disk.diskname();
        // Add assertions here

        d64lib_unit_test_method_cleanup(disk);
    }

    TEST(d64lib_unit_test, findFile_test)
    {
        d64lib_unit_test_method_initialize();

        d64 disk;
        auto fileEntry = disk.findFile("FILENAME");
        // Add assertions here

        d64lib_unit_test_method_cleanup(disk);
    }

    TEST(d64lib_unit_test, addFile_test)
    {
        d64lib_unit_test_method_initialize();

        d64 disk;
        std::vector<uint8_t> fileData = { 0x01, 0x02, 0x03 };
        bool result = disk.addFile("FILENAME", d64FileTypes::PRG, fileData);
        // Add assertions here

        d64lib_unit_test_method_cleanup(disk);
    }

    TEST(d64lib_unit_test, removeFile_test)
    {
        d64lib_unit_test_method_initialize();

        d64 disk;
        bool result = disk.removeFile("FILENAME");
        // Add assertions here

        d64lib_unit_test_method_cleanup(disk);
    }

    TEST(d64lib_unit_test, renameFile_test)
    {
        d64lib_unit_test_method_initialize();

        d64 disk;
        EXPECT_ANY_THROW(bool result = disk.renameFile("OLDNAME", "NEWNAME"));
        // Add assertions here

        d64lib_unit_test_method_cleanup(disk);
    }

    TEST(d64lib_unit_test, extractFile_test)
    {
        d64lib_unit_test_method_initialize();

        d64 disk;


        EXPECT_ANY_THROW(bool result = disk.extractFile("FILENAME"));
        // Add assertions here

        d64lib_unit_test_method_cleanup(disk);
    }

    TEST(d64lib_unit_test, save_test)
    {
        d64lib_unit_test_method_initialize();

        d64 disk;
        bool result = disk.save("test.d64");
        // Add assertions here

        d64lib_unit_test_method_cleanup(disk);
    }

    TEST(d64lib_unit_test, load_test)
    {
        d64lib_unit_test_method_initialize();

        d64 disk;
        bool result = disk.load("test.d64");
        // Add assertions here

        d64lib_unit_test_method_cleanup(disk);
    }

    TEST(d64lib_unit_test, calcOffset_test)
    {
        d64lib_unit_test_method_initialize();

        d64 disk;
        int offset = disk.calcOffset(1, 0);
        // Add assertions here

        d64lib_unit_test_method_cleanup(disk);
    }

    TEST(d64lib_unit_test, writeByte_test)
    {
        d64lib_unit_test_method_initialize();

        d64 disk;
        bool result = disk.writeByte(1, 0, 0, 0xAA);
        // Add assertions here

        d64lib_unit_test_method_cleanup(disk);
    }

    TEST(d64lib_unit_test, writeSector_test)
    {
        d64lib_unit_test_method_initialize();

        d64 disk;
        std::vector<uint8_t> bytes = { 0xAA, 0xBB, 0xCC };
        EXPECT_ANY_THROW(bool result = disk.writeSector(1, 0, bytes));
        // Add assertions here

        d64lib_unit_test_method_cleanup(disk);
    }

    TEST(d64lib_unit_test, readByte_test)
    {
        d64lib_unit_test_method_initialize();

        d64 disk;
        auto byte = disk.readByte(1, 0, 0);
        // Add assertions here

        d64lib_unit_test_method_cleanup(disk);
    }

    TEST(d64lib_unit_test, readSector_test)
    {
        d64lib_unit_test_method_initialize();

        d64 disk;
        auto sector = disk.readSector(1, 0);
        // Add assertions here

        d64lib_unit_test_method_cleanup(disk);
    }

    TEST(d64lib_unit_test, freeSector_test)
    {
        d64lib_unit_test_method_initialize();

        d64 disk;
        bool result = disk.freeSector(1, 0);
        // Add assertions here

        d64lib_unit_test_method_cleanup(disk);
    }

    TEST(d64lib_unit_test, allocateSector_test)
    {
        d64lib_unit_test_method_initialize();

        d64 disk;
        bool result = disk.allocateSector(1, 0);
        // Add assertions here

        d64lib_unit_test_method_cleanup(disk);
    }

    TEST(d64lib_unit_test, findAndAllocateFreeSector_test)
    {
        d64lib_unit_test_method_initialize();

        d64 disk;
        int track, sector;
        bool result = disk.findAndAllocateFreeSector(track, sector);
        // Add assertions here

        d64lib_unit_test_method_cleanup(disk);
    }

    TEST(d64lib_unit_test, readFile_test)
    {
        d64lib_unit_test_method_initialize();

        d64 disk;
        EXPECT_ANY_THROW(auto fileData = disk.readFile("FILENAME"));
        // Add assertions here

        d64lib_unit_test_method_cleanup(disk);
    }

    TEST(d64lib_unit_test, getFreeSectorCount_test)
    {
        d64lib_unit_test_method_initialize();

        d64 disk;
        uint16_t freeSectors = disk.getFreeSectorCount();
        // Add assertions here

        d64lib_unit_test_method_cleanup(disk);
    }

    TEST(d64lib_unit_test, compactDirectory_test)
    {
        d64lib_unit_test_method_initialize();

        d64 disk;
        bool result = disk.compactDirectory();
        // Add assertions here

        d64lib_unit_test_method_cleanup(disk);
    }

    TEST(d64lib_unit_test, verifyBAMIntegrity_test)
    {
        d64lib_unit_test_method_initialize();

        d64 disk;
        bool result = disk.verifyBAMIntegrity(false, "");
        // Add assertions here

        d64lib_unit_test_method_cleanup(disk);
    }

    TEST(d64lib_unit_test, reorderDirectory_compare_test)
    {
        d64lib_unit_test_method_initialize();

        d64 disk;
        bool result = disk.reorderDirectory([](const directoryEntry& a, const directoryEntry& b)
            {
                return std::string(a.file_name) < std::string(b.file_name);
            });
        // Add assertions here

        d64lib_unit_test_method_cleanup(disk);
    }

    TEST(d64lib_unit_test, reorderDirectory_vector_test)
    {
        d64lib_unit_test_method_initialize();

        d64 disk;
        std::vector<directoryEntry> files;
        bool result = disk.reorderDirectory(files);
        // Add assertions here

        d64lib_unit_test_method_cleanup(disk);
    }

    TEST(d64lib_unit_test, reorderDirectory_fileOrder_test)
    {
        d64lib_unit_test_method_initialize();

        d64 disk;
        std::vector<std::string> fileOrder;
        bool result = disk.reorderDirectory(fileOrder);
        // Add assertions here

        d64lib_unit_test_method_cleanup(disk);
    }

    TEST(d64lib_unit_test, movefileFirst_test)
    {
        d64lib_unit_test_method_initialize();

        d64 disk;
        bool result = disk.movefileFirst("FILENAME");
        // Add assertions here

        d64lib_unit_test_method_cleanup(disk);
    }

    TEST(d64lib_unit_test, lockfile_test)
    {
        d64lib_unit_test_method_initialize();

        d64 disk;
        EXPECT_ANY_THROW (bool result = disk.lockfile("FILENAME", true));
        // Add assertions here

        d64lib_unit_test_method_cleanup(disk);
    }

    TEST(d64lib_unit_test, directory_test)
    {
        d64lib_unit_test_method_initialize();

        d64 disk;
        auto dir = disk.directory();
        // Add assertions here

        d64lib_unit_test_method_cleanup(disk);
    }

    TEST(d64lib_unit_test, Trim_test)
    {
        d64lib_unit_test_method_initialize();

        const char filename[FILE_NAME_SZ] = "FILENAME";
        std::string trimmed = d64::Trim(filename);
        // Add assertions here

        d64 disk;
        d64lib_unit_test_method_cleanup(disk);
    }
}
