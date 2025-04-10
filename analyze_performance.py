#!/usr/bin/env python3
# analyze_performance.py

import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
import glob
import os

# Create results directory for charts
os.makedirs("performance_charts", exist_ok=True)

# Load all result files
result_files = glob.glob("performance_results/*.csv")
all_data = []

for file in result_files:
    config_name = os.path.basename(file).replace('_results.csv', '')
    df = pd.read_csv(file)
    df['Configuration'] = config_name
    all_data.append(df)

# Combine all data
combined_df = pd.concat(all_data)

# Calculate average duration by query, configuration, and run type
avg_duration = combined_df.groupby(['Query', 'Configuration', 'RunType'])['Duration(ms)'].mean().reset_index()

# Plot 1: Cold vs. Warm Cache by Configuration
plt.figure(figsize=(12, 8))
sns.barplot(x='Configuration', y='Duration(ms)', hue='RunType', data=avg_duration)
plt.title('Average Query Duration: Cold vs. Warm Cache')
plt.ylabel('Average Duration (ms)')
plt.xlabel('Configuration')
plt.xticks(rotation=45)
plt.tight_layout()
plt.savefig('performance_charts/cold_vs_warm.png')

# Plot 2: Speedup for each configuration (Warm/Cold ratio)
speedup_df = avg_duration.pivot_table(
    index=['Query', 'Configuration'],
    columns='RunType',
    values='Duration(ms)'
).reset_index()
speedup_df['Speedup'] = speedup_df['cold'] / speedup_df['warm']

plt.figure(figsize=(12, 8))
sns.barplot(x='Configuration', y='Speedup', data=speedup_df)
plt.title('Cache Speedup (Cold/Warm Ratio)')
plt.ylabel('Speedup Factor')
plt.xlabel('Configuration')
plt.xticks(rotation=45)
plt.axhline(y=1, color='r', linestyle='--')
plt.tight_layout()
plt.savefig('performance_charts/speedup_factor.png')

# Plot 3: Query performance by configuration (warm cache)
warm_data = avg_duration[avg_duration['RunType'] == 'warm']
plt.figure(figsize=(12, 8))
sns.barplot(x='Query', y='Duration(ms)', hue='Configuration', data=warm_data)
plt.title('Warm Cache Performance by Query and Configuration')
plt.ylabel('Average Duration (ms)')
plt.xlabel('Query')
plt.xticks(rotation=45)
plt.legend(bbox_to_anchor=(1.05, 1), loc='upper left')
plt.tight_layout()
plt.savefig('performance_charts/query_performance.png')

# Plot 4: Multi-server comparison (shared memory effect)
multi_server_data = combined_df[combined_df['Configuration'].str.startswith('multi_server')]
multi_server_data['Server'] = multi_server_data['Configuration'].str.extract(r'multi_server_(\d+)')
multi_server_avg = multi_server_data.groupby(['Query', 'Server', 'RunType'])['Duration(ms)'].mean().reset_index()

plt.figure(figsize=(12, 8))
sns.barplot(x='Query', y='Duration(ms)', hue='Server', data=multi_server_avg[multi_server_avg['RunType'] == 'warm'])
plt.title('Multi-Server Performance (Warm Cache)')
plt.ylabel('Average Duration (ms)')
plt.xlabel('Query')
plt.xticks(rotation=45)
plt.tight_layout()
plt.savefig('performance_charts/multi_server_comparison.png')

print("Analysis complete! Charts saved to performance_charts/")