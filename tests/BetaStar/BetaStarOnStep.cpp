#include "BetaStar.h"

using namespace sc2;


// compute useful statistics and store them in the class
// for things we want to compute each step which won't change until next step
void BetaStar::OnStepComputeStatistics()
{
    //const ObservationInterface* observation = Observation();

    // how much supply we have left
    //m_supply_left = (observation->GetFoodCap() - observation->GetFoodUsed());
}

// train workers if we need them
// some things in this function could be sped up with caching, if neccessary.
void BetaStar::OnStepTrainWorkers()
{
    const ObservationInterface* observation = Observation();

    int num_minerals = observation->GetMinerals();
    int supply_left = (observation->GetFoodCap() - observation->GetFoodUsed());

    // check whether we have enough minerals to train a worker
    if (num_minerals < 50) {
        return;
    }

    // check whether we have enough supply to train a worker
    if (supply_left == 0) {
        return;
    }

    int sum_assigned_harvesters = 0;    // # of workers currently working
    int sum_ideal_harvesters = 0;       // ideal # of workers

    int workers_training = 0;   // # of workers currently being trained

    const Units bases = FriendlyUnitsOfType(m_base_typeid);
    const Units gases = FriendlyUnitsOfType(m_gas_building_typeid);

    // compute base worker counts
    // compute number of workers being trained
    for (const auto& base : bases) {
        // base still under construction
        if (base->build_progress != 1) {
            sum_ideal_harvesters += 16;  // train workers for the base under construction
            continue;
        }
        sum_assigned_harvesters += base->assigned_harvesters;
        sum_ideal_harvesters += base->ideal_harvesters;

        // check for workers being trained
        for (const auto& order : base->orders) {
            if (order.ability_id == m_worker_train_abilityid) {
                ++workers_training;
            }
        }
    }

    // compute gas worker counts
    for (const auto& gas : gases) {
        // gas still under construction
        if (gas->build_progress != 1) {
            sum_ideal_harvesters += 3;  // train 3 workers so that the gas can be worked once it's done
            continue;
        }
        sum_assigned_harvesters += gas->assigned_harvesters;
        sum_ideal_harvesters += gas->ideal_harvesters;
    }

    for (const auto& base : bases) {
        // check whether we should train another worker, if not then return
        if (sum_assigned_harvesters + workers_training >= sum_ideal_harvesters) {
            return;
        }
        if (num_minerals < 50) {
            return;
        }
        if (supply_left == 0) {
            return;
        }

        // check whether the base can train a worker, if not then continue
        if (base->build_progress != 1) {
            continue;
        }
        if (base->orders.size() > 0) {
            continue;
        }

        // train a worker
        Actions()->UnitCommand(base, m_worker_train_abilityid);

        // update stats
        ++workers_training;
        --supply_left;
        num_minerals -= 50;
    }
}

// constructs additional pylons
// TODO: better placement logic
void BetaStar::OnStepBuildPylons()
{
    const ObservationInterface* observation = Observation();

    // build a pylon if we have less than supply_threshold supply left
    const int supply_threshold = 6;

    int num_minerals = observation->GetMinerals();
    int supply_left = (observation->GetFoodCap() - observation->GetFoodUsed());

    // check whether we can afford a pylon
    if (num_minerals < 100) {
        return;
    }

    // check whether we're over the supply threshold to build a pylon
    if (supply_left >= supply_threshold) {
        return;
    }

    const Units pylons = FriendlyUnitsOfType(m_supply_building_typeid);

    // check whether we are already building a pylon
    for (const auto& pylon : pylons) {
        if (pylon->build_progress != 1) {
            return;
        }
    }

    Units workers = FriendlyUnitsOfType(m_worker_typeid);

    // maybe unneccessary, we're kind of screwed anyways if this is the case
    if (workers.empty()) {
        return;
    }

    for (const auto& worker : workers) {
        for (const auto& order : worker->orders) {
            if (order.ability_id == m_supply_building_abilityid) {
                return;
            }
        }
    }

    // select the worker to build the pylon
    const Unit* worker_to_build = nullptr;
    for (const auto& worker : workers) {
        if (worker_to_build != nullptr) {
            break;
        }
        bool scout = false;
        for (const auto& scouting_probe : m_scouting_probes) {
            if (worker->tag == scouting_probe->tag) {
                scout = true;
            }
        }
        if (!scout) {
            worker_to_build = worker;
        }
    }

    if (worker_to_build == nullptr) {
        return;
    }

    // find a position where we can build the pylon
    Point2D pylon_pos;

    if (pylons.size() < m_first_pylon_positions.size()) {
        pylon_pos = m_first_pylon_positions[pylons.size()];
        pylon_pos = rotate_position(pylon_pos, m_starting_quadrant);
    }
    else {
        float rx = GetRandomScalar();
        float ry = GetRandomScalar();
        pylon_pos = Point2D((worker_to_build->pos.x + (rx * 10.0f)), (worker_to_build->pos.y + (ry * 10.0f)));
        while (!Query()->Placement(m_supply_building_abilityid, pylon_pos)) {
            rx = GetRandomScalar();
            ry = GetRandomScalar();
            pylon_pos = Point2D((worker_to_build->pos.x + (rx * 10.0f)), (worker_to_build->pos.y + (ry * 10.0f)));
        }
    }

    // build a pylon (no need for additional checks since we've already checked to make sure we have the minerals)
    Actions()->UnitCommand(worker_to_build, m_supply_building_abilityid, pylon_pos);
}

