#include <iostream>
#include <fstream>
#include <string>
#include <vector>

const int BLOCK_SIZE = 128; // postings per block 
// One open temp file and the row it is currently on
struct TempFile {
    std::ifstream file;
    std::string term; // current row: term
    int docID = 0; // current row: docID
    int freq = 0; // current row: frequency
    bool done = false; // true when this file has no rows left
};
std::vector<TempFile> temp_files; // all the open temp files
// The output files
std::ofstream indexFile("index.bin", std::ios::binary); // compressed inverted lists
std::ofstream lexiconFile("lexicon.txt"); // term  start  num_postings  num_blocks  first_block
std::ofstream metaFile("blockmeta.txt"); // per block: last_docID  docID_bytes  freq_bytes
long long index_position = 0; // bytes written to index.bin so far
long long block_number = 0;   // blocks written so far
// Read the next row of one temp file (a line like  apple,19,10)
void readNext(TempFile& t) {
    std::string line;
    if (!std::getline(t.file, line)) { t.done = true; return; } // no more rows
    if (!line.empty() && line.back() == '\r') line.pop_back(); // Windows line endings
    size_t c1 = line.find(','); // first comma
    size_t c2 = line.find(',', c1 + 1); // second comma
    t.term  = line.substr(0, c1);
    t.docID = std::stoi(line.substr(c1 + 1, c2 - c1 - 1));
    t.freq  = std::stoi(line.substr(c2 + 1));
}
// Open temp_0.csv, temp_1.csv, ... and read the first row of each
void openTempFiles() {
    for (int i = 0; ; i++) {
        TempFile t;
        t.file.open("temp_" + std::to_string(i) + ".csv");
        if (!t.file) break; // no more temp files
        std::string heading;
        std::getline(t.file, heading); // skip the "Term,DocID,Frequency" heading line
        readNext(t); // load the first row
        temp_files.push_back(std::move(t));
    }
    std::cout << "Found " << temp_files.size() << " temp files\n";
}
// Find the smallest term among the current rows of all temp files ("" if all files are done)
std::string smallestTerm() {
    std::string smallest = "";
    for (TempFile& t : temp_files) {
        if (t.done) continue;
        if (smallest == "" || t.term < smallest) smallest = t.term;
    }
    return smallest;
}
// Turn one number into 1 or more bytes (varbyte encoding)
void varbyteEncode(unsigned int num, std::vector<unsigned char>& bytes) {
    while (num >= 128) {
        bytes.push_back(128 + (num & 127)); // lowest 7 bits, top bit 1 = "more bytes follow"
        num = num >> 7;
    }
    bytes.push_back(num);                   // last byte, top bit 0
}
// Compress one word's list and write it, in blocks of 128 postings
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
            varbyteEncode(freqs[i], freqBytes);            // frequency
        }
        // docIDs first, then freqs (not mixed)
        indexFile.write((char*) docBytes.data(), docBytes.size());
        indexFile.write((char*) freqBytes.data(), freqBytes.size());
        index_position += docBytes.size() + freqBytes.size();

        // block info: last docID, size of docID part, size of freq part
        metaFile << docIDs[end - 1] << " " << docBytes.size() << " " << freqBytes.size() << "\n";
        num_blocks++;
        block_number++;
    }
    lexiconFile << term << " " << start << " " << docIDs.size() << " "
                << num_blocks << " " << first_block << "\n";
}
int main() {
    openTempFiles();
    int words = 0;
    while (true) {
        std::string term = smallestTerm();   // next word in A-Z order
        if (term == "") break;               // all temp files are finished

        // collect this word's postings from every temp file, in file order (keeps docIDs sorted)
        std::vector<int> docIDs, freqs;
        for (TempFile& t : temp_files) {
            while (!t.done && t.term == term) {
                docIDs.push_back(t.docID);
                freqs.push_back(t.freq);
                readNext(t);
            }
        }
        writeList(term, docIDs, freqs);
        words++;
    }
    std::cout << "Done: " << words << " words, " << block_number << " blocks, "
              << index_position << " bytes in index.bin\n";
    return 0;
}