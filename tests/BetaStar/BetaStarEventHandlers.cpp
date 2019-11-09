#include "BetaStar.h"

using namespace sc2;


// called each time the coordinator steps the simulation forward
void BetaStar::OnStep() {

    OnStepComputeStatistics();

    OnStepTrainWorkers();

    OnStepBuildPylons();

    OnStepBuildGas();

    OnStepExpand();

    OnStepManageWorkers();

    //TrainWorkers();

    //TryBuildSupplyDepot();

    //BuildGas();

    ManageWorkers(UNIT_TYPEID::PROTOSS_PROBE, ABILITY_ID::HARVEST_GATHER_PROBE, UNIT_TYPEID::PROTOSS_ASSIMILATOR);

    //TryExpand(ABILITY_ID::BUILD_NEXUS, UNIT_TYPEID::PROTOSS_PROBE);
}

void BetaStar::OnGameStart() {
    const ObservationInterface* observation = Observation();
    std::cout << "I am player number " << observation->GetPlayerID() << std::endl;

    // get position of first command center
    for (const auto& base : FriendlyUnitsOfType(UNIT_TYPEID::PROTOSS_NEXUS)) {
        m_starting_pos = base->pos;
    }

    expansion_locations = search::CalculateExpansionLocations(Observation(), Query());
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
            m_building_nexus = false;
            break;
        }
    }
}
