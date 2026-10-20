#include <iostream>
#include <fstream>
#include <string>
#include <cmath>
#include <vector>
#include <cctype> // tolower
#include <sstream>
#include <unordered_map>
#include <algorithm> // sort
#include <unordered_set>


// loaded from stats.csv
long long passage_count;
double avg_doc_length;

struct block_meta{
    unsigned int last_docID; // largest doc id in block
    size_t doc_bytes; // bytes for docs in block
    size_t freq_bytes; // bytes for freq in block
};
std::vector<block_meta> block_metadatas;

struct lexicon_data{
    long long start; // byte offset in index.bin / where first block of term begins
    int doc_freq; // total documents for term
    int num_blocks; // num of 128 post blocks for term
    long long first_block; // index of term's first block
};

unsigned int varbyte_decode(const std::vector<unsigned char>& bytes, std::size_t& pos) {
    unsigned int result = 0;
    int shift = 0;
    unsigned char byte;
    do {
        byte = bytes[pos];
        unsigned int num = byte & 127; // take lower seven bits
        unsigned int num_shifted = num << shift; // move num to its shifted position
        result |= num_shifted; // combine shifted num with accumulated result
        shift += 7; // shift up seven bits for next byte read
        pos++; // move to next byte
    } while (byte & 128); // continue if continuation bit is 1
    return result; 
}

void decode_block(const std::vector<unsigned char>& bytes, const block_meta& meta, 
                    unsigned int prev_doc_id, std::vector<std::pair<unsigned int, unsigned int>>& out){
    out.clear(); // reset output
    
    unsigned int prev = prev_doc_id;
    std::size_t doc_pos = 0;
    std::size_t freq_pos = meta.doc_bytes;
    
    while (doc_pos < meta.doc_bytes) { // stop when reaching the freq block region
        unsigned int gap = varbyte_decode(bytes, doc_pos);
        prev += gap; // calculate actual doc id

        unsigned int freq = varbyte_decode(bytes, freq_pos);
        out.push_back({prev, freq});
    }
}

double BM25_ranking(int term_freq, int doc_length, long long doc_freq, long long N, double avg_doc_length){
    // term_freq = f_{d,t}, from posting
    // doc_length = |d|, from pagetable
    // doc_freq = f_t, from lexicon (numPostings)
    // N = passage count, from statistics
    // avg_doc_length = |d|_avg, from statistics

    float k1 = 1.2; // default frequency scaling term
    float b = 0.75; // default length normalization

    if ( term_freq <= 0 ) return 0.0;
    double inverse_doc_freq = std::log(( N - doc_freq + 0.5 ) / ( doc_freq + 0.5 ));
    double K = k1 * (( 1 - b ) + b * ( doc_length / avg_doc_length ));
    double scaled_term_freq = (( k1 + 1.0 ) * term_freq ) / ( K + term_freq );

    return inverse_doc_freq * scaled_term_freq;
}

class ListReader {
    public:
        ListReader(std::ifstream& index_stream_in, const lexicon_data& term_entry_in): 
            index_stream(index_stream_in), term_entry(term_entry_in),
            block_idx(0), pos_in_block(0), prev_block_last_docID(0), current_docID(0), current_freq(0),
            has_current(false), loaded_block_idx(-1), current_byte_offset(term_entry_in.start) {}

        bool seek(unsigned int target) { // move to first posting with docID >= target + return true if found
            // if already at or past target -> return true
            if (has_current && current_docID >= target) return true;
            
            while (block_idx < term_entry.num_blocks) { // linear scan over blocks
                const block_meta& m = block_metadatas[term_entry.first_block + block_idx];
                
                if (m.last_docID < target) { // skip block if its max docID is below the target
                    prev_block_last_docID = m.last_docID;
                    current_byte_offset += m.doc_bytes + m.freq_bytes;
                    block_idx++;
                    pos_in_block = 0;
                    loaded_block_idx = -1;
                    continue;
                }

                load_block(block_idx); 
                while (pos_in_block < current_block.size()) {
                    if (current_block[pos_in_block].first >= target) {
                        current_docID = current_block[pos_in_block].first;
                        current_freq = current_block[pos_in_block].second;
                        has_current = true;
                        return true;
                    }
                    pos_in_block++;
                }

                // no match found
                prev_block_last_docID = m.last_docID;
                current_byte_offset += m.doc_bytes + m.freq_bytes;
                block_idx++;
                pos_in_block = 0;
                loaded_block_idx = -1;
            }

            has_current = false;
            return false;
        }

