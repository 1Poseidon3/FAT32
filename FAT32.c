/*****************************************************
 *                                                   *
 *   Author: Liam Craig                              *
 *   Description: Lab 3: FAT32 File System Emulation *
 *   Creation date: 4/18/2022                        *
 *                                                   *
*****************************************************/

#pragma pack(1);
#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Struct to hold the data for a Partition Table Entry
struct PartitionTableEntry
{
    unsigned char bootFlag;
    unsigned char CHSBegin[3];
    unsigned char typeCode;
    unsigned char CHSEnd[3];
    unsigned int LBABegin;
    unsigned int LBALength;
};

typedef struct PartitionTableEntry PartitionTableEntry;

// Struct to hold the Master Boot Record
struct MBRStruct
{
    unsigned char bootCode[446];
    PartitionTableEntry part1;
    PartitionTableEntry part2;
    PartitionTableEntry part3;
    PartitionTableEntry part4;
    unsigned short flag;
} MBR;

typedef struct MBRStruct MBRStruct;

// Struct to hold the BIOS Parameter Block
struct BPBStruct
{
    unsigned char BS_jmpBoot[3];        // Jump instruction to boot code
    unsigned char BS_OEMName[8];        // 8-Character string (not null terminated)
    unsigned short BPB_BytsPerSec;      // Had BETTER be 512!
    unsigned char BPB_SecPerClus;       // How many sectors make up a cluster?
    unsigned short BPB_RsvdSecCnt;      // # of reserved sectors at the beginning (including the BPB)?
    unsigned char BPB_NumFATs;          // How many copies of the FAT are there? (had better be 2)
    unsigned short BPB_RootEntCnt;      // ZERO for FAT32
    unsigned short BPB_TotSec16;        // ZERO for FAT32
    unsigned char BPB_Media;            // SHOULD be 0xF8 for "fixed", but isn't critical for us
    unsigned short BPB_FATSz16;         // ZERO for FAT32
    unsigned short BPB_SecPerTrk;       // Don't care; we're using LBA; no CHS
    unsigned short BPB_NumHeads;        // Don't care; we're using LBA; no CHS
    unsigned int BPB_HiddSec;           // Don't care ?
    unsigned int BPB_TotSec32;          // Total Number of Sectors on the volume
    unsigned int BPB_FATSz32;           // How many sectors long is ONE Copy of the FAT?
    unsigned short BPB_Flags;           // Flags (see document)
    unsigned short BPB_FSVer;           // Version of the File System
    unsigned int BPB_RootClus;          // Cluster number where the root directory starts (should be 2)
    unsigned short BPB_FSInfo;          // What sector is the FSINFO struct located in? Usually 1
    unsigned short BPB_BkBootSec;       // REALLY should be 6 – (sector # of the boot record backup)
    unsigned char BPB_Reserved[12];     // Should be all zeroes -- reserved for future use
    unsigned char BS_DrvNum;            // Drive number for int 13 access (ignore)
    unsigned char BS_Reserved1;         // Reserved (should be 0)
    unsigned char BS_BootSig;           // Boot Signature (must be 0x29)
    unsigned char BS_VolID[4];          // Volume ID
    unsigned char BS_VolLab[11];        // Volume Label
    unsigned char BS_FilSysType[8];     // Must be "FAT32 "
    unsigned char unused[420];          // Not used
    unsigned char signature[2];         // MUST BE 0x55 AA
} BPB;

typedef struct BPBStruct BPBStruct;

// Structure to hold the data for an 8.3 directory entry
struct EPTStruct {
    unsigned char DIR_Name[11];
    unsigned char DIR_Attr;
    unsigned char DIR_NTRes;
    unsigned char DIR_CrtTimeTenth;
    unsigned short DIR_CrtTime;
    unsigned short DIR_CrtDate;
    unsigned short DIR_LstAccDate;
    unsigned short DIR_FstClusHI;
    unsigned short DIR_WrtTime;
    unsigned short DIR_WrtDate;
    unsigned short DIR_FstClusLO;
    unsigned int DIR_FileSize;
} EPT;

