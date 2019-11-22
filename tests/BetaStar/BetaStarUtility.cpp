#include "BetaStar.h"

using namespace sc2;

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

    return TryIssueCommand(unit_to_build, ability_type_for_structure, Point2D(unit_to_build->pos.x + rx * 15.0f, unit_to_build->pos.y + ry * 15.0f));
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
        // attempt to issue the build command in the valid location
        return TryIssueCommand(unit, ability_type_for_structure, target);
    }
    return false;
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

AbilityID BetaStar::GetUnitWarpAbility(UnitTypeID unitToWarp)
{
    switch (unitToWarp.ToType())
    {
        case UNIT_TYPEID::PROTOSS_ZEALOT:
            return ABILITY_ID::TRAINWARP_ZEALOT;
        case UNIT_TYPEID::PROTOSS_SENTRY:
            return ABILITY_ID::TRAINWARP_SENTRY;
        case UNIT_TYPEID::PROTOSS_STALKER:
            return ABILITY_ID::TRAINWARP_STALKER;
        case UNIT_TYPEID::PROTOSS_ADEPT:
            return ABILITY_ID::TRAINWARP_ADEPT;
        case UNIT_TYPEID::PROTOSS_HIGHTEMPLAR:
            return ABILITY_ID::TRAINWARP_HIGHTEMPLAR;
        case UNIT_TYPEID::PROTOSS_DARKTEMPLAR:
            return ABILITY_ID::TRAINWARP_DARKTEMPLAR;
        // unit cannot be warped
        default:
            return ABILITY_ID::INVALID;
    }
}

bool BetaStar::IsStructure(UnitTypeID unitType)
{
    switch (unitType.ToType())
    {
        case UNIT_TYPEID::PROTOSS_ASSIMILATOR:
        case UNIT_TYPEID::PROTOSS_CYBERNETICSCORE:
        case UNIT_TYPEID::PROTOSS_DARKSHRINE:
        case UNIT_TYPEID::PROTOSS_FLEETBEACON:
        case UNIT_TYPEID::PROTOSS_FORGE:
        case UNIT_TYPEID::PROTOSS_GATEWAY:
        case UNIT_TYPEID::PROTOSS_NEXUS:
        case UNIT_TYPEID::PROTOSS_PHOTONCANNON:
        case UNIT_TYPEID::PROTOSS_PYLON:
        case UNIT_TYPEID::PROTOSS_PYLONOVERCHARGED:
        case UNIT_TYPEID::PROTOSS_ROBOTICSBAY:
        case UNIT_TYPEID::PROTOSS_ROBOTICSFACILITY:
        case UNIT_TYPEID::PROTOSS_STARGATE:
        case UNIT_TYPEID::PROTOSS_TEMPLARARCHIVE:
        case UNIT_TYPEID::PROTOSS_TWILIGHTCOUNCIL:
        case UNIT_TYPEID::PROTOSS_WARPGATE:
        case UNIT_TYPEID::TERRAN_ARMORY:
        case UNIT_TYPEID::TERRAN_AUTOTURRET:
        case UNIT_TYPEID::TERRAN_BARRACKS:
        case UNIT_TYPEID::TERRAN_BARRACKSFLYING:
        case UNIT_TYPEID::TERRAN_BARRACKSREACTOR:
        case UNIT_TYPEID::TERRAN_BARRACKSTECHLAB:
        case UNIT_TYPEID::TERRAN_BUNKER:
        case UNIT_TYPEID::TERRAN_COMMANDCENTER:
        case UNIT_TYPEID::TERRAN_COMMANDCENTERFLYING:
        case UNIT_TYPEID::TERRAN_ENGINEERINGBAY:
        case UNIT_TYPEID::TERRAN_FACTORY:
        case UNIT_TYPEID::TERRAN_FACTORYFLYING:
        case UNIT_TYPEID::TERRAN_FACTORYREACTOR:
        case UNIT_TYPEID::TERRAN_FACTORYTECHLAB:
        case UNIT_TYPEID::TERRAN_FUSIONCORE:
        case UNIT_TYPEID::TERRAN_GHOSTACADEMY:
        case UNIT_TYPEID::TERRAN_MISSILETURRET:
        case UNIT_TYPEID::TERRAN_ORBITALCOMMAND:
        case UNIT_TYPEID::TERRAN_ORBITALCOMMANDFLYING:
        case UNIT_TYPEID::TERRAN_PLANETARYFORTRESS:
        case UNIT_TYPEID::TERRAN_POINTDEFENSEDRONE:
        case UNIT_TYPEID::TERRAN_REACTOR:
        case UNIT_TYPEID::TERRAN_REFINERY:
        case UNIT_TYPEID::TERRAN_SENSORTOWER:
        case UNIT_TYPEID::TERRAN_STARPORT:
        case UNIT_TYPEID::TERRAN_STARPORTFLYING:
        case UNIT_TYPEID::TERRAN_STARPORTREACTOR:
        case UNIT_TYPEID::TERRAN_STARPORTTECHLAB:
        case UNIT_TYPEID::TERRAN_SUPPLYDEPOT:
        case UNIT_TYPEID::TERRAN_SUPPLYDEPOTLOWERED:
        case UNIT_TYPEID::TERRAN_TECHLAB:
        case UNIT_TYPEID::ZERG_BANELINGNEST:
        case UNIT_TYPEID::ZERG_CREEPTUMOR:
        case UNIT_TYPEID::ZERG_CREEPTUMORBURROWED:
        case UNIT_TYPEID::ZERG_CREEPTUMORQUEEN:
        case UNIT_TYPEID::ZERG_EVOLUTIONCHAMBER:
        case UNIT_TYPEID::ZERG_EXTRACTOR:
        case UNIT_TYPEID::ZERG_GREATERSPIRE:
        case UNIT_TYPEID::ZERG_HATCHERY:
        case UNIT_TYPEID::ZERG_HIVE:
        case UNIT_TYPEID::ZERG_HYDRALISKDEN:
        case UNIT_TYPEID::ZERG_INFESTATIONPIT:
        case UNIT_TYPEID::ZERG_LAIR:
        case UNIT_TYPEID::ZERG_NYDUSCANAL:
        case UNIT_TYPEID::ZERG_NYDUSNETWORK:
        case UNIT_TYPEID::ZERG_ROACHWARREN:
        case UNIT_TYPEID::ZERG_SPAWNINGPOOL:
        case UNIT_TYPEID::ZERG_SPIRE:
        case UNIT_TYPEID::ZERG_SPINECRAWLER:
        case UNIT_TYPEID::ZERG_SPINECRAWLERUPROOTED:
        case UNIT_TYPEID::ZERG_SPORECRAWLER:
        case UNIT_TYPEID::ZERG_SPORECRAWLERUPROOTED:
        case UNIT_TYPEID::ZERG_ULTRALISKCAVERN:
            return true;
        default:
            return false;
    }
}