        bool next() { // move to next posting
            if (!has_current) return false;

            pos_in_block++;
            if (pos_in_block < current_block.size()) {
                current_docID = current_block[pos_in_block].first;
                current_freq = current_block[pos_in_block].second;
                return true;
            }

            // move to next block.
            const block_meta& prev_m = block_metadatas[term_entry.first_block + block_idx];
            current_byte_offset += prev_m.doc_bytes + prev_m.freq_bytes;
            block_idx++;
            pos_in_block = 0;

            while (block_idx < term_entry.num_blocks) {
                const block_meta& m = block_metadatas[term_entry.first_block + block_idx];
                load_block(block_idx);
                if (pos_in_block < current_block.size()) {
                    current_docID = current_block[pos_in_block].first;
                    current_freq = current_block[pos_in_block].second;
                    return true;
                }
                prev_block_last_docID = m.last_docID;
                current_byte_offset += m.doc_bytes + m.freq_bytes;
                block_idx++;
                pos_in_block = 0;
                loaded_block_idx = -1;
            }
            has_current = false;
            return false;
        }

        unsigned int get_docID() const { return current_docID; }
        unsigned int get_freq() const { return current_freq; }
        bool is_valid() const { return has_current; }

    private:
        void load_block(long long b) {
            if (loaded_block_idx == b) return; // already loaded

            const block_meta& m = block_metadatas[term_entry.first_block + b];
            index_stream.clear();
            index_stream.seekg(current_byte_offset, std::ios::beg);
            std::vector<unsigned char> buf(m.doc_bytes + m.freq_bytes);
            index_stream.read(reinterpret_cast<char*>(&buf[0]), buf.size());
            decode_block(buf, m, prev_block_last_docID, current_block);
            pos_in_block = 0;
            loaded_block_idx = b;
        }

        std::ifstream& index_stream;
        lexicon_data term_entry;

        long long block_idx; // block index of term
        std::size_t pos_in_block; // index of next posting within current_block
        unsigned int prev_block_last_docID; // last docID of previous block, 0 if none
        unsigned int current_docID; // docID of current posting
        unsigned int current_freq; // freq of current posting
        bool has_current; // if current posting is valid

        std::vector<std::pair<unsigned int, unsigned int>> current_block; // decoded postings of loaded block
        long long loaded_block_idx; // block index currently decoded into current_block (-1 = none)
        long long current_byte_offset; // byte offset in index.bin of current block's start
};

void search_setup(const std::unordered_set<std::string>& terms,
                const std::unordered_map<std::string, lexicon_data>& lexicon, 
                std::vector<lexicon_data>& term_data,
                std::vector<ListReader>& readers,
                std::ifstream& idxFile){
    // get lexicon data for each term
    for (const auto& t : terms) {
        auto iterator = lexicon.find(t);
        if (iterator != lexicon.end()){
            term_data.push_back(iterator->second);
        }
    }

    // create a ListReader for each term.
    for (std::size_t i = 0; i < term_data.size(); i++){
        readers.push_back(ListReader(idxFile, term_data[i]));
    }
}

void print_results(std::vector<std::pair<int, double>>& results){
    // sort by score descending, then print top 10.
    std::sort(results.begin(), results.end(),
              [](const std::pair<int,double>& a, const std::pair<int,double>& b){
                  return a.second > b.second;
              });
              
    int count = 0;
    std::cout << "docID | score"  << std::endl;
    for (std::size_t i = 0; i < results.size() && count < 10; i++, count++)
        std::cout << results[i].first << " | " << results[i].second << std::endl;
    if (count == 0) std::cout << "no results" << std::endl;
}

void disjunctive_search(std::vector<lexicon_data>& term_data, std::vector<ListReader>& readers, const std::vector<int>& word_counts){
    // get the total BM25 for each docID
    std::unordered_map<int,double> total_score;
    for (std::size_t i = 0; i < readers.size(); i++) {
        if (!readers[i].seek(0)) continue;
        while (readers[i].is_valid()) {
            unsigned int d = readers[i].get_docID();
            unsigned int f = readers[i].get_freq();
            int dl = word_counts[d];
            total_score[d] += BM25_ranking(f, dl, term_data[i].doc_freq, passage_count, avg_doc_length);
            readers[i].next();
        }
    }

    // convert to a sortable vector.
    std::vector<std::pair<int,double> > results;
    for (std::unordered_map<int,double>::iterator it = total_score.begin(); it != total_score.end(); ++it)
        results.push_back({it->first, it->second});

    print_results(results);
}

