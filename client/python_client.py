#!/usr/bin/env python3

import sys
import time
import grpc
import argparse
import textwrap
from typing import List

import movie_pb2
import movie_pb2_grpc

class MovieClient:
    def __init__(self, server_address: str):
        """Initialize the MovieClient with the server address."""
        self.channel = grpc.insecure_channel(server_address)
        self.stub = movie_pb2_grpc.MovieSearchStub(self.channel)
        print(f"📡 Connected to server at {server_address}")

    def search_movie(self, query: str) -> List[movie_pb2.MovieInfo]:
        """Search for movies matching the query."""
        try:
            print(f"🔍 Searching for: {query}")
            start_time = time.time()
            
            # Create request
            request = movie_pb2.SearchRequest(title=query)
            
            # Send request to server A
            response = self.stub.Search(request)
            
            # Calculate elapsed time
            elapsed_time = (time.time() - start_time) * 1000  # Convert to milliseconds
            
            # Format and print results
            result_count = len(response.results)
            print(f"🎬 Results for query \"{query}\" (found {result_count} matches in {elapsed_time:.1f}ms):")
            
            if result_count == 0:
                print("No movies found matching your query.")
                return []
            
            # Print header
            print(f"{'TITLE':<40} {'PRODUCTION':<30} {'GENRE':<25} YEAR")
            print("-" * 100)
            
            # Print each movie
            for movie in response.results:
                title = (movie.title[:37] + "...") if len(movie.title) > 37 else movie.title
                director = (movie.director[:27] + "...") if len(movie.director) > 27 else movie.director
                genre = (movie.genre[:22] + "...") if len(movie.genre) > 22 else movie.genre
                
                print(f"{title:<40} {director:<30} {genre:<25} {movie.year}")
            
            return response.results
            
        except grpc.RpcError as e:
            print(f"Error: {e.code()}")
            print(f"   Details: {e.details()}")
            
            if e.code() == grpc.StatusCode.UNAVAILABLE:
                print("   Server is not available. Check if it's running and the address is correct.")
            elif e.code() == grpc.StatusCode.DEADLINE_EXCEEDED:
                print("   Request timed out. The server might be overloaded.")
            
            return []

def main():
    """Main function to run the movie search client."""
    parser = argparse.ArgumentParser(
        description="Movie Search Client",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=textwrap.dedent('''
        Examples:
          python python_client.py localhost:50001
          python python_client.py 192.168.1.100:50001 Inception
        ''')
    )
    
    parser.add_argument("server_address", help="Address of server A (host:port)")
    parser.add_argument("query", nargs="?", help="Optional search query")
    
    args = parser.parse_args()
    
    client = MovieClient(args.server_address)
    
    if args.query:
        client.search_movie(args.query)
        return
    
    try:
        while True:
            query = input("\n🔍 Enter search term (title, genre, keywords) or 'exit' to quit: ")
            if query.lower() in ('exit', 'quit', 'q'):
                break
            
            if not query.strip():
                continue
                
            client.search_movie(query)
    except KeyboardInterrupt:
        print("\nExiting...")
    finally:
        print("Goodbye! 👋")

if __name__ == "__main__":
    main()