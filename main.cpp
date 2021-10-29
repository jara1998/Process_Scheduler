#include<iostream>
#include<fstream>
#include<sstream>
#include<string>
#include<string.h>
#include <cstdlib>
#include <stdlib.h>
#include <vector>
#include <list>
#include<queue>
#include<set>
#include <unordered_set>
#include <unordered_map>
#include <iomanip>
#include <map>
#include<getopt.h>
using namespace std;


bool v_flag = false;
bool t_flag = false;
bool e_flag = false;
bool p_flag = false;
int quantum = 100000;
int maxprio = 4;
string sche_name;
ifstream randFile;
ifstream inputFile;
vector<int> rand_arr;
int rand_ofs = 1;
int latest_FT;

enum Transition {TRANS_TO_READY, TRANS_TO_RUN, TRANS_TO_BLOCK, TRANS_TO_PREEMPT, TRANS_TO_FINISHED};
enum proc_state {CREATED, READY, RUNNING, BLOCKED, FINISHED};
const char *state_types[] = {"CREATED", "READY", "RUNNING", "BLOCKED", "FINISHED"};
const char *trans_types[] = {"TRANS_TO_READY", "TRANS_TO_RUN", "TRANS_TO_BLOCK", "TRANS_TO_PREEMPT", "Done"};


int myrandom(int burst) { return 1 + (rand_arr[(rand_ofs++) % rand_arr.size()] % burst); }

class Process {
    public:
        int pid;
        int static_prio;
        int dynamic_prio;
        // input data
        int AT;
        int TC;
        int CB;
        int IO;
        
        proc_state state;
        int FT; // finishing time 
        int TT; // turnaround time
        
        int IT; // I/O time
        int CW; // CPU waiting time
        
        int remaining_CPU;
        int remaining_CPU_burst;
        int state_ts; // state start time
        
        Process(int pid, int AT, int TC, int CB, int IO) {
            this->static_prio = myrandom(maxprio);
            this->state_ts = AT;
            this->pid = pid;
            this->AT = AT;
            this->TC = TC;
            this->CB = CB;
            this->IO = IO;
            this->state = CREATED;
            this->remaining_CPU = TC; // zero means finished
            this->remaining_CPU_burst = 0;
            this->IT = 0;
            this->CW = 0;
            this->TT = -1;
            this->FT = -1;
        }
        
        void print_proc() {
            printf("%04d: %4d %4d %4d %4d %1d | %5d %5d %5d %5d\n", this->pid, this->AT, this->TC, this->CB, this->IO, this->static_prio, this->FT,
                    this->TT, this->IT, this->CW);
        }
};


vector<Process*> proc_arr;

class Event {
    public:
        int Timestamp;
        Process *proc;
        proc_state oldstate;
        proc_state newstate;
        Transition transition;

        Event(int Timestamp, Process *proc, proc_state oldstate, proc_state newstate, Transition trans) {
            this->transition = trans;
            this->Timestamp = Timestamp;
            this->proc = proc;
            this->oldstate = oldstate;
            this->newstate = newstate;
        }
};

class DES {
    vector<Event> event_queue;
    public:
        Event* get_event() {
            if (event_queue.size() == 0) {
                return NULL;
            }
            Event *evt_ptr = &event_queue[0];
            return evt_ptr;
        }

        void rm_event() {
            event_queue.erase(event_queue.begin());
        }

        void put_event(Event e) {
            for (int i = 0; i < event_queue.size(); i++) {
                if (event_queue[i].Timestamp > e.Timestamp) {
                    event_queue.insert(event_queue.begin() + i, e);
                    // std::cout << "inserted" << endl;
                    return;
                }
            }
            event_queue.push_back(e);
        }

        int get_next_event_time() {
            if(event_queue.empty())
            {
                return -1;
            }
            
            return event_queue.front().Timestamp;
        }
        
};

class Scheduler {
    public:
        virtual void add_process(Process *proc) = 0;
        virtual Process* get_next_process() = 0;
        
};

class FCFS: public Scheduler {
    vector<Process*> run_queue;
    public: 
        void add_process(Process *proc) {
            run_queue.push_back(proc);
        }

        Process* get_next_process() {
            Process* proc_ptr;
            if (run_queue.size() == 0) {
                return NULL;
            }
            proc_ptr = run_queue.front();
            run_queue.erase(run_queue.begin());
            return proc_ptr;
        }
};