// Structure to hold the data for a Long File Name directory entry
struct LFNStruct {
    unsigned char LDIR_Ord;
    unsigned char LDIR_Name1[10];
    unsigned char LDIR_Attr;
    unsigned char LDIR_Type;
    unsigned char LDIR_Chksum;
    unsigned char LDIR_Name2[12];
    unsigned short LDIR_FstClusLO;
    unsigned char LDIR_Name3[4];
} LFN;

typedef struct EPTStruct EPTStruct;
typedef struct LFNStruct LFNStruct;

// Union to hold either an LFN or an 8.3 entry
// so they can be stored together.
union DirStruct {
    EPTStruct ept;
    LFNStruct lfn;
} DIR;

// A hacky solution to a problem that was
// probably a lot simpler than I thought it was
struct EaseofExtractStruct {
    unsigned char name[257];
    int index;
};

typedef struct EaseofExtractStruct EaseofExtractStruct;

// Another hacky solution to a problem that was
// probably a lot simpler than I thought it was
struct FATLLStruct {
    unsigned int cluster;
} FATLL;

typedef union DirStruct DirStruct;
typedef struct FATLLStruct FATLLStruct;

// This is just a simple little function that
// takes in the Logical Block Address of the area
// in the file you want to seek to and multiplies it
// by 512 (the size of a sector) to get you the byte
// offset you need. I figured it was easier on me to
// do this than to calcuslate everything by 512 bytes
// every time I needed to go somewhere in the file.
long LBAtoOffset(int lba) {
    return lba * 512;
} 

