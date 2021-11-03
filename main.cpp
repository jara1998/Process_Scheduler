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
int time_cpubusy;
int time_iobusy;
int blk_cnt = 0;
int last_record_time = 0;
bool show_quantum = false;
bool preemptive = false;


enum Transition {TRANS_TO_READY, TRANS_TO_RUN, TRANS_TO_BLOCK, TRANS_TO_PREEMPT, TRANS_TO_FINISHED};
enum proc_state {CREATED, READY, RUNNING, BLOCKED, FINISHED};
const char *state_types[] = {"CREATED", "READY", "RUNNING", "BLOCKED", "FINISHED"};
const char *trans_types[] = {"TRANS_TO_READY", "TRANS_TO_RUN", "TRANS_TO_BLOCK", "TRANS_TO_PREEMPT", "Done"};


int myrandom(int burst) { 
    return 1 + (rand_arr[(rand_ofs++) % rand_arr.size()] % burst); 
    }

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
            this->dynamic_prio = this->static_prio - 1;
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
    list<Event> event_queue;
    public:
        Event* get_event() {
            if (event_queue.size() == 0) {
                return NULL;
            }
            Event *evt_ptr = &(event_queue.front());
            return evt_ptr;
        }

        void rm_event() {
            event_queue.erase(event_queue.begin());
        }

        void put_event(Event e) {
            for (auto i = event_queue.begin(); i != event_queue.end(); i++) {
                if ((*i).Timestamp > e.Timestamp) {
                    event_queue.insert(i, e);
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

        bool delete_future_event(Process *proc_ptr, int time_stamp) {
            for (auto i = event_queue.begin(); i != event_queue.end(); i++) {
                if ((*i).Timestamp > time_stamp && (*i).proc->pid == proc_ptr->pid) {
                    event_queue.erase(i);
                    // std::cout << "future event deleted" << endl;
                    return true;
                }
            }
            // std::cout << "no event deleted" << endl;
            return false;
        }

        void print_des() {
            for (auto i = event_queue.begin(); i != event_queue.end(); i++) {
                std::cout << "[Time: "<< (*i).Timestamp << ", id: " << (*i).proc->pid << ", Transition: " << trans_types[(*i).transition] << "]  ";
            }
            std::cout << endl;
        }
        
};

class Scheduler {
    public:
        virtual void add_process(Process *proc) = 0;
        virtual Process* get_next_process() = 0;
        
};

class FCFS: public Scheduler {
    list<Process*> run_queue;
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

class LCFS: public Scheduler {
    list<Process*> run_queue;
    public: 
        void add_process(Process *proc) {
            run_queue.push_back(proc);
        }

        Process* get_next_process() {
            Process* proc_ptr;
            if (run_queue.size() == 0) {
                return NULL;
            }
            proc_ptr = run_queue.back();
            run_queue.pop_back();
            return proc_ptr;
        }
};

class SRTF: public Scheduler {
    list<Process*> run_queue;
    public: 
        void add_process(Process *proc) {
            for (auto it = run_queue.begin(); it != run_queue.end(); it++) {
                if (proc->remaining_CPU < (*it)->remaining_CPU) {
                    run_queue.insert(it, proc);
                    return;
                }
            }
            run_queue.push_back(proc);
        }


        Process* get_next_process() {
            Process* proc_ptr;
            if (run_queue.size() == 0) {
                return NULL;
            }
            proc_ptr = run_queue.front();
            run_queue.pop_front();
            return proc_ptr;
        }
};

class RR: public Scheduler {
    list<Process*> run_queue;
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


class PRIO: public Scheduler {
    vector<list<Process*>> *activeQ;
    vector<list<Process*>> *expiredQ;

    vector<list<Process*>> q1;
    vector<list<Process*>> q2;
    public: 
        PRIO() {
            for (int i = 0; i < maxprio; i++) {
                q1.push_back(list<Process*>());
                q2.push_back(list<Process*>());
            }
            activeQ = &q1;
            expiredQ = &q2;
        }

        bool activeQ_empty() {
            for (int i = 0; i < activeQ->size(); i++) {
                if (activeQ->at(i).size() != 0) {
                    return false;
                }
            }
            return true;
        }

        
        void add_process(Process *proc) {
            if (proc->dynamic_prio == -1) {
                
                proc->dynamic_prio = proc->static_prio - 1;
                expiredQ->at(proc->dynamic_prio).push_back(proc);
            } else {
                activeQ->at(proc->dynamic_prio).push_back(proc);
            }
        }

        Process* get_next_process() {
            Process* proc_ptr;
            if (activeQ_empty()) {
                std::swap(activeQ, expiredQ);
                if (activeQ_empty()) {
                    return NULL;
                }
            }

            for (int i = activeQ->size() - 1; i >= 0; i--) {
                if (activeQ->at(i).size() != 0) {
                    proc_ptr = activeQ->at(i).front();
                    activeQ->at(i).pop_front();
                    break;
                }
            }
            return proc_ptr;
        }
};


void update_stats(bool is_running, int curr_time) {
    if (is_running) {
        time_cpubusy += (curr_time - last_record_time);
    }
    if (blk_cnt > 0) {
        time_iobusy += (curr_time - last_record_time);
    }
    last_record_time = curr_time;
}

void simulation(DES des, Scheduler* scheduler_ptr) {
    Event* evt;
    Process *CURRENT_RUNNING_PROCESS = NULL;

    
    while ((evt = des.get_event())) {
        Process *proc = evt->proc;
        int CURRENT_TIME = evt->Timestamp;
        int timeInPrevState = CURRENT_TIME - proc->state_ts;
        proc->state_ts = CURRENT_TIME;
        bool CALL_SCHEDULER = false;


        if (v_flag) {
            std::cout << CURRENT_TIME << " " << proc->pid << " " << timeInPrevState << ": " << trans_types[evt->transition] << endl;
        }

        update_stats((CURRENT_RUNNING_PROCESS != NULL), CURRENT_TIME);

        switch(evt->transition) {
            case TRANS_TO_READY:
                {   
                    CALL_SCHEDULER = true;
                    // add to run queue
                    if (evt->oldstate == BLOCKED) {
                        blk_cnt--;
                        // When a process returns from I/O its dynamic priority is reset to (static_priority-1).
                        proc->dynamic_prio = proc->static_prio - 1;
                    }
                    // check preemption event
                    if (preemptive && CURRENT_RUNNING_PROCESS != NULL && evt->oldstate != RUNNING 
                            && proc->dynamic_prio > CURRENT_RUNNING_PROCESS->dynamic_prio 
                            && des.delete_future_event(CURRENT_RUNNING_PROCESS, CURRENT_TIME)) {

                                Event running_to_preemp(CURRENT_TIME, CURRENT_RUNNING_PROCESS, RUNNING, READY, TRANS_TO_PREEMPT);
                                des.put_event(running_to_preemp);

                    } 
                    
                    // will be called by scheduler in a later time
                    scheduler_ptr->add_process(proc);
                    // evt->newstate = READY;
                }
                break;
                


            case TRANS_TO_RUN:
                {
                    CURRENT_RUNNING_PROCESS = proc;
                    int burst_rest = CURRENT_RUNNING_PROCESS->remaining_CPU_burst;
                    // create event for either preemption or blocking for current running process
                    if (burst_rest <= 0) {
                        // generate new CPU burst
                        burst_rest = myrandom(proc->CB);
                    }

                    burst_rest = std::min(burst_rest, proc->remaining_CPU);
                    proc->CW += timeInPrevState; // keep track of cpu waiting time



                    if (burst_rest > quantum) {
                        proc->remaining_CPU_burst = burst_rest;
                        Event preempt_evt(CURRENT_TIME + quantum, proc, RUNNING, READY, TRANS_TO_PREEMPT);
                        des.put_event(preempt_evt);
                    }
                    // blocked event created
                    else if (proc->remaining_CPU > burst_rest) {
                        proc->remaining_CPU_burst = burst_rest;
                        Event block_evt(CURRENT_TIME + burst_rest, proc, RUNNING, BLOCKED, TRANS_TO_BLOCK);
                        burst_rest = 0;
                        des.put_event(block_evt);
                    } else { // proc->remaining_CPU == burst_rest
                        proc->remaining_CPU_burst = burst_rest;
                        Event finish_evt(CURRENT_TIME + burst_rest, proc, RUNNING, FINISHED, TRANS_TO_FINISHED);
                        des.put_event(finish_evt);
                    }
                    
                }
                break;



            case TRANS_TO_BLOCK:
                {
                    blk_cnt++;
                    // evt->newstate = BLOCKED;
                    CALL_SCHEDULER = true;
                    CURRENT_RUNNING_PROCESS = NULL;
                    int io_burst = myrandom(proc->IO);
                    proc->IT += io_burst;
                    proc->remaining_CPU -= timeInPrevState;
                    proc->remaining_CPU_burst = 0;
                    // create event that transfer the process from blocked to ready
                    // // All: When a process returns from I/O its dynamic priority is reset to (static_priority-1).
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
                    proc->remaining_CPU -= timeInPrevState;
                    proc->remaining_CPU_burst -= timeInPrevState;
                    // add to runqueue (no event is generated)
                    // std::cout << "preempt add process to ready started" << endl;
                    proc->dynamic_prio = proc->dynamic_prio - 1;
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
                    scheduler_ptr = new LCFS();
                    sche_name = "LCFS";
                    break;

                    case 'S':
                    scheduler_ptr = new SRTF();
                    sche_name = "SRTF";
                    break;

                    case 'R':
                    sscanf(optarg, "%c%d", &first, &quantum);
                    scheduler_ptr = new RR();
                    sche_name = "RR";
                    show_quantum = true;
                    
                    break;

                    case 'P':
                    sscanf(optarg, "%c%d:%d",&first, &quantum, &maxprio);
                    scheduler_ptr = new PRIO();
                    show_quantum = true;
                    sche_name = "PRIO";
                    
                    break;

                    case 'E':
                    sscanf(optarg, "%c%d:%d", &first, &quantum, &maxprio);
                    sche_name = "PREPRIO";
                    preemptive = true;
                    scheduler_ptr = new PRIO();
                    show_quantum = true;
                    
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


    simulation(des, scheduler_ptr);


    std::cout << sche_name;
    if (show_quantum) {
        std::cout << ' ' << quantum;
    }
    std::cout << endl;
    // print processes stats
    int total_tt = 0;
    int total_wait = 0;
    for (int i = 0; i < proc_arr.size(); i++) {
        proc_arr[i]->print_proc();
        total_tt += proc_arr[i]->TT;
        total_wait += proc_arr[i]->CW;
    }
    
    double cpu_util = 100.0 * (time_cpubusy / (double) latest_FT);
    double io_util = 100.0 * (time_iobusy / (double) latest_FT);
    double throughput = 100.0 * (proc_arr.size() / (double) latest_FT);
    double avg_tt = (((double) total_tt) / proc_arr.size());
    double avg_cpu_wait = (((double) total_wait)/ proc_arr.size());
    

    // print summary 
    printf("SUM: %d %.2lf %.2lf %.2lf %.2lf %.3lf\n", latest_FT, cpu_util, io_util, avg_tt, avg_cpu_wait, throughput);
    

    return 0;
}