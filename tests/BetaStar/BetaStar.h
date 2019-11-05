#pragma once;

#include <iostream>
#include "sc2api/sc2_api.h"
#include "sc2api/sc2_args.h"
#include "sc2lib/sc2_lib.h"
#include "sc2utils/sc2_manage_process.h"
#include "sc2utils/sc2_arg_parser.h"

class BetaStar : public sc2::Agent {
public:
    // this function runs at the start of the game
    virtual void OnGameStart() final;

    // called each time the coordinator steps the simulation forward
    virtual void OnStep() final;

private:
    // Called each time a unit has been built and has no orders or the unit had orders in the previous step and currently does not
    // Both buildings and units are considered units and are represented with a Unit object.
    virtual void OnUnitIdle(const sc2::Unit* unit) final;

    size_t CountUnitType(sc2::UNIT_TYPEID unit_type);

    bool TryBuildStructure(sc2::ABILITY_ID ability_type_for_structure, sc2::UNIT_TYPEID unit_type);

    bool TryBuildSupplyDepot();

    bool TryBuildBarracks();

    const sc2::Unit* FindNearestNeutralUnit(const sc2::Point2D& start, sc2::UNIT_TYPEID target_unit_type);

};

