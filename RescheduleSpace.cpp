/*
 * Authors: 
 *   Felix Brandt <brandt@fzi.de>, 
 *   Jochen Speck <speck@kit.edu>, 
 *   Markus Voelker <markus.voelker@kit.edu>
 *
 * Copyright (c) 2012 Felix Brandt, Jochen Speck, Markus Voelker
 *
 * Permission is hereby granted, free of charge, to any person obtaining 
 * a copy of this software and associated documentation files (the 
 * "Software"), to deal in the Software without restriction, including 
 * without limitation the rights to use, copy, modify, merge, publish, 
 * distribute, sublicense, and/or sell copies of the Software, and to 
 * permit persons to whom the Software is furnished to do so, subject to 
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included 
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS 
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF 
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. 
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY 
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, 
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE 
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <map>
#include <algorithm>

#include <gecode/minimodel.hh>

#include "RescheduleSpace.h"
#include "ProcessPropagator.h"
#include "CostPropagator.h"
#include "BestCostBrancher.h"

using namespace Gecode;

ProcessCostMap::ProcessCostMap (unsigned int _size, Gecode::Space& space) :
size(_size),
cost_map(static_cast<CostMap*>(space.ralloc(sizeof(CostMap) * _size))),
cost_bound(static_cast<CostBound*>(space.ralloc(sizeof(CostBound) * _size)))
{
    for (unsigned int i = 0; i < size; ++i) {
        new (&(cost_map[i])) CostMap(CostMap::allocator_type(space));
        new (&(cost_bound[i])) CostBound();
    }
}

ProcessCostMap::ProcessCostMap (const ProcessCostMap& p, Gecode::Space& space) :
size(p.size),
cost_map(static_cast<CostMap*>(space.ralloc(sizeof(CostMap) * p.size))),
cost_bound(static_cast<CostBound*>(space.ralloc(sizeof(CostBound) * p.size)))
{
    for (unsigned int i = 0; i < size; ++i) {
        new (&(cost_map[i])) CostMap(p.cost_map[i].begin(), p.cost_map[i].end(), CostMap::allocator_type(space));
        new (&(cost_bound[i])) CostBound(p.cost_bound[i]);
    }
}

void ProcessCostMap::setCost(unsigned int process, unsigned int machine, CostMap::mapped_type value)
{
    cost_map[process][machine] = value;
    
    if (value.first < cost_bound[process].min.cost) {
        cost_bound[process].min.cost = value.first;
        cost_bound[process].min.machine = machine;
    }
    
    if (value.second > cost_bound[process].max.cost) {
        cost_bound[process].max.cost = value.second;
        cost_bound[process].max.machine = machine;
    }
}

ProcessCostMap::CostMap::mapped_type ProcessCostMap::getCost(unsigned int process, unsigned int machine) const
{
    return cost_map[process][machine];
}

CostBound& ProcessCostMap::bound (unsigned int process)
{
    return cost_bound[process];
}

void ProcessCostMap::setBound (unsigned int process, CostBound& bound)
{
    cost_bound[process] = bound;
}

void ProcessCostMap::remove(unsigned int process, unsigned int machine)
{
    cost_map[process].erase(machine);
}

/** Initializing constructor */
RescheduleSpace::RescheduleSpace (const Instance& _instance, const ReAssignment& _state, const ProcessList& _moved) :
    instance(_instance), state(_state), moved(_moved),
    process(*this, _moved.size(), 0, _instance.num_machines - 1),
    process_move_cost(*this, _moved.size(), Gecode::Int::Limits::min, Gecode::Int::Limits::max),
    base_total_cost(0),
    delta(construct<PatchMap>(PatchMap::allocator_type(*this))),
    modified_machines(0, 0, gVector<int>::allocator_type(*this)),
    cost_cache(_moved.size(), *this),
    min_unassigned_balance(_instance.balance.size(), 0, gVector<int>::allocator_type(*this)),
    max_unassigned_balance(_instance.balance.size(), 0, gVector<int>::allocator_type(*this)),
    total_cost(*this, Gecode::Int::Limits::min, Gecode::Int::Limits::max)
{
    // setup constraints
    this->setupLoadConstraints();
    this->setupConflictConstraint();
    this->setupSpreadConstraint();
    this->setupDependencyConstraint();
    
    // setup objective value calculation
    this->setupObjectiveFunction();
    
    // setup additional constraints
    
    // setup brancher
    //branch(*this, process, INT_VAR_DEGREE_MIN, INT_VAL_RND);
    BestCostBrancher::post(*this, process, process_move_cost);
}

