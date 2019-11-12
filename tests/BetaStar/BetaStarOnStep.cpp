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
        if (worker->tag != m_initial_scouting_probe->tag) {
            worker_to_build = worker;
            break;
        }
    }

    if (worker_to_build == nullptr) {
        return;
    }

    // find a position where we can build the pylon
    float rx = GetRandomScalar();
    float ry = GetRandomScalar();
    Point2D pylon_pos = Point2D((worker_to_build->pos.x + (rx * 10.0f)), (worker_to_build->pos.y + (ry * 10.0f)));
    while (!Query()->Placement(m_supply_building_abilityid, pylon_pos)) {
        rx = GetRandomScalar();
        ry = GetRandomScalar();
        pylon_pos = Point2D((worker_to_build->pos.x + (rx * 10.0f)), (worker_to_build->pos.y + (ry * 10.0f)));
    }

    // build a pylon
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

    // if we don't have enough workers on minerals then return
    for (const auto& base : bases) {
        if (base->build_progress != 1) {
            continue;
        }
        if (base->assigned_harvesters < base->ideal_harvesters) {
            return;
        }
    }

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

    // build gas
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

    // order worker to build the base
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

    size_t num_bases = CountUnitType(UNIT_TYPEID::PROTOSS_NEXUS);
    size_t num_gases = CountUnitType(UNIT_TYPEID::PROTOSS_ASSIMILATOR);
    size_t num_pylons = CountUnitType(UNIT_TYPEID::PROTOSS_PYLON);
    size_t num_gateways = CountUnitType(UNIT_TYPEID::PROTOSS_GATEWAY);
    size_t num_warpgates = CountUnitType(UNIT_TYPEID::PROTOSS_WARPGATE);
    size_t num_forges = CountUnitType(UNIT_TYPEID::PROTOSS_FORGE);
    size_t num_cybernetics_cores = CountUnitType(UNIT_TYPEID::PROTOSS_CYBERNETICSCORE);

    if (num_pylons >= 1 && (num_gateways + num_warpgates) < 1 && num_minerals >= 150) {
        TryBuildStructureNearPylon(ABILITY_ID::BUILD_GATEWAY, m_worker_typeid);
        return;
    }

    if ((num_gateways + num_warpgates) > 0 && num_gases < 1 && num_minerals >= 75) {
        OnStepBuildGas();
        return;
    }

    if (num_gases > 0 && num_cybernetics_cores < 1 && num_minerals >= 150) {
        TryBuildStructureNearPylon(ABILITY_ID::BUILD_CYBERNETICSCORE, m_worker_typeid);
        return;
    }

    if (num_cybernetics_cores > 0 && num_gases < 2 && num_minerals >= 75) {
        OnStepBuildGas();
        return;
    }

    if (num_gases >= 2 && (num_gateways + num_warpgates) < 4 && num_minerals >= 150) {
        TryBuildStructureNearPylon(ABILITY_ID::BUILD_GATEWAY, m_worker_typeid);
        return;
    }

    if ((num_gateways + num_warpgates) >= 4 && num_minerals > 300) {
        TryBuildStructureNearPylon(ABILITY_ID::BUILD_GATEWAY, m_worker_typeid);
        return;
    }
}

