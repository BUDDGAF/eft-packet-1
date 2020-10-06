// TODO: Get some class inheritance going
// TODO: Change eft.h offsets to class based offsets?
#pragma once
#include <Windows.h>
#include <iostream>
#include <vector>
#include <string.h>
#include <stdint.h>
#include <boost/algorithm/string.hpp>
#include <intrin.h>
#include <io.h>
#include <fcntl.h>
#include <basetsd.h>
#include <tchar.h>
#include <strsafe.h>
#include <conio.h>
#include <stdio.h>

#include "leechcore.h"
#include "vmmdll.h"
#include "json.hpp"

#include "eft.h"
#include "unitystructures.h"
#include "Util.h"

#pragma comment(lib, "leechcore")
#pragma comment(lib, "vmm")

//#define TWOGAMEWORLDS

// Class PlayerInfo
PlayerInfo::PlayerInfo() {
    this->addr = 0x00000000;

}
PlayerInfo::PlayerInfo(uint64_t addr, MemoryAccess ma) {

    this->addr = addr;
    this->ma = ma;
    
}
uint64_t PlayerInfo::GetAddr() {
    return this->addr;
}
// Get the name of the player
std::wstring PlayerInfo::GetName() {

    return this->ma.ReadUnityString(addr + PlayerInfoName);
}

uint32_t PlayerInfo::GetSide() {
    return this->ma.ReadIntMemory(addr + PlayerInfoSide);
}

uint32_t PlayerInfo::GetCreationDate() {
    return this->ma.ReadIntMemory(addr + CreationDate);
}

// Class PlayerProfile
PlayerProfile::PlayerProfile() {
    this->addr = 0x00000000;
}

PlayerProfile::PlayerProfile(uint64_t addr, MemoryAccess ma) {

    this->addr = addr;
    this->ma = ma;
}

uint64_t PlayerProfile::GetPlayerInfoObjectAddr() {
    return ma.Readuint64_tMemory(this->addr + PlayerInfoObjectAddr);
}

PlayerInfo PlayerProfile::GetPlayerInfo() {
    return PlayerInfo::PlayerInfo(GetPlayerInfoObjectAddr(), ma);
}

uint64_t PlayerProfile::GetAddr() {
    return this->addr;
}

std::wstring PlayerProfile::GetPlayerID() {
    return this->ma.ReadUnityString(addr + PlayerProfilePlayerID);
}

// Class MovementContext

MovementContext::MovementContext() {
    this->addr = 0x00000000;

}

MovementContext::MovementContext(uint64_t addr, MemoryAccess ma) {
    this->addr = addr;
    this->ma = ma;
}

// Get the direction the player is facing (yaw)
float MovementContext::GetDirection() {
    float deg = ma.ReadFloatMemory(this->addr + DirectionAddr);
    if (deg < 0) {
        return (float)360.0 + deg;
    }
    else if (deg > 360) {
        return (float)fmod(deg, 360);
    }

    return deg;
}

// Class Player
Player::Player() {
    this->addr = 0x00000000;
    
    this->pos.x = 0;
    this->pos.y = 0;
    this->pos.z = 0;
    this->direction = 0;
    this->side = 0;
    this->playerID = L"";
    this->name = L"";

}

Player::Player(uint64_t addr, MemoryAccess ma) {
    this->addr = addr;
    this->ma = ma;
    // Set the coordinates for this player
    this->pos = this->GetPos();
    //  Set the yaw the player is facing
    this->direction = this->GetDirection();
    // Set the name of the player
    PlayerInfo pi = this->GetPlayerProfile().GetPlayerInfo();
    this->name = pi.GetName();
    // Set the PlayerID
    //this->playerID = this->GetPlayerProfile().GetPlayerID();
    this->playerID = L"";
    // Set the side of the player
    //this->side = this->GetPlayerProfile().GetPlayerInfo().GetSide();
    this->side = 0;
    // Get the creation date
    this->creationDate = pi.GetCreationDate();
}

std::wstring Player::GetTeam() {
    uint32_t creationDate = this->GetCreationDate();
    if (name == LOCALPLAYERNAME || creationDate == MYCREATIONDATE) {
        return L"localplayer";
    }
    else if (name == FRIEND1NAME || name == FRIEND2NAME || name == FRIEND3NAME || name == FRIEND4NAME 
        || creationDate == FRIEND1CREATIONDATE || creationDate == FRIEND2CREATIONDATE || creationDate == FRIEND3CREATIONDATE || creationDate == FRIEND4CREATIONDATE) {
        return L"friendly";
    }
    // It's some kind of AI
    else if (Util::NameIsScav(name)) {
        if (this->GetCreationDate() <= 0) {
            return L"aiscav";
        }
        else {
            return L"playerscav";
        }
    }
    else if (Util::NameIsRaider(name)) {
        return L"raider";
    }
    else {
        return L"pmc";
    }
}

uint64_t Player::GetAddr() {
    return this->addr;
}

uint64_t Player::GetPlayerProfileObjectAddr() {
    return ma.Readuint64_tMemory(addr + PlayerProfileObjectAddr);
}

PlayerProfile Player::GetPlayerProfile() {
    return PlayerProfile::PlayerProfile(GetPlayerProfileObjectAddr(), this->ma);
}

std::wstring Player::GetName() {
    return this->name;
}

std::wstring Player::GetPlayerID() {
    return this->playerID;
}

uint32_t Player::GetCreationDate() {
    return this->creationDate;
}

uint32_t Player::GetSide() {
    return this->side;
}

float Player::GetX() {
    return this->pos.x;
}

float Player::GetY() {
    return this->pos.y;
}

float Player::GetZ() {
    return this->pos.z;
}

POINT3D Player::GetPos() {

    POINT3D pos;
    // Get the player's position based on their bones matrix
    uint64_t bone_matrix = this->GetBoneTransform();
    uint64_t bone = ma.ReadChain(bone_matrix, { 0x20, 0x10, 0x38 });
    pos = ma.ReadVector3Memory(bone + 0xB0);
    //pos = ma.ReadVector3Memory(bone + 0x30);

    // We swap them because something is fucked up but w/e
    return { pos.x, pos.z, pos.y };
}

