#include "Types.hpp"

#include "Game.hpp"

std::mt19937_64& mt()
{
	thread_local static std::random_device srd;
	thread_local static std::mt19937_64 smt(srd());
	return smt;
}

std::uniform_real_distribution<double> rand_01()
{
	thread_local static  std::uniform_real_distribution<double> rng(0.0, 1.0);
	return rng;
}

double GetRandom01()
{
	return rand_01()(mt());
}

void SetRandom01Seed(long long seed)
{
	mt().seed(seed);
}
