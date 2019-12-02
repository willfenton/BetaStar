#include "BetaStar.h"

using namespace sc2;


// called each time the coordinator steps the simulation forward
void BetaStar::OnStep() {
    const ObservationInterface* observation = Observation();
    const GameInfo game_info = observation->GetGameInfo();

    //std::cout << Observation()->GetGameLoop() << std::endl;

    //OnStepComputeStatistics();

    if (m_warpgate_researched) {
        for (const auto& gateway : FriendlyUnitsOfType(UNIT_TYPEID::PROTOSS_GATEWAY)) {
            if (AlmostEqual(gateway->build_progress, 1.0f) && gateway->orders.size() == 0) {
                Actions()->UnitCommand(gateway, ABILITY_ID::MORPH_WARPGATE);
            }
        }
    }

    OnStepTrainWorkers();

    // for finding positions of points
    //for (const auto& pylon : FriendlyUnitsOfType(UNIT_TYPEID::PROTOSS_PYLON)) {
    //    std::cout << "(" << pylon->pos.x << "," << pylon->pos.y << ")" << std::endl;
    //}
    //if (CountUnitType(UNIT_TYPEID::PROTOSS_PYLON) > 0) {
    //    std::cout << std::endl;
    //}

    OnStepBuildPylons();

    //OnStepBuildGas();

    //OnStepExpand();

    OnStepManageWorkers();

    OnStepBuildArmy();

    OnStepBuildOrder();

    OnStepManageArmy();

    OnStepResearchUpgrades();

    // do this at the end so that it can properly boost new production
    OnStepChronoBoost();
}

void BetaStar::OnGameStart()
{
    const ObservationInterface* observation = Observation();
    const GameInfo game_info = observation->GetGameInfo();

    std::cout << "I am player number " << observation->GetPlayerID() << std::endl;
    //Actions()->SendChat("gl hf");

    // get position of first command center
    m_starting_pos = observation->GetStartLocation();
    m_starting_quadrant = GetQuadrantByPoint(m_starting_pos);
    std::cout << "Start location: (" << m_starting_pos.x << "," << m_starting_pos.y << ")" << std::endl;

    // Decent default values in the case where something goes wrong with scouting
    m_enemy_base_pos = m_starting_pos;
    m_enemy_base_quadrant = m_starting_quadrant;

    m_army_rally_pos = RotatePosition(m_army_rally_pos, m_starting_quadrant);

    // calculate all expansion locations (this takes a while so we do it at the start of the game)
    // m_expansion_locations = search::CalculateExpansionLocations(Observation(), Query());
    m_expansion_locations.push_back(m_starting_pos);
    m_expansion_locations.push_back(Point2D(59.5f, 54.5f));
    m_expansion_locations.push_back(Point2D(30.5f, 66.5f));
    m_expansion_locations.push_back(Point2D(35.5, 93.5));
    
    for (size_t q = 1; q <= 3; ++q)
    {
        for (size_t i = 0; i < 4; ++i)
        {
            m_expansion_locations.push_back(RotatePosition(m_expansion_locations[i], q));
        }
    }

    /*for (Point2D loc : m_expansion_locations)
    {
        std::cout << loc.x << "," << loc.y << std::endl;
    }*/

    // cache info about all SC2 units
    all_unit_type_data = observation->GetUnitTypeData(true);
    // cache info about all SC2 abilities
    all_ability_data = observation->GetAbilityData(true);
    // cache info about all SC2 upgrades
    all_upgrades = observation->GetUpgradeData(true);
    // cache info about all SC2 buffs
    all_buffs = observation->GetBuffData(true);

    /* SCOUTING */

    // pick a random worker to be our scout
    Units workers = FriendlyUnitsOfType(m_worker_typeid);
    m_initial_scouting_probe = GetRandomEntry(workers);

    // this is a list of points where the enemy could have started
    std::vector<Point2D> enemy_start_locations = game_info.enemy_start_locations;
    size_t size = enemy_start_locations.size();

    // command the scout probe to visit all possible enemy starting locations
    // this big block of code just makes the probe visit them in optimal order
    // after this, the scouting probe will have move orders queued for each enemy start location
    Actions()->UnitCommand(m_initial_scouting_probe, ABILITY_ID::MOVE, m_initial_scouting_probe->pos);
    Point2D last_location = m_initial_scouting_probe->pos;
    for (size_t i = 0; i < size; ++i) {
        Point2D closest_start_location;
        float closest_distance = std::numeric_limits<float>::max();
        for (const auto& enemy_start_location : enemy_start_locations) {
            float distance = DistanceSquared2D(last_location, enemy_start_location);
            if (distance < closest_distance) {
                closest_start_location = enemy_start_location;
                closest_distance = distance;
            }
        }
        Actions()->UnitCommand(m_initial_scouting_probe, ABILITY_ID::MOVE, closest_start_location, true);
        last_location = closest_start_location;
        enemy_start_locations.erase(std::find(enemy_start_locations.begin(), enemy_start_locations.end(), closest_start_location));
    }

    // Testing basic blink stalker strat (should be set dynamically based on intelligence about enemy and our win/loss record)
    SetStrategy(Strategy::Blink_Stalker_Rush);
}

