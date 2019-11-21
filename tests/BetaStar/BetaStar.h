#pragma once;

#include <iostream>
#include "sc2api/sc2_api.h"
#include "sc2api/sc2_args.h"
#include "sc2lib/sc2_lib.h"
#include "sc2utils/sc2_manage_process.h"
#include "sc2utils/sc2_arg_parser.h"

#include <map>
#include <tuple>

using namespace sc2;

/*
    BetaStar agent class
    This is our bot, the Coordinator calls our public Event-handling functions according to what happens in the game.
*/
class BetaStar : public Agent {
public:
    // Enumeration of our possible strategies
    enum Strategy
    {
        None,
        Blink_Stalker_Rush,
        Cannon_Rush
    };

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

    bool TryBuildStructure(AbilityID ability_type_for_structure, UnitTypeID unit_type);

    bool TryBuildStructure(AbilityID ability_type_for_structure, UnitTypeID unit_type, Tag location_tag);

    void TryBuildStructureNearPylon(AbilityID ability_type_for_structure, UnitTypeID unit_type);

    bool NeedWorkers();

    const Unit* FindNearestNeutralUnit(const Point2D& start, UnitTypeID target_unit_type);

    const Units FriendlyUnitsOfType(UnitTypeID unit_type) const;

    const Unit* FindResourceToGather(Point2D unit_pos);

    bool TryExpand(AbilityID build_ability, UnitTypeID worker_type);

    // Returns the UnitTypeID of the unit that builds the specified unit
    // Example: Terran Command Center for SCV and SCV for Terran Command Center
    UnitTypeID GetUnitBuilder(UnitTypeID unitToBuild);

    // Returns the AbilityID of the ability a builder will need to use to build the specified unit
    AbilityID GetUnitBuildAbility(UnitTypeID unitToBuild);

    // Returns the AbilityID of the ability to warp in the specified unit at a warp gate
    AbilityID GetUnitWarpAbility(UnitTypeID unitToWarp);

    // Returns true if unitType is a structure
    bool IsStructure(UnitTypeID unitType);

    // Attempts to train unit of unitType at random, valid building
    // Returns true if successful, false otherwise
    bool TrainUnit(UnitTypeID unitType);

    // Attempts to train unit of unitType at specified building
    // Returns true if successful, false otherwise
    bool TrainUnit(const Unit* building, UnitTypeID unitType);

    // Attempts to warp a unit in beside the power source closest to the requested build location
    bool WarpUnit(Point2D warpLocation, UnitTypeID unitType);

    // Attempts to warp a unit in beside the power source closest to the requested build location from the requested building
    bool WarpUnit(const Unit *building, Point2D warpLocation, UnitTypeID unitType);

    // Attempts to train unit of unitType at all valid buildings
    // Warpgates will warp unit close to themselves when this is used (i.e. call WarpUnit directly when attacking to warp units closer to enemy)
    size_t MassTrainUnit(UnitTypeID unitType);

    // Tries to train a balanced army based on private member unit ratios
    void TrainBalancedArmy();

    // Sets all unit ratios to 0.0f - nothing will be built after this is called unless ratios are set
    void ClearArmyRatios();

    // Counts all units of specified type
    // If includeIncomplete = true (default) buildings under construction and units in the production queue are counted
    size_t BetaStar::CountUnitType(UnitTypeID unitType, bool includeIncomplete = true);

    void TryResearchUpgrade(AbilityID upgrade_abilityid, UnitTypeID building_type);

    // Changes the current global strategy and makes adjustments accordingly
    void SetStrategy(Strategy newStrategy);

    // Attempts to issue command to unit. Returns true if successful.
    bool TryIssueCommand(const Unit *unit, AbilityID ability);

    // Attempts to issue command with target unit parameter to unit. Returns true if successful.
    bool TryIssueCommand(const Unit *unit, AbilityID ability, const Unit *target);

    // Attempts to issue command with point parameter to unit. Returns true if successful.
    bool TryIssueCommand(const Unit *unit, AbilityID ability, Point2D point);

    // Returns the closest unit in units to position
    const Unit* GetClosestUnit(Point2D position, const Units units);

    /* COMBAT FUNCTIONS */

