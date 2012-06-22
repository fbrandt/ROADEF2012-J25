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

#include "IterativeSearch.h"

#include <time.h>
#include <gecode/gist.hh>
#include <algorithm>


using namespace std;
using namespace Gecode;

IterativeSearch::IterativeSearch (int _identifier, time_t _start_time, bool _abort_on_nonimproving) :
identifier(_identifier), BaseSearch(_start_time), abort_on_nonimproving(_abort_on_nonimproving)
{ }

IterativeSearch::~IterativeSearch ()
{ }

ReAssignment* IterativeSearch::run(const ReAssignment* best_known, time_t time_limit)
{
    this->time_limit = time_limit;
    unsigned int i = 0;
    unsigned int fail_count = 0;
    ReAssignment* best = NULL;
    while(time(NULL) < time_limit && fail_count < 50000) {
        i++;
        
        ReAssignment* solution = runOnce(best ? best : best_known);
        if (solution) {
            if (best) delete best;
            best = solution;
            
            #ifdef LOGGING
            std::cerr << identifier << " " << i << " " << time(NULL) - start_time << " " << best->getCost() << std::endl;
            #endif
            
            fail_count = 0;
        } else {
            fail_count++;
        }
    }
    return best;
}

/**
 * Calculate for each process an upper bound of cost reduction, when the process is moved
 */
void IterativeSearch::process_cost(const ReAssignment& state, std::vector<ProcessCost>& cost)
{
    Instance& instance = *state.instance;
    const Assignment& initial_state = state.assignment;
    
    cost.clear();
    cost.resize(instance.num_processes, ProcessCost(0,0));
    
    int c = 0;
    
    // determine costs for each process
    for (unsigned int p = 0; p < instance.num_processes; ++p)
    {
        Process& process = instance.process[p];
        if (process.fixed)
            continue;
        
        cost[c].index = (int)p;
        
        int m = (int)initial_state[p];
        
        for (unsigned int r = 0; r < instance.num_resources; ++r)
        {
            int excess = state.excess[m][r];
            cost[c].cost += (std::max(0, excess) - std::max(0, (int)(excess - instance.process[p].requirement[r]))) * instance.resource[r].weight_load_cost;
        }
        
        // process and machine move cost
        if (m != process.original_machine) {
            cost[c].cost += process.move_cost * instance.weight_process_move_cost;
            cost[c].cost += instance.machine[process.original_machine].move_cost[m] * instance.weight_machine_move_cost;
		}
      
        c++;
    }
    
    cost.resize(c);
}
