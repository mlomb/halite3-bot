#include "Types.hpp"

#include "Game.hpp"

std::mt19937_64& mt()
{
	thread_local static std::random_device srd;
	thread_local static std::mt19937_64 smt(srd());
	return smt;
}
