#include "BetaStar.h"

#include "LadderInterface.h"

using namespace sc2;

int main(int argc, char* argv[])
{
    Agent* agent = new BetaStar();
	RunBot(argc, argv, agent, Race::Terran);
}
