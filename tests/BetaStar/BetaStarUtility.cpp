#include "BetaStar.h"

using namespace sc2;


// return the number of friendly units of type unit_type
const size_t BetaStar::CountUnitType(UnitTypeID unit_type) const {
    return FriendlyUnitsOfType(unit_type).size();
}

// returns false if all bases and vespene buildings are worker capped, else true
bool BetaStar::NeedWorkers() {
    int num_workers = 0;
    int ideal_workers = 0;

    for (const auto& base : FriendlyUnitsOfType(m_base_typeid)) {
        num_workers += base->assigned_harvesters;
        ideal_workers += base->ideal_harvesters;
    }
    for (const auto& vg : FriendlyUnitsOfType(m_gas_building_typeid)) {
        num_workers += vg->assigned_harvesters;
        ideal_workers += vg->ideal_harvesters;
    }

    //std::cout << num_workers << " " << ideal_workers << std::endl;

    return (num_workers < ideal_workers);
}

// COULD USE SOME WORK
// try to build a structure
// returns true if successful, else false
bool BetaStar::TryBuildStructure(AbilityID ability_type_for_structure, UnitTypeID unit_type)
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

// MOSTLY COPIED
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
    // If we still have enough supply, don't build a supply depot.
    if (std::max(0, Observation()->GetFoodCap() - Observation()->GetFoodUsed()) > 3) {
        return false;
    }

    // Try and build a depot. Find a random SCV and give it the order.
    return TryBuildStructure(m_supply_building_ability, m_worker_typeid);
}

// build refineries at our bases if we haven't yet
void BetaStar::BuildGas() {
    if (CountUnitType(m_supply_building_typeid) < 1) {
        return;
    }

    for (const auto& cc : FriendlyUnitsOfType(m_base_typeid)) {
        TryBuildGas(cc->pos);
    }
}

// MOSTLY COPIED
// Tries to build a geyser for a base
bool BetaStar::TryBuildGas(Point2D base_location) {

    const ObservationInterface* observation = Observation();

    Units geysers = observation->GetUnits(Unit::Alliance::Neutral, IsUnit(UNIT_TYPEID::NEUTRAL_VESPENEGEYSER));

    //only search within this radius
    float minimum_distance = 225.0f;
    Tag closestGeyser = 0;
    for (const auto& geyser : geysers) {
        float current_distance = DistanceSquared2D(base_location, geyser->pos);
        if (current_distance < minimum_distance) {
            if (Query()->Placement(m_gas_building_ability, geyser->pos)) {
                minimum_distance = current_distance;
                closestGeyser = geyser->tag;
            }
        }
    }

    // In the case where there are no more available geysers nearby
    if (closestGeyser == 0) {
        return false;
    }
    return TryBuildStructure(m_gas_building_ability, m_worker_typeid, closestGeyser);
}

// returns the nearest neutral unit of type target_unit_type
// if there is none, returns nullptr
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

// returns a vector containing friendly units of type unit_type
const Units BetaStar::FriendlyUnitsOfType(UnitTypeID unit_type) const
{
    return Observation()->GetUnits(Unit::Alliance::Self, IsUnit(unit_type));
}

// returns an uncapped mineral or vespene geyser, or the closest mineral if they are all capped
const Unit* BetaStar::FindResourceToGather(Point2D unit_pos) {
    for (const auto& cc : FriendlyUnitsOfType(m_base_typeid)) {
        if (cc->assigned_harvesters < cc->ideal_harvesters) {
            return FindNearestNeutralUnit(cc->pos, UNIT_TYPEID::NEUTRAL_MINERALFIELD);
        }
    }
    for (const auto& vg : FriendlyUnitsOfType(m_gas_building_typeid)) {
        if (vg->assigned_harvesters < vg->ideal_harvesters) {
            return vg;
        }
    }
    return FindNearestNeutralUnit(unit_pos, UNIT_TYPEID::NEUTRAL_MINERALFIELD);
}

