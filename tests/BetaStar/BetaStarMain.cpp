#include "BetaStar.h"

#include "LadderInterface.h"

int main(int argc, char* argv[])
{
    sc2::Agent* agent = new BetaStar();
	RunBot(argc, argv, agent, sc2::Race::Terran);
}
