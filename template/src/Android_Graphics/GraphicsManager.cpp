#include "GraphicsManager.h"
#include "OpenGLGraphics.h"

std::unique_ptr<AndroidImgui> GraphicsManager::getGraphicsInterface(GraphicsAPI api) {
    (void)api;
    return std::make_unique<OpenGLGraphics>();
}

