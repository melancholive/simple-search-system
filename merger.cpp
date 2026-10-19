#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <chrono>

const int BLOCK_SIZE = 128; // postings per block 
std::ofstream indexFile("index.bin", std::ios::binary); // compressed inverted lists
std::ofstream lexiconFile("lexicon.txt"); // term  start  num_postings  num_blocks  first_block
std::ofstream metaFile("blockmeta.txt"); // per block: last_docID  docID_bytes  freq_bytes
long long index_position = 0; // bytes written to index.bin so far
long long block_number = 0; // blocks written so far

struct TempFile {
    std::ifstream file;// it opens the temp file, it holds the data from the temp file
    std::string term; // the term from the temp file
    int docID = 0; // the docID from the temp file     
    int freq = 0;  // the frequency from the temp file      
    bool done = false; // if the temp file is done reading, it will be true  
};
std::vector<TempFile> temp_files;
// The output files

void readNext(TempFile& temp) {
    std::string line;
    if (!std::getline(temp.file, line)) {
        temp.done = true;
        return;
    }
    size_t c1 = line.find(',');            // position of the 1st comma
    size_t c2 = line.find(',', c1 + 1);    // position of the 2nd comma
    temp.term  = line.substr(0, c1);                               // text before the 1st comma
    temp.docID = std::stoi(line.substr(c1 + 1, c2 - c1 - 1));      // text between the commas → number
    temp.freq  = std::stoi(line.substr(c2 + 1));                   // text after the 2nd comma → number
}

std::string smallestTerm() {
    std::string smallest = "";
    for (TempFile& temp : temp_files) {
        if (temp.done) continue; // skip finished files
        if (smallest == "" || temp.term < smallest) smallest = temp.term;
    }
    return smallest; // means all files are finished
}

void varbyteEncode(unsigned int num, std::vector<unsigned char>& bytes) {
    while (num >= 128) {
        bytes.push_back(128 + (num & 127)); // lowest 7 bits + "more bytes coming"
        num = num >> 7; // drop those 7 bits
    }
    bytes.push_back(num); // last byte
}
void writeList(const std::string& term, const std::vector<int>& docIDs, const std::vector<int>& freqs) {
    long long start = index_position;
    long long first_block = block_number;
    int num_blocks = 0;
    int previous = 0; // previous docID, for the gaps
    for (size_t b = 0; b < docIDs.size(); b += BLOCK_SIZE) {
        size_t end = std::min(b + BLOCK_SIZE, docIDs.size());

        std::vector<unsigned char> docBytes, freqBytes;
        for (size_t i = b; i < end; i++) {
            varbyteEncode(docIDs[i] - previous, docBytes); // docID gap: 19, 27, 39 -> 19, 8, 12
            previous = docIDs[i];
            varbyteEncode(freqs[i], freqBytes); // frequency
        }
        // docIDs first, then freqs (not mixed)
        indexFile.write((char*) docBytes.data(), docBytes.size());
        indexFile.write((char*) freqBytes.data(), freqBytes.size());
        index_position += docBytes.size() + freqBytes.size();

        metaFile << docIDs[end - 1] << " " << docBytes.size() << " " << freqBytes.size() << "\n"; // block info: last docID, size of docID part, size of freq part
        num_blocks++;
        block_number++;
    }
    lexiconFile << term << " " << start << " " << docIDs.size() << " "
                << num_blocks << " " << first_block << "\n";
}

int main() {
    auto start_time = std::chrono::steady_clock::now();
    for (int i = 0; ; i++) {
        TempFile temp;
        temp.file.open("temp_" + std::to_string(i) + ".csv");
        if (!temp.file) break;
        std::string heading;
        std::getline(temp.file, heading); // skip heading
        readNext(temp); // read the first row
        temp_files.push_back(std::move(temp));
    }
    std::cout << "Found " << temp_files.size() << " temp files\n";
    int words = 0;
    while (true) {
        std::string term = smallestTerm();// next word A-Z
        if (term == "") break; // all temp files finished

        std::vector<int> docIDs, freqs;  // this word's postings
        for (TempFile& temp : temp_files) { // go through files in order (keeps docIDs sorted)
            while (!temp.done && temp.term == term) {
                docIDs.push_back(temp.docID);
                freqs.push_back(temp.freq);
                readNext(temp); // move this file to its next row
            }
        }
        writeList(term, docIDs, freqs); // Write the postings for this word
        words++;
    }
    std::cout << "Done: " << words << " words, " << block_number << " blocks, "
              << index_position << " bytes in index.bin\n";
    std::cout << "Total Merger time taken: " << std::chrono::duration<double>(std::chrono::steady_clock::now() - start_time).count() << " seconds\n";
    return 0;
}