/** Copy constructor */
RescheduleSpace::RescheduleSpace (bool share, RescheduleSpace& s) :
    Space(share, s), instance(s.instance), state(s.state), moved(s.moved),
    base_total_cost(s.base_total_cost),
    delta(construct<PatchMap>(s.delta, PatchMap::allocator_type(*this))),
    modified_machines(s.modified_machines.begin(), s.modified_machines.end(), gVector<int>::allocator_type(*this)),
    cost_cache(s.cost_cache, *this),
    min_unassigned_balance(s.min_unassigned_balance.begin(), s.min_unassigned_balance.end(), gVector<int>::allocator_type(*this)),
    max_unassigned_balance(s.max_unassigned_balance.begin(), s.max_unassigned_balance.end(), gVector<int>::allocator_type(*this))
{
    process.update(*this, share, s.process);
    process_move_cost.update(*this, share, s.process_move_cost);
    
    total_cost.update(*this, share, s.total_cost);
}

Gecode::Space* RescheduleSpace::copy (bool share)
{
    return new RescheduleSpace(share, *this);
}

void RescheduleSpace::constrain (const Gecode::Space& _best)
{
    const RescheduleSpace& best = static_cast<const RescheduleSpace&>(_best);
    long long limit = (best.base_total_cost + best.total_cost.val()) - base_total_cost;
    
    #ifdef LOGGING  
    if (limit >= Gecode::Int::Limits::max)
        std::cerr << "{RescheduleSpace::constrain} Warning: limit exceeds 32bit integer" << std::endl;
    #endif
    
    rel(*this, total_cost < (int)(limit));
}

void RescheduleSpace::setupLoadConstraints ()
{
    int process_move_delta = 0;
    int machine_move_delta = 0;
    
    // Aggregate load which might be moved away
    for (unsigned int m = 0; m < moved.size(); ++m) {
        unsigned int current_machine = state.assignment[moved[m]];
        MachinePatch patch(instance, MachinePatch::allocator_type(*this));
        const Process& process_moved = instance.process[moved[m]];
        
        PatchMap::iterator it;
        if (delta.end() != (it = delta.find(current_machine))) {
            patch = it->second;
        } else {
            // initialize transient
            std::copy(state.transient[current_machine].begin(), state.transient[current_machine].end(), patch.transient.begin());
        }
        
        for (unsigned int r = 0; r < instance.num_resources; ++r) {
            patch.excess[r] -= process_moved.requirement[r];
        }
        
        // adjust transient load if the process is currently moved
        if (process_moved.original_machine != current_machine) {
            for (unsigned int r = 0; r < instance.transient_count; ++r) {
                patch.transient[r] -= process_moved.requirement[r];
            }
        }
        
        if (it != delta.end()) {
            it->second = patch;
        } else {
            // initialize machine balance
            std::copy(state.balance[current_machine].begin(), state.balance[current_machine].end(), patch.balance.begin());
            delta.insert(PatchMap::value_type(current_machine, patch));
        }
        
        if (process_moved.original_machine != current_machine) {
            process_move_delta -= process_moved.move_cost;
            machine_move_delta -= instance.machine[process_moved.original_machine].move_cost[current_machine];
        }
        
        ProcessPropagator::post(*this, m, moved[m]);
        CostPropagator::post(*this, m, moved[m]);
    }
    
    this->setupLoadCost();
    this->setupBalanceCost();
    
    base_total_cost += (state.process_moves + process_move_delta) * instance.weight_process_move_cost + (state.machine_moves + machine_move_delta) * instance.weight_machine_move_cost;
    long long best = state.load_cost + state.balance_cost + state.process_moves * instance.weight_process_move_cost + state.machine_moves * instance.weight_machine_move_cost;
    long long limit = best - base_total_cost;
    rel(*this, total_cost, IRT_LE, (int)(limit));
}