// MOSTLY COPIED
// To ensure that we do not over or under saturate any base.
void BetaStar::ManageWorkers(UnitTypeID worker_type, AbilityID worker_gather_command, UnitTypeID vespene_building_type) {
    const ObservationInterface* observation = Observation();
    Units bases = observation->GetUnits(Unit::Alliance::Self, IsUnit(m_base_typeid));
    Units geysers = observation->GetUnits(Unit::Alliance::Self, IsUnit(vespene_building_type));

    if (bases.empty()) {
        return;
    }

    for (const auto& base : bases) {
        //If we have already mined out or still building here skip the base.
        if (base->ideal_harvesters == 0 || base->build_progress != 1) {
            continue;
        }
        //if base is
        if (base->assigned_harvesters > base->ideal_harvesters) {
            Units workers = observation->GetUnits(Unit::Alliance::Self, IsUnit(worker_type));

            for (const auto& worker : workers) {
                if (!worker->orders.empty()) {
                    if (worker->orders.front().target_unit_tag == base->tag) {
                        //This should allow them to be picked up by mineidleworkers()
                        MineIdleWorkers(worker, worker_gather_command, vespene_building_type);
                        return;
                    }
                }
            }
        }
    }
    Units workers = observation->GetUnits(Unit::Alliance::Self, IsUnit(worker_type));
    for (const auto& geyser : geysers) {
        if (geyser->ideal_harvesters == 0 || geyser->build_progress != 1) {
            continue;
        }
        if (geyser->assigned_harvesters > geyser->ideal_harvesters) {
            for (const auto& worker : workers) {
                if (!worker->orders.empty()) {
                    if (worker->orders.front().target_unit_tag == geyser->tag) {
                        //This should allow them to be picked up by mineidleworkers()
                        MineIdleWorkers(worker, worker_gather_command, vespene_building_type);
                        return;
                    }
                }
            }
        }
        else if (geyser->assigned_harvesters < geyser->ideal_harvesters) {
            for (const auto& worker : workers) {
                if (!worker->orders.empty()) {
                    //This should move a worker that isn't mining gas to gas
                    const Unit* target = observation->GetUnit(worker->orders.front().target_unit_tag);
                    if (target == nullptr) {
                        continue;
                    }
                    if (target->unit_type != vespene_building_type) {
                        //This should allow them to be picked up by mineidleworkers()
                        MineIdleWorkers(worker, worker_gather_command, vespene_building_type);
                        return;
                    }
                }
            }
        }
    }
}

// MOSTLY COPIED
// Mine the nearest mineral to Town hall.
// If we don't do this, probes may mine from other patches if they stray too far from the base after building.
void BetaStar::MineIdleWorkers(const Unit* worker, AbilityID worker_gather_command, UnitTypeID vespene_building_type) {
    const ObservationInterface* observation = Observation();
    Units bases = observation->GetUnits(Unit::Alliance::Self, IsUnit(m_base_typeid));
    Units geysers = observation->GetUnits(Unit::Alliance::Self, IsUnit(vespene_building_type));

    const Unit* valid_mineral_patch = nullptr;

    if (bases.empty()) {
        return;
    }

    //Search for a base that is missing workers.
    for (const auto& base : bases) {
        //If we have already mined out here skip the base.
        if (base->ideal_harvesters == 0 || base->build_progress != 1) {
            continue;
        }
        if (base->assigned_harvesters < base->ideal_harvesters) {
            valid_mineral_patch = FindNearestNeutralUnit(base->pos, UNIT_TYPEID::NEUTRAL_MINERALFIELD);
            Actions()->UnitCommand(worker, worker_gather_command, valid_mineral_patch);
            return;
        }
    }

    for (const auto& geyser : geysers) {
        if (geyser->assigned_harvesters < geyser->ideal_harvesters) {
            Actions()->UnitCommand(worker, worker_gather_command, geyser);
            return;
        }
    }

    if (!worker->orders.empty()) {
        return;
    }

    //If all workers are spots are filled just go to any base.
    const Unit* random_base = GetRandomEntry(bases);
    valid_mineral_patch = FindNearestNeutralUnit(random_base->pos, UNIT_TYPEID::NEUTRAL_MINERALFIELD);
    Actions()->UnitCommand(worker, worker_gather_command, valid_mineral_patch);
}

