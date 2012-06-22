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

#include "ProcessNeighborhoodSearch.h"

#include <gecode/gist.hh>

using namespace Gecode;

ProcessNeighborhoodSearch::ProcessNeighborhoodSearch(int identifier, time_t start_time) :
IterativeSearch(identifier, start_time)
{ }

ProcessNeighborhoodSearch::~ProcessNeighborhoodSearch ()
{ }

ReAssignment* ProcessNeighborhoodSearch::runOnce(const ReAssignment* current_state)
{
    const Instance& instance = *current_state->instance;
    
    std::vector<ProcessCost> pcost;
    process_cost(*current_state, pcost);
    std::sort(pcost.begin(), pcost.end());
    
    int i = 0;
    while (i < pcost.size() && pcost[i].cost > 0)
        i++;
    pcost.resize(i);
    
    std::vector<int> except_process;
    
    // choose 4 processes based on the sorted list and 3 additional random processes
    int start = 0;
    int step = 4;
    int size_opt = 4;
    int size_rand = 3;
    
    ReAssignment* solution = NULL;
    do
    {
        ProcessList n(size_opt+size_rand);
        int t = 0;
        for (int p = start; p < start + size_opt && p < pcost.size(); ++p) {
            n[t++] = pcost[p].index;
        }
        
        size_rand = size_opt+size_rand - t;
        for (int i = 0; i < size_rand; i++) {
            unsigned int rp;
            bool ok;
            do {
                ok = true;
                rp = instance.movable_processes_by_size[rand() % instance.num_movable_processes];
                for (int j = 0; j < t; j++)
                    if (n[j] == rp)
                        ok = false;
            } while (!ok);
            n[t++] = rp;
        }
        
        RescheduleSpace space(instance, *current_state, n);
        
        Gecode::Search::Options o;
        o.stop = new Gecode::Search::FailStop(n.size()*5);
        Gecode::DFS<RescheduleSpace> algo(&space, o);
        RescheduleSpace* solutionSpace = algo.next();
        delete o.stop;
        if (solutionSpace) {
            solution = solutionSpace->getResultState();
            delete solutionSpace;
        } else {
            start += step;
        }
    } while (!solution && start < pcost.size() && time(NULL) < time_limit);
    
    return solution;
}