void RescheduleSpace::setupLoadCost ()
{
    long long moved_load_cost = 0;
    
    /** Subtract the moved processes from the base excess load */
    for (PatchMap::iterator iter = delta.begin(); iter != delta.end(); ++iter) {
        const MachineLoad& excess = state.excess[iter->first];
        
        for (unsigned int r = 0; r < instance.num_resources; ++r) {
            iter->second.excess[r] += excess[r];
            
            long long old_load_cost = std::max(0, excess[r]);
            long long new_load_cost = std::max(0, iter->second.excess[r]);
            
            moved_load_cost += (new_load_cost - old_load_cost) * instance.resource[r].weight_load_cost;
        }
    }
    
    base_total_cost += state.load_cost + moved_load_cost;
}

void RescheduleSpace::setupBalanceCost ()
{
    long long moved_balance_cost = 0;
    
    for (unsigned int b = 0; b < instance.balance.size(); ++b) {
        const unsigned int r1 = instance.balance[b].resource1;
        const unsigned int r2 = instance.balance[b].resource2;
        const unsigned int bal = instance.balance[b].balance;
        const unsigned int weight = instance.balance[b].weight_balance_cost;
        
        for (unsigned int m = 0; m < moved.size(); ++m) {
            const MachineLoad& requirement = instance.process[moved[m]].requirement;
            
            int diff = (int)(requirement[r2]) - (int)(bal * requirement[r1]);
            
            if (diff < 0) {
                min_unassigned_balance[b] += diff;
            } else {
                max_unassigned_balance[b] += diff;
            }
            
            PatchMap::iterator patch = delta.find(state.assignment[moved[m]]);
            long long old_balance = std::max(0, patch->second.balance[b]);
            patch->second.balance[b] -= diff;
            long long new_balance = std::max(0, patch->second.balance[b]);
            
            moved_balance_cost += (new_balance - old_balance) * weight;
        }
    }
    
    base_total_cost += state.balance_cost + moved_balance_cost;
}

/**
 * Remove conflicting machines from the search space of each moveable process.
 * 
 * Handling of moveable processes of the same service:
 *  - don't remove the current machine of another moveable process of the same service from the search space of a moveable process
 *  - post distinct constraint among all moveable processes of the same service
 */
void RescheduleSpace::setupConflictConstraint ()
{
    std::map<int, std::vector<unsigned int> > moved_processes_per_service;
    std::map<unsigned int, IntVarArgs> moved_conflicting_processes;
    
    // group moveable processes by their service
    int i = 0;
    for (ProcessList::const_iterator p = moved.begin(); p != moved.end(); ++p, ++i) {
        moved_processes_per_service[instance.process[*p].service].push_back(*p);
        moved_conflicting_processes[instance.process[*p].service] << process[i];
    }
    
    // post distinct constraint between moveable processes of the same service
    for (std::map<unsigned int, IntVarArgs>::const_iterator i = moved_conflicting_processes.begin(); i != moved_conflicting_processes.end(); ++i) {
        if (i->second.size() > 1) {
            distinct(*this, i->second);
        }
    }
    
    
    // remove all machines blocked by procceses of the same service as the moved process
    i = 0;
    for (ProcessList::const_iterator p = moved.begin(); p != moved.end(); ++p, ++i) {
        const std::vector<unsigned int>& members = instance.service[instance.process[*p].service].process;
        const std::vector<unsigned int>& unassigned = moved_processes_per_service[instance.process[*p].service];
        
        for (std::vector<unsigned int>::const_iterator m = members.begin(); m != members.end(); ++m) {
            if (*p != *m && std::find(unassigned.begin(), unassigned.end(), *m) == unassigned.end()) {
                rel(*this, process[i], IRT_NQ, state.assignment[*m]);
            }
        }
    }
}