// Called each time a unit has been built and has no orders or the unit had orders in the previous step and currently does not
// Both buildings and units are considered units and are represented with a Unit object.
void BetaStar::OnUnitIdle(const Unit* unit)
{
    //switch (unit->unit_type.ToType()) {

    //    //case UNIT_TYPEID::PROTOSS_PROBE: {
    //    //    const Unit* target = FindResourceToGather(unit->pos);
    //    //    Actions()->UnitCommand(unit, ABILITY_ID::HARVEST_GATHER, target);
    //    //    break;
    //    //}

    //    default: {
    //        //std::cout << "Idle unit of type " << unit->unit_type.to_string() << std::endl;
    //        break;
    //    }
    //}
}

void BetaStar::OnBuildingConstructionComplete(const Unit* unit)
{
    switch (unit->unit_type.ToType()) {

        case UNIT_TYPEID::PROTOSS_PYLON: {
            if (!m_forward_pylon_complete && DistanceSquared2D(unit->pos, m_forward_pylon_pos) < 10) {
                std::cout << "Proxy pylon completed" << std::endl;
                m_forward_pylon_complete = true;
            }
            break;
        }
    }
}

void BetaStar::OnUnitEnterVision(const Unit* unit)
{
    // initial scouting probe found it
    if (!m_enemy_base_scouted && IsStructure(unit->unit_type) && DistanceSquared2D(unit->pos, m_initial_scouting_probe->pos) < 150) {
        // find closest enemy starting location
        Point2D closest_enemy_start_location;
        float closest_distance = std::numeric_limits<float>::max();
        for (const auto& enemy_start_location : Observation()->GetGameInfo().enemy_start_locations) {
            float distance = DistanceSquared2D(unit->pos, enemy_start_location);
            if (distance < closest_distance) {
                closest_enemy_start_location = enemy_start_location;
                closest_distance = distance;
            }
        }
        if (closest_distance < std::numeric_limits<float>::max()) {
            std::cout << "Enemy start location found: (" << closest_enemy_start_location.x << "," << closest_enemy_start_location.y << ")" << std::endl;
            m_enemy_base_pos = closest_enemy_start_location;
            m_enemy_base_quadrant = GetQuadrantByPoint(m_enemy_base_pos);
            m_forward_pylon_pos = RotatePosition(m_forward_pylon_pos, m_enemy_base_quadrant);
            m_enemy_base_scouted = true;
            Actions()->UnitCommand(m_initial_scouting_probe, ABILITY_ID::MOVE, m_starting_pos);
        }
    }

    // gather meta-information about opponent - does not take any actions
    GatherIntelligence(unit);

    //For testing purposes
    //std::cout << "Unit Priority is: " << GetUnitAttackPriority(unit) << std::endl;

    // TODO: Respond to threats
}

void BetaStar::OnUpgradeCompleted(UpgradeID upgrade_id)
{
    const ObservationInterface *observation = Observation();

    // re-cache info about all SC2 units
    all_unit_type_data = observation->GetUnitTypeData(true);
    // re-cache info about all SC2 abilities
    all_ability_data = observation->GetAbilityData(true);
    // re-cache info about all SC2 upgrades
    all_upgrades = observation->GetUpgradeData(true);
    // re-cache info about all SC2 buffs
    all_buffs = observation->GetBuffData(true);

    switch (upgrade_id.ToType()) {
        case UPGRADE_ID::WARPGATERESEARCH: {
            std::cout << "Warpgate research complete" << std::endl;
            m_warpgate_researched = true;
            break;
        }
        case UPGRADE_ID::BLINKTECH: {
            std::cout << "Blink research complete" << std::endl;
            m_blink_researched = true;
            break;
        }
        default: {
            break;
        }
    }
}

void BetaStar::OnUnitCreated(const Unit *unit)
{
    switch (unit->unit_type.ToType()) {
        case UNIT_TYPEID::PROTOSS_STALKER: {
            Actions()->UnitCommand(unit, ABILITY_ID::MOVE, m_army_rally_pos, true);
            break;
        }
    }
}

void BetaStar::OnUnitDestroyed(const Unit *unit)
{
    switch (unit->unit_type.ToType()) {
        case UNIT_TYPEID::PROTOSS_PROBE: {
            if (unit->tag == m_initial_scouting_probe->tag && !m_enemy_base_scouted) {
                Point2D closest_enemy_start_location;
                float closest_distance = std::numeric_limits<float>::max();
                for (const auto& enemy_start_location : Observation()->GetGameInfo().enemy_start_locations) {
                    float distance = DistanceSquared2D(unit->pos, enemy_start_location);
                    if (distance < closest_distance) {
                        closest_enemy_start_location = enemy_start_location;
                        closest_distance = distance;
                    }
                }
                if (closest_distance < std::numeric_limits<float>::max()) {
                    std::cout << "Enemy start location guessed: (" << closest_enemy_start_location.x << "," << closest_enemy_start_location.y << ")" << std::endl;
                    m_enemy_base_pos = closest_enemy_start_location;
                    m_enemy_base_quadrant = GetQuadrantByPoint(m_enemy_base_pos);
                    m_forward_pylon_pos = RotatePosition(m_forward_pylon_pos, m_enemy_base_quadrant);
                    m_enemy_base_scouted = true;
                }
            }
            break;
        }
    }
}
