#include "ReplayValidator.hpp"

#include "FileSystem.hpp"
#include "GameManager.hpp"
#include "ReplayData.hpp"
#include "ReplayManager.hpp"
#include "utils.hpp"

#include <dirent.h>
#include <string>
#include <sys/stat.h>
#include <vector>

// TODO: Use a proper Win32 define...
#ifdef WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "Ws2_32.lib")
#endif

#ifndef _S_ISTYPE
#define _S_ISTYPE(mode, mask) (((mode) & _S_IFMT) == (mask))
#define S_ISDIR(mode) _S_ISTYPE((mode), _S_IFDIR)
#endif

struct DesyncResult
{
    int stage;
    int expectedScore;
    int actualScore;
};

static bool g_TcpMode;
static std::vector<std::string> g_ReplayQueue;
static SOCKET g_TcpSocket = INVALID_SOCKET;
static DesyncResult g_DesyncResult;
static bool g_FirstRun = true;

namespace ReplayValidator
{

bool Init(const char *path)
{
    if (!strncmp(path, "tcp:", 4))
    {
        g_TcpMode = true;

#ifdef WIN32_LEAN_AND_MEAN
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData))
        {
            std::printf("Failed to init Winsock!\n");
            return false;
        }
#endif

        struct addrinfo hints = {
            .ai_family = AF_UNSPEC,
            .ai_socktype = SOCK_STREAM,
            .ai_protocol = IPPROTO_TCP,
        };
        struct addrinfo *addrOut;
        if (getaddrinfo("127.0.0.1", &path[4], &hints, &addrOut))
        {
            std::printf("Failed to resolve address for TCP!\n");
            return false;
        }

        g_TcpSocket = socket(addrOut->ai_family, addrOut->ai_socktype, addrOut->ai_protocol);
        if (g_TcpSocket == INVALID_SOCKET)
        {
            std::printf("Failed to create socket for TCP!\n");
            freeaddrinfo(addrOut);
            return false;
        }

        if (connect(g_TcpSocket, addrOut->ai_addr, (int)addrOut->ai_addrlen) == SOCKET_ERROR)
        {
            std::printf("Failed to connect to TCP server!\n");
            freeaddrinfo(addrOut);
            return false;
        }
    }
    else
    {
        g_TcpMode = false;

        struct stat pathStat;
        if (stat(path, &pathStat))
        {
            std::printf("Failed to open replay path!\n");
            return false;
        }

        if (S_ISDIR(pathStat.st_mode))
        {
            std::printf("Getting replays from directory...\n");

            DIR *dir = opendir(path);
            if (!dir)
            {
                std::printf("Failed to open replay directory!\n");
                return false;
            }

            struct dirent *entry;
            while ((entry = readdir(dir)))
            {
                if (entry->d_name[0] == '.')
                {
                    continue;
                }

                std::printf("Adding %s\n", entry->d_name);
                g_ReplayQueue.emplace_back(std::string(path) + "/" + entry->d_name);
            }

            closedir(dir);
        }
        else
        {
            g_ReplayQueue.emplace_back(path);
        }
    }

    return true;
}

static bool GetNextReplay(std::string& ret)
{
    if (g_TcpMode)
    {
        if (!g_FirstRun)
        {
            if (send(g_TcpSocket, (const char *)&g_DesyncResult, sizeof(g_DesyncResult), 0) == SOCKET_ERROR)
            {
                std::printf("Failed to send result to TCP server!\n");
                return false;
            }
        }
        g_FirstRun = false;

        char buf[256];
        int bytesRead = recv(g_TcpSocket, buf, sizeof(buf) - 1, 0);
        if (bytesRead <= 0)
        {
            std::printf("Failed to read from TCP socket!\n");
            return false;
        }
        buf[bytesRead] = '\0';
        ret = std::string(buf);
        return true;
    }
    else
    {
        if (g_ReplayQueue.empty())
        {
            return false;
        }

        ret = std::move(g_ReplayQueue.back());
        g_ReplayQueue.pop_back();
        return true;
    }
}

ChainCallbackResult OnModeChange()
{
    std::string nextReplay;
    ReplayHeader *replayHeader;

    while (true)
    {
        if (!GetNextReplay(nextReplay))
        {
            std::printf("Done with all replays!\n");
            return CHAIN_CALLBACK_RESULT_EXIT_GAME_SUCCESS;
        }

        replayHeader = (ReplayHeader *)FileSystem::OpenPath(nextReplay.c_str(), 1);
        if (!replayHeader)
        {
            std::printf("Failed to open replay %s!\n", nextReplay.c_str());
            g_DesyncResult.stage = -1;
            continue;
        }

        if (ReplayManager::ValidateReplayData(replayHeader, g_LastFileSize) != ZUN_SUCCESS)
        {
            std::printf("Replay %s is invalid!\n", nextReplay.c_str());
            g_DesyncResult.stage = -1;
            continue;
        }

        break;
    }
    g_DesyncResult.stage = 0;

    StageReplayData *stageReplayData[7] = {};
    for (int cur = 0; cur < ARRAY_SIZE_SIGNED(stageReplayData); cur++)
    {
        if (replayHeader->stageReplayDataOffsets[cur] != 0)
        {
            stageReplayData[cur] = (StageReplayData *)((u8 *)replayHeader + replayHeader->stageReplayDataOffsets[cur]);
        }
        else
        {
            stageReplayData[cur] = nullptr;
        }
    }

    int stageNum = 0;
    while (!stageReplayData[stageNum])
    {
        stageNum++;
    }

    g_GameManager.isInReplay = 1;
    g_Supervisor.framerateMultiplier = 1.0;
    std::strcpy((char *)g_GameManager.replayFile, nextReplay.c_str());
    g_GameManager.difficulty = (Difficulty)replayHeader->difficulty;
    g_GameManager.character = replayHeader->shottypeChara / 2;
    g_GameManager.shotType = replayHeader->shottypeChara % 2;
    g_GameManager.livesRemaining = stageReplayData[stageNum]->livesRemaining;
    g_GameManager.bombsRemaining = stageReplayData[stageNum]->bombsRemaining;
    std::free(replayHeader);
    g_GameManager.currentStage = stageNum;
    g_Supervisor.curState = SUPERVISOR_STATE_GAMEMANAGER;
    g_Supervisor.wantedState = SUPERVISOR_STATE_GAMEMANAGER;
    g_Supervisor.wantedState2 = SUPERVISOR_STATE_GAMEMANAGER;

    if (GameManager::RegisterChain() != ZUN_SUCCESS)
    {
        std::printf("Failed to init GameManager!\n");
        return CHAIN_CALLBACK_RESULT_EXIT_GAME_SUCCESS;
    }

    std::printf("Playing replay %s...\n", nextReplay.c_str());
    return CHAIN_CALLBACK_RESULT_CONTINUE;
}

bool VerifyStageEndState()
{
    StageReplayData *stageReplayData = g_ReplayManager->replayData->stageReplayData[g_GameManager.currentStage - 1];
    if (stageReplayData->score != g_GameManager.score)
    {
        std::printf("DESYNC ON STAGE %d! Expected %u, got %i\n", g_GameManager.currentStage, stageReplayData->score,
                    g_GameManager.score);
        if (g_DesyncResult.stage <= 0)
        {
            g_DesyncResult.stage = g_GameManager.currentStage;
            g_DesyncResult.expectedScore = stageReplayData->score;
            g_DesyncResult.actualScore = g_GameManager.score;
        }
        return false;
    }
    return true;
}

} // namespace ReplayValidator