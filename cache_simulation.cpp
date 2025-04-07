//sri rama jayam
#include <iostream>
#include <list>
#include <vector>
#include <unordered_map>
#include <deque>
#include <cstdlib> // For rand() and srand()
#include <ctime>   // For time()
using namespace std;

int mainMemory[1024] = {0}; // Initialize all to 0

struct CacheBlock {
    int address;
    int tag;
    int data;
    bool valid_bit;
    bool dirty_bit;
};

class LRUCache {
public:
    int capacity;
    list<CacheBlock> blocks;
    unordered_map<int, list<CacheBlock>::iterator> cacheMap;
    vector<CacheBlock> writeBuffer;
    deque<CacheBlock> victimCache;
    
    // Statistics tracking
    int totalAccesses = 0;
    int hits = 0;
    int misses = 0;
    
    LRUCache(int capacity) {
        this->capacity = capacity;
    }
    
    void writeToMainMemory() {
        for (auto &block : writeBuffer) {
            mainMemory[block.address] = block.data;
        }
        writeBuffer.clear();
    }
    
    void evictBlock(list<CacheBlock>::iterator block) {
        if (block->dirty_bit) {
            writeBuffer.push_back(*block);
            if (writeBuffer.size() == 4) writeToMainMemory();
        }
        
        victimCache.push_front(*block);
        
        if(victimCache.size() > 4) {
            victimCache.pop_back();
        }
        
        cacheMap.erase(block->address % capacity);
        blocks.erase(block);
    }
    
    int get(int address) {
        totalAccesses++;
        
        int cacheIndex = address % capacity;
        int tag = address / capacity;
        
        if (cacheMap.find(cacheIndex) != cacheMap.end()) {
            auto it = cacheMap[cacheIndex];
            if (it->valid_bit && it->tag == tag) {
                hits++;
                int data = it->data;
                blocks.splice(blocks.begin(), blocks, it);  
                return data;
            } else {
                evictBlock(it);
            }
        }
        
        misses++;
        
        // Check victim cache
        for(int i = 0; i < victimCache.size(); i++) {
            if(victimCache[i].tag == tag && (victimCache[i].address % capacity) == cacheIndex) {
                int data = victimCache[i].data;
                bool is_dirty = victimCache[i].dirty_bit;
                
                blocks.push_front({address, tag, data, true, is_dirty});
                cacheMap[cacheIndex] = blocks.begin();
                
                victimCache.erase(victimCache.begin() + i);
                
                return data;
            }
        }
        
        // Check write buffer
        for (int i = 0; i < writeBuffer.size(); i++) {
            if (writeBuffer[i].tag == tag && (writeBuffer[i].address % capacity) == cacheIndex) {
                int data = writeBuffer[i].data;
                
                blocks.push_front({address, tag, data, true, true});
                cacheMap[cacheIndex] = blocks.begin();
                
                auto temp = writeBuffer[i].data;
                writeBuffer.erase(writeBuffer.begin() + i);
                
                return temp;
            }
        }
        
        int data = mainMemory[address];
        
        if (blocks.size() >= capacity) {
            evictBlock(--blocks.end());
        }
        
        blocks.push_front({address, tag, data, true, false});
        cacheMap[cacheIndex] = blocks.begin();
        
        return data;
    }
    
    void put(int address, int data) {
        totalAccesses++;
        
        int cacheIndex = address % capacity;
        int tag = address / capacity;
        
        if (cacheMap.find(cacheIndex) != cacheMap.end()) {
            auto block = cacheMap[cacheIndex];
            if (block->valid_bit) {
                if (block->tag == tag) {
                    hits++;
                    block->data = data;
                    block->dirty_bit = true;
                    blocks.splice(blocks.begin(), blocks, block);
                    return;
                } else {
                    misses++;
                    evictBlock(block);
                }
            }
        } else {
            misses++;
        }
        
        if (blocks.size() >= capacity) {
            evictBlock(--blocks.end());
        }
        
        blocks.push_front({address, tag, data, true, true});
        cacheMap[cacheIndex] = blocks.begin();
    }
    
    void printStats() {
        cout << "\n=== L2 Cache Statistics ===" << endl;
        cout << "Total memory accesses: " << totalAccesses << endl;
        cout << "Cache hits: " << hits << " (" << (totalAccesses > 0 ? (hits * 100.0 / totalAccesses) : 0) << "%)" << endl;
        cout << "Cache misses: " << misses << " (" << (totalAccesses > 0 ? (misses * 100.0 / totalAccesses) : 0) << "%)" << endl;
        cout << "Verification: " << hits << " + " << misses << " = " << (hits + misses) << " (should equal " << totalAccesses << ")" << endl;
    }
};

struct L1CacheBlock {
    int tag;
    int data;
    bool valid;
    bool dirty;
};

class L1Cache {
public:
    int size;                      
    vector<L1CacheBlock> blocks;   
    LRUCache* l2Cache;
    
    // Statistics tracking
    int totalAccesses = 0;
    int hits = 0;
    int misses = 0;
    
    L1Cache(int size, LRUCache* l2) {
        this->size = size;
        this->l2Cache = l2;
        // Initialize cache blocks with invalid data
        blocks.resize(size, {0, 0, false, false});
    }
    