// MOSTLY COPIED
// CURRENTLY BROKEN
// Expands to nearest location
bool BetaStar::TryExpand(AbilityID build_ability, UnitTypeID worker_type) {
    const ObservationInterface* observation = Observation();

    if (building_nexus || observation->GetMinerals() < 450 || NeedWorkers()) {
        return false;
    }

    float minimum_distance = std::numeric_limits<float>::max();
    Point3D closest_expansion;
    for (const auto& expansion : search::CalculateExpansionLocations(Observation(), Query())) {
        float current_distance = Distance2D(starting_pos, expansion);
        if (current_distance < .01f) {
            continue;
        }

        if (current_distance < minimum_distance) {
            if (Query()->Placement(build_ability, expansion)) {
                closest_expansion = expansion;
                minimum_distance = current_distance;
            }
        }
    }

    const Unit* unit_to_build = nullptr;
    Units units = observation->GetUnits(Unit::Alliance::Self);
    for (const auto& unit : units) {
        for (const auto& order : unit->orders) {
            if (order.ability_id == build_ability) {
                return false;
            }
        }

        if (unit->unit_type == worker_type) {
            unit_to_build = unit;
        }
    }

    building_nexus = true;

    Actions()->UnitCommand(unit_to_build, build_ability, closest_expansion);

    return true;
}