bool BetaStar::TrainUnit(UnitTypeID unitType)
{
    const UnitTypeID unitBuilder = GetUnitBuilder(unitType);

    Units buildings = Observation()->GetUnits(Unit::Alliance::Self, IsUnit(unitBuilder));
    // warpgates = gateways, so we can build at them too
    if (unitBuilder == UNIT_TYPEID::PROTOSS_GATEWAY)
    {
        Units warpgates = Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_WARPGATE));
        if (!warpgates.empty())
        {
            buildings.insert(buildings.end(), warpgates.begin(), warpgates.end());
        }
    }

    // we don't have any building that can build this unit
    if (buildings.size() == 0)
    {
        return false;
    }

    // get random building from vector and build unit there
    return TrainUnit(GetRandomEntry(buildings), unitType);
}

bool BetaStar::TrainUnit(const Unit *building, UnitTypeID unitType)
{
    // note that all TrainUnit variants resolve themselves here, so this is where we can do all of our
    // checks and handle edge-cases

    // if a building isn't finished, it can't build units
    if (building->build_progress < 1.0f)
    {
        return false;
    }

    // only the nexus can build units without power
    if (building->unit_type != UNIT_TYPEID::PROTOSS_NEXUS && !building->is_powered)
    {
        return false;
    }

    UnitTypeID unitBuilder = GetUnitBuilder(unitType);

    // building at a warpgate instead of a gateway (warp close to warpgate)
    if (unitBuilder == UNIT_TYPEID::PROTOSS_GATEWAY && building->unit_type == UNIT_TYPEID::PROTOSS_WARPGATE)
    {
        // if we're attacking, warp as close to the enemy as possible
        if (m_attacking)
        {
            return WarpUnit(building, m_enemy_base_pos, unitType);
        }
        // if we're building up forces, stick near production building
        else
        {
            return WarpUnit(building, building->pos, unitType);
        }     
    }
    // normal training process
    else if (unitBuilder == building->unit_type)
    {
        AbilityID buildAbility = GetUnitBuildAbility(unitType);
        for (UnitOrder order : building->orders)
        {
            // if we have this unit queued to build already, don't queue another one
            if (order.ability_id == buildAbility)
            {
                return false;
            }
        }

        // Return true if training succeeds, false otherwise
        return TryIssueCommand(building, buildAbility);
    }

    // couldn't build that unit at that building - should probably never reach this exit point
    return false;
}