// build more gas if neccessary
// this could probably be optimized further
void BetaStar::OnStepBuildGas()
{
    const ObservationInterface* observation = Observation();

    int num_minerals = observation->GetMinerals();

    // check whether we can afford a gas
    if (num_minerals < 75) {
        return;
    }

    Units bases = FriendlyUnitsOfType(m_base_typeid);
    Units gases = FriendlyUnitsOfType(m_gas_building_typeid);

    int num_complete_bases = 0;

    for (const auto& base : bases) {
        if (base->build_progress == 1) {
            ++num_complete_bases;
        }
    }

    // check whether we have any gas open
    if (gases.size() >= (2 * num_complete_bases)) {
        return;
    }

    // if we are already building gas then return
    // if gas isn't saturated then return
    for (const auto& gas : gases) {
        if (gas->build_progress != 1) {
            return;
        }
        if (gas->assigned_harvesters < gas->ideal_harvesters) {
            return;
        }
    }

    //NOTE: Commented out because it can needlessly delay moving through the build order if, for example, one worker is building/scouting so minerals are desaturated
    // if we don't have enough workers on minerals then return
    /*for (const auto& base : bases) {
        if (base->build_progress != 1) {
            continue;
        }
        if (base->assigned_harvesters < base->ideal_harvesters) {
            return;
        }
    }*/

    Units geysers = observation->GetUnits(Unit::Alliance::Neutral, IsUnit(UNIT_TYPEID::NEUTRAL_VESPENEGEYSER));

    // find the closest geyser to one of our bases
    float minimum_distance = 225.0f;
    const Unit* closest_geyser = nullptr;
    for (const auto& geyser : geysers) {
        for (const auto& base : bases) {
            float distance = DistanceSquared2D(base->pos, geyser->pos);
            if (distance < minimum_distance) {
                if (Query()->Placement(m_gas_building_abilityid, geyser->pos)) {
                    minimum_distance = distance;
                    closest_geyser = geyser;
                }
            }
        }
    }

    // we didn't find a geyser close enough to one of our bases
    if (closest_geyser == nullptr) {
        return;
    }

    Units workers = FriendlyUnitsOfType(m_worker_typeid);

    if (workers.empty()) {
        return;
    }

    // check to see if there is already a worker heading out to build it
    for (const auto& worker : workers) {
        for (const auto& order : worker->orders) {
            if (order.ability_id == m_gas_building_abilityid) {
                return;
            }
        }
    }

    // find closest worker to build it
    const Unit* closest_worker = nullptr;
    float closest_distance = std::numeric_limits<float>::max();
    for (const auto& worker : workers) {
        float distance = DistanceSquared2D(closest_geyser->pos, worker->pos);
        if (distance < closest_distance) {
            closest_worker = worker;
            closest_distance = distance;
        }
    }

    if (closest_worker == nullptr) {
        std::cerr << "This should never happen (OnStepBuildGas)" << std::endl;
        exit(1);
    }

    // build gas (no need for additional checks since we've already checked to make sure we have the minerals)
    Actions()->UnitCommand(closest_worker, m_gas_building_abilityid, closest_geyser);
}