// Besides the above function, everything was written in the Main method.
// Why? Because I'm bad at C and bad at making function that can take
// in pointers as arguments because I had professor Xu for non-linear
// data structures and he didn't teach pointers very well and now as
// a result I hate C and C++. That and it also "Just works!" (tm).
void main(int argc, char *argv[]) {
    // If no file name was provided as a command line parameter when running the file,
    // let the user know and then quit.
    if (argc == 1) {
        printf("No file name provided.\n");
        exit(-1);
    }
    FILE *fp = fopen(argv[1], "r+");
    // If a file the the speicifed name could not be found, let the user know and then quit.
    if (fp == NULL) {
        printf("No file with the specified name could be found.\n");
        exit(-2);
    }
    // We are assuming that whoever is using this program is only attempting to open
    // a file containing a valid FAT32 partition.
    // Generic array to hold one sector of data
    unsigned char sector[512];
    int directoryListings = 0;
    MBRStruct mbr;
    BPBStruct bpb;
    EPTStruct rootDir;
    FATLLStruct fatll;
    // Seek to the MBR and load it into the struct
    fseek(fp, LBAtoOffset(0), SEEK_SET);
    fread(&mbr, 512, 1, fp);
    // Seek to the BPB and load it into the struct
    fseek(fp, LBAtoOffset(mbr.part1.LBABegin), SEEK_SET);
    fread(&bpb, 512, 1, fp);
    // Generic array to hold one cluster of data (no matter how big a cluster actually is)
    unsigned char cluster [bpb.BPB_SecPerClus * 512];
    // Create an array that can house the entirety of the FAT
    unsigned int *fat = (unsigned int *)malloc(bpb.BPB_FATSz32 * 128 * sizeof(unsigned int));
    fseek(fp, LBAtoOffset(mbr.part1.LBABegin + bpb.BPB_RsvdSecCnt), SEEK_SET); // FAT
    fread(sector, 512, 1, fp);
    // Load the FAT into the array in 4 byte chunks
    for (int i = 0; i < bpb.BPB_FATSz32; i++) {
        for (int j = 0; j < 128; j++) {
            unsigned char temp[4];
            fseek(fp, LBAtoOffset(mbr.part1.LBABegin + bpb.BPB_RsvdSecCnt + i) + (j * 4), SEEK_SET); // FAT
            fread(&fatll, 4, 1, fp);
            fat[j + (128 * i)] = fatll.cluster;
        }
    }
    // Determine how many clusters make up the root directory
    // clusters4root is 1 because we are guaranteed at least 1 cluster for the directory
    // currentSector starts at 2 because that's the sector in which the root directory starts
    int clusters4root = 1, currentSector = 2, clustersStart[32] = { 0 };
    for (int i = 1; i < 32; i++) {
        // Check to see where the FAT tells us the next
        // cluster of the root directory is
        // If it gives us 0FFFFFFF (268435455), which is EoF
        // Then we know the root directory has ended at that cluster
        if (fat[currentSector] == 268435455)
            break;
        else {
            clusters4root++;
            currentSector = fat[currentSector];
            clustersStart[i] = currentSector - 2;
        }
    }
    // Create an array the size of the root directory and put all of the entries in it
    DirStruct *rootListing = malloc(bpb.BPB_FATSz32 * 128 * clusters4root * sizeof(DirStruct));
    EaseofExtractStruct *eoes = malloc(bpb.BPB_FATSz32 * 128 * clusters4root * sizeof(DirStruct));
    // Load the root directory into the array, 32 bytes at a time
    for (int i = 0; i < clusters4root; i++) {
        for (int j = 0; j < 128; j++) {
            unsigned char temp;
            // This specific byte tells me whether or not the entry is of the type LFN or 8.3
            fseek(fp, LBAtoOffset(mbr.part1.LBABegin + bpb.BPB_RsvdSecCnt + (bpb.BPB_FATSz32 * bpb.BPB_NumFATs) + (clustersStart[i] * 8)) + (32 * j) + 11, SEEK_SET);
            fread(&temp, 1, 1, fp);
            // Now that we have the attribute byte, seek to the beginning of the row so we are ready to read it into the struct
            fseek(fp, LBAtoOffset(mbr.part1.LBABegin + bpb.BPB_RsvdSecCnt + (bpb.BPB_FATSz32 * bpb.BPB_NumFATs) + (clustersStart[i] * 8)) + (32 * j), SEEK_SET);
            DirStruct dirEntry;
            // 0x0F is the attribute value telling us the entry is of the LFN variety
            if (temp == 0x0F)
                fread(&dirEntry.lfn, 32, 1, fp);
            else
                fread(&dirEntry.ept, 32, 1, fp);
            rootListing[j + (128 * i)] = dirEntry;
            directoryListings++;
        }
    }
    // Now after we have loaded all of this data into easy-to-access arrays, we can prompt the user for input
    while (1 == 1) {
        // These are also SIGNED char arrays (whoops, I think I was going crazy by this point)
        // but, once again, it works! (at least on my machine...)
        char input[512], temp[8] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, temp2[4] = { 0x00, 0x00, 0x00, 0x00 }, temp3[5] = { 0x00, 0x00, 0x00, 0x00, 0x00 };
        printf("\\>");
        fgets(input, 512, stdin);
        if (strcasecmp(strncpy(temp2, input, 3), "DIR") == 0) {
            // Run for as many times as there are directory listings
            for (int i = 0; i < directoryListings; i++) {
                // Go this way if it is an LFN entry
                if (rootListing[i].lfn.LDIR_Attr == 0x0F) {
                    // I just now realized on the last day that this is a
                    // SIGNED char array but hey, it works!
                    // (Probably because of the loop farther down...)
                    char tempName[257];
                    int howManySectors = rootListing[i].lfn.LDIR_Ord - 0x40, howManyCharsIn = 0;
                    // Clear the array
                    for (int j = 0; j < 257; j++) {
                        if (j != 256)
                            tempName[j] = 0x20;
                        else
                            tempName[j] = 0x00;
                    }
                    // Combine all 3 parts of the LFN name
                    for (int j = howManySectors + i - 1; j >= i; j--) {
                        for(int k = 0; k < 10; k++) {
                            if (rootListing[j].lfn.LDIR_Name1[k] == 0x00)
                                continue;
                            tempName[howManyCharsIn] = rootListing[j].lfn.LDIR_Name1[k];
                            howManyCharsIn++;
                        }
                        for (int k = 0; k < 12; k++) {
                            if (rootListing[j].lfn.LDIR_Name2[k] == 0x00)
                                continue;
                            tempName[howManyCharsIn] = rootListing[j].lfn.LDIR_Name2[k];
                            howManyCharsIn++;
                        }
                        for (int k = 0; k < 4; k++) {
                            if (rootListing[j].lfn.LDIR_Name3[k] == 0x00)
                                continue;
                            tempName[howManyCharsIn] = rootListing[j].lfn.LDIR_Name3[k];
                            howManyCharsIn++;
                        }
                    }
                    // Make the remainder of the array 0x00 because I couldn't
                    // be bothered to try and mess with malloc to make a
                    // dynamic array at this point in the project.
                    for (int j = howManyCharsIn; j < 257; j++) {
                        tempName[j] = 0x00;
                    }
                    // Make any (for some reason) negative values into null
                    for (int j = 0; j < 257; j++) {
                        if (tempName[j] < 0x00)
                            tempName[j] = 0x00;
                    }
                    if (tempName[0] != 0xE5 && tempName[0] != 0x00) {
                        // Because of the System Volume Information
                        if (i == 1)
                            printf("%s\n", tempName);
                        else
                            printf("%s ", tempName);
                    }
                    //i += howManySectors; This would skip over displaying the 8.3 version of the file name
                // Go this way if it is an 8.3 entry
                } else  if (rootListing[i].ept.DIR_Attr != 0x02) {
                    if(rootListing[i].ept.DIR_Attr == 0x08)
                        printf("--- %s ---\n", rootListing[i].ept.DIR_Name);
                    else {
                        unsigned char tempName[13] = { 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x00};
                        int dotPlaced = 0, difference = 0, noFileExtension = 1, empty = 0;
                        for (int j = 0; j < 12; j++) {
                            if (j < 8 && rootListing[i].ept.DIR_Name[j] > 0x20 && rootListing[i].ept.DIR_Name[j] != 0x22 && rootListing[i].ept.DIR_Name[j] != 0x2A && rootListing[i].ept.DIR_Name[j] != 0x2B && rootListing[i].ept.DIR_Name[j] != 0x2C && rootListing[i].ept.DIR_Name[j] != 0x2E && rootListing[i].ept.DIR_Name[j] != 0x2F && rootListing[i].ept.DIR_Name[j] != 0x3A && rootListing[i].ept.DIR_Name[j] != 0x3B && rootListing[i].ept.DIR_Name[j] != 0x3C && rootListing[i].ept.DIR_Name[j] != 0x3D && rootListing[i].ept.DIR_Name[j] != 0x3E && rootListing[i].ept.DIR_Name[j] != 0x3F && rootListing[i].ept.DIR_Name[j] != 0x5B && rootListing[i].ept.DIR_Name[j] != 0x5C && rootListing[i].ept.DIR_Name[j] != 0x5D && rootListing[i].ept.DIR_Name[j] != 0x7C) {
                                tempName[j] = rootListing[i].ept.DIR_Name[j];
                                empty = 0;
                            } else if ((rootListing[i].ept.DIR_Name[j] == 0x20 || j ==  8) && dotPlaced == 0) {
                                if (rootListing[i].ept.DIR_Attr == 0x00)
                                    empty = 1;
                                for (int k = j; k < 11; k++) {
                                    if (rootListing[i].ept.DIR_Name[k] != 0x20)
                                        noFileExtension = 0;
                                }
                                if (noFileExtension == 0) {
                                    tempName[j] = '.';
                                }
                                difference = 8 - j;
                                j = 8;
                                dotPlaced = 1;
                            } else if (j >= 9) {
                                tempName[j - difference] = rootListing[i].ept.DIR_Name[j - 1];
                            }
                        }
                        if (empty == 0 && tempName[0] != 0xE5) {
                            unsigned int year = rootListing[i].ept.DIR_CrtDate & 0x7F, month = (rootListing[i].ept.DIR_CrtDate >> 7) & 0x0F, date = (rootListing[i].ept.DIR_CrtDate >> 11) & 0x1F, hour = rootListing[i].ept.DIR_CrtTime & 0x1F, minute = (rootListing[i].ept.DIR_CrtTime >> 5) & 0x3F, second = (rootListing[i].ept.DIR_CrtTime >> 11) & 0x1F;
                            printf("%s %i bytes created on %i/%i/%i at %i:%i\n", tempName, rootListing[i].ept.DIR_FileSize, month, date, year + 1980, hour, minute);
                        }
                    }
                }
            }
        } else if (strcasecmp(strncpy(temp, input, 7), "EXTRACT") == 0) {
            // EoES stands for Ease of Extract Struct
            int eoesIndex = 0, didIFindTheFile = 0;
            unsigned char fileNameInput[257];
            // This is basically copied exactly from the DIR command branch.
            // The only difference here is this one runs silently and adds
            // the file entry to my EoES array instead.
            for (int i = 0; i < directoryListings; i++) {
                if (rootListing[i].lfn.LDIR_Attr == 0x0F) {
                    unsigned char tempName[257];
                    int howManySectors = rootListing[i].lfn.LDIR_Ord - 0x40, howManyCharsIn = 0;
                    // Clear the array
                    for (int j = 0; j < 257; j++) {
                        if (j != 256)
                            tempName[j] = 0x20;
                        else
                            tempName[j] = 0x00;
                    }
                    for (int j = howManySectors + i - 1; j >= i; j--) {
                        for(int k = 0; k < 10; k++) {
                            if (rootListing[j].lfn.LDIR_Name1[k] == 0x00)
                                continue;
                            tempName[howManyCharsIn] = rootListing[j].lfn.LDIR_Name1[k];
                            howManyCharsIn++;
                        }
                        for (int k = 0; k < 12; k++) {
                            if (rootListing[j].lfn.LDIR_Name2[k] == 0x00)
                                continue;
                            tempName[howManyCharsIn] = rootListing[j].lfn.LDIR_Name2[k];
                            howManyCharsIn++;
                        }
                        for (int k = 0; k < 4; k++) {
                            if (rootListing[j].lfn.LDIR_Name3[k] == 0x00)
                                continue;
                            tempName[howManyCharsIn] = rootListing[j].lfn.LDIR_Name3[k];
                            howManyCharsIn++;
                        }
                    }
                    for (int j = howManyCharsIn; j < 257; j++) {
                        tempName[j] = 0x00;
                    }
                    for (int j = 0; j < 257; j++) {
                        if (tempName[j] < 0x00)
                            tempName[j] = 0x00;
                    }
                    if (tempName[0] != 0xE5) {
                        eoes[eoesIndex].index = i;
                        for (int j = 0; j < 257; j++) {
                            if (tempName[j] < 0x20 || tempName[j] > 0x7E) {
                                eoes[eoesIndex].name[j] = 0x0A;
                                break;
                            }
                            eoes[eoesIndex].name[j] = tempName[j];
                        }
                        eoesIndex++;
                    }
                    i += howManySectors;
                } else if (rootListing[i].ept.DIR_Attr != 0x02) {
                    if(rootListing[i].ept.DIR_Attr != 0x08) {
                        unsigned char tempName[13] = { 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x00};
                        int dotPlaced = 0, difference = 0, noFileExtension = 1, empty = 0;
                        for (int j = 0; j < 12; j++) {
                            if (j < 8 && rootListing[i].ept.DIR_Name[j] > 0x20 && rootListing[i].ept.DIR_Name[j] != 0x22 && rootListing[i].ept.DIR_Name[j] != 0x2A && rootListing[i].ept.DIR_Name[j] != 0x2B && rootListing[i].ept.DIR_Name[j] != 0x2C && rootListing[i].ept.DIR_Name[j] != 0x2E && rootListing[i].ept.DIR_Name[j] != 0x2F && rootListing[i].ept.DIR_Name[j] != 0x3A && rootListing[i].ept.DIR_Name[j] != 0x3B && rootListing[i].ept.DIR_Name[j] != 0x3C && rootListing[i].ept.DIR_Name[j] != 0x3D && rootListing[i].ept.DIR_Name[j] != 0x3E && rootListing[i].ept.DIR_Name[j] != 0x3F && rootListing[i].ept.DIR_Name[j] != 0x5B && rootListing[i].ept.DIR_Name[j] != 0x5C && rootListing[i].ept.DIR_Name[j] != 0x5D && rootListing[i].ept.DIR_Name[j] != 0x7C) {
                                tempName[j] = rootListing[i].ept.DIR_Name[j];
                                empty = 0;
                            } else if ((rootListing[i].ept.DIR_Name[j] == 0x20 || j ==  8) && dotPlaced == 0) {
                                if (rootListing[i].ept.DIR_Attr == 0x00)
                                    empty = 1;
                                for (int k = j; k < 11; k++) {
                                    if (rootListing[i].ept.DIR_Name[k] != 0x20)
                                        noFileExtension = 0;
                                }
                                if (noFileExtension == 0) {
                                    tempName[j] = '.';
                                }
                                difference = 8 - j;
                                j = 8;
                                dotPlaced = 1;
                            } else if (j >= 9) {
                                tempName[j - difference] = rootListing[i].ept.DIR_Name[j - 1];
                            }
                        }
                        if (empty == 0 && tempName[0] != 0xE5) {
                            eoes[eoesIndex].index = i;
                            for (int j = 0; j < 257; j++) {
                                if (tempName[j] <= 0x20 || tempName[j] > 0x7E) {
                                    eoes[eoesIndex].name[j] = 0x0A;
                                    break;
                                }
                                eoes[eoesIndex].name[j] = tempName[j];
                            }
                            eoesIndex++;
                        }
                    }
                }
                else {
                    printf("File is hidden!\n");
                    break;
                }
            }
            // Everything after index 8 of the input string (the space after "extract")
            // is considered what the user is looking for (need to include the file extension!)
            strncpy(fileNameInput, &input[8], 256);
            // This is where the file will go if/when we find it.
            DirStruct foundFile;
            for (int i = 0; i < directoryListings; i++) {
                if (strcasecmp(eoes[i].name, fileNameInput) == 0) {
                    didIFindTheFile = 1;
                    foundFile = rootListing[eoes[i].index];
                    int testtest = foundFile.lfn.LDIR_Ord & 0x0F;
                    if (foundFile.lfn.LDIR_Attr == 0x0F)
                        foundFile = rootListing[eoes[i].index + ((int)foundFile.lfn.LDIR_Ord & 0x0F)];
                    //unsigned int clusterLocations[512]; Relic of me before I used malloc in this area of the code
                    //unsigned int *clusterLocations = (unsigned int *)malloc(((foundFile.ept.DIR_FileSize / (bpb.BPB_SecPerClus * 512)) + 1) * sizeof(unsigned int)); This should work the same as the code below, it was just split up for debugging purposes and it works and I'm too scared to change it back in case it doesn't work...
                    int clusterCount = ((foundFile.ept.DIR_FileSize / (bpb.BPB_SecPerClus * 512)) + 1) * sizeof(unsigned int);
                    unsigned int *clusterLocations = (unsigned int*) malloc(clusterCount* sizeof(unsigned int));
                    // Zero out the array (just in case... This project has made me very paranoid).
                    for (int j = 0; j < (foundFile.ept.DIR_FileSize / (bpb.BPB_SecPerClus * 512)) + 1; j++) {
                        clusterLocations[j] = 0x00;
                    }
                    // Run through the FAT and put every location in an array for later seeking usage.
                    for (int j = 0; j < (foundFile.ept.DIR_FileSize / (bpb.BPB_SecPerClus * 512)) + 1; j++) {
                        if (j == 0)
                            clusterLocations[j] = ((foundFile.ept.DIR_FstClusHI << 8) | foundFile.ept.DIR_FstClusLO);
                        else 
                            clusterLocations[j] = fat[clusterLocations[j - 1]];
                        // End of File code
                        if (clusterLocations[j] >= 0x0FFFFFF8)
                            break;
                    }
                    // Array to hold the name of the file with no New Line character at the end.
                    // Whatever I did is bad and wrong and stupid but I made it work I just had to do this.
                    unsigned char nameWithNoNL[4096];
                    for (int j = 0; j < 4096; j++) {
                        nameWithNoNL[j] = 0x00;
                    }
                    for (int j = 0; j < 512; j++) {
                        // As long as the character in the name isn't a New Line, add it.
                        // If it is a New Line, we know we've hit the end of the name and we're done.
                        if(eoes[i].name[j] != 0x0A)
                            nameWithNoNL[j] = eoes[i].name[j];
                        else
                            break;
                    }
                    // The file to be exported (this creates it).
                    // "w" is write mode and will delete the file with the same name if it already exists and remake it.
                    // "w+" will overwrite the data of the file instead of deleting and recreating it.
                    FILE *fpEx = fopen(nameWithNoNL, "w");
                    // Create and array that will hold the data for the cluster to be written to the file and zero it out.
                    unsigned char cluster2File[bpb.BPB_SecPerClus * 512];
                    for (int j = 0; j < bpb.BPB_SecPerClus * 512; j++) {
                        cluster2File[j] = 0x00;
                    }
                    int clustersThatMakeUpTheFile = foundFile.ept.DIR_FileSize / (bpb.BPB_SecPerClus * 512) + 1, remainderOfLastCluster = foundFile.ept.DIR_FileSize % (bpb.BPB_SecPerClus * 512);
                    // This loop just writes all of the data to the file, one cluster at a time.
                    for (int j = 0; j <= clustersThatMakeUpTheFile; j++) {
                        if (j == clustersThatMakeUpTheFile) {
                            unsigned int testtestss = (clusterLocations[j] - 2);
                            fseek(fp, LBAtoOffset(mbr.part1.LBABegin + bpb.BPB_RsvdSecCnt) + LBAtoOffset(bpb.BPB_FATSz32 * bpb.BPB_NumFATs) + LBAtoOffset((clusterLocations[j] - 2) * 8), SEEK_SET);
                            fread(&cluster2File, remainderOfLastCluster, 1, fp);
                            fwrite(cluster2File, remainderOfLastCluster, 1, fpEx);
                            break;
                        }
                        unsigned int testtedstss = (clusterLocations[j] - 2);
                        fseek(fp, LBAtoOffset(mbr.part1.LBABegin + bpb.BPB_RsvdSecCnt) + LBAtoOffset(bpb.BPB_FATSz32 * bpb.BPB_NumFATs) + LBAtoOffset((clusterLocations[j] - 2) * 8), SEEK_SET);
                        fread(&cluster2File, bpb.BPB_SecPerClus * 512, 1, fp);
                        fwrite(cluster2File, bpb.BPB_SecPerClus * 512, 1, fpEx);
                    }
                    fclose(fpEx);
                    break;
                }
            }
            // Since the above loop ran through all of the files in the directory
            // if it never went into the if branch, that means it never found a file
            // that matches the name provided by the user.
            if (didIFindTheFile == 0)
                printf("File not found\n");
        } else if (strcasecmp(strncpy(temp3, input, 4), "QUIT") == 0) {
            exit(1);
        } else {
            printf("Invalid command\n");
            continue;
        }
    }
}