float Player::GetDirection() {
    // Get the direction (degrees) the player is facing
    return this->GetMovementContextObject().GetDirection();
}

uint64_t Player::GetBoneTransform() {
    uint64_t transformAddr = 0;
    return ma.ReadChain(this->addr, { PlayerBody, SkeletonRootJoin, BoneEnumerator, BoneTransformArray });
}

// TODO: Unfuck this piece of garbage.
POINT3D Player::GetBonePosition(uint64_t transform) {

    POINT3D pos;
    pos.x = 0;
    pos.y = 0;
    pos.z = 0;

    uint64_t playerBodyAddr = ma.Readuint64_tMemory(this->addr + PlayerBody);
    uint64_t skeletonRootJoinAddr = ma.Readuint64_tMemory(playerBodyAddr + SkeletonRootJoin);
    uint64_t boneEnumeratorAddr = ma.Readuint64_tMemory(skeletonRootJoinAddr + BoneEnumerator);
    uint64_t bonesAddr = ma.Readuint64_tMemory(boneEnumeratorAddr + BoneTransformArray);

    std::vector<BYTE> bvBoneData = ma.ReadByteArrayMemory(bonesAddr, 64);
    auto transform_internal =ma.Readuint64_tMemory(transform + 0x10);

    auto matrices = ma.Readuint64_tMemory(transform_internal + 0x38);
    auto index = ma.ReadIntMemory(transform_internal + 0x40);
    uint64_t matrix_list_base = 0;
    uint64_t dependency_index_table_base = 0;

    matrix_list_base = ma.Readuint64_tMemory(matrices + 0x18);

    dependency_index_table_base = ma.Readuint64_tMemory(matrices + 0x20);

    static auto get_dependency_index = [this](uint64_t base, int32_t index)
    {
        index = ma.ReadIntMemory((uintptr_t)(base + index * 4));
        return index;
    };

    static auto get_matrix_blob = [this](uint64_t base, uint64_t offs, float* blob, uint32_t size) {
        std::vector<BYTE> bvFloat = ma.ReadByteArrayMemory((uintptr_t)(base + offs), size);
        std::memcpy(blob, bvFloat.data(), sizeof(float));
    };

    int32_t index_relation = get_dependency_index(dependency_index_table_base, index);

    POINT3D ret_value;
    {
        float* base_matrix3x4 = (float*)malloc(64),
            * matrix3x4_buffer0 = (float*)((uint64_t)base_matrix3x4 + 16),
            * matrix3x4_buffer1 = (float*)((uint64_t)base_matrix3x4 + 32),
            * matrix3x4_buffer2 = (float*)((uint64_t)base_matrix3x4 + 48);

        get_matrix_blob(matrix_list_base, index * 48, base_matrix3x4, 16);

        __m128 xmmword_1410D1340 = { -2.f, 2.f, -2.f, 0.f };
        __m128 xmmword_1410D1350 = { 2.f, -2.f, -2.f, 0.f };
        __m128 xmmword_1410D1360 = { -2.f, -2.f, 2.f, 0.f };

        while (index_relation >= 0)
        {
            uint32_t matrix_relation_index = 6 * index_relation;

            // paziuret kur tik 3 nureadina, ten translationas, kur 4 = quatas ir yra rotationas.
            get_matrix_blob(matrix_list_base, 8 * matrix_relation_index, matrix3x4_buffer2, 16);
            __m128 v_0 = *(__m128*)matrix3x4_buffer2;

            get_matrix_blob(matrix_list_base, 8 * matrix_relation_index + 32, matrix3x4_buffer0, 16);
            __m128 v_1 = *(__m128*)matrix3x4_buffer0;

            get_matrix_blob(matrix_list_base, 8 * matrix_relation_index + 16, matrix3x4_buffer1, 16);
            __m128i v9 = *(__m128i*)matrix3x4_buffer1;

            __m128* v3 = (__m128*)base_matrix3x4; // r10@1
            __m128 v10; // xmm9@2
            __m128 v11; // xmm3@2
            __m128 v12; // xmm8@2
            __m128 v13; // xmm4@2
            __m128 v14; // xmm2@2
            __m128 v15; // xmm5@2
            __m128 v16; // xmm6@2
            __m128 v17; // xmm7@2

            v10 = _mm_mul_ps(v_1, *v3);
            v11 = _mm_castsi128_ps(_mm_shuffle_epi32(v9, 0));
            v12 = _mm_castsi128_ps(_mm_shuffle_epi32(v9, 85));
            v13 = _mm_castsi128_ps(_mm_shuffle_epi32(v9, -114));
            v14 = _mm_castsi128_ps(_mm_shuffle_epi32(v9, -37));
            v15 = _mm_castsi128_ps(_mm_shuffle_epi32(v9, -86));
            v16 = _mm_castsi128_ps(_mm_shuffle_epi32(v9, 113));

            v17 = _mm_add_ps(
                _mm_add_ps(
                    _mm_add_ps(
                        _mm_mul_ps(
                            _mm_sub_ps(
                                _mm_mul_ps(_mm_mul_ps(v11, (__m128)xmmword_1410D1350), v13),
                                _mm_mul_ps(_mm_mul_ps(v12, (__m128)xmmword_1410D1360), v14)),
                            _mm_castsi128_ps(_mm_shuffle_epi32(_mm_castps_si128(v10), -86))),
                        _mm_mul_ps(
                            _mm_sub_ps(
                                _mm_mul_ps(_mm_mul_ps(v15, (__m128)xmmword_1410D1360), v14),
                                _mm_mul_ps(_mm_mul_ps(v11, (__m128)xmmword_1410D1340), v16)),
                            _mm_castsi128_ps(_mm_shuffle_epi32(_mm_castps_si128(v10), 85)))),
                    _mm_add_ps(
                        _mm_mul_ps(
                            _mm_sub_ps(
                                _mm_mul_ps(_mm_mul_ps(v12, (__m128)xmmword_1410D1340), v16),
                                _mm_mul_ps(_mm_mul_ps(v15, (__m128)xmmword_1410D1350), v13)),
                            _mm_castsi128_ps(_mm_shuffle_epi32(_mm_castps_si128(v10), 0))),
                        v10)),
                v_0);

            *v3 = v17;

            index_relation = get_dependency_index(dependency_index_table_base, index_relation);
        }

        ret_value = *(POINT3D*)base_matrix3x4;
        delete[] base_matrix3x4;
    }

    return ret_value;

    return pos;
}

