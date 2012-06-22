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

#include "RandomSearch.h"

#include <gecode/gist.hh>

using namespace Gecode;

RandomSearch::RandomSearch (int identifier, time_t start_time, int _neighborhood) :
IterativeSearch(identifier, start_time, false), neighborhood(_neighborhood)
{ }

RandomSearch::~RandomSearch(void)
{ }

ReAssignment* RandomSearch::runOnceFast(const ReAssignment* state)
{
    const Instance& instance = *(state->instance);
    ProcessList n(neighborhood);
    for (unsigned int t = 0; t < neighborhood; ++t) {
        int rp = instance.movable_processes_by_size[rand() % instance.num_movable_processes];
        n[t] = rp;
    }
    std::sort(n.begin(), n.end());
    ProcessList::iterator last = unique(n.begin(), n.end());
    n.resize(last - n.begin());
    
    RescheduleSpace space(instance, *state, n);
    Gecode::Search::Options o;
    o.stop = new Gecode::Search::FailStop(n.size() * 5);
    Gecode::DFS<RescheduleSpace> algo(&space, o);
    RescheduleSpace* best = NULL;
    RescheduleSpace* solution = NULL;
    
    if ((solution = algo.next())) {
        if (best) delete best;
        best = solution;
    }
    delete o.stop;
    
    ReAssignment* assignment = NULL;
    if (best) {
        assignment = best->getResultState();
        delete best;
    }
    
    return assignment;
}

ReAssignment* RandomSearch::runOnceWeighted(const ReAssignment* state)
{
    const Instance& instance = *(state->instance);
    
    std::vector<ProcessCost> pcost;
    process_cost(*state, pcost);
    
    long sum = 0;
    for (int i = 0; i < pcost.size(); i++)
        sum += pcost[i].cost + 10;
    
    std::map<double, int> cum_map;
    
    double prob_sum = 0;
    for (int i = 0; i < pcost.size(); i++) {
        prob_sum += (double)(pcost[i].cost+10) / (double)sum;
        cum_map[prob_sum] = i;
    }
    
    int count = std::min(neighborhood, (int)pcost.size());
    
    ReAssignment* solution = NULL;
    while (!solution && time(NULL) < time_limit && count > 0) {
        std::vector<bool> is_selected(pcost.size(), false);
        ProcessList n(count);
        for (int i = 0; i < count; i++) {
            int pi = -1;
            while (pi == -1 || is_selected[pi]) {
                double rnd = rand()*1.0/RAND_MAX;
                std::map<double,int>::iterator it = cum_map.lower_bound(rnd);
                if (it == cum_map.end())
                    continue;
                pi = it->second;
            }
            is_selected[pi] = true;
            n[i] = pcost[pi].index;
        }
        
        RescheduleSpace space(instance, *state, n);
        Gecode::Search::Options o;
        o.stop = new Gecode::Search::FailStop(n.size() * 5);
        Gecode::DFS<RescheduleSpace> algo(&space, o);
        RescheduleSpace* solutionSpace = algo.next();
        if (solutionSpace) {
            solution = solutionSpace->getResultState();
            delete solutionSpace;
        }
        
        delete o.stop;
    }
    
    return solution;
}

ReAssignment* RandomSearch::runOnce(const ReAssignment* state)
{
    return runOnceWeighted(state);
}
