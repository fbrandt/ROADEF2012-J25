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

#include "UndoMoveSearch.h"
#include <gecode/gist.hh>

using namespace Gecode;

UndoMoveSearch::UndoMoveSearch(int identifier, time_t _start_time) :
IterativeSearch(identifier, _start_time)
{ }

UndoMoveSearch::~UndoMoveSearch()
{ }

ReAssignment* UndoMoveSearch::runOnce(const ReAssignment* state)
{
    ReAssignment* solution = NULL;
    const Instance& instance = *state->instance;
    const Assignment& assignment = state->assignment;
    
    std::vector<ProcessCost> cost(instance.num_processes);
    
    int p_start = rand() % instance.num_processes;
    int p = p_start;
    while (assignment[p] == instance.process[p].original_machine) {
        p = (p+1) % instance.num_processes;
        if (p == p_start) // no moved processes
            return 0;
    }
    
    int m = instance.process[p].original_machine;
    
    // processes that have been moved to this machine
    std::vector<int> moved(instance.num_processes);
    
    int t = 0;
    for (int i = 0; i < instance.num_processes; i++) {
        if (assignment[i] == m && instance.process[i].original_machine != m) {
            moved[t++] = i;
        }
    }
    moved.resize(t);
    std::random_shuffle(moved.begin(), moved.end());
    
    int num_remove = 5;
    if (moved.size() > num_remove)
        moved.resize(num_remove);
    
    ProcessList n(moved.size()+1);
    n[0] = p;
    for (int i = 0; i < moved.size(); i++)
        n[i+1] = moved[i];
    
    RescheduleSpace space(instance, *state, n);
    rel(space, space.process[0], IRT_EQ, m);
    
    Gecode::Search::Options o;
    o.stop = new Gecode::Search::FailStop(n.size() * 5);
    Gecode::DFS<RescheduleSpace> algo(&space, o);
    RescheduleSpace* solutionSpace = NULL;
    
    int not_orig1 = 0;
    for (int i = 0; i < instance.num_processes; i++) {
        if (assignment[i] != instance.process[i].original_machine)
            not_orig1++;
    }
    
    if ((solutionSpace = algo.next())) {
        solution = solutionSpace->getResultState();
        delete solutionSpace;
        int not_orig2 = 0;
        for (int i = 0; i < instance.num_processes; i++)
            if (solution->assignment[i] != instance.process[i].original_machine)
                not_orig2++;
    }
    delete o.stop;
    return solution;
}