void conjunctive_search(std::vector<lexicon_data>& term_data,
                        std::vector<ListReader>& readers,
                        const std::vector<int>& word_counts){

    std::vector<std::pair<int,double>> results;

    while (true) {
        // get largest current docID across all readers.
        int target = 0;
        for (std::size_t i = 0; i < readers.size(); i++)
            if (readers[i].get_docID() > target) target = readers[i].get_docID();

        // align every reader on target docID.
        bool match = true;
        for (std::size_t i = 0; i < readers.size(); i++) {
            if (!readers[i].seek(target)) {
                // this term has no more postings >= target
                // no other doc can contain all terms
                print_results(results);
                return;
            }
            if (readers[i].get_docID() != target) {
                // reader jumped past target, retry with the new max
                match = false;
                break;
            }
        }

        if (!match) continue; // recompute target next iteration

        // if all docID match, score the document
        int dl = word_counts[target];

        double total_score = 0.0;
        for (std::size_t i = 0; i < readers.size(); i++)
            total_score += BM25_ranking(readers[i].get_freq(), dl, term_data[i].doc_freq, passage_count, avg_doc_length);
        results.push_back({target, total_score});

        // advance to the current max
        for (std::size_t i = 0; i < readers.size(); i++)
            if (!readers[i].next()){ // if any reader ran out, no future match is possible
                print_results(results);
                return;
            }
    }
}

int main() {
    std::string line; // temp string for parsing files

    // get statistics for BM25 ranking function
    std::fstream statistics_file("stats.csv");

    std::getline(statistics_file, line, ',');
    statistics_file >> passage_count;

    std::getline(statistics_file, line, ',');
    statistics_file >> avg_doc_length;

    statistics_file.close();

    // load lexicon into memory
    std::unordered_map<std::string, lexicon_data> lexicon;
    
    std::fstream lexicon_file("lexicon.txt");

    std::string term;
    lexicon_data l;

    while ( lexicon_file >> term >> l.start >> l.doc_freq >> l.num_blocks >> l.first_block){
        lexicon[term] = l;
    }

    std::cout << "loaded " << lexicon.size() << " terms" << std::endl;

    lexicon_file.close();

    // load block metadata into memory
    std::fstream block_metadata_file("blockmeta.txt");
    block_meta b;

    while ( block_metadata_file >> b.last_docID >> b.doc_bytes >> b.freq_bytes ){
        block_metadatas.push_back(b);
    }

    std::cout << "loaded " << block_metadatas.size() << " block records" << std::endl;
    block_metadata_file.close();

    // load page table into memory
    std::fstream pagetable_file("pagetable.csv"); // docID, word_count
    std::vector<int> word_counts; // index correlates to docid
    char deliminator;
    int doc_id;
    int word_count;
    
    std::getline(pagetable_file, line); // header
    while ( std::getline(pagetable_file, line) ){
        std::stringstream ss(line);

        ss >> doc_id >> deliminator >> word_count;
        word_counts.push_back(word_count);
    }

    std::cout << "loaded " << word_counts.size() << " word counts" << std::endl;
    pagetable_file.close();

    // open index.bin once
    static std::ifstream index_file("index.bin", std::ios::binary);

    // query loop
    while (true){
        // get type of query, disjuctive or conjuctive
        std::string query_type;
        std::cout << "disjunctive or conjunctive query processing" << std::endl;
        std::getline(std::cin, query_type);
        if (query_type != "disjunctive" && query_type != "conjunctive"){
            break;
        }
        
        // get query input from user
        std::string query;
        std::cout << "enter query : ";
        std::getline(std::cin, query);

        // normalize query
        for (char& c : query){
            if (isalnum(static_cast<unsigned char>(c))){ // convert to lowercase
                c = std::tolower(static_cast<unsigned char>(c));
            } else{ // remove symbols and replace with space
                c = ' ';
            }
        }
        
        // parse query
        std::unordered_set<std::string> query_terms;
        std::stringstream ss(query);

        while ( ss >> term ){
            query_terms.insert(term);
        }

        std::vector<lexicon_data> term_data;
        std::vector<ListReader> readers;
        search_setup(query_terms, lexicon, term_data, readers, index_file);
        if (term_data.empty()){
            std::cout << "no terms found in index" << std::endl;
            continue;
        }

        if (query_type == "disjunctive"){
            disjunctive_search(term_data, readers, word_counts);
        } else if (term_data.size() == query_terms.size()) { // conjunctive + make sure that all terms exist
            conjunctive_search(term_data, readers, word_counts);
        }
    }
    return 0;
}