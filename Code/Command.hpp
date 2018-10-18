#pragma once

#include <string>

#include "Types.hpp"

Command SpawnCommand();
Command MoveCommand(EntityID id, Direction direction);