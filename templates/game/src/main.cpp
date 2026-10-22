#include "Game.hpp"

// --help lists the options every engine game has: window size, preferences
// file, and automated runs (--frames 120 --lockstep --screenshot shot.png).
int main(int argc, char** argv)
{
    return CnaEngine::App::RunGame<@GAME_CLASS@>(argc, argv);
}
