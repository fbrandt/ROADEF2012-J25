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

#include <fstream>
#include <time.h>

#include <gecode/gist.hh>

#include <pthread.h>

#include "Instance.h"
#include "RescheduleSpace.h"
#include "RandomSearch.h"
#include "TargetMoveSearch.h"
#include "UndoMoveSearch.h"
#include "ProcessNeighborhoodSearch.h"
#include "SchedulePlotter.h"
#include "ProcessFixing.h"

using namespace std;

// lock for exclusive access to global_best
pthread_mutex_t mutex1 = PTHREAD_MUTEX_INITIALIZER; 

// global optimal solution
ReAssignment* global_best;

int write_counter;

void print (char* file, ReAssignment* best = NULL)
{
    std::ostream* out = &std::cout;
    std::ofstream fout;
    
    if (file != NULL) {
        fout.open(file);
        if (fout.good()) {
            out = &fout;
        }
    }
    
    if (best) {
        std::copy(best->assignment.begin(), best->assignment.end(), std::ostream_iterator<int>(*out, " "));
        (*out) << std::endl;
    } else {
        (*out) << "no solution found" << std::endl;
    }
    
    if (file != NULL) {
        fout.close();
    }
}

struct SearchEntry {
public:
    SearchEntry(string _label, BaseSearch* _search, int _start_time, int _end_time, int _duration) : label(_label), search(_search), start_time(_start_time), end_time(_end_time), duration(_duration), active(true) {};
    
    string label;
    BaseSearch* search;
    int start_time; // time after which search is started
    int end_time; // time after which search is ended
    int duration; // duration of one run
    bool active; // flag to enable/disable the search strategy
};

struct threadworkdata{
    vector<SearchEntry>* searches;
    ReAssignment* best;
    time_t deadline;
    Instance* instancep;
    time_t start;
    char* solution_file;
    bool manage_process_fixing;
};

// thread function
void * threadwork(void* datav)
{
    threadworkdata data = *((threadworkdata*) datav);
    ProcessFixing process_fixing(*(data.instancep));
    if (data.manage_process_fixing)
    	process_fixing.fixTransient(data.instancep->num_processes > 3000 ? 0.9 : 0.8);
    ReAssignment* new_best = NULL;
    
    while (time(NULL) < data.deadline) {
        // rotate through searches
        for (int i = 0; i < data.searches->size(); i++) {
            SearchEntry& se = (*(data.searches))[i];
            
            time_t cur_time = time(NULL);
            if (cur_time >= data.deadline)
                break;
            
            // skip search if not active or not within time constraints
            if (!se.active || cur_time-data.start < se.start_time || (se.end_time >= 0 && cur_time-data.start > se.end_time))
                continue;
            
            #ifdef LOGGING
            std::cerr << cur_time-data.start << ": Starting " << se.label << endl;
            #endif
            
            // check whether better solution exists
            pthread_mutex_lock(&mutex1);
            if (data.best->getCost() > global_best->getCost())
            {
                delete data.best;
                data.best = new ReAssignment(*global_best);
            }
            pthread_mutex_unlock(&mutex1);
            
            // run search
            new_best = se.search->run(data.best, std::min(time(NULL) + se.duration, data.deadline));
            
            if (new_best) {
                // synchronisation
                pthread_mutex_lock(&mutex1);
                if(new_best->getCost() < global_best->getCost())
                {
                    // we found a better solution
                    delete global_best;
                    global_best = new ReAssignment(*new_best);
                    write_counter++;
                    if(write_counter > 4)
                    {
                        // write solution to file
                        #ifdef LOGGING
                        std::cerr << "Result: " << global_best->load_cost << " " << global_best->balance_cost << " " << global_best->process_moves << " " << global_best->machine_moves << std::endl;
                        #endif
                        print(data.solution_file, global_best);
                        write_counter = 0;
                    }
                }
                pthread_mutex_unlock( &mutex1 );
                delete data.best;
                data.best = new_best;
            }
            
            // release fixed processes after 45 seconds
            if (data.manage_process_fixing && cur_time-data.start >= 45) {
                data.manage_process_fixing = false;
                process_fixing.reset();
            }
        }
    }
}

