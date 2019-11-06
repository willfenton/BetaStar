#include "BetaStar.h"

using namespace sc2;


void BetaStar::OnGameStart() {
    const ObservationInterface* observation = Observation();
    std::cout << "I am player number " << observation->GetPlayerID() << std::endl;

    // get position of first command center
    for (const auto& base : FriendlyUnitsOfType(UNIT_TYPEID::TERRAN_COMMANDCENTER)) {
        starting_pos = base->pos;
    }
}

// called each time the coordinator steps the simulation forward
void BetaStar::OnStep() {

    const ObservationInterface* observation = Observation();

    TrainWorkers();

    TryBuildSupplyDepot();

    BuildRefineries();

    ManageWorkers(UNIT_TYPEID::TERRAN_SCV, ABILITY_ID::HARVEST_GATHER_SCV, UNIT_TYPEID::TERRAN_REFINERY);

    // TODO: stop this from running all the time, scout enemy
    if (CountUnitType(UNIT_TYPEID::TERRAN_MARINE) >= 30) {
        const GameInfo& game_info = Observation()->GetGameInfo();
        for (const auto& marine : FriendlyUnitsOfType(UNIT_TYPEID::TERRAN_MARINE)) {
            Actions()->UnitCommand(marine, ABILITY_ID::ATTACK_ATTACK, game_info.enemy_start_locations.front());
        }
    }

    //TryExpand(ABILITY_ID::BUILD_COMMANDCENTER, UNIT_TYPEID::TERRAN_SCV);

    TryBuildBarracks();
}

// Called each time a unit has been built and has no orders or the unit had orders in the previous step and currently does not
// Both buildings and units are considered units and are represented with a Unit object.
void BetaStar::OnUnitIdle(const Unit* unit) {

    switch (unit->unit_type.ToType()) {

        case UNIT_TYPEID::TERRAN_COMMANDCENTER: {
            //if (Observation()->GetMinerals() >= 50 && std::max(0, Observation()->GetFoodCap() - Observation()->GetFoodUsed()) != 0 && NeedWorkers()) {
            //    Actions()->UnitCommand(unit, ABILITY_ID::TRAIN_SCV);
            //    break;
            //}
            //break;
        }

        case UNIT_TYPEID::TERRAN_SCV: {
            const Unit* target = FindResourceToGather(unit->pos);
            Actions()->UnitCommand(unit, ABILITY_ID::HARVEST_GATHER, target);
            break;
        }

        case UNIT_TYPEID::TERRAN_BARRACKS: {
            Actions()->UnitCommand(unit, ABILITY_ID::TRAIN_MARINE);
            break;
        }

        case UNIT_TYPEID::TERRAN_MARINE: {
            //const GameInfo& game_info = Observation()->GetGameInfo();
            //Actions()->UnitCommand(unit, ABILITY_ID::ATTACK_ATTACK, game_info.enemy_start_locations.front());
            break;
        }

        default: {
            //std::cout << "Idle unit of type " << unit->unit_type.to_string() << std::endl;
            break;
        }
    }
}
