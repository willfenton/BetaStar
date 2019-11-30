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
    /* Nested Class/Struct/Enum Definitions */

    // Filter to use with GetUnit. Selects units of a certain type. Allows chaining our custom filters.
    struct BetterIsUnit {
        BetterIsUnit(UNIT_TYPEID type, Filter filter = false) : _type(type), _filter(filter) { }
        UNIT_TYPEID _type;
        Filter _filter;
        bool operator()(const Unit &unit) { return unit.unit_type == _type && (!_filter || _filter(unit)); }
    };

    // Filter to use with GetUnit. Selects units that are buildings. These can also all combine into one compound filter by passing one as an optional argument to the next.
    struct IsBuilding {
        IsBuilding(Filter filter = false) : _filter(filter) { }
        Filter _filter;
        bool operator()(const Unit &unit) { return IsStructure(unit.unit_type) && (!_filter || _filter(unit)); }
    };

    // Filter to use with GetUnit. Selects units that are within radius of point or unit.
    struct IsNear {
        IsNear(const Unit *unit, float radius, Filter filter = false) : _pos(unit->pos), _radiusSquared(radius*radius), _filter(filter) { }
        IsNear(Point2D pos, float radius, Filter filter = false) : _pos(pos), _radiusSquared(radius*radius), _filter(filter) { }
        Point2D _pos;
        float _radiusSquared;
        Filter _filter;
        bool operator()(const Unit &unit) { return DistanceSquared2D(unit.pos, _pos) <= _radiusSquared && (!_filter || _filter(unit)); }
    };

    // Filter to use with GetUnit. Selects units that don't have any orders queued.
    struct HasNoOrders {
        HasNoOrders(Filter filter = false) : _filter(filter) { }
        Filter _filter;
        bool operator()(const Unit &unit) { return unit.orders.empty() && (!_filter || _filter(unit)); }
    };

    // Filter to use with GetUnit. Selects units that aren't actively cloaked. 
    struct IsNotCloaked {
        IsNotCloaked(Filter filter = false) : _filter(filter) { }
        Filter _filter;
        bool operator()(const Unit &unit) { return unit.cloak != Unit::CloakState::Cloaked && (!_filter || _filter(unit)); }
    };

    // Filter to use with GetUnit. Selects units that aren't hidden by fog of war or saved as a snapshot.
    struct IsVisible {
        IsVisible(Filter filter = false) : _filter(filter) { }
        Filter _filter;
        bool operator()(const Unit &unit) { return unit.display_type == Unit::DisplayType::Visible && (!_filter || _filter(unit)); }
    };

    // Enumeration of our possible strategies
    enum Strategy
    {
        None,
        Blink_Stalker_Rush,
        Cannon_Rush
    };

    //Functor for sorting vector of enemy units by priority; used in EnemyBaseAttackMacro
    // It uses the the GetUnitAttackPriority function which has to be non-stactic so this functor must be called with
    // the argument "this"
    struct IsHigherPriority {

        BetaStar* CurrentBot;
        Point2D army_centroid;

        IsHigherPriority(BetaStar* _CurrentBot, Point2D _army_centroid) : CurrentBot(_CurrentBot), army_centroid(_army_centroid) {}

        //Note that we define use the ">" operation and not the "<" one because when we want to sort the enemy units
        //using this functor, we want the highest priority units to be at the front of the sorted enemy units vector
        inline bool operator() (const Unit* unit1, const Unit* unit2) {
            return CurrentBot->GetUnitAttackPriority(unit1, army_centroid) > CurrentBot->GetUnitAttackPriority(unit2, army_centroid);
        }
    };

    struct IsCloser {
        IsCloser(Point2D compPoint) : _compPoint(compPoint) { }
        Point2D _compPoint;
        inline bool operator() (const Unit* lhs, const Unit *rhs) {
            return DistanceSquared2D(lhs->pos, _compPoint) < DistanceSquared2D(rhs->pos, _compPoint);
        }
    };

    /* Static Functions (Can also be used in functors) */

    // Returns the UnitTypeID of the unit that builds the specified unit
    // Example: Terran Command Center for SCV and SCV for Terran Command Center
    static UnitTypeID GetUnitBuilder(UnitTypeID unitToBuild);

    // Returns the AbilityID of the ability a builder will need to use to build the specified unit
    static AbilityID GetUnitBuildAbility(UnitTypeID unitToBuild);

    // Returns the AbilityID of the ability to warp in the specified unit at a warp gate
    static AbilityID GetUnitWarpAbility(UnitTypeID unitToWarp);

    // Returns true if unitType is a structure
    static bool IsStructure(UnitTypeID unitType);

    // Account for floating point errors and such
    static bool AlmostEqual(float lhs, float rhs, float threshold = 0.01f);

    // Account for floating point errors and such in Point2D components
    static bool AlmostEqual(Point2D lhs, Point2D rhs, Point2D threshold = Point2D(0.01f, 0.01f));

    /* API Callbacks */

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

    // Keeps chrono boost active when possible
    void OnStepChronoBoost();

    /* ON-UNIT-ENTER-VISION FUNCTIONS */

    // used to build a profile of the opponent over time - no response to threat in this function
    void GatherIntelligence(const Unit *unit);

    /* UTILITY FUNCTIONS */

    void TryBuildStructureNearPylon(AbilityID ability_type_for_structure, UnitTypeID unit_type = UNIT_TYPEID::PROTOSS_PROBE);

    // Attempts to build buildingType near a random friendly Pylon.
    // Parameters:
    //    UnitTypeID buildingType: The type of building to produce.
    //    Unit* builder: (Optional) A specific worker to use. Selects closest worker to intended build location otherwise.
    // Returns true if successful, false otherwise.
    bool TryBuildStructureNearPylon(UnitTypeID buildingType, const Unit *builder = nullptr);

    // Attempts to build buildingType near the closest pylon to nearPosition.
    // Parameters:
    //    UnitTypeID buildingType: The type of building to produce.
    //    Point2D nearPosition: Will select the friendly pylon closest to this point as the one to build near.
    //    Unit* builder: (Optional) A specific worker to use. Selects closest worker to intended build location otherwise.
    // Returns true if successful, false otherwise.
    bool TryBuildStructureNearPylon(UnitTypeID buildingType, Point2D nearPosition, const Unit *builder = nullptr);

    // Attempts to build buildingType near a random pylon within maxRadius of nearPosition.
    // Parameters:
    //    UnitTypeID buildingType: The type of building to produce.
    //    Point2D nearPosition: Will select a random friendly pylon around this point as the one to build near.
    //    float maxRadius: The distance from nearPosition to search for friendly pylons. Will select a random pylon from one of the ones found.
    //    Unit* builder: (Optional) A specific worker to use. Selects closest worker to intended build location otherwise.
    // Returns true if successful, false otherwise.
    bool TryBuildStructureNearPylon(UnitTypeID buildingType, Point2D nearPosition, float maxRadius, const Unit *builder = nullptr);

    // All TryBuildStructureNearPylon overloads resolve here. Attempts to build buildingType a position with builder.
    // Check to make sure the building will have power before using this method.
    // Parameters:
    //    UnitTypeID buildingType: The type of building to produce.
    //    Point2D buildPos: The position to build at.
    //    Unit* builder: The specific worker to use for building.
    // Returns true if successful, false otherwise.
    bool TryBuildStructure(UnitTypeID buildingType, Point2D buildPos, const Unit *builder);

    const Unit* FindNearestNeutralUnit(const Point2D& start, UnitTypeID target_unit_type);

    const Units FriendlyUnitsOfType(UnitTypeID unit_type) const;

    const Unit* FindResourceToGather(Point2D unit_pos);

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

    // Returns the average position of all provided units
    Point2D GetUnitsCentroid(const Units units);
    // Returns the average position of the closest ceil(Units.size() * unitFrac) to the desired target
    Point2D GetUnitsCentroidNearPoint(const Units units, float unitFrac, Point2D desiredTarget);

    // Returns the current game time, in seconds
    float GetGameTime();

    //Return the targeting priority of a unit as an integer.
    // The integer returned is: 
    // Between 0 and 100 for low priority units without weapons (i.e. structures)
    // Between 100 and 200 for low priority units with weapons
    // Between 200 and 300 for medium priority units
    // Between 300 and 400 for high priority units
    // The return value has been made to be a range so that we can vary priority values within the range if we like
    int GetUnitAttackPriority(const Unit* unit, Point2D army_centroid);

    //Helper functions for GetUnitAttackPriority
    int GetProtossUnitAttackPriority(const Unit* unit);
    int GetTerranUnitAttackPriority(const Unit* unit);
    int GetZergUnitAttackPriority(const Unit* unit);
    int GenericPriorityFallbacks(const Unit* unit);

    //Returns a boolean depending on whether the unit passed as argument can attack air units or not
    bool CanAttackAirUnits(const Unit* unit);

    /* COMBAT FUNCTIONS */

    // Macro actions to defend our base. Call first and override with micro.
    void BaseDefenseMacro(const Units units);
    // Macro actions to attack enemy bases. Call first and override with micro.
    void EnemyBaseAttackMacro(const Units units);
    // Smart targeting for all units to pick targets from enemy_units. 
    void TargetingMicro(const Units units, Units enemy_units);
    // Automatically performs blink micro on any stalkers that need it
    void StalkerBlinkMicro();

    /* MEMBER DATA */

    // info about all SC2 units
    UnitTypes all_unit_type_data;
    // info about all SC2 abilities
    Abilities all_ability_data;
    // info about all SC2 upgrades
    Upgrades all_upgrades;
    // info about all SC2 buffs
    Buffs all_buffs;

    // position of our starting base
    Point3D m_starting_pos;
    int m_starting_quadrant;

    std::vector<Point2D> m_first_pylon_positions = { Point2D(43, 34), Point2D(33, 43) };
    std::vector<Point2D> m_placed_pylon_positions;

    // Contains building position and ids to recreate when destroyed
    std::vector<Point2D> m_building_positions;
    std::vector<AbilityID> m_building_ids;
    std::vector<std::tuple<Point2D, AbilityID>> m_buildings;

    // quadrants of the map
    enum starting_positions { SW, NW, NE, SE };

    // return the quadrant of the map
    int GetQuadrantByPoint(Point2D pos);

    // assumes points are in the south-west quadrant (x < 96, y < 96)
    Point2D RotatePosition(Point2D pos, int new_quadrant);

    // position of enemy's starting base
    bool m_enemy_base_scouted = false;
    Point2D m_enemy_base_pos;
    int m_enemy_base_quadrant;

    bool m_warpgate_researching = false;
    bool m_warpgate_researched = false;

    bool m_blink_researching = false;
    bool m_blink_researched = false;

    bool m_attacking = false;
    bool m_all_workers = true;
    bool m_searching_new_enemy_base = false;
    //std::vector<Point2D> m_visited_expansion_locations;
    std::vector<Point2D> m_open_expansion_locations;

    // how many bases to build (max)
    const int m_max_bases = 3;

    // whether we are currently building a nexus (don't consider expanding if true)
    bool m_building_nexus = false;

    // all expansion locations
    std::vector<Point3D> m_expansion_locations;

    // scouting
    const Unit* m_initial_scouting_probe;

    // common unit ids
    const UnitTypeID m_base_typeid = UNIT_TYPEID::PROTOSS_NEXUS;
    const UnitTypeID m_worker_typeid = UNIT_TYPEID::PROTOSS_PROBE;
    const UnitTypeID m_supply_building_typeid = UNIT_TYPEID::PROTOSS_PYLON;
    const UnitTypeID m_gas_building_typeid = UNIT_TYPEID::PROTOSS_ASSIMILATOR;

    // common ability ids
    const AbilityID m_base_building_abilityid = ABILITY_ID::BUILD_NEXUS;
    const AbilityID m_worker_train_abilityid = ABILITY_ID::TRAIN_PROBE;
    const AbilityID m_worker_gather_abilityid = ABILITY_ID::HARVEST_GATHER;
    const AbilityID m_supply_building_abilityid = ABILITY_ID::BUILD_PYLON;
    const AbilityID m_gas_building_abilityid = ABILITY_ID::BUILD_ASSIMILATOR;

    // info about enemy
    Race enemy_race = Race::Random;
    bool has_flying = false;
    bool has_cloaked = false;
    bool has_detection = false;
    bool building_near_our_base = false;
    bool rush_detected = false;
    float last_detected_at_base_time;
    std::set<const Unit*> rush_units;

    // ratios for units in the army
    // Formula: ceiling of ratio * (current army size + X) = target # of unit to produce (X is arbitrary, and represents a future army size so that builds don't get stuck at perfect ratios)
    // note that by default army_rations[UNIT_TYPEID] will return 0.0f if a unit type hasn't been added
    std::map<UNIT_TYPEID, float> army_ratios;

    // all unit types that we can manage by ratio
    std::vector<UNIT_TYPEID> managed_unit_types = {
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
