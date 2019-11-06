#pragma once;

#include <iostream>
#include "sc2api/sc2_api.h"
#include "sc2api/sc2_args.h"
#include "sc2lib/sc2_lib.h"
#include "sc2utils/sc2_manage_process.h"
#include "sc2utils/sc2_arg_parser.h"

using namespace sc2;


class BetaStar : public Agent {
public:
    // this function runs at the start of the game
    virtual void OnGameStart() final;

    // called each time the coordinator steps the simulation forward
    virtual void OnStep() final;

private:
    // Called each time a unit has been built and has no orders or the unit had orders in the previous step and currently does not
    // Both buildings and units are considered units and are represented with a Unit object.
    virtual void OnUnitIdle(const Unit* unit) final;

    const size_t CountUnitType(UnitTypeID unit_type) const;

    bool TryBuildStructure(AbilityID ability_type_for_structure, UnitTypeID unit_type);

    bool TryBuildStructure(AbilityID ability_type_for_structure, UnitTypeID unit_type, Tag location_tag);

    bool TryBuildSupplyDepot();

    bool TryBuildBarracks();

    void BuildRefineries();

    bool TryBuildRefinery(Point2D base_location);

    bool NeedWorkers();

    const Unit* FindNearestNeutralUnit(const Point2D& start, UnitTypeID target_unit_type);

    const Units FriendlyUnitsOfType(UnitTypeID unit_type) const;

    const Unit* FindResourceToGather(Point2D unit_pos);

    void ManageWorkers(UnitTypeID worker_type, AbilityID worker_gather_command, UnitTypeID vespene_building_type);

    void MineIdleWorkers(const Unit* worker, AbilityID worker_gather_command, UnitTypeID vespene_building_type);

    bool TryExpand(AbilityID build_ability, UnitTypeID worker_type);

    void TrainWorkers();

    Point3D starting_pos;
};
