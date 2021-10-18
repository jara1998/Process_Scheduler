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
int rand_ofs = 0;


enum proc_state {CREATED, READY, RUNNING, BLOCKED, FINISHED};
const char *state_types[] = {"CREATED", "READY", "RUNNING", "BLOCKED", "FINISHED"};


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
        
        Process(int pid, int AT, int TC, int CB, int IO) {
            this->static_prio = myrandom(maxprio);
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

        Event(int Timestamp, Process *proc, proc_state oldstate, proc_state newstate) {
            this->Timestamp = Timestamp;
            this->proc = proc;
            this->oldstate = oldstate;
            this->newstate = newstate;
        }
};

class DES {
    vector<Event*> event_queue;
    public:
        Event* get_event() {
            if (event_queue.size() == 0) {
                return NULL;
            }
            return event_queue[0];
        }

        void rm_event() {
            delete event_queue[0];
            event_queue.erase(event_queue.begin());
        }

        void put_event(Event *e) {
            for (int i = 0; i < event_queue.size(); i++) {
                if (event_queue[i]->Timestamp > e->Timestamp) {
                    event_queue.insert(event_queue.begin() + i, e);
                    // std::cout << "inserted" << endl;
                    return;
                }
            }
            event_queue.push_back(e);
        }
        
};



// read options
int main(int argc, char *argv[]) {
    int opt;
    const char *optstring = "vteps:";
    int count = 0;
    char first;
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
    std::cout << endl << endl;
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
        Event * e_ptr = new Event(Proc_ptr->AT, Proc_ptr, Proc_ptr->state, pstate);
        des.put_event(e_ptr);
    }

    Event * current_event_ptr;
    count = 0;
    while ((current_event_ptr = des.get_event())) {
        std::cout << "Time stamp: " << current_event_ptr->Timestamp << endl;
        (current_event_ptr->proc)->print_proc();
        std::cout << "old state: " << state_types[current_event_ptr->oldstate] << endl;
        std::cout << "new state: " << state_types[current_event_ptr->newstate] << endl;
        std::cout << endl;
        des.rm_event();
    }

    // std::cout << "DES Finished!" << endl;
    

    

    return 0;
}