// logic for building new bases
// TODO: check for units blocking the expansion
void BetaStar::OnStepExpand()
{
    const ObservationInterface* observation = Observation();

    int num_minerals = observation->GetMinerals();

    // check whether we can afford a new base
    if (num_minerals < 400) {
        return;
    }

    Units bases = FriendlyUnitsOfType(m_base_typeid);

    // check whether we have enough bases
    if (bases.size() >= m_max_bases) {
        return;
    }

    // check if any of our bases still need workers, if so then return
    for (const auto& base : bases) {
        if (base->build_progress != 1) {
            return;
        }
        if (base->assigned_harvesters < base->ideal_harvesters) {
            return;
        }
    }

    Units gases = FriendlyUnitsOfType(m_gas_building_typeid);

    // check whether we still need to build gases
    //if (gases.size() < (2 * bases.size())) {
        //return;
    //}

    // check if any of our gases still need workers, if so then return
    for (const auto& gas : gases) {
        if (gas->build_progress != 1) {
            continue;
        }
        if (gas->assigned_harvesters < gas->ideal_harvesters) {
            return;
        }
    }

    Units workers = FriendlyUnitsOfType(m_worker_typeid);

    if (workers.empty()) {
        return;
    }

    // check whether a worker is already on its way to build a base
    for (const auto& worker : workers) {
        for (const auto& order : worker->orders) {
            if (order.ability_id == m_base_building_abilityid) {
                return;
            }
        }
    }

    // find the closest expansion location
    float minimum_distance = std::numeric_limits<float>::max();
    Point3D closest_expansion;
    for (const auto& expansion : m_expansion_locations) {
        float current_distance = Distance2D(m_starting_pos, expansion);
        if (current_distance < .01f) {
            continue;
        }
        if (current_distance < minimum_distance) {
            if (Query()->Placement(m_base_building_abilityid, expansion)) {
                closest_expansion = expansion;
                minimum_distance = current_distance;
            }
        }
    }

    // find closest worker to build it
    const Unit* closest_worker = nullptr;
    float closest_distance = std::numeric_limits<float>::max();
    for (const auto& worker : workers) {
        float distance = DistanceSquared2D(closest_expansion, worker->pos);
        if (distance < closest_distance) {
            closest_worker = worker;
            closest_distance = distance;
        }
    }

    // order worker to build the base (no need for additional checks since we've already made sure we have the minerals)
    Actions()->UnitCommand(closest_worker, m_base_building_abilityid, closest_expansion);
}

// Struct used in OnStepManageWorkers()
struct UnevenResource
{
    int worker_diff;
    const Unit* unit;

    UnevenResource(const Unit* unit)
        : unit(unit)
    {
        worker_diff = abs(unit->ideal_harvesters - unit->assigned_harvesters);
    }
};

