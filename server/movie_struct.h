#ifndef MOVIE_STRUCT_H
#define MOVIE_STRUCT_H

#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>

// Structure to hold movie data based on TMDB format
struct Movie {
    int id;
    std::string title;
    double vote_average;
    int vote_count;
    std::string status;
    std::string release_date;
    int64_t revenue;
    int runtime;
    bool adult;
    std::string backdrop_path;
    int64_t budget;
    std::string homepage;
    std::string imdb_id;
    std::string original_language;
    std::string original_title;
    std::string overview;
    double popularity;
    std::string poster_path;
    std::string tagline;
    std::string genres;
    std::string production_companies;
    std::string production_countries;
    std::string spoken_languages;
    std::string keywords;
};

// Trim whitespace from start and end of string
static inline std::string trim(const std::string& str) {
    std::string s = str;
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }));
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), s.end());
    return s;
}

// Parse a CSV line while respecting quotes
static std::vector<std::string> parseCSVLine(const std::string& line) {
    std::vector<std::string> result;
    bool inQuotes = false;
    std::string field;
    
    for (char c : line) {
        if (c == '\"') {
            inQuotes = !inQuotes;
        } else if (c == ',' && !inQuotes) {
            result.push_back(trim(field));
            field.clear();
        } else {
            field += c;
        }
    }
    result.push_back(trim(field)); // Add the last field
    
    return result;
}

// Function to load movies from TMDB CSV format
static std::vector<Movie> loadMoviesFromCSV(const std::string& filename) {
    std::vector<Movie> movies;
    std::ifstream file(filename);
    
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return movies;
    }
    
    std::string line;
    // Skip header line
    std::getline(file, line);
    
    while (std::getline(file, line)) {
        std::vector<std::string> fields = parseCSVLine(line);
        
        // Check if we have all fields
        if (fields.size() < 24) {
            std::cerr << "Warning: Skipping incomplete row in CSV" << std::endl;
            continue;
        }
        
        try {
            Movie movie;
            movie.id = fields[0].empty() ? 0 : std::stoi(fields[0]);
            movie.title = fields[1];
            movie.vote_average = fields[2].empty() ? 0.0 : std::stod(fields[2]);
            movie.vote_count = fields[3].empty() ? 0 : std::stoi(fields[3]);
            movie.status = fields[4];
            movie.release_date = fields[5];
            movie.revenue = fields[6].empty() ? 0 : std::stoll(fields[6]);
            movie.runtime = fields[7].empty() ? 0 : std::stoi(fields[7]);
            movie.adult = (fields[8] == "TRUE" || fields[8] == "true");
            movie.backdrop_path = fields[9];
            movie.budget = fields[10].empty() ? 0 : std::stoll(fields[10]);
            movie.homepage = fields[11];
            movie.imdb_id = fields[12];
            movie.original_language = fields[13];
            movie.original_title = fields[14];
            movie.overview = fields[15];
            movie.popularity = fields[16].empty() ? 0.0 : std::stod(fields[16]);
            movie.poster_path = fields[17];
            movie.tagline = fields[18];
            movie.genres = fields[19];
            movie.production_companies = fields[20];
            movie.production_countries = fields[21];
            movie.spoken_languages = fields[22];
            movie.keywords = fields[23];
            
            movies.push_back(movie);
        } catch (const std::exception& e) {
            std::cerr << "Error parsing movie data: " << e.what() << std::endl;
        }
    }
    
    std::cout << "Loaded " << movies.size() << " movies from " << filename << std::endl;
    return movies;
}

#endif // MOVIE_STRUCT_H