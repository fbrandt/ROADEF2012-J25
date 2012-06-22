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

#include "CostPropagator.h"

using namespace Gecode;
using namespace std;

CostPropagator::CostPropagator (Home home, unsigned int index, unsigned int process_id, IntVar& process, IntVar& cost) :
Propagator(home),
m_index(index), m_process_id(process_id),
m_process(process), m_cost(cost), cache_stage(-1), m_lastbound(0,0)
{
    m_process.subscribe(home, *this, Int::PC_INT_BND);
    m_cost.subscribe(home, *this, Int::PC_INT_BND);
}

CostPropagator::CostPropagator (Home home, bool share, CostPropagator& p) :
Propagator(home, share, p),
m_index(p.m_index), m_process_id(p.m_process_id), cache_stage(p.cache_stage), m_lastbound(p.m_lastbound)
{
    m_process.update(home, share, p.m_process);
    m_cost.update(home, share, p.m_cost);
}

CostPropagator* CostPropagator::copy (Space& home, bool share) 
{
    return new (home) CostPropagator(home, share, *this);
}

size_t CostPropagator::dispose (Space& home) 
{
    m_process.cancel(home, *this, Int::PC_INT_BND);
    m_cost.cancel(home, *this, Int::PC_INT_BND);
    
    (void) Propagator::dispose(home);
    
    return sizeof(*this);
}

PropCost CostPropagator::cost (const Space& home, const ModEventDelta& delta) const
{
    return PropCost::binary(PropCost::HI);
}

ExecStatus CostPropagator::propagate (Space& home, const ModEventDelta& delta)
{
    RescheduleSpace& space = static_cast<RescheduleSpace&>(home);
    
    if (m_process.assigned()) {
        return home.ES_SUBSUMED(*this);
    }
    
    ProcessList blacklist;
    std::pair<int, int> cost_bound;
    
    if (true || cache_stage == -1) {
        m_lastbound = initCache(space, blacklist);
        cache_stage = (int)space.modified_machines.size();
    } else {
        // updating the cost cache is only necessary if either the min-max machines changed
        // or the bound is closer than min-max
        bool need_update = m_lastbound.first < m_cost.min() || m_lastbound.second > m_cost.max();
        
        if (!need_update) {
            unsigned int min_machine = space.cost_cache.bound(m_index).min.machine;
            unsigned int max_machine = space.cost_cache.bound(m_index).max.machine;
            
            for (int i = cache_stage; i < space.modified_machines.size(); ++i) {
                if (space.modified_machines[i] == min_machine || space.modified_machines[i] == max_machine) {
                    need_update = true;
                    break;
                }
            }
        }
        
        if (need_update) {
            m_lastbound = updateCache(space, blacklist, cache_stage);
            cache_stage = (int)space.modified_machines.size();
        }
    }
    
    for (ProcessList::const_iterator iter = blacklist.begin(); iter != blacklist.end(); ++iter) {
        GECODE_ME_CHECK(m_process.nq(home, (int)(*iter)));
    }
    
    GECODE_ME_CHECK(m_cost.gq(space, m_lastbound.first));
    GECODE_ME_CHECK(m_cost.lq(space, m_lastbound.second));
    
    return ES_NOFIX;
}

std::pair<int, int> CostPropagator::initCache (RescheduleSpace& space, ProcessList& blacklist)
{
    const Process& process = space.instance.process[m_process_id];
    CostBound bound;
    bound.min.cost = Gecode::Int::Limits::max;
    bound.max.cost = Gecode::Int::Limits::min;
    
    for (Int::ViewValues<Int::IntView> m(m_process); m(); ++m) {
        std::pair<int, int> cost = this->getAdditionalCost(space, process, m.val());
        
        if (cost.first == Gecode::Int::Limits::max) {
            blacklist.push_back(m.val());
        } else {
            // check remaining load cost
            if (cost.first > m_cost.max() || cost.second < m_cost.min()) {
                blacklist.push_back(m.val());
            } else {
                space.cost_cache.setCost(m_index, m.val(), cost);
                if (bound.min.cost > cost.first) {
                    bound.min = BoundMachine(m.val(), cost.first);
                }
                if (bound.max.cost < cost.second) {
                    bound.max = BoundMachine(m.val(), cost.second);
                }
            }
        }
    }
    
    space.cost_cache.setBound(m_index, bound);
    return std::pair<int, int>((int)bound.min.cost, (int)bound.max.cost);
}

std::pair<int, int> CostPropagator::updateCache (RescheduleSpace& space, ProcessList& blacklist, int cache_pos, bool need_sweep)
{
    const Process& process = space.instance.process[m_process_id];
    
    // for all machines changed since the last update
    for (unsigned int last = -1; cache_pos < space.modified_machines.size(); ++cache_pos) {
        if (last != space.modified_machines[cache_pos]) {
            // getAdditional cost for machine
            std::pair<int, int> cost = this->getAdditionalCost(space, process, space.modified_machines[cache_pos]);
            space.cost_cache.setCost(m_index, space.modified_machines[cache_pos], cost);
            last = space.modified_machines[cache_pos];
        }
    }
    
    CostBound bound;
    bound.min.cost = Gecode::Int::Limits::max;
    bound.max.cost = Gecode::Int::Limits::min;
    
    for (Int::ViewValues<Int::IntView> m(m_process); m(); ++m) {
        std::pair<int, int> cost = space.cost_cache.getCost(m_index, m.val());
        
        if (cost.first == Gecode::Int::Limits::max) {
            blacklist.push_back(m.val());
        } else {
            // check remaining load cost
            if (cost.first > m_cost.max() || cost.second < m_cost.min()) {
                blacklist.push_back(m.val());
                space.cost_cache.remove(m_index, m.val());
            } else {
                if (bound.min.cost > cost.first) {
                    bound.min.cost = cost.first;
                    bound.min.machine = m.val();
                }
                if (bound.max.cost < cost.second) {
                    bound.max.cost = cost.second;
                    bound.max.machine = m.val();
                }
            }
        }
    }
    
    space.cost_cache.setBound(m_index, bound);
    return std::pair<int, int>((int)bound.min.cost, (int)bound.max.cost);
}

