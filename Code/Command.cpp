#include "Command.hpp"

#include "Game.hpp"

Command SpawnCommand()
{
	return "g";
}

Command MoveCommand(EntityID id, Direction direction)
{
#ifdef HALITE_LOCAL
	Ship* s = Game::Get()->GetMyPlayer().ships[id];
	out::LogShip(s->ship_id, {
		{ "task_type", s->task.type },
		{ "task_x", s->task.position.x },
		{ "task_y", s->task.position.y },
		{ "task_priority", s->task.priority },
		{ "navigation_order", 0 }
	});
#endif
	return "m " + std::to_string(id) + ' ' + static_cast<char>(direction);
}

Command TransformShipIntoDropoffCommand(EntityID id)
{
	return "c " + std::to_string(id);
}
