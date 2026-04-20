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

#ifndef _S_ISTYPE
#define _S_ISTYPE(mode, mask) (((mode) & _S_IFMT) == (mask))
#define S_ISDIR(mode) _S_ISTYPE((mode), _S_IFDIR)
#endif

static std::vector<std::string> g_ReplayQueue;

namespace ReplayValidator
{

bool Init(const char *path)
{
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

    return true;
}

ChainCallbackResult OnModeChange()
{
    if (g_ReplayQueue.empty())
    {
        std::printf("Done with all replays!\n");
        return CHAIN_CALLBACK_RESULT_EXIT_GAME_SUCCESS;
    }

    std::string nextReplay = std::move(g_ReplayQueue.back());
    g_ReplayQueue.pop_back();

    ReplayHeader *replayHeader = (ReplayHeader *)FileSystem::OpenPath(nextReplay.c_str(), 1);
    if (!replayHeader)
    {
        std::printf("Failed to parse replay %s!\n", nextReplay.c_str());
        return CHAIN_CALLBACK_RESULT_EXIT_GAME_SUCCESS;
    }

    if (ReplayManager::ValidateReplayData(replayHeader, g_LastFileSize) != ZUN_SUCCESS)
    {
        std::printf("Replay %s is invalid!\n", nextReplay.c_str());
        return CHAIN_CALLBACK_RESULT_EXIT_GAME_SUCCESS;
    }

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
        return false;
    }
    return true;
}

} // namespace ReplayValidator