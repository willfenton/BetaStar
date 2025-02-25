#pragma once;

using namespace sc2;

#include <iostream>
#include "sc2api/sc2_api.h"
#include "sc2api/sc2_args.h"
#include "sc2lib/sc2_lib.h"
#include "sc2utils/sc2_manage_process.h"
#include "sc2utils/sc2_arg_parser.h"

class HumanBot : public sc2::Agent {
public:
    virtual void OnGameStart() final {
        const sc2::ObservationInterface* observation = Observation();
        std::cout << "I am player number " << observation->GetPlayerID() << std::endl;
    };

    virtual void OnStep() final {
    };

    virtual void OnBuildingConstructionComplete(const Unit* unit)
    {
        std::cout << UnitTypeToName(unit->unit_type) << " built by HOOMAN at: (" << unit->pos.x << "," << unit->pos.y << ")" << std::endl;
    }

private:
};

static Difficulty GetDifficultyFromString(const std::string &InDifficulty)
{
	if (InDifficulty == "VeryEasy")
	{
		return Difficulty::VeryEasy;
	}
	if (InDifficulty == "Easy")
	{
		return Difficulty::Easy;
	}
	if (InDifficulty == "Medium")
	{
		return Difficulty::Medium;
	}
	if (InDifficulty == "MediumHard")
	{
		return Difficulty::MediumHard;
	}
	if (InDifficulty == "Hard")
	{
		return Difficulty::Hard;
	}
	if (InDifficulty == "HardVeryHard")
	{
		return Difficulty::HardVeryHard;
	}
	if (InDifficulty == "VeryHard")
	{
		return Difficulty::VeryHard;
	}
	if (InDifficulty == "CheatVision")
	{
		return Difficulty::CheatVision;
	}
	if (InDifficulty == "CheatMoney")
	{
		return Difficulty::CheatMoney;
	}
	if (InDifficulty == "CheatInsane")
	{
		return Difficulty::CheatInsane;
	}

	return Difficulty::Easy;
}

static Race GetRaceFromString(const std::string & RaceIn)
{
	std::string race(RaceIn);
	std::transform(race.begin(), race.end(), race.begin(), ::tolower);

	if (race == "terran")
	{
		return Race::Terran;
	}
	else if (race == "protoss")
	{
		return Race::Protoss;
	}
	else if (race == "zerg")
	{
		return Race::Zerg;
	}
	else if (race == "random")
	{
		return Race::Random;
	}

	return Race::Random;
}

struct ConnectionOptions
{
	int32_t GamePort;
	int32_t StartPort;
	std::string ServerAddress;
	bool ComputerOpponent;
	Difficulty ComputerDifficulty;
	Race ComputerRace;
    Race HumanRace;
	std::string OpponentId;
};

static void ParseArguments(int argc, char *argv[], ConnectionOptions &connect_options)
{
	ArgParser arg_parser(argv[0]);
	arg_parser.AddOptions({
		{ "-g", "--GamePort", "Port of client to connect to", false },
		{ "-o", "--StartPort", "Starting server port", false },
		{ "-l", "--LadderServer", "Ladder server address", false },
		{ "-c", "--ComputerOpponent", "If we set up a computer oppenent" },
		{ "-a", "--ComputerRace", "Race of computer oppent"},
        { "-hr", "--HumanRace", "Race of human opponent"},
		{ "-d", "--ComputerDifficulty", "Difficulty of computer oppenent"},
		{ "-x", "--OpponentId", "PlayerId of opponent"}
		});
	arg_parser.Parse(argc, argv);
	std::string GamePortStr;
	if (arg_parser.Get("GamePort", GamePortStr)) {
		connect_options.GamePort = atoi(GamePortStr.c_str());
	}
	std::string StartPortStr;
	if (arg_parser.Get("StartPort", StartPortStr)) {
		connect_options.StartPort = atoi(StartPortStr.c_str());
	}
	arg_parser.Get("LadderServer", connect_options.ServerAddress);
	std::string CompOpp;
	if (arg_parser.Get("ComputerOpponent", CompOpp))
	{
		connect_options.ComputerOpponent = true;
		std::string CompRace;
		if (arg_parser.Get("ComputerRace", CompRace))
		{
			connect_options.ComputerRace = GetRaceFromString(CompRace);
		}
		std::string CompDiff;
		if (arg_parser.Get("ComputerDifficulty", CompDiff))
		{
			connect_options.ComputerDifficulty = GetDifficultyFromString(CompDiff);
		}

	}
	else
	{
		connect_options.ComputerOpponent = false;
        std::string HumanRace;
        if (arg_parser.Get("HumanRace", HumanRace))
        {
            connect_options.HumanRace = GetRaceFromString(HumanRace);
        }
        else
        {
            connect_options.HumanRace = Race::Random;
        }
	}
	arg_parser.Get("OpponentId", connect_options.OpponentId);
}

static void RunBot(int argc, char *argv[], Agent *Agent, Race race)
{
	ConnectionOptions Options;
	ParseArguments(argc, argv, Options);

	Coordinator coordinator;
    coordinator.LoadSettings(argc, argv);

	// Add the custom bot, it will control the players.
	int num_agents;
	if (Options.ComputerOpponent)
	{
		num_agents = 1;
        coordinator.SetRealtime(false);
		coordinator.SetParticipants({
			CreateParticipant(race, Agent),
			CreateComputer(Options.ComputerRace, Options.ComputerDifficulty)
			});
	}
	else
	{
		num_agents = 2;
        coordinator.SetRealtime(true);
        coordinator.SetParticipants({
            CreateParticipant(Options.HumanRace, new HumanBot()),
            CreateParticipant(race, Agent)
			});
	}

	// Start the game.
    std::cout << "Comp/Race/Diff: " << Options.ComputerOpponent << Options.ComputerRace << Options.ComputerDifficulty << std::endl;

	 //Step forward the game simulation.
	//std::cout << "Connecting to port " << Options.GamePort << std::endl;
	//coordinator.Connect(Options.GamePort);
	//coordinator.SetupPorts(num_agents, Options.StartPort, false);

	// Step forward the game simulation.
	//coordinator.JoinGame();
	//coordinator.SetTimeoutMS(10000);
	//std::cout << " Successfully joined game" << std::endl;

    coordinator.LaunchStarcraft();
    coordinator.StartGame("CactusValleyLE.SC2Map");
	while (coordinator.Update()) {
	}
}

