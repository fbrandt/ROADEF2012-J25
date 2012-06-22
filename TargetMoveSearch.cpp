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

#include "TargetMoveSearch.h"
#include <gecode/gist.hh>

using namespace Gecode;

TargetMoveSearch::TargetMoveSearch(int identifier, time_t _start_time) :
IterativeSearch(identifier, _start_time)
{ }

TargetMoveSearch::~TargetMoveSearch()
{ }

ReAssignment* TargetMoveSearch::runOnce(const ReAssignment* current_state)
{
    ReAssignment* solution = NULL;
    const Instance& instance = *current_state->instance;
    const Assignment& initial_state = current_state->assignment;
    static unsigned int last_p = 0;
    std::vector<ProcessCost> cost(instance.num_processes);
    
    int t = 0;
    
    // determine costs for each process
    for (unsigned int p = 0; p < initial_state.size(); ++p)
    {
        if (instance.process[p].fixed) continue;
        
        cost[t].index = p;
        cost[t].cost = 0;
        
        for (unsigned int r = 0; r < instance.num_resources; ++r)
        {
            int m = initial_state[p];
            int excess = current_state->excess[m][r];
            cost[t].cost += (std::max(0, excess) - std::max(0, (int)(excess - instance.process[p].requirement[r]))) * instance.resource[r].weight_load_cost;
        }
        t++;
    } 
    cost.resize(t);
    sort(cost.begin(), cost.end());
    
    if (last_p >= cost.size()) 
        last_p = 0;
    
    // determine process that causes the highest load costs
    for (unsigned int _p = 0; !solution && _p < cost.size() && cost[_p].cost > 0 && time(NULL) < time_limit; ++_p)
    {
        last_p = _p;
        int p = cost[_p].index;
        
        std::vector<ProcessCost> addcost(instance.num_machines, ProcessCost(-1));
        int mm = 0;
        
        for (int m = 0; m < instance.num_machines; m++) {
            bool valid = true;
            addcost[mm].index = m;
            addcost[mm].cost = 0;
            
            for (unsigned int r = 0; r < instance.num_resources; ++r)
            {
                if (instance.machine[m].capacity[r] < instance.process[p].requirement[r]) {
                    valid = false;
                    break;
                }
                int excess = current_state->excess[m][r];
                int cc = (excess + instance.process[p].requirement[r]);
                addcost[mm].cost += (cc > 0 ? 2 : 1) * cc;
            }
            
            if (valid) mm++; 
        }
        
        addcost.resize(mm);
        sort(addcost.begin(), addcost.end());
        
        for (int _m = addcost.size() - 1; solution == NULL && _m >= 0 && cost[_p].cost > addcost[_m].cost && time(NULL) < time_limit; --_m)
        {
            int m = addcost[_m].index;
            if (m != -1 && m != current_state->assignment[p])
            {
                ProcessList n(instance.num_processes);
                int t = 0;
                
                int remove_num = 7; // remove (up to 7) most expensive processes from the considered machine
                
                for (unsigned int i = 0; i < cost.size(); ++i) {
                    int p = cost[i].index;
                    if (!instance.process[p].fixed && current_state->assignment[p] == m) {
                        n[t++] = p;
                    }
                }
                n.resize(t);
                
                std::random_shuffle(n.begin(), n.end());
                t = std::min(t, remove_num); 
                n.resize(t+1);
                n[t] = p;
                
                RescheduleSpace space(instance, *current_state, n);
                int index = t;
                
                rel(space, space.process[index], IRT_EQ, m);
                
                Gecode::Search::Options o;
                o.stop = new Gecode::Search::FailStop(n.size() * 5);
                Gecode::DFS<RescheduleSpace> algo(&space, o);
                RescheduleSpace* solutionSpace = NULL;
                
                if (solutionSpace = algo.next()) {
                    solution = solutionSpace->getResultState();
                    delete solutionSpace;
                }
                delete o.stop;
            }
        }
    }
    return solution;
}