UnitTypeID BetaStar::GetUnitBuilder(UnitTypeID unitToBuild)
{
    // TODO: Warp gates are not represented as a builder
    switch (unitToBuild.ToType())
    {
        // Protoss Units
        case UNIT_TYPEID::PROTOSS_ADEPT:
            return UNIT_TYPEID::PROTOSS_GATEWAY;
        case UNIT_TYPEID::PROTOSS_ASSIMILATOR:
            return UNIT_TYPEID::PROTOSS_PROBE;
        case UNIT_TYPEID::PROTOSS_CARRIER:
            return UNIT_TYPEID::PROTOSS_STARGATE;
        case UNIT_TYPEID::PROTOSS_COLOSSUS:
            return UNIT_TYPEID::PROTOSS_ROBOTICSFACILITY;
        case UNIT_TYPEID::PROTOSS_CYBERNETICSCORE:
            return UNIT_TYPEID::PROTOSS_PROBE;
        case UNIT_TYPEID::PROTOSS_DARKSHRINE:
            return UNIT_TYPEID::PROTOSS_PROBE;
        case UNIT_TYPEID::PROTOSS_DARKTEMPLAR:
            return UNIT_TYPEID::PROTOSS_GATEWAY;
        case UNIT_TYPEID::PROTOSS_DISRUPTOR:
            return UNIT_TYPEID::PROTOSS_ROBOTICSFACILITY;
        case UNIT_TYPEID::PROTOSS_FLEETBEACON:
            return UNIT_TYPEID::PROTOSS_PROBE;
        case UNIT_TYPEID::PROTOSS_FORGE:
            return UNIT_TYPEID::PROTOSS_PROBE;
        case UNIT_TYPEID::PROTOSS_GATEWAY:
            return UNIT_TYPEID::PROTOSS_PROBE;
        case UNIT_TYPEID::PROTOSS_HIGHTEMPLAR:
            return UNIT_TYPEID::PROTOSS_GATEWAY;
        case UNIT_TYPEID::PROTOSS_IMMORTAL:
            return UNIT_TYPEID::PROTOSS_ROBOTICSFACILITY;
        case UNIT_TYPEID::PROTOSS_INTERCEPTOR:
            return UNIT_TYPEID::PROTOSS_CARRIER;
        case UNIT_TYPEID::PROTOSS_MOTHERSHIP:
            return UNIT_TYPEID::PROTOSS_NEXUS;
        case UNIT_TYPEID::PROTOSS_NEXUS:
            return UNIT_TYPEID::PROTOSS_PROBE;
        case UNIT_TYPEID::PROTOSS_OBSERVER:
            return UNIT_TYPEID::PROTOSS_ROBOTICSFACILITY;
        case UNIT_TYPEID::PROTOSS_ORACLE:
            return UNIT_TYPEID::PROTOSS_STARGATE;
        case UNIT_TYPEID::PROTOSS_PHOENIX:
            return UNIT_TYPEID::PROTOSS_STARGATE;
        case UNIT_TYPEID::PROTOSS_PHOTONCANNON:
            return UNIT_TYPEID::PROTOSS_PROBE;
        case UNIT_TYPEID::PROTOSS_PROBE:
            return UNIT_TYPEID::PROTOSS_NEXUS;
        case UNIT_TYPEID::PROTOSS_PYLON:
            return UNIT_TYPEID::PROTOSS_PROBE;
        case UNIT_TYPEID::PROTOSS_ROBOTICSBAY:
            return UNIT_TYPEID::PROTOSS_PROBE;
        case UNIT_TYPEID::PROTOSS_ROBOTICSFACILITY:
            return UNIT_TYPEID::PROTOSS_PROBE;
        case UNIT_TYPEID::PROTOSS_SENTRY:
            return UNIT_TYPEID::PROTOSS_GATEWAY;
        case UNIT_TYPEID::PROTOSS_SHIELDBATTERY:
            return UNIT_TYPEID::PROTOSS_PROBE;
        case UNIT_TYPEID::PROTOSS_STALKER:
            return UNIT_TYPEID::PROTOSS_GATEWAY;
        case UNIT_TYPEID::PROTOSS_STARGATE:
            return UNIT_TYPEID::PROTOSS_PROBE;
        case UNIT_TYPEID::PROTOSS_TEMPEST:
            return UNIT_TYPEID::PROTOSS_STARGATE;
        case UNIT_TYPEID::PROTOSS_TEMPLARARCHIVE:
            return UNIT_TYPEID::PROTOSS_PROBE;
        case UNIT_TYPEID::PROTOSS_TWILIGHTCOUNCIL:
            return UNIT_TYPEID::PROTOSS_PROBE;
        case UNIT_TYPEID::PROTOSS_VOIDRAY:
            return UNIT_TYPEID::PROTOSS_STARGATE;
        case UNIT_TYPEID::PROTOSS_WARPPRISM:
            return UNIT_TYPEID::PROTOSS_ROBOTICSFACILITY;
        case UNIT_TYPEID::PROTOSS_ZEALOT:
            return UNIT_TYPEID::PROTOSS_GATEWAY;

        // Terran Units - no need to complete
        case UNIT_TYPEID::TERRAN_BARRACKS:
            return UNIT_TYPEID::TERRAN_SCV;
        case UNIT_TYPEID::TERRAN_COMMANDCENTER:
            return UNIT_TYPEID::TERRAN_SCV;
        case UNIT_TYPEID::TERRAN_MARINE:
            return UNIT_TYPEID::TERRAN_BARRACKS;
        case UNIT_TYPEID::TERRAN_SCV:
            return UNIT_TYPEID::TERRAN_COMMANDCENTER;

        // Should never be reached
        default:
            // Program broke because unit you asked for wasn't added to the switch case yet. Add it.
            std::cout << "ERROR: GetUnitBuilder could not find a builder for [" << unitToBuild.to_string() << "]. Add this to the switch case." << std::endl;
            return UNIT_TYPEID::INVALID;
    }
}

