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

#include "BestCostBrancher.h"
#include "RescheduleSpace.h"

using namespace Gecode;

ProcessChoice::ProcessChoice (const Brancher& b, int _process, int _machine) :
Choice(b, 2),
process(_process),
machine(_machine)
{ }

ProcessChoice::ProcessChoice (const Brancher& b, Archive& e) :
Choice(b, 2)
{
    e >> process >> machine;
}

void ProcessChoice::archive (Archive& e) const
{
    Choice::archive(e);
    e << process << machine;
}

size_t ProcessChoice::size () const
{
    return sizeof(*this);
}

BestCostBrancher::BestCostBrancher (Home& home, IntVarArray& _process, IntVarArray& _cost) :
Brancher(home), process(home, IntVarArgs(_process)), cost(home, IntVarArgs(_cost))
{ }

BestCostBrancher::BestCostBrancher (Gecode::Space& space, bool share, BestCostBrancher& b) :
Brancher(space, share, b)
{
    process.update(space, share, b.process);
    cost.update(space, share, b.cost);
}

BestCostBrancher* BestCostBrancher::copy (Gecode::Space& space, bool share)
{
    return new (space) BestCostBrancher(space, share, *this);
}

void BestCostBrancher::post (Gecode::Home home, Gecode::IntVarArray& process, Gecode::IntVarArray& cost)
{
    if (home.failed()) {
        return;
    }
    
    new (home) BestCostBrancher(home, process, cost);
}

bool BestCostBrancher::status (const Gecode::Space& space) const
{
    for (unsigned int p = 0; p < process.size(); ++p) {
        if (!process[p].assigned()) {
            return true;
        }
    }
    
    return false;
}

Gecode::Choice* BestCostBrancher::choice (Gecode::Space& _space)
{
    RescheduleSpace& space = static_cast<RescheduleSpace&>(_space);
    
    int max_cost_savings = Gecode::Int::Limits::min;
    int max_index = -1;
    
    for (int i = 0; i < process.size(); ++i) {
        if (!process[i].assigned() && cost[i].max() - cost[i].min() > max_cost_savings) {
            max_cost_savings = cost[i].max() - cost[i].min();
            max_index = i;
        }
    }
    
    if (max_index >= 0) {
        unsigned int opt_machine = space.cost_cache.bound(max_index).min.machine;
        
        if (!process[max_index].in((int)(opt_machine))) {
            unsigned int opt_machine_cost = -1;
            for (Int::ViewValues<Int::IntView> view(process[max_index]); view(); ++view) {
                unsigned int m = view.val();
                std::pair<int, int> cost = space.cost_cache.getCost(max_index, m);
                if (cost.first < opt_machine_cost) {
                    opt_machine = m;
                    opt_machine_cost = cost.first;
                }
            }
        }
        
        return new ProcessChoice(*this, max_index, opt_machine);
    }
    
    GECODE_NEVER;
    return NULL;
}

Gecode::Choice* BestCostBrancher::choice (const Gecode::Space& space, Gecode::Archive& e)
{
    return new ProcessChoice(*this, e);
}

Gecode::ExecStatus BestCostBrancher::commit (Gecode::Space& space, const Gecode::Choice& c, unsigned int a)
{
    const ProcessChoice& choice = static_cast<const ProcessChoice&>(c);
    
    if (a == 0) {
        // assign process to given machine
        GECODE_ME_CHECK(process[choice.process].eq(space, choice.machine));
        static_cast<RescheduleSpace&>(space).modified_machines.push_back(choice.machine);
    } else {
        // exclude process from the given machine
        GECODE_ME_CHECK(process[choice.process].nq(space, choice.machine));
    }
    
    return ES_OK;
}

size_t BestCostBrancher::dispose (Gecode::Space& space)
{
    Brancher::dispose(space);
    
    return sizeof(*this);
}