    int get(int address) {
        totalAccesses++;
        
        int index = address % size;
        int tag = address / size;
        
        // Cache hit
        if (blocks[index].valid && blocks[index].tag == tag) {
            hits++;
            return blocks[index].data;
        }
        
        // L1 Cache miss
        misses++;

        // If block is dirty, write back to L2 cache
        if (blocks[index].valid && blocks[index].dirty) {
            int oldAddress = (blocks[index].tag * size) + index;
            l2Cache->put(oldAddress, blocks[index].data);
        }
        
        // Fetch from L2 cache
        int data = l2Cache->get(address);
        
        // Update L1 cache block
        blocks[index] = {tag, data, true, false};
        
        return data;
    }
    
    void put(int address, int data) {
        totalAccesses++;
        
        int index = address % size;
        int tag = address / size;
        
        if (blocks[index].valid && blocks[index].tag == tag) {
            hits++;
            blocks[index].data = data;
            blocks[index].dirty = true;  
        } 
        else {
            misses++;

            if (blocks[index].valid && blocks[index].dirty) {
                int oldAddress = (blocks[index].tag * size) + index;
                l2Cache->put(oldAddress, blocks[index].data);
            }
            
            blocks[index] = {tag, data, true, true};
        }
    }
    
    void flush() {
        for (int i = 0; i < size; i++) {
            if (blocks[i].valid && blocks[i].dirty) {
                int address = (blocks[i].tag * size) + i;
                l2Cache->put(address, blocks[i].data);
                blocks[i].dirty = false;
            }
        }
    }
    
    void printStats() {
        cout << "\n=== L1 Cache Statistics ===" << endl;
        cout << "Total memory accesses: " << totalAccesses << endl;
        cout << "Cache hits: " << hits << " (" << (totalAccesses > 0 ? (hits * 100.0 / totalAccesses) : 0) << "%)" << endl;
        cout << "Cache misses: " << misses << " (" << (totalAccesses > 0 ? (misses * 100.0 / totalAccesses) : 0) << "%)" << endl;
        cout << "Verification: " << hits << " + " << misses << " = " << (hits + misses) << " (should equal " << totalAccesses << ")" << endl;
    }
};

void printCombinedStats(L1Cache& l1, LRUCache& l2) {
    cout << "\n=== Combined Cache Statistics ===" << endl;
    
    // Total CPU accesses (L1 accesses)
    int totalCPUAccesses = l1.totalAccesses;
    
    // L1 statistics
    double l1HitRate = (double)l1.hits / totalCPUAccesses * 100.0;
    cout << "L1 Cache: " << l1.hits << " hits, " << l1.misses << " misses" << endl;
    cout << "L1 Hit Rate: " << l1HitRate << "%" << endl;
    
    // L2 statistics (L2 accesses come from L1 misses)
    double l2HitRate = (l1.misses > 0) ? (double)l2.hits / l1.misses * 100.0 : 0.0;
    cout << "L2 Cache: " << l2.hits << " hits, " << l2.misses << " misses" << endl;
    cout << "L2 Hit Rate (hits/L1 misses): " << l2HitRate << "%" << endl;
    
    // Calculate global hit rate
    // Global hit rate = L1 hit rate + (L1 miss rate * L2 hit rate)
    double l1MissRate = 100.0 - l1HitRate;
    double globalHitRate = l1HitRate + (l1MissRate * l2HitRate / 100.0);
    double globalMissRate = 100.0 - globalHitRate;
    
    cout << "Global Hit Rate: " << globalHitRate << "%" << endl;
    cout << "Global Miss Rate: " << globalMissRate << "%" << endl;
    cout << "Hit Rate Verification: " << globalHitRate + globalMissRate << "% (should equal 100%)" << endl;
}

void performRandomAccesses(L1Cache& l1Cache, int numAccesses) {
    cout << "\n=== Beginning " << numAccesses << " Random Memory Accesses ===" << endl;
    
    for (int i = 0; i < numAccesses; i++) {
        int address = rand() % 1024;  // Random address in range [0, 1023]
        
        // 70% reads, 30% writes
        if (rand() % 100 < 70) {
            // Read operation
            l1Cache.get(address);
        } else {
            // Write operation
            int value = rand() % 10000;  // Random values [0, 9999]
            l1Cache.put(address, value);
        }
        
        // Optional: Print progress
        if (i % 100 == 0 && i > 0) {
            cout << "Completed " << i << " accesses..." << endl;
        }
    }
    
    cout << "Random access test completed." << endl;
}

int main() {
    // Seed the random number generator
    srand(time(0));
    
    // Initialize some values in main memory
    for (int i = 0; i < 1024; i++) {
        mainMemory[i] = i * 10;
    }
    
    // Create L2 cache with 64 blocks
    LRUCache l2Cache(64);
    
    // Create L1 cache with 8 blocks, connected to L2 cache
    L1Cache l1Cache(8, &l2Cache);
    
    cout << "=== Multi-level Cache Simulation ===" << endl;
    
    // Perform random memory accesses through L1 cache
    performRandomAccesses(l1Cache, 500);
    
    // Print individual statistics
    l1Cache.printStats();
    l2Cache.printStats();
    
    // Print combined statistics
    printCombinedStats(l1Cache, l2Cache);
    
    // Flush L1 cache to L2
    l1Cache.flush();
    
    // Final statistics after flush
    cout << "\n=== Statistics after flush ===" << endl;
    l1Cache.printStats();
    l2Cache.printStats();
    printCombinedStats(l1Cache, l2Cache);
    
    return 0;
}