void simulation(DES des, Scheduler* scheduler_ptr) {
    Event* evt;
    Process *CURRENT_RUNNING_PROCESS = NULL;
    while ((evt = des.get_event())) {
        Process *proc = evt->proc;
        int CURRENT_TIME = evt->Timestamp;
        int timeInPrevState = CURRENT_TIME - proc->state_ts;
        proc->state_ts = CURRENT_TIME;
        bool CALL_SCHEDULER = false;
        // all transitions: {TRANS_TO_READY, TRANS_TO_RUN, TRANS_TO_BLOCK, TRANS_TO_PREEMPT};
        // std::cout << trans_types[evt->transition] << endl;
        // std::cout << "Current time: " << CURRENT_TIME << endl;
        // std::cout << "Time in previous state: " << timeInPrevState << endl;
        // proc->print_proc();
        // std::cout << endl;
        if (v_flag) {
            std::cout << CURRENT_TIME << " " << proc->pid << " " << timeInPrevState << ": " << trans_types[evt->transition] <<  endl;;
        }


        switch(evt->transition) {
            case TRANS_TO_READY:
                {
                    CALL_SCHEDULER = true;
                    // add to run queue
                    // will be called by scheduler in a later time
                    scheduler_ptr->add_process(proc);
                    // evt->newstate = READY;
                    

                    

                    
                }
                break;
                


            case TRANS_TO_RUN:
                {
                    // evt->newstate = RUNNING;
                    // running process assignment
                    CURRENT_RUNNING_PROCESS = proc;
                    if (CURRENT_RUNNING_PROCESS == NULL) {
                        std::cout << "buggy current running process" << endl;
                    }
                    int burst_rest = CURRENT_RUNNING_PROCESS->remaining_CPU_burst;
                    // create event for either preemption or blocking for current running process
                    if (burst_rest <= 0) {
                        // generate new CPU burst
                        burst_rest = myrandom(proc->CB);
                    }

                    burst_rest = std::min(burst_rest, proc->remaining_CPU);
                    
                    proc->CW += timeInPrevState; // keep track of cpu waiting time


                    // preemptive event created
                    // quantum expiration
                    if (burst_rest > quantum) {
                        proc->remaining_CPU -= quantum;
                        proc->remaining_CPU_burst = burst_rest - quantum;
                        proc->dynamic_prio = (proc->dynamic_prio < 0) ? (proc->static_prio - 1) : (proc->dynamic_prio - 1);
                        Event preempt_evt(CURRENT_TIME + quantum, proc, RUNNING, READY, TRANS_TO_PREEMPT);
                        des.put_event(preempt_evt);
                    }
                    // blocked event created
                    else if (proc->remaining_CPU > burst_rest) {
                        proc->remaining_CPU -= burst_rest;
                        proc->remaining_CPU_burst = 0;
                        
                        Event block_evt(CURRENT_TIME + burst_rest, proc, RUNNING, BLOCKED, TRANS_TO_BLOCK);
                        burst_rest = 0;
                        des.put_event(block_evt);
                    } else { // proc->remaining_CPU == burst_rest
                        // process finished
                        proc->remaining_CPU = 0;
                        proc->remaining_CPU_burst = 0;
                        Event finish_evt(CURRENT_TIME + burst_rest, proc, RUNNING, FINISHED, TRANS_TO_FINISHED);
                        des.put_event(finish_evt);
                    }

                    
                }
                break;



            case TRANS_TO_BLOCK:
                {
                    // evt->newstate = BLOCKED;
                    CALL_SCHEDULER = true;
                    CURRENT_RUNNING_PROCESS = NULL;
                    int io_burst = myrandom(proc->IO);
                    proc->IT += io_burst;
                    // create event that transfer the process from blocked to ready
                    // All: When a process returns from I/O its dynamic priority is reset to (static_priority-1).
                    proc->dynamic_prio = proc->static_prio - 1;
                    Event rdy_evt(CURRENT_TIME + io_burst, proc, BLOCKED, READY, TRANS_TO_READY);
                    des.put_event(rdy_evt);
                    
                    
                }
                break;




            // case not for non-preemptive schedulers
            case TRANS_TO_PREEMPT:
                {
                    // evt->newstate = PREEMPT;
                    CALL_SCHEDULER = true;
                    CURRENT_RUNNING_PROCESS = NULL;
                    // add to runqueue (no event is generated)
                    scheduler_ptr->add_process(proc);
                    
                }
                break;


            case TRANS_TO_FINISHED:
                {
                    CALL_SCHEDULER = true;
                    CURRENT_RUNNING_PROCESS = NULL;
                    proc->FT = CURRENT_TIME;
                    proc->TT = CURRENT_TIME - proc->AT;
                    latest_FT = proc->FT; // keep track of the whole finish time
                    
                    
                }
                break;
        }
        des.rm_event();


        // schedule new events
        if (CALL_SCHEDULER) {
            // if the next timestamp is the same as the current one,
            // DON'T call scheduler. Instead, keep processing the events that have the same tiemstamp
            if (des.get_next_event_time() == CURRENT_TIME) {
                continue; // process next event from Event queue for the same timestamp
            }
            CALL_SCHEDULER = false;
            // std::cout << "nullptr condition check: " << (CURRENT_RUNNING_PROCESS == NULL) << endl;
            if (CURRENT_RUNNING_PROCESS == NULL) {
                CURRENT_RUNNING_PROCESS = scheduler_ptr->get_next_process();
                if (CURRENT_RUNNING_PROCESS == NULL) {
                    continue;
                }
                // create event to make this process runnable for same time.
                Event e(CURRENT_TIME, CURRENT_RUNNING_PROCESS, READY, RUNNING, TRANS_TO_RUN);
                des.put_event(e);
                CURRENT_RUNNING_PROCESS = NULL;
            }
        }

        
    }
}















