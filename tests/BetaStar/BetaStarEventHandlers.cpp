#include "BetaStar.h"

using namespace sc2;


void BetaStar::OnGameStart() {
    const ObservationInterface* observation = Observation();
    std::cout << "I am player number " << observation->GetPlayerID() << std::endl;

    // get position of first command center
    for (const auto& base : FriendlyUnitsOfType(UNIT_TYPEID::PROTOSS_NEXUS)) {
        starting_pos = base->pos;
    }
}

// called each time the coordinator steps the simulation forward
void BetaStar::OnStep() {

    const ObservationInterface* observation = Observation();

    TrainWorkers();

    TryBuildSupplyDepot();

    BuildGas();

    ManageWorkers(UNIT_TYPEID::PROTOSS_PROBE, ABILITY_ID::HARVEST_GATHER_PROBE, UNIT_TYPEID::PROTOSS_ASSIMILATOR);

    TryExpand(ABILITY_ID::BUILD_NEXUS, UNIT_TYPEID::PROTOSS_PROBE);
}

// Called each time a unit has been built and has no orders or the unit had orders in the previous step and currently does not
// Both buildings and units are considered units and are represented with a Unit object.
void BetaStar::OnUnitIdle(const Unit* unit) {

    switch (unit->unit_type.ToType()) {

        case UNIT_TYPEID::PROTOSS_PROBE: {
            const Unit* target = FindResourceToGather(unit->pos);
            Actions()->UnitCommand(unit, ABILITY_ID::HARVEST_GATHER, target);
            break;
        }

        default: {
            //std::cout << "Idle unit of type " << unit->unit_type.to_string() << std::endl;
            break;
        }
    }
}

void BetaStar::OnBuildingConstructionComplete(const Unit* unit) {
    switch (unit->unit_type.ToType()) {

        case UNIT_TYPEID::PROTOSS_NEXUS: {
            building_nexus = false;
            break;
        }
    }
}
