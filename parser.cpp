#include <iostream>
#include <fstream>
#include <string>
#include <cctype>
#include <sstream>
#include <istream>
#include <map>
#include <vector>
#include <set>

std::string line;
std::string word;
int count = 0;
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

    std::ofstream termFile("Term Counts.txt"); // this is for the terms and their counts go here. Can delete it later, Just for visual
    if (!termFile) {
        std::cout << "Could not create Term Count file\n";
        return 1;
    }

    std::ofstream pageFile("pagetable.txt"); // docID length
    if (!pageFile) {
        std::cout << "Could not create page table file\n";
        return 1;
    }

    if (!postingsFile) {
        std::cout << "Could not create posting file\n";
        return 1;
    }

    while (std::getline(file, line)) {
        if (count >= 100) break;
        // grab the line from the file
        size_t tab = line.find('\t'); // find the tab which is between the ID and the text
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
        }
        outFile << "\n";
        for (auto& p : passage_terms) inverted_index[p.first].emplace_back(count, p.second);
        outFile << "\n";
        pageFile << DocID << " | " << word_count << "\n";
        count++;
    }
    termFile << "Term Count:" "\n";
    for (auto& p : term_count) termFile << p.first << ": " << p.second << "\n";

    postingsFile << "Term | (Doc ID, Frequency)\n"; // heading
    for (auto& entry : inverted_index) {
        postingsFile << entry.first << ":"; // this is for the term
        for (auto& posting : entry.second)
            postingsFile << " (" << posting.first << ", " << posting.second << ")"; // (docID, freq)
        postingsFile << "\n";
    }
    std::cout << "Total lines read: " << count << "\n";
    outFile.close();
    termFile.close();
    postingsFile.close();
    pageFile.close();
    return 0;
}