//Try to get upgrades depending on build
void BetaStar::OnStepResearchUpgrades() {
    const ObservationInterface* observation = Observation();

    std::vector<UpgradeID> upgrades = observation->GetUpgrades();

    Units bases = FriendlyUnitsOfType(m_base_typeid);
    size_t base_count = bases.size();

    if (upgrades.empty()) {
        TryResearchUpgrade(ABILITY_ID::RESEARCH_WARPGATE, UNIT_TYPEID::PROTOSS_CYBERNETICSCORE);
        for (const auto& cybernetics_core : FriendlyUnitsOfType(UNIT_TYPEID::PROTOSS_CYBERNETICSCORE)) {
            if (cybernetics_core->build_progress != 1) {
                continue;
            }
            for (const auto& order : cybernetics_core->orders) {
                if (order.ability_id == ABILITY_ID::RESEARCH_WARPGATE) {
                    auto buffs = cybernetics_core->buffs;
                    if (std::find(buffs.begin(), buffs.end(), BuffID(281)) == buffs.end()) {
                        for (const auto& base : bases) {
                            Actions()->UnitCommand(base, AbilityID(3755), cybernetics_core);
                            break;
                        }
                    }
                }
            }
        }
    }
    else {
        for (const auto& upgrade : upgrades) {
            if (upgrade == UPGRADE_ID::PROTOSSGROUNDWEAPONSLEVEL1 && base_count > 2) {
                TryResearchUpgrade(ABILITY_ID::RESEARCH_PROTOSSGROUNDWEAPONS, UNIT_TYPEID::PROTOSS_FORGE);
            }
            else if (upgrade == UPGRADE_ID::PROTOSSGROUNDARMORSLEVEL1 && base_count > 2) {
                TryResearchUpgrade(ABILITY_ID::RESEARCH_PROTOSSGROUNDARMOR, UNIT_TYPEID::PROTOSS_FORGE);
            }
            else if (upgrade == UPGRADE_ID::PROTOSSGROUNDWEAPONSLEVEL2 && base_count > 3) {
                TryResearchUpgrade(ABILITY_ID::RESEARCH_PROTOSSGROUNDWEAPONS, UNIT_TYPEID::PROTOSS_FORGE);
            }
            else if (upgrade == UPGRADE_ID::PROTOSSGROUNDARMORSLEVEL2 && base_count > 3) {
                TryResearchUpgrade(ABILITY_ID::RESEARCH_PROTOSSGROUNDARMOR, UNIT_TYPEID::PROTOSS_FORGE);
            }
            else {
                TryResearchUpgrade(ABILITY_ID::RESEARCH_EXTENDEDTHERMALLANCE, UNIT_TYPEID::PROTOSS_ROBOTICSBAY);
                TryResearchUpgrade(ABILITY_ID::RESEARCH_BLINK, UNIT_TYPEID::PROTOSS_TWILIGHTCOUNCIL);
                TryResearchUpgrade(ABILITY_ID::RESEARCH_CHARGE, UNIT_TYPEID::PROTOSS_TWILIGHTCOUNCIL);
                TryResearchUpgrade(ABILITY_ID::RESEARCH_PROTOSSGROUNDWEAPONS, UNIT_TYPEID::PROTOSS_FORGE);
                TryResearchUpgrade(ABILITY_ID::RESEARCH_PROTOSSGROUNDARMOR, UNIT_TYPEID::PROTOSS_FORGE);
            }
        }
    }
}

void BetaStar::OnStepBuildArmy()
{
    const ObservationInterface* observation = Observation();

    int num_minerals = observation->GetMinerals();
    int num_gas = observation->GetVespene();

    Units gateways = FriendlyUnitsOfType(UNIT_TYPEID::PROTOSS_GATEWAY);
    Units warpgates = FriendlyUnitsOfType(UNIT_TYPEID::PROTOSS_WARPGATE);

    Units cybernetics_cores = FriendlyUnitsOfType(UNIT_TYPEID::PROTOSS_CYBERNETICSCORE);
    bool core_built = false;
    for (const auto& core : cybernetics_cores) {
        if (core->build_progress == 1) {
            core_built = true;
            break;
        }
    }

    if (m_warpgate_researched) {
        TryWarpInUnit(ABILITY_ID::TRAINWARP_STALKER);
    }
    else {
        for (const auto& gateway : gateways) {
            if (gateway->build_progress != 1) {
                continue;
            }
            if (gateway->orders.size() != 0) {
                continue;
            }
            if (core_built && num_minerals >= 125 && num_gas >= 50) {
                Actions()->UnitCommand(gateway, ABILITY_ID::TRAIN_STALKER);
                num_minerals -= 125;
                num_gas -= 50;
            }
            else if (num_minerals >= 100) {
                Actions()->UnitCommand(gateway, ABILITY_ID::TRAIN_ZEALOT);
                num_minerals -= 100;
            }
        }
    }
}

void BetaStar::OnStepManageArmy()
{
    const ObservationInterface* observation = Observation();

    Units zealots = FriendlyUnitsOfType(UNIT_TYPEID::PROTOSS_ZEALOT);
    Units stalkers = FriendlyUnitsOfType(UNIT_TYPEID::PROTOSS_STALKER);

    if (!m_attacking && m_enemy_base_scouted && (zealots.size() + stalkers.size()) >= 12) {
        m_attacking = true;
    }

    if (m_attacking) {
        for (const auto& zealot : zealots) {
            if (zealot->orders.empty()) {
                Actions()->UnitCommand(zealot, ABILITY_ID::ATTACK_ATTACK, m_enemy_base_pos);
            }
        }
        for (const auto& stalker : stalkers) {
            if (stalker->orders.empty()) {
                Actions()->UnitCommand(stalker, ABILITY_ID::ATTACK_ATTACK, m_enemy_base_pos);
            }
        }
    }
}
