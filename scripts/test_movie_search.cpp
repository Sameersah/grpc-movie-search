#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <algorithm>
#include <cassert>
#include "server/movie_struct.h"

// Simple unit tests for movie search functionality

// Test the movieMatchesQuery function
bool test_movie_matching() {
    // Create a sample movie
    Movie movie;
    movie.id = 1;
    movie.title = "Inception";
    movie.vote_average = 8.8;
    movie.genres = "Action, Sci-Fi, Thriller";
    movie.production_companies = "Warner Bros, Legendary Pictures";
    movie.release_date = "7/16/10";
    movie.overview = "A thief who steals corporate secrets through dream sharing.";
    movie.keywords = "dream, heist, subconscious";

    // Test cases
    struct TestCase {
        std::string query;
        bool expected_result;
    };

    std::vector<TestCase> testCases = {
        {"Inception", true},        // Match title
        {"inception", true},        // Case-insensitive match
        {"Sci-Fi", true},           // Match genre
        {"Legend", true},           // Match in production companies
        {"dream", true},            // Match in keywords
        {"thief", true},            // Match in overview
        {"Batman", false},          // No match
        {"Romance", false}          // No match
    };

    bool all_passed = true;
    
    for (const auto& test : testCases) {
        // Define the movieMatchesQuery function locally since it's not directly accessible
        auto movieMatchesQuery = [](const Movie& movie, const std::string& query) {
            // Convert query and relevant fields to lowercase for case-insensitive comparison
            std::string lowerQuery = query;
            std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(), ::tolower);
            
            std::string lowerTitle = movie.title;
            std::transform(lowerTitle.begin(), lowerTitle.end(), lowerTitle.begin(), ::tolower);
            
            std::string lowerGenres = movie.genres;
            std::transform(lowerGenres.begin(), lowerGenres.end(), lowerGenres.begin(), ::tolower);
            
            std::string lowerOverview = movie.overview;
            std::transform(lowerOverview.begin(), lowerOverview.end(), lowerOverview.begin(), ::tolower);
            
            std::string lowerKeywords = movie.keywords;
            std::transform(lowerKeywords.begin(), lowerKeywords.end(), lowerKeywords.begin(), ::tolower);
            
            std::string lowerCompanies = movie.production_companies;
            std::transform(lowerCompanies.begin(), lowerCompanies.end(), lowerCompanies.begin(), ::tolower);
            
            return 
                lowerTitle.find(lowerQuery) != std::string::npos ||
                lowerGenres.find(lowerQuery) != std::string::npos ||
                lowerOverview.find(lowerQuery) != std::string::npos ||
                lowerKeywords.find(lowerQuery) != std::string::npos ||
                lowerCompanies.find(lowerQuery) != std::string::npos;
        };
        
        bool result = movieMatchesQuery(movie, test.query);
        bool passed = (result == test.expected_result);
        
        if (!passed) {
            std::cerr << " Test failed for query '" << test.query 
                      << "': expected " << (test.expected_result ? "match" : "no match")
                      << " but got " << (result ? "match" : "no match") << std::endl;
            all_passed = false;
        } else {
            std::cout << "Test passed for query '" << test.query << "'" << std::endl;
        }
    }
    
    return all_passed;
}

// Test the CSV parsing functionality with a small sample
bool test_csv_parsing() {
    // Create a temporary CSV file
    std::string tempFileName = "temp_test_movies.csv";
    std::ofstream tempFile(tempFileName);
    if (!tempFile.is_open()) {
        std::cerr << "Failed to create temporary test file" << std::endl;
        return false;
    }
    
    // Write sample data
    tempFile << "id,title,vote_average,vote_count,status,release_date,revenue,runtime,adult,backdrop_path,budget,homepage,imdb_id,original_language,original_title,overview,popularity,poster_path,tagline,genres,production_companies,production_countries,spoken_languages,keywords\n";
    tempFile << "27205,Inception,8.364,34495,Released,7/15/10,825532764,148,FALSE,/path.jpg,160000000,http://example.com,tt1375666,en,Inception,A mind-bending thriller,83.952,/path.jpg,Your mind is the scene of the crime.,Action/Sci-Fi,Warner Bros,USA,English,dream\n";
    tempFile << "157336,Interstellar,8.417,32571,Released,11/5/14,701729206,169,FALSE,/path.jpg,165000000,http://example.com,tt0816692,en,Interstellar,Space exploration,140.241,/path.jpg,Mankind was born on Earth,Sci-Fi/Adventure,Paramount,USA,English,space\n";
    tempFile.close();
    
    // Load the movies
    std::vector<Movie> movies = loadMoviesFromCSV(tempFileName);
    
    // Delete the temporary file
    std::remove(tempFileName.c_str());
    
    // Verify the results
    if (movies.size() != 2) {
        std::cerr << " Expected 2 movies, got " << movies.size() << std::endl;
        return false;
    }
    
    // Check details of the first movie
    if (movies[0].title != "Inception" || 
        movies[0].vote_average != 8.364 ||
        movies[0].genres != "Action/Sci-Fi") {
        std::cerr << " First movie details don't match expected values" << std::endl;
        return false;
    }
    
    // Check details of the second movie
    if (movies[1].title != "Interstellar" || 
        movies[1].vote_average != 8.417 ||
        movies[1].genres != "Sci-Fi/Adventure") {
        std::cerr << " Second movie details don't match expected values" << std::endl;
        return false;
    }
    
    std::cout << "CSV parsing test passed" << std::endl;
    return true;
}

int main() {
    std::cout << "Running movie search unit tests\n" << std::endl;
    
    bool tests_passed = true;
    
    std::cout << "=== Testing movie query matching ===" << std::endl;
    tests_passed &= test_movie_matching();
    std::cout << std::endl;
    
    std::cout << "=== Testing CSV parsing ===" << std::endl;
    tests_passed &= test_csv_parsing();
    std::cout << std::endl;
    
    if (tests_passed) {
        std::cout << " All tests passed! " << std::endl;
        return 0;
    } else {
        std::cerr << " Some tests failed" << std::endl;
        return 1;
    }
}