bool BetaStar::WarpUnit(Point2D warpLocation, UnitTypeID unitType)
{
    Units warpgates = FriendlyUnitsOfType(UNIT_TYPEID::PROTOSS_WARPGATE);

    // we don't have any warpgates
    if (warpgates.size() == 0)
    {
        return false;
    }

    // get random warpgate and warp unit in with that warpgate
    return WarpUnit(GetRandomEntry(warpgates), warpLocation, unitType);
}

bool BetaStar::WarpUnit(const Unit *building, Point2D warpLocation, UnitTypeID unitType)
{
    // if a building isn't finished, it can't build units
    if (building->build_progress < 1.0f)
    {
        return false;
    }

    // if our warpgate isn't powered, it can't build units
    if (!building->is_powered)
    {
        return false;
    }

    const ObservationInterface* observation = Observation();
    const GameInfo gameInfo = observation->GetGameInfo();

    std::vector<PowerSource> powerSources = observation->GetPowerSources();

    if (powerSources.empty())
    {
        return false;
    }

    // find the closest power source to the specified point to warp in around
    size_t minIndex = 0;
    float minDist = DistanceSquared2D(warpLocation, powerSources[0].position);
    for (size_t i = 1; i < powerSources.size(); ++i)
    {
        float testDist = DistanceSquared2D(warpLocation, powerSources[i].position);
        if (testDist < minDist)
        {
            minDist = testDist;
            minIndex = i;
        }
    }

    const PowerSource &targetPowerSource = powerSources[minIndex];
    float randomX = GetRandomScalar();
    float randomY = GetRandomScalar();
    Point2D finalWarpPoint = Point2D(targetPowerSource.position.x + randomX * targetPowerSource.radius, targetPowerSource.position.y + randomY * targetPowerSource.radius);

    // If the warp location is walled off, don't warp there.
    // We check this to see if there is pathing from the build location to the center of the map
    if (Query()->PathingDistance(finalWarpPoint, Point2D(gameInfo.playable_max.x / 2, gameInfo.playable_max.y / 2)) < .01f) {
        return false;
    }

    // If this succeeds, return true. Otherwise, return false.
    return TryIssueCommand(building, GetUnitWarpAbility(unitType), finalWarpPoint);
}

size_t BetaStar::MassTrainUnit(UnitTypeID unitType)
{
    UnitTypeID unitBuilder = GetUnitBuilder(unitType);

    Units buildings = Observation()->GetUnits(Unit::Alliance::Self, IsUnit(unitBuilder));
    // warpgates = gateways, so build for them too
    if (unitBuilder == UNIT_TYPEID::PROTOSS_GATEWAY)
    {
        Units warpgates = Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_WARPGATE));
        if (!warpgates.empty())
        {
            buildings.insert(buildings.end(), warpgates.begin(), warpgates.end());
        }
    }

    size_t buildCount = 0;
    for (const Unit* building : buildings)
    {
        if (TrainUnit(building, unitType))
        {
            ++buildCount;
        }
    }

    return buildCount;
}

void BetaStar::TryBuildStructureNearPylon(AbilityID ability_type_for_structure, UnitTypeID unit_type = UNIT_TYPEID::PROTOSS_PROBE) {
    const ObservationInterface* observation = Observation();

    std::vector<PowerSource> power_sources = observation->GetPowerSources();

    if (power_sources.empty()) {
        return;
    }

    int num_tries = 10;

    for (int i = 0; i < num_tries; ++i) {
        const PowerSource& random_power_source = GetRandomEntry(power_sources);
        
        if (observation->GetUnit(random_power_source.tag)->unit_type == UNIT_TYPEID::PROTOSS_WARPPRISM) {
            continue;
        }

        float radius = random_power_source.radius;
        float rx = GetRandomScalar();
        float ry = GetRandomScalar();
        Point2D build_location = Point2D(random_power_source.position.x + rx * radius, random_power_source.position.y + ry * radius);

        if (DistanceSquared2D(build_location, m_starting_pos) > 10000) {
            continue;
        }

        if (Query()->Placement(ability_type_for_structure, build_location)) {
            Units workers = FriendlyUnitsOfType(unit_type);
            for (const auto& worker : workers) {
                for (const auto& order : worker->orders) {
                    if (order.ability_id == ability_type_for_structure) {
                        return;
                    }
                }
            }
            const Unit* closest_worker = nullptr;
            float closest_distance = std::numeric_limits<float>::max();
            for (const auto& worker : workers) {
                float distance = DistanceSquared2D(build_location, worker->pos);
                if (distance < closest_distance) {
                    closest_worker = worker;
                    closest_distance = distance;
                }
            }
            if (closest_worker == nullptr) {
                return;
            }

            // try to build the structure in the valid build location
            TryIssueCommand(closest_worker, ability_type_for_structure, build_location);

            // if we get here, we either succeeded or can't succeed. Exit.
            return;
        }
    }
}