std::pair<int, int> CostPropagator::getAdditionalCost(const RescheduleSpace& space, const Process& process, unsigned int machine_id)
{
    const Machine& machine = space.instance.machine[machine_id];
    PatchMap::const_iterator delta = space.delta.find(machine_id);
    const MachinePatch* patch = NULL;
    int cost = 0;
    
    if (delta != space.delta.end()) {
        patch = &(delta->second);
    } else {
        patch = NULL;
    }
    
    
    // check machine capacity and get excess load costs
    cost += this->getExcessCost(space, process, machine_id, machine, space.state.excess[machine_id], space.state.transient[machine_id], patch);
    if (cost == Gecode::Int::Limits::max) {
        return std::pair<int, int>(cost, cost);
    }
    
    // add process move cost
    if (process.original_machine != machine_id) {
        cost += process.move_cost * space.instance.weight_process_move_cost;
    }
    
    // add machine move cost
    cost += space.instance.machine[process.original_machine].move_cost[machine_id] * space.instance.weight_machine_move_cost;
    
    // get balance costs
    std::pair<int, int> balance_cost = this->getBalanceCost(space, process, machine, space.state.balance[machine_id], patch);
    
    int min_cost = cost + balance_cost.first;
    int max_cost = cost + balance_cost.second;
    
    return std::pair<int, int>(min_cost, max_cost);
}

int CostPropagator::getExcessCost (const RescheduleSpace& space, const Process& process, unsigned int machine_id, const Machine& machine, const MachineLoad& load, const MachineLoad& transient, const MachinePatch* patch)
{
    int cost = 0;
    
    for (unsigned int r = 0; r < machine.capacity.size(); ++r) {
        int gap = machine.capacity[r] - machine.safety_capacity[r];
        int excess = patch ? patch->excess[r] : load[r];
        int transientload = r < space.instance.transient_count ? (patch ? patch->transient[r] : transient[r]) : 0;
        gap -= excess;
        
        // capacity constraint resource failed
        if (gap < process.requirement[r]) {
            return Gecode::Int::Limits::max;
        }
        
        // transient capacity constraint for resource failed
        if (r < space.instance.transient_count && transientload + (process.original_machine == machine_id ? 0 : process.requirement[r]) > machine.capacity[r]) {
            return Gecode::Int::Limits::max;
        }
        
        int old_cost = std::max(0, excess);
        int new_cost = std::max(0, excess + process.requirement[r]);
        
        cost += (new_cost - old_cost) * space.instance.resource[r].weight_load_cost;
    }
    
    return cost;
}

std::pair<int, int> CostPropagator::getBalanceCost (const RescheduleSpace& space, const Process& process, const Machine& machine, const MachineBalance& balance, const MachinePatch* patch)
{
    int min_cost = 0;
    int max_cost = 0;
    
    for (unsigned int b = 0; b < space.instance.balance.size(); ++b) {
        const Balance& bal = space.instance.balance[b];
        
        int machine_balance = patch ? patch->balance[b] : balance[b];
        int process_balance = process.requirement[bal.resource2] - bal.balance * process.requirement[bal.resource1];
        
        if (process_balance < 0) {
            int old_min = std::max(0, machine_balance + space.max_unassigned_balance[b]);
            int new_min = std::max(0, machine_balance + space.max_unassigned_balance[b] + process_balance);
            
            int old_max = std::max(0, machine_balance + space.min_unassigned_balance[b] - process_balance);
            int new_max = std::max(0, machine_balance + space.min_unassigned_balance[b]);
            
            min_cost += (new_min - old_min) * bal.weight_balance_cost;
            max_cost += (new_max - old_max) * bal.weight_balance_cost;
        } else {
            int old_min = std::max(0, machine_balance + space.min_unassigned_balance[b] - process_balance);
            int new_min = std::max(0, machine_balance + space.min_unassigned_balance[b]);
            
            int old_max = std::max(0, machine_balance + space.max_unassigned_balance[b]);
            int new_max = std::max(0, machine_balance + space.max_unassigned_balance[b] + process_balance);
            
            min_cost += (new_min - old_min) * bal.weight_balance_cost;
            max_cost += (new_max - old_max) * bal.weight_balance_cost;
        }
    }
    
    return std::pair<int, int>(min_cost, max_cost);
}

ExecStatus CostPropagator::post (Gecode::Home home, unsigned int index, unsigned int process_id)
{
    if (home.failed()) {
        return ES_FAILED;
    }
    
    RescheduleSpace& space = static_cast<RescheduleSpace&>((Space&)(home));
    
    new (home) CostPropagator (home, index, process_id, space.process[index], space.process_move_cost[index]);
    
    return ES_OK;
}