uint64_t Player::GetMovementContextObjectAddr() {
    return ma.Readuint64_tMemory(this->addr + MovementContextObjectAddr);
}

MovementContext Player::GetMovementContextObject() {
    return MovementContext::MovementContext(GetMovementContextObjectAddr(), this->ma);
}


// Class PlayerList
PlayerList::PlayerList() {
    this->addr = 0x00000000;
}

PlayerList::PlayerList(uint64_t addr, MemoryAccess ma) {
    this->addr = addr;
    this->ma = ma;

}

int PlayerList::GetPlayerCount() {
    return ma.ReadIntMemory(addr + PlayerCountAddr);
}

std::vector<Player> PlayerList::GetPlayers() {

    uint64_t firstPlayer = ma.Readuint64_tMemory(addr + FirstPlayerAddr);
    //std::wcout << "Got first player\n";
    std::vector<Player> pvPlayers;
    for (int i = 0; i < GetPlayerCount(); i++) {
        // Magic if this works the way I hope on the first try
        pvPlayers.push_back(Player::Player(ma.Readuint64_tMemory(firstPlayer + PlayerArrayNext + (i * PlayerArrayCount)), ma));
    }

    return pvPlayers;
}

uint64_t PlayerList::GetAddr() {
    return this->addr;
}

// Class LocalGameWorld
LocalGameWorld::LocalGameWorld() {
    this->addr = 0x00000000;
}

LocalGameWorld::LocalGameWorld(uint64_t addr, MemoryAccess ma) {
    this->addr = addr;
    this->ma = ma;

}

uint64_t LocalGameWorld::GetPlayerArrayObjectAddr() {
    return ma.Readuint64_tMemory(addr + PlayerArrayObjectAddr);
}

PlayerList LocalGameWorld::GetPlayerArrayObject() {
    return PlayerList::PlayerList(GetPlayerArrayObjectAddr(), ma);

}

uint64_t LocalGameWorld::GetAddr() {
    return this->addr;
}

uint64_t LocalGameWorld::GetItemArrayAddr() {
    //std::wcout << L"Getting ItemListAddr" << std::endl;
    return ma.Readuint64_tMemory(addr + ArrayItemsObjectAddr);
}

ItemList LocalGameWorld::GetItemArray() {
    //std::wcout << L"Getting ItemList" << std::endl;
    return ItemList::ItemList(GetItemArrayAddr(), ma);
}

// Class GameObject
GameObject::GameObject() {
    this->addr = 0x00000000;
    
}

GameObject::GameObject(uint64_t addr, MemoryAccess ma) {
    this->addr = addr;
    this->ma = ma;

}

uint64_t GameObject::GetGameObjectNameAddr() {
    return ma.Readuint64_tMemory(addr + GameObjectNameAddr);
}

std::string GameObject::GetObjectName() {

    
    uint64_t objNameAddr = GetGameObjectNameAddr();
    int i = 0;
    //std::wcout << "Read name bytes\n";
    for (; i < 256; i++)
    {
        if (ma.ReadByteMemory(objNameAddr + i) == 0)    
        {
            break;
        }
    }
    
    std::vector<BYTE> bvName = ma.ReadByteArrayMemory(objNameAddr, i);
    
    std::string name(bvName.begin(), bvName.end());
    std::wstring wName(bvName.begin(), bvName.end());
    //std::wcout << "Got name: " << wName << "\n";

    //std::wcout << L"Got GameObject name: " << wName << std::endl;

    return name;
}

uint64_t GameObject::GetN65Addr() {
    return ma.Readuint64_tMemory(addr + N65Addr);
}

uint64_t GameObject::GetAddr() {

    return this->addr;
}

uint64_t GameObject::GetLocalGameWorldObjectAddr() {
    uint64_t n65a = ma.Readuint64_tMemory(addr + N65Addr);
    uint64_t n70a = ma.Readuint64_tMemory(n65a + N70Addr);
    uint64_t lgwa = ma.Readuint64_tMemory(n70a + LocalGameWorldAddr);
    //printf("LocalGameWorld = 0x%llx\n", lgwa);
    return lgwa;
}

LocalGameWorld GameObject::GetLocalGameWorldObject() {
    return LocalGameWorld::LocalGameWorld(GetLocalGameWorldObjectAddr(), ma);
}

// Class BaseObject
BaseObject::BaseObject() {
    this->addr = 0x00000000;
}

BaseObject::BaseObject(uint64_t addr, MemoryAccess ma) {
    this->addr = addr;
    this->ma = ma;
}

uint64_t BaseObject::GetGameObjectAddr() {
    return ma.Readuint64_tMemory(addr + GameObjectAddr);
}

GameObject BaseObject::GetGameObject() {
    return GameObject::GameObject(GetGameObjectAddr(), ma);
}

uint64_t BaseObject::GetNextBaseObjectAddr() {
    return ma.Readuint64_tMemory(addr + NextBaseObjectAddr);
}

BaseObject BaseObject::GetNextBaseObject() {
    return BaseObject::BaseObject(GetNextBaseObjectAddr(), ma);
}

uint64_t BaseObject::GetAddr() {
    return this->addr;
}

// Class MemoryAccess
MemoryAccess::MemoryAccess() {
    this->processName = "";

}

MemoryAccess::MemoryAccess(DWORD pid) {
    this->processName = processName;
    this->dwPID = pid;
    this->dmaInit = FALSE;

    std::wcout << L"[EFT DMA] Initializing PCILeech DMA to PID: << " << pid << std::endl;

    std::string strArg1 = "";
    std::string strArg2 = "-device";
    std::string strArg3 = "fpga";

    LPSTR lpArg1 = const_cast<char*>(strArg1.c_str());
    LPSTR lpArg2 = const_cast<char*>(strArg2.c_str());
    LPSTR lpArg3 = const_cast<char*>(strArg3.c_str());

    LPSTR* lpsArgs = (LPSTR*)malloc(3 * sizeof(LPSTR));
    lpsArgs[0] = lpArg1;
    lpsArgs[1] = lpArg2;
    lpsArgs[2] = lpArg3;

    // Initialize PCILeech DLL with a FPGA hardware device
    BOOL result = VMMDLL_Initialize(3, lpsArgs);
    if (result) {
        wprintf(L"SUCCESS: VMMDLL_Initialize\n");
        this->dmaInit = TRUE;
    }
    else {
        wprintf(L"FAIL:    VMMDLL_Initialize\n");
    }
}

