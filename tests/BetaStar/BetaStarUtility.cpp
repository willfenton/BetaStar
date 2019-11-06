#include "BetaStar.h"

using namespace sc2;

const size_t BetaStar::CountUnitType(UnitTypeID unit_type) const {
    return FriendlyUnitsOfType(unit_type).size();
}

bool BetaStar::NeedWorkers() {
    int num_workers = CountUnitType(UNIT_TYPEID::TERRAN_SCV);

    int ideal_workers = 0;
    for (const auto& cc : FriendlyUnitsOfType(UNIT_TYPEID::TERRAN_COMMANDCENTER)) {
        ideal_workers += cc->ideal_harvesters;
    }
    for (const auto& vg : FriendlyUnitsOfType(UNIT_TYPEID::TERRAN_REFINERY)) {
        ideal_workers += vg->ideal_harvesters;
    }

    return (num_workers < ideal_workers);
}

bool BetaStar::TryBuildStructure(AbilityID ability_type_for_structure, UnitTypeID unit_type = UNIT_TYPEID::TERRAN_SCV)
{
    const ObservationInterface* observation = Observation();

    // If a unit already is building a supply structure of this type, do nothing.
    // Also get an scv to build the structure.
    const Unit* unit_to_build = nullptr;
    Units units = observation->GetUnits(Unit::Alliance::Self);
    for (const auto& unit : units) {
        for (const auto& order : unit->orders) {
            if (order.ability_id == ability_type_for_structure) {
                return false;
            }
        }

        if (unit->unit_type == unit_type) {
            unit_to_build = unit;
        }
    }

    float rx = GetRandomScalar();
    float ry = GetRandomScalar();

    Actions()->UnitCommand(unit_to_build, ability_type_for_structure, sc2::Point2D(unit_to_build->pos.x + rx * 15.0f, unit_to_build->pos.y + ry * 15.0f));

    return true;
}

// COPIED
//Try to build a structure based on tag, Used mostly for Vespene, since the pathing check will fail even though the geyser is "Pathable"
bool BetaStar::TryBuildStructure(AbilityID ability_type_for_structure, UnitTypeID unit_type, Tag location_tag) {
    const ObservationInterface* observation = Observation();
    Units workers = observation->GetUnits(Unit::Alliance::Self, IsUnit(unit_type));
    const Unit* target = observation->GetUnit(location_tag);

    if (workers.empty()) {
        return false;
    }

    // Check to see if there is already a worker heading out to build it
    for (const auto& worker : workers) {
        for (const auto& order : worker->orders) {
            if (order.ability_id == ability_type_for_structure) {
                return false;
            }
        }
    }

    // If no worker is already building one, get a random worker to build one
    const Unit* unit = GetRandomEntry(workers);

    // Check to see if unit can build there
    if (Query()->Placement(ability_type_for_structure, target->pos)) {
        Actions()->UnitCommand(unit, ability_type_for_structure, target);
        return true;
    }
    return false;

}

bool BetaStar::TryBuildSupplyDepot() {
    const ObservationInterface* observation = Observation();

    int supply_left = observation->GetFoodCap() - observation->GetFoodUsed();

    // If we still have supply, don't build a supply depot.
    if (supply_left > 2) {
        return false;
    }

    // Try and build a depot. Find a random SCV and give it the order.
    return TryBuildStructure(ABILITY_ID::BUILD_SUPPLYDEPOT);
}

bool BetaStar::TryBuildBarracks() {
    const ObservationInterface* observation = Observation();

    if (CountUnitType(UNIT_TYPEID::TERRAN_SUPPLYDEPOT) < 1) {
        return false;
    }

    if (CountUnitType(UNIT_TYPEID::TERRAN_BARRACKS) > 0) {
        return false;
    }

    return TryBuildStructure(ABILITY_ID::BUILD_BARRACKS);
}

void BetaStar::BuildRefineries() {
    for (const auto& cc : FriendlyUnitsOfType(UNIT_TYPEID::TERRAN_COMMANDCENTER)) {
        TryBuildRefinery(cc->pos);
    }
}

// COPIED
//Tries to build a geyser for a base
bool BetaStar::TryBuildRefinery(Point2D base_location) {
    const ObservationInterface* observation = Observation();
    Units geysers = observation->GetUnits(Unit::Alliance::Neutral, IsUnit(UNIT_TYPEID::NEUTRAL_VESPENEGEYSER));

    //only search within this radius
    float minimum_distance = 225.0f;
    Tag closestGeyser = 0;
    for (const auto& geyser : geysers) {
        float current_distance = DistanceSquared2D(base_location, geyser->pos);
        if (current_distance < minimum_distance) {
            if (Query()->Placement(ABILITY_ID::BUILD_REFINERY, geyser->pos)) {
                minimum_distance = current_distance;
                closestGeyser = geyser->tag;
            }
        }
    }

    // In the case where there are no more available geysers nearby
    if (closestGeyser == 0) {
        return false;
    }
    return TryBuildStructure(ABILITY_ID::BUILD_REFINERY, UNIT_TYPEID::TERRAN_SCV, closestGeyser);
}

const Unit* BetaStar::FindNearestNeutralUnit(const Point2D& start, UnitTypeID target_unit_type) {
    Units units = Observation()->GetUnits(Unit::Alliance::Neutral);

    float distance = std::numeric_limits<float>::max();
    const Unit* target = nullptr;

    for (const auto& unit : units) {
        if (unit->unit_type == target_unit_type) {
            float d = DistanceSquared2D(unit->pos, start);
            if (d < distance) {
                distance = d;
                target = unit;
            }
        }
    }

    return target;
}

const Units BetaStar::FriendlyUnitsOfType(UnitTypeID unit_type) const
{
    return Observation()->GetUnits(Unit::Alliance::Self, IsUnit(unit_type));
}