/**
 * Reduce machines in used locations if the spread is critical
 * @todo: Make location spread count part of the ReAssignment (less computation)
 */
void RescheduleSpace::setupSpreadConstraint ()
{
    // tuple of moved-index and process id
    typedef std::pair< std::vector<unsigned int>, std::vector<unsigned int> > MovedService;
    std::map<unsigned int, MovedService> services;
    
    // filter for services that need to be checked
    // (at least one process moved and min_spread > 1)
    for (unsigned int p = 0; p < moved.size(); ++p) {
        if (instance.service[instance.process[moved[p]].service].min_spread > 1) {
            services[instance.process[moved[p]].service].first.push_back(p);
            services[instance.process[moved[p]].service].second.push_back(moved[p]);
        }
    }
    
    // preprocess machine locations -> this can be done in a more central place if needed elsewhere!
    IntArgs machine_location(instance.num_machines);
    for (unsigned int m = 0; m < instance.num_machines; ++m) {
        machine_location[m] = instance.machine[m].location;
    }
    
    // setup constraint for each affected service
    for (std::map<unsigned int, MovedService>::const_iterator service = services.begin(); service != services.end(); ++service) {
        // count processes of this service per location
        std::map<int, int> count;
        
        // get current number of distinct locations (ignore the moved process)
        const Service& service_obj = instance.service[service->first];
        const std::vector<unsigned int>& moved_p = service->second.second;
        for (std::vector<unsigned int>::const_iterator p = service_obj.process.begin(); p != service_obj.process.end(); ++p) {
            if (std::find(moved_p.begin(), moved_p.end(), *p) == moved_p.end()) {
                count[instance.machine[state.assignment[*p]].location]++;
            }
        }
        
        // only place further constraints if the spread of the remaining (staying) processes is too little
        if (count.size() < service_obj.min_spread) {
            const std::vector<unsigned int>& service_p = instance.service[service->first].process;
            IntVarArgs process_location(*this, service_p.size(), 0, instance.location.size() - 1);
            
            // we have to use the nvalue constraint to also consider the placement of staying processes of the service
            // nevertheless the element constraint mapping a process to its location is only posted for moveable constraints and otherwise fixed
            std::vector<unsigned int>::const_iterator it;
            for (int p = 0; p < process_location.size(); ++p) {
                if (moved_p.end() != (it = std::find(moved_p.begin(), moved_p.end(), service_p[p]))) {
                    element(*this, machine_location, process[service->second.first[it - moved_p.begin()]], process_location[p]);
                } else {
                    unsigned int l = instance.machine[state.assignment[service_p[p]]].location;
                    process_location[p] = IntVar(*this, l, l);
                }
            }
            
            nvalues(*this, process_location, IRT_GQ, service_obj.min_spread);
        }
    }
}

/**
 * Reduce machines to neighborhoods that are covered by all required services.
 */