    // Macro actions to defend our base. Call first and override with micro.
    void BaseDefenseMacro(const Units units);
    // Macro actions to attack enemy bases. Call first and override with micro.
    void EnemyBaseAttackMacro(const Units units);
    // Automatically performs blink micro on any stalkers that need it
    void StalkerBlinkMicro();

    /* MEMBER DATA */

    // position of our starting base
    Point3D m_starting_pos;
    int m_starting_quadrant;

    std::vector<Point2D> m_first_pylon_positions = {Point2D(43, 34), Point2D(33, 43)};
    std::vector<Point2D> m_placed_pylon_positions;

    // Contains building position and ids to recreate when destroyed
    std::vector<Point2D> m_building_positions;
    std::vector<AbilityID> m_building_ids;
    std::vector<std::tuple<Point2D, AbilityID>> m_buildings;

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

    // assumes points are in the south-west quadrant (x < 96, y < 96)
    Point2D rotate_position(Point2D pos, int new_quadrant) {
        Point2D new_pos(pos);
        new_pos.x = new_pos.x - 96;
        new_pos.y = new_pos.y - 96;
        switch (new_quadrant) {
            case (SW): {
                break;
            }
            case (NW): {
                for (int i = 0; i < 1; i++) {
                    float x = new_pos.x;
                    float y = new_pos.y;
                    new_pos.x = y;
                    new_pos.y = x * -1;
                }
                break;
            }
            case (NE): {
                for (int i = 0; i < 2; i++) {
                    float x = new_pos.x;
                    float y = new_pos.y;
                    new_pos.x = y;
                    new_pos.y = x * -1;
                }
                break;
            }
            case (SE): {
                for (int i = 0; i < 3; i++) {
                    float x = new_pos.x;
                    float y = new_pos.y;
                    new_pos.x = y;
                    new_pos.y = x * -1;
                }
                break;
            }
        }
        new_pos.x = new_pos.x + 96;
        new_pos.y = new_pos.y + 96;
        return new_pos;
    }

    // position of enemy's starting base
    bool m_enemy_base_scouted = false;
    Point2D m_enemy_base_pos;
    int m_enemy_base_quadrant;

    // how much supply we have left, updated each step
    int m_supply_left = 0;

    bool m_warpgate_researching = false;
    bool m_warpgate_researched = false;

    bool m_blink_researching = false;
    bool m_blink_researched = false;

    bool m_attacking = false;
    bool m_searching_new_enemy_base = false;
    size_t m_current_search_index = 0;

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
    bool has_detection = false;
    bool building_near_our_base = false;
    bool rush_detected = false;
    uint32_t last_detected_at_base_loop;
    std::set<const Unit*> rush_units;

    // ratios for units in the army
    // Formula: ceiling of ratio * (current army size + X) = target # of unit to produce (X is arbitrary, and represents a future army size so that builds don't get stuck at perfect ratios)
    // note that by default army_rations[UNIT_TYPEID] will return 0.0f if a unit type hasn't been added
    std::map<UNIT_TYPEID, float> army_ratios;

    // all unit types that we can manage by ratio
    std::vector<UNIT_TYPEID> managed_unit_types{
        UNIT_TYPEID::PROTOSS_ADEPT,
        UNIT_TYPEID::PROTOSS_CARRIER,
        UNIT_TYPEID::PROTOSS_COLOSSUS,
        UNIT_TYPEID::PROTOSS_DARKTEMPLAR,
        UNIT_TYPEID::PROTOSS_DISRUPTOR,
        UNIT_TYPEID::PROTOSS_HIGHTEMPLAR,
        UNIT_TYPEID::PROTOSS_IMMORTAL,
        UNIT_TYPEID::PROTOSS_ORACLE,
        UNIT_TYPEID::PROTOSS_PHOENIX,
        UNIT_TYPEID::PROTOSS_SENTRY,
        UNIT_TYPEID::PROTOSS_STALKER,
        UNIT_TYPEID::PROTOSS_TEMPEST,
        UNIT_TYPEID::PROTOSS_VOIDRAY,
        UNIT_TYPEID::PROTOSS_WARPPRISM,
        UNIT_TYPEID::PROTOSS_ZEALOT
    };

    Strategy m_current_strategy;
};