// read options
int main(int argc, char *argv[]) {
    int opt;
    const char *optstring = "vteps:";
    int count = 0;
    char first;
    Scheduler *scheduler_ptr;
    while ((opt = getopt(argc, argv, optstring)) != -1) {
        switch(opt) {
            case 'v': 
            v_flag = true;
            break;

            case 't': 
            t_flag = true;
            break;

            case 'e': 
            e_flag = true;
            break;

            case 'p': 
            p_flag = true;
            break;

            case 's': 
            // std::cout << "found s opt" << endl;
            if (optarg != NULL) {
                // std::cout << optarg << endl;
                switch(optarg[0]) {
                    case 'F':
                    scheduler_ptr = new FCFS();
                    sche_name = "FCFS";
                    break;
                    
                    case 'L':
                    sche_name = "LCFS";
                    break;

                    case 'S':
                    sche_name = "SRTF";
                    break;

                    case 'R':
                    sche_name = "Round";
                    sscanf(optarg, "%c%d", &first, &quantum);
                    break;

                    case 'P':
                    sche_name = "PRIO";
                    sscanf(optarg, "%c%d:%d",&first, &quantum, &maxprio);
                    break;

                    case 'E':
                    sche_name = "PREEMP";
                    sscanf(optarg, "%c%d:%d", &first, &quantum, &maxprio);
                    break;

                    default: 
                    std::cout << "invalid scheduler name" << endl;
                    return 1;
                    break;
                }
            }
            break;

            case '?': 
            if (optopt == 's') {
                std::cout << "a scheduler name is required" << endl;
                return 1;
            }
            break;

            default:
            break;
        }
    }

    const char *input_file_name = argv[optind];
    const char *rand_file_name = argv[optind+1];
    // std::cout << v_flag << endl;
    // std::cout << t_flag << endl;
    // std::cout << e_flag << endl;
    // std::cout << p_flag << endl;
    std::cout << "scheduler name: " << sche_name << endl;
    std::cout << "quantum: " << quantum << endl;
    std::cout << "max priority: " << maxprio << endl;
    std::cout << endl;
    // std::cout << input_file_name << endl;
    // std::cout << rand_file_name << endl;

    randFile.open(rand_file_name);
    string word;
    while(randFile >> word) {
        rand_arr.push_back(stoi(word));
    }
    randFile.close();

    inputFile.open(input_file_name);
    int id_count = 0;
    while(inputFile.peek() != EOF) {
        // craete a process object
        string line_str;
        getline(inputFile, line_str);
        istringstream line(line_str);
        vector<int> input_params;
        for (int i = 0; i < 4; i++) {
            string temp;
            line >> temp;
            // cout << temp << endl;
            input_params.push_back(stoi(temp));
            // cout << input_params.size() << endl;
        }
        Process *proc = new Process(id_count++, input_params[0], input_params[1], input_params[2], input_params[3]);
        proc_arr.push_back(proc);
        // cout << proc_q.size() << endl;
    }
    inputFile.close();

    // cout << proc_arr.size() << endl;
    DES des;
    for (int i = 0; i < proc_arr.size(); i++) {
        Process *Proc_ptr = proc_arr[i];
        proc_state pstate = READY;
        Transition trans = TRANS_TO_READY;
        Event e(Proc_ptr->AT, Proc_ptr, Proc_ptr->state, pstate, trans);
        des.put_event(e);
    }

    // Event * current_event_ptr;
    // count = 0;
    // while ((current_event_ptr = des.get_event())) {
    //     std::cout << "Time stamp: " << current_event_ptr->Timestamp << endl;
    //     (current_event_ptr->proc)->print_proc();
    //     std::cout << "old state: " << state_types[current_event_ptr->oldstate] << endl;
    //     std::cout << "new state: " << state_types[current_event_ptr->newstate] << endl;
    //     std::cout << endl;
    //     des.rm_event();
    // }

    // std::cout << "DES Finished!" << endl;

    simulation(des, scheduler_ptr);


    std::cout << sche_name << endl;
    // print processes stats
    for (int i = 0; i < proc_arr.size(); i++) {
        proc_arr[i]->print_proc();
    }
    
    // print summary 
    
    

    return 0;
}