void BetaStar::TryResearchUpgrade(AbilityID upgrade_abilityid, UnitTypeID building_type)
{
    const ObservationInterface* observation = Observation();

    Units buildings = FriendlyUnitsOfType(building_type);

    // check whether the upgrade is already being researched
    for (const auto& building : buildings) {
        if (building->build_progress != 1) {
            continue;
        }
        for (const auto& order : building->orders) {
            if (order.ability_id == upgrade_abilityid) {
                return;
            }
        }
    }

    // look for unoccupied building, research upgrade
    for (const auto& building : buildings) {
        if (building->build_progress != 1) {
            continue;
        }
        if (building->orders.size() == 0) {
            // if we're successful in researching the upgrade, exit
            if (TryIssueCommand(building, upgrade_abilityid))
            {
                return;
            }
        }
    }
}

void BetaStar::ClearArmyRatios()
{
    army_ratios.clear();
}

size_t BetaStar::CountUnitType(UnitTypeID unitType, bool includeIncomplete)
{
    const ObservationInterface *observation = Observation();

    const Units allUnits = observation->GetUnits(Unit::Alliance::Self, IsUnit(unitType));

    size_t count = 0;

    // count structures differently than units
    if (IsStructure(unitType))
    {
        // buildings under construction will be included in allUnits
        if (includeIncomplete)
        {
            count = allUnits.size();
        }
        // only count buildings that are fully constructed
        else
        {
            for (const Unit *building : allUnits)
            {
                if (building->build_progress == 1.0f)
                {
                    ++count;
                }
            }
        }
    }
    else
    {
        count = allUnits.size();
        // units in the queue are not included in allUnits
        if (includeIncomplete)
        {
            const Units trainingUnits = observation->GetUnits(Unit::Alliance::Self, IsUnit(GetUnitBuilder(unitType)));
            for (const Unit *trainer : trainingUnits)
            {
                AbilityID buildAbility = GetUnitBuildAbility(unitType);
                for (UnitOrder order : trainer->orders)
                {
                    if (order.ability_id == buildAbility)
                    {
                        ++count;
                    }
                }
            }
        }
    }

    return count;
}

void BetaStar::SetStrategy(Strategy newStrategy)
{
    m_current_strategy = newStrategy;
    // Set all ratios to 0. No new units will be built automatically.
    ClearArmyRatios();

    // Set new army ratios based on the strategy (can be fine-tuned elsewhere)
    switch (newStrategy)
    {
        case Strategy::Blink_Stalker_Rush:
            army_ratios[UNIT_TYPEID::PROTOSS_STALKER] = 1.0f;
            break;
    }
}

bool BetaStar::TryIssueCommand(const Unit *unit, AbilityID ability)
{
    // Generic catch for when we can't use this ability (i.e. can't afford it, don't have the upgrades, on cooldown, etc.)
    AvailableAbilities abilities = Query()->GetAbilitiesForUnit(unit);
    for (const auto &tempAbility : abilities.abilities)
    {
        if (tempAbility.ability_id == ability)
        {
            Actions()->UnitCommand(unit, ability);
            return true;
        }
    }
    return false;
}

bool BetaStar::TryIssueCommand(const Unit *unit, AbilityID ability, const Unit *target)
{
    // Generic catch for when we can't use this ability (i.e. can't afford it, don't have the upgrades, on cooldown, etc.)
    AvailableAbilities abilities = Query()->GetAbilitiesForUnit(unit);
    for (const auto &tempAbility : abilities.abilities)
    {
        if (tempAbility.ability_id == ability)
        {
            Actions()->UnitCommand(unit, ability, target);
            return true;
        }
    }
    return false;
}

bool BetaStar::TryIssueCommand(const Unit *unit, AbilityID ability, Point2D point)
{
    // Generic catch for when we can't use this ability (i.e. can't afford it, don't have the upgrades, on cooldown, etc.)
    AvailableAbilities abilities = Query()->GetAbilitiesForUnit(unit);
    for (const auto &tempAbility : abilities.abilities)
    {
        if (tempAbility.ability_id == ability)
        {
            Actions()->UnitCommand(unit, ability, point);
            return true;
        }
    }
    return false;
}

const Unit* BetaStar::GetClosestUnit(Point2D position, const Units units)
{
    // empty collections of units should never be passed
    if (units.empty())
    {
        return nullptr;
    }

    size_t minIndex = 0;
    float minDist = DistanceSquared2D(position, units[minIndex]->pos);

    for (size_t i = 1; i < units.size(); ++i)
    {
        float testDist = DistanceSquared2D(position, units[i]->pos);
        if (testDist < minDist)
        {
            minDist = testDist;
            minIndex = i;
        }
    }

    return units[minIndex];
}