MemoryAccess::MemoryAccess(std::string processName) {
    this->processName = processName;
    this->dmaInit = FALSE;

    std::string strArg1 = "";
    std::string strArg2 = "-device";
    std::string strArg3 = "fpga";

    LPSTR lpArg1 = const_cast<char*>(strArg1.c_str());
    LPSTR lpArg2 = const_cast<char*>(strArg2.c_str());
    LPSTR lpArg3 = const_cast<char*>(strArg3.c_str());

    LPSTR* lpsArgs = (LPSTR*)malloc(3 * sizeof(LPSTR));
    lpsArgs[0] = lpArg1;
    lpsArgs[1] = lpArg2;
    lpsArgs[2] = lpArg3;

    // Initialize PCILeech DLL with a FPGA hardware device
    BOOL result = VMMDLL_Initialize(3, lpsArgs);
    if (result) {
        wprintf(L"SUCCESS: VMMDLL_Initialize\n");
    }
    else {
        wprintf(L"FAIL:    VMMDLL_Initialize\n");
    }

    // TODO, fix this cast
    result = VMMDLL_PidGetFromName((LPSTR)processName.c_str(), &dwPID);
    if (result) {
        wprintf(L"SUCCESS: VMMDLL_PidGetFromName\n");
        wprintf(L"         PID = %i\n", dwPID);
        // redundant
        this->dwPID = dwPID;
        this->dmaInit = TRUE;
    }
    else {
        wprintf(L"FAIL:    VMMDLL_PidGetFromName\n");
    }

}

MemoryAccess::MemoryAccess(std::string processName, std::string deviceType) {
    this->processName = processName;
    this->dmaInit = FALSE;
    std::string strArg1;
    std::string strArg2;
    std::string strArg3;

    if (deviceType == "fpga") {
        strArg1 = "";
        strArg2 = "-device";
        strArg3 = "fpga";
    }
    else if (deviceType == "netv2") {
        strArg1 = "";
        strArg2 = "-device";
        strArg3 = "RAWUDP://ip=192.168.1.208";
    }
    else {
        std::wcout << "[EFT DMA] Unsupported device!\n";
        exit(-1);
    }

    LPSTR lpArg1 = const_cast<char*>(strArg1.c_str());
    LPSTR lpArg2 = const_cast<char*>(strArg2.c_str());
    LPSTR lpArg3 = const_cast<char*>(strArg3.c_str());

    LPSTR* lpsArgs = (LPSTR*)malloc(3 * sizeof(LPSTR));
    lpsArgs[0] = lpArg1;
    lpsArgs[1] = lpArg2;
    lpsArgs[2] = lpArg3;

    // Initialize PCILeech DLL with a FPGA hardware device
    BOOL result = VMMDLL_Initialize(3, lpsArgs);
    if (result) {
        wprintf(L"SUCCESS: VMMDLL_Initialize\n");
    }
    else {
        wprintf(L"FAIL:    VMMDLL_Initialize\n");
        exit(-1);
    }

    // TODO, fix this cast
    result = VMMDLL_PidGetFromName((LPSTR)processName.c_str(), &dwPID);
    if (result) {
        wprintf(L"SUCCESS: VMMDLL_PidGetFromName\n");
        wprintf(L"         PID = %i\n", dwPID);
        // redundant
        this->dwPID = dwPID;
        this->dmaInit = TRUE;
    }
    else {
        wprintf(L"FAIL:    VMMDLL_PidGetFromName\n");
    }
}

// Retrieve the module of explorer.exe by its name. Note it is also possible
// to retrieve it by retrieving the complete module map (list) and iterate
// over it. But if the name of the module is known this is more convenient.
// This required that the PEB and LDR list in-process haven't been tampered
// with ...
uint64_t MemoryAccess::GetImageBaseAddress(std::string imageName) {
    BOOL result;
    int i;
    VMMDLL_MAP_MODULEENTRY ModuleEntryExplorer;

    // TODO the LPWSTR case might break things
    std::wstring wImageName(L"UnityPlayer.dll");
    LPWSTR lpwImageName = const_cast<LPWSTR>(wImageName.c_str());
    result = VMMDLL_ProcessMap_GetModuleFromName(this->dwPID, lpwImageName, &ModuleEntryExplorer);
    if (result) {
        wprintf(L"[EFT DMA] Got base address with method 1\n");
        return ModuleEntryExplorer.vaBase;
    }
    else {
        wprintf(L"FAIL method 1:    VMMDLL_ProcessMap_GetModuleFromName\n");
        return this->GetImageBaseAddress2(imageName);
    }
}

