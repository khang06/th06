#include <vector>
#include "Chain.hpp"

namespace ReplayValidator
{

bool Init(const char* path);
ChainCallbackResult OnModeChange();
bool VerifyStageEndState();

}