// Move workers from oversaturated resources to undersaturated resources
// Also gives orders to idle workers
// BUG: doesn't seem to catch workers who built assimilators, they wait around until it's done and then start working there
// This could probably be set to run every couple steps instead of every step
void BetaStar::OnStepManageWorkers()
{
    const ObservationInterface* observation = Observation();

    Units bases = FriendlyUnitsOfType(m_base_typeid);
    Units gases = FriendlyUnitsOfType(m_gas_building_typeid);

    if (bases.empty()) {
        return;
    }

    std::vector<UnevenResource> oversaturated;
    std::vector<UnevenResource> undersaturated;

    // add over / undersaturated bases to the respective vectors
    for (const auto& base : bases) {
        if (base->build_progress != 1) {
            continue;
        }
        int worker_diff = (base->ideal_harvesters - base->assigned_harvesters);
        if (worker_diff == 0) {
            continue;
        }
        if (worker_diff > 0) {
            undersaturated.push_back(UnevenResource(base));
            continue;
        }
        if (worker_diff < 0) {
            oversaturated.push_back(UnevenResource(base));
            continue;
        }
    }

    // add over / undersaturated gases to the respective vectors
    for (const auto& gas : gases) {
        if (gas->build_progress != 1) {
            continue;
        }
        int worker_diff = (gas->ideal_harvesters - gas->assigned_harvesters);
        if (worker_diff == 0) {
            continue;
        }
        if (worker_diff > 0) {
            undersaturated.push_back(UnevenResource(gas));
            continue;
        }
        if (worker_diff < 0) {
            oversaturated.push_back(UnevenResource(gas));
            continue;
        }
    }

    Units workers = FriendlyUnitsOfType(m_worker_typeid);
    Units idle_workers;

    // iterate through workers, adding idle workers and workers on oversaturated resources to idle_workers
    for (const auto& worker : workers) {
        bool scout = false;
        for (const auto& scouting_probe : m_scouting_probes) {
            if (worker->tag == scouting_probe->tag) {
                scout = true;
                break;
            }
        }
        if (scout) {
            continue;
        }

        // idle worker
        if (worker->orders.size() == 0) {
            idle_workers.push_back(worker);
            continue;
        }
        // check for working oversaturated resource
        for (const auto& order : worker->orders) {
            for (auto& uneven_resource : oversaturated) {
                // already took workers off this resource
                if (uneven_resource.worker_diff == 0) {
                    continue;
                }
                // take worker off the oversaturated resource, update the struct
                if (order.target_unit_tag == uneven_resource.unit->tag) {
                    idle_workers.push_back(worker);
                    --uneven_resource.worker_diff;
                    continue;
                }
            }
        }
    }

    if (idle_workers.size() == 0) {
        return;
    }

    // iterate throuh undersatured resources, assigning workers from idle_workers to work them
    for (auto& uneven_resource : undersaturated) {
        while (idle_workers.size() > 0 && uneven_resource.worker_diff > 0) {
            // remove worker from idle_workers
            const Unit* worker = idle_workers.back();
            idle_workers.pop_back();

            // update struct
            --uneven_resource.worker_diff;

            // set worker to mine minerals from undersaturated base
            if (uneven_resource.unit->unit_type == m_base_typeid) {
                const Unit* mineral = FindNearestNeutralUnit(uneven_resource.unit->pos, UNIT_TYPEID::NEUTRAL_MINERALFIELD);
                Actions()->UnitCommand(worker, m_worker_gather_abilityid, mineral);
            }
            // set worker to mine gas from undersaturated gas
            if (uneven_resource.unit->unit_type == m_gas_building_typeid) {
                Actions()->UnitCommand(worker, m_worker_gather_abilityid, uneven_resource.unit);
            }
        }
    }

    // maybe check here if there are still idle workers, reassign them?
}

