#include "BetaStar.h"

using namespace sc2;

void BetaStar::OnGameStart() {
    const ObservationInterface* observation = Observation();
    std::cout << "I am player number " << observation->GetPlayerID() << std::endl;
}

// called each time the coordinator steps the simulation forward
void BetaStar::OnStep() {

    // interface that contains functions to examine the current game state
    //const ObservationInterface* observation = Observation();

    // issues actions to units
    //ActionInterface* action = Actions();

    //TryBuildSupplyDepot();

    std::cout << "Building Refineries" << std::endl;
    BuildRefineries();

    //TryBuildBarracks();
}

// Called each time a unit has been built and has no orders or the unit had orders in the previous step and currently does not
// Both buildings and units are considered units and are represented with a Unit object.
void BetaStar::OnUnitIdle(const Unit* unit) {
    switch (unit->unit_type.ToType()) {
        case UNIT_TYPEID::TERRAN_COMMANDCENTER: {
            if (NeedWorkers()) {
                Actions()->UnitCommand(unit, ABILITY_ID::TRAIN_SCV);
                break;
            }
            break;
        }
        case UNIT_TYPEID::TERRAN_SCV: {
            const Unit* mineral_target = FindNearestNeutralUnit(unit->pos, UNIT_TYPEID::NEUTRAL_MINERALFIELD);
            if (!mineral_target) {
                break;
            }
            // The ability type of SMART is equivalent to a right click in Starcraft 2 when you have a unit selected.
            // Right clicking a mineral patch with an SCV commands it to mine from that patch
            Actions()->UnitCommand(unit, ABILITY_ID::SMART, mineral_target);
            break;
        }
        case UNIT_TYPEID::TERRAN_BARRACKS: {
            Actions()->UnitCommand(unit, ABILITY_ID::TRAIN_MARINE);
            break;
        }
        case UNIT_TYPEID::TERRAN_MARINE: {
            const GameInfo& game_info = Observation()->GetGameInfo();
            Actions()->UnitCommand(unit, ABILITY_ID::ATTACK_ATTACK, game_info.enemy_start_locations.front());
            break;
        }
        default: {
            break;
        }
    }
}