AbilityID BetaStar::GetUnitBuildAbility(UnitTypeID unitToBuild)
{
    // TODO: Warping units is a different action. Not represented.
    switch (unitToBuild.ToType())
    {
        // Protoss Build Actions
        case UNIT_TYPEID::PROTOSS_ADEPT:
            return ABILITY_ID::TRAIN_ADEPT;
        case UNIT_TYPEID::PROTOSS_ASSIMILATOR:
            return ABILITY_ID::BUILD_ASSIMILATOR;
        case UNIT_TYPEID::PROTOSS_CARRIER:
            return ABILITY_ID::TRAIN_CARRIER;
        case UNIT_TYPEID::PROTOSS_COLOSSUS:
            return ABILITY_ID::TRAIN_COLOSSUS;
        case UNIT_TYPEID::PROTOSS_CYBERNETICSCORE:
            return ABILITY_ID::BUILD_CYBERNETICSCORE;
        case UNIT_TYPEID::PROTOSS_DARKSHRINE:
            return ABILITY_ID::BUILD_DARKSHRINE;
        case UNIT_TYPEID::PROTOSS_DARKTEMPLAR:
            return ABILITY_ID::TRAIN_DARKTEMPLAR;
        case UNIT_TYPEID::PROTOSS_DISRUPTOR:
            return ABILITY_ID::TRAIN_DISRUPTOR;
        case UNIT_TYPEID::PROTOSS_FLEETBEACON:
            return ABILITY_ID::BUILD_FLEETBEACON;
        case UNIT_TYPEID::PROTOSS_FORGE:
            return ABILITY_ID::BUILD_FORGE;
        case UNIT_TYPEID::PROTOSS_GATEWAY:
            return ABILITY_ID::BUILD_GATEWAY;
        case UNIT_TYPEID::PROTOSS_HIGHTEMPLAR:
            return ABILITY_ID::TRAIN_HIGHTEMPLAR;
        case UNIT_TYPEID::PROTOSS_IMMORTAL:
            return ABILITY_ID::TRAIN_IMMORTAL;
        case UNIT_TYPEID::PROTOSS_INTERCEPTOR:
            return ABILITY_ID::BUILD_INTERCEPTORS;
        case UNIT_TYPEID::PROTOSS_MOTHERSHIP:
            return ABILITY_ID::TRAIN_MOTHERSHIP;
        case UNIT_TYPEID::PROTOSS_NEXUS:
            return ABILITY_ID::BUILD_NEXUS;
        case UNIT_TYPEID::PROTOSS_OBSERVER:
            return ABILITY_ID::TRAIN_OBSERVER;
        case UNIT_TYPEID::PROTOSS_ORACLE:
            return ABILITY_ID::TRAIN_ORACLE;
        case UNIT_TYPEID::PROTOSS_PHOENIX:
            return ABILITY_ID::TRAIN_PHOENIX;
        case UNIT_TYPEID::PROTOSS_PHOTONCANNON:
            return ABILITY_ID::BUILD_PHOTONCANNON;
        case UNIT_TYPEID::PROTOSS_PROBE:
            return ABILITY_ID::TRAIN_PROBE;
        case UNIT_TYPEID::PROTOSS_PYLON:
            return ABILITY_ID::BUILD_PYLON;
        case UNIT_TYPEID::PROTOSS_ROBOTICSBAY:
            return ABILITY_ID::BUILD_ROBOTICSBAY;
        case UNIT_TYPEID::PROTOSS_ROBOTICSFACILITY:
            return ABILITY_ID::BUILD_ROBOTICSFACILITY;
        case UNIT_TYPEID::PROTOSS_SENTRY:
            return ABILITY_ID::TRAIN_SENTRY;
        case UNIT_TYPEID::PROTOSS_SHIELDBATTERY:
            return ABILITY_ID::BUILD_SHIELDBATTERY;
        case UNIT_TYPEID::PROTOSS_STALKER:
            return ABILITY_ID::TRAIN_STALKER;
        case UNIT_TYPEID::PROTOSS_STARGATE:
            return ABILITY_ID::BUILD_STARGATE;
        case UNIT_TYPEID::PROTOSS_TEMPEST:
            return ABILITY_ID::TRAIN_TEMPEST;
        case UNIT_TYPEID::PROTOSS_TEMPLARARCHIVE:
            return ABILITY_ID::BUILD_TEMPLARARCHIVE;
        case UNIT_TYPEID::PROTOSS_TWILIGHTCOUNCIL:
            return ABILITY_ID::BUILD_TWILIGHTCOUNCIL;
        case UNIT_TYPEID::PROTOSS_VOIDRAY:
            return ABILITY_ID::TRAIN_VOIDRAY;
        case UNIT_TYPEID::PROTOSS_WARPPRISM:
            return ABILITY_ID::TRAIN_WARPPRISM;
        case UNIT_TYPEID::PROTOSS_ZEALOT:
            return ABILITY_ID::TRAIN_ZEALOT;

        // Terran Build Actions - no need to complete
        case UNIT_TYPEID::TERRAN_BARRACKS:
            return ABILITY_ID::BUILD_BARRACKS;
        case UNIT_TYPEID::TERRAN_COMMANDCENTER:
            return ABILITY_ID::BUILD_COMMANDCENTER;
        case UNIT_TYPEID::TERRAN_MARINE:
            return ABILITY_ID::TRAIN_MARINE;
        case UNIT_TYPEID::TERRAN_SCV:
            return ABILITY_ID::TRAIN_SCV;

        // Should never be reached
        default:
            // Program broke because unit you asked for wasn't added to the switch case yet. Add it.
            std::cout << "ERROR: GetUnitBuildAbility could not find a build command for [" << unitToBuild.to_string() << "]. Add this to the switch case." << std::endl;
            return ABILITY_ID::INVALID;
    }
}

