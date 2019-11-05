#include "BetaStar.h"

void BetaStar::OnGameStart() {
    const sc2::ObservationInterface* observation = Observation();
    std::cout << "I am player number " << observation->GetPlayerID() << std::endl;
}

// called each time the coordinator steps the simulation forward
void BetaStar::OnStep() {

    // interface that contains functions to examine the current game state
    //const sc2::ObservationInterface* observation = Observation();

    // issues actions to units
    //sc2::ActionInterface* action = Actions();

    TryBuildSupplyDepot();

    TryBuildBarracks();
}

// Called each time a unit has been built and has no orders or the unit had orders in the previous step and currently does not
// Both buildings and units are considered units and are represented with a Unit object.
void BetaStar::OnUnitIdle(const sc2::Unit* unit) {
    switch (unit->unit_type.ToType()) {
    case sc2::UNIT_TYPEID::TERRAN_COMMANDCENTER: {
        Actions()->UnitCommand(unit, sc2::ABILITY_ID::TRAIN_SCV);
        break;
    }
    case sc2::UNIT_TYPEID::TERRAN_SCV: {
        const sc2::Unit* mineral_target = FindNearestNeutralUnit(unit->pos, sc2::UNIT_TYPEID::NEUTRAL_MINERALFIELD);
        if (!mineral_target) {
            break;
        }
        // The ability type of SMART is equivalent to a right click in Starcraft 2 when you have a unit selected.
        // Right clicking a mineral patch with an SCV commands it to mine from that patch
        Actions()->UnitCommand(unit, sc2::ABILITY_ID::SMART, mineral_target);
        break;
    }
    case sc2::UNIT_TYPEID::TERRAN_BARRACKS: {
        Actions()->UnitCommand(unit, sc2::ABILITY_ID::TRAIN_MARINE);
        break;
    }
    case sc2::UNIT_TYPEID::TERRAN_MARINE: {
        const sc2::GameInfo& game_info = Observation()->GetGameInfo();
        Actions()->UnitCommand(unit, sc2::ABILITY_ID::ATTACK_ATTACK, game_info.enemy_start_locations.front());
        break;
    }
    default: {
        break;
    }
    }
}
