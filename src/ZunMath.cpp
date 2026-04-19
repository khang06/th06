#include "ZunMath.hpp"
#include "AnmManager.hpp"

void ZunViewport::Set()
{
    i32 viewportWidth = this->width * WIDTH_RESOLUTION_SCALE;
    i32 viewportHeight = this->height * HEIGHT_RESOLUTION_SCALE;
    g_glFuncTable.glViewport(this->x * WIDTH_RESOLUTION_SCALE + VIEWPORT_OFF_X,
                             (GAME_WINDOW_HEIGHT_REAL - ((this->y + this->height) * HEIGHT_RESOLUTION_SCALE)) -
                                 VIEWPORT_OFF_Y,
                             viewportWidth, viewportHeight);
    g_glFuncTable.glDepthRangef(this->minZ, this->maxZ);

    g_AnmManager->gfxBackend->SetInvViewport(1.0f / viewportWidth, 1.0f / viewportHeight);
}
