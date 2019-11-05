#include "BetaStar.h"

const size_t BetaStar::CountUnitType(sc2::UNIT_TYPEID unit_type) const {
    return FriendlyUnitsOfType(unit_type).size();
}

bool BetaStar::TryBuildStructure(sc2::ABILITY_ID ability_type_for_structure, sc2::UNIT_TYPEID unit_type = sc2::UNIT_TYPEID::TERRAN_SCV)
{
    const sc2::ObservationInterface* observation = Observation();

    // If a unit already is building a supply structure of this type, do nothing.
    // Also get an scv to build the structure.
    const sc2::Unit* unit_to_build = nullptr;
    sc2::Units units = observation->GetUnits(sc2::Unit::Alliance::Self);
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

    float rx = sc2::GetRandomScalar();
    float ry = sc2::GetRandomScalar();

    Actions()->UnitCommand(unit_to_build, ability_type_for_structure, sc2::Point2D(unit_to_build->pos.x + rx * 15.0f, unit_to_build->pos.y + ry * 15.0f));

    return true;
}

bool BetaStar::TryBuildSupplyDepot() {
    const sc2::ObservationInterface* observation = Observation();

    // If we are not supply capped, don't build a supply depot.
    if (observation->GetFoodUsed() <= observation->GetFoodCap() - 2) {
        return false;
    }

    // Try and build a depot. Find a random SCV and give it the order.
    return TryBuildStructure(sc2::ABILITY_ID::BUILD_SUPPLYDEPOT);
}

bool BetaStar::TryBuildBarracks() {
    const sc2::ObservationInterface* observation = Observation();

    if (CountUnitType(sc2::UNIT_TYPEID::TERRAN_SUPPLYDEPOT) < 1) {
        return false;
    }

    if (CountUnitType(sc2::UNIT_TYPEID::TERRAN_BARRACKS) > 0) {
        return false;
    }

    return TryBuildStructure(sc2::ABILITY_ID::BUILD_BARRACKS);
}

const sc2::Unit* BetaStar::FindNearestNeutralUnit(const sc2::Point2D& start, sc2::UNIT_TYPEID target_unit_type) {
    std::cout << Observation()->GetUnits(sc2::Unit::Alliance::Neutral).size() << std::endl;

    sc2::Units units = Observation()->GetUnits(sc2::Unit::Alliance::Neutral);

    float distance = std::numeric_limits<float>::max();
    const sc2::Unit* target = nullptr;

    for (const auto& unit : units) {
        if (unit->unit_type == target_unit_type) {
            float d = sc2::DistanceSquared2D(unit->pos, start);
            if (d < distance) {
                distance = d;
                target = unit;
            }
        }
    }

    return target;
}

const sc2::Units BetaStar::FriendlyUnitsOfType(sc2::UNIT_TYPEID unit_type) const
{
    return Observation()->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnit(unit_type));
}
