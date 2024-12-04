#include <iostream>
#include <fstream>
#include <cstring>
#include <pthread.h>
#include <queue>
#include <unordered_map>
#include <set>
#include <algorithm>

using namespace std;

// Input for map threads.
struct mapper_input {
    int thread_id;

    // Counter which tracks the current position
    // inside the queue of files.
    int *counter;

    // The queue of files.
    queue<string> *file_queue;

    // Vector of partial lists obtained.
    vector<unordered_map<string, set<int>>> *partial_list;

    // Sync data.
    pthread_mutex_t *mutex;
    pthread_barrier_t *barrier;
};

// Input for reducer threads.
struct reducer_input {
    int thread_id;

    // Counter which indicates the next letter that will
    // be processed.
    char *counter;

    // Vector of partial lists received from mappers.
    vector<unordered_map<string, set<int>>> *partial_list;

    // Sync data.
    pthread_mutex_t *mutex;
    pthread_barrier_t *barrier;
};

bool comparator(pair<string, set<int>>& pair1, pair<string, set<int>>& pair2) {
    // Comparator for sorting the data that will be sent through reducer
    // threads.

    // Check num of appearances.
    if(pair1.second.size() != pair2.second.size()) {
        return pair1.second.size() > pair2.second.size();
    }

    // If the num of appearances is equal, check the words.
    return pair1.first < pair2.first;
}

void *mapper_function(void *arg)
{
    // Function for map threads.
	struct mapper_input thread_info = *(struct mapper_input *)arg;

    ifstream data_file;
    string word;
    string actual_word;

    // Keep track of the thread's current counter.
    int internal_counter;

    // While there's a file inside the queue, extract it.
    while(1) {
        pthread_mutex_lock(thread_info.mutex);

        if(!thread_info.file_queue->empty()) {
            data_file.open(thread_info.file_queue->front());
            thread_info.file_queue->pop();
            internal_counter = *thread_info.counter;
            (*thread_info.counter)++;
        } else {
            pthread_mutex_unlock(thread_info.mutex);
            break;
        }

        pthread_mutex_unlock(thread_info.mutex);

        // Extract the words inside each text file.
        while (data_file >> word) {
            for(unsigned int i = 0; i < word.size(); i++) {
                // Check for capital letters.
                if(word[i] >= 'A' && word[i] <= 'Z') {
                    actual_word.push_back(word[i] + 'a' - 'A');
                } else if(word[i] >= 'a' && word[i] <= 'z') {
                    // Keep only the characters which are valid.
                    actual_word.push_back(word[i]);
                }
            }
            (*thread_info.partial_list)[thread_info.thread_id][actual_word].insert(internal_counter);
            actual_word.clear();
        }

        // Close the file.
        data_file.close();
    }

    // Wait for every mapper here.
    pthread_barrier_wait(thread_info.barrier);

	pthread_exit(NULL);
}

void *reducer_function(void *arg)
{
    // Function for Reducer threads.
	struct reducer_input thread_info = *(struct reducer_input *)arg;

    // Wait for mappers to finish their work.
    pthread_barrier_wait(thread_info.barrier);

    // Map which stores & puts together the data that will be printed.
    unordered_map<string, set<int>> aggr_map;

    // Vector used for printing data inside output files.
    vector<pair<string, set<int>>> aggr_list;

    // Internal counter for the reducer thread.
    char internal_counter;
    
    // Output file.
    ofstream data_file;

    // Name of file.
    string filename;

    // For each char from a to z, create files and insert the corresponding words
    // inside these files.
    while(1) {
        pthread_mutex_lock(thread_info.mutex);

        if(*thread_info.counter <= 'z') {
            internal_counter = *thread_info.counter;
            (*thread_info.counter)++;
        } else {
            pthread_mutex_unlock(thread_info.mutex);
            break;
        }

        pthread_mutex_unlock(thread_info.mutex);

        // Extract all the words that start with the char saved in internal_counter.
        for(unsigned int i = 0; i < thread_info.partial_list->size(); i++) {
            for(auto it : (*thread_info.partial_list)[i]) {
                if(it.first[0] == internal_counter) {
                    if(aggr_map.find(it.first) == aggr_map.end()) {
                        aggr_map[it.first] = it.second;
                    } else {
                        aggr_map[it.first].insert(it.second.begin(), it.second.end());
                    }
                }
            }
        }

        // Copy the mapped values.
        for(auto it : aggr_map) {
            aggr_list.push_back({it.first, it.second});
        }


        // Sort the values.
        sort(aggr_list.begin(), aggr_list.end(), comparator);

        // Open the data file and insert the sorted values.
        filename = ".txt";
        filename = internal_counter + filename;
        data_file.open(filename);

        for(unsigned int i = 0; i < aggr_list.size(); i++) {
            // Write data.
            data_file << aggr_list[i].first << ":[";
            for(auto it = aggr_list[i].second.begin(); it != aggr_list[i].second.end(); it++) {
                data_file << *it;
                if(next(it) != aggr_list[i].second.end()) {
                    data_file << " ";
                }
            }
            data_file << "]" << endl;
        }

        aggr_map.clear();
        aggr_list.clear();
        data_file.close();
    }

	pthread_exit(NULL);
}

void extract_data(char *main_file, queue<string>& file_queue) {
    // Extract the name of the files from the entry file.
    ifstream entry_file;
    string filename;
    int n;

    entry_file.open(main_file);
    entry_file >> n;
    for(int i = 0; i < n; i++) {
        entry_file >> filename;
        file_queue.push(filename);
    }

    entry_file.close();
}

int main(int argc, char **argv)
{
    // Initialize data.
    int M = atoi(argv[1]), R = atoi(argv[2]);
    pthread_t tid[M + R];

    // Data for the files.
    queue<string> file_queue;

    // Sync data.
    pthread_mutex_t mutex;
    pthread_barrier_t barrier;

    // Structures for mapper threads arguments and reducer threads arguments.
    vector<struct mapper_input> mapin(M);
    struct reducer_input redin;

    // Partial lists resulted from the MAP threads.
    vector<unordered_map<string, set<int>>> partial_list(M);

    // Extract the files from the entry file.
    extract_data(argv[3], file_queue);

    // Counter for mapper threads.
    int map_counter = 1;

    // Counter for reducer threads.
    char reducer_counter = 'a';

    // Initialize sync data.
    pthread_mutex_init(&mutex, NULL);
    pthread_barrier_init(&barrier, NULL, M + R);

    // Initialize reducer structure.
    redin.barrier = &barrier;
    redin.mutex = &mutex;
    redin.partial_list = &partial_list;
    redin.counter = &reducer_counter;

    // Create the threads.
    for (int i = 0; i < M + R; i++) {
        if(i < M) {
            // Initialize mapper argument.
            mapin[i].thread_id = i;
            mapin[i].file_queue = &file_queue;
            mapin[i].barrier = &barrier;
            mapin[i].mutex = &mutex;
            mapin[i].partial_list = &partial_list;
            mapin[i].counter = &map_counter;

            pthread_create(&tid[i], NULL, mapper_function, &mapin[i]);
        } else {
            pthread_create(&tid[i], NULL, reducer_function, &redin);
        }
	}

    // Join threads.
    for (int i = 0; i < M + R; i++) {
		pthread_join(tid[i], NULL);
	}

    // Destroy sync data.
    pthread_mutex_destroy(&mutex);
    pthread_barrier_destroy(&barrier);

    return 0;
}