// Retrieve the memory map from the page table. This function also tries to
// make additional parsing to identify modules and tag the memory map with
// them. This is done by multiple methods internally and may sometimes be
// more resilient against anti-reversing techniques that may be employed in
// some processes.
uint64_t MemoryAccess::GetImageBaseAddress2(std::string imageName) {
    BOOL result;
    int i;
    ULONG64 cMemMapEntries;

    wprintf(L"------------------------------------------------------------\n");
    wprintf(L"#06: Get PTE Memory Map of 'explorer.exe'.                  \n");
    DWORD cbPteMap = 0;
    PVMMDLL_MAP_PTE pPteMap = NULL;
    PVMMDLL_MAP_PTEENTRY pPteMapEntry;
    wprintf(L"CALL:    VMMDLL_ProcessMap_GetPte #1\n");
    result = VMMDLL_ProcessMap_GetPte(this->dwPID, NULL, &cbPteMap, TRUE);
    if (result) {
        wprintf(L"SUCCESS: VMMDLL_ProcessMap_GetPte #1\n");
        wprintf(L"         ByteCount = %i\n", cbPteMap);
    }
    else {
        wprintf(L"FAIL:    VMMDLL_ProcessMap_GetPte #1\n");
        return 1;
    }
    pPteMap = (PVMMDLL_MAP_PTE)LocalAlloc(0, cbPteMap);
    if (!pPteMap) {
        wprintf(L"FAIL:    OutOfMemory\n");
        return 1;
    }
    wprintf(L"CALL:    VMMDLL_ProcessMap_GetPte #2\n");
    result = VMMDLL_ProcessMap_GetPte(this->dwPID, pPteMap, &cbPteMap, TRUE);
    if (result) {
        wprintf(L"SUCCESS: VMMDLL_ProcessMap_GetPte #2\n");
        wprintf(L"         #      #PAGES ADRESS_RANGE                      SRWX\n");
        wprintf(L"         ====================================================\n");
        for (i = 0; i < pPteMap->cMap; i++) {
            pPteMapEntry = &pPteMap->pMap[i];
            wprintf(
                L"         %04x %8x %016llx-%016llx %sr%s%s%s%S\n",
                i,
                (DWORD)pPteMapEntry->cPages,
                pPteMapEntry->vaBase,
                pPteMapEntry->vaBase + (pPteMapEntry->cPages << 12) - 1,
                pPteMapEntry->fPage & VMMDLL_MEMMAP_FLAG_PAGE_NS ? "-" : "s",
                pPteMapEntry->fPage & VMMDLL_MEMMAP_FLAG_PAGE_W ? "w" : "-",
                pPteMapEntry->fPage & VMMDLL_MEMMAP_FLAG_PAGE_NX ? "-" : "x",
                pPteMapEntry->cwszText ? (pPteMapEntry->fWoW64 ? " 32 " : "    ") : "",
                pPteMapEntry->wszText
            );
        }
    }
    else {
        wprintf(L"FAIL:    VMMDLL_ProcessMap_GetPte #2\n");
        return 1;
    }
    LocalFree(pPteMap);
    pPteMap = NULL;

    
    return this->GetImageBaseAddress3(imageName);
}

uint64_t MemoryAccess::GetImageBaseAddress3(std::string imageName) {
    return 0;
}



uint64_t MemoryAccess::Readuint64_tMemory(uint64_t ptr) {
    uint64_t ret = 0x0;

    

    if (!VMMDLL_MemRead(this->dwPID, ptr, (PBYTE)& ret, sizeof(ret))) {
        //std::wcout << L"[EFT DMA] Could not read address in Readuint64_tMemory!\n";
        //exit(-1);
    }
    else {
        //printf("Readuint64_tMemory = %p\n", ret);
    }

    return ret;
}

uint64_t MemoryAccess::ReadChain(uint64_t ptr, std::vector<uint64_t> chain) {
    uint64_t ret = MemoryAccess::Readuint64_tMemory(ptr + chain.at(0));
    for (int i = 1; i < chain.size(); i++) {
        ret = MemoryAccess::Readuint64_tMemory(ret + chain.at(i));
    }

    return ret;
}

POINT3D MemoryAccess::ReadVector3Memory(uint64_t ptr) {
    POINT3D ret;
    ret.x = 0;
    ret.y = 0;
    ret.z = 0;

    if (!VMMDLL_MemRead(this->dwPID, ptr, (PBYTE)& ret, sizeof(POINT3D))) {
        //std::wcout << L"[EFT DMA] Could not read address in ReadVector3Memory!\n";
        //exit(-1);

    }
    return ret;
}

DWORD MemoryAccess::ReadDWORDMemory(DWORD ptr) {
    DWORD ret = 0x0;

    //std::wcout << "Attempting to read: ";
    //printf("val = 0x%llx\n", ptr);
    if (!VMMDLL_MemRead(this->dwPID, ptr, (PBYTE)& ret, sizeof(ret))) {
        //std::wcout << L"[EFT DMA] Could not read address in ReadDWORDMemory!\n";
        //exit(-1);

    }
    else {
        //printf("Readuint64_tMemory = %p\n", ret);
    }

    return ret;
}

// DANGER!!! DO NOT USE THIS UNLESS YOU KNOW WHAT YOU'RE DOING
// YOU WILL GET BANNED
BOOL MemoryAccess::Writeuint64_tMemory(uint64_t ptr, uint64_t val) {
    BOOL status = FALSE;

    if (!VMMDLL_MemWrite(dwPID, ptr, (PBYTE)& val, sizeof(val))) {
        std::wcout << L"[EFT DMA] Error writing to memory\n";
        //exit(-1);
    }
    else {
        std::wcout << L"[EFT DMA] Wrote uint64_t to memory\n";
        status = TRUE;
    }

    return status;
}

int MemoryAccess::ReadIntMemory(uint64_t ptr) {
    int ret = 0;
    //std::cout << "Attempting to read: ";
    //printf("val = 0x%llx\n", ptr);
    if (!VMMDLL_MemRead(this->dwPID, ptr, (PBYTE)& ret, sizeof(ret))) {
        //std::wcout << "[EFT DMA] Could not read address in ReadIntMemory!\n";
        //exit(-1);
    }
    else {
        //printf("ReadIntMemory = %p\n", ret);
    }

    return ret;
}

float MemoryAccess::ReadFloatMemory(uint64_t ptr) {
    float ret = 0.0;
    //std::cout << "Attempting to read: ";
    //printf("val = 0x%llx\n", ptr);
    if (!VMMDLL_MemRead(this->dwPID, ptr, (PBYTE)& ret, sizeof(ret))) {
        //std::wcout << L"[EFT DMA] Could not read address in ReadFloatMemory!\n";
    }
    else {
        //printf("ReadFloatMemory = %p\n", ret);
    }

    return ret;
}

BYTE MemoryAccess::ReadByteMemory(uint64_t ptr) {
    BYTE ret = 0x00;
    //std::cout << "Attempting to read: ";
    //printf("val = 0x%llx\n", ptr);
    if (!VMMDLL_MemRead(this->dwPID, ptr, (PBYTE)& ret, sizeof(ret))) {
        //std::wcout << L"[EFT DMA] Could not read address in ReadByteMemory!\n";
        //exit(-1);
    }
    else {
        //printf("ReadByteMemory = %p\n", ret);
    }

    return ret;
}