/**
 * Run solver if all parameters are present.
 * Otherwise print usage information to stderr
 */
int main (int args, char** argv)
{
    time_t start = time(NULL);
    time_t time_limit = -1;
    int seed = -1;
    int neighbor = -1;
    
    char* model = NULL; // the machine model file
    char* initial = NULL; // initial solution file
    char* current = NULL; // current solution file
    char* solution_file = NULL; // best solution of this run file
    
    bool chart = false;
    bool depgraph = false;
    
    for (int a = 1; a < args; ++a)
    {
        if (argv[a][0] == '-')
        {
            // its a parameter
            switch (argv[a][1])
            {
                case 't': // time limit
                    time_limit = atoi(argv[++a]);
                    break;
                case 'p': // problem instance
                    model = argv[++a];
                    break;
                case 'i': // initial solution
                    initial = argv[++a];
                    break;
                case 'j': // current solution
                    current = argv[++a];
                    break;
                case 'c': // make plot of solution
                    chart = true;
                    break;
                case 'd': // make service dependency graph
                    depgraph = true;
                    break;
                case 'r': // random search, size of the neighborhood
                    neighbor = atoi(argv[++a]);
                    break;
                case 'o': // result file
                    solution_file = argv[++a];
                    break;
                case 'n': // team name
                    std::cout << "J25" << std::endl;
                    return 0;
                    break;
                case 's': // seed
                    seed = atoi(argv[++a]);
                    srand(seed);
                    #ifdef LOGGING
                    std::cerr << "Init rand() first number generated: " << rand() << std::endl;
                    #endif
                    break;
                default:  // unknown parameter -> quit
                    std::cerr << "Unknown parameter: " << argv[a] << std::endl;
                    return 1;
            }
        }
    }
    
    time_t firstdeadline = start + (time_limit/2);
    time_t deadline = start + time_limit - 1; // one second buffer time
    
    if (model == NULL)
    {
        std::cerr << "Model file not given" << std::endl;
        return 1;
    }
    
    ifstream model_file(model);
    if (!model_file.good())
    {
        std::cerr << "Could not open model file" << std::endl;
        return 1;
    }
    
    if (initial == NULL)
    {
        std::cerr << "Assignment file not given" << std::endl;
        return 1;
    }
    
    ifstream assignment_file(initial);
    if (!assignment_file.good())
    {
        std::cerr << "Could not open assignment file" << std::endl;
        return 1;
    }
    
    #ifdef LOGGING
    std::cerr << "reading instance " << model_file << " ... ";
    #endif
    
    Instance instance(model_file);
    Assignment initial_state;
    std::copy(istream_iterator<int>(assignment_file), istream_iterator<int>(), back_inserter(initial_state));
    for (int p = 0; p < instance.num_processes; p++)
        instance.process[p].original_machine = initial_state[p];
    
    #ifdef LOGGING
    std::cerr << "done" << std::endl;
    #endif
    
    ostream* out = &std::cout;
    
    model_file.close();
    assignment_file.close();
    
    if (chart)
    {
        Assignment current_state(initial_state);
        if (current != NULL) {
            ifstream current_file(current);
            std::copy(istream_iterator<int>(current_file), istream_iterator<int>(), current_state.begin());
        }
        
        SchedulePlotter::plot(*out, instance, initial_state, current_state);
    }
    else if (depgraph)
    {
        std::cout << "digraph {" << std::endl;
        for (unsigned int s = 0; s < instance.service.size(); ++s)
        {
            for (unsigned int d = 0; d < instance.service[s].depends_on.size(); ++d)
            {
                std::cout << s << " -> " << instance.service[s].depends_on[d] << ";" << std::endl;
            }
        }
        std::cout << "graph [ file = \"" << model << "\" ]" << std::endl << "}" << std::endl;
    }
    else
    {
        #ifdef LOGGING
        std::cerr << "Setup space ... ";
        #endif
        
        // thread data
        threadworkdata *data1 = (threadworkdata*)malloc(sizeof(threadworkdata));
        threadworkdata *data2 = (threadworkdata*)malloc(sizeof(threadworkdata));
        
        ReAssignment initial_solution;
        instance.reorderResources();
        instance.setAssignment(initial_state, &initial_solution);
        
        data1->instancep = &instance;
        data2->instancep = &instance;
        data1->start = start;
        data1->deadline = deadline;
        data2->start = start;
        data2->deadline = deadline;
        data1->solution_file = solution_file;
        data2->solution_file = solution_file;
        data1->manage_process_fixing = true;
        data2->manage_process_fixing = false;
        
        data1->best = new ReAssignment;
        data2->best = new ReAssignment;
        global_best = new ReAssignment;
        
        instance.setAssignment(initial_state, data1->best);
        instance.setAssignment(initial_state, data2->best);
        instance.setAssignment(initial_state, global_best);
        write_counter = 0;
        
        TargetMoveSearch tms1(11, start);
        TargetMoveSearch tms2(12, start);
        ProcessNeighborhoodSearch pns1(21, start);
        ProcessNeighborhoodSearch pns2(22, start);
        RandomSearch rs1(31, start, 7);
        RandomSearch rs2(32, start, 9);
        UndoMoveSearch ums1(41, start);
        UndoMoveSearch ums2(42, start);
        
        data1->searches = new vector<SearchEntry>;
        data2->searches = new vector<SearchEntry>;
        
        data1->searches->push_back(SearchEntry("P1: 11 TMS", &tms1, 0, 45, 5)); // earliest start: 0, latest start: 45, duration: 5 seconds
        data1->searches->push_back(SearchEntry("P1: 21 PNS", &pns1, 0, -1, 4)); // earliest start: 0, latest start: -, duration: 4 seconds
        data1->searches->push_back(SearchEntry("P1: 31 RS7", &rs1, 60, -1, 4)); // earliest start: 60, latest start: -, duration: 4 seconds
        data1->searches->push_back(SearchEntry("P1: 41 UMS", &ums1, 0, -1, 1)); // earliest start: 0, latest start: -, duration: 1 seconds
        
        data2->searches->push_back(SearchEntry("P2: 22 PNS", &pns2, 0, -1, 5)); // earliest start: 0, latest start: -, duration: 5 seconds
        data2->searches->push_back(SearchEntry("P2: 12 TMS", &tms2, 0, 60, 5)); // earliest start: 0, latest start: 60, duration: 5 seconds
        data2->searches->push_back(SearchEntry("P2: 42 UMS", &ums2, 0, -1, 1)); // earliest start: 0, latest start: -, duration: 1 seconds
        data2->searches->push_back(SearchEntry("P2: 32 RS9", &rs2, 60, -1, 4)); // earliest start: 60, latest start: -, duration: 4 seconds
        
        pthread_t* iThreadIds = new pthread_t[2];
        
        int rv = pthread_create(&iThreadIds[0], NULL, &threadwork, data1);
        
        #ifdef LOGGING
        if (rv < 0) printf("error creating thread.\n");
        #endif
        
        rv = pthread_create(&iThreadIds[1], NULL, &threadwork, data2);
        
        #ifdef LOGGING
        if (rv < 0) printf("error creating thread.\n");
        #endif
        
        int i, status;
        for (i = 0; i < 2; i++) {
            status = pthread_join(iThreadIds[i],NULL);
            #ifdef LOGGING
            if (status != 0) printf("error in thread %d with id %d\n", i, (int)iThreadIds[i]);
            else printf(" thread %d with id %d stopped without errors\n", i, (int)iThreadIds[i]);
            #endif
        }
        
        // write final solution to file
        std::cerr << "Final result: " << global_best->load_cost << " " << global_best->balance_cost << " " << global_best->process_moves << " " << global_best->machine_moves << std::endl;
        print(solution_file, global_best);
        delete global_best;
        
    }
    
    return 0;
}