void RescheduleSpace::setupDependencyConstraint ()
{
    // tuple of moved-index and process id
    typedef std::pair< std::vector<unsigned int>, std::vector<unsigned int> > MovedService;
    std::map<unsigned int, MovedService> services;
    
    // filter for services that need to be checked
    for (unsigned int m = 0; m < moved.size(); ++m) {
        if (instance.service[instance.process[moved[m]].service].depends_on.size() > 0) {
            services[instance.process[moved[m]].service].first.push_back(m);
            services[instance.process[moved[m]].service].second.push_back(moved[m]);
        }
        
        const Service& service = instance.service[instance.process[moved[m]].service];
        
        if (service.required_by.size() > 0) {
            unsigned int stay_in_neighborhood = 0;
            unsigned int current_neighborhood = instance.machine[state.assignment[moved[m]]].neighborhood;
            bool forbid_move = false;
            
            // check if at least one process of this service remains in the neighborhood
            for (ProcessList::const_iterator p = service.process.begin(); p != service.process.end() && !stay_in_neighborhood; ++p) {
                if (current_neighborhood == instance.machine[state.assignment[*p]].neighborhood &&
                    std::find(moved.begin(), moved.end(), *p) == moved.end()) {
                    stay_in_neighborhood++;
                }
            }
            
            // all processes of this service might be moved from the neighborhood, check if there is one process that depends on it
            if (stay_in_neighborhood == 0) {
                for (ServiceList::const_iterator s = service.required_by.begin(); s != service.required_by.end() && !forbid_move; ++s) {
                    for (ProcessList::const_iterator d = instance.service[*s].process.begin(); d != instance.service[*s].process.end() && !forbid_move; ++d) {
                        if (instance.machine[state.assignment[*d]].neighborhood == current_neighborhood) {
                            forbid_move = true;
                        }
                    }
                }
            }
            
            if (forbid_move) {
                // restrict process to current neighborhood
                const ProcessList& nh = instance.neighborhood[current_neighborhood];
                std::vector<int> machines(nh.size());
                for (int i = 0; i < nh.size(); i++)
                    machines[i] = nh[i];
                IntSet available_machines(&(machines[0]), (int)machines.size());
                dom(*this, process[m], available_machines);
            }
        }
    }
    
    // setup constraint for each affected service
    for (std::map<unsigned int, MovedService>::const_iterator s_iter = services.begin(); s_iter != services.end(); ++s_iter) {
        const Service& service = instance.service[s_iter->first];
        
        // determine available neighborhoods (intersection of neighborhoods covered by all required services)
        std::vector<unsigned int> neighborhoods;
        bool neighborhoods_initialized = false;
        
        for (std::vector<unsigned int>::const_iterator d = service.depends_on.begin(); d != service.depends_on.end(); ++d) {
            // neighborhoods covered by this service
            std::vector<unsigned int> covered;
            
            for (std::vector<unsigned int>::const_iterator p = instance.service[*d].process.begin(); p != instance.service[*d].process.end(); ++p) {
                // only consider neighborhoods of non-moved processes
                if (std::find(moved.begin(), moved.end(), *p) == moved.end()) {
                    covered.push_back(instance.machine[state.assignment[*p]].neighborhood);
                }
            }
            
            // the STL set_intersect algorithm needs sorted data
            std::sort(covered.begin(), covered.end());
            
            // don't intersect the first neighborhood (with the set of all neighborhoods), just take it :)
            if (!neighborhoods_initialized) {
                neighborhoods = covered;
                neighborhoods_initialized = true;
            } else {
                // do the intersection
                std::vector<unsigned int> new_neighborhood(neighborhoods.size());
                std::vector<unsigned int>::iterator it = set_intersection(neighborhoods.begin(), neighborhoods.end(), covered.begin(), covered.end(), new_neighborhood.begin());
                new_neighborhood.resize(it - new_neighborhood.begin());
                neighborhoods = new_neighborhood;
            }
        }
        
        unsigned int distinct_neighbors = 1;
        for (unsigned int i = 1; i < neighborhoods.size(); ++i) {
            if (neighborhoods[i-1] != neighborhoods[i]) distinct_neighbors++;
        }
        
        // if all neighborhoods present => no reduction possible => skip
        if (instance.neighborhood.size() == distinct_neighbors) {
            continue;
        }
        
        // no available neighborhoods for this service, this might happen if one required process is in the moved array
        // anyway the required process will not be moved (made sure by forbid_move above)
        // now we also have to fix the processes of the given service
        if (neighborhoods.size() == 0) {
            for (std::vector<unsigned int>::const_iterator p = s_iter->second.first.begin(); p != s_iter->second.first.end(); ++p) {
                rel(*this, process[*p], IRT_EQ, state.assignment[moved[*p]]);
            }
            continue;
        }
        
        // limit moveable processes to machines in these neighborhoods
        std::vector<int> machines;
        for (std::vector<unsigned int>::const_iterator iter = neighborhoods.begin(); iter != neighborhoods.end(); ++iter) {
            int s = machines.size();
            machines.resize(s + instance.neighborhood[*iter].size());
            std::copy(instance.neighborhood[*iter].begin(), instance.neighborhood[*iter].end(), &(machines[s]));
        }
        
        std::sort(machines.begin(), machines.end());
        
        // limit all moved processes of the given service
        for (std::vector<unsigned int>::const_iterator p = s_iter->second.first.begin(); p != s_iter->second.first.end(); ++p) {
            IntVar avail_machines(*this, IntSet(&(machines[0]), machines.size()));
            rel(*this, process[*p], IRT_EQ, avail_machines);
        }
    }
}