std::vector<BYTE> MemoryAccess::ReadByteArrayMemory(uint64_t ptr, int size) {
    std::vector<BYTE> bvByteArray;
    //std::cout << "Attempting to read array at: ";
    //printf("val = 0x%llx\n", ptr);

    BYTE* baBytes;
    baBytes = (BYTE*)malloc(size);
    memset(baBytes, 0x0, size);

    if (!VMMDLL_MemRead(this->dwPID, ptr, (PBYTE)baBytes, size)) {
        //std::wcout << L"[EFT DMA] Could not read address in ReadByteArrayMemory!\n";
        //exit(-1);
    }
    else {
        //printf("ReadByteArrayMemory:\n");
        for (int i = 0; i < size; i++) {
            bvByteArray.push_back(baBytes[i]);
            //wprintf(L"%02X", bvByteArray.at(i));
            if (i+1 % 32 == 0) {
                //wprintf(L"\n");
            }
        }
    }

    return bvByteArray;
}

std::wstring MemoryAccess::ReadUnityString(uint64_t ptr, int type) {
    if (type == 0) {

    }
    else {

    }
    // We expect to be given a pointer directly to the object
    uint64_t unicodeStringObjAddr = MemoryAccess::Readuint64_tMemory(ptr);
    // The length of the string in characters (they are two bytes long each)
    int lengthOfString = MemoryAccess::ReadIntMemory(unicodeStringObjAddr + UniStringLengthAddr);
    // TODO: Is this a bad assumption?
    if (lengthOfString > 1000 || lengthOfString < 0) {
        //std::wcout << L"ERROR: Got bad UnityEngineString length!" << std::endl;
        return L"";
    }
    // Read the name from memory, length in bytes is length * 2 (UTF 16 Big endian)
    std::vector<BYTE> bvName = MemoryAccess::ReadByteArrayMemory(unicodeStringObjAddr + UniStringNameAddr, lengthOfString * 2);
    // Convert to UTF16 LE
    std::wstring name;
    for (int i = 0; i < bvName.size(); i += 2) {
        char16_t myChar = bvName.at(i);
        myChar += (bvName.at(i + 1) << 8);
        name.push_back(myChar);
    }
    return name;
}

std::wstring MemoryAccess::ReadUTF8String(uint64_t ptr, int type) {
    // We expect to be given a pointer directly to the object
    uint64_t unicodeStringObjAddr = MemoryAccess::Readuint64_tMemory(ptr);
    // The length of the string in characters (they are two bytes long each)
    int lengthOfString = MemoryAccess::ReadIntMemory(unicodeStringObjAddr + UniStringLengthAddr);
    // TODO: Is this a bad assumption?
    if (lengthOfString > 1000) {
        std::wcout << L"ERROR: Got bad UnityEngineString length!" << std::endl;
        return L"";
    }
    // Read the name from memory, length in bytes is length * 1 (UTF8)
    std::vector<BYTE> bvName = MemoryAccess::ReadByteArrayMemory(unicodeStringObjAddr + UniStringNameAddr, lengthOfString);
    // Convert to UTF16 LE
    std::wstring name;
    for (int i = 0; i < bvName.size(); i ++) {
        name.push_back(bvName.at(i));
    }
    return name;
}

std::wstring MemoryAccess::ReadUTF16String(uint64_t ptr, int type) {
    // We expect to be given a pointer directly to the object
    uint64_t unicodeStringObjAddr = MemoryAccess::Readuint64_tMemory(ptr);
    // The length of the string in characters (they are two bytes long each)
    int lengthOfString = MemoryAccess::ReadIntMemory(unicodeStringObjAddr + UniStringLengthAddr);
    // TODO: Is this a bad assumption?
    if (lengthOfString > 1000) {
        std::wcout << L"ERROR: Got bad UnityEngineString length!" << std::endl;
        return L"";
    }
    // Read the name from memory, length in bytes is length * 2 (UTF 16 Big endian)
    std::vector<BYTE> bvName = MemoryAccess::ReadByteArrayMemory(unicodeStringObjAddr + UniStringNameAddr, lengthOfString * 2);
    // Convert to UTF16 LE
    std::wstring name;
    for (int i = 0; i < bvName.size(); i += 2) {
        char16_t myChar = bvName.at(i);
        myChar += (bvName.at(i + 1) << 8);
        name.push_back(myChar);
    }
    return name;
}


// Class GameObjectManager
GameObjectManager::GameObjectManager() {
    this->addr = 0x0;
}

GameObjectManager::GameObjectManager(uint64_t addr,  MemoryAccess ma) {
    this->addr = addr;
    this->ma = ma;
}

uint64_t GameObjectManager::GetAddr() {
    return this->addr;
}

uint64_t GameObjectManager::GetFirstActiveObjectAddr() {
    //std::wcout << "Reading first active object addr\n";
    return ma.Readuint64_tMemory(addr + FirstActiveObjectAddr);
}
BaseObject GameObjectManager::GetFirstActiveObject() {
    //std::wcout << "Searching for first active object" << std::endl;
    return BaseObject::BaseObject(GetFirstActiveObjectAddr(), ma);
}

uint64_t GameObjectManager::GetLastActiveObjectAddr() {
    return ma.Readuint64_tMemory(addr + LastActiveObjectAddr);
}
BaseObject GameObjectManager::GetLastActiveObject() {
    return BaseObject::BaseObject(GetLastActiveObjectAddr(), ma);
}

uint64_t GameObjectManager::GetFirstTaggedObjectAddr() {
    return ma.Readuint64_tMemory(addr + FirstTaggedObjectAddr);
}
BaseObject GameObjectManager::GetFirstTaggedObject() {
    return BaseObject::BaseObject(GetFirstTaggedObjectAddr(), ma);
}

GameObject GameObjectManager::FindActiveObject(std::string objName, int numWorlds) {
    BaseObject activeObject = GetFirstActiveObject();
    BaseObject firstObject = GetFirstActiveObject();
    int count = 0;
    do {
        std::string thisObject = activeObject.GetGameObject().GetObjectName();
        boost::algorithm::to_lower(thisObject);

        std::string otherObject = objName;
        boost::algorithm::to_lower(objName);

        if (thisObject == otherObject) {
            if (count >= numWorlds) {
                return activeObject.GetGameObject();
            }
            count++;
    }


        activeObject = activeObject.GetNextBaseObject();
    } while (activeObject.GetAddr() != firstObject.GetAddr());

    GameObject nullObject;
    return nullObject;
}

