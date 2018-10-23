#include "Command.hpp"

Command SpawnCommand()
{
	return "g";
}

Command MoveCommand(EntityID id, Direction direction)
{
	return "m " + std::to_string(id) + ' ' + static_cast<char>(direction);
}

Command TransformShipIntoDropoffCommand(EntityID id)
{
	return "c " + std::to_string(id);
}
