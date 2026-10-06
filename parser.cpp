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
std::map<std::string, int> term_count;
std::map<std::string, std::vector<std::pair<int, int>>> inverted_index;
std::set<std::string> stopwords = {
    "a", "an", "and", "are", "as", "at", "be", "by", "for", "from",
    "has", "he", "in", "is", "it", "its", "of", "on", "or", "that",
    "the", "to", "was", "were", "will", "with"
};

int main() {
    std::ifstream file("collection.tsv");
    if (!file) {
        std::cout << "Could not open the file\n";
        return 1;
    }
    std::ofstream outFile("output.txt"); // I output all the separated IDs and Text Passages here, we can delete it later. Just for visual
    if (!outFile) {
        std::cout << "Could not create output file\n";
        return 1;
    }

    std::ofstream termFile("Term Counts.csv"); // this is for the terms and their counts go here. Can delete it later, Just for visual
    if (!termFile) {
        std::cout << "Could not create Term Count file\n";
        return 1;
    }

    std::ofstream pageFile("pagetable.csv"); // docID length
    if (!pageFile) {
        std::cout << "Could not create page table file\n";
        return 1;
    }
    pageFile << "DocID" << "," << "word_count" << "\n";

    std::ofstream postingsFile("posting.csv"); // Term, Doc ID, Frequency
    if (!postingsFile) {
        std::cout << "Could not create posting file\n";
        return 1;
    }
    postingsFile << "Term,DocID,Frequency\n"; // heading

    while (std::getline(file, line)) {
        if (count >= 100) break;
        // grab the line from the file
        size_t tab = line.find('\t'); // find the tab which is between the ID and the text
        if (tab == std::string::npos) continue; // this is to skip lines with no tab
        std::string DocID = line.substr(0, tab); // take the ID before the tab
        std::string text = line.substr(tab + 1); // take the text after the tab
        outFile << "ID: " << DocID << "\n";
        outFile << "Text: " << text << "\n\n";
        for (char& c : text) { // it goes through letter by letter
            if (isalnum(static_cast<unsigned char>(c))) c = static_cast<char>(tolower(c)); // keep letters and numbers, make lowercase
            else c = ' '; // everything else becomes a space like punctuations
        }
        std::stringstream ss(text); // it puts the clean text into string stream
        int word_count = 0;
        std::map<std::string, int> passage_terms;
        outFile << "Words: ";
        while (ss >> word) { // to keep giving the next word
            if (stopwords.count(word)) continue; // skip stopwords like "the", "a"
            word_count++;
            term_count[word]++;
            passage_terms[word]++;
            outFile << word << " | "; // this is to separate each word
        }
        outFile << "\n";
        for (auto& p : passage_terms) inverted_index[p.first].emplace_back(count, p.second);
        outFile << "\n";
        pageFile << DocID << "," << word_count << "\n";
        total_words += word_count;
        count++;
        // change it to 500 000 later, just keep 25 for now cuz testing with just 100 passages
        if (count % 25 == 0) {
            std::ofstream tempFile("temp_" + std::to_string(temp_number) + ".csv");
            tempFile << "Term,DocID,Frequency\n";
            for (auto& entry : inverted_index) {
                for (auto& posting : entry.second) {
                    tempFile << entry.first << "," << posting.first << "," << posting.second << "\n";
                    postingsFile << entry.first << "," << posting.first << "," << posting.second << "\n";
                }
            }
            inverted_index.clear(); // after each temp file is written, so temp files no longer repeat earlier postings.
            temp_number++;  // next file number
        }
    }
    termFile << "Term,Count\n";
    for (auto& p : term_count) termFile << p.first << "," << p.second << "\n";

    // write whatever is left as the last temp file
    std::ofstream tempFile("temp_" + std::to_string(temp_number) + ".csv");
    tempFile << "Term,DocID,Frequency\n";
    for (auto& entry : inverted_index) {
        for (auto& posting : entry.second) {
            tempFile << entry.first << "," << posting.first << "," << posting.second << "\n";
            postingsFile << entry.first << "," << posting.first << "," << posting.second << "\n"; // this is for the term
        }
    }
    std::ofstream statsFile("stats.csv");
    statsFile << "Passage Count," << count << "\n";
    statsFile << "Average Passage Length," << (double) total_words / count << "\n"; // avg passage length = total_words ÷ number of passages
    std::cout << "Total lines read: " << count << "\n";
    outFile.close();
    termFile.close();
    postingsFile.close();
    pageFile.close();
    tempFile.close();
    statsFile.close();
    return 0;
}