void GameObjectManager::ListActiveObjects() {
    //std::cout << "Searching for active object" << std::endl;
    BaseObject activeObject = GetFirstActiveObject();
    do {
        std::string thisObject = activeObject.GetGameObject().GetObjectName();
        boost::algorithm::to_lower(thisObject);

        //std::wcout << Util::StrToWstr(thisObject) << std::endl;


        activeObject = activeObject.GetNextBaseObject();
    } while (activeObject.GetAddr() != GetFirstActiveObject().GetAddr());
}

int GameObjectManager::GetNumObjectsByName(std::string objName) {
    BaseObject activeObject = GetFirstActiveObject();
    int count = 0;
    do {
        std::string thisObject = activeObject.GetGameObject().GetObjectName();
        boost::algorithm::to_lower(thisObject);

        std::string otherObject = objName;
        boost::algorithm::to_lower(objName);

        if (thisObject == otherObject) {
            count++;
        }
        activeObject = activeObject.GetNextBaseObject();
    } while (activeObject.GetAddr() != GetFirstActiveObject().GetAddr());

    GameObject nullObject;
    return count;
}

GameObject GameObjectManager::FindTaggedObject(std::string objName) {
    BaseObject activeObject = GetFirstTaggedObject();

    do {
        std::string thisObject = activeObject.GetGameObject().GetObjectName();
        boost::algorithm::to_lower(thisObject);

        std::string otherObject = objName;
        boost::algorithm::to_lower(objName);

        if (thisObject == otherObject) {
            return activeObject.GetGameObject();
        }
        activeObject = activeObject.GetNextBaseObject();
    } while (activeObject.GetAddr() != GetFirstActiveObject().GetAddr());

    GameObject nullObject;
    return nullObject;
}

GameObject GameObjectManager::FindObject(BaseObject b, std::string objName) {
    BaseObject activeObject = b;
    do {
        std::string thisObject = activeObject.GetGameObject().GetObjectName();
        boost::algorithm::to_lower(thisObject);

        std::string otherObject = objName;
        boost::algorithm::to_lower(objName);

        if (thisObject == otherObject) {
            return activeObject.GetGameObject();
        }
        activeObject = activeObject.GetNextBaseObject();
    } while (activeObject.GetAddr() != b.GetAddr());

    GameObject nullObject;
    return nullObject;
}

// Class ItemList

ItemList::ItemList() {
    this->addr = 0;
}

ItemList::ItemList(uint64_t addr, MemoryAccess ma) {
    this->addr = addr;
    this->ma = ma;
}

uint64_t ItemList::GetAddr() {
    return this->addr;
}


int ItemList::GetItemListCount() {
    return ma.ReadIntMemory(addr + ItemCountAddr);
}

std::vector<Item> ItemList::GetItems() {
    uint64_t firstItem = ma.Readuint64_tMemory(addr + FirstItemAddr);

    std::vector<Item> ivItems;
    int count = GetItemListCount();
    for (int i = 0; i < count; i++) {
        // Magic if this works the way I hope on the first try
        uint64_t ptr = firstItem + PlayerArrayNext + (i * PlayerArrayCount);
        if (ptr != 0) {
            //std::wcout << "Pointer not zero\n";
            uint64_t itemAddr = ma.Readuint64_tMemory(ptr);
            //std::wcout << "itemAddr not zero\n";
            if (itemAddr != 0) {
                //std::wcout << std::hex << itemAddr << std::dec << std::nouppercase << std::endl;
                ivItems.push_back(Item::Item(itemAddr, ma));
            }
        }
    }
    return ivItems;
}

// Class Item
Item::Item() {
    this->addr = 0;
    this->pos = { 0,0,0 };
    this->name = L"";
    this->name2 = L"";
    this->hashName = L"";
    this->prettyName = L"";
}

Item::Item(uint64_t addr, MemoryAccess ma) {
    this->addr = addr;
    this->ma = ma;

    // Set the name of the item upon initialization
    //this->name = GetItemProfile().GetItemInfo().GetName();
    this->name = L"";
    this->name2 = L"";
    // Set the name from ItemStats
    this->name2 = this->GetItemProfile().GetItemStats().GetGameItem().GetItemTemplate().GetName();
    this->hashName = this->GetItemProfile().GetItemStats().GetGameItem().GetItemTemplate().GetHashName();
    // Set the position of the item
    this->pos = GetItemProfile().GetItemInfo().GetItemLocalization().GetItemCoords().GetItemLocationContainer().GetItemPosition();
    //this->pos = { 0,0,0 };
    this->rarity = RARITY_COMMON;
}

std::wstring Item::GetRarity() {
    switch (this->rarity) {
        case 0:
            return L"trash";
        case 1:
            return L"common";
        case 2:
            return L"rare";
        case 3:
            return L"ultra";
        default:
            return L"common";
    }
}


uint64_t Item::GetAddr() {
    return this->addr;
}

std::wstring Item::GetName() {
    if (this->name == L"Стандартный инвентарь") {
        return L"body";
    }
    else {
        return this->name;
    }
}

std::wstring Item::GetName2() {
    if (this->name2 == L"Стандартный инвентарь") {
        return L"body";
    }
    else {
        return this->name2;
    }
}

std::wstring Item::GetPrettyName() {
    return this->prettyName;
}

// We explicitly call the setter because we only want to call this if we turn
// on all items, or if we have added an item to the valuable item list
BOOL Item::SetPrettyName(nlohmann::json jf) {
    using json = nlohmann::json;

    auto res = std::find_if(jf.begin(), jf.end(), [this](const json& x) {
        auto it = x.find("bsgId");
        return it != x.end() and it.value() == Util::WstrToStr(this->hashName);

        // alternative approach using value member function
        // return x.is_object() and x.value("orientation", "") == "center";
        });
    auto prettyName = res->find("name");
    if (prettyName != res->end()) {
        std::wstring wstrPrettyName = Util::StrToWstr(*prettyName);
        this->prettyName = wstrPrettyName;
        return TRUE;
    }
    else {
        return FALSE;
    }
}