void BetaStar::OnStepBuildOrder()
{
    const ObservationInterface* observation = Observation();

    int num_minerals = observation->GetMinerals();
    int num_gas = observation->GetVespene();

    size_t num_bases = CountUnitType(UNIT_TYPEID::PROTOSS_NEXUS);
    size_t num_gases = CountUnitType(UNIT_TYPEID::PROTOSS_ASSIMILATOR);
    size_t num_pylons = CountUnitType(UNIT_TYPEID::PROTOSS_PYLON);
    size_t num_gateways = CountUnitType(UNIT_TYPEID::PROTOSS_GATEWAY);
    size_t num_warpgates = CountUnitType(UNIT_TYPEID::PROTOSS_WARPGATE);
    size_t num_forges = CountUnitType(UNIT_TYPEID::PROTOSS_FORGE);
    size_t num_cybernetics_cores = CountUnitType(UNIT_TYPEID::PROTOSS_CYBERNETICSCORE);
    size_t num_twilight_councils = CountUnitType(UNIT_TYPEID::PROTOSS_TWILIGHTCOUNCIL);
    size_t num_photon_cannons = CountUnitType(UNIT_TYPEID::PROTOSS_PHOTONCANNON);
    size_t num_robotics_facilities = CountUnitType(UNIT_TYPEID::PROTOSS_ROBOTICSFACILITY);

    bool cybernetics_core_completed = false;
    for (const auto& cybernetics_core : FriendlyUnitsOfType(UNIT_TYPEID::PROTOSS_CYBERNETICSCORE)) {
        if (cybernetics_core->build_progress == 1) {
            cybernetics_core_completed = true;
        }
    }

    if (num_gateways > 0 && !m_proxy_pylon_built && m_enemy_base_scouted && num_minerals >= 100 && m_scouting_probes.size() >= 1) {
        std::cout << "Build proxy pylon" << std::endl;
        Point2D pylon_pos = rotate_position(m_proxy_pylon_positions.front(), m_enemy_base_quadrant);

        for (const auto& pylon : FriendlyUnitsOfType(UNIT_TYPEID::PROTOSS_PYLON)) {
            if (DistanceSquared2D(pylon->pos, m_enemy_base_pos) < 5000) {
                m_proxy_pylon_built = true;
                return;
            }
        }

        const Unit* closest_scouting_probe = GetClosestUnit(pylon_pos, m_scouting_probes);
        for (const auto& order : closest_scouting_probe->orders) {
            if (order.ability_id == ABILITY_ID::BUILD_PYLON) {
                return;
            }
        }

        if (Query()->Placement(ABILITY_ID::BUILD_PYLON, pylon_pos, closest_scouting_probe)) {
            std::cout << "Build proxy pylon 2" << std::endl;
            Actions()->UnitCommand(closest_scouting_probe, ABILITY_ID::BUILD_PYLON, pylon_pos);
            //Actions()->UnitCommand(closest_scouting_probe, ABILITY_ID::MOVE, m_proxy_probe_hiding_pos, true);
            return;
        }
    }

    if (!m_proxy_cannon_built && m_proxy_pylon_completed && m_enemy_base_scouted && num_forges >= 1 && num_minerals >= 150 && m_scouting_probes.size() >= 1) {
        std::cout << "Build proxy cannon" << std::endl;
        Point2D cannon_pos = rotate_position(m_proxy_cannon_positions.front(), m_enemy_base_quadrant);

        for (const auto& cannon : FriendlyUnitsOfType(UNIT_TYPEID::PROTOSS_PHOTONCANNON)) {
            if (DistanceSquared2D(cannon->pos, m_enemy_base_pos) < 5000) {
                m_proxy_cannon_built = true;
                return;
            }
        }

        const Unit* closest_scouting_probe = GetClosestUnit(cannon_pos, m_scouting_probes);
        for (const auto& order : closest_scouting_probe->orders) {
            if (order.ability_id == ABILITY_ID::BUILD_PHOTONCANNON) {
                return;
            }
        }

        if (Query()->Placement(ABILITY_ID::BUILD_PHOTONCANNON, cannon_pos, closest_scouting_probe)) {
            std::cout << "Build proxy cannon 2" << std::endl;
            Actions()->UnitCommand(closest_scouting_probe, ABILITY_ID::BUILD_PHOTONCANNON, cannon_pos);
            //Actions()->UnitCommand(closest_scouting_probe, ABILITY_ID::MOVE, m_proxy_probe_hiding_pos, true);
            return;
        }
    }

    if (!m_proxy_robo_built && m_proxy_pylon_completed && m_enemy_base_scouted && cybernetics_core_completed && num_minerals >= 150 && num_gas >= 100 && m_scouting_probes.size() >= 1) {
        std::cout << "Build proxy robo" << std::endl;
        Point2D robo_pos = rotate_position(m_proxy_robo_positions.front(), m_enemy_base_quadrant);

        for (const auto& robo : FriendlyUnitsOfType(UNIT_TYPEID::PROTOSS_ROBOTICSFACILITY)) {
            if (DistanceSquared2D(robo->pos, m_enemy_base_pos) < 5000) {
                m_proxy_robo_built = true;
                return;
            }
        }

        const Unit* closest_scouting_probe = GetClosestUnit(robo_pos, m_scouting_probes);
        for (const auto& order : closest_scouting_probe->orders) {
            if (order.ability_id == ABILITY_ID::BUILD_ROBOTICSFACILITY) {
                return;
            }
        }

        if (Query()->Placement(ABILITY_ID::BUILD_ROBOTICSFACILITY, robo_pos, closest_scouting_probe)) {
            std::cout << "Build proxy robo 2" << std::endl;
            Actions()->UnitCommand(closest_scouting_probe, ABILITY_ID::BUILD_ROBOTICSFACILITY, robo_pos);
            //Actions()->UnitCommand(closest_scouting_probe, ABILITY_ID::MOVE, m_proxy_probe_hiding_pos, true);
            return;
        }
    }

    if (!m_proxy_shield_battery_built && m_enemy_base_scouted && num_robotics_facilities >= 1 && cybernetics_core_completed && num_minerals >= 150 && m_scouting_probes.size() >= 1) {
        std::cout << "Build proxy shield battery" << std::endl;
        Point2D shield_battery_pos = rotate_position(m_proxy_shield_battery_positions.front(), m_enemy_base_quadrant);

        for (const auto& shield_battery : FriendlyUnitsOfType(UNIT_TYPEID::PROTOSS_SHIELDBATTERY)) {
            if (DistanceSquared2D(shield_battery->pos, m_enemy_base_pos) < 5000) {
                m_proxy_shield_battery_built = true;
                return;
            }
        }

        const Unit* closest_scouting_probe = GetClosestUnit(shield_battery_pos, m_scouting_probes);
        for (const auto& order : closest_scouting_probe->orders) {
            if (order.ability_id == ABILITY_ID::BUILD_SHIELDBATTERY) {
                return;
            }
        }

        if (Query()->Placement(ABILITY_ID::BUILD_SHIELDBATTERY, shield_battery_pos, closest_scouting_probe)) {
            std::cout << "Build proxy shield battery 2" << std::endl;
            Actions()->UnitCommand(closest_scouting_probe, ABILITY_ID::BUILD_SHIELDBATTERY, shield_battery_pos);
            //Actions()->UnitCommand(closest_scouting_probe, ABILITY_ID::MOVE, m_proxy_probe_hiding_pos, true);
            return;
        }
    }

    if (num_pylons >= 1 && num_forges < 1 && num_minerals >= 150) {
        TryBuildStructureNearPylon(ABILITY_ID::BUILD_FORGE, m_worker_typeid);
        return;
    }

    if (num_pylons >= 1 && (num_gateways + num_warpgates) < 1 && num_minerals >= 150) {
        TryBuildStructureNearPylon(ABILITY_ID::BUILD_GATEWAY, m_worker_typeid);
        return;
    }

    if ((num_gateways + num_warpgates) > 0 && num_gases < 1 && num_minerals >= 75) {
        OnStepBuildGas();
        return;
    }

    if (num_gases < 2 && num_minerals >= 150) {
        OnStepBuildGas();
        return;
    }

    if (m_proxy_cannon_built && num_cybernetics_cores < 1 && num_minerals >= 150) {
        TryBuildStructureNearPylon(ABILITY_ID::BUILD_CYBERNETICSCORE, m_worker_typeid);
        return;
    }
}

