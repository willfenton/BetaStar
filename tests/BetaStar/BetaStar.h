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
    virtual void OnUnitIdle(const Unit* unit);

    // called each a building is finished construction
    virtual void OnBuildingConstructionComplete(const Unit* unit) final;

    // called when an enemy unit enters vision from out of fog of war
    virtual void OnUnitEnterVision(const Unit *unit) final;

    virtual void OnUpgradeCompleted(UpgradeID upgrade_id) final;

private:

    /* ON-STEP FUNCTIONS */

    void OnStepComputeStatistics();

    void OnStepTrainWorkers();

    void OnStepBuildPylons();

    void OnStepBuildGas();

    void OnStepExpand();

    void OnStepManageWorkers();

    void OnStepBuildOrder();

    void OnStepBuildArmy();

    void OnStepManageArmy();

    void OnStepResearchUpgrades();

    /* ON-UNIT-ENTER-VISION FUNCTIONS */

    // used to build a profile of the opponent over time - no response to threat in this function
    void GatherIntelligence(const Unit *unit);

    /* UTILITY FUNCTIONS */

    const size_t CountUnitType(UnitTypeID unit_type) const;

    bool TryBuildStructure(AbilityID ability_type_for_structure, UnitTypeID unit_type);

    bool TryBuildStructure(AbilityID ability_type_for_structure, UnitTypeID unit_type, Tag location_tag);

    void TryBuildStructureNearPylon(AbilityID ability_type_for_structure, UnitTypeID unit_type);

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

    bool TryWarpInUnit(AbilityID ability_type_for_unit);

    // Returns the UnitTypeID of the unit that builds the specified unit
    // Example: Terran Command Center for SCV and SCV for Terran Command Center
    UnitTypeID GetUnitBuilder(UnitTypeID unitToBuild);

    // Returns the AbilityID of the ability a builder will need to use to build the specified unit
    AbilityID GetUnitBuildAbility(UnitTypeID unitToBuild);

    // Returns true if unitType is a structure
    bool IsStructure(UnitTypeID unitType);

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

    void TryResearchUpgrade(AbilityID upgrade_abilityid, UnitTypeID building_type);


    /* MEMBER DATA */

    // position of our starting base
    Point3D m_starting_pos;

    enum starting_positions { SW, NW, NE, SE };
    int get_starting_position_of_point(Point2D pos)
    {
        int starting_position = -1;
        if (pos.x == 33.5 && pos.y == 33.5) {
            starting_position = SW;
        }
        else if (pos.x == 33.5 && pos.y == 158.5) {
            starting_position = NW;
        }
        else if (pos.x == 158.5 && pos.y == 158.5) {
            starting_position = NE;
        }
        else if (pos.x == 158.5 && pos.y == 33.5) {
            starting_position = SE;
        }
        else {
            std::cerr << "Error in get_starting_position_of_point()" << std::endl;
        }
        return starting_position;
    }

    // position of enemy's starting base
    bool m_enemy_base_scouted = false;
    Point2D m_enemy_base_pos;

    // how much supply we have left, updated each step
    int m_supply_left = 0;

    bool m_warpgate_researching = false;
    bool m_warpgate_researched = false;

    bool m_blink_researching = false;
    bool m_blink_researched = false;

    bool m_attacking = false;

    // how many bases to build (max)
    const int m_max_bases = 3;

    // whether we are currently building a nexus (don't consider expanding if true)
    bool m_building_nexus = false;

    // all expansion locations
    std::vector<Point3D> m_expansion_locations;

    const Unit* m_initial_scouting_probe;

    // common unit ids
    const UnitTypeID m_base_typeid =               UNIT_TYPEID::PROTOSS_NEXUS;
    const UnitTypeID m_worker_typeid =             UNIT_TYPEID::PROTOSS_PROBE;
    const UnitTypeID m_supply_building_typeid =    UNIT_TYPEID::PROTOSS_PYLON;
    const UnitTypeID m_gas_building_typeid =       UNIT_TYPEID::PROTOSS_ASSIMILATOR;

    // common ability ids
    const AbilityID m_base_building_abilityid =    ABILITY_ID::BUILD_NEXUS;
    const AbilityID m_worker_train_abilityid =     ABILITY_ID::TRAIN_PROBE;
    const AbilityID m_worker_gather_abilityid =    ABILITY_ID::HARVEST_GATHER;
    const AbilityID m_supply_building_abilityid =  ABILITY_ID::BUILD_PYLON;
    const AbilityID m_gas_building_abilityid =     ABILITY_ID::BUILD_ASSIMILATOR;

    // info about enemy
    bool has_flying = false;
    bool has_cloaked = false;
    bool building_near_our_base = false;
    bool rush_detected = false;
    uint32_t last_detected_at_base_loop;
    std::set<const Unit*> rush_units;
};
