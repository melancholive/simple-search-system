#include <iostream>
#include <fstream>
#include <string>
#include <cctype>
#include <sstream>
#include <istream>
#include <map>
#include <vector>
#include <set>
#include <cstdio> // For std::remove

std::string line;
std::string word;
int count = 0;
int temp_number = 0; // temp file number
long long total_words = 0; // for stats: all words added together
std::map<std::string, std::vector<std::pair<int, int>>> inverted_index;

int main() {
    std::ifstream file("collection.tsv");
    if (!file) {
        std::cout << "Could not open the file\n";
        return 1;
    }

    std::ofstream pageFile("pagetable.csv"); // docID length
    if (!pageFile) {
        std::cout << "Could not create page table file\n";
        return 1;
    }
    pageFile << "DocID" << "," << "word_count" << "\n";

    while (std::getline(file, line)) {
        // grab the line from the file
        size_t tab = line.find('\t'); // find the tab which is between the ID and the text
        if (tab == std::string::npos) continue; // this is to skip lines with no tab
        std::string DocID = line.substr(0, tab); // take the ID before the tab
        std::string text = line.substr(tab + 1); // take the text after the tab
        for (char& c : text) { // it goes through letter by letter
            if (isalnum(static_cast<unsigned char>(c))) c = static_cast<char>(tolower(c)); // keep letters and numbers, make lowercase
            else c = ' '; // everything else becomes a space like punctuations
        }
        std::stringstream ss(text); // it puts the clean text into string stream
        int word_count = 0;
        std::map<std::string, int> passage_terms;
        while (ss >> word) {
            // to keep giving the next word
                word_count++;
                passage_terms[word]++;

        }
        for (auto& p : passage_terms) inverted_index[p.first].emplace_back(count, p.second);
        pageFile << DocID << "," << word_count << "\n";
        total_words += word_count;
        count++;
        if (count % 500000 == 0) {
            std::ofstream tempFile("temp_" + std::to_string(temp_number) + ".csv");
            tempFile << "Term,DocID,Frequency\n";
            for (auto& entry : inverted_index) {
                for (auto& posting : entry.second) {
                    tempFile << entry.first << "," << posting.first << "," << posting.second << "\n";
                }
            }
            inverted_index.clear(); // after each temp file is written, so temp files no longer repeat earlier postings.
            temp_number++;  // next file number
        }
    }
    // write whatever is left as the last temp file
    std::ofstream tempFile("temp_" + std::to_string(temp_number) + ".csv");
    tempFile << "Term,DocID,Frequency\n";
    for (auto& entry : inverted_index) {
        for (auto& posting : entry.second) {
            tempFile << entry.first << "," << posting.first << "," << posting.second << "\n";
        }
    }
    std::ofstream statsFile("stats.csv");
    statsFile << "Passage Count," << count << "\n";
    statsFile << "Average Passage Length," << (double) total_words / count << "\n"; // avg passage length = total_words ÷ number of passages
    std::cout << "Total lines read: " << count << "\n";
    pageFile.close();
    tempFile.close();
    statsFile.close();
    return 0;
}