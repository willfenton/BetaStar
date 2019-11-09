#pragma once;

#include <iostream>
#include "sc2api/sc2_api.h"
#include "sc2api/sc2_args.h"
#include "sc2lib/sc2_lib.h"
#include "sc2utils/sc2_manage_process.h"
#include "sc2utils/sc2_arg_parser.h"

using namespace sc2;

/*
    BetaStar agent class
    This is our bot, the Coordinator calls our public Event-handling functions according to what happens in the game.
*/
class BetaStar : public Agent {
public:
    // this function runs at the start of the game
    virtual void OnGameStart() final;

    // called each time the coordinator steps the simulation forward
    virtual void OnStep() final;

    // Called each time a unit has been built and has no orders or the unit had orders in the previous step and currently does not
    // Both buildings and units are considered units and are represented with a Unit object.
    virtual void BetaStar::OnUnitIdle(const Unit* unit);

    // called each a building is finished construction
    virtual void BetaStar::OnBuildingConstructionComplete(const Unit* unit) final;

private:

    /* ON-STEP FUNCTIONS */

    void OnStepComputeStatistics();

    void OnStepTrainWorkers();

    void OnStepBuildPylons();

    void OnStepBuildGas();

    void OnStepExpand();

    void OnStepManageWorkers();

    /* UTILITY FUNCTIONS */

    const size_t CountUnitType(UnitTypeID unit_type) const;

    bool TryBuildStructure(AbilityID ability_type_for_structure, UnitTypeID unit_type);

    bool TryBuildStructure(AbilityID ability_type_for_structure, UnitTypeID unit_type, Tag location_tag);

    bool TryBuildSupplyDepot();

    void BuildGas();

    bool TryBuildGas(Point2D base_location);

    bool NeedWorkers();

    const Unit* FindNearestNeutralUnit(const Point2D& start, UnitTypeID target_unit_type);

    const Units FriendlyUnitsOfType(UnitTypeID unit_type) const;

    const Unit* FindResourceToGather(Point2D unit_pos);

    void ManageWorkers(UnitTypeID worker_type, AbilityID worker_gather_command, UnitTypeID vespene_building_type);

    void MineIdleWorkers(const Unit* worker, AbilityID worker_gather_command, UnitTypeID vespene_building_type);

    bool TryExpand(AbilityID build_ability, UnitTypeID worker_type);

    void TrainWorkers();

    // Returns the UnitTypeID of the unit that builds the specified unit
    // Example: Terran Command Center for SCV and SCV for Terran Command Center
    UnitTypeID GetUnitBuilder(UnitTypeID unitToBuild);

    // Returns the AbilityID of the ability a builder will need to use to build the specified unit
    AbilityID GetUnitBuildAbility(UnitTypeID unitToBuild);

    // Attempts to train unit of unitType at random, valid building
    // Returns true if successful, false otherwise
    bool TrainUnit(UnitTypeID unitType);

    // Attempts to train unit of unitType at specified building
    // Returns true if successful, false otherwise
    bool TrainUnit(const Unit* building, UnitTypeID unitType);

    // Attempts to train one unit of unitType at all valid buildings
    // Returns number of units trained this way
    size_t TrainUnitMultiple(UnitTypeID unitType);

    // Attempts to train one unit of unitType at specified buildings
    // Returns number of units trained this way
    size_t TrainUnitMultiple(const Units &buildings, UnitTypeID unitType);


    /* MEMBER DATA */

    // position of our starting base
    Point3D m_starting_pos;

    // how much supply we have left, updated each step
    int m_supply_left = 0;

    // whether we are currently building a nexus (don't consider expanding if true)
    bool m_building_nexus = false;

    // all expansion locations
    std::vector<Point3D> expansion_locations;

    // common unit ids
    const UnitTypeID m_base_typeid =               UNIT_TYPEID::PROTOSS_NEXUS;
    const UnitTypeID m_worker_typeid =             UNIT_TYPEID::PROTOSS_PROBE;
    const UnitTypeID m_supply_building_typeid =    UNIT_TYPEID::PROTOSS_PYLON;
    const UnitTypeID m_gas_building_typeid =       UNIT_TYPEID::PROTOSS_ASSIMILATOR;

    // common ability ids
    const AbilityID m_base_building_abilityid =    ABILITY_ID::BUILD_NEXUS;
    const AbilityID m_worker_train_abilityid =     ABILITY_ID::TRAIN_PROBE;
    const AbilityID m_worker_gather_abilityid =    ABILITY_ID::HARVEST_GATHER_PROBE;
    const AbilityID m_supply_building_abilityid =  ABILITY_ID::BUILD_PYLON;
    const AbilityID m_gas_building_abilityid =     ABILITY_ID::BUILD_ASSIMILATOR;

};