BOOL Item::SetPrettyName(std::wstring nameToSet) {
    this->prettyName = nameToSet;
    return TRUE;
}

std::wstring Item::GetHashName() {
    return this->hashName;
}

POINT3D Item::GetPos() {
    return this->pos;
}

float Item::GetX() {
    return this->pos.x;
}

float Item::GetY() {
    return this->pos.y;
}

float Item::GetZ() {
    return this->pos.z;
}


uint64_t Item::GetItemProfileAddr() {
    return this->ma.Readuint64_tMemory(addr + ItemProfileAddr);
}

ItemProfile Item::GetItemProfile() {
    return ItemProfile::ItemProfile(GetItemProfileAddr(), ma);
}

// Class ItemProfile
ItemProfile::ItemProfile() {
    this->addr = 0;
}

ItemProfile::ItemProfile(uint64_t addr, MemoryAccess ma) {
    this->addr = addr;
    this->ma = ma;
}

uint64_t ItemProfile::GetAddr() {
    return this->addr;
}

uint64_t ItemProfile::GetItemInfoAddr() {
    return this->ma.Readuint64_tMemory(addr + ItemInfoAddr);
}

ItemBasicInformation ItemProfile::GetItemInfo() {
    return ItemBasicInformation::ItemBasicInformation(GetItemInfoAddr(), ma);
}

uint64_t ItemProfile::GetItemStatsAddr() {
    return this->ma.Readuint64_tMemory(addr + ItemStatsAddr);
}

ItemStats ItemProfile::GetItemStats() {
    return ItemStats::ItemStats(GetItemStatsAddr(), ma);
}

// Class ItemBasicInformation
ItemBasicInformation::ItemBasicInformation() {
    this->addr = 0;
    this->name = L"";
}

ItemBasicInformation::ItemBasicInformation(uint64_t addr, MemoryAccess ma) {
    this->addr = addr;
    this->ma = ma;
    this->name = L"";
}

uint64_t ItemBasicInformation::GetAddr() {
    return this->addr;
}

std::wstring ItemBasicInformation::GetName() {
    return this->name;
}

uint64_t ItemBasicInformation::GetItemLocalizationAddress() {
    return this->ma.Readuint64_tMemory(addr + ItemLocalizationAddr);
}

ItemLocalization ItemBasicInformation::GetItemLocalization() {
    return ItemLocalization::ItemLocalization(GetItemLocalizationAddress(), ma);
}

// Class ItemLocalization
ItemLocalization::ItemLocalization() {
    this->addr = 0;
}

ItemLocalization::ItemLocalization(uint64_t addr, MemoryAccess ma) {
    this->addr = addr;
    this->ma = ma;
}

uint64_t ItemLocalization::GetAddr() {
    return this->addr;
}

uint64_t ItemLocalization::GetItemCoordsAddr() {
    return this->ma.Readuint64_tMemory(addr + ItemCoordsAddr);
}

ItemCoordinates ItemLocalization::GetItemCoords() {
    return ItemCoordinates::ItemCoordinates(GetItemCoordsAddr(), ma);
}

// Class ItemCoordinates
ItemCoordinates::ItemCoordinates() {
    this->addr = 0;
}

ItemCoordinates::ItemCoordinates(uint64_t addr, MemoryAccess ma) {
    this->addr = addr;
    this->ma = ma;
}

uint64_t ItemCoordinates::GetAddr() {
    return this->addr;
}

uint64_t ItemCoordinates::GetItemLocationContainerAddr() {
    return this->ma.Readuint64_tMemory(addr + ItemLocationContainerAddr);
}

ItemLocationContainer ItemCoordinates::GetItemLocationContainer() {
    return ItemLocationContainer::ItemLocationContainer(GetItemLocationContainerAddr(), ma);
}

// Class ItemLocationContainer
ItemLocationContainer::ItemLocationContainer() {
    this->addr = 0;
    this->position = { 0,0,0 };
}

ItemLocationContainer::ItemLocationContainer(uint64_t addr, MemoryAccess ma) {
    this->addr = addr;
    this->ma = ma;
    POINT3D pos = ma.ReadVector3Memory(addr + ItemPositionAddr);
    this->position = { pos.x, pos.z, pos.y };
}

uint64_t ItemLocationContainer::GetAddr() {
    return this->addr;
}

POINT3D ItemLocationContainer::GetItemPosition() {
    return this->position;
}

// Class ItemTemplate
ItemTemplate::ItemTemplate() {
    this->addr = 0;
    this->name = L"";
}

ItemTemplate::ItemTemplate(uint64_t addr, MemoryAccess ma) {
    this->addr = addr;
    this->ma = ma;
}

uint64_t ItemTemplate::GetAddr() {
    return this->addr;
}

std::wstring ItemTemplate::GetName() {
    return  this->ma.ReadUnityString(addr + ItemTemplateNameAddr, 0);
}

std::wstring ItemTemplate::GetHashName() {
    return this->ma.ReadUnityString(addr + ItemTemplateHashNameAddr, 0);
}

// Class GameItem

GameItem::GameItem() {
    this->addr = 0;
}

GameItem::GameItem(uint64_t addr, MemoryAccess ma) {
    this->addr = addr;
    this->ma = ma;

}

uint64_t GameItem::GetAddr() {
    return this->addr;
}

uint64_t GameItem::GetItemTemplateAddr() {
    return this->ma.Readuint64_tMemory(addr + ItemTemplateAddr);
}

ItemTemplate GameItem::GetItemTemplate() {
    return ItemTemplate::ItemTemplate(GetItemTemplateAddr(), ma);
}

// ItemStats
ItemStats::ItemStats() {
    this->addr = 0;
}

ItemStats::ItemStats(uint64_t addr, MemoryAccess ma) {
    this->addr = addr;
    this->ma = ma;
}

uint64_t ItemStats::GetAddr() {
    return this->addr;
}

uint64_t ItemStats::GetGameItemAddr() {
    return this->ma.Readuint64_tMemory(addr + GameItemAddr);
}

GameItem ItemStats::GetGameItem() {
    return GameItem::GameItem(GetGameItemAddr(), ma);
}