ReAssignment* RescheduleSpace::getResultState () const
{
    ReAssignment* result = new ReAssignment(this->state);
    
    for (unsigned int m = 0; m < moved.size(); ++m) {
        result->assignment[moved[m]] = process[m].val();
        
        if (instance.process[moved[m]].original_machine == state.assignment[moved[m]]) {
            if (instance.process[moved[m]].original_machine != result->assignment[moved[m]]) {
                result->process_moves += instance.process[moved[m]].move_cost;
            }
        } else {
            if (instance.process[moved[m]].original_machine == result->assignment[moved[m]]) {
                result->process_moves -= instance.process[moved[m]].move_cost;
            }
        }
        
        result->machine_moves -= instance.machine[instance.process[moved[m]].original_machine].move_cost[state.assignment[moved[m]]];
        result->machine_moves += instance.machine[instance.process[moved[m]].original_machine].move_cost[result->assignment[moved[m]]];
    }
    
    for (PatchMap::const_iterator patch = delta.begin(); patch != delta.end(); ++patch) {
        for (unsigned int r = 0; r < instance.num_resources; ++r) {
            int old_excess = std::max(0, result->excess[patch->first][r]);
            int new_excess = std::max(0, patch->second.excess[r]);
            
            result->load_cost += (new_excess - old_excess) * (int)(instance.resource[r].weight_load_cost);
        }
        
        for (unsigned int b = 0; b < instance.balance.size(); ++b) {
            int old_balance = std::max(0, result->balance[patch->first][b]);
            int new_balance = std::max(0, patch->second.balance[b]);
            
            result->balance_cost += (new_balance - old_balance) * (int)(instance.balance[b].weight_balance_cost);
        }
        
        std::copy(patch->second.excess.begin(), patch->second.excess.end(), result->excess[patch->first].begin());
        std::copy(patch->second.transient.begin(), patch->second.transient.end(), result->transient[patch->first].begin());
        std::copy(patch->second.balance.begin(), patch->second.balance.end(), result->balance[patch->first].begin());
    }
    
    
    #ifdef LOGGING
    long long r_total_cost = result->getCost();
    long long s_total_cost = base_total_cost + total_cost.max();
    
    if (r_total_cost != s_total_cost) {
        std::cerr << "{RescheduleSpace::getResultState} Warning calculated and realized costs are not equal " << s_total_cost << " " << r_total_cost << std::endl;
    }
    #endif
    
    return result;
}

void RescheduleSpace::setupObjectiveFunction ()
{
    linear(*this, process_move_cost, IRT_EQ, total_cost);
}

void RescheduleSpace::print (std::ostream& out) const
{
    out << process << std::endl << std::endl << process_move_cost << std::endl;
    
    out << "Total Cost: ";
    if (total_cost.assigned()) {
        out << base_total_cost + total_cost.val();
    } else {
        out << "[" << base_total_cost + total_cost.min() << ".." << base_total_cost + total_cost.max() << "]";
    }
    out << " " << total_cost << std::endl;
    
}
