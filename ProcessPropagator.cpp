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

#include "ProcessPropagator.h"

using namespace Gecode;
using namespace std;

ProcessPropagator::ProcessPropagator (Home home, unsigned int index, unsigned int process, IntVar& machine) :
Propagator(home),
m_index(index), m_process(process),
m_machine(machine)
{
    m_machine.subscribe(home, *this, Int::PC_INT_VAL);
}

ProcessPropagator::ProcessPropagator (Home home, bool share, ProcessPropagator& p) :
Propagator(home, share, p),
m_index(p.m_index), m_process(p.m_process)
{
    m_machine.update(home, share, p.m_machine);
}

ProcessPropagator* ProcessPropagator::copy (Space& home, bool share) 
{
    return new (home) ProcessPropagator(home, share, *this);
}

size_t ProcessPropagator::dispose (Space& home) 
{
    m_machine.cancel(home, *this, Int::PC_INT_VAL);
    
    (void) Propagator::dispose(home);
    
    return sizeof(*this);
}

PropCost ProcessPropagator::cost (const Space& home, const ModEventDelta& delta) const
{
    return PropCost::unary(PropCost::LO);
}

ExecStatus ProcessPropagator::propagate(Space& home, const ModEventDelta& delta)
{
    RescheduleSpace& space = static_cast<RescheduleSpace&>(home);
    
    unsigned int machine_id = m_machine.val();
    MachinePatch patch(space.instance, MachinePatch::allocator_type(space));
    
    PatchMap::iterator it;
    if (space.delta.end() != (it = space.delta.find(machine_id))) {
        patch = it->second;
    } else {
        // initialize patch from original
        std::copy(space.state.excess[machine_id].begin(), space.state.excess[machine_id].end(), patch.excess.begin());
        std::copy(space.state.transient[machine_id].begin(), space.state.transient[machine_id].end(), patch.transient.begin());
        std::copy(space.state.balance[machine_id].begin(), space.state.balance[machine_id].end(), patch.balance.begin());
    }
    
    int cost = propagateLoad(space, machine_id, patch);
    
    if (cost < 0) {
        return ES_FAILED;
    }
    
    cost += propagateBalance(space, machine_id, patch);
    
    const Process& process = space.instance.process[m_process];
    
    if (process.original_machine != machine_id) {
        cost += process.move_cost * space.instance.weight_process_move_cost;
    }
    
    cost += space.instance.machine[process.original_machine].move_cost[machine_id] * space.instance.weight_machine_move_cost;
    
    Gecode::Int::IntView process_move_cost(space.process_move_cost[m_index]);
    GECODE_ME_CHECK(process_move_cost.eq(home, cost));
    
    if (space.delta.end() != it) {
        it->second = patch;
    } else {
        space.delta.insert(PatchMap::value_type(machine_id, patch));
    }
    
    return home.ES_SUBSUMED(*this);
}

int ProcessPropagator::propagateLoad (RescheduleSpace& space, unsigned int machine_id, MachinePatch& patch)
{
    // adjust excess load cost
    const Instance& instance = space.instance;
    const Machine& machine = instance.machine[machine_id];
    const Process& process = instance.process[m_process];
    
    long long delta_load_cost = 0;
    
    for (unsigned int r = 0; r < process.requirement.size(); ++r) {
        long long old_excess = std::max(0, patch.excess[r]);
        patch.excess[r] += process.requirement[r];
        long long new_excess = std::max(0, patch.excess[r]);
        
        if (patch.excess[r] > machine.capacity[r] - machine.safety_capacity[r]) {
            return -1;
        }
        
        if (r < instance.transient_count && process.original_machine != machine_id) {
            patch.transient[r] += process.requirement[r];
            if (patch.transient[r] > machine.capacity[r]) {
                return -1;
            }
        }
        
        delta_load_cost += (new_excess - old_excess) * instance.resource[r].weight_load_cost;
    }
    
    // remove processes from machines that do not fit any longer due to capacity constraints
    for (int i = 0; i < space.process.size(); i++) {
        if (space.process[i].assigned())
            continue;
        int p = space.moved[i];
        bool ok = true;
        for (int r = 0; ok && r < instance.num_resources; r++) {
            if (patch.excess[r] + instance.process[p].requirement[r] > machine.capacity[r] - machine.safety_capacity[r])
                ok = false;
            if (r < instance.transient_count && instance.process[p].original_machine != machine_id) {
                if (patch.transient[r] + instance.process[p].requirement[r] > machine.capacity[r])
                    ok = false;
            }
        }
        if (!ok) {
            Int::IntView iv(space.process[i]);
            GECODE_ME_CHECK(iv.nq(space, (int)machine_id));
        }
    }
    
    #ifdef LOGGING
    if (delta_load_cost > Gecode::Int::Limits::max)
        std::cerr << "{ProcessPropagator::propagateLoad} Warning: delta_load_cost exceeds 32bit integer" << std::endl;
    #endif
    
    return (int)(delta_load_cost);
}

int ProcessPropagator::propagateBalance (RescheduleSpace& space, unsigned int machine_id, MachinePatch& patch)
{
    // adjust balance cost
    const Process& process = space.instance.process[m_process];
    
    long long delta_balance_cost = 0;
    
    for (unsigned int b = 0; b < space.instance.balance.size(); ++b) {
        const Balance& balance = space.instance.balance[b];
        int process_balance = process.requirement[balance.resource2] - balance.balance * (process.requirement[balance.resource1]);
        
        if (process_balance < 0) {
            space.min_unassigned_balance[b] -= process_balance;
        } else { 
            space.max_unassigned_balance[b] -= process_balance;
        }
        
        long long old_balance = std::max(0, patch.balance[b]);
        patch.balance[b] += process_balance;
        long long new_balance = std::max(0, patch.balance[b]);
        
        delta_balance_cost += (new_balance - old_balance) * balance.weight_balance_cost;
    }
    
    #ifdef LOGGING
    if (delta_balance_cost > Gecode::Int::Limits::max)
        std::cerr << "{ProcessPropagator::propagateBalance} Warning: delta_balance_cost exceeds 32bit integer" << std::endl;
    #endif
    
    return delta_balance_cost;
}

ExecStatus ProcessPropagator::post (Gecode::Home home, unsigned int index, unsigned int process)
{
    if (home.failed()) {
        return ES_FAILED;
    }
    
    RescheduleSpace& space = static_cast<RescheduleSpace&>((Space&)(home));
    
    new (home) ProcessPropagator (home, index, process, space.process[index]);
    
    return ES_OK;
}