//Try to get upgrades depending on build
void BetaStar::OnStepResearchUpgrades() {
    const ObservationInterface* observation = Observation();

    std::vector<UpgradeID> upgrades = observation->GetUpgrades();

    Units bases = FriendlyUnitsOfType(m_base_typeid);
    size_t base_count = bases.size();

    for (const auto& cybernetics_core : FriendlyUnitsOfType(UNIT_TYPEID::PROTOSS_CYBERNETICSCORE)) {
        if (cybernetics_core->build_progress != 1) {
            continue;
        }
        for (const auto& order : cybernetics_core->orders) {
            if (order.ability_id == ABILITY_ID::RESEARCH_WARPGATE) {
                m_warpgate_researching = true;
                if (order.progress < 0.8) { // not sure about this number
                    auto buffs = cybernetics_core->buffs;
                    if (std::find(buffs.begin(), buffs.end(), BuffID(281)) == buffs.end()) {
                        for (const auto& base : bases) {
                            // No errors when attempting without energy
                            TryIssueCommand(base, AbilityID(3755), cybernetics_core);
                            break;
                        }
                    }
                }
            }
        }
    }

    for (const auto& twilight_council : FriendlyUnitsOfType(UNIT_TYPEID::PROTOSS_TWILIGHTCOUNCIL)) {
        if (twilight_council->build_progress != 1) {
            continue;
        }
        for (const auto& order : twilight_council->orders) {
            if (order.ability_id == ABILITY_ID::RESEARCH_BLINK) {
                m_blink_researching = true;
                if (order.progress < 0.8) {
                    auto buffs = twilight_council->buffs;
                    if (std::find(buffs.begin(), buffs.end(), BuffID(281)) == buffs.end()) {
                        for (const auto& base : bases) {
                            // No errors when attempting without energy
                            TryIssueCommand(base, AbilityID(3755), twilight_council);
                            break;
                        }
                    }
                }
            }
        }
    }

    if (!m_warpgate_researching) {
        TryResearchUpgrade(ABILITY_ID::RESEARCH_WARPGATE, UNIT_TYPEID::PROTOSS_CYBERNETICSCORE);
    }
    if (!m_blink_researching) {
        TryResearchUpgrade(ABILITY_ID::RESEARCH_BLINK, UNIT_TYPEID::PROTOSS_TWILIGHTCOUNCIL);
    }
}