bool BetaStar::TrainUnit(UnitTypeID unitType)
{
    Units buildings = Observation()->GetUnits(Unit::Alliance::Self, IsUnit(GetUnitBuilder(unitType)));

    // we don't have any building that can build this unit
    if (buildings.size() == 0)
    {
        return false;
    }

    // get random building from vector (slight bias to lower indeces) and build unit there
    Actions()->UnitCommand(buildings[rand() % buildings.size()], GetUnitBuildAbility(unitType));

    return true;
}

bool BetaStar::TrainUnit(const Unit *building, UnitTypeID unitType)
{
    // We've asked a building that can't build something to build it. Exit.
    if (GetUnitBuilder(unitType) != building->unit_type)
    {
        return false;
    }

    // May need to queue some commands? 
    Actions()->UnitCommand(building, GetUnitBuildAbility(unitType));

    return true;
}

size_t BetaStar::TrainUnitMultiple(UnitTypeID unitType)
{
    Units buildings = Observation()->GetUnits(Unit::Alliance::Self, IsUnit(GetUnitBuilder(unitType)));

    // May need to queue some commands? 
    Actions()->UnitCommand(buildings, GetUnitBuildAbility(unitType));

    return buildings.size();
}

size_t BetaStar::TrainUnitMultiple(const Units &buildings, UnitTypeID unitType)
{
    for (const Unit* building : buildings)
    {
        // We've asked a building that can't build something to build it. Exit.
        // Possible case of non-homogenous selection of buildings
        if (GetUnitBuilder(unitType) != building->unit_type)
        {
            return 0;
        }
    }

    // May need to queue some commands? 
    Actions()->UnitCommand(buildings, GetUnitBuildAbility(unitType));

    return buildings.size();
}

void BetaStar::TrainWorkers() {
    for (const auto& base : FriendlyUnitsOfType(m_base_typeid)) {
        if (Observation()->GetMinerals() >= 50 && base->orders.size() == 0 && std::max(0, Observation()->GetFoodCap() - Observation()->GetFoodUsed()) != 0 && NeedWorkers()) {
            Actions()->UnitCommand(base, m_worker_train_ability);
        }
    }
}