void BetaStar::OnStepBuildArmy()
{
    const ObservationInterface* observation = Observation();

    int num_minerals = observation->GetMinerals();
    int num_gas = observation->GetVespene();

    Units gateways = FriendlyUnitsOfType(UNIT_TYPEID::PROTOSS_GATEWAY);
    Units warpgates = FriendlyUnitsOfType(UNIT_TYPEID::PROTOSS_WARPGATE);

    Units twilight_councils = FriendlyUnitsOfType(UNIT_TYPEID::PROTOSS_TWILIGHTCOUNCIL);

    Units cybernetics_cores = FriendlyUnitsOfType(UNIT_TYPEID::PROTOSS_CYBERNETICSCORE);

    //bool core_built = false;
    //for (const auto& core : cybernetics_cores) {
    //    if (core->build_progress == 1) {
    //        core_built = true;
    //        break;
    //    }
    //}

    //if (twilight_councils.size() == 0) {
    //    return;
    //}

    for (const auto& robo : FriendlyUnitsOfType(UNIT_TYPEID::PROTOSS_ROBOTICSFACILITY)) {
        if (robo->build_progress == 1) {
            if (robo->orders.size() == 0) {
                if (num_minerals >= 275 && num_gas >= 100) {
                    Actions()->UnitCommand(robo, ABILITY_ID::TRAIN_IMMORTAL);
                    num_minerals -= 275;
                    num_gas -= 100;
                }
            }
        }
    }

    //if (core_built)
    //{
    //    if (m_warpgate_researched && m_attacking)
    //    {
    //        for (const Unit *warpgate : FriendlyUnitsOfType(UNIT_TYPEID::PROTOSS_WARPGATE))
    //        {
    //            if (WarpUnit(warpgate, m_enemy_base_pos, UNIT_TYPEID::PROTOSS_STALKER))
    //            {
    //                num_minerals -= 125;
    //                num_gas -= 125;
    //            }
    //        }
    //    }

    //    int num_built = (int)MassTrainUnit(UNIT_TYPEID::PROTOSS_STALKER);
    //    num_minerals -= num_built * 125;
    //    num_gas -= num_built * 50;
    //}
}

void BetaStar::OnStepManageArmy()
{

    if (!m_attacking && CountUnitType(UNIT_TYPEID::PROTOSS_IMMORTAL) >= 3) {
        m_attacking = true;
    }

    if (m_attacking) {
        for (const auto& immortal : FriendlyUnitsOfType(UNIT_TYPEID::PROTOSS_IMMORTAL)) {
            if (immortal->orders.size() == 0) {
                Actions()->UnitCommand(immortal, ABILITY_ID::ATTACK, m_enemy_base_pos);
            }
        }
    }

    //// Switch to offensive mode. Trigger will be different for each strategy.
    //// TODO: Turn off offensive mode if it's going to lose us the match. 
    //switch (m_current_strategy)
    //{
    //    case Strategy::Blink_Stalker_Rush:
    //        if (!m_attacking && m_blink_researched && m_enemy_base_scouted && Observation()->GetArmyCount() >= 12) {
    //            m_attacking = true;
    //        }
    //        break;
    //    default:
    //        if (!m_attacking && m_blink_researched && m_enemy_base_scouted && Observation()->GetArmyCount() >= 20) {
    //            m_attacking = true;
    //        }
    //        break;
    //}

    //// defend our base
    //if (!m_attacking) {
    //    for (UNIT_TYPEID unitType : managed_unit_types)
    //    {
    //        BaseDefenseMacro(FriendlyUnitsOfType(unitType));
    //    }
    //}
    //// attack enemy base
    //else
    //{
    //    for (UNIT_TYPEID unitType : managed_unit_types)
    //    {
    //        EnemyBaseAttackMacro(FriendlyUnitsOfType(unitType));
    //    }
    //}

    //// This micro is always valid to perform
    //StalkerBlinkMicro();
}
